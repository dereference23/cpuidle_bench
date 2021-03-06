/* Copyright (C) 2022 Alexander Winkowski <dereference23@outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* CPU count limit */
#define CLIM 128
/* Idle states count limit */
#define SLIM 32
/* This is "/sys/devices/system/cpu/cpuXXX/cpuidle/stateXX/usage" + 1 */
#define MAXPATH 53
/* The maximum number of characters unsigned long long can handle:
   18446744073709551615 (from limits.h) */
#define ULLCH 20
/* Convert seconds to microseconds */
#define S_TO_US(x) ((x) * 1000000)
/* Convert a string to an ULL interger */
#define STR_TO_ULL(x) strtoull((x), NULL, 10)

struct cpuidle_fd {
	int time[CLIM][SLIM];
	int usage[CLIM][SLIM];
};

struct cpuidle_paths {
        char time[CLIM][SLIM][MAXPATH];
        char usage[CLIM][SLIM][MAXPATH];
};

struct cpuidle_stats {
	struct {
	char time[CLIM][SLIM][ULLCH+1];
	char usage[CLIM][SLIM][ULLCH+1];
	} raw;

	unsigned long long total[CLIM]; /* overflow? */
};

int cpu_count;
int states_count;

int get_cpu_count(void);
int get_states_count(void);
void count_total_idle(struct cpuidle_stats *stats);
void open_files(struct cpuidle_paths *paths, struct cpuidle_fd *fd);
void prepare_paths(struct cpuidle_paths *paths);
void read_times(struct cpuidle_stats *stats, struct cpuidle_fd *fd);
void read_usage(struct cpuidle_stats *stats, struct cpuidle_fd *fd);

int main(int argc, char *argv[])
{
	struct cpuidle_stats stats0 = {0}, stats1 = {0};
	struct cpuidle_fd fd = {0};
	struct cpuidle_paths paths = {0};
	unsigned long long total_idle0 = 0, total_idle1 = 0; /* overflow? */
	int sample = 1;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [sample duration in seconds]\n", argv[0]);
		return 1;
	}

	if (argc == 2) {
		char *ptr;
		long temp = strtol(argv[1], &ptr, 10);
		if (*ptr == '\0') {
			if (temp > INT_MAX || temp < 1)
				fprintf(stderr, "Value %ld is out of range, using default\n", temp);
			else
				sample = (int)temp;
		} else {
			fprintf(stderr, "Sample duration should be an integer\n");
			return 1;
		}
	}

	cpu_count = get_cpu_count();
	states_count = get_states_count();

	prepare_paths(&paths);
	open_files(&paths, &fd);

	read_times(&stats0, &fd);
	sleep(sample);
	read_times(&stats1, &fd);
	read_usage(&stats1, &fd);

	count_total_idle(&stats0);
	count_total_idle(&stats1);

	for (int i = 0; i < cpu_count; ++i) {
		total_idle0 += stats0.total[i];
		total_idle1 += stats1.total[i];

		printf("\tCPU %d\n", i);
		printf("idle ratio: %.4f\n", (stats1.total[i] - stats0.total[i]) / S_TO_US(1.0 * sample));
		for (int j = 0; j < states_count; ++j) {
			printf("- state %d\n", j);
			printf("  avg: %llu\n", STR_TO_ULL(stats1.raw.usage[i][j]) ?
				(STR_TO_ULL(stats1.raw.time[i][j]) / STR_TO_ULL(stats1.raw.usage[i][j])) : 0);
			printf("  total: %llu\n", STR_TO_ULL(stats1.raw.time[i][j]));
		}
		printf("--------------------------\n");
	}

	printf("\tTotal\n");
	printf("idle ratio: %.4f\n", (total_idle1 - total_idle0) / S_TO_US(1.0 * sample * cpu_count));

	return 0;
}

int get_cpu_count(void)
{
	const char base[] = "/sys/devices/system/cpu/cpu";
	char path[MAXPATH];
	struct stat s;
	/* CPU0 always exists. */
	int i = 1;

	while (i < CLIM) {
		sprintf(path, "%s%d", base, i);
		if (stat(path, &s))
			break;
		++i;
	}

	return i;
}

int get_states_count(void)
{
	const char base[] = "/sys/devices/system/cpu/cpu0/cpuidle/state";
	char path[MAXPATH];
	struct stat s;
	/* Assume state0 exists.
	   If it doesn't exist the program
	   will fail later with a verbose error. */
	int j = 1;

	while (j < SLIM) {
		sprintf(path, "%s%d", base, j);
		if (stat(path, &s))
			break;
		++j;
	}

	return j;
}

void count_total_idle(struct cpuidle_stats *stats)
{
	for (int i = 0; i < cpu_count; ++i) {
		for (int j = 0; j < states_count; ++j)
			stats->total[i] += STR_TO_ULL(stats->raw.time[i][j]);
	}
}

void open_files(struct cpuidle_paths *paths, struct cpuidle_fd *fd)
{
	for (int i = 0; i < cpu_count; ++i) {
		for (int j = 0; j < states_count; ++j) {
			if ((fd->time[i][j] = open(paths->time[i][j], O_RDONLY)) == -1) {
				fprintf(stderr, "%s: %s\n", paths->time[i][j], strerror(errno));
				exit(errno);
			}
			if ((fd->usage[i][j] = open(paths->usage[i][j], O_RDONLY)) == -1) {
				fprintf(stderr, "%s: %s\n", paths->usage[i][j], strerror(errno));
				exit(errno);
			}
		}
	}
}

void prepare_paths(struct cpuidle_paths *paths)
{
	for (int i = 0; i < cpu_count; ++i) {
		for (int j = 0; j < states_count; ++j) {
			sprintf(paths->time[i][j], "%s%d%s%d%s",
				"/sys/devices/system/cpu/cpu", i, "/cpuidle/state", j, "/time");
			sprintf(paths->usage[i][j], "%s%d%s%d%s",
				"/sys/devices/system/cpu/cpu", i, "/cpuidle/state", j, "/usage");
		}
	}
}

void read_times(struct cpuidle_stats *stats, struct cpuidle_fd *fd)
{
	for (int i = 0; i < cpu_count; ++i) {
		for (int j = 0; j < states_count; ++j) {
			lseek(fd->time[i][j], 0, 0);
			if (read(fd->time[i][j], stats->raw.time[i][j], ULLCH) == -1)
				exit(errno);
			stats->raw.time[i][j][ULLCH] = '\0';
		}
	}
}

void read_usage(struct cpuidle_stats *stats, struct cpuidle_fd *fd)
{
	for (int i = 0; i < cpu_count; ++i) {
		for (int j = 0; j < states_count; ++j) {
			if (read(fd->usage[i][j], stats->raw.usage[i][j], ULLCH) == -1)
				exit(errno);
			stats->raw.usage[i][j][ULLCH] = '\0';
		}
	}
}
