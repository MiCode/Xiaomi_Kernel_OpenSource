/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define ETM_ARCH_V1_0		(0x00)
#define ETM_ARCH_V1_2		(0x02)
#define ETM_ARCH_V3_3		(0x23)
#define ETM_ARCH_V3_5		(0x25)
#define PFT_ARCH_MAJOR		(0x30)
#define PFT_ARCH_V1_1		(0x31)

enum coresight_clk_rate {
	CORESIGHT_CLK_RATE_OFF,
	CORESIGHT_CLK_RATE_TRACE = 1000,
	CORESIGHT_CLK_RATE_HSTRACE = 2000,
};

enum coresight_dev_type {
	CORESIGHT_DEV_TYPE_NONE,
	CORESIGHT_DEV_TYPE_SINK,
	CORESIGHT_DEV_TYPE_LINK,
	CORESIGHT_DEV_TYPE_LINKSINK,
	CORESIGHT_DEV_TYPE_SOURCE,
};

enum coresight_dev_subtype_sink {
	CORESIGHT_DEV_SUBTYPE_SINK_NONE,
	CORESIGHT_DEV_SUBTYPE_SINK_PORT,
	CORESIGHT_DEV_SUBTYPE_SINK_BUFFER,
};

enum coresight_dev_subtype_link {
	CORESIGHT_DEV_SUBTYPE_LINK_NONE,
	CORESIGHT_DEV_SUBTYPE_LINK_MERG,
	CORESIGHT_DEV_SUBTYPE_LINK_SPLIT,
	CORESIGHT_DEV_SUBTYPE_LINK_FIFO,
};

enum coresight_dev_subtype_source {
	CORESIGHT_DEV_SUBTYPE_SOURCE_NONE,
	CORESIGHT_DEV_SUBTYPE_SOURCE_PROC,
	CORESIGHT_DEV_SUBTYPE_SOURCE_BUS,
	CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE,
};

struct coresight_dev_subtype {
	enum coresight_dev_subtype_sink sink_subtype;
	enum coresight_dev_subtype_link link_subtype;
	enum coresight_dev_subtype_source source_subtype;
};

struct coresight_platform_data {
	int id;
	const char *name;
	int nr_inports;
	const int *outports;
	const int *child_ids;
	const int *child_ports;
	int nr_outports;
	bool default_sink;
};

struct coresight_desc {
	enum coresight_dev_type type;
	struct coresight_dev_subtype subtype;
	const struct coresight_ops *ops;
	struct coresight_platform_data *pdata;
	struct device *dev;
	const struct attribute_group **groups;
	struct module *owner;
};

struct coresight_connection {
	int outport;
	int child_id;
	int child_port;
	struct coresight_device *child_dev;
	struct list_head link;
};

struct coresight_refcnt {
	int sink_refcnt;
	int *link_refcnts;
	int source_refcnt;
};

struct coresight_device {
	int id;
	struct coresight_connection *conns;
	int nr_conns;
	enum coresight_dev_type type;
	struct coresight_dev_subtype subtype;
	const struct coresight_ops *ops;
	struct device dev;
	struct coresight_refcnt refcnt;
	struct list_head dev_link;
	struct list_head path_link;
	struct module *owner;
	bool enable;
};

#define to_coresight_device(d) container_of(d, struct coresight_device, dev)

struct coresight_ops_sink {
	int (*enable)(struct coresight_device *csdev);
	void (*disable)(struct coresight_device *csdev);
	void (*abort)(struct coresight_device *csdev);
};

struct coresight_ops_link {
	int (*enable)(struct coresight_device *csdev, int iport, int oport);
	void (*disable)(struct coresight_device *csdev, int iport, int oport);
};

struct coresight_ops_source {
	int (*enable)(struct coresight_device *csdev);
	void (*disable)(struct coresight_device *csdev);
};

struct coresight_ops {
	const struct coresight_ops_sink *sink_ops;
	const struct coresight_ops_link *link_ops;
	const struct coresight_ops_source *source_ops;
};

#ifdef CONFIG_CORESIGHT
extern struct coresight_device *
coresight_register(struct coresight_desc *desc);
extern void coresight_unregister(struct coresight_device *csdev);
extern int coresight_enable(struct coresight_device *csdev);
extern void coresight_disable(struct coresight_device *csdev);
extern void coresight_abort(void);
#else
static inline struct coresight_device *
coresight_register(struct coresight_desc *desc) { return NULL; }
static inline void coresight_unregister(struct coresight_device *csdev) {}
static inline int
coresight_enable(struct coresight_device *csdev) { return -ENOSYS; }
static inline void coresight_disable(struct coresight_device *csdev) {}
static inline void coresight_abort(void) {}
#endif

#endif
