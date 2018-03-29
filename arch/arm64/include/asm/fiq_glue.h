/*
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_FIQ_GLUE_H
#define __ASM_FIQ_GLUE_H

struct fiq_glue_handler {
	struct fiq_glue_handler *next;
	void (*fiq)(struct fiq_glue_handler *h, const struct pt_regs *regs,
		    void *svc_sp);
	void (*resume)(struct fiq_glue_handler *h);
};

int fiq_glue_register_handler(struct fiq_glue_handler *handler);

#endif
