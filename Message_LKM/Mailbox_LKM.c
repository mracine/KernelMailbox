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

unsigned long **sys_call_table;
struct kmem_cache *mailbox_cache = NULL;
struct kmem_cache *message_cache = NULL;
hashtable *ht = NULL;

asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	int err;

	// Init hashtable
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", MAX_MSG_SIZE, 0, 0, NULL);
	}

	if((getBox(ht, dest)) == NULL){
		createMailbox(dest);
	}

	err = insertMsg(dest, (char *)msg, len, block);

	if(err != 0){
		return err;
	}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	int err;
	void *newMsg;

	// Init hashtable
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", MAX_MSG_SIZE, 0, 0, NULL);
	}

	err = removeMsg(sender, newMsg, len, block);

	if(err != 0){
		return err;
	}

	if(copy_to_user(msg, &newMsg, (unsigned long)*len)){
		return EFAULT;
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

asmlinkage long ManageMailbox(bool stop, int *count){
	// mailbox *m = getBox(ht, dest);

	// Init hashtable
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", MAX_MSG_SIZE, 0, 0, NULL);
	}

	/*
	if(m == NULL){
		return MAILBOX_INVALID;
	}
	*/

	// &count = m->msgNum;
	return 0;
}	// asmlinkage long ManageMailbox(bool stop, int *count)

mailbox *createMailbox(int key){
	mailbox *newBox = (mailbox *)kmem_cache_alloc(mailbox_cache, GFP_KERNEL);
	ht->mailboxes[ht->boxNum] = (mailbox *)kmalloc(sizeof(mailbox *),GFP_KERNEL);
	ht->size++;
	newBox->key = key;
	newBox->msgNum = 0;
	newBox->stopped = FALSE;
	newBox->next = NULL;
	return newBox;
}

int insertMsg(int dest, char *msg, int len, bool block){
	mailbox *m = getBox(ht, dest);

	if(m == NULL){
		return MAILBOX_INVALID;
	}

	if(m->msgNum < MAX_MAILBOX_SIZE){
		m->messages[m->msgNum] = (char *)kmem_cache_alloc(message_cache, GFP_KERNEL);
	}

	if(m->msgNum >= MAX_MAILBOX_SIZE && block == FALSE){
		return MAILBOX_FULL;
	}

	// TODO: Add wait
	if(m->msgNum >= MAX_MAILBOX_SIZE && block == TRUE){
		//
	}

	if(len > MAX_MSG_SIZE){
		return MSG_LENGTH_ERROR;
	}

	m->messages[m->msgNum] = msg;
	m->msgNum++;
	return 0;
}

int removeMsg(int *sender, void *msg, int *len, bool block){
	int i;
	mailbox *m = getBox(ht, *sender);

	if(m == NULL){
		return MAILBOX_INVALID;
	}

	if(m->msgNum != 0){
		printk(KERN_INFO "%s", m->messages[0]);
		msg = (void *)m->messages[0];
		len = (int *)sizeof(m->messages[0]);
	}

	// TODO: Add wait
	if(m->msgNum == 0 && m->stopped == FALSE && block){
		//
	}

	if(m->msgNum == 0 && m->stopped == FALSE && !block){
		return MAILBOX_EMPTY;
	}

	if(m->stopped){
		if(m->msgNum == 0){
			return MAILBOX_STOPPED;
		}

		printk(KERN_INFO "%s", m->messages[0]);
		msg = (void *)m->messages[0];
		len = (int *)sizeof(m->messages[0]);
	}

	for(i = 0; i < m->msgNum; i++){
		m->messages[i] = m->messages[i+1];
	}

	m->msgNum--;
	return 0;
}

hashtable *create(void){
	hashtable *newHash;

	// Allocate space for hashtable
	if((newHash = (hashtable *)kmalloc(sizeof(hashtable), GFP_KERNEL)) == NULL)
		return NULL;

	// Allocate space for pointer of mailboxes
	if((newHash->mailboxes = (mailbox **)kmalloc(sizeof(mailbox *), GFP_KERNEL)) == NULL)
		return NULL;

	newHash->mailboxes[0] = NULL;
	newHash->size = 1; // Initial is 16 mailboxes
	newHash->boxNum = 0;
	return newHash;
}

int insert(hashtable *h, int key){
	mailbox *next, *last;
	next = h->mailboxes[0];
	last = NULL;

	// Traverse linked list to end
	while(next != NULL){
		last = next;
		next = next->next;
	}

	next = createMailbox(key);
	last->next = next;

	// At max size, make more space for pointers
	if(h->size <=  h->boxNum){
		h->mailboxes[h->boxNum] = (mailbox *)kmalloc(sizeof(mailbox *), GFP_KERNEL);
		h->mailboxes[h->boxNum] = NULL;
		h->size++;
	}

	h->mailboxes[h->boxNum] = next;
	h->boxNum++;
	return 0;
}

mailbox *getBox(hashtable *h, int key){
	mailbox *next = h->mailboxes[0];

	// Return mailbox * if found. Else return NULL
	while(next != NULL){
		if(next->key == key){
			return next;
		}

		next = next->next;
	}

	return NULL;
}

int remove(hashtable *h, int key){
	int i = 0;
	int j;
	mailbox *temp = NULL;
	mailbox *prev = h->mailboxes[0];
	mailbox *next = h->mailboxes[0];

	while(next != NULL){
		if(next->key == key){
			temp = next->next;
			prev->next = temp;

			for(j = i; j < h->size; j++){
				h->mailboxes[j] = h->mailboxes[j+1];
			}

			kmem_cache_free(mailbox_cache, &next);
			h->boxNum--;
			return 0;
		}

		i++;
		prev = next;
		next = next->next;
	}

	return -1;
}

static unsigned long **find_sys_call_table(void) {
	unsigned long int offset = PAGE_OFFSET;
	unsigned long **sct;

	while (offset < ULLONG_MAX) {
		sct = (unsigned long **)offset;

		if (sct[__NR_close] == (unsigned long *) sys_close) {
			printk(KERN_INFO "Interceptor: Found syscall table at address: 0x%02lX", (unsigned long) sct);
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
