// Daniel Benson (djbenson) and Michael Racine (mrracine)
// Project4

// We need to define __KERNEL__ and MODULE to be in Kernel space
// If they are defined, undefined them and define them again:
#undef __KERNEL__
#undef MODULE

#define __KERNEL__ 
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mailbox.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#define MAX_MSG_SIZE 128
#define MAX_MAILBOX_SIZE 64
#define MAX_MAILBOXES 32

asmlinkage long MailboxExit(int error_code);
asmlinkage long MailboxExitGroup(int error_code);

void doExit(void);
struct hashtable *create(void);
struct mailbox *createMailbox(pid_t key);
int insertMsg(pid_t dest, void *msg, int len, bool block);
int removeMsg(pid_t *sender, void *msg, int *len, bool block);
int insert(pid_t key);
struct mailbox *getBox(pid_t key);
int remove(struct mailbox *m);

unsigned long **sys_call_table;
struct kmem_cache *mailbox_cache = NULL;
struct kmem_cache *message_cache = NULL;
struct hashtable *ht = NULL;

struct message {
	int len;
	pid_t sender;
	char msg[MAX_MSG_SIZE];
};

struct mailbox {
	pid_t key; // PID
	int msgNum; // Number of messages
	int ref_counter; // Ref counter for when a mailbox is stopped
	bool stopped;
	struct message *messages[64];
	struct mailbox *next;
	wait_queue_head_t queue;
	spinlock_t lock;
}; // struct mailbox

struct hashtable {
	int size;
	int boxNum;
	struct mailbox **mailboxes;
	spinlock_t main_lock;
}; // struct hashtable

asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);
asmlinkage long (*ref_sys_exit)(int error_code);
asmlinkage long (*ref_sys_exit_group)(int error_code);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	int err;
	int destination = (int) dest;

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(struct mailbox) + sizeof(struct message *)*64, 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(struct message), 0, 0, NULL);
	}

	err = insertMsg(destination, msg, len, block);

	// Error in sending the message
	//if(err != 0){
	//	printk(KERN_INFO "SendMsg: Error inserting message, error code %d\n", err);
	//	return err;
	//}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	int err;
	printk(KERN_INFO "RcvMsg: Receiving from PID %d", current->pid);

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(struct mailbox) + sizeof(struct message *)*64, 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(struct message), 0, 0, NULL);
	}

	err = removeMsg(sender, msg, len, block);

	// Error in removing the message
	if(err != 0){
		printk(KERN_INFO "RcvMsg: Error removing message, error code %d\n", err);
		return err;
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

asmlinkage long ManageMailbox(bool stop, int *count){
	struct mailbox *m = getBox(current->pid);

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(struct mailbox) + sizeof(struct message *)*64, 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(struct message), 0, 0, NULL);
	}

	if(m == NULL){
		return MAILBOX_INVALID;
	}

	copy_to_user(count, &m->msgNum, sizeof(int)); // Copy the count to user
	m->stopped = stop; // Copy boolean value from user
	printk(KERN_INFO "ManageMailbox: Changed PID %d stopped variable to: %d\n", current->pid, m->stopped);
	return 0;
}	// asmlinkage long ManageMailbox(bool stop, int *count)


asmlinkage long MailboxExit(int error_code){
	return ref_sys_exit(error_code);
} // asmlinkage long MailboxExit(void)

asmlinkage long MailboxExitGroup(int error_code){
	//doExit();
	return ref_sys_exit_group(error_code);
} // asmlinkage long MailboxExitGroup(int error_code)

void doExit(void){
	struct mailbox *m = getBox(current->pid);
	remove(m);
}

struct hashtable *create(void){
	int i;
	struct hashtable *newHash;

	// Allocate space for hashtable
	if((newHash = (struct hashtable *)kmalloc(sizeof(struct hashtable), GFP_KERNEL)) == NULL){
		return NULL;
	}

	// Allocate space for pointer of mailboxes
	if((newHash->mailboxes = (struct mailbox **)kmalloc(sizeof(struct mailbox *)*64, GFP_KERNEL)) == NULL){
		return NULL;
	}

	// Initialize hashtable
	for(i = 0; i < 64; i++){
		newHash->mailboxes[i] = NULL;
	}

	newHash->size = 64; // Allocate size to 64 mailbox pointers
	newHash->boxNum = 0; // Initialize number of mailboxes
	spin_lock_init(&newHash->main_lock);
	return newHash;
} // hashtable *create(void)

struct mailbox *createMailbox(pid_t key){
	int i;
	struct mailbox *newBox = (struct mailbox *)kmem_cache_alloc(mailbox_cache, GFP_KERNEL); // Allocate mailbox from cache
	
	spin_lock(&ht->main_lock);

	// Init values
	newBox->key = key;
	newBox->ref_counter = 0;
	newBox->msgNum = 0;
	newBox->stopped = false;
	newBox->next = NULL;
	init_waitqueue_head(&newBox->queue);
	spin_lock_init(&newBox->lock);
	
	// Initialize messages to NULL
	for(i = 0; i < MAX_MAILBOX_SIZE; i++){
		newBox->messages[i] = NULL;
	}

	// Check size of hashtable to see if there is room for a new mailbox pointer
	if(ht->size == ht->boxNum){
		printk(KERN_INFO "createMailbox: Need more space in hashtable for new mailbox. Reallocating...\n");
		ht->mailboxes = (struct mailbox **)krealloc(ht->mailboxes, 2*ht->size*sizeof(struct mailbox *), GFP_KERNEL);
		ht->size = 2*ht->size;
	}

	// Insert at the first null pointer
	for(i = 0; i < ht->size; i++){
		if(ht->mailboxes[i] == NULL){
			ht->mailboxes[i] = newBox;
			break;
		}
	}

	// Increment hashtable allocated size
	// Race condition here
	ht->boxNum++;
	spin_unlock(&ht->main_lock);
	return newBox;
} // mailbox *createMailbox(int key)

int insertMsg(pid_t dest, void *msg, int len, bool block){
	struct mailbox *m = getBox(dest);
	struct message *newMsg = NULL;

	printk(KERN_INFO "*************************** insertMsg *****************************\n");

	// Mailbox does not exist in hashtable
	if(m == NULL){
		printk(KERN_INFO "insertMsg: Mailbox doesnt exist, creating new\n");
		m = createMailbox(dest);
	}

	spin_lock(&m->lock);

	if (m->stopped){
		spin_unlock(&m->lock);
		return MAILBOX_STOPPED;
	}

	if(m->msgNum >= MAX_MAILBOX_SIZE && block == false){
		spin_unlock(&m->lock);
		return MAILBOX_FULL;
	}

	// TODO: Add wait
	if(m->msgNum >= MAX_MAILBOX_SIZE && block == true){
		m->ref_counter++;
		wait_event(m->queue, m->msgNum < MAX_MAILBOX_SIZE);

		if (m == NULL){
			return MAILBOX_INVALID;
		}

		printk(KERN_INFO "insertMsg: Process woken up\n");
		m->ref_counter--;
	}

	// Check message length
	if(len > MAX_MSG_SIZE){
		spin_unlock(&m->lock);
		return MSG_LENGTH_ERROR;
	}

	// If it has room put in the list of messages
	//if(m->msgNum < MAX_MAILBOX_SIZE){
	//	m->messages[m->msgNum] = (message *)kmem_cache_alloc(message_cache, GFP_KERNEL);
	//}

	// Initialize the new message struct to insert
	newMsg = kmem_cache_alloc(message_cache, GFP_KERNEL);
	newMsg->len = len;
	newMsg->sender = current->pid;
	copy_from_user(newMsg->msg, msg, len);

	m->messages[m->msgNum] = newMsg;
	m->msgNum++;

	printk(KERN_INFO "insertMsg: %d messages in this mailbox\n", m->msgNum);
	printk(KERN_INFO "insertMsg: Mailbox PID = %d\n", m->key);
	printk(KERN_INFO "insertMsg: New message = %s\n", m->messages[m->msgNum-1]->msg);
	printk(KERN_INFO "insertMsg: Message length = %d", m->messages[m->msgNum-1]->len);
	printk(KERN_INFO "*******************************************************************\n");

	if(m->msgNum == 1 && m->ref_counter > 0){
		wake_up(&m->queue);
	}

	spin_unlock(&m->lock);
	return 0;
} // int insertMsg(int dest, char *msg, int len, bool block)

int removeMsg(pid_t *sender, void *msg, int *len, bool block){
	int i;
	struct mailbox *m = getBox(current->pid);
	struct message *newMsg = NULL;

	printk(KERN_INFO "*************************** removeMsg *****************************\n");

	if(m == NULL){
		printk(KERN_INFO "removeMsg: Mailbox doesnt exist, creating new\n");
		m = createMailbox(current->pid);
	}

	spin_lock(&m->lock);

	printk("removeMsg: Mailbox PID = %d\n", m->key);

	newMsg = m->messages[0];
	if(newMsg == NULL){
		printk(KERN_INFO "removeMsg: Message is NULL. Returning...\n");
		spin_unlock(&m->lock);
		return -1;
	}

	// TODO: Add wait
	if(m->msgNum == 0 && m->stopped == false && block){
		m->ref_counter++;
		wait_event(m->queue, m->msgNum > 0);

		if (m == NULL){
			return MAILBOX_INVALID;
		}

		printk(KERN_INFO "removeMsg: Process woken up\n");
		m->ref_counter--;
	}

	if(m->msgNum == 0 && m->stopped == false && !block){
		spin_unlock(&m->lock);
		return MAILBOX_EMPTY;
	}

	if(m->stopped){
		if(m->msgNum == 0){
			spin_unlock(&m->lock);
			return MAILBOX_STOPPED;
		}

		printk(KERN_INFO "removeMsg: Message = %s\n", newMsg->msg);
		printk(KERN_INFO "removeMsg: First character is %c\n", newMsg->msg[0]);
		printk(KERN_INFO "removeMsg: Message length = %d\n", newMsg->len);

		// Copy the string back to the receiving mailbox
		if(copy_to_user(msg, newMsg->msg, newMsg->len)){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		// Copy sender PID
		if(copy_to_user(sender, &newMsg->sender, sizeof(pid_t))){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		// Copy message length
		if(copy_to_user(len, &newMsg->len, sizeof(int))){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		kmem_cache_free(message_cache, &newMsg);
	}

	else {
		printk(KERN_INFO "removeMsg: Message = %s\n", newMsg->msg);
		printk(KERN_INFO "removeMsg: First character is %c\n", newMsg->msg[0]);
		printk(KERN_INFO "removeMsg: Message length = %d\n", newMsg->len);

		// Copy the string back to the receiving mailbox
		if(copy_to_user(msg, newMsg->msg, newMsg->len)){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		// Copy sender PID
		if(copy_to_user(sender, &newMsg->sender, sizeof(pid_t))){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		// Copy message length
		if(copy_to_user(len, &newMsg->len, sizeof(int))){
			spin_unlock(&m->lock);
			return EFAULT;
		}

		kmem_cache_free(message_cache, &newMsg);
	}

	if(m->msgNum >= MAX_MAILBOX_SIZE && m->ref_counter > 0){
		wake_up(&m->queue);
	}

	// Update the array of messages inside the mailbox
	for(i = 0; i < m->msgNum; i++){
		if(i == m->msgNum - 1){
			m->messages[i] = NULL;
		}

		else{
			m->messages[i] = m->messages[i + 1];
		}
	}

	m->msgNum--;
	printk(KERN_INFO "*******************************************************************\n");
	spin_unlock(&m->lock);
	return 0;
} // int removeMsg(int *sender, void *msg, int *len, bool block)

int insert(pid_t key){
	struct mailbox *next, *last;
	next = ht->mailboxes[0];
	last = NULL;

	spin_lock(&ht->main_lock);

	// Traverse linked list to end
	while(next != NULL){
		last = next;
		next = next->next;
	}

	// Create new mailbox. Have last point to the new box
	next = createMailbox(key);
	last->next = next;

	// At max size, make more space for pointers
	if(ht->boxNum == 64){
		spin_unlock(&ht->main_lock);
		return -1;
	}

	else{
		ht->mailboxes[ht->boxNum] = next;
		ht->boxNum++;
	}

	spin_unlock(&ht->main_lock);
	return 0;
} // int insert(hashtable *h, int key)

struct mailbox *getBox(pid_t key){
	//mailbox *next = h->mailboxes[0];

	// If found, return mailbox *. Else return NULL
	//while(next != NULL){
	//	if(next->key == key){
	//		return next;
	//	}

	//	next = next->next;
	//}

	int i;

	spin_lock(&ht->main_lock);
	for (i = 0; i < ht->boxNum; i++){
		if(ht->mailboxes[i]->key == key){
			spin_unlock(&ht->main_lock);
			return ht->mailboxes[i];
		}
	}

	printk("getBox: Returned null mailbox pointer\n");
	spin_unlock(&ht->main_lock);
	return NULL;
} // mailbox *getBox(hashtable *h, int key)

int remove(struct mailbox *m){
	int i;
	int *len = NULL;

	if(ht != NULL){
		spin_lock(&ht->main_lock);

		ManageMailbox(true, len);

		while(m->ref_counter > 0){
			wake_up_all(&m->queue);
			wait_event(m->queue, m->ref_counter == 0);
		}

		// Free up all messages
		for(i = 0; i < m->msgNum; i++){
			kmem_cache_free(message_cache, m->messages[i]);
		}

		for(i = 0; i < ht->size; i++){
			if(ht->mailboxes[i]->key == m->key){
				ht->mailboxes[i] = NULL;
			}
		}

		printk(KERN_INFO "remove: Messages freed from Mailbox PID %d\n", m->key);
		kmem_cache_free(mailbox_cache, &m); // Free mailbox in cache
		printk(KERN_INFO "remove: Mailbox successfully deleted\n");
		ht->boxNum--;

		// Reposition array of mailboxes
		//for(j = i; j < ht->boxNum; j++){
		//	ht->mailboxes[j] = ht->mailboxes[j + 1];
		//}

		spin_unlock(&ht->main_lock);
		return 0;
	}

	return 0;
} // int remove(struct mailbox *m)

static unsigned long **find_sys_call_table(void) {
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close) {
			printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX\n", (unsigned long) sct);
			return sct;
		}

	offset += sizeof(void *);
	}

	return NULL;
}	// static unsigned long **find_sys_call_table(void)

static void disable_page_protection(void) {
	/*
	Control Register 0 (cr0) governs how the CPU operates.

	Bit #16, if set, prevents the CPU from writing to memory marked as
	read only. Well, our system call table meets that description.
	But, we can simply turn off this bit in cr0 to allow us to make
	changes. We read in the current value of the register (32 or 64
	bits wide), and AND that with a value where all bits are 0 except
	the 16th bit (using a negation operation), causing the write_cr0
	value to have the 16th bit cleared (with all other bits staying
	the same. We will thus be able to write to the protected memory.

	It's good to be the kernel!
	*/

	write_cr0 (read_cr0 () & (~ 0x10000));
}	//static void disable_page_protection(void)

static void enable_page_protection(void) {
	/*
	See the above description for cr0. Here, we use an OR to set the
	16th bit to re-enable write protection on the CPU.
	*/

	write_cr0 (read_cr0 () | 0x10000);
}	// static void enable_page_protection(void)

static int __init interceptor_start(void) {
	/* Find the system call table */
	if(!(sys_call_table = find_sys_call_table())) {
		/* Well, that didn't work.
		Cancel the module loading step. */
		return -1;
	}

	/* Store a copy of all the existing functions */
	ref_cs3013_syscall1 = (void *)sys_call_table[__NR_cs3013_syscall1];
	ref_cs3013_syscall2 = (void *)sys_call_table[__NR_cs3013_syscall2];
	ref_cs3013_syscall3 = (void *)sys_call_table[__NR_cs3013_syscall3];
	ref_sys_exit = (void *)sys_call_table[__NR_exit];
	ref_sys_exit_group = (void *)sys_call_table[__NR_exit_group];

	/* Intercept call */
	disable_page_protection();
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)SendMsg;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)RcvMsg;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ManageMailbox;
	sys_call_table[__NR_exit] = (unsigned long *)MailboxExit;
	sys_call_table[__NR_exit_group] = (unsigned long *)MailboxExitGroup;
	enable_page_protection();
	return 0;
}	// static int __init interceptor_start(void)

static void __exit interceptor_end(void) {
	/* If we don't know what the syscall table is, don't bother. */
	// int i;

	if(!sys_call_table)
		return;

	// for(i = 0; i < ht->size; i++){
	// 	if(ht->mailboxes[i] != NULL){
	// 		printk(KERN_INFO "interceptor_end: Removing mailbox PID %d\n", ht->mailboxes[i]->key);
	// 		remove(ht->mailboxes[i]);
	// 		kfree(ht->mailboxes[i]);
	// 	}
	// }

	// kmem_cache_destroy(message_cache);
	// printk(KERN_INFO "interceptor_end: Destroyed message cache\n");
	// kmem_cache_destroy(mailbox_cache);
	// printk(KERN_INFO "interceptor_end: Destroyed mailbox cache\n");
	// kfree(ht);

	/* Revert all system calls to what they were before we began. */
	disable_page_protection();
	printk(KERN_INFO "interceptor_end: Page protection disabled\n");
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)ref_cs3013_syscall1;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_cs3013_syscall2;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ref_cs3013_syscall3;
	sys_call_table[__NR_exit] = (unsigned long *)ref_sys_exit;
	sys_call_table[__NR_exit_group] = (unsigned long *)ref_sys_exit_group;
	printk(KERN_INFO "interceptor_end: Re-enabling page protection\n");
	enable_page_protection();

	printk(KERN_INFO "interceptor_end: Mailbox_LKM successfully unloaded\n");
	
}	// static void __exit interceptor_end(void)

module_init(interceptor_start);
module_exit(interceptor_end);
