#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char buf[1];
  int p_to_c[2];
  int c_to_p[2];
  if(pipe(p_to_c) < 0 || pipe(c_to_p) < 0){
    fprintf(2, "pingpong: pipe error\n");
    exit(1);
  } 

  int pid;
  pid = fork();
  if(pid < 0){
    fprintf(2, "pingpong: fork error\n");
    exit(1);
  }

  if(pid == 0){ // child process
    close(p_to_c[1]);
    close(c_to_p[0]);
    read(p_to_c[0], buf, 1);
    printf("%d: received ping\n", getpid());
    write(c_to_p[1], buf, 1);
    close(p_to_c[0]);
    close(c_to_p[1]);
    exit(0);
  }

  // parent process
  buf[0] = 'f';
  close(p_to_c[0]);
  close(c_to_p[1]);
  write(p_to_c[1], buf, 1);
  read(c_to_p[0], buf, 1);
  printf("%d: received pong\n", getpid());
  close(p_to_c[1]);
  close(c_to_p[0]);

  wait(0);
  exit(0);
}
