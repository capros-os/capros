#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>

struct sockaddr_un addr;
const char *pipename;

/* This is here only to suppress warnings about tempnam() */
const char *mypipename()
{
  struct stat statbuf;
  static char thePipeName[20];
  static int i = 0;

  for(;;) {
    if (i == 65536)
      i = 0;

    sprintf(thePipeName, "/tmp/vmtty-%d", i);
    i++;

    if (stat(thePipeName, &statbuf) == -1 && errno == ENOENT)
      return thePipeName;
  }
}

const char *str_append(const char *s1, const char *s2)
{
  char *s;
  size_t len = strlen(s1);
  len += strlen(s2) + 1;
  s = calloc(len, 1);
  strcpy(s, s1);
  strcat(s, s2);

  return s;
}

int
make_unix_pipe()
{
  int sockfd;

  for(;;) {
    int len = sizeof(struct sockaddr_un);
    pipename = mypipename();

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pipename);

    if (bind(sockfd, (struct sockaddr *)&addr, len)) {
      fprintf(stderr, "Could not bind \"%s\": %s\n", pipename, strerror(errno));
      close(sockfd);
      continue;
    }

    return sockfd;
  }
}


int sig_int = 0;

void
set_sig_int(int signo)
{
  sig_int = 1;
}

int
main(int argc, char **argv)
{
  int tty = open("/dev/tty", O_RDWR);
  int sockfd = make_unix_pipe();
  int done = 0;
  int sig_int = 0;
  fd_set input_set;
  struct termios tstate;

  {
    struct sigaction act, cact;
    act.sa_handler = set_sig_int;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, &cact);
  }

  {
    struct termios new_tstate;
    tcgetattr(tty, &tstate);
    memcpy(&new_tstate, &tstate, sizeof(tstate));
    new_tstate.c_cc[VMIN] = 1;
    new_tstate.c_cc[VTIME] = 0;

    /* FIX: This setting prevents interaction with kdb.  Commenting it
        out seems to work.  Someone needs to verify this setting!
        (vandy) */
    //    new_tstate.c_lflag &= ~(ICANON|ECHO);

    tcsetattr(tty, TCSANOW, &new_tstate);
  }

  if (fork() == 0) {
    int vmware_argc = argc + 16;

    /* FIX: This is the original code: With the Linux kernel 2.4.20-24.9,
        this gives a segmentation fault.  Using a local declaration is
        mo' better, but I'm (vandy) not sure why.  Does it have to do
        with initializing the memory to 0?  */
    //    const char ** vmware_argv = malloc(sizeof(const char *) * (vmware_argc + 1));

    const char *vmware_argv[vmware_argc + 1];

    close(tty);
    close(sockfd);

    vmware_argv[0] = "vmware";
    vmware_argv[1] = "-s";
    vmware_argv[2] = "serial.v2=true";
    vmware_argv[3] = "-s";
    vmware_argv[4] = "serial0.present=TRUE";
    vmware_argv[5] = "-s";
    vmware_argv[6] = "serial0.fileType=pipe";
    vmware_argv[7] = "-s";
    vmware_argv[8] = str_append("serial0.fileName=", pipename);
    vmware_argv[9] = "-s";
    vmware_argv[10] = "serial0.pipe.endPoint=client";
    vmware_argv[11] = "-s";
    vmware_argv[12] = "serial0.tryNoRxLoss=true";

    /* FIX: Why is this memcpy starting with element 11 when the above
       statement adds the last arg in elements 11 and 12? (vandy) */
    //    memcpy(&vmware_argv[11], &argv[1], argc * sizeof(argv[0]));

    // Here the compiler warns that the arguments are supposed to be writeable:
    memcpy(&vmware_argv[13], &argv[1], argc * sizeof(argv[0]));

    execvp("vmware", vmware_argv);
  }
  else {
    int len;
    if (listen(sockfd, 1)) {
      fprintf(stderr, "Error on listen: %s\n", strerror(errno));
      kill(0, SIGKILL);
    }

    sockfd = accept(sockfd, (struct sockaddr *) &addr, &len);
  }
      

  while(!done) {
    char buf[1024];
    int res;

    FD_ZERO(&input_set);
    FD_SET(tty, &input_set);
    FD_SET(sockfd, &input_set);

    res = select(sockfd + 1, &input_set, NULL, NULL, NULL);

    if (res == 0 || res == -1 && sig_int) {
      write(sockfd, "\3", 1);
      sig_int = 0;
    }
    else if (FD_ISSET(sockfd, &input_set)) {
      res = read(sockfd, buf, sizeof(buf));
      if (res > 0)
	write(tty, buf, res);
      else if (res < 1)
	done = 1;
    }
    else if (FD_ISSET(tty, &input_set)) {
      res = read(tty, buf, sizeof(buf));
      if (res > 1)
	write(sockfd, buf, res);
    }
  }

  /* Restore terminal modes: */
  tcsetattr(tty, TCSANOW, &tstate);
  unlink(pipename);
  exit(0);
}
