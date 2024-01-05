#define _BSD_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define SZ(x) (sizeof(x)/sizeof(*x))

int main(int argc, char **argv) {
	long sum, vals[10], oldsum, oldvals[10] = {0};
	double answer;
	int r, sleep_int = 1;
	FILE *stat;

	if (argc == 2)
		sleep_int = atoi(argv[1]);
	if (sleep_int == 0)
		sleep_int = 1;

	stat = fopen("/proc/stat", "r");
	setlinebuf(stdout);
	if (!stat) return 1;
	do {
		/* cpu field scanf */
		r = fscanf(stat,
			"%*s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
			&vals[0], &vals[1], &vals[2], &vals[3], &vals[4],
			&vals[5], &vals[6], &vals[7], &vals[8], &vals[9]);
		if (r != SZ(vals)) {
			return 1;
		}
		/* get sum */
		sum = 0;
		for (int i = 0; i < SZ(vals); i++)
			sum += vals[i];
		/* compute everything - idle */
		answer = (double)(vals[3] + vals[4] - oldvals[3] - oldvals[4])/(double)(sum-oldsum);
		answer *= 100; answer = 100 - answer;
		r = printf(answer >= 10 ? "%4.1f%%\n" : "%1.2f%%\n", answer);
		fflush(stdout);
		if (r < 0)
			return 1;
		/* backup old */
		for (int i = 0; i < SZ(vals); i++)
			oldvals[i] = vals[i];
		oldsum = sum;
		/* wait and compare */
		sleep(sleep_int);
		stat = freopen("/proc/stat", "r", stat);
	} while (stat != NULL);

	return 1;
}
