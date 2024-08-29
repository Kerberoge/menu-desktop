#include <stdio.h>
#include <stdlib.h> 	/* for qsort() */
#include <pwd.h>		/* for getpwuid() */
#include <unistd.h> 	/* for fork() and execvp() */
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>	/* for wait() */

#define ROWS 100 /* Max number of paths in apps[] */

struct path {
	char dir[100];
	char filename[100];
	int active;
};

struct menuentry {
	char name[100];
	char cmd[100];
};

char *const menu = "mew";
char *const menu_arguments[] = { menu, NULL };

char system_dir[] = "/usr/share/applications";
char user_dir[100]; /* This string is defined in main() after getting the user name */

struct path apps[ROWS] = {0};

int startswith(const char *str, const char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

int endswith(const char *str, const char *suffix) {
	int str_len = strlen(str);
	int suf_len = strlen(suffix);

	return str_len > suf_len && strcmp(str + str_len - suf_len, suffix) == 0;
}

void add_app(const char *dir, const char *filename) {
	int position;

	for (position = 0; apps[position].active; position++);
	strcpy(apps[position].dir, dir);
	strcpy(apps[position].filename, filename);
	apps[position].active = 1;
}

void get_apps(const char *dir) {
	DIR *applications;
	struct dirent *entry;

	applications = opendir(dir);

	while (entry = readdir(applications)) {
		if (endswith(entry->d_name, ".desktop")) {
			add_app(dir, entry->d_name);
		}
	}

	closedir(applications);
}

int filenames_cmp_func(const void *a, const void *b) {
	return strcmp(((struct path *)a)->filename, ((struct path*)b)->filename);
}

void sort_filenames(struct path *arr, int size) {
	qsort(apps, size, sizeof(struct path), filenames_cmp_func);
}

void remove_overridden_files(void) {
	sort_filenames(apps, ROWS);

	for (int i = 0; i < ROWS - 1; i++) {
		if (apps[i].active && strcmp(apps[i].filename, apps[i + 1].filename) == 0) {
			/* Two identical filenames found, keep the one found in the user directory */
			if (strcmp(apps[i].dir, system_dir) == 0) {
				/* Current entry is overridden */
				apps[i].active = 0;
			} else {
				/* Next entry is overridden */
				apps[i + 1].active = 0;
			}
		}
	}
}

int file_contains(const char *path, const char *str) {
	FILE *f = fopen(path, "r");
	char buffer[strlen(str) + 1];

	while (fgets(buffer, sizeof(buffer), f)) {
		if (strcmp(buffer, str) == 0) {
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	return 0;
}

void remove_hidden_apps(void) {
	char path[100];

	for (int i = 0; i < ROWS; i++) {
		if (apps[i].active) {
			sprintf(path, "%s/%s", apps[i].dir, apps[i].filename);
			if (file_contains(path, "NoDisplay=true")) {
				apps[i].active = 0;
			}
		}
	}
}

int count_entries(void) {
	/* This function is called in main() */
	int count = 0;

	for (int i = 0; i < ROWS; i++) {
		if (apps[i].active)
			count++;
	}

	return count;
}

void remove_newline(char *str) {
	for (int i = strlen(str) - 1; i >= 0; i--) {
		if (str[i] == '\n') {
			str[i] = '\0';
			break;
		}
	}
}

void remove_field_codes(char *str) {
	/* Removes tokens like %F and %u */
	for (int i = 0; i < strlen(str) - 1; i++) {
		if (str[i] == '%') {
			str[i] = ' ';
			str[i + 1] = ' ';
		}
	}
}

struct menuentry create_entry(const char *path) {
	struct menuentry entry = {.name = "", .cmd = ""};
	FILE *f = fopen(path, "r");
	char buffer[100];

	while (fgets(buffer, sizeof(buffer), f)
			&& (strlen(entry.name) == 0 || strlen(entry.cmd) == 0)) {
		/* As soon as we have found a name and a command, stop processing
		 * further occurrences of name and command. These are often present
		 * in custom actions, which we will not process.
		 */
		if (startswith(buffer, "Name=")) {
			remove_newline(buffer);
			strcpy(entry.name, buffer + strlen("Name="));
		} else if (startswith(buffer, "Exec=")) {
			remove_newline(buffer);
			remove_field_codes(buffer);
			strcpy(entry.cmd, buffer + strlen("Exec="));
		}
	}

	return entry;
}

void compile_entries(struct menuentry *entries) {
	char path[100];
	int entry_it = 0;

	for (int i = 0; i < ROWS; i++) {
		if (apps[i].active) {
			sprintf(path, "%s/%s", apps[i].dir, apps[i].filename);
			entries[entry_it] = create_entry(path);
			entry_it++;
		}
	}
}

int entries_cmp_func(const void *a, const void *b) {
	/* Dark magic of sorting strings in C */
	return strcmp(((struct menuentry *)a)->name, ((struct menuentry *)b)->name);
}

void sort_entries(struct menuentry *entries, int size) {
	qsort(entries, size, sizeof(struct menuentry), entries_cmp_func);
}

void create_menu_input(char *menu_input, struct menuentry *entries, int size) {
	for (int i = 0; i < size; i++) {
		strcat(menu_input, entries[i].name);
		strcat(menu_input, "\n");
	}
}

void launch_menu(const char *menu_input, char *response) {
	int pipe_to_menu[2], pipe_from_menu[2];
	pid_t p;

	pipe(pipe_to_menu);
	pipe(pipe_from_menu);
	p = fork();

	if (p > 0) {
		/* Parent */
		close(pipe_to_menu[0]);
		close(pipe_from_menu[1]);

		write(pipe_to_menu[1], menu_input, strlen(menu_input));
		close(pipe_to_menu[1]); /* Required, or else program will hang */

		wait(NULL);

		read(pipe_from_menu[0], response, 100);
		close(pipe_from_menu[0]);

		remove_newline(response);
	} else if (p == 0) {
		/* Child */
		close(pipe_to_menu[1]); /* Required */
		close(pipe_from_menu[0]);

		dup2(pipe_to_menu[0], STDIN_FILENO);
		close(pipe_to_menu[0]);

		dup2(pipe_from_menu[1], STDOUT_FILENO);
		close(pipe_from_menu[1]);

		execvp(menu, menu_arguments);
	}
}

void launch_program(const char *response, struct menuentry *entries, int size) {
	pid_t p;

	for (int i = 0; i < size; i++) {
		if (strcmp(entries[i].name, response) == 0 && (p = fork()) == 0) {
			/* Child process */
			char *const arguments[] = {"bash", "-c", entries[i].cmd, NULL};
			execvp("bash", arguments);
		}
	}
}

int main(void) {
	struct passwd *pw = getpwuid(getuid());
	sprintf(user_dir, "/home/%s/.local/share/applications", pw->pw_name);

	get_apps(system_dir);
	get_apps(user_dir);
	remove_overridden_files();
	remove_hidden_apps();

	int size = count_entries();
	struct menuentry entries[size];
	char menu_input[1000] = ""; /* Remove bogus chars from string */
	char response[100] = "";

	compile_entries(entries);
	sort_entries(entries, size);
	create_menu_input(menu_input, entries, size);
	launch_menu(menu_input, response);
	launch_program(response, entries, size);

	return 0;
}
