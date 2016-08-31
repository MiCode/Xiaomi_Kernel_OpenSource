/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SYSEDP_INTERNAL_H
#define _SYSEDP_INTERNAL_H

#include <linux/mutex.h>
#include <linux/sysedp.h>

extern struct mutex sysedp_lock;
extern struct dentry *edp_debugfs_dir;
extern struct dentry *sysedp_debugfs_dir;
extern int margin;
extern unsigned int avail_budget;
extern unsigned int consumer_sum;
extern struct list_head registered_consumers;
extern struct mutex sysedp_lock;

static inline unsigned int _cur_level(struct sysedp_consumer *c)
{
	return c->states[c->state];
}

static inline unsigned int _cur_oclevel(struct sysedp_consumer *c)
{
	return c->ocpeaks ? c->ocpeaks[c->state] : c->states[c->state];
}

void sysedp_set_avail_budget(unsigned int);
void sysedp_set_dynamic_cap(unsigned int, unsigned int);
struct sysedp_consumer *sysedp_get_consumer(const char *);

int sysedp_init_sysfs(void);
void sysedp_init_debugfs(void);

void _sysedp_refresh(void);
int sysedp_consumer_add_kobject(struct sysedp_consumer *);
void sysedp_consumer_remove_kobject(struct sysedp_consumer *);

#endif
