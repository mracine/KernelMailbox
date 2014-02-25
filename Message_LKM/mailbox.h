/* Daniel Benson (djbenson) and Michael Racine (mrracine)
 * Project4
 */

#ifndef __MAILBOX__
#define __MAILBOX__

#include <stdbool.h>
#include <linux/types.h>
#include <linux/slab.h>

#define NO_BLOCK 0
#define BLOCK   1
#define MAX_MSG_SIZE 128
#define MAX_MAILBOX_SIZE 64
#define MAX_MAILBOXES 32
#define FALSE 0
#define TRUE 1

/**
 * Functions for msgs
 * 
 * */
asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block);
asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block);

/**
 * Functions for maintaining mailboxes
 * 
 * */
asmlinkage long ManageMailbox(bool stop, int *count);
/**
 * Hashtable implementation
 *
 * */
typedef struct message_s {
	char *msg;
	int len;
} message;
 
typedef struct mailbox_s {
	int key;
	int msgNum;
	bool stopped;
	message **messages;
	struct mailbox_s *next;
} mailbox; // struct mailbox

typedef struct hashtable_s {
	int size;
	int boxNum;
	mailbox **mailboxes;
} hashtable; // struct hashtable



hashtable *create(void); // Initialize table to 16 mailboxes
int insert(hashtable *h, int key);
mailbox *getBox(hashtable *h, int key);
int remove(hashtable *h, int key);
mailbox *createMailbox(int key);
int insertMsg(int dest, char *msg, int len, bool block);
int removeMsg(int *sender, void *msg, int *len, bool block);

extern struct kmem_cache *cache;
extern hashtable *ht;

/**
 * error codes pertaining to mailboxes
 * 
 * */
#define MAILBOX_FULL		1001
#define MAILBOX_EMPTY		1002
#define MAILBOX_STOPPED		1003
#define MAILBOX_INVALID		1004
#define MSG_LENGTH_ERROR	1005
#define MSG_ARG_ERROR		1006
#define MAILBOX_ERROR		1007

#endif
