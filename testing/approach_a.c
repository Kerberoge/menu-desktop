#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define ROWS 10000
#define COLS 5

char initial_data[ROWS][100];
char system_strings[ROWS][COLS], user_strings[ROWS][COLS];

void remove_last_newline(char *str) {
    int last = strlen(str) - 1;
    if (str[last] == '\n') {
        str[last] = '\0';
    }
}

void get_data(char (*arr)[100], char *file) {
    FILE *src_f = fopen(file, "r");
    char buffer[100];

    for (int i = 0; i < ROWS && fgets(buffer, 100, src_f); i++) {
        remove_last_newline(buffer);
        strcpy(arr[i], buffer);
    }
}

char *find_last_slash(char *str) {
    char *c = str + strlen(str) - 1;

    for (; c >= str; c--)
        if (*c == '/')
            break;

    return c;
}

void extract_data(char (*system)[COLS], char (*user)[COLS], char (*src)[100]) {
	int system_idx = 0, user_idx = 0;

    for (int i = 0; i < ROWS; i++) {
        char *delim = find_last_slash(src[i]);
        char *prefix = "/usr/share/applications";

        if (strncmp(src[i], prefix, delim - src[i] - 1) == 0) {
			strcpy(system[system_idx], delim + 1);
			system_idx++;
        } else {
			strcpy(user[user_idx], delim + 1);
			user_idx++;
        }
    }
}

int remove_duplicates(void) {
	int occurrences = 0;

    for (int system_idx = 0; system_idx < ROWS; system_idx++) {
        for (int user_idx = 0; user_idx < ROWS; user_idx++) {
            if (user_strings[user_idx][0] != '\0'
            		&& strcmp(system_strings[system_idx], user_strings[user_idx]) == 0) {
                system_strings[system_idx][0] = '\0';
                occurrences++;
                break;
            }
        }
    }

    return occurrences;
}

int main() {
	int occ;
	clock_t time_start, time_end;
	double duration;

	get_data(initial_data, "./data");
	extract_data(system_strings, user_strings, initial_data);

	time_start = clock();
	occ = remove_duplicates();
	time_end = clock();
	duration = (double) (time_end - time_start) / CLOCKS_PER_SEC;

	for (int i = 0; i < ROWS; i++) {
		if (user_strings[i][0] != '\0') {
			printf("/usr/share/applications/%s    "
				"/home/$USER/.local/share/applications/%s\n",
				system_strings[i], user_strings[i]);
		}
	}

	printf("Occurrences: %d\n", occ);
	printf("Took %f seconds\n", duration);

	return 0;
}
