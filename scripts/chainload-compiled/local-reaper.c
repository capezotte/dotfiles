#include <unistd.h>
#include <stdio.h>

#include <sys/prctl.h>

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fputs("Usage: local-reaper true|enable|false|disable prog...\n", stderr);
		return -1;
	}

	unsigned long is_sr = 1;
	switch (*argv[1]) {
		case 'f':
		case 'd':
			is_sr = 0;
		case 't':
		case 'e':
			break;
		default:
			fputs("Expected true/false or enable/disable as the argument.\n", stderr);
			return -1;
	}

	if (prctl(PR_SET_CHILD_SUBREAPER, is_sr, 0, 0, 0)) {
		perror("failed to become subreaper");
		return -1;
	}
	
	execvp(argv[2], &argv[2]);

	perror("failed to exec");
	return -1;
}
