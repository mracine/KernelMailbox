/* Daniel Benson(djbenson) and Michael Racine(mrracine)
 * Project4
 */

#include "mailbox.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CHILD_NUM 50

int main() {
  int childCounter;
  
  for(childCounter = 0; childCounter < CHILD_NUM; childCounter++) {
    int childPID = fork();
    
    if(childPID == 0){
      pid_t sender;
      void *msg[128];
      int len;
      bool block = true;
      
      sleep(0.1);

      RcvMsg(&sender,msg,&len,block);
      
      printf("Message: %s\n", (char *)msg);
      char myMesg[] = "I am your child";

      if(SendMsg(sender, myMesg, 16, block)) {
        printf("Child send failed.\n");
      }
      
      return 0;
    }

    else{
      char mesg[] = "I am your father";

      if (SendMsg(childPID, mesg, 17, false)){
        printf("Send failed\n");
      }

      wait(&childPID);
    }
  }
  
  int msgCounter;
  for(msgCounter = 0; msgCounter < CHILD_NUM; msgCounter++) {
    pid_t aSender;
    void *reply[128];
    int mLen;
    bool mBlock = true;

    sleep(0.1);
    
    RcvMsg(&aSender,reply,&mLen,mBlock);
    printf("Child %d, enqueued # %d Message: %s\n", aSender, msgCounter, (char *)reply);
  }

  return 0;
}
