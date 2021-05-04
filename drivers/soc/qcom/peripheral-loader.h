/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __MSM_PERIPHERAL_LOADER_H
#define __MSM_PERIPHERAL_LOADER_H

#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include "minidump_private.h"
#ifdef CONFIG_QGKI_MSM_BOOT_TIME_MARKER
#include <soc/qcom/boot_stats.h>
#endif
#define SECURE_PAGE_MAGIC 0xEEEEEEEE
struct device;
struct module;
/**
 * struct pil_priv - Private state for a pil_desc
 * @proxy: work item used to run the proxy unvoting routine
 * @ws: wakeup source to prevent suspend during pil_boot
 * @wname: name of @ws
 * @desc: pointer to pil_desc this is private data for
 * @seg: list of segments sorted by physical address
 * @entry_addr: physical address where processor starts booting at
 * @base_addr: smallest start address among all segments that are relocatable
 * @region_start: address where relocatable region starts or lowest address
 * for non-relocatable images
 * @region_end: address where relocatable region ends or highest address for
 * non-relocatable images
 * @region: region allocated for relocatable images
 * @unvoted_flag: flag to keep track if we have unvoted or not.
 *
 * This struct contains data for a pil_desc that should not be exposed outside
 * of this file. This structure points to the descriptor and the descriptor
 * points to this structure so that PIL drivers can't access the private
 * data of a descriptor but this file can access both.
 */
struct pil_priv {
	struct delayed_work proxy;
	struct wakeup_source *ws;
	char wname[32];
	struct pil_desc *desc;
	int num_segs;
	struct list_head segs;
	phys_addr_t entry_addr;
	phys_addr_t base_addr;
	phys_addr_t region_start;
	phys_addr_t region_end;
	bool is_region_allocated;
	struct pil_image_info __iomem *info;
	int id;
	int unvoted_flag;
	size_t region_size;
};

/**
 * struct pil_desc - PIL descriptor
 * @name: string used for pil_get()
 * @fw_name: firmware name
 * @dev: parent device
 * @ops: callback functions
 * @owner: module the descriptor belongs to
 * @proxy_timeout: delay in ms until proxy vote is removed
 * @flags: bitfield for image flags
 * @priv: DON'T USE - internal only
 * @attrs: DMA attributes to be used during dma allocation.
 * @proxy_unvote_irq: IRQ to trigger a proxy unvote. proxy_timeout
 * is ignored if this is set.
 * @map_fw_mem: Custom function used to map physical address space to virtual.
 * This defaults to ioremap if not specified.
 * @unmap_fw_mem: Custom function used to undo mapping by map_fw_mem.
 * This defaults to iounmap if not specified.
 * @shutdown_fail: Set if PIL op for shutting down subsystem fails.
 * @modem_ssr: true if modem is restarting, false if booting for first time.
 * @clear_fw_region: Clear fw region on failure in loading.
 * @subsys_vmid: memprot id for the subsystem.
 */
struct pil_desc {
	const char *name;
	const char *fw_name;
	struct device *dev;
	const struct pil_reset_ops *ops;
	struct module *owner;
	unsigned long proxy_timeout;
	unsigned long flags;
#define PIL_SKIP_ENTRY_CHECK	BIT(0)
	struct pil_priv *priv;
	unsigned long attrs;
	unsigned int proxy_unvote_irq;
	void __iomem * (*map_fw_mem)(phys_addr_t phys, size_t size, void *data);
	void (*unmap_fw_mem)(void __iomem *virt, size_t size, void *data);
	void *map_data;
	bool shutdown_fail;
	bool modem_ssr;
	bool clear_fw_region;
	bool sequential_loading;
	u32 subsys_vmid;
	bool signal_aop;
	struct mbox_client cl;
	struct mbox_chan *mbox;
	struct md_ss_toc *minidump_ss;
	struct md_ss_toc **aux_minidump;
	int minidump_id;
	int *aux_minidump_ids;
	int num_aux_minidump_ids;
	u32 extra_size;
	bool minidump_as_elf32;
};

/**
 * struct pil_image_info - info in IMEM about image and where it is loaded
 * @name: name of image (may or may not be NULL terminated)
 * @start: indicates physical address where image starts (little endian)
 * @size: size of image (little endian)
 */
struct pil_image_info {
	char name[8];
	__le64 start;
	__le32 size;
} __attribute__((__packed__));

/**
 * struct pil_reset_ops - PIL operations
 * @init_image: prepare an image for authentication
 * @mem_setup: prepare the image memory region
 * @verify_blob: authenticate a program segment, called once for each loadable
 *		 program segment (optional)
 * @proxy_vote: make proxy votes before auth_and_reset (optional)
 * @auth_and_reset: boot the processor
 * @proxy_unvote: remove any proxy votes (optional)
 * @deinit_image: restore actions performed in init_image if necessary
 * @shutdown: shutdown the processor
 */
struct pil_reset_ops {
	int (*init_image)(struct pil_desc *pil, const u8 *metadata,
			  size_t size);
	int (*mem_setup)(struct pil_desc *pil, phys_addr_t addr, size_t size);
	int (*verify_blob)(struct pil_desc *pil, phys_addr_t phy_addr,
			   size_t size);
	int (*proxy_vote)(struct pil_desc *pil);
	int (*auth_and_reset)(struct pil_desc *pil);
	void (*proxy_unvote)(struct pil_desc *pil);
	int (*deinit_image)(struct pil_desc *pil);
	int (*shutdown)(struct pil_desc *pil);
};

#if IS_ENABLED(CONFIG_MSM_PIL)
extern int pil_desc_init(struct pil_desc *desc);
extern int pil_boot(struct pil_desc *desc);
extern void pil_shutdown(struct pil_desc *desc);
extern void pil_free_memory(struct pil_desc *desc);
extern void pil_desc_release(struct pil_desc *desc);
extern phys_addr_t pil_get_entry_addr(struct pil_desc *desc);
extern int pil_do_ramdump(struct pil_desc *desc, void *ramdump_dev,
			  void *minidump_dev);
extern int pil_assign_mem_to_subsys(struct pil_desc *desc, phys_addr_t addr,
						size_t size);
extern int pil_assign_mem_to_linux(struct pil_desc *desc, phys_addr_t addr,
						size_t size);
extern int pil_assign_mem_to_subsys_and_linux(struct pil_desc *desc,
						phys_addr_t addr, size_t size);
extern int pil_reclaim_mem(struct pil_desc *desc, phys_addr_t addr, size_t size,
						int VMid);
extern bool pil_is_timeout_disabled(void);
#else
static inline int pil_desc_init(struct pil_desc *desc) { return 0; }
static inline int pil_boot(struct pil_desc *desc) { return 0; }
static inline void pil_shutdown(struct pil_desc *desc) { }
static inline void pil_free_memory(struct pil_desc *desc) { }
static inline void pil_desc_release(struct pil_desc *desc) { }
static inline phys_addr_t pil_get_entry_addr(struct pil_desc *desc)
{
	return 0;
}
static inline int pil_do_ramdump(struct pil_desc *desc,
		void *ramdump_dev, void *minidump_dev)
{
	return 0;
}
static inline int pil_assign_mem_to_subsys(struct pil_desc *desc,
						phys_addr_t addr, size_t size)
{
	return 0;
}
static inline int pil_assign_mem_to_linux(struct pil_desc *desc,
						phys_addr_t addr, size_t size)
{
	return 0;
}
static inline int pil_assign_mem_to_subsys_and_linux(struct pil_desc *desc,
						phys_addr_t addr, size_t size)
{
	return 0;
}
static inline int pil_reclaim_mem(struct pil_desc *desc, phys_addr_t addr,
					size_t size, int VMid)
{
	return 0;
}
static inline bool pil_is_timeout_disabled(void) { return false; }
#endif

#endif
