/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#define pr_fmt(fmt) "seemp: %s: " fmt, __func__

#include "seemp_logk.h"
#include "seemp_ringbuf.h"
#include "seemp_event_encoder.h"

/*initial function no need to hold ring_lock*/
int ringbuf_init(struct seemp_logk_dev *sdev)
{
	char *buf;
	unsigned long virt_addr;

	if (kmalloc_flag) {
		sdev->ring = kmalloc(sdev->ring_sz, GFP_KERNEL);
		if (sdev->ring == NULL) {
			pr_err("kmalloc failed, ring_sz= %d\n", sdev->ring_sz);
			return -ENOMEM;
		}

		buf = (char *)sdev->ring;

		/*reserve kmalloc memory as pages to make them remapable*/
		for (virt_addr = (unsigned long)buf;
				virt_addr < (unsigned long)buf + sdev->ring_sz;
				virt_addr += PAGE_SIZE) {
				SetPageReserved(virt_to_page((virt_addr)));
		}
	} else {
		sdev->ring = vmalloc(sdev->ring_sz);
		if (sdev->ring == NULL) {
			pr_err("vmalloc failed, ring_sz = %d\n", sdev->ring_sz);
			return -ENOMEM;
		}
		buf = (char *)sdev->ring;

		/*reserve vmalloc memory as pages to make them remapable*/
		for (virt_addr = (unsigned long)buf;
				virt_addr < (unsigned long)buf + sdev->ring_sz;
				virt_addr += PAGE_SIZE) {
			SetPageReserved(vmalloc_to_page(
				(unsigned long *) virt_addr));
		}
	}

	memset(sdev->ring, 0, sdev->ring_sz);

	sdev->num_tot_blks = (sdev->ring_sz / BLK_SIZE);
	sdev->num_writers = 0;
	sdev->write_idx = 0;
	sdev->read_idx = 0;

	sdev->num_write_avail_blks = sdev->num_tot_blks;
	/*no. of blocks available for write*/
	sdev->num_write_in_prog_blks = 0;
	/*no. of blocks held by writers to perform writes*/

	sdev->num_read_avail_blks = 0;
	/*no. of blocks ready for read*/
	sdev->num_read_in_prog_blks = 0;
	/*no. of blocks held by the reader to perform read*/

	return 0;
}

void ringbuf_cleanup(struct seemp_logk_dev *sdev)
{
	unsigned long virt_addr;

	if (kmalloc_flag) {
		for (virt_addr = (unsigned long)sdev->ring;
			virt_addr < (unsigned long)sdev->ring + sdev->ring_sz;
			virt_addr += PAGE_SIZE) {
			/*clear all pages*/
			ClearPageReserved(virt_to_page((unsigned long *)
				virt_addr));
		}
		kfree(sdev->ring);
	} else {
		for (virt_addr = (unsigned long)sdev->ring;
			virt_addr < (unsigned long)sdev->ring + sdev->ring_sz;
			virt_addr += PAGE_SIZE) {
			/*clear all pages*/
			ClearPageReserved(vmalloc_to_page((unsigned long *)
				virt_addr));
		}
		vfree(sdev->ring);
	}
}

struct seemp_logk_blk *ringbuf_fetch_wr_block
					(struct seemp_logk_dev *sdev)
{
	struct seemp_logk_blk *blk = NULL;
	int idx;

	mutex_lock(&sdev->lock);
	if (sdev->num_write_avail_blks == 0) {
		idx = -1;
		mutex_unlock(&sdev->lock);
		return blk;
	}

	idx = sdev->write_idx;
	sdev->write_idx = (sdev->write_idx + 1) % sdev->num_tot_blks;
	sdev->num_write_avail_blks--;
	sdev->num_write_in_prog_blks++;
	sdev->num_writers++;

	blk = &sdev->ring[idx];
	blk->status = 0x0;

	mutex_unlock(&sdev->lock);
	return blk;
}

void ringbuf_finish_writer(struct seemp_logk_dev *sdev,
				struct seemp_logk_blk *blk)
{
	/* Encode seemp parameters in multi-threaded mode (before mutex lock) */
	encode_seemp_params(blk);

	/*
	 * finish writing...
	 * the calling process will no longer access this block.
	 */
	mutex_lock(&sdev->lock);

	sdev->num_writers--;
	sdev->num_write_in_prog_blks--;
	sdev->num_read_avail_blks++;

	/*wake up any readers*/
	if (sdev->num_writers == 0)
		wake_up_interruptible(&sdev->readers_wq);

	mutex_unlock(&sdev->lock);
}

int ringbuf_count_marked(struct seemp_logk_dev *sdev)
{
	int i;
	unsigned int marked;

	mutex_lock(&sdev->lock);
	for (marked = 0, i = 0; i < sdev->num_tot_blks; i++)
		if (sdev->ring[i].status & 0x1)
			marked++;
	mutex_unlock(&sdev->lock);

	return marked;
}
