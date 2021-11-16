/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_OS_H__
#define __NANDX_OS_H__

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/bug.h>
#include <linux/jiffies.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <stdarg.h>
#include <linux/of_irq.h>

#ifndef NFI_BASE
#define NFI_BASE 0
#endif

#ifndef NFIECC_BASE
#define NFIECC_BASE 0
#endif

#define nwritel(value, reg) writel(value, (void __iomem *)reg)
#define nwritew(value, reg) writew(value, (void __iomem *)reg)
#define nwriteb(value, reg) writeb(value, (void __iomem *)reg)
#define nreadl(reg) readl((void __iomem *)reg)
#define nreadw(reg) readw((void __iomem *)reg)
#define nreadb(reg) readb((void __iomem *)reg)

/* extern common irq handler function */
extern enum NIRQ_RETURN nfi_irq_handler(void *arg);
extern enum NIRQ_RETURN ecc_irq_handler(void *arg);

static inline void *mem_alloc(u32 count, u32 size)
{
	return kcalloc(count, size, GFP_KERNEL);
}

static inline void mem_free(void *mem)
{
	kfree(mem);
}

static inline u32 nand_dma_map(void *dev, void *buf, u64 len,
			       enum NDMA_OPERATION op)
{
	u32 addr;
	int ret;
	enum dma_data_direction dir;
	struct platform_device *pdev = (struct platform_device *)dev;
	struct device *ndev = &pdev->dev;

	dir = (op == NDMA_FROM_DEV) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	addr = dma_map_single(ndev, (void *)buf, len, dir);
	ret = dma_mapping_error(ndev, addr);
	if (ret)
		pr_err("dma mapping error!\n");
	return addr;
}

static inline void nand_dma_unmap(void *dev, void *buf, u32 addr, u64 len,
				  enum NDMA_OPERATION op)
{
	enum dma_data_direction dir;
	struct platform_device *pdev = (struct platform_device *)dev;
	struct device *ndev = &pdev->dev;

	dir = (op == NDMA_FROM_DEV) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	dma_unmap_single(ndev, addr, len, dir);
}

static inline void *nand_lock_create(void)
{
	spinlock_t *lock;

	lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL | __GFP_ZERO);
	spin_lock_init(lock);
	return lock;
}

static inline void nand_lock_destroy(void *lock)
{
	kfree(lock);
}

static inline void nand_lock(void *lock)
{
	spin_lock((spinlock_t *) lock);
}

static inline void nand_unlock(void *lock)
{
	spin_unlock((spinlock_t *) lock);
}

static inline irqreturn_t nfi_interrupt_handler(int irq, void *id)
{
	enum NIRQ_RETURN nret;

	nret = nfi_irq_handler(id);

	return (nret == NIRQ_HANDLED) ? IRQ_HANDLED : IRQ_NONE;
}

static inline irqreturn_t ecc_interrupt_handler(int irq, void *id)
{
	enum NIRQ_RETURN nret;

	nret = ecc_irq_handler(id);

	return (nret == NIRQ_HANDLED) ? IRQ_HANDLED : IRQ_NONE;
}

static inline void *nand_event_create(void)
{
	struct completion *event;

	event = kmalloc(sizeof(struct completion), GFP_KERNEL | __GFP_ZERO);
	init_completion(event);
	return event;
}

static inline void nand_event_destroy(void *event)
{
	kfree(event);
}

static inline void nand_event_complete(void *event)
{
	complete(event);
}

static inline void nand_event_init(void *event)
{
	init_completion(event);
}

/*
 * timeout value is millisecond
 * return non-zero for complete, else timeout
 */
static inline int nand_event_wait_complete(void *event, u32 timeout)
{
	return wait_for_completion_timeout(event, usecs_to_jiffies(timeout));
}

/* @data for different register function */
static inline int nand_irq_register(u32 irq_id, void *irq_handler,
				    const char *name, void *data)
{
	return request_irq(irq_id, irq_handler, IRQF_TRIGGER_NONE, name,
			   data);
}

static inline u64 get_current_time_us(void)
{
	struct timespec64 ts;
	u64 usec;

	getrawmonotonic64(&ts);

	usec = ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;

	return usec;
}

static inline bool is_support_mntl(void)
{
	return IS_ENABLED(CONFIG_MNTL_SUPPORT);
}

#endif				/* __NANDX_OS_H__ */
