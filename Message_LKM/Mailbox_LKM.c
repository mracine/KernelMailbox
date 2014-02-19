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

unsigned long **sys_call_table;
struct kmem_cache *cache = NULL;
hashtable *ht = NULL;

asmlinkage long (*ref_cs3013_syscall1)(void);
asmlinkage long (*ref_cs3013_syscall2)(void);
asmlinkage long (*ref_cs3013_syscall3)(void);

asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block) {
	if(cache == NULL){
		cache = kmem_cache_create("Mailbox", sizeof(mailbox) + (MAX_MSG_SIZE*32), 0, 0, NULL);
	}

	return 0;
}	// asmlinkage long SendMsg(pid_t dest, void *msg, int len, bool block)

asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block) {
	if(cache == NULL){
		cache = kmem_cache_create("Mailbox", sizeof(mailbox) + (MAX_MSG_SIZE*32), 0, 0, NULL);
	}

	return 0;
}	// asmlinkage long RcvMsg(pid_t *sender, void *msg, int *len, bool block)

asmlinkage long ManageMailbox(bool stop, int *count){
	if(cache == NULL){
		cache = kmem_cache_create("Mailbox", sizeof(mailbox) + (MAX_MSG_SIZE*32), 0, 0, NULL);
	}

	return 0;
}	// asmlinkage long ManageMailbox(bool stop, int *count)

mailbox *createMailbox(int key){
	mailbox *newBox = (mailbox *)kmem_cache_alloc(cache, GFP_KERNEL);
	newBox->key = key;
	newBox->next = NULL;
	return newBox;
}

hashtable *create(void){
	int i;
	hashtable *newHash;

	if((newHash = (hashtable *)kmalloc(sizeof(hashtable), GFP_KERNEL)) == NULL)
		return NULL;

	if((newHash->mailboxes = (mailbox **)kmalloc(16*sizeof(mailbox *), GFP_KERNEL)) == NULL)
		return NULL;

	for(i = 0; i < 16; i++){
		newHash->mailboxes[i] = NULL;
	}

	newHash->size = 16;
	return newHash;
}

int insert(hashtable *h, int key){
	int i;
	mailbox *next, *last;
	next = h->mailboxes[0];

	while(next != NULL){
		last = next;
		next = next->next;
	}

	next = createMailbox(key);
	last->next = next;

	if(h->size <=  h->boxNum){
		for(i = h->boxNum; i < h->size + 16; i++){
			h->mailboxes[i] = (mailbox *)kmalloc(sizeof(mailbox *), GFP_KERNEL);
			h->mailboxes[i] = NULL;
		}

		h->size += 16;
	}

	h->mailboxes[h->boxNum] = next;
	h->boxNum++;
	return 0;
}

mailbox *getBox(hashtable *h, int key){
	mailbox *next = h->mailboxes[0];

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

			kmem_cache_free(cache, &next);
			h->boxNum--;
			return 0;
		}

		i++;
		prev = next;
		next = next->next;
	}

	return -1;
}

// Used to find the system call table
// #DO NOT MODIFY
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

// Disables the protection for read-only memory
// # DO NOT MODIFY
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

// Enables the protection for read-only memory
// # DO NOT MODIFY
static void enable_page_protection(void) {
	/*
	See the above description for cr0. Here, we use an OR to set the
	16th bit to re-enable write protection on the CPU.
	*/

	write_cr0 (read_cr0 () | 0x10000);
}	// static void enable_page_protection(void)


// Interceptor Start
//
// Finds the system call table
//
// Saves the address of the existing open and/or close in a pointer
//
// Disables the paging protections
//
// Replaces the call's entry in the page table
// with a pointer to the new function
//
// Re-enables the page protections
//
// Writes a note to the system log
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

// Interceptor End
//
// Reverses changes of interceptor start function
// 
// Uses saved pointer value for original system call
// and puts it back in the syscall table in the right array location
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