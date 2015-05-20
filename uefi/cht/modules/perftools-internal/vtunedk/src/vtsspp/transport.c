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
#include "transport.h"
#include "procfs.h"
#ifdef VTSS_USE_UEC
#include "uec.h"
#else
#include <linux/ring_buffer.h>
#include <asm/local.h>
#endif

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/delay.h>        /* for msleep_interruptible() */
#include <linux/fs.h>           /* for struct file_operations */
#include <linux/namei.h>        /* for struct nameidata       */
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/nmi.h>

#include "vtsstrace.h"

#ifndef VTSS_MERGE_MEM_LIMIT
#define VTSS_MERGE_MEM_LIMIT 160 /* max pages allowed */
#endif

/* Define this to wake up transport by timeout */
/* transprot timer interval in jiffies  (default 10ms) */
#define VTSS_TRANSPORT_TIMER_INTERVAL   (10 * HZ / 1000)
#define VTSS_TRANSPORT_COMPLETE_TIMEOUT 1000 /*< wait count about 100sec */

#define VTSS_MAX_RING_BUF_SIZE (unsigned long)VTSS_RING_BUFFER_PAGE_SIZE*256

#ifndef preempt_enable_no_resched
#define preempt_enable_no_resched() preempt_enable()
#endif


#ifndef VTSS_USE_UEC

struct vtss_transport_entry
{
    unsigned long  seqnum;
    unsigned short size;
    char           data[0];
};

struct vtss_transport_temp
{
    struct vtss_transport_temp* prev;
    struct vtss_transport_temp* next;
    unsigned long seq_begin;
    unsigned long seq_end;
    size_t        size;
    unsigned int  order;
    char          data[0];
};

/* The value was gotten from the kernel's ring_buffer code. */
#define VTSS_RING_BUFFER_PAGE_SIZE      4080
#define VTSS_TRANSPORT_MAX_RESERVE_SIZE (VTSS_RING_BUFFER_PAGE_SIZE - \
                                        sizeof(struct ring_buffer_event) - \
                                        sizeof(struct vtss_transport_entry) - 64)
#define VTSS_TRANSPORT_IS_EMPTY(trnd)   (1 + (atomic_read(&trnd->seqnum) - trnd->seqdone) == 0)
#define VTSS_TRANSPORT_DATA_READY(trnd) (1 + (atomic_read(&trnd->seqnum) - trnd->seqdone) > VTSS_MERGE_MEM_LIMIT/4)

struct rb_page
{
    u64     ts;
    local_t commit;
    char    data[VTSS_RING_BUFFER_PAGE_SIZE];
};

#endif /* VTSS_USE_UEC */

extern int uid;
extern int gid;
extern int mode;

static struct timer_list vtss_transport_timer;

#ifdef CONFIG_PREEMPT_RT
static DEFINE_RAW_SPINLOCK(vtss_transport_list_lock);
#else
static DEFINE_SPINLOCK(vtss_transport_list_lock);
#endif
static LIST_HEAD(vtss_transport_list);

static atomic_t vtss_transport_npages = ATOMIC_INIT(0);

#define VTSS_TR_REG    (1<<0)
#define VTSS_TR_CFG    (1<<1) /* aux */

struct vtss_transport_data
{
    struct list_head    list;
    struct file*        file;
    wait_queue_head_t   waitq;
    char                name[36];    /* enough for "%d-%d.%d.aux" */

    atomic_t            refcount;
    atomic_t            loscount;
    atomic_t            is_attached;
    atomic_t            is_complete;
    atomic_t            is_overflow;

#ifdef VTSS_USE_UEC
    uec_t*              uec;
#else
    struct vtss_transport_temp* head;
    struct ring_buffer* buffer;
    unsigned long       seqdone;
    unsigned long       seqcpu[NR_CPUS];
    atomic_t            seqnum;
    int                 is_abort;
#endif
    int type;
};

void vtss_transport_addref(struct vtss_transport_data* trnd)
{
    atomic_inc(&trnd->refcount);
}

int vtss_transport_delref(struct vtss_transport_data* trnd)
{
    return atomic_dec_return(&trnd->refcount);
}

char *vtss_transport_get_filename(struct vtss_transport_data* trnd)
{
    return trnd->name;
}

int vtss_transport_is_overflowing(struct vtss_transport_data* trnd)
{
    return atomic_read(&trnd->is_overflow);
}
int vtss_transport_is_attached(struct vtss_transport_data* trnd)
{
    return atomic_read(&trnd->is_attached);
}

#ifdef VTSS_USE_UEC

void vtss_transport_callback(uec_t* uec, int reason, void *context)
{
    TRACE("context=0x%p, reason=%d", context, reason);
}

#define UEC_FREE_SIZE(uec) \
({ (uec->tail <= uec->head) ? \
        uec->hsize - (size_t)(uec->head - uec->tail) \
    : \
        (size_t)(uec->tail - uec->head); \
})

#define UEC_FILLED_SIZE(uec) \
({  size_t tsize = 0; \
    if (uec->head > uec->tail) { \
        tsize = (size_t)(uec->head - uec->tail); \
    } else if (uec->head < uec->tail || (uec->head == uec->tail && uec->ovfl)) { \
        tsize = (size_t)(uec->tsize - (uec->tail - uec->buffer)); \
    } \
    tsize; \
})

#define VTSS_TRANSPORT_IS_EMPTY(trnd)   (UEC_FILLED_SIZE(trnd->uec) == 0)
#define VTSS_TRANSPORT_DATA_READY(trnd) (UEC_FILLED_SIZE(trnd->uec) != 0)
 
int vtss_transport_record_write(struct vtss_transport_data* trnd, void* part0, size_t size0, void* part1, size_t size1, int is_safe)
{
    int rc = 0;

    if (trnd == NULL) {
        ERROR("Transport is NULL");
        return -EINVAL;
    }

    if (atomic_read(&trnd->is_complete)) {
        TRACE("Transport is COMPLETED");
        return -EINVAL;
    }

    /* Don't use spill notifications from uec therefore its UECMODE_SAFE always */
    rc = trnd->uec->put_record(trnd->uec, part0, size0, part1, size1, UECMODE_SAFE);

    if (is_safe) {
        TRACE("WAKE UP");
        if (waitqueue_active(&trnd->waitq))
            wake_up_interruptible(&trnd->waitq);
    }
    if (rc) {
        atomic_inc(&trnd->loscount);
    }
    return rc;
}

#else  /* VTSS_USE_UEC */

/*void* vtss_transport_record_reserve_try_hard(struct vtss_transport_data* trnd, void** entry, size_t size)
{
      void* record = NULL;
    void* record = vtss_transport_record_reserve(trnd, entry, size);
    if (record != NULL){
        return record;
    }
    if (unlikely(trnd == NULL || entry == NULL)) {
        ERROR("Transport or Entry is NULL");
        return NULL;
    }
    if (unlikely(atomic_read(&trnd->is_complete))) {
        TRACE("'%s' is COMPLETED", trnd->name);
        return NULL;
    }
    if (unlikely(size == 0 || size > 0xffff)) {
        TRACE("'%s' incorrect size (%zu bytes)", trnd->name, size);
        return NULL;
    }

    while ((!record) && (ring_buffer_size(trnd->buffer) < VTSS_MAX_RING_BUF_SIZE))
    {
        unsigned long rb_size = ring_buffer_size(trnd->buffer)>>12;
        printk("before reallocated,  buffer_size = %lx, rb = %lu\n",  ring_buffer_size(trnd->buffer), rb_size);
        ring_buffer_resize(trnd->buffer, (rb_size+1)* PAGE_SIZE);
        printk("reallocated,  buffer_size = %lx\n",  ring_buffer_size(trnd->buffer));
        record = vtss_transport_record_reserve(trnd, entry, size);
    }
    return record;
}
*/
void* vtss_transport_record_reserve(struct vtss_transport_data* trnd, void** entry, size_t size)
{
    struct ring_buffer_event* event;
    struct vtss_transport_entry* data;

    if (unlikely(trnd == NULL || entry == NULL)) {
        ERROR("Transport or Entry is NULL");
        return NULL;
    }

    if (unlikely(atomic_read(&trnd->is_complete))) {
        TRACE("'%s' is COMPLETED", trnd->name);
        return NULL;
    }

    if (unlikely(size == 0 || size > 0xffff /* max short */)) {
        TRACE("'%s' incorrect size (%zu bytes)", trnd->name, size);
        return NULL;
    }

    if (likely(size < VTSS_TRANSPORT_MAX_RESERVE_SIZE)) {
#if 0
        if (atomic_read(&vtss_transport_npages) > VTSS_MERGE_MEM_LIMIT/2) {
            TRACE("'%s' memory limit for data %zu bytes", trnd->name, size);
            atomic_inc(&trnd->loscount);
            return NULL;
        }
#endif
#ifdef VTSS_AUTOCONF_RING_BUFFER_FLAGS
        event = ring_buffer_lock_reserve(trnd->buffer, size + sizeof(struct vtss_transport_entry), 0);
#else
        event = ring_buffer_lock_reserve(trnd->buffer, size + sizeof(struct vtss_transport_entry));
#endif
        if (unlikely(event == NULL)) {
            atomic_inc(&trnd->loscount);
            atomic_inc(&trnd->is_overflow);
            TRACE("'%s' ring_buffer_lock_reserve failed 1, size = %d", trnd->name, (int)(size + sizeof(struct vtss_transport_entry)));
//            ERROR("'%s' ring_buffer_lock_reserve failed 1, size = %d, reserved = %d, seqnum=%d", trnd->name, (int)(size + sizeof(struct vtss_transport_entry)), trnd->reserved, (int)atomic_read(&trnd->seqnum));
            return NULL;
        }
        *entry = (void*)event;
        data = (struct vtss_transport_entry*)ring_buffer_event_data(event);
        data->seqnum = atomic_inc_return(&trnd->seqnum);
        data->size   = size;
        return (void*)data->data;
    } else { /* blob */
        unsigned int order = get_order(size + sizeof(struct vtss_transport_temp));
        struct vtss_transport_temp* blob;

        if (atomic_read(&vtss_transport_npages) > VTSS_MERGE_MEM_LIMIT/2) {
            TRACE("'%s' memory limit for blob %zu bytes", trnd->name, size);
//            ERROR("'%s' memory limit for blob %zu bytes", trnd->name, size);
            atomic_inc(&trnd->loscount);
            return NULL;
        }
        blob = (struct vtss_transport_temp*)__get_free_pages((GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN), order);
        if (unlikely(blob == NULL)) {
            TRACE("'%s' no memory for blob %zu bytes", trnd->name, size);
//            ERROR("'%s' no memory for blob %zu bytes", trnd->name, size);
            atomic_inc(&trnd->loscount);
            return NULL;
        }
        atomic_add(1<<order, &vtss_transport_npages);
        blob->size  = size;
        blob->order = order;
#ifdef VTSS_AUTOCONF_RING_BUFFER_FLAGS
        event = ring_buffer_lock_reserve(trnd->buffer, sizeof(void*) + sizeof(struct vtss_transport_entry), 0);
#else
        event = ring_buffer_lock_reserve(trnd->buffer, sizeof(void*) + sizeof(struct vtss_transport_entry));
#endif
        if (unlikely(event == NULL)) {
            free_pages((unsigned long)blob, order);
            atomic_sub(1<<order, &vtss_transport_npages);
            atomic_inc(&trnd->loscount);
            atomic_inc(&trnd->is_overflow);
            TRACE("'%s' ring_buffer_lock_reserve failed overflow", trnd->name);
//            ERROR("'%s' ring_buffer_lock_reserve failed overflow", trnd->name);
            return NULL;
        }
        *entry = (void*)event;
        data = (struct vtss_transport_entry*)ring_buffer_event_data(event);
        data->seqnum = atomic_inc_return(&trnd->seqnum);
        data->size   = 0;
        *((void**)&(data->data)) = (void*)blob;
        return (void*)blob->data;
    }
}

int vtss_transport_is_ready(struct vtss_transport_data* trnd)
{
    /*if (atomic_read(&trnd->is_complete)) {
        TRACE("Transport is COMPLETED");
        return 0;
    }
    return (trnd->seqdone > 1 || waitqueue_active(&trnd->waitq));*/
    return atomic_read(&trnd->is_attached);
}

int vtss_transport_record_commit(struct vtss_transport_data* trnd, void* entry, int is_safe)
{
    int rc = 0;
    struct ring_buffer_event* event = (struct ring_buffer_event*)entry;

    if (unlikely(trnd == NULL || entry == NULL)) {
        ERROR("Transport or Entry is NULL");
        return -EINVAL;
    }
#ifdef VTSS_AUTOCONF_RING_BUFFER_FLAGS
    rc = ring_buffer_unlock_commit(trnd->buffer, event, 0);
#else
    rc = ring_buffer_unlock_commit(trnd->buffer, event);
#endif
    if (rc) {
        struct vtss_transport_entry* data = (struct vtss_transport_entry*)ring_buffer_event_data(event);
        ERROR("'%s' commit error: seq=%lu, size=%u", trnd->name, data->seqnum, data->size);
    }
    if (unlikely(is_safe && VTSS_TRANSPORT_DATA_READY(trnd))) {
        if (waitqueue_active(&trnd->waitq))
        {
            wake_up_interruptible(&trnd->waitq);
        }
    }
    return rc;
}

int vtss_transport_record_write(struct vtss_transport_data* trnd, void* part0, size_t size0, void* part1, size_t size1, int is_safe)
{
    int rc = -EFAULT;
    void* entry;
    void* p = vtss_transport_record_reserve(trnd, &entry, size0 + size1);
    if (p) {
        memcpy(p, part0, size0);
        if (size1)
            memcpy(p + size0, part1, size1);
        rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
    return rc;
}

#endif /* VTSS_USE_UEC */

int vtss_transport_record_write_all(void* part0, size_t size0, void* part1, size_t size1, int is_safe)
{
    int rc = 0;
    unsigned long flags;
    struct list_head *p;
    struct vtss_transport_data *trnd = NULL;

    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    list_for_each(p, &vtss_transport_list) {
        trnd = list_entry(p, struct vtss_transport_data, list);
        TRACE("put_record(%d) to trnd=0x%p => '%s'", atomic_read(&trnd->is_complete), trnd, trnd->name);
        if (likely(!atomic_read(&trnd->is_complete) && trnd->type == VTSS_TR_REG)) {
#ifdef VTSS_USE_UEC
            /* Don't use spill notifications from uec therefore its UECMODE_SAFE always */
            int rc1 = trnd->uec->put_record(trnd->uec, part0, size0, part1, size1, UECMODE_SAFE);
            if (rc1) {
                atomic_inc(&trnd->loscount);
                rc = -EFAULT;
            }
            if (unlikely(is_safe && VTSS_TRANSPORT_DATA_READY(trnd))) {
                TRACE("WAKE UP");
                if (waitqueue_active(&trnd->waitq))
                    wake_up_interruptible(&trnd->waitq);
            }
#else  /* VTSS_USE_UEC */
            void* entry;
            void* p = vtss_transport_record_reserve(trnd, &entry, size0 + size1);
            if (likely(p)) {
                memcpy(p, part0, size0);
                if (size1)
                    memcpy(p + size0, part1, size1);
                rc = vtss_transport_record_commit(trnd, entry, is_safe) ? -EFAULT : rc;
            } else {
                rc = -EFAULT;
            }
#endif /* VTSS_USE_UEC */
        }
    }
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
    return rc;
}

#ifndef VTSS_USE_UEC

static void vtss_transport_temp_free_all(struct vtss_transport_data* trnd, struct vtss_transport_temp** head)
{
    struct vtss_transport_temp* temp;
    struct vtss_transport_temp** pstore = head;

    while ((temp = *pstore) != NULL) {
        if (temp->prev) {
            pstore = &(temp->prev);
            continue;
        }
        if (temp->next) {
            pstore = &(temp->next);
            continue;
        }
        TRACE("'%s' [%lu, %lu), size=%zu of %lu",
                trnd->name, temp->seq_begin, temp->seq_end,
                temp->size, (PAGE_SIZE << temp->order));
        free_pages((unsigned long)temp, temp->order);
        atomic_sub(1<<temp->order, &vtss_transport_npages);
        *pstore = NULL;
        pstore = head; /* restart from head */
    }
}

static struct vtss_transport_temp* vtss_transport_temp_merge(struct vtss_transport_data* trnd, struct vtss_transport_temp** pstore)
{
    struct vtss_transport_temp* temp = *pstore;

    if (temp != NULL) {
        struct vtss_transport_temp* prev = temp->prev;
        struct vtss_transport_temp* next = temp->next;

        /* try to merge with prev element... */
        if (prev != NULL && (prev->seq_end == temp->seq_begin) &&
            /* check for enough space in buffer */
            ((prev->size + temp->size + sizeof(struct vtss_transport_temp)) < (PAGE_SIZE << prev->order)))
        {
            TRACE("'%s' [%lu - %lu), size=%zu <+ [%lu - %lu), size=%zu", trnd->name,
                prev->seq_begin, prev->seq_end, prev->size,
                temp->seq_begin, temp->seq_end, temp->size);
            memcpy(&(prev->data[prev->size]), temp->data, temp->size);
            prev->size += temp->size;
            prev->seq_end = temp->seq_end;
            if (prev->next) {
                ERROR("'%s' [%lu, %lu) incorrect next link", trnd->name,
                        prev->seq_begin, prev->seq_end);
                vtss_transport_temp_free_all(trnd, &(prev->next));
            }
            prev->next = temp->next;
            *pstore = prev;
            free_pages((unsigned long)temp, temp->order);
            atomic_sub(1<<temp->order, &vtss_transport_npages);
            return prev;
        }
        /* try to merge with next element... */
        if (next != NULL && (next->seq_begin == temp->seq_end) &&
            /* check for enough space in buffer */
            ((next->size + temp->size + sizeof(struct vtss_transport_temp)) < (PAGE_SIZE << temp->order)))
        {
            TRACE("'%s' [%lu - %lu), size=%zu +> [%lu - %lu), size=%zu", trnd->name,
                temp->seq_begin, temp->seq_end, temp->size,
                next->seq_begin, next->seq_end, next->size);
            memcpy(&(temp->data[temp->size]), next->data, next->size);
            temp->size += next->size;
            temp->seq_end = next->seq_end;
            temp->next = next->next;
            if (next->prev) {
                ERROR("'%s' [%lu, %lu) incorrect prev link", trnd->name,
                        next->seq_begin, next->seq_end);
                vtss_transport_temp_free_all(trnd, &(next->prev));
            }
            free_pages((unsigned long)next, next->order);
            atomic_sub(1<<next->order, &vtss_transport_npages);
            return temp;
        }
    }
    return temp;
}

static int vtss_transport_temp_store_data(struct vtss_transport_data* trnd, unsigned long seqnum, void* data, unsigned short size)
{
    struct vtss_transport_temp* temp;
    struct vtss_transport_temp** pstore = &(trnd->head);
    unsigned int order = get_order(size + sizeof(struct vtss_transport_temp));
    while (((temp = vtss_transport_temp_merge(trnd, pstore)) != NULL) && (seqnum != temp->seq_end)) {
        pstore = (seqnum < temp->seq_begin) ? &(temp->prev) : &(temp->next);
    }
    if (temp == NULL) {
        struct vtss_transport_temp* temp1;
        struct vtss_transport_temp** pstore1 = &(trnd->head);
        while (((temp1 = *pstore1) != NULL) && ((seqnum + 1) != temp1->seq_begin)) {
            pstore1 = (seqnum < temp1->seq_begin) ? &(temp1->prev) : &(temp1->next);
        }
        if (temp1 != NULL) { /* try to prepend */
            /* check for enough space in buffer */
            if ((temp1->size + size + sizeof(struct vtss_transport_temp)) < (PAGE_SIZE << temp1->order)) {
                TRACE("'%s' [%lu - %lu), size=%u +> [%lu - %lu), size=%zu",
                        trnd->name, seqnum, seqnum + 1, size,
                        temp1->seq_begin, temp1->seq_end, temp1->size);
                memmove(&(temp1->data[size]), temp1->data, temp1->size);
                memcpy(temp1->data, data, size);
                temp1->seq_begin = seqnum;
                temp1->size += size;
                vtss_transport_temp_merge(trnd, pstore1);
                return 0;
            }
        }
        TRACE("'%s' new [%lu - %lu), size=%u", trnd->name, seqnum, seqnum + 1, size);
        temp = (struct vtss_transport_temp*)__get_free_pages((GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN), order);
        if (temp == NULL) {
            return -ENOMEM;
        }
        atomic_add(1<<order, &vtss_transport_npages);
        temp->prev  = NULL;
        temp->next  = NULL;
        temp->seq_begin = seqnum;
        temp->size  = 0;
        temp->order = order;
        if (*pstore) {
            ERROR("'%s' new [%lu - %lu), size=%u ==> [%lu - %lu)", trnd->name,
                    seqnum, seqnum + 1, size, (*pstore)->seq_begin, (*pstore)->seq_end);
        }
        *pstore = temp;
    } else {
        /* check for enough space in buffer */
        if ((temp->size + size + sizeof(struct vtss_transport_temp)) >= (PAGE_SIZE << temp->order)) {
            struct vtss_transport_temp* next;
            TRACE("'%s' new [%lu - %lu), size=%u, temp->size=%zu", trnd->name,
                    seqnum, seqnum + 1, size, temp->size);
            next = (struct vtss_transport_temp*)__get_free_pages((GFP_NOWAIT | __GFP_NORETRY | __GFP_NOWARN), order);
            if (next == NULL) {
                return -ENOMEM;
            }
            atomic_add(1<<order, &vtss_transport_npages);
            next->prev  = NULL;
            next->next  = temp->next;
            next->seq_begin = seqnum;
            next->size  = 0;
            next->order = order;
            temp->next  = next;
            pstore = &(temp->next);
            temp = next;
        } else {
            TRACE("'%s' [%lu - %lu), size=%zu <+ [%lu - %lu), size=%u", trnd->name,
                    temp->seq_begin, temp->seq_end, temp->size,
                    seqnum, seqnum + 1, size);
        }
    }
    memcpy(&(temp->data[temp->size]), data, size);
    temp->seq_end = seqnum + 1;
    temp->size += size;
    vtss_transport_temp_merge(trnd, pstore);
    return 0;
}

static int vtss_transport_temp_store_blob(struct vtss_transport_data* trnd, unsigned long seqnum, struct vtss_transport_temp* blob)
{
    struct vtss_transport_temp* temp;
    struct vtss_transport_temp** pstore = &(trnd->head);

    TRACE("'%s' blob [%lu - %lu), size=%zu", trnd->name, seqnum, seqnum + 1, blob->size);
    while (((temp = vtss_transport_temp_merge(trnd, pstore)) != NULL) && (seqnum != temp->seq_end)) {
        pstore = (seqnum < temp->seq_begin) ? &(temp->prev) : &(temp->next);
    }
    blob->prev      = NULL;
    blob->seq_begin = seqnum;
    blob->seq_end   = seqnum + 1;
    if (temp == NULL) {
        blob->next = NULL;
        if (*pstore) {
            ERROR("'%s' blob [%lu - %lu), size=%zu ==> [%lu - %lu)", trnd->name,
                    seqnum, seqnum + 1, blob->size, (*pstore)->seq_begin, (*pstore)->seq_end);
        }
        *pstore = blob;
    } else {
        blob->next = temp->next;
        temp->next = blob;
    }
    return 0;
}

#define VTSS_TRANSPORT_COPY_TO_USER(src, len) do { \
    if (copy_to_user(buf, (void*)(src), (len))) { \
        ERROR("copy_to_user(0x%p, 0x%p, %zu): error", buf, (src), (len)); \
    } \
    size -= (len); \
    buf += (len); \
    rc += (len); \
} while (0)

static size_t vtss_transport_temp_flush(struct vtss_transport_data* trnd, char __user* buf, size_t size)
{
    size_t rc = 0;

    if (!trnd->is_abort) {
        struct vtss_transport_temp* temp;
        struct vtss_transport_temp** pstore = &(trnd->head);

        TRACE("'%s' -== flush begin ==- :: %u (%lu bytes) at seq=%lu", trnd->name,
                atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE, trnd->seqdone);
        /* Look for output seq with merge on the way */
        while ((temp = vtss_transport_temp_merge(trnd, pstore)) != NULL) {
            if (trnd->seqdone == temp->seq_begin) {
                if (size < temp->size)
                    break;
                TRACE("'%s' output [%lu, %lu), size=%zu", trnd->name,
                        temp->seq_begin, temp->seq_end, temp->size);
                VTSS_TRANSPORT_COPY_TO_USER(temp->data, temp->size);
                trnd->seqdone = temp->seq_end;
                *pstore = temp->next;
                if (temp->prev) {
                    ERROR("'%s' [%lu, %lu) incorrect prev link", trnd->name, temp->seq_begin, temp->seq_end);
                    vtss_transport_temp_free_all(trnd, &(temp->prev));
                }
                free_pages((unsigned long)temp, temp->order);
                atomic_sub(1<<temp->order, &vtss_transport_npages);
                pstore = &(trnd->head); /* restart from head */
            } else {
                pstore = (trnd->seqdone < temp->seq_begin) ? &(temp->prev) : &(temp->next);
            }
        }
        TRACE("'%s' -== flush  end  ==- :: %u (%lu bytes) at seq=%lu", trnd->name,
                atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE, trnd->seqdone);
    }
    return rc;
}

static size_t vtss_transport_temp_parse_event(struct vtss_transport_data* trnd, struct ring_buffer_event* event, char __user* buf, size_t size, int cpu)
{
    size_t rc = 0;
    struct vtss_transport_entry* data = (struct vtss_transport_entry*)ring_buffer_event_data(event);
    unsigned long seqnum = data->seqnum;

    trnd->seqcpu[cpu] = seqnum;
    if (trnd->is_abort || trnd->seqdone > seqnum) {
        if (data->size) {
            TRACE("DROP seq=%lu, size=%u, from cpu%d", seqnum, data->size, cpu);
        } else { /* blob */
            struct vtss_transport_temp* blob = *((struct vtss_transport_temp**)(data->data));
            TRACE("DROP seq=%lu, size=%zu, from cpu%d", seqnum, blob->size, cpu);
            free_pages((unsigned long)blob, blob->order);
            atomic_sub(1<<blob->order, &vtss_transport_npages);
        }
#ifndef VTSS_NO_MERGE
    } else if (trnd->seqdone != seqnum) { /* disordered event */
        if (data->size) {
            if (vtss_transport_temp_store_data(trnd, seqnum, data->data, data->size)) {
                ERROR("'%s' seq=%lu => store data seq=%lu error", trnd->name, trnd->seqdone, seqnum);
            }
        } else { /* blob */
            struct vtss_transport_temp* blob = *((struct vtss_transport_temp**)(data->data));
            if (vtss_transport_temp_store_blob(trnd, seqnum, blob)) {
                ERROR("'%s' seq=%lu => store blob seq=%lu error", trnd->name, trnd->seqdone, seqnum);
            }
        }
#endif
    } else { /* ordered event */
        if (data->size) {
            TRACE("'%s' output [%lu - %lu), size=%u from cpu%d", trnd->name, seqnum, seqnum+1, data->size, cpu);
#ifndef VTSS_NO_MERGE
            if (size < data->size) {
                if (vtss_transport_temp_store_data(trnd, seqnum, data->data, data->size)) {
                    ERROR("'%s' seq=%lu => store data seq=%lu error", trnd->name, trnd->seqdone, seqnum);
                }
            } else
#endif
            {
                VTSS_TRANSPORT_COPY_TO_USER(data->data, (size_t)data->size);
                trnd->seqdone++;
            }
        } else { /* blob */
            struct vtss_transport_temp* blob = *((struct vtss_transport_temp**)(data->data));
            TRACE("'%s' output [%lu - %lu), size=%zu from cpu%d", trnd->name, seqnum, seqnum+1, blob->size, cpu);
#ifndef VTSS_NO_MERGE
            if (size < blob->size) {
                if (vtss_transport_temp_store_blob(trnd, seqnum, blob)) {
                    ERROR("'%s' seq=%lu => store blob seq=%lu error", trnd->name, trnd->seqdone, seqnum);
                }
            } else
#endif
            {
                VTSS_TRANSPORT_COPY_TO_USER(blob->data,
#ifndef VTSS_NO_MERGE
                    blob->size
#else
                    (size_t)8UL /* FIXME: just something is not overflowed output buffer */
#endif
                );
                free_pages((unsigned long)blob, blob->order);
                atomic_sub(1<<blob->order, &vtss_transport_npages);
                trnd->seqdone++;
            }
        }
    }
    return rc;
}

#endif /* VTSS_USE_UEC */

static ssize_t vtss_transport_read(struct file *file, char __user* buf, size_t size, loff_t* ppos)
{
    int i, cpu;
    size_t len;
    ssize_t rc;
    struct vtss_transport_data* trnd = (struct vtss_transport_data*)file->private_data;

    if (unlikely(trnd == NULL || buf == NULL))
        return -EINVAL;
    while (!atomic_read(&trnd->is_complete) && !VTSS_TRANSPORT_DATA_READY(trnd)) {
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        rc = wait_event_interruptible(trnd->waitq,
             (atomic_read(&trnd->is_complete) || VTSS_TRANSPORT_DATA_READY(trnd)));
        if (rc < 0)
            return -ERESTARTSYS;
    }
#ifdef VTSS_USE_UEC
    rc = trnd->uec->pull(trnd->uec, buf, size);
#else
    rc = 0;
    preempt_disable();
#ifndef VTSS_NO_MERGE
    /* Flush buffers if possible first of all */
    len = vtss_transport_temp_flush(trnd, buf, size);
    size -= len;
    buf += len;
    rc += len;
#endif
    for (i = 0;
        (i < 30*num_online_cpus()) && /* no more 30 loops on each online cpu */
        (size >= VTSS_RING_BUFFER_PAGE_SIZE) && /* while buffer size is enough */
        !VTSS_TRANSPORT_IS_EMPTY(trnd) /* have something to output */
        ; i++)
    {
        int fast = 1;
        void* bpage;
        struct ring_buffer_event *event;

        for_each_online_cpu(cpu) {
            if (ring_buffer_entries_cpu(trnd->buffer, cpu) == 0)
                continue; /* nothing to read on this cpu */
            if  (trnd->seqcpu[cpu] > trnd->seqdone &&
                (1 + trnd->seqcpu[cpu] - trnd->seqdone) > VTSS_MERGE_MEM_LIMIT/3 &&
                !trnd->is_abort && !atomic_read(&trnd->is_complete))
            {
                TRACE("'%s' cpu%d=%lu :: %u (%lu bytes) at seq=%lu", trnd->name,
                        cpu, trnd->seqcpu[cpu], atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE, trnd->seqdone);
                continue; /* skip it for a while */
            }
            if (atomic_read(&vtss_transport_npages) > VTSS_MERGE_MEM_LIMIT) {
                atomic_inc(&trnd->is_overflow);
                ring_buffer_record_disable(trnd->buffer);
                ERROR("'%s' abort. Buffers %u (%lu bytes) at seq=%lu",
                        trnd->name, atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE, trnd->seqdone);
                vtss_transport_temp_free_all(trnd, &(trnd->head));
                trnd->is_abort = 1;
            } else if (atomic_read(&vtss_transport_npages) > VTSS_MERGE_MEM_LIMIT/2) {
                atomic_inc(&trnd->is_overflow);
                TRACE("'%s' cpu%d=%lu :: %u (%lu bytes) at seq=%lu", trnd->name,
                        cpu, trnd->seqcpu[cpu], atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE, trnd->seqdone);
                fast = 0; /* Carefully get events to avoid memory overflow */
            }
#ifdef VTSS_AUTOCONF_RING_BUFFER_ALLOC_READ_PAGE
            bpage = ring_buffer_alloc_read_page(trnd->buffer, cpu);
#else
            bpage = ring_buffer_alloc_read_page(trnd->buffer);
#endif
            if (bpage == NULL) {
                preempt_enable_no_resched();
                ERROR("'%s' cannot allocate free rb read page", trnd->name);
                return -EFAULT;
            }
            if (fast && ring_buffer_read_page(trnd->buffer, &bpage, PAGE_SIZE, cpu, (!trnd->is_abort && !atomic_read(&trnd->is_complete))) >= 0) {
                int i, inc;
                struct rb_page* rpage = (struct rb_page*)bpage;
                /* The commit may have missed event flags set, clear them */
                unsigned long commit = local_read(&rpage->commit) & 0xfffff;

                TRACE("page[%d]=[0 - %4lu) :: rc=%zd, size=%zu", cpu, commit, rc, size);
                for (i = 0; i < commit; i += inc) {
                    if (i >= (PAGE_SIZE - offsetof(struct rb_page, data))) {
                        ERROR("'%s' incorrect data index", trnd->name);
                        break;
                    }
                    inc = -1;
                    event = (void*)&rpage->data[i];
                    switch (event->type_len) {
                    case RINGBUF_TYPE_PADDING:
                        /* failed writes or may be discarded events */
                        inc = event->array[0] + 4;
                        break;
                    case RINGBUF_TYPE_TIME_EXTEND:
                        inc = 8;
                        break;
                    case 0:
                        len = vtss_transport_temp_parse_event(trnd, event, buf, size, cpu);
                        size -= len;
                        buf += len;
                        rc += len;
                        if (!event->array[0]) {
                            ERROR("'%s' incorrect event data", trnd->name);
                            break;
                        }
                        inc = event->array[0] + 4;
                        break;
                    default:
                        len = vtss_transport_temp_parse_event(trnd, event, buf, size, cpu);
                        size -= len;
                        buf += len;
                        rc += len;
                        inc = ((event->type_len + 1) * 4);
                    }
                    if (inc <= 0) {
                        ERROR("'%s' incorrect next data index", trnd->name);
                        break;
                    }
                } /* for each event in page */
                TRACE("page[%d] -==end==-  :: rc=%zd, size=%zu", cpu, rc, size);
            } else { /* reader page is not full of careful, so read one by one */
                u64 ts;
                int count;

                for (count = 0; count < 10 && NULL !=
#ifdef VTSS_AUTOCONF_RING_BUFFER_LOST_EVENTS
                    (event = ring_buffer_peek(trnd->buffer, cpu, &ts, NULL))
#else
                    (event = ring_buffer_peek(trnd->buffer, cpu, &ts))
#endif
                    ; count++)
                {
                    struct vtss_transport_entry* data = (struct vtss_transport_entry*)ring_buffer_event_data(event);

                    trnd->seqcpu[cpu] = data->seqnum;
                    if ((1 + trnd->seqcpu[cpu] - trnd->seqdone) > VTSS_MERGE_MEM_LIMIT/4 &&
                        !trnd->is_abort && !atomic_read(&trnd->is_complete))
                    {
                        break; /* will not read this event */
                    }
#ifdef VTSS_AUTOCONF_RING_BUFFER_LOST_EVENTS
                    event = ring_buffer_consume(trnd->buffer, cpu, &ts, NULL);
#else
                    event = ring_buffer_consume(trnd->buffer, cpu, &ts);
#endif
                    if (event != NULL) {
                        len = vtss_transport_temp_parse_event(trnd, event, buf, size, cpu);
                        size -= len;
                        buf += len;
                        rc += len;
                    }
                } /* for */
            }
            ring_buffer_free_read_page(trnd->buffer, bpage);
#ifndef VTSS_NO_MERGE
            /* Flush buffers if possible */
            len = vtss_transport_temp_flush(trnd, buf, size);
            size -= len;
            buf += len;
            rc += len;
#endif
            if (size < VTSS_RING_BUFFER_PAGE_SIZE) {
                preempt_enable_no_resched();
                TRACE("'%s' read %zd bytes [%d]...", trnd->name, rc, i);
                return rc;
            }
        } /* for each online cpu */
    } /* while have something to output */
    preempt_enable_no_resched();
    if (rc == 0 && !trnd->is_abort && !atomic_read(&trnd->is_complete)) { /* !!! something wrong !!! */
        ERROR("'%s' [%d] rb=%lu :: %u (%lu bytes) evtstore=%lu of %d", trnd->name, i,
                ring_buffer_entries(trnd->buffer), atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE,
                trnd->seqdone-1, atomic_read(&trnd->seqnum));
        if (!ring_buffer_empty(trnd->buffer)) {
            for_each_online_cpu(cpu) {
                unsigned long count = ring_buffer_entries_cpu(trnd->buffer, cpu);
                if (count)
                    ERROR("'%s' evtcount[%03d]=%lu", trnd->name, cpu, count);
            }
        }
        /* We cannot return 0 if transport is not complete, so write the magic */
        *((unsigned int*)buf) = UEC_MAGIC;
        buf += sizeof(unsigned int);
        *((unsigned int*)buf) = UEC_MAGICVALUE;
        buf += sizeof(unsigned int);
        size -= 2*sizeof(unsigned int);
        rc += 2*sizeof(unsigned int);
    }
    atomic_set(&trnd->is_overflow, 0);
#endif /* VTSS_USE_UEC */
    TRACE("'%s' read %zd bytes [%d]", trnd->name, rc, i);
    return rc;
}

static ssize_t vtss_transport_write(struct file *file, const char __user * buf, size_t count, loff_t * ppos)
{
    /* the transport is read only */
    return -EINVAL;
}

static unsigned int vtss_transport_poll(struct file *file, poll_table* poll_table)
{
    unsigned int rc = 0;
    struct vtss_transport_data* trnd = (struct vtss_transport_data*)file->private_data;
    
    if (trnd == NULL)
        return (POLLERR | POLLNVAL);
    poll_wait(file, &trnd->waitq, poll_table);
    if (atomic_read(&trnd->is_complete) || VTSS_TRANSPORT_DATA_READY(trnd))
        rc = (POLLIN | POLLRDNORM);
    else
        atomic_set(&trnd->is_overflow, 0);
    TRACE("%s: file=0x%p, trnd=0x%p", (rc ? "READY" : "-----"), file, trnd);
    return rc;
}

static int vtss_transport_open(struct inode *inode, struct file *file)
{
    int rc;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct vtss_transport_data *trnd = (struct vtss_transport_data *)PDE(inode)->data;
#else
    struct vtss_transport_data *trnd = (struct vtss_transport_data *)PDE_DATA(inode);
#endif
    TRACE("inode=0x%p, file=0x%p, trnd=0x%p", inode, file, trnd);
    if (trnd == NULL)
        return -ENOENT;

    rc = generic_file_open(inode, file);
    if (rc)
        return rc;

    if (atomic_read(&trnd->is_complete) && VTSS_TRANSPORT_IS_EMPTY(trnd)) {
        return -EINVAL;
    }
    if (atomic_inc_return(&trnd->is_attached) > 1) {
        atomic_dec(&trnd->is_attached);
        return -EBUSY;
    }
    trnd->file = file;
    file->private_data = trnd;
    /* Increase the priority for trace reader to avoid lost events */
    set_user_nice(current, -19);
    return rc;
}

static int vtss_transport_close(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    struct vtss_transport_data *trnd = (struct vtss_transport_data*)PDE(inode)->data;
#else
    struct vtss_transport_data *trnd = (struct vtss_transport_data*)PDE_DATA(inode);
#endif
    file->private_data = NULL;
    /* Restore default priority for trace reader */
    set_user_nice(current, 0);
    TRACE("inode=0x%p, file=0x%p, trnd=0x%p", inode, file, trnd);
    if (trnd == NULL)
        return -ENOENT;

    trnd->file = NULL;
    if (!atomic_dec_and_test(&trnd->is_attached)) {
        ERROR("'%s' wrong state", trnd->name);
        atomic_set(&trnd->is_attached, 0);
        return -EFAULT;
    }
    return 0;
}

static struct file_operations vtss_transport_fops = {
    .owner   = THIS_MODULE,
    .read    = vtss_transport_read,
    .write   = vtss_transport_write,
    .open    = vtss_transport_open,
    .release = vtss_transport_close,
    .poll    = vtss_transport_poll,
};

static void vtss_transport_remove(struct vtss_transport_data* trnd)
{
    struct proc_dir_entry *procfs_root = vtss_procfs_get_root();

    if (procfs_root != NULL) {
        remove_proc_entry(trnd->name, procfs_root);
    }
}

struct vtss_transport_data* vtss_transport_create_trnd(void)
{
    unsigned long rb_size = (num_present_cpus() > 32) ? 32 : 64;
    struct vtss_transport_data* trnd = (struct vtss_transport_data*)kmalloc(sizeof(struct vtss_transport_data), GFP_KERNEL);
    if (trnd == NULL) {
        ERROR("Not enough memory for transport data");
        return NULL;
    }
    memset(trnd, 0, sizeof(struct vtss_transport_data));
    init_waitqueue_head(&trnd->waitq);
    atomic_set(&trnd->refcount,    1);
    atomic_set(&trnd->loscount,    0);
    atomic_set(&trnd->is_attached, 0);
    atomic_set(&trnd->is_complete, 0);
    atomic_set(&trnd->is_overflow, 0);
    trnd->file = NULL;
    trnd->type = VTSS_TR_REG;
#ifdef VTSS_USE_UEC
    trnd->uec = (uec_t*)kmalloc(sizeof(uec_t), GFP_KERNEL);
    if (trnd->uec != NULL) {
        int rc = -1;
        size_t size = UEC_BUFSIZE;
        trnd->uec->callback = vtss_transport_callback;
        trnd->uec->context  = (void*)trnd;
        while (size >= UEC_BUFSIZE_MIN && (rc = init_uec(trnd->uec, size, NULL, 0)))
            size >>= 1;
        if (rc) {
            ERROR("Unable to init UEC");
            kfree(trnd->uec);
            kfree(trnd);
            return NULL;
        }
        TRACE("Use %zu bytes for UEC", size);
    } else {
        ERROR("Could not create UEC");
        kfree(trnd);
        return NULL;
    }
#else
    trnd->is_abort = 0;
    trnd->seqdone  = 1;
    trnd->head     = NULL;
    atomic_set(&trnd->seqnum, 0);
    trnd->buffer = ring_buffer_alloc(rb_size*PAGE_SIZE, 0);
//    printk("allocated %lu bytes for transport buffer, PAGE_SIZE=%lx, buffer_size = %lx\n", (unsigned long)rb_size*PAGE_SIZE, (unsigned long)PAGE_SIZE, ring_buffer_size(trnd->buffer));
    if (trnd->buffer == NULL) {
        ERROR("Unable to allocate %d * %lu bytes for transport buffer", num_present_cpus(), (unsigned long)rb_size*PAGE_SIZE);
        kfree(trnd);
        return NULL;
    }
#endif
    return trnd;
}

static void vtss_transport_destroy_trnd(struct vtss_transport_data* trnd)
{
#ifdef VTSS_USE_UEC
        destroy_uec(trnd->uec);
        kfree(trnd->uec);
#else
        printk("buffer deallocated %d \n", num_present_cpus());
        ring_buffer_free(trnd->buffer);
#endif
        kfree(trnd);
}

static void vtss_transport_create_trnd_name(struct vtss_transport_data* trnd, pid_t ppid, pid_t pid, uid_t cuid, gid_t cgid)
{
    int seq = -1;
    struct path path;
    char buf[MODULE_NAME_LEN + sizeof(trnd->name) + 8 /* strlen("/proc/<MODULE_NAME>/%d-%d.%d") */];

    do { /* Find out free name */
        if (++seq > 0) path_put(&path);
        snprintf(trnd->name, sizeof(trnd->name)-1, "%d-%d.%d", ppid, pid, seq);
        snprintf(buf, sizeof(buf)-1, "%s/%s", vtss_procfs_path(), trnd->name);
        TRACE("lookup '%s'", buf);
    } while (!kern_path(buf, 0, &path));
    /* Doesn't exist, so create it */
    return;

}

int vtss_transport_create_pde (struct vtss_transport_data* trnd, uid_t cuid, gid_t cgid)
{
    unsigned long flags;
    struct proc_dir_entry* pde;
    struct proc_dir_entry* procfs_root = vtss_procfs_get_root();

    if (procfs_root == NULL) {
        ERROR("Unable to get PROCFS root");
        return 1;
    }
//    vtss_transport_create_pde(trnd)
    pde = proc_create_data(trnd->name, (mode_t)(mode ? (mode & 0444) : 0440), procfs_root, &vtss_transport_fops, trnd);

    if (pde == NULL) {
        ERROR("Could not create '%s/%s'", vtss_procfs_path(), trnd->name);
        vtss_transport_destroy_trnd(trnd);
        return 1;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#ifdef VTSS_AUTOCONF_PROCFS_OWNER
    pde->owner = THIS_MODULE;
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    pde->uid = cuid ? cuid : uid;
    pde->gid = cgid ? cgid : gid;
#else
#if defined CONFIG_UIDGID_STRICT_TYPE_CHECKS || (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
{
    kuid_t kuid = KUIDT_INIT(cuid ? cuid : uid);
    kgid_t kgid = KGIDT_INIT(cgid ? cgid : gid);
    proc_set_user(pde, kuid, kgid);
}
#else
    proc_set_user(pde, cuid ? cuid : uid, cgid ? cgid : gid);
#endif
#endif
    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    list_add_tail(&trnd->list, &vtss_transport_list);
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
    TRACE("trnd=0x%p => '%s' done", trnd, trnd->name);
    return 0;
}
struct vtss_transport_data* vtss_transport_create(pid_t ppid, pid_t pid, uid_t cuid, gid_t cgid)
{
    struct vtss_transport_data* trnd = vtss_transport_create_trnd();

    if (trnd == NULL) {
        ERROR("Not enough memory for transport data");
        return NULL;
    }
    vtss_transport_create_trnd_name(trnd, ppid, pid, cuid, cgid);
    /* Doesn't exist, so create it */
    if (vtss_transport_create_pde(trnd, cuid, cgid)){
        ERROR("Could not create '%s/%s'", vtss_procfs_path(), trnd->name);
        vtss_transport_destroy_trnd(trnd);
        return NULL;
    }
    return trnd;
}
struct vtss_transport_data* vtss_transport_create_aux(struct vtss_transport_data* main_trnd, uid_t cuid, gid_t cgid)
{
    char* main_trnd_name = main_trnd->name;
    struct vtss_transport_data* trnd = vtss_transport_create_trnd();

    if (trnd == NULL) {
        ERROR("Not enough memory for transport data");
        return NULL;
    }
    memcpy((void*)trnd->name, (void*)main_trnd_name, strlen(main_trnd_name));
    memcpy((void*)trnd->name+strlen(main_trnd_name),(void*)".aux", 5);
    trnd->type = VTSS_TR_CFG;
    /* Doesn't exist, so create it */
    if (vtss_transport_create_pde(trnd, cuid, cgid)){
        ERROR("Could not create '%s/%s'", vtss_procfs_path(), trnd->name);
        vtss_transport_destroy_trnd(trnd);
        return NULL;
    }
    return trnd;
}

int vtss_transport_complete(struct vtss_transport_data* trnd)
{
    if (trnd == NULL)
        return -ENOENT;
    if (atomic_read(&trnd->refcount)) {
        ERROR("'%s' refcount=%d != 0", trnd->name, atomic_read(&trnd->refcount));
    }
    if (waitqueue_active(&trnd->waitq)) {
        wake_up_interruptible(&trnd->waitq);
    }
    atomic_inc(&trnd->is_complete);
    return 0;
}

#ifdef VTSS_TRANSPORT_TIMER_INTERVAL
static void vtss_transport_tick(unsigned long val)
{
    unsigned long flags;
    struct list_head *p;
    struct vtss_transport_data *trnd = NULL;

    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    list_for_each(p, &vtss_transport_list) {
        trnd = list_entry(p, struct vtss_transport_data, list);
        if (trnd == NULL){
             ERROR("tick: trnd in list is NULL");
             continue;
        }
        if (atomic_read(&trnd->is_attached)) {
            if (waitqueue_active(&trnd->waitq)) {
                TRACE("trnd=0x%p => '%s'", trnd, trnd->name);
                wake_up_interruptible(&trnd->waitq);
            }
        }
    }
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
    mod_timer(&vtss_transport_timer, jiffies + VTSS_TRANSPORT_TIMER_INTERVAL);
}
#endif /* VTSS_TRANSPORT_TIMER_INTERVAL */

int vtss_transport_debug_info(struct seq_file *s)
{
    int cpu;
    unsigned long flags;
    struct list_head *p;
    struct vtss_transport_data *trnd = NULL;

    seq_printf(s, "\n[transport]\nnbuffers=%u (%lu bytes)\n", atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE);
    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    list_for_each(p, &vtss_transport_list) {
        trnd = list_entry(p, struct vtss_transport_data, list);
        seq_printf(s, "\n[proc %s]\nis_attached=%s\nis_complete=%s\nis_overflow=%s\nrefcount=%d\nloscount=%d\nevtcount=%lu\n",
                    trnd->name,
                    atomic_read(&trnd->is_attached) ? "true" : "false",
                    atomic_read(&trnd->is_complete) ? "true" : "false",
                    atomic_read(&trnd->is_overflow) ? "true" : "false",
                    atomic_read(&trnd->refcount),
                    atomic_read(&trnd->loscount),
#ifdef VTSS_USE_UEC
                    0UL);
#else
                    ring_buffer_entries(trnd->buffer));
        if (!ring_buffer_empty(trnd->buffer)) {
            for_each_online_cpu(cpu) {
                unsigned long count = ring_buffer_entries_cpu(trnd->buffer, cpu);
                if (count)
                    seq_printf(s, "evtcount[%03d]=%lu\n", cpu, count);
            }
        }
        seq_printf(s, "evtstore=%lu of %d\n", trnd->seqdone-1, atomic_read(&trnd->seqnum));
        seq_printf(s, "is_abort=%s\n", trnd->is_abort ? "true" : "false");
#endif /* VTSS_USE_UEC */
    }
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
    return 0;
}

int vtss_transport_init(void)
{
    unsigned long flags;

    atomic_set(&vtss_transport_npages, 0);
    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    INIT_LIST_HEAD(&vtss_transport_list);
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
#ifdef VTSS_TRANSPORT_TIMER_INTERVAL
    init_timer(&vtss_transport_timer);
    vtss_transport_timer.expires  = jiffies + VTSS_TRANSPORT_TIMER_INTERVAL;
    vtss_transport_timer.function = vtss_transport_tick;
    vtss_transport_timer.data     = 0;
    add_timer(&vtss_transport_timer);
#endif
    return 0;
}

void vtss_transport_fini(void)
{
    int wait_count = VTSS_TRANSPORT_COMPLETE_TIMEOUT;
    unsigned long flags, count;
    struct list_head* p = NULL;
    struct list_head* tmp = NULL;
    struct vtss_transport_data *trnd = NULL;

#ifdef VTSS_TRANSPORT_TIMER_INTERVAL
    del_timer_sync(&vtss_transport_timer);
#endif

again:
    spin_lock_irqsave(&vtss_transport_list_lock, flags);
    list_for_each_safe(p, tmp, &vtss_transport_list) {
        trnd = list_entry(p, struct vtss_transport_data, list);
        touch_nmi_watchdog();
        if (trnd == NULL){
             ERROR("fini: trnd in list is NULL");
             continue;
        }
        TRACE("trnd=0x%p => '%s'", trnd, trnd->name);
        atomic_inc(&trnd->is_complete);
        if (atomic_read(&trnd->is_attached)) {
            if (waitqueue_active(&trnd->waitq)) {
                wake_up_interruptible(&trnd->waitq);
            }
            if (--wait_count > 0) {
                spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
//                INFO("awaiting transport finish!");
                msleep_interruptible(100);
                goto again;
            }
            ERROR("'%s' complete timeout", trnd->name);
            if (trnd->file)
                trnd->file->private_data = NULL;
            trnd->file = NULL;
        }
        list_del(p);
        vtss_transport_remove(trnd);
        if (atomic_read(&trnd->loscount)) {
            ERROR("'%s' lost %d events", trnd->name, atomic_read(&trnd->loscount));
//            ERROR("'%s' lost %d events", trnd->name, atomic_read(&trnd->loscount));
        }
#ifdef VTSS_USE_UEC
        destroy_uec(trnd->uec);
        kfree(trnd->uec);
#else
        count = atomic_read(&trnd->seqnum);
        if ((trnd->seqdone - 1) != count) {
            ERROR("'%s' drop %lu events", trnd->name, (count - trnd->seqdone - 1));
        }
        vtss_transport_temp_free_all(trnd, &(trnd->head));
        ring_buffer_free(trnd->buffer);
#endif
        kfree(trnd);
        wait_count = VTSS_TRANSPORT_COMPLETE_TIMEOUT;
    }
    INIT_LIST_HEAD(&vtss_transport_list);
    spin_unlock_irqrestore(&vtss_transport_list_lock, flags);
    if (atomic_read(&vtss_transport_npages)) {
        ERROR("lost %u (%lu bytes) buffers", atomic_read(&vtss_transport_npages), atomic_read(&vtss_transport_npages)*PAGE_SIZE);
    }
    atomic_set(&vtss_transport_npages, 0);
}
