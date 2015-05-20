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
#ifndef _UEC_H_
#define _UEC_H_

#include <linux/spinlock.h>
typedef spinlock_t sal_spinlock_t;
#define uec_lock(x)   spin_lock_irqsave((x), flags)
#define uec_unlock(x) spin_unlock_irqrestore((x), flags)

/**
//
// Universal Event Collector types and declarations
//
*/
typedef struct _uec_t
{
    /// methods
    int  (*put_record) (struct _uec_t* uec, void *part0, size_t size0, void *part1, size_t size1, int mode);
    int  (*init)       (struct _uec_t* uec, size_t size, char *name, int instance);
    void (*destroy)    (struct _uec_t* uec);
    int  (*pull)       (struct _uec_t* uec, char __user* buffer, size_t len);    /// returns the number of bytes copied
    void (*callback)   (struct _uec_t* uec, int reason, void *context);

    /// elements
    char *buffer;               /// collector buffer
    volatile char *head;        /// head-pointer to write to
    volatile char *tail;        /// tail-pointer to read from
    volatile char *head_;       /// head-pointer for asynchronous writes
    volatile char *tail_;       /// tail-pointer for asynchronous reads
    size_t tsize;               /// size of buffer for read operations
    size_t hsize;               /// size of buffer for write operations
    volatile int ovfl;          /// indicates buffer overflow
    volatile char *last_rec_ptr;/// points to the last successfully written record
    volatile int spill_active;  /// set when spill-method is invoked by put_record-method
    volatile int writer_count;  /// the number of active writers
    volatile int reader_count;  /// the number of active readers
    void *context;              /// callback context

    sal_spinlock_t lock;        /// spin lock protection
} uec_t;

int  init_uec(uec_t* uec, size_t size, char *name, int instance);
void destroy_uec(uec_t* uec);
int  put_record_async(uec_t* uec, void *part0, size_t size0, void *part1, size_t size1, int mode);
int  pull_uec(uec_t* uec, char __user* buffer, size_t len);

/// internal buffer size
#define UEC_BUFSIZE     (PAGE_SIZE<<10) /* ^10 = 4194304 (0x400000) */
#define UEC_BUFSIZE_MIN (PAGE_SIZE<<4)

/// maximum record size
#define MAX_RECORD_SIZE 0x10000

/// put_record modes
#define UECMODE_NORMAL 0
#define UECMODE_SAFE   1

/// UEC asynchronous thread commands
#define UECCOM_SPILL       0x00
#define UECCOM_REOPEN      0x01
#define UECCOM_TERMINATE   0x02

/// UEC asynchronous thread stati
#define UECSTS_OK    0x10
#define UECSTS_BUSY  0x20
#define UECSTS_FAIL  0x30

/// UEC callback reasons
#define UECCB_NEWTRACE  0
#define UECCB_OVERFLOW  1

/// UEC fill/spill border (numerator/denominator)
#define UECNUMER 3
#define UECDENOM 4

#endif /* _UEC_H_ */
