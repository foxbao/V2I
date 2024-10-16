#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "inc/launch.h"

static void init_daemon(void)
{
	fprintf(stderr, "zlauncher: running as daemon.\n");

	// disable signals having impact to control terminal
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	// run in background
	int pid = fork();
	if (pid) {
		// this is parents
		exit(0);
	}
	else if (pid < 0) {
		perror("fork");
		exit(2);
	}

	if (setsid() < 0) {
		perror("setsid");
		exit(3);
	}

	chdir("/tmp");

	int fd;	
	if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) close(fd);
    }

	umask(0);
	signal(SIGCHLD, SIG_IGN);
}

static void help(const char *name)
{
	fprintf(stderr, "Usage: %s [args...] [-- [args..]]\n", name);
	fprintf(stderr, "  -d, --daemon    Run the system_server as a daemon.\n");
	fprintf(stderr, "  -h, --help      Display this help message\n");
}

int main(int argc, char* argv[])
{
	int i, c;
	struct option opts[] = {
		{ "daemon", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ 0, 0, NULL, 0 }};

	while ((c = getopt_long(argc, argv, "dh", opts, &i)) != -1)
	{
		switch (c) {
		case 0: /* no need to handle here */
			break;

		case 'd': init_daemon();
			break;

		case 'h': help("zlaunch");
			return 0;

		default: return 1;
		}
	}
	int ret = zas::system::launch().run(argc, argv);
	return ret;
}

/* EOF */
