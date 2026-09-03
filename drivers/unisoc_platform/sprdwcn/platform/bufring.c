/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>

#include "bufring.h"
#include "mdbg_type.h"
#include "wcn_log.h"
#include "../include/wcn_dbg.h"
//#include "rdc_debug.h"

#define MDBG_RING_LOCK_INIT(ring)		mutex_init(ring->plock)
#define MDBG_RING_LOCK_UNINIT(ring)		mutex_destroy(ring->plock)
#define MDBG_RING_LOCK(ring)			mutex_lock(ring->plock)
#define MDBG_RING_UNLOCK(ring)			mutex_unlock(ring->plock)
#define _MDBG_RING_REMAIN(rp, wp, size)	((u_long)(wp) >= (u_long)(rp) ?\
			((size) - (u_long)(wp) + (u_long)(rp)) :\
			((u_long)(rp) - (u_long)(wp)))

/* valid buf for write */
long int mdbg_ring_free_space(struct mdbg_ring_t *ring)
{
	return (long int)_MDBG_RING_REMAIN(ring->rp,
				ring->wp, ring->size);
}

static inline char *mdbg_ring_start(struct mdbg_ring_t *ring)
{
	return ring->pbuff;
}

static inline char *mdbg_ring_end(struct mdbg_ring_t *ring)
{
	return ring->end;
}

bool mdbg_ring_over_loop(struct mdbg_ring_t *ring, u_long len, int rw)
{
	if (rw == MDBG_RING_R) {
		if ((u_long)ring->rp + len > (u_long)mdbg_ring_end(ring)) {
			ring->p_order_flag = 0;
			return true;
		}
		return false;
	}

	if ((u_long)ring->wp + len > (u_long)mdbg_ring_end(ring)) {
		ring->p_order_flag = 1;
		return true;
	}

	return false;
}

struct mdbg_ring_t *mdbg_ring_alloc(unsigned long int size)
{
	struct mdbg_ring_t *ring = NULL;

	do {
		if (size < MDBG_RING_MIN_SIZE) {
			WCN_ERR("size error:%ld\n", size);
			break;
		}
		ring = kmalloc(sizeof(struct mdbg_ring_t), GFP_KERNEL);
		if (ring == NULL) {
			WCN_ERR("Ring malloc Failed.\n");
			break;
		}
		ring->pbuff = kmalloc((unsigned int)size, GFP_KERNEL);
		if (ring->pbuff == NULL) {
			WCN_ERR("Ring buff malloc Failed.\n");
			break;
		}
		ring->plock = kmalloc(MDBG_RING_LOCK_SIZE, GFP_KERNEL);
		if (ring->plock == NULL) {
			WCN_ERR("Ring lock malloc Failed.\n");
			break;
		}
		MDBG_RING_LOCK_INIT(ring);
		memset(ring->pbuff, 0, (unsigned int)size);
		ring->size = size;
		ring->rp = ring->pbuff;
		ring->wp = ring->pbuff;
		ring->end = (char *)(((u_long)ring->pbuff) + (ring->size - 1));
		ring->p_order_flag = 0;

		return ring;
	} while (0);
	mdbg_ring_destroy(ring);

	return NULL;
}

void mdbg_ring_destroy(struct mdbg_ring_t *ring)
{
	if (unlikely(ZERO_OR_NULL_PTR(ring)))
		return;

	WCN_LOG("ring = %p", ring);
	WCN_LOG("ring->pbuff = %p", ring->pbuff);
	if (ring != NULL) {
		if (ring->pbuff != NULL) {
			WCN_LOG("to free ring->pbuff.");
			kfree(ring->pbuff);
			ring->pbuff = NULL;
		}
		if (ring->plock != NULL) {
			WCN_LOG("to free ring->plock.");
			MDBG_RING_LOCK_UNINIT(ring);
			kfree(ring->plock);
			ring->plock = NULL;
		}
		WCN_LOG("to free ring.");
		kfree(ring);
		ring = NULL;
	}
}

int mdbg_ring_read(struct mdbg_ring_t *ring, void *buf, int len)
{
	unsigned int len1, len2 = 0;
	long cont_len = 0;
	unsigned int read_len = 0;
	char *pstart = NULL;
	char *pend = NULL;
	static unsigned int total_len;

	if ((buf == NULL) || (ring == NULL) || (len == 0)) {
		WCN_ERR("Ring Read Failed,Param Error!,buf=%p,ring=%p,len=%d\n",
			buf, ring, len);
		return -MDBG_ERR_BAD_PARAM;
	}
	MDBG_RING_LOCK(ring);
	cont_len = mdbg_ring_readable_len(ring);
	read_len = (unsigned int)(cont_len >= len ? len :
				  (unsigned int)cont_len);
	pstart = mdbg_ring_start(ring);
	pend = mdbg_ring_end(ring);
	WCN_LOG("read_len=%d", read_len);
	WCN_LOG("buf=%p", buf);
	WCN_LOG("pstart=%p", pstart);
	WCN_LOG("pend=%p", pend);
	WCN_LOG("ring->wp = %p", ring->wp);
	WCN_LOG("ring->rp=%p\n", ring->rp);

	if ((read_len == 0) || (cont_len == 0)) {
		WCN_LOG("read_len = 0 OR Ring Empty.");
		MDBG_RING_UNLOCK(ring);
		return 0;	/*ring empty*/
	}

	if (mdbg_ring_over_loop(ring, read_len, MDBG_RING_R)) {
		WCN_LOG("Ring loopover.");
		len1 = pend - ring->rp + 1;
		len2 = read_len - len1;

		/* if ((uintptr_t)buf > TASK_SIZE) */
		if (!access_ok(buf, len)) {
			memcpy(buf, ring->rp, len1);
			memcpy((buf + len1), pstart, len2);
		} else if (copy_to_user((__force void __user *)buf,
					(void *)ring->rp, len1) ||
			   copy_to_user((__force void __user *)(buf + len1),
					(void *)pstart, len2)) {
			WCN_ERR("copy to user error!\n");
			MDBG_RING_UNLOCK(ring);
			return -EFAULT;
		}
		ring->rp = (char *)((u_long)pstart + len2);
	} else {
		/* RP < WP */
		if (ring->p_order_flag == 0) {
			if (((ring->rp + read_len) > ring->wp)
				&& (mdbg_dev->open_count != 0))
				WCN_ERR("read overlay\n");
		}

		/* if ((uintptr_t)buf > TASK_SIZE) */
		if (!access_ok(buf, len))
			memcpy(buf, ring->rp, read_len);
		else if (copy_to_user((__force void __user *)buf,
				      (void *)ring->rp, read_len)) {
			WCN_ERR("copy to user error!\n");
			MDBG_RING_UNLOCK(ring);

			return -EFAULT;
		}
		ring->rp += read_len;
	}
	total_len += read_len;
	wcn_pr_daterate(12, 1, total_len,
			": %s totallen:%u read:%d wp:%p rp:%p",
			__func__, total_len, read_len,
			ring->wp, ring->rp);
	WCN_LOG("<-----[read end] read len =%d.\n", read_len);
	MDBG_RING_UNLOCK(ring);

	return read_len;
}

/*
 * read:	Rp = Wp:	empty
 * write:	Wp+1=Rp:	full
 */
int mdbg_ring_write(struct mdbg_ring_t *ring, void *buf, unsigned int len)
{
#ifdef WCN_RDCDBG
	/*rdc_debug.c*/
	if (ring->is_mem == 1)
		dumpmem_rx_callback(buf, len);
	else
		log_rx_callback(buf, len);

	return len;
#else
	int len1, len2 = 0;
	char *pstart = NULL;
	char *pend = NULL;
	static unsigned int total_len;

	WCN_LOG("-->Ring Write len = %d\n", len);
	if ((ring == NULL) || (buf == NULL) || (len == 0)) {
		WCN_ERR(
			"Ring Write Failed,Param Error!,buf=%p,ring=%p,len=%d\n",
			buf, ring, len);

		return -MDBG_ERR_BAD_PARAM;
	}
	pstart = mdbg_ring_start(ring);
	pend = mdbg_ring_end(ring);
	WCN_LOG("pstart = %p", pstart);
	WCN_LOG("pend = %p", pend);
	WCN_LOG("buf = %p", buf);
	WCN_LOG("len = %d", len);
	WCN_LOG("ring->wp = %p", ring->wp);
	WCN_LOG("ring->rp=%p", ring->rp);

	/*
	 * ring buf valid space < len,need to write (buf_space-1)
	 * and then write len-(buf_space-1)
	 */
	if (((mdbg_ring_free_space(ring) - 1) < len)
		&& (mdbg_dev->open_count != 0)) {
		WCN_ERR("log buf is full, Discard the package=%d\n", len);
		wake_up_log_wait();
		return len;
	}

	/* ring buf valid space > len, you can write freely */
	if (mdbg_ring_over_loop(ring, len, MDBG_RING_W)) {
		WCN_LOG("Ring overloop.");
		len1 = pend - ring->wp + 1;
		len2 = (((int)len - len1) % (int)ring->size);

		/* if ((uintptr_t)buf > TASK_SIZE) */
		if (!access_ok(buf, len)) {
			memcpy(ring->wp, buf, len1);
			memcpy(pstart, (buf + len1), len2);
		} else if (copy_from_user((void *)ring->wp,
					  (__force void __user *)buf, len1) ||
			    copy_from_user((void *)pstart,
				(__force void __user *)(buf + len1), len2)) {
			WCN_ERR("%s copy from user error!\n", __func__);

			return -EFAULT;
		}

		ring->wp = (char *)(pstart + len2);

	} else {
		/* RP > WP */
		/* if ((uintptr_t)buf > TASK_SIZE) */
		if (!access_ok(buf, len))
			memcpy(ring->wp, buf, len);
		else if (copy_from_user((void *)ring->wp,
			    (__force void __user *)buf, len)) {
			WCN_ERR("%s copy from user error!\n", __func__);
			return -EFAULT;
		}
		ring->wp += len;
	}
	total_len += len;
	wcn_pr_daterate(12, 1, total_len,
			": %s totallen:%u write:%u wp:%p rp:%p",
			__func__, total_len, len, ring->wp, ring->rp);
	WCN_LOG("<------end len = %d\n", len);

	return len;
#endif
}

/* @timeout unit is ms */
int mdbg_ring_write_timeout(struct mdbg_ring_t *ring, void *buf,
			    unsigned int len, unsigned int timeout)
{
	unsigned  int cnt = timeout / 20;

	while (cnt > 0 && (mdbg_ring_free_space(ring) - 1) < len) {
		msleep(20);
		if (--cnt == 0)
			WCN_ERR("ringbuf is full, tiemout:%u\n",
				timeout);
	}

	return mdbg_ring_write(ring, buf, len);
}

char *mdbg_ring_write_ext(struct mdbg_ring_t *ring, long int len)
{
	char *wp = NULL;

	WCN_LOG("ring=%p,ring->wp=%p,len=%ld.", ring, ring->wp, len);

	if ((ring == NULL) || (len == 0)) {
		WCN_ERR("Ring Write Ext Failed,Param Error!\n");
		return NULL;
	}

	if (mdbg_ring_over_loop(ring, len, MDBG_RING_R)
			|| mdbg_ring_will_full(ring, len)) {
		WCN_LOG(
		"Ring Write Ext Failed,Ring State Error!,overloop=%d,full=%d.",
		mdbg_ring_over_loop(ring, len, MDBG_RING_R),
		mdbg_ring_will_full(ring, len));
		return NULL;
	}
	MDBG_RING_LOCK(ring);
	wp = ring->wp;
	ring->wp += len;
	WCN_LOG("return wp=%p,ring->wp=%p.", wp, ring->wp);
	MDBG_RING_UNLOCK(ring);

	return wp;
}

inline bool mdbg_ring_will_full(struct mdbg_ring_t *ring, long int len)
{
	return (len > mdbg_ring_free_space(ring));
}

/* remain data for read */
inline long int mdbg_ring_readable_len(struct mdbg_ring_t *ring)
{
	return (ring->size - ((u_long)mdbg_ring_free_space(ring)));
}

inline void mdbg_ring_clear(struct mdbg_ring_t *ring)
{
	ring->rp = ring->wp;
}

inline void mdbg_ring_reset(struct mdbg_ring_t *ring)
{
	ring->wp = ring->pbuff;
	ring->rp = ring->wp;
}

inline void mdbg_ring_print(struct mdbg_ring_t *ring)
{
	WCN_DEBUG("ring buf status:ring->rp=%p,ring->wp=%p.\n",
				ring->rp, ring->wp);
}
