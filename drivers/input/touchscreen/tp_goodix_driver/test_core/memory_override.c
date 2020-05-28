#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>		/* for list_head */
#include <linux/slab.h>
#include <asm/spinlock.h>
#include "board_opr_interface.h"

void *malloc(size_t size)
{
	void *ptr;
	ptr = kmalloc(size, GFP_KERNEL);
	if (!ptr)
		return NULL;
	return ptr;
}

void free(void *ptr)
{
	if (!ptr)
		return;
	kfree(ptr);
}

void *realloc(void *ptr, size_t size)
{
	void *new = NULL;

	new = kmalloc(size, GFP_KERNEL);
	if (!new)
		return NULL;
	if (ptr)
		kfree(ptr);
	ptr = NULL;

	return new;
}
