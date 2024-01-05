#define _POSIX_C_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <limits.h>
#include <dirent.h>

int isconf(const struct dirent *a) {
	char *ext = strrchr(a->d_name, '.');
	if (ext == NULL) return 0;
	return !strcmp(ext, ".conf");
}

char *xstrdup(const char *s) {
	char *r = strdup(s);
	if (r == NULL) err(1, "can't duplicate string");
	return r;
}

/* scan a configuration directory */
int scanconf(const char *path, char ***ret) {
	struct dirent **conf;
	int n = scandir(path, &conf, isconf, NULL);

	if (n == -1) {
		warn("ignoring folder %s", path);
		return 0;
	}

	*ret = calloc(n, sizeof(char *));
	for (int i = 0; i < n; i++) {
		char tmp[PATH_MAX];
		sprintf(tmp, "%s/%s", path, conf[i]->d_name);
		(*ret)[i] = xstrdup(tmp);
		free(conf[i]);
	}
	free(conf);
	return n;
}

int modcomp(const void *p1, const void *p2) {
	const char *s1 = *(const char **)p1;
	const char *s2 = *(const char **)p2;
	/* compare by basename first */
	int r1 = strcmp(strrchr(s1, '/'), strrchr(s2, '/'));
	if (r1) return r1;
	/* coincidentally /etc, /run and /usr are in alphabetical order */
	return strcmp(s1, s2);
}

int main(int argc, char **argv) {
	FILE *proc_cmdline;
	char cmdline[4096],  /* contents of kernel command line;
	                        should be big enough for all arches */
	     *arg, *module,  /* modules found in it */
	     *modules[4096]; /* modprobe command-line to exec into */
	size_t mod_i = 0;

	/* Initialize argument list */
	modules[mod_i++] = "modprobe";
	modules[mod_i++] = "-ab";

	/* Read modules from kernel command line */
	proc_cmdline = fopen("/proc/cmdline", "r");
	if (proc_cmdline == NULL) err(1, "can't open /proc/cmdline");
	if (fgets(cmdline, 4096, proc_cmdline) == NULL) err(1, "failed to read kernel command line");
	arg = strtok(cmdline, " \n");
	while (arg) {
		if (strncmp(arg, "rd.", 3) == 0)
			arg += 3;
		if (strncmp(arg, "modules-load=", 13) == 0)
			arg += 13;
		else
			goto next_arg;

		module = strtok(NULL, ",");
		while (module) {
			modules[mod_i++] = module;
			module = strtok(NULL, ",");
		}
next_arg:
		arg = strtok(NULL, " \n");
	}

	/* Scan conf files */
	/* Number of files */
	int n = 0;
	/* Lists of files containing modules */
	char **mods = NULL;
	/* Where module configurations are stored */
        char *module_locations[] = {
		"/etc/modules-load.d",
		"/run/modules-load.d",
		"/usr/lib/modules-load.d"
	};
	for (size_t i = 0; i < 3; i++) {
		char **new_mods;
		int j = scanconf(module_locations[i], &new_mods);
		mods = realloc(mods, (n + j) * sizeof(char *));
		if (mods == NULL) err(1, "can't expand array");
		if (j > 0) {
			memcpy(&mods[n], new_mods, j * sizeof(char *));
			n += j;
			free(new_mods);
		}
	}

	/* Sort and use awk-tier strategies */
	char *last_basename = "";  /* for enforcing "each basename only once" requirement */
	qsort(mods, n, sizeof(char *), modcomp);
	for (size_t i = 0; i < n; i++) {
		FILE *mod;
		char *line = NULL, *new_basename = strrchr(mods[i], '/');
		size_t len;

		/* Ignore repeat basenames */
		if (strcmp(last_basename, new_basename))
			last_basename = new_basename;
		else
			continue;
		mod = fopen(mods[i], "r");
		if (mod == NULL) {
			warn("ignoring file %s", mods[i]);
			continue;
		}
		while (getline(&line, &len, mod) > 0) {
			if (*line == 0 || *line == '#' || *line == ';') goto next_line;
			strtok(line, "\n"); /* trim abuse */
			modules[mod_i++] = xstrdup(line);
next_line:
			free(line);
			line = NULL;
		}
		if (ferror(mod)) {
			warn("failed to read lines from %s", mods[i]);
			goto next_file;
		}
next_file:
		if (line) free(line);
		fclose(mod);
	}

	/* load the modules */
	modules[mod_i++] = NULL;
	execvp(modules[0], modules);

	perror("execvp");
	return 1;
}
