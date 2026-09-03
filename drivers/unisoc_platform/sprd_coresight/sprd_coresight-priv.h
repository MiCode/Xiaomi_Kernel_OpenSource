/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 */

#ifndef _SPRD_CORESIGHT_PRIV_H
#define _SPRD_CORESIGHT_PRIV_H

#include <linux/amba/bus.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/coresight.h>
#include <linux/pm_runtime.h>

/*
 * Coresight management registers (0xf00-0xfcc)
 * 0xfa0 - 0xfa4: Management	registers in PFTv1.0
 *		  Trace		registers in PFTv1.1
 */
#define CORESIGHT_ITCTRL	0xf00
#define CORESIGHT_CLAIMSET	0xfa0
#define CORESIGHT_CLAIMCLR	0xfa4
#define CORESIGHT_LAR		0xfb0
#define CORESIGHT_LSR		0xfb4
#define CORESIGHT_DEVARCH	0xfbc
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		0xfc8
#define CORESIGHT_DEVTYPE	0xfcc


/*
 * Coresight device CLAIM protocol.
 * See PSCI - ARM DEN 0022D, Section: 6.8.1 Debug and Trace save and restore.
 */
#define CORESIGHT_CLAIM_SELF_HOSTED	BIT(1)

#define TIMEOUT_US		100
#define BMVAL(val, lsb, msb)	((val & GENMASK(msb, lsb)) >> lsb)

#define ETM_MODE_EXCL_KERN	BIT(30)
#define ETM_MODE_EXCL_USER	BIT(31)

typedef u32 (*coresight_read_fn)(const struct device *, u32 offset);
#define __coresight_simple_func(type, func, name, lo_off, hi_off)	\
static ssize_t name##_show(struct device *_dev,				\
			   struct device_attribute *attr, char *buf)	\
{									\
	type *drvdata = dev_get_drvdata(_dev->parent);			\
	coresight_read_fn fn = func;					\
	u64 val;							\
	pm_runtime_get_sync(_dev->parent);				\
	if (fn)								\
		val = (u64)fn(_dev->parent, lo_off);			\
	else								\
		val = coresight_read_reg_pair(drvdata->base,		\
						 lo_off, hi_off);	\
	pm_runtime_put_sync(_dev->parent);				\
	return scnprintf(buf, PAGE_SIZE, "0x%llx\n", val);		\
}									\
static DEVICE_ATTR_RO(name)

#define coresight_simple_func(type, func, name, offset)			\
	__coresight_simple_func(type, func, name, offset, -1)
#define coresight_simple_reg32(type, name, offset)			\
	__coresight_simple_func(type, NULL, name, offset, -1)
#define coresight_simple_reg64(type, name, lo_off, hi_off)		\
	__coresight_simple_func(type, NULL, name, lo_off, hi_off)

extern const u32 coresight_barrier_pkt[4];
#define CORESIGHT_BARRIER_PKT_SIZE (sizeof(coresight_barrier_pkt))

enum etm_addr_type {
	ETM_ADDR_TYPE_NONE,
	ETM_ADDR_TYPE_SINGLE,
	ETM_ADDR_TYPE_RANGE,
	ETM_ADDR_TYPE_START,
	ETM_ADDR_TYPE_STOP,
};

enum cs_mode {
	CS_MODE_DISABLED,
	CS_MODE_SYSFS,
	CS_MODE_PERF,
};

/**
 * struct cs_buffer - keep track of a recording session' specifics
 * @cur:	index of the current buffer
 * @nr_pages:	max number of pages granted to us
 * @pid:	PID this cs_buffer belongs to
 * @offset:	offset within the current buffer
 * @data_size:	how much we collected in this run
 * @snapshot:	is this run in snapshot mode
 * @data_pages:	a handle the ring buffer
 */
struct cs_buffers {
	unsigned int		cur;
	unsigned int		nr_pages;
	pid_t			pid;
	unsigned long		offset;
	local_t			data_size;
	bool			snapshot;
	void			**data_pages;
};

static inline void coresight_insert_barrier_packet(void *buf)
{
	if (buf)
		memcpy(buf, coresight_barrier_pkt, CORESIGHT_BARRIER_PKT_SIZE);
}

static inline void CS_LOCK(void __iomem *addr)
{
	do {
		/* Wait for things to settle */
		mb();
		writel_relaxed(0x0, addr + CORESIGHT_LAR);
	} while (0);
}

static inline void CS_UNLOCK(void __iomem *addr)
{
	do {
		writel_relaxed(CORESIGHT_UNLOCK, addr + CORESIGHT_LAR);
		/* Make sure everyone has seen this */
		mb();
	} while (0);
}

static inline u64
coresight_read_reg_pair(void __iomem *addr, s32 lo_offset, s32 hi_offset)
{
	u64 val;

	val = readl_relaxed(addr + lo_offset);
	val |= (hi_offset < 0) ? 0 :
	       (u64)readl_relaxed(addr + hi_offset) << 32;
	return val;
}

static inline void coresight_write_reg_pair(void __iomem *addr, u64 val,
						 s32 lo_offset, s32 hi_offset)
{
	writel_relaxed((u32)val, addr + lo_offset);
	if (hi_offset >= 0)
		writel_relaxed((u32)(val >> 32), addr + hi_offset);
}

void sprd_coresight_disable_path(struct list_head *path);
int sprd_coresight_enable_path(struct list_head *path, u32 mode, void *sink_data);
struct coresight_device *sprd_coresight_get_sink(struct list_head *path);
struct coresight_device *
sprd_coresight_get_enabled_sink(struct coresight_device *source);
struct coresight_device *coresight_get_sink_by_id(u32 id);
struct coresight_device *
coresight_find_default_sink(struct coresight_device *csdev);
struct list_head *coresight_build_path(struct coresight_device *csdev,
				       struct coresight_device *sink);
void sprd_coresight_release_path(struct list_head *path);
int coresight_add_sysfs_link(struct coresight_sysfs_link *info);
void coresight_remove_sysfs_link(struct coresight_sysfs_link *info);
int coresight_create_conns_sysfs_group(struct coresight_device *csdev);
void coresight_remove_conns_sysfs_group(struct coresight_device *csdev);
int coresight_make_links(struct coresight_device *orig,
			 struct coresight_connection *conn,
			 struct coresight_device *target);
void coresight_remove_links(struct coresight_device *orig,
			    struct coresight_connection *conn);

#if IS_ENABLED(CONFIG_UNISOC_CORESIGHT_SOURCE_ETM3X)
extern int etm_readl_cp14(u32 off, unsigned int *val);
extern int etm_writel_cp14(u32 off, u32 val);
#else
static inline int etm_readl_cp14(u32 off, unsigned int *val) { return 0; }
static inline int etm_writel_cp14(u32 off, u32 val) { return 0; }
#endif

struct cti_assoc_op {
	void (*add)(struct coresight_device *csdev);
	void (*remove)(struct coresight_device *csdev);
};

extern void coresight_set_cti_ops(const struct cti_assoc_op *cti_op);
extern void coresight_remove_cti_ops(void);

/*
 * Macros and inline functions to handle CoreSight UCI data and driver
 * private data in AMBA ID table entries, and extract data values.
 */

/* coresight AMBA ID, no UCI, no driver data: id table entry */
#define CS_AMBA_ID(pid)			\
	{				\
		.id	= pid,		\
		.mask	= 0x000fffff,	\
	}

/* coresight AMBA ID, UCI with driver data only: id table entry. */
#define CS_AMBA_ID_DATA(pid, dval)				\
	{							\
		.id	= pid,					\
		.mask	= 0x000fffff,				\
		.data	=  (void *)&(struct amba_cs_uci_id)	\
			{				\
				.data = (void *)dval,	\
			}				\
	}

/* coresight AMBA ID, full UCI structure: id table entry. */
#define CS_AMBA_UCI_ID(pid, uci_ptr)		\
	{					\
		.id	= pid,			\
		.mask	= 0x000fffff,		\
		.data	= (void *)uci_ptr	\
	}

/* extract the data value from a UCI structure given amba_id pointer. */
static inline void *coresight_get_uci_data(const struct amba_id *id)
{
	struct amba_cs_uci_id *uci_id = id->data;

	if (!uci_id)
		return NULL;

	return uci_id->data;
}

void sprd_coresight_release_platform_data(struct coresight_device *csdev,
				     struct coresight_platform_data *pdata);
struct coresight_device *
sprd_coresight_find_csdev_by_fwnode(struct fwnode_handle *r_fwnode);
void coresight_set_assoc_ectdev_mutex(struct coresight_device *csdev,
				      struct coresight_device *ect_csdev);

void coresight_set_percpu_sink(int cpu, struct coresight_device *csdev);
struct coresight_device *coresight_get_percpu_sink(int cpu);

#if IS_ENABLED(CONFIG_UNISOC_CORESIGHT)

static inline u32 csdev_access_relaxed_read32(struct csdev_access *csa,
					      u32 offset)
{
	if (likely(csa->io_mem))
		return readl_relaxed(csa->base + offset);

	return csa->read(offset, true, false);
}

static inline u32 csdev_access_read32(struct csdev_access *csa, u32 offset)
{
	if (likely(csa->io_mem))
		return readl(csa->base + offset);

	return csa->read(offset, false, false);
}

static inline void csdev_access_relaxed_write32(struct csdev_access *csa,
						u32 val, u32 offset)
{
	if (likely(csa->io_mem))
		writel_relaxed(val, csa->base + offset);
	else
		csa->write(val, offset, true, false);
}

static inline void csdev_access_write32(struct csdev_access *csa, u32 val, u32 offset)
{
	if (likely(csa->io_mem))
		writel(val, csa->base + offset);
	else
		csa->write(val, offset, false, false);
}

#ifdef CONFIG_64BIT

static inline u64 csdev_access_relaxed_read64(struct csdev_access *csa,
					      u32 offset)
{
	if (likely(csa->io_mem))
		return readq_relaxed(csa->base + offset);

	return csa->read(offset, true, true);
}

static inline u64 csdev_access_read64(struct csdev_access *csa, u32 offset)
{
	if (likely(csa->io_mem))
		return readq(csa->base + offset);

	return csa->read(offset, false, true);
}

static inline void csdev_access_relaxed_write64(struct csdev_access *csa,
						u64 val, u32 offset)
{
	if (likely(csa->io_mem))
		writeq_relaxed(val, csa->base + offset);
	else
		csa->write(val, offset, true, true);
}

static inline void csdev_access_write64(struct csdev_access *csa, u64 val, u32 offset)
{
	if (likely(csa->io_mem))
		writeq(val, csa->base + offset);
	else
		csa->write(val, offset, false, true);
}

#else	/* !CONFIG_64BIT */

static inline u64 csdev_access_relaxed_read64(struct csdev_access *csa,
					      u32 offset)
{
	WARN_ON(1);
	return 0;
}

static inline u64 csdev_access_read64(struct csdev_access *csa, u32 offset)
{
	WARN_ON(1);
	return 0;
}

static inline void csdev_access_relaxed_write64(struct csdev_access *csa,
						u64 val, u32 offset)
{
	WARN_ON(1);
}

static inline void csdev_access_write64(struct csdev_access *csa, u64 val, u32 offset)
{
	WARN_ON(1);
}
#endif	/* CONFIG_64BIT */

static inline bool coresight_is_percpu_source(struct coresight_device *csdev)
{
	return csdev && (csdev->type == CORESIGHT_DEV_TYPE_SOURCE) &&
	       (csdev->subtype.source_subtype == CORESIGHT_DEV_SUBTYPE_SOURCE_PROC);
}

static inline bool coresight_is_percpu_sink(struct coresight_device *csdev)
{
	return csdev && (csdev->type == CORESIGHT_DEV_TYPE_SINK) &&
	       (csdev->subtype.sink_subtype == CORESIGHT_DEV_SUBTYPE_SINK_PERCPU_SYSMEM);
}

extern struct coresight_device *
sprd_coresight_register(struct coresight_desc *desc);
extern void sprd_coresight_unregister(struct coresight_device *csdev);
extern int sprd_coresight_enable(struct coresight_device *csdev);
extern void sprd_coresight_disable(struct coresight_device *csdev);
extern int sprd_coresight_timeout(void __iomem *addr, u32 offset,
			     int position, int value);

extern int sprd_coresight_claim_device(struct coresight_device *csdev);
extern int sprd_coresight_claim_device_unlocked(struct coresight_device *csdev);

extern void sprd_coresight_disclaim_device(struct coresight_device *csdev);
extern void sprd_coresight_disclaim_device_unlocked(struct coresight_device *csdev);
extern char *sprd_coresight_alloc_device_name(struct coresight_dev_list *devs,
					 struct device *dev);

extern bool sprd_coresight_loses_context_with_cpu(struct device *dev);

u32 sprd_coresight_relaxed_read32(struct coresight_device *csdev, u32 offset);
u32 sprd_coresight_read32(struct coresight_device *csdev, u32 offset);
void sprd_coresight_write32(struct coresight_device *csdev, u32 val, u32 offset);
void sprd_coresight_relaxed_write32(struct coresight_device *csdev,
			       u32 val, u32 offset);
u64 sprd_coresight_relaxed_read64(struct coresight_device *csdev, u32 offset);
u64 sprd_coresight_read64(struct coresight_device *csdev, u32 offset);
void sprd_coresight_relaxed_write64(struct coresight_device *csdev,
			       u64 val, u32 offset);
void sprd_coresight_write64(struct coresight_device *csdev, u64 val, u32 offset);

#else
static inline struct coresight_device *
sprd_coresight_register(struct coresight_desc *desc) { return NULL; }
static inline void sprd_coresight_unregister(struct coresight_device *csdev) {}
static inline int
sprd_coresight_enable(struct coresight_device *csdev) { return -ENOSYS; }
static inline void sprd_coresight_disable(struct coresight_device *csdev) {}

static inline int sprd_coresight_timeout(void __iomem *addr, u32 offset,
				    int position, int value)
{
	return 1;
}

static inline int sprd_coresight_claim_device_unlocked(struct coresight_device *csdev)
{
	return -EINVAL;
}

static inline int sprd_coresight_claim_device(struct coresight_device *csdev)
{
	return -EINVAL;
}

static inline void sprd_coresight_disclaim_device(struct coresight_device *csdev) {}
static inline void sprd_coresight_disclaim_device_unlocked(struct coresight_device *csdev) {}

static inline bool sprd_coresight_loses_context_with_cpu(struct device *dev)
{
	return false;
}

static inline u32 sprd_coresight_relaxed_read32(struct coresight_device *csdev, u32 offset)
{
	WARN_ON_ONCE(1);
	return 0;
}

static inline u32 sprd_coresight_read32(struct coresight_device *csdev, u32 offset)
{
	WARN_ON_ONCE(1);
	return 0;
}

static inline void sprd_coresight_write32(struct coresight_device *csdev, u32 val, u32 offset)
{
}

static inline void sprd_coresight_relaxed_write32(struct coresight_device *csdev,
					     u32 val, u32 offset)
{
}

static inline u64 sprd_coresight_relaxed_read64(struct coresight_device *csdev,
					   u32 offset)
{
	WARN_ON_ONCE(1);
	return 0;
}

static inline u64 sprd_coresight_read64(struct coresight_device *csdev, u32 offset)
{
	WARN_ON_ONCE(1);
	return 0;
}

static inline void sprd_coresight_relaxed_write64(struct coresight_device *csdev,
					     u64 val, u32 offset)
{
}

static inline void sprd_coresight_write64(struct coresight_device *csdev, u64 val, u32 offset)
{
}

#endif		/* IS_ENABLED(CONFIG_UNISOC_CORESIGHT) */

extern int sprd_coresight_get_cpu(struct device *dev);

struct coresight_platform_data *sprd_coresight_get_platform_data(struct device *dev);


extern struct device *sprd_of_coresight_get_device_by_node(struct device_node *endpoint);

int sprd_coresight_enable_sink_show_export(struct coresight_device *csdev);
int sprd_coresight_enable_sink_store_export(struct coresight_device *csdev, int val);
int sprd_coresight_enable_source_show_export(struct coresight_device *csdev);
int sprd_coresight_enable_source_store_export(struct coresight_device *csdev, int val);

int sprd_tmc_enable_sink_show(struct device *dev);
int sprd_tmc_enable_sink_store(struct device *dev, int val);
int sprd_etm4_enable_source_show(struct device *dev);
int sprd_etm4_enable_source_store(struct device *dev, int val);

static inline int sprd_coresight_get_trace_id(int cpu)
{
	/*
	 * A trace ID of value 0 is invalid, so let's start at some
	 * random value that fits in 7 bits and go from there.  Since
	 * the common convention is to have data trace IDs be I(N) + 1,
	 * set instruction trace IDs as a function of the CPU number.
	 */
	//return (CORESIGHT_ETM_PMU_SEED + (cpu * 2));
	/* set trace ID to cpu+1 to fit trace32 etb parser tool */
	return (cpu+1);
}

#endif
