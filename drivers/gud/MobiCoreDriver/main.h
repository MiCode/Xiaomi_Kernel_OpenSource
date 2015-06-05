/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_MAIN_H_
#define _MC_MAIN_H_

#include <linux/slab.h>		/* gfp_t */

#define MC_VERSION(major, minor) \
		(((major & 0x0000ffff) << 16) | (minor & 0x0000ffff))
#define MC_VERSION_MAJOR(x) ((x) >> 16)
#define MC_VERSION_MINOR(x) ((x) & 0xffff)

/* MobiCore Driver Kernel Module context data. */
struct mc_device_ctx {
	struct device		*mcd;
	/* debugfs root */
	struct dentry		*debug_dir;

	/* GP sessions waiting final close notif */
	struct list_head	closing_sess;
	struct mutex		closing_lock; /* Closing sessions list */

	/* Features */
	/* - SWd can set a time out to get scheduled at a future time */
	bool			f_timeout;
	/* - SWd supports memory extension which allows for bigger TAs */
	bool			f_mem_ext;
	/* - SWd supports TA authorisation */
	bool			f_ta_auth;
	/* - SWd can map several buffers at once */
	bool			f_multimap;
	/* - SWd supports GP client authentication */
	bool			f_client_login;
};

extern struct mc_device_ctx g_ctx;

struct kasnprintf_buf {
	gfp_t gfp;
	void *buf;
	int size;
	int off;
};

extern __printf(2, 3)
int kasnprintf(struct kasnprintf_buf *buf, const char *fmt, ...);

#endif /* _MC_MAIN_H_ */
