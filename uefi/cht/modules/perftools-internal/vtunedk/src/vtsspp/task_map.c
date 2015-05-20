/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "task_map.h"

#include <linux/jhash.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/slab.h>

#ifdef CONFIG_PREEMPT_RT
static DEFINE_RAW_RWLOCK(vtss_task_map_lock);
#else
static DEFINE_RWLOCK(vtss_task_map_lock);
#endif

/* Should be 2^n */
#define HASH_TABLE_SIZE (1 << 10)

static struct hlist_head vtss_task_map_hash_table[HASH_TABLE_SIZE] = { {NULL} };
static atomic_t  vtss_map_initialized = ATOMIC_INIT(0);
/** Compute the map hash */
static inline u32 vtss_task_map_hash(pid_t key) __attribute__ ((always_inline));
static inline u32 vtss_task_map_hash(pid_t key)
{
    return (jhash_1word(key, 0) & (HASH_TABLE_SIZE - 1));
}

/**
 * Get an item if it's present in the hash table and increment its usage.
 * Returns NULL if not present.
 * Takes a read lock on vtss_task_map_lock.
 */
vtss_task_map_item_t* vtss_task_map_get_item(pid_t key)
{
    unsigned long flags;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    struct hlist_node *node = NULL;
#endif
    struct hlist_node *temp = NULL;
    vtss_task_map_item_t *item;
//    printk("get start");
    if (atomic_read(&vtss_map_initialized)==0)return NULL;

    read_lock_irqsave(&vtss_task_map_lock, flags);
    head = &vtss_task_map_hash_table[vtss_task_map_hash(key)];
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    hlist_for_each_entry_safe(item, node, temp, head, hlist)
#else
    hlist_for_each_entry_safe(item, temp, head,  hlist)
#endif    
    {
        if (key == item->key) {
            atomic_inc(&item->usage);
            read_unlock_irqrestore(&vtss_task_map_lock, flags);
//            printk("get end 1");
            return item;
        }
    }
    read_unlock_irqrestore(&vtss_task_map_lock, flags);
//    printk("get end");
    return NULL;
}

/**
 * Decrement count and destroy if usage == 0.
 * Returns 1 if item was destroyed otherwise 0.
 */
int vtss_task_map_put_item(vtss_task_map_item_t* item)
{
    unsigned long flags;

//    printk("put start");
    if ((item != NULL) && atomic_dec_and_test(&item->usage)) {
        if (item->in_list) {
            write_lock_irqsave(&vtss_task_map_lock, flags);
            if (item->in_list) {
                item->in_list = 0;
                hlist_del_init(&item->hlist);
            }
            write_unlock_irqrestore(&vtss_task_map_lock, flags);
        }
        if (atomic_read(&item->usage) == 0) {
            if (item->dtor)
                item->dtor(item, NULL);
            item->dtor = NULL;
            kfree(item);
//            printk("put end 1");
            return 1;
        }
    }
//    printk("put end");
    return 0;
}

/**
 * Add the item into the hash table with incremented usage.
 * Remove the item with the same key.
 * Returns 1 if old item was destroyed otherwise 0.
 * Takes a write lock on vtss_task_map_lock.
 */
int vtss_task_map_add_item(vtss_task_map_item_t* item2)
{
    unsigned long flags;
    int ret = 0;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    struct hlist_node *node = NULL;
#endif
    vtss_task_map_item_t *item = NULL;
    struct hlist_node *temp = NULL;
//    printk("add item\n");
    if ((item2 != NULL) && !item2->in_list) {
        write_lock_irqsave(&vtss_task_map_lock, flags);
        if (!item2->in_list)
        {
        head = &vtss_task_map_hash_table[vtss_task_map_hash(item2->key)];
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
        hlist_for_each_entry_safe(item, node, temp, head, hlist)
#else
        hlist_for_each_entry_safe(item, temp, head, hlist)
#endif
        {
            if (item2->key == item->key) {
                /* Already there, remove it */
                hlist_del_init(&item->hlist);
//                printk("add item delete from list\n");
                if (atomic_read(&item->usage) == 0){
                    //item = NULL; // it will be deleted in "put"
                    item->in_list = 0;
                }
                else if ( atomic_dec_and_test(&item->usage)) {
//                    printk("add item delete item really\n");
                    if (item->dtor)
                        item->dtor(item, NULL);
                    item->dtor = NULL;
                    kfree(item);
                    ret = 1;
                    //item = NULL;
                } else item->in_list = 0;
                break;
            }
            //item = NULL;
        }
        atomic_inc(&item2->usage);
        hlist_add_head(&item2->hlist, head);
        item2->in_list = 1;
        write_unlock_irqrestore(&vtss_task_map_lock, flags);
        }
    }
    return ret;
}

/**
 * Remove the item from the hash table and destroy if usage == 0.
 * Returns 1 if item was destroyed otherwise 0.
 * Takes a write lock on vtss_task_map_lock.
 */
int vtss_task_map_del_item(vtss_task_map_item_t* item)
{
    unsigned long flags;

//    printk("delete item\n");
    if (item != NULL) {
        if (item->in_list) {
            write_lock_irqsave(&vtss_task_map_lock, flags);
            if (item->in_list) {
                item->in_list = 0;
                hlist_del_init(&item->hlist);
            }
            write_unlock_irqrestore(&vtss_task_map_lock, flags);
        }
        if (atomic_dec_and_test(&item->usage)) {
//    printk("delete item really\n");
            if (item->dtor)
                item->dtor(item, NULL);
            item->dtor = NULL;
            kfree(item);
            return 1;
        }
    }
    return 0;
}

/**
 * allocate item + data but not insert it into the hash table, usage = 1
 * Takes a write lock on vtss_task_map_lock.
 */
vtss_task_map_item_t* vtss_task_map_alloc(pid_t key, size_t size, vtss_task_map_func_t* dtor, gfp_t flags)
{
    vtss_task_map_item_t *item = (vtss_task_map_item_t*)kmalloc(sizeof(vtss_task_map_item_t)+size, flags);

    if (item != NULL) {
        atomic_set(&item->usage, 1);
        item->key     = key;
        item->in_list = 0;
        item->dtor    = dtor;
    }
    return item;
}

int vtss_task_map_foreach(vtss_task_map_func_t* func, void* args)
{
    int i;
    unsigned long flags;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    struct hlist_node *node = NULL;
#endif
    vtss_task_map_item_t *item;

    if (func == NULL) {
        ERROR("Function pointer is NULL");
        return -EINVAL;
    }
    read_lock_irqsave(&vtss_task_map_lock, flags);
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        head = &vtss_task_map_hash_table[i];
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
        hlist_for_each_entry(item, node, head, hlist)
#else
        hlist_for_each_entry(item, head, hlist)
#endif
        {
            func(item, args);
        }
    }
    read_unlock_irqrestore(&vtss_task_map_lock, flags);
    return 0;
}

int vtss_task_map_init(void)
{
    int i;
    unsigned long flags;
    struct hlist_head *head;

    write_lock_irqsave(&vtss_task_map_lock, flags);
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        head = &vtss_task_map_hash_table[i];
        INIT_HLIST_HEAD(head);
    }
    write_unlock_irqrestore(&vtss_task_map_lock, flags);
    
    atomic_set(&vtss_map_initialized,1);
    
    
    
    return 0;
}

void vtss_task_map_fini(void)
{
    int i;
    unsigned long flags;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
    struct hlist_node *node = NULL;
#endif
    struct hlist_node *temp;
    vtss_task_map_item_t *item;

    atomic_set(&vtss_map_initialized,0);
    write_lock_irqsave(&vtss_task_map_lock, flags);
    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        head = &vtss_task_map_hash_table[i];
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0)
        hlist_for_each_entry_safe(item, node, temp, head, hlist)
#else        
        hlist_for_each_entry_safe(item, temp, head, hlist)
#endif
        {
            hlist_del_init(&item->hlist);
            if (atomic_read(&item->usage) == 0){
                //item = NULL; // it will be deleted in "put"
                item->in_list = 0;
            }
            else if (atomic_dec_and_test(&item->usage)) {
                if (item->dtor)
                    item->dtor(item, NULL);
                item->dtor = NULL;
                kfree(item);
            } else {
                item->in_list = 0;
                ERROR("item=0x%p is busy now, key=%d, usage=%d", item, item->key, atomic_read(&item->usage));
            }
        }
        INIT_HLIST_HEAD(head);
    }
    write_unlock_irqrestore(&vtss_task_map_lock, flags);
}
