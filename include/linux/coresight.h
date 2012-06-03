/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_CORESIGHT_H
#define _LINUX_CORESIGHT_H

#include <linux/device.h>

/* Peripheral id registers (0xFD0-0xFEC) */
#define CORESIGHT_PERIPHIDR4	(0xFD0)
#define CORESIGHT_PERIPHIDR5	(0xFD4)
#define CORESIGHT_PERIPHIDR6	(0xFD8)
#define CORESIGHT_PERIPHIDR7	(0xFDC)
#define CORESIGHT_PERIPHIDR0	(0xFE0)
#define CORESIGHT_PERIPHIDR1	(0xFE4)
#define CORESIGHT_PERIPHIDR2	(0xFE8)
#define CORESIGHT_PERIPHIDR3	(0xFEC)
/* Component id registers (0xFF0-0xFFC) */
#define CORESIGHT_COMPIDR0	(0xFF0)
#define CORESIGHT_COMPIDR1	(0xFF4)
#define CORESIGHT_COMPIDR2	(0xFF8)
#define CORESIGHT_COMPIDR3	(0xFFC)

/* DBGv7 with baseline CP14 registers implemented */
#define ARM_DEBUG_ARCH_V7B	(0x3)
/* DBGv7 with all CP14 registers implemented */
#define ARM_DEBUG_ARCH_V7	(0x4)
#define ARM_DEBUG_ARCH_V7_1	(0x5)
#define ETM_ARCH_V3_3		(0x23)
#define PFT_ARCH_V1_1		(0x31)

enum coresight_clk_rate {
	CORESIGHT_CLK_RATE_OFF,
	CORESIGHT_CLK_RATE_TRACE,
	CORESIGHT_CLK_RATE_HSTRACE,
};

enum coresight_dev_type {
	CORESIGHT_DEV_TYPE_SINK,
	CORESIGHT_DEV_TYPE_LINK,
	CORESIGHT_DEV_TYPE_SOURCE,
	CORESIGHT_DEV_TYPE_MAX,
};

struct coresight_connection {
	int child_id;
	int child_port;
	struct coresight_device *child_dev;
	struct list_head link;
};

struct coresight_device {
	int id;
	struct coresight_connection *conns;
	int nr_conns;
	const struct coresight_ops *ops;
	struct device dev;
	struct mutex mutex;
	int *refcnt;
	struct list_head link;
	struct module *owner;
	bool enable;
};

#define to_coresight_device(d) container_of(d, struct coresight_device, dev)

struct coresight_ops {
	int (*enable)(struct coresight_device *csdev, int port);
	void (*disable)(struct coresight_device *csdev, int port);
};

struct coresight_platform_data {
	int id;
	const char *name;
	int nr_ports;
	int *child_ids;
	int *child_ports;
	int nr_children;
};

struct coresight_desc {
	enum coresight_dev_type type;
	const struct coresight_ops *ops;
	struct coresight_platform_data *pdata;
	struct device *dev;
	const struct attribute_group **groups;
	struct module *owner;
};

struct qdss_source {
	struct list_head link;
	const char *name;
	uint32_t fport_mask;
};

struct msm_qdss_platform_data {
	struct qdss_source *src_table;
	size_t size;
	uint8_t afamily;
};


extern struct coresight_device *
coresight_register(struct coresight_desc *desc);
extern void coresight_unregister(struct coresight_device *csdev);
extern int coresight_enable(struct coresight_device *csdev, int port);
extern void coresight_disable(struct coresight_device *csdev, int port);

#ifdef CONFIG_MSM_QDSS
extern struct qdss_source *qdss_get(const char *name);
extern void qdss_put(struct qdss_source *src);
extern int qdss_enable(struct qdss_source *src);
extern void qdss_disable(struct qdss_source *src);
extern void qdss_disable_sink(void);
#else
static inline struct qdss_source *qdss_get(const char *name) { return NULL; }
static inline void qdss_put(struct qdss_source *src) {}
static inline int qdss_enable(struct qdss_source *src) { return -ENOSYS; }
static inline void qdss_disable(struct qdss_source *src) {}
static inline void qdss_disable_sink(void) {}
#endif

#endif
