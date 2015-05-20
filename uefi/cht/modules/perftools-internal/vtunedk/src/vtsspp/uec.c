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
#include "globals.h"
#include "uec.h"

#include <linux/slab.h>
#include <asm/uaccess.h>

/**
// Universal Event Collector: system mode implementation
*/

/// for safety reasons (quick initialization)
static int put_record_stub(uec_t* uec, void *part0, size_t size0, void *part1, size_t size1, int mode)
{
    return 0;
}

int init_uec(uec_t* uec, size_t size, char *name, int instance)
{
    int order;
    /// initialize methods
    uec->put_record = put_record_async;
    uec->init = init_uec;
    uec->destroy = destroy_uec;
    uec->pull = pull_uec;

    if (size == 0) { /// change name request
        ERROR("UEC size is 0");
        return VTSS_ERR_INTERNAL;
    }
    order = get_order(size);
    if (!(uec->buffer = (char*)__get_free_pages((GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN), order))) {
        return VTSS_ERR_NOMEMORY;
    }
    uec->last_rec_ptr = uec->head = uec->head_ = uec->tail = uec->tail_ = uec->buffer;
    uec->hsize = uec->tsize = (PAGE_SIZE << order);
    uec->ovfl = 0;
    uec->spill_active = 0;
    uec->writer_count = 0;
    uec->reader_count = 0;

    spin_lock_init(&uec->lock);
    /// notify on the creation of a new trace
//    uec->callback(uec, UECCB_NEWTRACE, uec->context);
    return 0;
}

void destroy_uec(uec_t* uec)
{
    uec->put_record = put_record_stub;

    if (uec->buffer) {
        free_pages((unsigned long)uec->buffer, get_order(uec->hsize));
        uec->buffer = NULL;
    }
}

#define safe_memcpy(dst, src, size) memcpy(dst, src, size)
#define spill_uec() /* empty */

int put_record_async(uec_t* uec, void *part0, size_t size0, void *part1, size_t size1, int mode)
{
    size_t tsize;                  /// total record size
    size_t fsize = 0;              /// free area length
    size_t psize;                  /// partial size
    size_t tmp;
    char *last_rec_ptr;
    char *head = 0;
    char *tail = 0;
    size_t hsize = 0;
    unsigned long flags;

    if (!uec->buffer || !part0 || !size0 || ((!size1) ^ (!part1))) {
        return VTSS_ERR_BADARG;
    }
    tsize = size0 + size1;

    /// lock UEC
    uec_lock(&uec->lock);

    /// sample the uec variables
    head = (char*)uec->head_;
    tail = (char*)uec->tail;
    last_rec_ptr = head;
    hsize = uec->hsize;

    /// is buffer full?
    if (uec->ovfl) {
        *((unsigned int*)uec->last_rec_ptr) |= UEC_OVERFLOW;
        /// signal overflow to overlying components
//        uec->callback(uec, UECCB_OVERFLOW, uec->context);
        uec_unlock(&uec->lock);
        spill_uec();
        return VTSS_ERR_BUFFERFULL;
    }
    /// compute free size
    if (tail <= head) {
        fsize = uec->hsize - (size_t)(head - tail);
    } else {
        fsize = (size_t)(tail - head);
    }
    /// handle 'no room' case
    if (fsize < tsize) {
        uec->ovfl = 1;
        *((unsigned int*)uec->last_rec_ptr) |= UEC_OVERFLOW;
        /// signal overflow to overlying components
//        uec->callback(uec, UECCB_OVERFLOW, uec->context);
        uec_unlock(&uec->lock);
        spill_uec();
        return VTSS_ERR_NOMEMORY;
    }
    /// allocate uec region
    psize = (size_t)(uec->buffer + hsize - head);

    if (psize > tsize) {
        uec->head_ = head + tsize;
    } else {
        uec->head_ = uec->buffer + tsize - psize;
    }
    if (uec->head_ == tail) {
        uec->ovfl = 1;
    }
    /// increment the writers' count
    uec->writer_count++;

    /// unlock UEC
    uec_unlock(&uec->lock);

    /// do the write to the allocated uec region

    if (tail <= head) {
        if (psize > tsize) {
            memcpy(head, part0, size0);
            head += size0;
            if (size1) {
                safe_memcpy(head, part1, size1);
                head += size1;
            }
        } else {
            tmp = size0 > psize ? psize : size0;
            memcpy(head, part0, tmp);
            head += tmp;

            if (tmp == psize) {
                head = uec->buffer;
                if ((size0 - tmp) > 0) {
                    memcpy(head, ((char*)part0) + tmp, size0 - tmp);
                    head += size0 - tmp;
                }
            }
            if (size1) {
                psize -= tmp;
                if (psize) {
                    safe_memcpy(head, part1, psize);
                    head = uec->buffer;
                    if ((size1 - psize) > 0) {
                        memcpy(head, ((char*)part1) + psize, size1 - psize);
                        head += size1 - psize;
                    }
                } else {
                    safe_memcpy(head, part1, size1);
                    head += size1;
                }
            }
        }
    } else /// tail > head
    {
        memcpy(head, part0, size0);
        head += size0;
        if (size1) {
            safe_memcpy(head, part1, size1);
            head += size1;
        }
    }

    /// lock UEC
    uec_lock(&uec->lock);

    /// decrement the writers' count
    uec->writer_count--;

    uec->last_rec_ptr = last_rec_ptr;

    /// update uec variables
    if (!uec->writer_count) {
        uec->head = uec->head_;
    }
    /// unlock UEC
    uec_unlock(&uec->lock);

    spill_uec();

    return 0;
}

int pull_uec(uec_t* uec, char __user* buffer, size_t len)
{
    int rc = 0;
    char *head;
    char *tail;
    size_t size;
    int ovfl;

    size_t copylen = 0;
    size_t partlen;

    unsigned long flags;

    /// sample the UEC state, copy the sampled contents to the specified buffer,
    /// and free the read part of the UEC buffer

    if (!uec->buffer || !buffer || !len) {
        return VTSS_ERR_BADARG;
    }
    /// sample data region
    uec_lock(&uec->lock);

    if (uec->spill_active) {
        uec_unlock(&uec->lock);
        return VTSS_ERR_BUSY;
    }
    uec->spill_active = 1;

    head = (char*)uec->head;
    tail = (char*)uec->tail;
    size = uec->tsize;
    ovfl = uec->ovfl;

    if ((head == tail && !ovfl) || (head == tail && ovfl && head != uec->head_)) {
        uec->spill_active = 0;
        uec_unlock(&uec->lock);
        return 0; ///empty
    }

    uec_unlock(&uec->lock);

    /// spill the sampled region
    if (head > tail) {
        copylen = (size_t)(head - tail);
        copylen = copylen > len ? len : copylen;

        rc = copy_to_user(buffer, (void*)tail, copylen);

        tail += copylen;
    } else if (head < tail || (head == tail && ovfl)) {
        copylen = partlen = (size_t)(size - (tail - uec->buffer));
        copylen = copylen > len ? len : copylen;

        rc = copy_to_user(buffer, (void*)tail, copylen);

        tail += copylen;

        if (copylen == partlen && copylen < len) {
            /// copy the second part
            partlen = (size_t)(head - uec->buffer);
            partlen = partlen > (len - copylen) ? (len - copylen) : partlen;

            rc |= copy_to_user(&((char*)buffer)[copylen], (void*)uec->buffer, partlen);

            copylen += partlen;
            /// assert(copylen <= len);

            tail = uec->buffer + partlen;
        }
    }

    uec_lock(&uec->lock);
    uec->tail = tail;
    uec->ovfl = 0;
    uec->spill_active = 0;
    uec_unlock(&uec->lock);

    return rc ? -1 : copylen;
}
