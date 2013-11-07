/*
** frandom.c
**      Fast pseudo-random generator 
**
**      (c) Copyright 2003-2011 Eli Billauer
**      http://www.billauer.co.il
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
**
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h> 
#include <linux/fs.h> 
#include <linux/errno.h>
#include <linux/types.h> 
#include <linux/random.h>

#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/device.h>

#define INTERNAL_SEED 0
#define EXTERNAL_SEED 1

#define FRANDOM_MAJOR 235
#define FRANDOM_MINOR 11 
#define ERANDOM_MINOR 12 

static struct file_operations frandom_fops; /* Values assigned below */

static int erandom_seeded = 0; /* Internal flag */

static int frandom_major = FRANDOM_MAJOR;
static int frandom_minor = FRANDOM_MINOR;
static int erandom_minor = ERANDOM_MINOR;
static int frandom_bufsize = 256;
static int frandom_chunklimit = 0; /* =0 means unlimited */

static struct cdev frandom_cdev;
static struct cdev erandom_cdev;
static struct class *frandom_class;
struct device *frandom_device;
struct device *erandom_device;

MODULE_DESCRIPTION("Fast pseudo-random number generator");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eli Billauer");
module_param(frandom_major, int, 0);
module_param(frandom_minor, int, 0);
module_param(erandom_minor, int, 0);
module_param(frandom_bufsize, int, 0);
module_param(frandom_chunklimit, int, 0);

MODULE_PARM_DESC(frandom_major,"Major number of /dev/frandom and /dev/erandom");
MODULE_PARM_DESC(frandom_minor,"Minor number of /dev/frandom");
MODULE_PARM_DESC(erandom_minor,"Minor number of /dev/erandom");
MODULE_PARM_DESC(frandom_bufsize,"Internal buffer size in bytes. Default is 256. Must be >= 256");
MODULE_PARM_DESC(frandom_chunklimit,"Limit for read() blocks size. 0 (default) is unlimited, otherwise must be >= 256");

struct frandom_state
{
	struct semaphore sem; /* Semaphore on the state structure */

	u8 S[256]; /* The state array */
	u8 i;        
	u8 j;

	char *buf;
};

static struct frandom_state *erandom_state;

static inline void swap_byte(u8 *a, u8 *b)
{
	u8 swapByte; 
  
	swapByte = *a; 
	*a = *b;      
	*b = swapByte;
}

static void init_rand_state(struct frandom_state *state, int seedflag);

void erandom_get_random_bytes(char *buf, size_t count)
{
	struct frandom_state *state = erandom_state;
	int k;

	unsigned int i;
	unsigned int j;
	u8 *S;
  
	/* If we fail to get the semaphore, we revert to external random data.
	   Since semaphore blocking is expected to be very rare, and interrupts
	   during these rare and very short periods of time even less frequent,
	   we take the better-safe-than-sorry approach, and fill the buffer
	   some expensive random data, in case the caller wasn't aware of this
	   possibility, and expects random data anyhow.
	*/

	if (down_interruptible(&state->sem)) {
		get_random_bytes(buf, count);
		return;
	}

	/* We seed erandom as late as possible, hoping that the kernel's main
	   RNG is already restored in the boot sequence (not critical, but
	   better.
	*/
	
	if (!erandom_seeded) {
		erandom_seeded = 1;
		init_rand_state(state, EXTERNAL_SEED);
		printk(KERN_INFO "frandom: Seeded global generator now (used by erandom)\n");
	}

	i = state->i;     
	j = state->j;
	S = state->S;  

	for (k=0; k<count; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		swap_byte(&S[i], &S[j]);
		*buf++ = S[(S[i] + S[j]) & 0xff];
	}
 
	state->i = i;     
	state->j = j;

	up(&state->sem);
}

static void init_rand_state(struct frandom_state *state, int seedflag)
{
	unsigned int i, j, k;
	u8 *S;
	u8 *seed = state->buf;

	if (seedflag == INTERNAL_SEED)
		erandom_get_random_bytes(seed, 256);
	else
		get_random_bytes(seed, 256);

	S = state->S;
	for (i=0; i<256; i++)
		*S++=i;

	j=0;
	S = state->S;

	for (i=0; i<256; i++) {
		j = (j + S[i] + *seed++) & 0xff;
		swap_byte(&S[i], &S[j]);
	}

	/* It's considered good practice to discard the first 256 bytes
	   generated. So we do it:
	*/

	i=0; j=0;
	for (k=0; k<256; k++) {
		i = (i + 1) & 0xff;
		j = (j + S[i]) & 0xff;
		swap_byte(&S[i], &S[j]);
	}

	state->i = i; /* Save state */
	state->j = j;
}

static int frandom_open(struct inode *inode, struct file *filp)
{
  
	struct frandom_state *state;

	int num = iminor(inode);

	/* This should never happen, now when the minors are regsitered
	 * explicitly
	 */
	if ((num != frandom_minor) && (num != erandom_minor)) return -ENODEV;
  
	state = kmalloc(sizeof(struct frandom_state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->buf = kmalloc(frandom_bufsize, GFP_KERNEL);
	if (!state->buf) {
		kfree(state);
		return -ENOMEM;
	}

	sema_init(&state->sem, 1); /* Init semaphore as a mutex */

	if (num == frandom_minor)
		init_rand_state(state, EXTERNAL_SEED);
	else
		init_rand_state(state, INTERNAL_SEED);

	filp->private_data = state;

	return 0; /* Success */
}

static int frandom_release(struct inode *inode, struct file *filp)
{

	struct frandom_state *state = filp->private_data;

	kfree(state->buf);
	kfree(state);
  
	return 0;
}

static ssize_t frandom_read(struct file *filp, char *buf, size_t count,
			    loff_t *f_pos)
{
	struct frandom_state *state = filp->private_data;
	ssize_t ret;
	int dobytes, k;
	char *localbuf;

	unsigned int i;
	unsigned int j;
	u8 *S;
  
	if (down_interruptible(&state->sem))
		return -ERESTARTSYS;
  
	if ((frandom_chunklimit > 0) && (count > frandom_chunklimit))
		count = frandom_chunklimit;

	ret = count; /* It's either everything or an error... */
  
	i = state->i;     
	j = state->j;
	S = state->S;  

	while (count) {
		if (count > frandom_bufsize)
			dobytes = frandom_bufsize;
		else
			dobytes = count;

		localbuf = state->buf;

		for (k=0; k<dobytes; k++) {
			i = (i + 1) & 0xff;
			j = (j + S[i]) & 0xff;
			swap_byte(&S[i], &S[j]);
			*localbuf++ = S[(S[i] + S[j]) & 0xff];
		}
 
		if (copy_to_user(buf, state->buf, dobytes)) {
			ret = -EFAULT;
			goto out;
		}

		buf += dobytes;
		count -= dobytes;
	}

 out:
	state->i = i;     
	state->j = j;

	up(&state->sem);
	return ret;
}

static struct file_operations frandom_fops = {
	read:       frandom_read,
	open:       frandom_open,
	release:    frandom_release,
};

static void frandom_cleanup_module(void) {
	unregister_chrdev_region(MKDEV(frandom_major, erandom_minor), 1);
	cdev_del(&erandom_cdev);
	device_destroy(frandom_class, MKDEV(frandom_major, erandom_minor));

	unregister_chrdev_region(MKDEV(frandom_major, frandom_minor), 1);
	cdev_del(&frandom_cdev);
	device_destroy(frandom_class, MKDEV(frandom_major, frandom_minor));
	class_destroy(frandom_class);

	kfree(erandom_state->buf);
	kfree(erandom_state);
}


static int frandom_init_module(void)
{
	int result;

	/* The buffer size MUST be at least 256 bytes, because we assume that
	   minimal length in init_rand_state().
	*/       
	if (frandom_bufsize < 256) {
		printk(KERN_ERR "frandom: Refused to load because frandom_bufsize=%d < 256\n",frandom_bufsize);
		return -EINVAL;
	}
	if ((frandom_chunklimit != 0) && (frandom_chunklimit < 256)) {
		printk(KERN_ERR "frandom: Refused to load because frandom_chunklimit=%d < 256 and != 0\n",frandom_chunklimit);
		return -EINVAL;
	}

	erandom_state = kmalloc(sizeof(struct frandom_state), GFP_KERNEL);
	if (!erandom_state)
		return -ENOMEM;

	/* This specific buffer is only used for seeding, so we need
	   256 bytes exactly */
	erandom_state->buf = kmalloc(256, GFP_KERNEL);
	if (!erandom_state->buf) {
		kfree(erandom_state);
		return -ENOMEM;
	}

	sema_init(&erandom_state->sem, 1); /* Init semaphore as a mutex */

	erandom_seeded = 0;

	frandom_class = class_create(THIS_MODULE, "fastrng");
	if (IS_ERR(frandom_class)) {
		result = PTR_ERR(frandom_class);
		printk(KERN_WARNING "frandom: Failed to register class fastrng\n");
		goto error0;
	}
	
	/*
	 * Register your major, and accept a dynamic number. This is the
	 * first thing to do, in order to avoid releasing other module's
	 * fops in frandom_cleanup_module()
	 */

	cdev_init(&frandom_cdev, &frandom_fops);
	frandom_cdev.owner = THIS_MODULE;
	result = cdev_add(&frandom_cdev, MKDEV(frandom_major, frandom_minor), 1);
	if (result) {
	  printk(KERN_WARNING "frandom: Failed to add cdev for /dev/frandom\n");
	  goto error1;
	}

	result = register_chrdev_region(MKDEV(frandom_major, frandom_minor), 1, "/dev/frandom");
	if (result < 0) {
		printk(KERN_WARNING "frandom: can't get major/minor %d/%d\n", frandom_major, frandom_minor);
	  goto error2;
	}

	frandom_device = device_create(frandom_class, NULL, MKDEV(frandom_major, frandom_minor), NULL, "frandom");

	if (IS_ERR(frandom_device)) {
		printk(KERN_WARNING "frandom: Failed to create frandom device\n");
		goto error3;
	}

	cdev_init(&erandom_cdev, &frandom_fops);
	erandom_cdev.owner = THIS_MODULE;
	result = cdev_add(&erandom_cdev, MKDEV(frandom_major, erandom_minor), 1);
	if (result) {
	  printk(KERN_WARNING "frandom: Failed to add cdev for /dev/erandom\n");
	  goto error4;
	}

	result = register_chrdev_region(MKDEV(frandom_major, erandom_minor), 1, "/dev/erandom");
	if (result < 0) {
		printk(KERN_WARNING "frandom: can't get major/minor %d/%d\n", frandom_major, erandom_minor);
		goto error5;
	}

	erandom_device = device_create(frandom_class, NULL, MKDEV(frandom_major, erandom_minor), NULL, "erandom");

	if (IS_ERR(erandom_device)) {
		printk(KERN_WARNING "frandom: Failed to create erandom device\n");
		goto error6;
	}
	return 0; /* succeed */

 error6:
	unregister_chrdev_region(MKDEV(frandom_major, erandom_minor), 1);
 error5:
	cdev_del(&erandom_cdev);
 error4:
	device_destroy(frandom_class, MKDEV(frandom_major, frandom_minor));
 error3:
	unregister_chrdev_region(MKDEV(frandom_major, frandom_minor), 1);
 error2:
	cdev_del(&frandom_cdev);
 error1:
	class_destroy(frandom_class);
 error0:
	kfree(erandom_state->buf);
	kfree(erandom_state);

	return result;	
}

module_init(frandom_init_module);
module_exit(frandom_cleanup_module);

EXPORT_SYMBOL(erandom_get_random_bytes);

MODULE_AUTHOR("Eli Billauer <eli@billauer.co.il>");
MODULE_DESCRIPTION("'char_random_frandom' - A fast random generator for "
"general usage");
MODULE_LICENSE("GPL");

