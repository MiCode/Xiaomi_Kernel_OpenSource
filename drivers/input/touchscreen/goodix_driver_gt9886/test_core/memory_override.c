#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>		/* for list_head */
#include <linux/slab.h>
#include <asm/spinlock.h>
#include "board_opr_interface.h"

typedef struct memblock_s {
	struct list_head list;
	void *ptr;
	size_t size;
} memblock_t;

/*
* The list of memory blocks (one simple list)
*/
static LIST_HEAD(memblock_list);
/*static rwlock_t memblock_lock = RW_LOCK_UNLOCKED;*/
static DEFINE_RWLOCK(memblock_lock);

static int memblock_add(void *ptr, size_t size)
{
	memblock_t *m;
	m = kmalloc(sizeof(memblock_t), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->ptr = ptr;
	m->size = size;

	write_lock(&memblock_lock);
	list_add(&m->list, &memblock_list);
	write_unlock(&memblock_lock);
	return 0;
}

static void memblock_del(memblock_t *m)
{
	write_lock(&memblock_lock);
	list_del(&m->list);
	write_unlock(&memblock_lock);
	kfree(m);
}

static memblock_t *memblock_get(void *ptr)
{
	struct list_head *l;
	memblock_t *m;

	read_lock(&memblock_lock);
	list_for_each(l, &memblock_list) {
		m = list_entry(l, memblock_t, list);
		if (m->ptr == ptr) {
			/* HIT */
			read_unlock(&memblock_lock);
			return m;
		}
	}
	read_unlock(&memblock_lock);
	return NULL;
}

void *malloc(size_t size)
{
	void *ptr;
	ptr = kmalloc(size, GFP_KERNEL);
	if (!ptr)
		return NULL;
	if (memblock_add(ptr, size)) {
		kfree(ptr);
		return NULL;
	}
	return ptr;
}

void free(void *ptr)
{
	memblock_t *m;
	if (!ptr)
		return;
	m = memblock_get(ptr);
	if (!m) {
		printk(KERN_ERR "bug: free non-exist memory\n");
		return;
	}
	memblock_del(m);
	kfree(ptr);
}

void *realloc(void *ptr, size_t size)
{
	memblock_t *m;
	void *new = NULL;

	if (ptr) {
		m = memblock_get(ptr);
		if (!m) {
			printk(KERN_ERR "bug: realloc non-exist memory\n");
			return NULL;
		}
		if (size == m->size)
			return ptr;
		if (size != 0) {
			new = kmalloc(size, GFP_KERNEL);
			if (!new)
				return NULL;
			memmove(new, ptr, m->size);
			if (memblock_add(new, size)) {
				kfree(new);
				return NULL;
			}
		}
		memblock_del(m);
		kfree(ptr);
	} else {
		if (size != 0) {
			new = kmalloc(size, GFP_KERNEL);
			if (!new)
				return NULL;
			if (memblock_add(new, size)) {
				kfree(new);
				return NULL;
			}
		}
	}
	return new;
}
