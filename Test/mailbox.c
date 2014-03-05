/**
 * Daniel Benson(djbenson) and Michael Racine(mrracine)
 * Project4
 */

#include "mailbox.h"

#define __NR_cs3013_syscall1 349
#define __NR_cs3013_syscall2 350
#define __NR_cs3013_syscall3 351

/**
 * Functions for msgs
 * 
 * */
long SendMsg(pid_t dest, void *msg, int len, bool block) {
  return syscall(__NR_cs3013_syscall1, dest, msg, len, block);
} 	// int SendMsg

long RcvMsg(pid_t *sender, void *msg, int *len, bool block){
  return syscall(__NR_cs3013_syscall2, sender, msg, len, block);
}	// int RcvMsg

/**
 * functions for maintaining mailboxes
 * 
 * */
long ManageMailbox(bool stop, int *count){
  return syscall(__NR_cs3013_syscall3, stop, count);
}	// int ManageMailbox



