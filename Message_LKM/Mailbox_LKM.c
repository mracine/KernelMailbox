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
struct kmem_cache *ht_cache = NULL;
struct kmem_cache *mailbox_cache = NULL;
struct kmem_cache *message_cache = NULL;
hashtable *ht = NULL;

asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	int err;

	// Hashtable cache
	if(ht_cache == NULL){
		ht_cache = kmem_cache_create("ht_cache", sizeof(mailbox *), 0, 0, NULL);
	}

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", MAX_MSG_SIZE, 0, 0, NULL);
	}

	err = insertMsg(dest, (char *)msg, len, block);

	if(err != 0){
		printf(KERN_INFO "%d", err);
		return err;
	}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	int err;
	void *newMsg;

	// Hashtable cache
	if(ht_cache == NULL){
		ht_cache = kmem_cache_create("ht_cache", sizeof(mailbox *), 0, 0, NULL);
	}

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	// Message cache
	if(message_cache == NULL){
		message_cache = kmem_cache_create("message_cache", MAX_MSG_SIZE, 0, 0, NULL);
	}

	err = removeMsg(sender, newMsg, len, block);

	if(err != 0){
		printf(KERN_INFO "%d", err);
		return err;
	}

	if(copy_to_user(msg, &newMsg, (unsigned long)*len)){
		return EFAULT;
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

asmlinkage long ManageMailbox(bool stop, int *count){
	// mailbox *m = getBox(ht, dest);

	// Hashtable cache
	if(ht_cache == NULL){
		ht_cache = kmem_cache_create("ht_cache", sizeof(mailbox *), 0, 0, NULL);
	}

	// Mailbox cache
	if(mailbox_cache == NULL){
		mailbox_cache = kmem_cache_create("mailbox_cache", sizeof(mailbox), 0, 0, NULL);
		ht = create();
	}

	// Message cache
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

hashtable *create(void){
	hashtable *newHash;

	// Allocate space for hashtable
	if((newHash = (hashtable *)kmalloc(sizeof(hashtable), GFP_KERNEL)) == NULL)
		return NULL;

	// Allocate space for pointer of mailboxes
	if((newHash->mailboxes[0] = (mailbox *)kmem_cache_alloc(ht_cache, GFP_KERNEL)) == NULL)
		return NULL;

	// Initialize hashtable
	newHash->mailboxes[0] = NULL;
	newHash->size = 1;
	newHash->boxNum = 0;
	return newHash;
} // hashtable *create(void)

mailbox *createMailbox(int key){
	mailbox *newBox = (mailbox *)kmem_cache_alloc(mailbox_cache, GFP_KERNEL); // Allocate mailbox from cache
	
	// Init values
	newBox->key = key;
	newBox->msgNum = 0;
	newBox->stopped = false;
	newBox->next = NULL;

	// Increment hashtable allocated size
	ht->size++;
	return newBox;
} // mailbox *createMailbox(int key)

int insertMsg(int dest, char *msg, int len, bool block){
	mailbox *m = getBox(ht, dest);

	// Mailbox does not exist in hashtable
	if(m == NULL){
		m = createMailbox(dest);
	}

	// 
	if(m->msgNum < MAX_MAILBOX_SIZE){
		m->messages[m->msgNum] = (char *)kmem_cache_alloc(message_cache, GFP_KERNEL);
	}

	if(m->msgNum >= MAX_MAILBOX_SIZE && block == false){
		return MAILBOX_FULL;
	}

	// TODO: Add wait
	if(m->msgNum >= MAX_MAILBOX_SIZE && block == true){
		//
	}

	if(len > MAX_MSG_SIZE){
		return MSG_LENGTH_ERROR;
	}

	m->messages[m->msgNum] = msg;
	m->msgNum++;
	return 0;
} // int insertMsg(int dest, char *msg, int len, bool block)

int removeMsg(int *sender, void *msg, int *len, bool block){
	int i;
	mailbox *m = getBox(ht, *sender);

	if(m == NULL){
		m = createMailbox(*sender);
	}

	if(m->msgNum != 0){
		printk(KERN_INFO "%s", m->messages[0]);
		msg = (void *)m->messages[0];
		len = (int *)sizeof(m->messages[0]);
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

		printk(KERN_INFO "%s", m->messages[0]);
		msg = (void *)m->messages[0];
		len = (int *)sizeof(m->messages[0]);
	}

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
	if(h->size <=  h->boxNum){
		h->mailboxes[h->boxNum] = (mailbox *)kmem_cache_alloc(ht_cache, GFP_KERNEL);
		h->size++;
	}

	h->mailboxes[h->boxNum] = next;
	h->boxNum++;
	return 0;
} // int insert(hashtable *h, int key)

mailbox *getBox(hashtable *h, int key){
	mailbox *next = h->mailboxes[0];

	// If found, return mailbox *. Else return NULL
	while(next != NULL){
		if(next->key == key){
			return next;
		}

		next = next->next;
	}

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

			kmem_cache_free(ht_cache, next); // Free pointer in hashtable
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
