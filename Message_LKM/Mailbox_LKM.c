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

unsigned long **sys_call_table;
struct kmem_cache *mailbox_cache = NULL;
struct kmem_cache *message_cache = NULL;
hashtable *ht = NULL;

asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	int err;
	int destination = (int) dest;

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox)+sizeof(message *)*64, 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(message), 0, 0, NULL);
	}

	err = insertMsg(destination, (char *)msg, len, block);

	if(err != 0){
		printk(KERN_INFO "%d", err);
		return err;
	}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	int err;
	printk(KERN_INFO "RcvMsg: Pid to receive from = %d", current->pid);

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(message)*64, 0, 0, NULL);
	}

	err = removeMsg(sender, msg, len, block);

	if(err != 0){
		printk(KERN_INFO "%d", err);
		return err;
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

asmlinkage long ManageMailbox(bool stop, int *count){
	// mailbox *m = getBox(ht, dest);

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", sizeof(message)*64, 0, 0, NULL);
	}

	/*
	if(m == NULL){
		return MAILBOX_INVALID;
	}
	*/

	// &count = m->msgNum;
	return 0;
}	// asmlinkage long ManageMailbox(bool stop, int *count)

hashtable *create(void){
	hashtable *newHash;
	int i;

	// Allocate space for hashtable
	if((newHash = (hashtable *)kmalloc(sizeof(hashtable), GFP_KERNEL)) == NULL)
		return NULL;

	// Allocate space for pointer of mailboxes
	if((newHash->mailboxes = (mailbox **)kmalloc(sizeof(mailbox *)*32, GFP_KERNEL)) == NULL)
		return NULL;

	// Initialize hashtable
	for (i=0; i<32; i++){
		newHash->mailboxes[i] = NULL;
	}

	newHash->size = 32;
	newHash->boxNum = 0;
	return newHash;
} // hashtable *create(void)

mailbox *createMailbox(int key){
	int i;
	mailbox *newBox = (mailbox *)kmem_cache_alloc(mailbox_cache, GFP_KERNEL); // Allocate mailbox from cache
	
	// Init values
	newBox->key = key;
	newBox->ref_counter = 0;
	newBox->msgNum = 0;
	newBox->stopped = false;
	newBox->next = NULL;
	
	for (i=0; i< 64; i++){
		newBox->messages[i] = NULL;
	}

	// we need to check the size of the hashtable to see if there is room here for this new mailbox pointer

	// Set the last entry in the mailbox pointer list in the hash to the new box
	ht->mailboxes[ht->boxNum] = newBox;

	// Increment hashtable allocated size
	//race condition here
	ht->boxNum++;
	return newBox;
} // mailbox *createMailbox(int key)

int insertMsg(int dest, char *msg, int len, bool block){
	mailbox *m = getBox(ht, dest);
	message *newMsg = NULL;

	// Mailbox does not exist in hashtable
	if(m == NULL){
		printk(KERN_INFO "insertMsg: Mailbox doesnt exist, creating new\n");
		m = createMailbox(dest);
	}


	if(m->msgNum >= MAX_MAILBOX_SIZE && block == false){
		return MAILBOX_FULL;
	}

	// TODO: Add wait
	if(m->msgNum >= MAX_MAILBOX_SIZE && block == true){
		// wait until the mailbox has room
	}

	// Check message length
	if(len > MAX_MSG_SIZE){
		return MSG_LENGTH_ERROR;
	}

	// if it has room put in the list of messages
	//if(m->msgNum < MAX_MAILBOX_SIZE){
	//	m->messages[m->msgNum] = (message *)kmem_cache_alloc(message_cache, GFP_KERNEL);
	//}

	// Initialize the new message struct to insert
	newMsg = kmem_cache_alloc(message_cache, GFP_KERNEL);
	newMsg->msg = msg;
	newMsg->len = len;

	m->messages[m->msgNum] = newMsg;
	m->msgNum++;

	printk(KERN_INFO "********************************************************\n");
	printk(KERN_INFO "insertMsg: %d messages in this mailbox\n", m->msgNum);
	printk(KERN_INFO "insertMsg: Mailbox PID = %d\n", m->key);
	printk(KERN_INFO "insertMsg: New message = %s\n", m->messages[m->msgNum-1]->msg);
	printk(KERN_INFO "insertMsg: Message length %d", m->messages[m->msgNum-1]->len);
	printk(KERN_INFO "********************************************************\n");

	return 0;
} // int insertMsg(int dest, char *msg, int len, bool block)

int removeMsg(int *sender, void *msg, int *len, bool block){
	int i;
	mailbox *m = getBox(ht, current->pid);
	message *newMsg = NULL;

	printk(KERN_INFO "********************************************************\n");

	if(m == NULL){
		printk(KERN_INFO "removeMsg: Mailbox doesnt exist, creating new\n");
		m = createMailbox(current->pid);
	}

	printk("removeMsg: Mailbox PID = %d\n", m->key);

	newMsg = m->messages[0];

	if (newMsg == NULL){
		printk(KERN_INFO "removeMsg: Message is NULL. Returning...\n");
		return -1;
	}

	else {
		printk(KERN_INFO "removeMsg: Message = %s\n", newMsg->msg);
		printk(KERN_INFO "removeMsg: First character is %c\n", newMsg->msg[0]);
		printk(KERN_INFO "removeMsg: Message length = %d\n", newMsg->len);

		if(copy_to_user(msg, newMsg, sizeof(newMsg))){
			return EFAULT;
		}
	}

	// If the mailbox is not empty then get the first one. 
	if(m->msgNum != 0){
		msg = newMsg->msg;
		len = (int *)newMsg->len;
	}

	// TODO: Add wait
	if(m->msgNum == 0 && m->stopped == false && block){
		//
	}

	if(m->msgNum == 0 && m->stopped == false && !block){
		return MAILBOX_EMPTY;
	}

	if(m->stopped){
		if(m->msgNum == 0){
			return MAILBOX_STOPPED;
		}

		printk(KERN_INFO "%s\n", newMsg->msg);
		msg = newMsg->msg;
		len = (int *)newMsg->len;
	}

	// Update the array of messages inside the mailbox 
	// Would a linked list of messages be easier here? instead of 
	for(i = 0; i < m->msgNum; i++){
		m->messages[i] = m->messages[i+1];
	}

	m->msgNum--;
	return 0;
} // int removeMsg(int *sender, void *msg, int *len, bool block)

int insert(hashtable *h, int key){
	mailbox *next, *last;
	next = h->mailboxes[0];
	last = NULL;

	// Traverse linked list to end
	while(next != NULL){
		last = next;
		next = next->next;
	}

	// Create new mailbox. Have last point to the new box
	next = createMailbox(key);
	last->next = next;

	// At max size, make more space for pointers
	if(h->boxNum ==  32){
		return -1;
	}
	else{
		h->mailboxes[h->boxNum] = next;
		h->boxNum++;
	}

	return 0;
} // int insert(hashtable *h, int key)

mailbox *getBox(hashtable *h, int key){
	//mailbox *next = h->mailboxes[0];

	// If found, return mailbox *. Else return NULL
	//while(next != NULL){
	//	if(next->key == key){
	//		return next;
	//	}

	//	next = next->next;
	//}

	int i;

	for (i = 0; i < h->boxNum; i++){
		if(h->mailboxes[i]->key == key){
			return h->mailboxes[i];
		}
	}

	printk("getBox: Returned null mailbox pointer\n");
	return NULL;
} // mailbox *getBox(hashtable *h, int key)

int remove(hashtable *h, int key){
	int i, j;
	mailbox *temp = NULL;
	mailbox *prev = h->mailboxes[0];
	mailbox *next = h->mailboxes[0];
	i = 0;

	// Search for mailbox
	while(next != NULL){
		if(next->key == key){
			temp = next->next;
			prev->next = temp;

			// Reposition array of mailboxes
			for(j = i; j < h->size; j++){
				h->mailboxes[j] = h->mailboxes[j+1];
			}

			// Free up all messages
			for(j = 0; j < next->msgNum; j++){
				kmem_cache_free(message_cache, &next->messages[j]);
			}

			kmem_cache_free(mailbox_cache, &next); // Free mailbox in cache
			h->boxNum--;
			return 0;
		}

		prev = next;
		next = next->next;
		i++;
	}

	return MAILBOX_INVALID; // Mailbox not found in hashtable
} // int remove(hashtable *h, int key)

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

	/* Intercept call */
	disable_page_protection();
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)SendMsg;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)RcvMsg;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ManageMailbox;
	enable_page_protection();
	return 0;
}	// static int __init interceptor_start(void)

static void __exit interceptor_end(void) {
	/* If we don't know what the syscall table is, don't bother. */
	if(!sys_call_table)
		return;

	/* Revert all system calls to what they were before we began. */
	disable_page_protection();
	sys_call_table[__NR_cs3013_syscall1] = (unsigned long *)ref_cs3013_syscall1;
	sys_call_table[__NR_cs3013_syscall2] = (unsigned long *)ref_cs3013_syscall2;
	sys_call_table[__NR_cs3013_syscall3] = (unsigned long *)ref_cs3013_syscall3;
	enable_page_protection();
}	// static void __exit interceptor_end(void)

module_init(interceptor_start);
module_exit(interceptor_end);
