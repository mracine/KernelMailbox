// Michael Racine (mrracine)
// Project2

/*
 * Sample_LKM.c
 *
 *  Undated on: Jan 26, 2014
 *      Author: Craig Shue
 *      Updated: Hugh C. Lauer
 */

// We need to define __KERNEL__ and MODULE to be in Kernel space
// If they are defined, undefined them and define them again:
#undef __KERNEL__
#undef MODULE

#define __KERNEL__ 
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/cred.h>

// Pointer to pointer to system call table
// Interception will replace one or more lines in this table
unsigned long **sys_call_table;

// Pointer to a function
// Interceptor start will store pointer to
// open and/or close
// At interceptor end, pointer needs to be
// restored to original value so state of
// system is safely restored
asmlinkage long (*ref_sys_open)(const char __user *filename, int flags, int mode);
asmlinkage long (*ref_sys_close)(unsigned int fd);

// New system call replacing the original
// open call
asmlinkage long new_sys_open(const char __user *filename, int flags, int mode) {
	// Request made by user
	if(current_uid() >= 1000){
		printk(KERN_INFO "User %d is opening file %s\n", current_uid(), filename);
	}

	return ref_sys_open(filename, flags, mode);
}	// asmlinkage long new_sys_open(void)

asmlinkage long new_sys_close(unsigned int fd){
	// Request made by user
	if(current_uid() >= 1000){
		printk(KERN_INFO "User %d is closing file descriptor %d\n", current_uid(), fd);
	}

	return ref_sys_close(fd);
}	// asmlinkage long new_sys_close(void)

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
	ref_sys_open = (void *)sys_call_table[__NR_open];
	ref_sys_close = (void *)sys_call_table[__NR_close];

	/* Intercept call */
	disable_page_protection();
	sys_call_table[__NR_open] = (unsigned long *)new_sys_open;
	sys_call_table[__NR_close] = (unsigned long *)new_sys_close;
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
	sys_call_table[__NR_open] = (unsigned long *)ref_sys_open;
	sys_call_table[__NR_close] = (unsigned long *)ref_sys_close;
	enable_page_protection();
}	// static void __exit interceptor_end(void)

module_init(interceptor_start);
module_exit(interceptor_end);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("mrracine");


