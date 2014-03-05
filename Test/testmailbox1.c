/* Daniel Benson(djbenson) and Michael Racine(mrracine)
 * Project4
 */

#include "mailbox.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
  int childPID = fork();

  if(childPID == 0){
    pid_t sender;
    void *msg[128];
    int len;
    bool block = true;
    sleep(1);
    RcvMsg(&sender,msg,&len,block);
    printf("Message received.\n");
    printf("Message: %s\n", (char *) msg);
  }

  else{
    char mesg[] = "I am your father";
    printf("Sending Message to child.\n");

    if (SendMsg(childPID, mesg, 17, false)){
      printf("Send failed\n");
    }

    wait(&childPID);
  }

  return 0;
}
