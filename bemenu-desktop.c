#include <stdio.h>
#include <stdlib.h> /* for qsort() */
#include <unistd.h> /* for getlogin_r(), fork(), and execvp() */
#include <dirent.h>
#include <string.h>
#include <sys/wait.h> /* for wait() */

#define ROWS 50
#define COLS 100

#define BAR_AC "#9186db"
#define BAR_BG "#222033"
#define BAR_FG "#dddddd"

struct menuentry {
	char name[100];
	char cmd[100];
};

char *const bemenu_arguments[] = {
	"bemenu", "-i",
	"--fn", "DejaVu Sans Mono 10.5",
	"-p", "",
	"-H", "18",
	"--hp", "20",
	"--ch", "16",
	"--fb", BAR_BG,
	"--ff", BAR_FG,
	"--nb", BAR_BG,
	"--nf", BAR_FG,
	"--hb", BAR_BG,
	"--hf", BAR_AC,
	"--ab", BAR_BG,
	"--af", BAR_FG,
	NULL
};

char system_dir[] = "/usr/share/applications";
char username[20];
char user_dir[100]; /* This string is defined in main() after getting the user name */
char system_apps[ROWS][COLS], user_apps[ROWS][COLS];

int startswith(const char *str, const char *prefix) {
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

int endswith(const char *str, const char *suffix) {
	int str_len = strlen(str);
	int suf_len = strlen(suffix);

	return str_len > suf_len && strcmp(str + str_len - suf_len, suffix) == 0;
}

void add_app(char (*arr)[COLS], const char *app) {
	int position;

	for (position = 0; arr[position][0] != '\0'; position++);
	strcpy(arr[position], app);
}

void get_apps(const char *srcdir, char (*apps_arr)[COLS]) {
	DIR *applications;
	struct dirent *entry;

	applications = opendir(srcdir);

	while (entry = readdir(applications)) {
		if (endswith(entry->d_name, ".desktop")) {
			add_app(apps_arr, entry->d_name);
		}
	}

	closedir(applications);
}

void remove_overridden_files(void) {
	for (int system_idx = 0; system_idx < ROWS; system_idx++) {
		for (int user_idx = 0; user_idx < ROWS; user_idx++) {
			if (strcmp(system_apps[system_idx], user_apps[user_idx]) == 0) {
				system_apps[system_idx][0] = '\0';
				break;
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
	for (int i = 0; i < ROWS; i++) {
		/* Remove apps from both arrays in a single loop, this saves time */
		char path[100];

		if (strlen(system_apps[i]) != 0) {
			sprintf(path, "%s/%s", system_dir, system_apps[i]);
			if (file_contains(path, "NoDisplay=true")) {
				system_apps[i][0] = '\0';
			}
		}

		if (strlen(user_apps[i]) != 0) {
			sprintf(path, "%s/%s", user_dir, user_apps[i]);
			if (file_contains(path, "NoDisplay=true")) {
				user_apps[i][0] = '\0';
			}
		}
	}
}

int count_entries(void) {
	/* This function is called in main() */
	int count = 0;

	for (int i = 0; i < ROWS; i++) {
		/* Do this for both arrays in a single loop to save time */
		if (strlen(system_apps[i]) > 0)
			count++;

		if (strlen(user_apps[i]) > 0)
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
	int count = 0;

	for (int i = 0; i < ROWS; i++) {
		/* Process both system_apps and user_apps in one loop to speed up things,
		 * entries will be sorted anyways in the next step
		 */
		if (strlen(system_apps[i]) > 0) {
			sprintf(path, "%s/%s", system_dir, system_apps[i]);
			entries[count] = create_entry(path);
			count++;
		}

		if (strlen(user_apps[i]) > 0) {
			sprintf(path, "%s/%s", user_dir, user_apps[i]);
			entries[count] = create_entry(path);
			count++;
		}
	}
}

int compare_func(const void *a, const void *b) {
	/* Dark magic of sorting strings in C */
	return strcmp(((struct menuentry *)a)->name, ((struct menuentry *)b)->name);
}

void sort_entries(struct menuentry *entries, int size) {
	qsort(entries, size, sizeof(struct menuentry), compare_func);
}

void create_bemenu_input(char *bemenu_input, struct menuentry *entries, int size) {
	for (int i = 0; i < size; i++) {
		strcat(bemenu_input, entries[i].name);
		strcat(bemenu_input, "\n");
	}
}

void launch_bemenu(const char *bemenu_input, char *response) {
	int pipe_to_bemenu[2], pipe_from_bemenu[2];
	pid_t p;

	pipe(pipe_to_bemenu);
	pipe(pipe_from_bemenu);
	p = fork();

	if (p > 0) {
		/* Parent */
		close(pipe_to_bemenu[0]);
		close(pipe_from_bemenu[1]);

		write(pipe_to_bemenu[1], bemenu_input, strlen(bemenu_input));
		close(pipe_to_bemenu[1]); /* Required, or else program will hang */

		wait(NULL);

		read(pipe_from_bemenu[0], response, 100);
		close(pipe_from_bemenu[0]);

		remove_newline(response);
	} else if (p == 0) {
		/* Child */
		close(pipe_to_bemenu[1]); /* Required */
		close(pipe_from_bemenu[0]);

		dup2(pipe_to_bemenu[0], STDIN_FILENO);
		close(pipe_to_bemenu[0]);

		dup2(pipe_from_bemenu[1], STDOUT_FILENO);
		close(pipe_from_bemenu[1]);

		execvp("bemenu", bemenu_arguments);
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
	getlogin_r(username, sizeof(username));
	sprintf(user_dir, "/home/%s/.local/share/applications", username);

	get_apps(system_dir, system_apps);
	get_apps(user_dir, user_apps);
	remove_overridden_files();
	remove_hidden_apps();

	int size = count_entries();
	struct menuentry entries[size];
	char bemenu_input[1000] = ""; /* Remove bogus chars from string */
	char response[100] = "";

	compile_entries(entries);
	sort_entries(entries, size);
	create_bemenu_input(bemenu_input, entries, size);
	launch_bemenu(bemenu_input, response);
	launch_program(response, entries, size);

	return 0;
}
