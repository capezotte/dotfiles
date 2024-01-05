#include <sys/socket.h>
#include <skalibs/stddjb.h>
#include <execline/execline.h>

static const char *USAGE = "socket-helper [ -w ] name1:socket1.sock name2:socket2.sock ... \"\" prog";

#define MKENV(cname, envname, size) \
	char cname##_env[sizeof(envname)+(size)+1] = envname "=", \
	     *cname = cname##_env + sizeof(envname)
#define FORMAT(type, val, store) (store)[type##_fmt((store), (val))] = 0
#define dieusage() strerr_dieusage(100, USAGE)

int main(int argc, char const **argv) {
	PROG = "socket-helper";
	unsigned int wait = 0;
	{
		subgetopt l = SUBGETOPT_ZERO;
		for (;;) {
			int opt = subgetopt_r(argc, argv, "w", &l);
			if (opt == -1) break;
			switch (opt) {
				case 'w': wait = 1; break;
				default: dieusage();
			}
		}
		argc -= l.ind; argv += l.ind;
	}

	stralloc fdnames = STRALLOC_ZERO;
	MKENV(fdcount, "LISTEN_FDS", INT_FMT);
	MKENV(listenpid, "LISTEN_PID", PID_FMT);
	/* ok so el_semicolon does mutate argv */
	int argc_block = el_semicolon(argv);
	if (argc_block >= argc)
		strerr_dief1x(100, "unterminated block");
	int socks[argc_block], sock_count = argc_block;

	while (argc_block--) {
		const int cur_sock = sock_count - argc_block - 1;
		size_t len_str = str_len(*argv);
		size_t colon_at = byte_chr(*argv, len_str, ':');

		if (!(stralloc_cats(&fdnames, cur_sock == 0 ? "LISTEN_FDNAMES=" : ":") &&
					stralloc_catb(&fdnames, *argv, colon_at)))
			strerr_diefu1sys(111, "allocate memory");

		if (len_str == 0 || colon_at >= len_str - 1)
			dieusage();
		socks[cur_sock] = ipc_stream();
		if (socks[cur_sock] != cur_sock + 3) {
			char overwritten[INT_FMT];
			FORMAT(int, cur_sock + 3, overwritten);
			if (fd_move(socks[cur_sock], cur_sock + 3) == -1)
				strerr_diefu1sys(111, "move fd");
			strerr_warnw3x("fd #", overwritten, " was overwritten");
			socks[cur_sock] = cur_sock + 3;
		}
		if (socks[cur_sock] == -1)
			strerr_diefu1sys(111, "create socket");
		if (ipc_bind_reuse(socks[cur_sock], &(*argv)[colon_at+1]) == -1)
			strerr_diefu1sys(111, "bind socket");
		if (ipc_listen(socks[cur_sock], SOMAXCONN) == -1)
			strerr_diefu1sys(111, "make socket listening");
		argc--; argv++;
	}
	/* eat the end of the block */
	argc--; argv++;

	if (!stralloc_0(&fdnames))
		strerr_diefu1sys(111, "allocate memory");

	FORMAT(int, sock_count, fdcount);
	FORMAT(pid, getpid(), listenpid);
	if (putenv(fdnames.s) || putenv(fdcount_env) || putenv(listenpid_env))
		strerr_diefu1sys(111, "set the environment");

	if (argc > 0) {
		if (wait) {
			struct pollfd pfds[sock_count];
			for (int i = 0; i < sock_count; i++) {
				pfds[i].fd = socks[i];
				pfds[i].events = POLLIN;
			}
			poll(pfds, sock_count, -1);
		}
		xexec(argv);
		strerr_diefu2sys(111, "exec ", argv[0]);
	} else {
		dieusage();
	}
}
