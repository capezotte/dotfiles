#include <stdio.h>
#include <stdlib.h>

int main() {
	long free_perc, total, avail;
	int r;
	if(!freopen("/proc/meminfo", "r", stdin))
		return 1;
	if (scanf("%*s  %ld %*s"
	          "%*s %*ld %*s"
	          "%*s  %ld %*s",
	          &total, &avail) != 2)
		return 1;
	free_perc = (long)(100*(double)avail/(double)total);
	r = printf("%ld%%\n", 100 - free_perc);
	return !(r >= 0);
}
