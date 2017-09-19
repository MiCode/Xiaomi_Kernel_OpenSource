/* Copyright (c) 2013-2014, 2017 The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_CORESIGHT_CTI_H
#define _LINUX_CORESIGHT_CTI_H

#include <linux/list.h>

struct coresight_cti_data {
	int nr_ctis;
	const char **names;
};

struct coresight_cti {
	const char *name;
	struct list_head link;
};

#ifdef CONFIG_CORESIGHT_CTI
extern struct coresight_cti *coresight_cti_get(const char *name);
extern void coresight_cti_put(struct coresight_cti *cti);
extern int coresight_cti_map_trigin(
			struct coresight_cti *cti, int trig, int ch);
extern int coresight_cti_map_trigout(
			struct coresight_cti *cti, int trig, int ch);
extern void coresight_cti_unmap_trigin(
			struct coresight_cti *cti, int trig, int ch);
extern void coresight_cti_unmap_trigout(
			struct coresight_cti *cti, int trig, int ch);
extern void coresight_cti_reset(struct coresight_cti *cti);
extern int coresight_cti_set_trig(struct coresight_cti *cti, int ch);
extern void coresight_cti_clear_trig(struct coresight_cti *cti, int ch);
extern int coresight_cti_pulse_trig(struct coresight_cti *cti, int ch);
extern int coresight_cti_enable_gate(struct coresight_cti *cti, int ch);
extern void coresight_cti_disable_gate(struct coresight_cti *cti, int ch);
extern void coresight_cti_ctx_save(void);
extern void coresight_cti_ctx_restore(void);
extern int coresight_cti_ack_trig(struct coresight_cti *cti, int trig);
#else
static inline struct coresight_cti *coresight_cti_get(const char *name)
{
	return NULL;
}
static inline void coresight_cti_put(struct coresight_cti *cti) {}
static inline int coresight_cti_map_trigin(
			struct coresight_cti *cti, int trig, int ch)
{
	return -ENODEV;
}
static inline int coresight_cti_map_trigout(
			struct coresight_cti *cti, int trig, int ch)
{
	return -ENODEV;
}
static inline void coresight_cti_unmap_trigin(
			struct coresight_cti *cti, int trig, int ch) {}
static inline void coresight_cti_unmap_trigout(
			struct coresight_cti *cti, int trig, int ch) {}
static inline void coresight_cti_reset(struct coresight_cti *cti) {}
static inline int coresight_cti_set_trig(struct coresight_cti *cti, int ch)
{
	return -ENODEV;
}
static inline void coresight_cti_clear_trig(struct coresight_cti *cti, int ch)
{}
static inline int coresight_cti_pulse_trig(struct coresight_cti *cti, int ch)
{
	return -ENODEV;
}
static inline int coresight_cti_enable_gate(struct coresight_cti *cti, int ch)
{
	return -ENODEV;
}
static inline void coresight_cti_disable_gate(struct coresight_cti *cti, int ch)
{}
static inline void coresight_cti_ctx_save(void){}
static inline void coresight_cti_ctx_restore(void){}
static inline int coresight_cti_ack_trig(struct coresight_cti *cti, int trig)
{
	return -ENODEV;
}
#endif

#endif
