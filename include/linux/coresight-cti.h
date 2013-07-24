/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#else
static inline struct coresight_cti *coresight_cti_get(const char *name)
{
	return NULL;
}
static inline void coresight_cti_put(struct coresight_cti *cti) {}
static inline int coresight_cti_map_trigin(
			struct coresight_cti *cti, int trig, int ch)
{
	return -ENOSYS;
}
static inline int coresight_cti_map_trigout(
			struct coresight_cti *cti, int trig, int ch)
{
	return -ENOSYS;
}
static inline void coresight_cti_unmap_trigin(
			struct coresight_cti *cti, int trig, int ch) {}
static inline void coresight_cti_unmap_trigout(
			struct coresight_cti *cti, int trig, int ch) {}
static inline void coresight_cti_reset(struct coresight_cti *cti) {}
#endif

#endif
