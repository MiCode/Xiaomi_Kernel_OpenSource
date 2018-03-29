/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef _MTK_VCODEC_INTR_H_
#define _MTK_VCODEC_INTR_H_

#define MTK_INST_IRQ_RECEIVED		0x1
#define MTK_INST_WORK_THREAD_ABORT_DONE	0x2

/* timeout is ms */
int mtk_vcodec_wait_for_done_ctx(void *data, int command, unsigned int timeout,
				 int interrupt);
int mtk_vcodec_wait_for_done_dev(void *data, int command, unsigned int timeout,
				 int interrupt);

void mtk_vcodec_clean_dev_int_flags(void *data);

#endif /* _MTK_VCODEC_INTR_H_ */
