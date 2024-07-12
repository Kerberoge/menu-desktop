#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define ROWS 10000
#define COLS 5

struct path {
	char dir[100];
	char filename[COLS];
};

char initial_data[ROWS][100];
struct path app_paths[ROWS];

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

void extract_data(struct path *dest, char (*src)[100], int rows) {
	for (int i = 0; i < rows; i++) {
		char *delim = find_last_slash(src[i]);

		strncpy(dest[i].dir, src[i], delim - &src[i][0]);
		strcpy(dest[i].filename, delim + 1);
	}
}

int filenames_compare_func(const void *a, const void *b) {
	return strcmp(((struct path *)a)->filename, ((struct path *)b)->filename);
}

void sort_filenames(struct path *paths, int size) {
	qsort(paths, size, sizeof(struct path), filenames_compare_func);
}

int remove_duplicates(struct path *arr, int size) {
	int occurrences = 0;

	sort_filenames(arr, size);

	for (int i = 0; i < size - 1; i++) {
		if (arr[i].filename[0] != '\0'
				&& strcmp(arr[i].filename, arr[i + 1].filename) == 0
				&& strcmp(arr[i].dir, arr[i + 1].dir) != 0) {
			/* Two identical filenames found, keep the one with
			 * /home/$USER/.local/share/applications in the path
			 */
			if (strcmp(arr[i].dir, "/usr/share/applications") == 0) {
				arr[i].filename[0] = '\0';
			} else {
				arr[i + 1].filename[0] = '\0';
			}

			occurrences++;
		}
	}

	return occurrences;
}

int main() {
	int occ;
	clock_t time_start, time_end;
	double duration;

	get_data(initial_data, "./data");
	extract_data(app_paths, initial_data, ROWS);

	time_start = clock();
	occ = remove_duplicates(app_paths, ROWS);
	time_end = clock();
	duration = (double) (time_end - time_start) / CLOCKS_PER_SEC;

	for (int i = 0; i < ROWS; i++) {
		printf("%s/%s\n", app_paths[i].dir, app_paths[i].filename);
	}

	printf("Occurrences: %d\n", occ);
	printf("Took %f seconds\n", duration);

	return 0;
}
