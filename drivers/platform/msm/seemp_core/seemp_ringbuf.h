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

#ifndef __seemp_RINGBUF_H__
#define __seemp_RINGBUF_H__

/*
 * This header exports pingpong's API
 */

int ringbuf_init(struct seemp_logk_dev *sdev);
struct seemp_logk_blk *ringbuf_fetch_wr_block
(struct seemp_logk_dev *sdev);
void ringbuf_finish_writer(struct seemp_logk_dev *sdev);
void ringbuf_cleanup(struct seemp_logk_dev *sdev);
int ringbuf_count_marked(struct seemp_logk_dev *sdev);

#endif
