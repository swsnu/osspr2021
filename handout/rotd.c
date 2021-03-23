#define SYSCALL_SET_ORIENTATION 399

#include <signal.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int notFinished = 1;

void term(int signum)
{
	notFinished = 0;
}

void sensor()
{
	/* setup for handling SIGTERM signal */
	struct sigaction action;
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = term;
	if (sigaction(SIGTERM, &action, NULL) == -1)
		exit(-1);
	
	int degree = 0;
	while (notFinished) {
		degree = (degree + 30) % 360;
		syscall(SYSCALL_SET_ORIENTATION, degree);
		sleep(2);
	}
}

int main()
{
	pid_t pid, sid;

	/* fork to re-parent */
	pid = fork();
	if (pid == -1)
		exit(-1); /* fork failed */
	if (pid > 0)
		exit(0); /* parent process exits */
	/* the child process has no parent now,
	   so the system re-parents it with init */

	/* start new session to detatch from controlling tty */
	sid = setsid();
	if (sid < 0)
		exit(-1); /* unable to start new session */
	/* child process is session leader */

	/* fork again to prevent process from re-acquring tty */
	pid = fork();
	if (pid == -1)
		exit(-1); /* fork failed */
	if (pid > 0)
		exit(0);
	/* the grandchild process is re-parented with init,
	   and is not a session leader */

	/* reset permissions */
	umask(0);

	/* move to safe directory that is not unmountable */
	if (chdir("/") < 0)
		exit(-1);

	/* close file desciptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* start daemon function */
	sensor();

	return 0;
}
