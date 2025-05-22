#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void primes(int p_to_c[2]) __attribute__((noreturn));
void primes(int p_to_c[2]){
    int num;
    int p;
    close(p_to_c[1]);
    int c_to_s[2];
    if(read(p_to_c[0], &num, sizeof(int)) == sizeof(int)){
        printf("prime %d\n", num);
        p = num;
        if(pipe(c_to_s) < 0){
            fprintf(2, "primes: pipe error in primes\n");
            exit(1);
        }
        
        if(fork() == 0){
            close(p_to_c[0]);   // Attention !!! close all grandparent pipe
            primes(c_to_s);
            exit(0);
        }

        close(c_to_s[0]);
        while (read(p_to_c[0], &num, sizeof(int)) == sizeof(int))
        {
            if(num % p != 0){
                write(c_to_s[1], &num, sizeof(int));
            }
        }
        close(c_to_s[1]);
    }
    close(p_to_c[0]);
    wait(0);
    exit(0);
}

int
main(int argc, char *argv[])
{
  int p_to_c[2];
  if(pipe(p_to_c) < 0){
    fprintf(2, "primes: pipe error\n");
    exit(1);
  }

  if(fork() == 0){ // child process
    //all the child process do the same thing, so here we do recurison
    primes(p_to_c);
  }
  // parent process
  close(p_to_c[0]);
  for (int i = 2 ; i <= 280 ; i++){
    write(p_to_c[1], &i, sizeof(int));  // the pipe is sync, so here we use &i
  }
  close(p_to_c[1]);
  wait(0);
  exit(0);
}
