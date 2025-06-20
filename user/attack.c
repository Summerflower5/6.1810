#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  const char *target = "my very very very secret pw is:   ";
  char *end = sbrk(PGSIZE*32);
  for(int i = 0 ; i < 32 ; i++){
    int match = 0;
    for(int j = 0 ; j < strlen(target) ; j++){
      if(*(end + PGSIZE*i + j) == target[j]){
        match++;
      }
    }
    if(match > strlen(target)>>1){
      write(2, end + PGSIZE*i + 32, 8);
      break;
    }
  }

  exit(0);
}
