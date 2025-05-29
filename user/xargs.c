#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

#define MAXARG       32  // max exec arguments
#define MAXBUFSIZE      512 // max buf size

void
execute(char *argv[]){
    if(fork() == 0){
        exec(argv[0], argv);
        fprintf(2, "xargs: exec %s failed\n", argv[0]);
        exit(1);
    }
    wait(0);
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "xargs: parameter mismatch. Usage: xargs <command> [parameters...]\n");
    exit(1);
  }

  char *args[MAXARG];
  char buf[MAXBUFSIZE], *p;

  for(int i = 1 ; i < argc ; i++){
    args[i-1] = argv[i];
  }

  int args_count = argc - 1;
  int n;
  while ((n = read(0, buf, MAXBUFSIZE)) > 0){
    p = buf;
    char *end = buf + n;

    while(p < end){
      char *e = p;
      
      while (e < end && *e != '\n') e++;
      
      char *brk = p;
      while(brk < e){
        while(brk < e && *brk != ' ') brk++; // 一开始写 while(*brk != ' ') 出现bug 因为会把'\n'读了 导致会把所有的读完 作为参数只执行一次

        if(args_count >= MAXARG - 1){
          fprintf(2, "xargs: too many parameters\n");
          exit(1);
        }

        *brk = '\0';
        args[args_count] = malloc(brk - p + 1);
        if (!args[args_count]) {
          fprintf(2, "xargs: malloc failed\n");
          exit(1);
        }

        strcpy(args[args_count], p);
        args_count++;

        brk++;
        p = brk;
      }

      if(brk > p && brk == e){
        *brk = '\0';
        args[args_count] = malloc(brk - p + 1);
        if (!args[args_count]) {
          fprintf(2, "xargs: malloc failed\n");
          exit(1);
        }

        strcpy(args[args_count], p);
        args_count++;

        brk++;
        p = brk;
      }

      args[args_count] = 0;
      execute(args);
      while (args_count > argc - 1)
      {
        free(args[--args_count]);
      }
      
    }
  }
  

  exit(0);
}
