// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/module.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/sprd_iommu.h>
#include <linux/ion.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>

#include <linux/dma-heap.h>
#include <linux/mutex.h>

#include "sprd_iommu_sysfs.h"
#include "drv/com/sprd_com.h"

BLOCKING_NOTIFIER_HEAD(sprd_iommu_notif_chain);

static int sprd_iommu_probe(struct platform_device *pdev);
static int sprd_iommu_remove(struct platform_device *pdev);

struct system_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table sg_table;
	int vmap_cnt;
	void *vaddr;

	bool uncached;
};

struct carveout_heap_buffer {
	struct dma_heap *heap;
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table *sg_table;
	//struct gen_pool *pool;
	phys_addr_t base;
	int vmap_cnt;
	void *vaddr;
#ifdef CONFIG_E_SHOW_MEM
	struct rb_node node;
	pid_t pid;
	char task_name[TASK_COMM_LEN];
	struct timespec64 alloc_ts;
	struct dmabuf_map_info mappers[MAX_MAP_USER];
#endif
	bool uncached;
};

int sprd_iommu_notifier_call_chain(void *data)
{
	return blocking_notifier_call_chain(&sprd_iommu_notif_chain, 0, data);
}
EXPORT_SYMBOL(sprd_iommu_notifier_call_chain);

static struct sprd_iommu_list_data sprd_iommu_list[SPRD_IOMMU_MAX] = {
	{ .iommu_id = SPRD_IOMMU_VSP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VSP1,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VSP2,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DCAM,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DCAM1,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_CPP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_GSP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_GSP1,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_JPG,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DISP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_DISP1,
	   .enabled = false,
	   .iommu_dev = NULL},


	{ .iommu_id = SPRD_IOMMU_ISP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_ISP1,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_FD,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_AI,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_EPP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_EDP,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_IDMA,
	   .enabled = false,
	   .iommu_dev = NULL},

	{ .iommu_id = SPRD_IOMMU_VDMA,
	   .enabled = false,
	   .iommu_dev = NULL},
};

static const struct of_device_id sprd_iommu_ids[] = {
	{ .compatible = "unisoc,iommuexle-gsp",
	   .data = (void *)(IOMMU_EXLE_GSP)},

	{ .compatible = "unisoc,iommuexle-dispc",
	   .data = (void *)(IOMMU_EXLE_DISP)},

	{ .compatible = "unisoc,iommuexle-vsp",
	   .data = (void *)(IOMMU_EXLE_VSP)},

	{ .compatible = "unisoc,iommuexle-dcam",
	   .data = (void *)(IOMMU_EXLE_DCAM)},

	{ .compatible = "unisoc,iommuexle-isp",
	   .data = (void *)(IOMMU_EXLE_ISP)},

	{ .compatible = "unisoc,iommuexle-cpp",
	   .data = (void *)(IOMMU_EXLE_CPP)},

	{ .compatible = "unisoc,iommuexle-jpg",
	   .data = (void *)(IOMMU_EXLE_JPG)},

	{ .compatible = "unisoc,iommuexpk2-gsp",
	   .data = (void *)(IOMMU_EXPK2_GSP)},

	{ .compatible = "unisoc,iommuexpk2-dispc",
	   .data = (void *)(IOMMU_EXPK2_DISP)},

	{ .compatible = "unisoc,iommuexpk2-vsp",
	   .data = (void *)(IOMMU_EXPK2_VSP)},

	{ .compatible = "unisoc,iommuexpk2-dcam",
	   .data = (void *)(IOMMU_EXPK2_DCAM)},

	{ .compatible = "unisoc,iommuexpk2-isp",
	   .data = (void *)(IOMMU_EXPK2_ISP)},

	{ .compatible = "unisoc,iommuexpk2-cpp",
	   .data = (void *)(IOMMU_EXPK2_CPP)},

	{ .compatible = "unisoc,iommuexpk2-jpg",
	   .data = (void *)(IOMMU_EXPK2_JPG)},

	{ .compatible = "unisoc,iommuexl3-dispc",
	   .data = (void *)(IOMMU_EXL3_DISP)},

	{ .compatible = "unisoc,iommuexl3-vsp",
	   .data = (void *)(IOMMU_EXL3_VSP)},

	{ .compatible = "unisoc,iommuexl3-dcam",
	   .data = (void *)(IOMMU_EXL3_DCAM)},

	{ .compatible = "unisoc,iommuexl3-isp",
	   .data = (void *)(IOMMU_EXL3_ISP)},

	{ .compatible = "unisoc,iommuexl3-cpp",
	   .data = (void *)(IOMMU_EXL3_CPP)},

	{ .compatible = "unisoc,iommuexl3-jpg",
	   .data = (void *)(IOMMU_EXL3_JPG)},
	{ .compatible = "unisoc,iommuexl5-dispc",
	   .data = (void *)(IOMMU_EXL5_DISP)},

	{ .compatible = "unisoc,iommuexl5-vsp",
	   .data = (void *)(IOMMU_EXL5_VSP)},

	{ .compatible = "unisoc,iommuexl5-dcam",
	   .data = (void *)(IOMMU_EXL5_DCAM)},

	{ .compatible = "unisoc,iommuexl5-isp",
	   .data = (void *)(IOMMU_EXL5_ISP)},

	{ .compatible = "unisoc,iommuexl5-cpp",
	   .data = (void *)(IOMMU_EXL5_CPP)},

	{ .compatible = "unisoc,iommuexl5-jpg",
	   .data = (void *)(IOMMU_EXL5_JPG)},

	{ .compatible = "unisoc,iommuexl5-fd",
	  .data = (void *)(IOMMU_EXL5_FD)},

	{ .compatible = "unisoc,iommuexroc1-dispc",
	   .data = (void *)(IOMMU_EXROC1_DISP)},

	{ .compatible = "unisoc,iommuexroc1-vsp",
	   .data = (void *)(IOMMU_EXROC1_VSP)},

	{ .compatible = "unisoc,iommuexroc1-dcam",
	   .data = (void *)(IOMMU_EXROC1_DCAM)},

	{ .compatible = "unisoc,iommuexroc1-isp",
	   .data = (void *)(IOMMU_EXROC1_ISP)},

	{ .compatible = "unisoc,iommuexroc1-cpp",
	   .data = (void *)(IOMMU_EXROC1_CPP)},

	{ .compatible = "unisoc,iommuexroc1-jpg",
	   .data = (void *)(IOMMU_EXROC1_JPG)},

	{ .compatible = "unisoc,iommuexroc1-fd",
	  .data = (void *)(IOMMU_EXROC1_FD)},

	{ .compatible = "unisoc,iommuexroc1-ai",
	  .data = (void *)(IOMMU_EXROC1_AI)},

	{ .compatible = "unisoc,iommuexroc1-epp",
	  .data = (void *)(IOMMU_EXROC1_EPP)},

	{ .compatible = "unisoc,iommuexroc1-edp",

	  .data = (void *)(IOMMU_EXROC1_EDP)},

	{ .compatible = "unisoc,iommuvaul5p-dispc",
	   .data = (void *)(IOMMU_VAUL5P_DISP)},

	{ .compatible = "unisoc,iommuvaul5p-vsp",
	   .data = (void *)(IOMMU_VAUL5P_VSP)},

	{ .compatible = "unisoc,iommuvaul5p-dcam",
	   .data = (void *)(IOMMU_VAUL5P_DCAM)},

	{ .compatible = "unisoc,iommuvaul5p-isp",
	   .data = (void *)(IOMMU_VAUL5P_ISP)},

	{ .compatible = "unisoc,iommuvaul5p-cpp",
	   .data = (void *)(IOMMU_VAUL5P_CPP)},

	{ .compatible = "unisoc,iommuvaul5p-jpg",
	   .data = (void *)(IOMMU_VAUL5P_JPG)},

	{ .compatible = "unisoc,iommuvaul5p-fd",
	  .data = (void *)(IOMMU_VAUL5P_FD)},

	{ .compatible = "unisoc,iommuvaul5p-ai",
	  .data = (void *)(IOMMU_VAUL5P_AI)},

	{ .compatible = "unisoc,iommuvaul5p-epp",
	  .data = (void *)(IOMMU_VAUL5P_EPP)},

	{ .compatible = "unisoc,iommuvaul5p-edp",
	  .data = (void *)(IOMMU_VAUL5P_EDP)},

	{ .compatible = "unisoc,iommuvaul5p-idma",
	  .data = (void *)(IOMMU_VAUL5P_IDMA)},

	{ .compatible = "unisoc,iommuvaul5p-vdma",
	  .data = (void *)(IOMMU_VAUL5P_VDMA)},

	{ .compatible = "sprd,iommuvaul6-dispc",
	   .data = (void *)(IOMMU_VAUL5P_DISP)},

	{ .compatible = "sprd,iommuvaul6-vsp",
	   .data = (void *)(IOMMU_VAUL6_VSP)},

	{ .compatible = "sprd,iommuvaul6-dcam",
	   .data = (void *)(IOMMU_VAUL6_DCAM)},

	{ .compatible = "sprd,iommuvaul6-isp",
	   .data = (void *)(IOMMU_VAUL6_ISP)},

	{ .compatible = "sprd,iommuvaul6-cpp",
	   .data = (void *)(IOMMU_VAUL6_CPP)},

	{ .compatible = "sprd,iommuvaul6-jpg",
	   .data = (void *)(IOMMU_VAUL6_JPG)},

	{ .compatible = "sprd,iommuvaul6-fd",
	  .data = (void *)(IOMMU_VAUL6_FD)},

	{ .compatible = "unisoc,iommuvaul6p-gsp",
	   .data = (void *)(IOMMU_VAUL6P_GSP)},

	{ .compatible = "unisoc,iommuvaul6p-gsp1",
	   .data = (void *)(IOMMU_VAUL6P_GSP1)},


	{ .compatible = "unisoc,iommuvaul6p-dispc",
	   .data = (void *)(IOMMU_VAUL6P_DISP)},

	{ .compatible = "unisoc,iommuvaul6p-dispc1",
	   .data = (void *)(IOMMU_VAUL6P_DISP1)},

	{ .compatible = "unisoc,iommuvaul6p-vsp",
	   .data = (void *)(IOMMU_VAUL6P_VSP)},

	{ .compatible = "unisoc,iommuvaul6p-vsp1",
	   .data = (void *)(IOMMU_VAUL6P_VSP1)},

	{ .compatible = "unisoc,iommuvaul6p-vsp2",
	   .data = (void *)(IOMMU_VAUL6P_VSP2)},

	{ .compatible = "unisoc,iommuvaul6p-dcam",
	   .data = (void *)(IOMMU_VAUL6P_DCAM)},

	{ .compatible = "unisoc,iommuvaul6p-dcam1",
	   .data = (void *)(IOMMU_VAUL6P_DCAM1)},

	{ .compatible = "unisoc,iommuvaul6p-isp",
	   .data = (void *)(IOMMU_VAUL6P_ISP)},

	{ .compatible = "unisoc,iommuvaul6p-cpp",
	   .data = (void *)(IOMMU_VAUL6P_CPP)},

	{ .compatible = "unisoc,iommuvaul6p-jpg",
	   .data = (void *)(IOMMU_VAUL6P_JPG)},

	{ .compatible = "unisoc,iommuvaul6p-fd",
	  .data = (void *)(IOMMU_VAUL6P_FD)},

	{ .compatible = "unisoc,iommuvaul6p-ai",
	  .data = (void *)(IOMMU_VAUL6P_AI)},

	{ .compatible = "unisoc,iommuvaul6p-epp",
	  .data = (void *)(IOMMU_VAUL6P_EPP)},

	{ .compatible = "unisoc,iommuvaul6p-edp",
	  .data = (void *)(IOMMU_VAUL6P_EDP)},

	{ .compatible = "unisoc,iommuvaul6p-idma",
	  .data = (void *)(IOMMU_VAUL6P_IDMA)},

	{ .compatible = "unisoc,iommuvaul6p-vdma",
	  .data = (void *)(IOMMU_VAUL6P_VDMA)},

	/* QogirN6Lite */
	{ .compatible = "unisoc,iommuvaun6l-gsp",
	  .data = (void *)(IOMMU_VAUN6L_GSP)},

	{ .compatible = "unisoc,iommuvaun6l-gsp1",
	  .data = (void *)(IOMMU_VAUN6L_GSP1)},

	{ .compatible = "unisoc,iommuvaun6l-dispc",
	  .data = (void *)(IOMMU_VAUN6L_DISP)},

	{ .compatible = "unisoc,iommuvaun6l-dispc1",
	  .data = (void *)(IOMMU_VAUN6L_DISP1)},

	{ .compatible = "unisoc,iommuvaun6l-vsp",
	  .data = (void *)(IOMMU_VAUN6L_VSP)},

	{ .compatible = "unisoc,iommuvaun6l-vsp1",
	  .data = (void *)(IOMMU_VAUN6L_VSP1)},

	{ .compatible = "unisoc,iommuvaun6l-vsp2",
	  .data = (void *)(IOMMU_VAUN6L_VSP2)},

	{ .compatible = "unisoc,iommuvaun6l-dcam",
	  .data = (void *)(IOMMU_VAUN6L_DCAM)},

	{ .compatible = "unisoc,iommuvaun6l-dcam1",
	  .data = (void *)(IOMMU_VAUN6L_DCAM1)},

	{ .compatible = "unisoc,iommuvaun6l-isp",
	  .data = (void *)(IOMMU_VAUN6L_ISP)},

	{ .compatible = "unisoc,iommuvaun6l-cpp",
	  .data = (void *)(IOMMU_VAUN6L_CPP)},

	{ .compatible = "unisoc,iommuvaun6l-jpg",
	  .data = (void *)(IOMMU_VAUN6L_JPG)},

	{ .compatible = "unisoc,iommuvaun6l-fd",
	  .data = (void *)(IOMMU_VAUN6L_FD)},

	{ .compatible = "unisoc,iommuvaun6l-ai",
	  .data = (void *)(IOMMU_VAUN6L_AI)},

	{ .compatible = "unisoc,iommuvaun6l-epp",
	  .data = (void *)(IOMMU_VAUN6L_EPP)},

	{ .compatible = "unisoc,iommuvaun6l-edp",
	  .data = (void *)(IOMMU_VAUN6L_EDP)},

	{ .compatible = "unisoc,iommuvaun6l-idma",
	  .data = (void *)(IOMMU_VAUN6L_IDMA)},

	{ .compatible = "unisoc,iommuvaun6l-vdma",
	  .data = (void *)(IOMMU_VAUN6L_VDMA)},

	{},
};

static void sprd_iommu_set_list(struct sprd_iommu_dev *iommu_dev)
{
	if (iommu_dev == NULL) {
		pr_err("%s, iommu_dev == NULL!\n", __func__);
		return;
	}

	switch (iommu_dev->init_data->id) {
	case IOMMU_EX_VSP:
		sprd_iommu_list[SPRD_IOMMU_VSP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_VSP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP;
		break;
	case IOMMU_EX_VSP1:
		sprd_iommu_list[SPRD_IOMMU_VSP1].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_VSP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP1;
		break;
	case IOMMU_EX_VSP2:
		sprd_iommu_list[SPRD_IOMMU_VSP2].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_VSP2].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VSP2;
		break;
	case IOMMU_EX_DCAM:
		sprd_iommu_list[SPRD_IOMMU_DCAM].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_DCAM].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DCAM;
		break;
	case IOMMU_EX_DCAM1:
		sprd_iommu_list[SPRD_IOMMU_DCAM1].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_DCAM1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DCAM1;
		break;
	case IOMMU_EX_CPP:
		sprd_iommu_list[SPRD_IOMMU_CPP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_CPP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_CPP;
		break;
	case IOMMU_EX_GSP:
		sprd_iommu_list[SPRD_IOMMU_GSP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_GSP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_GSP;
		break;
	case IOMMU_EX_GSP1:
		sprd_iommu_list[SPRD_IOMMU_GSP1].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_GSP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_GSP1;
		break;
	case IOMMU_EX_JPG:
		sprd_iommu_list[SPRD_IOMMU_JPG].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_JPG].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_JPG;
		break;
	case IOMMU_EX_DISP:
		sprd_iommu_list[SPRD_IOMMU_DISP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_DISP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DISP;
		break;
	case IOMMU_EX_DISP1:
		sprd_iommu_list[SPRD_IOMMU_DISP1].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_DISP1].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_DISP1;
	break;
	case IOMMU_EX_ISP:
	case IOMMU_EX_NEWISP:
		sprd_iommu_list[SPRD_IOMMU_ISP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_ISP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_ISP;
		break;
	case IOMMU_EX_EPP:
		sprd_iommu_list[SPRD_IOMMU_EPP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_EPP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_EPP;
		break;
	case IOMMU_EX_EDP:
		sprd_iommu_list[SPRD_IOMMU_EDP].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_EDP].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_EDP;
		break;
	case IOMMU_EX_FD:
		sprd_iommu_list[SPRD_IOMMU_FD].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_FD].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_FD;
		break;
	case IOMMU_EX_IDMA:
		sprd_iommu_list[SPRD_IOMMU_IDMA].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_IDMA].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_IDMA;
		break;
	case IOMMU_EX_VDMA:
		sprd_iommu_list[SPRD_IOMMU_VDMA].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_VDMA].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_VDMA;
		break;

	case IOMMU_EX_AI:
		sprd_iommu_list[SPRD_IOMMU_AI].enabled = true;
		sprd_iommu_list[SPRD_IOMMU_AI].iommu_dev = iommu_dev;
		iommu_dev->id = SPRD_IOMMU_AI;
		break;
	default:
		pr_err("%s, no iommu id: %d\n", __func__,
			iommu_dev->init_data->id);
		iommu_dev->id = -1;
		break;
	}
}

static bool sprd_iommu_is_dev_valid_master(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	/*
	 * An iommu master has an iommus property containing a list of phandles
	 * to iommu nodes, each with an #iommu-cells property with value 0.
	 */
	ret = of_count_phandle_with_args(np, "iommus", "#iommu-cells");
	return (ret > 0);
}

static struct sprd_iommu_dev *sprd_iommu_get_subnode(struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", 0,
					 &args);
	if (ret) {
		IOMMU_ERR("of_parse_phandle_with_args(%s) => %d\n",
			np->full_name, ret);
		return NULL;
	}

	if (args.args_count != 0) {
		IOMMU_ERR("err param for %s (found %d, expected 0)\n",
			args.np->full_name, args.args_count);
		return NULL;
	}

	return args.np->data;
}

static int sprd_iommu_get_sg(void *buf, struct sg_table **table)
{
	struct system_heap_buffer *buffer;

	if (!buf) {
		pr_err("%s, buf==NULL", __func__);
		return -EINVAL;
	}

	buffer = (struct system_heap_buffer *)buf;
	*table = &buffer->sg_table;

	return 0;
}

static int sprd_iommu_carveout_get_sg(void *buf, struct sg_table **table)
{
	struct carveout_heap_buffer *buffer;

	if (!buf) {
		pr_err("%s, buf==NULL", __func__);
		return -EINVAL;
	}

	buffer = (struct carveout_heap_buffer *)buf;
	*table = buffer->sg_table;

	return 0;
}

static struct sprd_iommu_dev *sprd_iommu_get_subnode_with_idx(
		struct device *dev, int idx)
{
	struct device_node *np = dev->of_node;
	int ret;
	struct of_phandle_args args;

	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", idx,
					 &args);
	if (ret) {
		IOMMU_ERR("of_parse_phandle_with_args(%s) => %d\n",
			np->full_name, ret);
		return NULL;
	}

	if (args.args_count != 0) {
		IOMMU_ERR("err param for %s (found %d, expected 0)\n",
			args.np->full_name, args.args_count);
		return NULL;
	}

	return args.np->data;
}

static bool sprd_iommu_target_buf(struct sprd_iommu_dev *iommu_dev,
						void *buf_addr,
						unsigned long *iova_addr)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].buf_addr == buf_addr) {
			*iova_addr = iommu_dev->sg_pool.slot[index].iova_addr;
			iommu_dev->sg_pool.slot[index].map_usrs++;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_target_iova_find_buf(struct sprd_iommu_dev *iommu_dev,
						unsigned long iova_addr,
						size_t iova_size,
						void **buf)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].iova_addr == iova_addr &&
		   iommu_dev->sg_pool.slot[index].iova_size == iova_size) {
			*buf = iommu_dev->sg_pool.slot[index].buf_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_target_buf_find_iova(struct sprd_iommu_dev *iommu_dev,
						void *buf_addr,
						size_t iova_size,
						unsigned long *iova_addr)
{
	int index;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED &&
		   iommu_dev->sg_pool.slot[index].buf_addr == buf_addr &&
		   iommu_dev->sg_pool.slot[index].iova_size == iova_size) {
			*iova_addr = iommu_dev->sg_pool.slot[index].iova_addr;
			break;
		}
	}
	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_insert_slot(struct sprd_iommu_dev *iommu_dev,
						unsigned long sg_table_addr,
						void *buf_addr,
						unsigned long iova_addr,
						unsigned long iova_size)
{
	int index = 0;
	struct sprd_iommu_sg_rec *rec;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		rec = &(iommu_dev->sg_pool.slot[index]);
		if (rec->status == SG_SLOT_FREE) {
			rec->sg_table_addr = sg_table_addr;
			rec->buf_addr = buf_addr;
			rec->iova_addr = iova_addr;
			rec->iova_size = iova_size;
			rec->status = SG_SLOT_USED;
			rec->map_usrs++;
			iommu_dev->sg_pool.pool_cnt++;
			break;
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

/**
* remove timeout sg table addr from sg pool
*/
static bool sprd_iommu_remove_sg_iova(struct sprd_iommu_dev *iommu_dev,
							unsigned long iova_addr,
							bool *be_free)
{
	int index = 0;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		if (iommu_dev->sg_pool.slot[index].status == SG_SLOT_USED) {
			if (iommu_dev->sg_pool.slot[index].iova_addr == iova_addr) {
				iommu_dev->sg_pool.slot[index].map_usrs--;

				if (iommu_dev->sg_pool.slot[index].map_usrs == 0) {
					iommu_dev->sg_pool.slot[index].sg_table_addr = 0;
					iommu_dev->sg_pool.slot[index].iova_addr = 0;
					iommu_dev->sg_pool.slot[index].iova_size = 0;
					iommu_dev->sg_pool.slot[index].status = SG_SLOT_FREE;
					iommu_dev->sg_pool.pool_cnt--;
					*be_free = true;
				} else
					*be_free = false;
				break;
			}
		}
	}

	if (index < SPRD_MAX_SG_CACHED_CNT)
		return true;
	else
		return false;
}

static bool sprd_iommu_clear_sg_iova(struct sprd_iommu_dev *iommu_dev,
			void *buf_addr,
			unsigned long sg_addr, unsigned long size,
			unsigned long *iova)
{
	int index;
	struct sprd_iommu_sg_rec *rec;
	bool ret = false;

	for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
		rec = &(iommu_dev->sg_pool.slot[index]);
		if (rec->status == SG_SLOT_USED &&
		    rec->buf_addr == buf_addr &&
		    rec->sg_table_addr == sg_addr &&
		    rec->iova_size == size) {
			rec->map_usrs = 0;
			rec->status = SG_SLOT_FREE;
			iommu_dev->sg_pool.pool_cnt--;
			*iova = rec->iova_addr;
			ret = true;
			break;
		}
	}

	return ret;
}

static void sprd_iommu_pool_show(struct sprd_iommu_dev *iommu_dev)
{
	int index;
	struct sprd_iommu_sg_rec *rec;

	IOMMU_DEBUG("%s restore, map_count %u\n",
			iommu_dev->init_data->name,
			iommu_dev->map_count);

	if (iommu_dev->map_count > 0)
		for (index = 0; index < SPRD_MAX_SG_CACHED_CNT; index++) {
			rec = &(iommu_dev->sg_pool.slot[index]);
			if (rec->status == SG_SLOT_USED) {
				IOMMU_DEBUG("buffer is not unmapped, iova 0x%lx "
					"size 0x%lx sg 0x%lx buf %p map_usrs %d\n",
					rec->iova_addr, rec->iova_size,
					rec->sg_table_addr, rec->buf_addr,
					rec->map_usrs);
			}
		}
}

int sprd_iommu_attach_device(struct device *dev)
{
	struct device_node *np = NULL;
	struct of_phandle_args args;
	const char *status;
	int ret;
	int err = -1;

	if (dev == NULL) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	np = dev->of_node;
	/*
	 * An iommu master has an iommus property containing a list of phandles
	 * to iommu nodes, each with an #iommu-cells property with value 0.
	 */
	ret = of_parse_phandle_with_args(np, "iommus", "#iommu-cells", 0,
					 &args);
	if (!ret) {
		ret = of_property_read_string(args.np, "status", &status);
		if (!ret) {
			if (!strcmp("disabled", status))
				err = -EPERM;
			else
				err = 0;
		} else
			err = -EPERM;
	} else
		err = -EPERM;

	return err;
}
EXPORT_SYMBOL(sprd_iommu_attach_device);

int sprd_iommu_dettach_device(struct device *dev)
{
	struct device_node *np = NULL;

	if (dev == NULL) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	np = dev->of_node;

	return 0;
}

int sprd_iommu_map(struct device *dev, struct sprd_iommu_map_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (data->buf == NULL) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_iommu_get_sg(data->buf, &table);
	if (ret || table == NULL) {
		IOMMU_ERR("%s get sg error, buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}


	/**search the sg_cache_pool to identify if buf already mapped;
	* if yes, return cached iova directly, otherwise, alloc new iova for it;
	*/
	buf_cached = sprd_iommu_target_buf(iommu_dev, data->buf,
						(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		if (iommu_dev->init_data->id == IOMMU_EX_VSP)
			IOMMU_ERR("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		else
			IOMMU_DEBUG("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, table, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_map);

int sprd_iommu_map_v2(struct device *dev, struct sprd_iommu_map_data *data,
		enum sprd_iommu_buftype buftype)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (data->buf == NULL) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	if (buftype == SPRD_IOMMU_BUFTYPE_CARVEOUT)
		ret = sprd_iommu_carveout_get_sg(data->buf, &table);
	else
		ret = sprd_iommu_get_sg(data->buf, &table);
	if (ret || table == NULL) {
		IOMMU_ERR("%s get sg error, buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}

	buf_cached = sprd_iommu_target_buf(iommu_dev, data->buf,
						(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		if (iommu_dev->init_data->id == IOMMU_EX_VSP)
			IOMMU_ERR("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		else
			IOMMU_DEBUG("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, table, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_map_v2);

int sprd_iommu_map_single_page(struct device *dev, struct sprd_iommu_map_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (data->buf == NULL) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_iommu_get_sg(data->buf, &table);
	if (ret || table == NULL) {
		IOMMU_ERR("%s get sg error, buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}

	/*
	 * search the sg_cache_pool to identify if buf already mapped;
	 * if yes, return cached iova directly, otherwise, alloc new iova for it;
	 */
	buf_cached = sprd_iommu_target_buf(iommu_dev, data->buf,
						(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		if (iommu_dev->init_data->id == IOMMU_EX_VSP)
			IOMMU_ERR("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		else
			IOMMU_DEBUG("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, NULL, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL_GPL(sprd_iommu_map_single_page);

int sprd_iommu_map_with_idx(
		struct device *dev,
		struct sprd_iommu_map_data *data, int idx)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	unsigned long iova = 0;
	unsigned long flag = 0;
	bool buf_cached = false;
	bool buf_insert = true;
	struct sg_table *table = NULL;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (data->buf == NULL) {
		IOMMU_ERR("null buf pointer!\n");
		return -EINVAL;
	}


	iommu_dev = sprd_iommu_get_subnode_with_idx(dev, idx);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev idx %d\n", idx);
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	ret = sprd_iommu_get_sg(data->buf, &table);
	if (ret || table == NULL) {
		IOMMU_ERR("%s sg error,buf %p size 0x%zx ret %d table %p\n",
			  iommu_dev->init_data->name,
			  data->buf, data->iova_size, ret, table);
		data->iova_addr = 0;
		ret = -EINVAL;
		goto out;
	}


	/**search the sg_cache_pool to identify if buf already mapped;*/
	/* if yes, return cached iova directly, otherwise, */
	/* alloc new iova for it;*/
	buf_cached = sprd_iommu_target_buf(iommu_dev, data->buf,
						(unsigned long *)&iova);
	if (buf_cached) {
		data->iova_addr = iova;
		ret = 0;
		if (iommu_dev->init_data->id == IOMMU_EX_VSP)
			IOMMU_ERR("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		else
			IOMMU_DEBUG("%s use old mapping, iova 0x%lx size 0x%zx buf %p\n",
				  iommu_dev->init_data->name, iova,
				  data->iova_size, data->buf);
		goto out;
	}

	/*new sg, alloc for it*/
	iova = iommu_dev->ops->iova_alloc(iommu_dev,
			data->iova_size, data);
	if (iova == 0) {
		IOMMU_ERR("%s alloc error iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name, iova,
			  data->iova_size, data->buf);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}

	ret = iommu_dev->ops->iova_map(iommu_dev,
			iova, data->iova_size, table, data);
	if (ret) {
		IOMMU_ERR("%s error, iova 0x%lx size 0x%zx ret %d buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, ret, data->buf);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		data->iova_addr = 0;
		ret = -ENOMEM;
		goto out;
	}
	iommu_dev->map_count++;
	data->iova_addr = iova;
	buf_insert = sprd_iommu_insert_slot(iommu_dev,
				(unsigned long)table,
				data->buf,
				data->iova_addr,
				data->iova_size);
	if (!buf_insert) {
		IOMMU_ERR("%s error pool full iova 0x%lx size 0x%zx buf %p\n",
				iommu_dev->init_data->name, iova,
				data->iova_size, data->buf);
		iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
		ret = -ENOMEM;
		goto out;
	}

	IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
		  iommu_dev->init_data->name, iova,
		  data->iova_size, data->buf);

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_map_with_idx);

int sprd_iommu_unmap(struct device *dev, struct sprd_iommu_unmap_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	bool be_free = false;
	unsigned long flag = 0;
	bool valid_iova = false;
	bool valid_buf = false;
	void *buf;
	unsigned long iova;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	/*check iova legal*/
	valid_iova = sprd_iommu_target_iova_find_buf(iommu_dev,
				data->iova_addr, data->iova_size, &buf);
	if (valid_iova) {
		iova = data->iova_addr;
	} else {
		valid_buf = sprd_iommu_target_buf_find_iova(iommu_dev,
					data->buf,
					data->iova_size, &iova);
		if (valid_buf) {
			buf = data->buf;
			data->iova_addr = iova;
		} else {
			IOMMU_ERR("%s illegal error iova 0x%lx buf %p size 0x%zx\n",
				iommu_dev->init_data->name,
				data->iova_addr, data->buf,
				data->iova_size);
			ret = -EINVAL;
			goto out;
		}
	}

	sprd_iommu_remove_sg_iova(iommu_dev, iova, &be_free);
	if (be_free) {
		iommu_dev->ch_type = data->ch_type;
		iommu_dev->channel_id = data->channel_id;
		ret = iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		if (ret)
			IOMMU_ERR("%s error iova 0x%lx 0x%zx buf %p\n",
				iommu_dev->init_data->name,
				iova, data->iova_size, buf);
		iommu_dev->map_count--;
		iommu_dev->ops->iova_free(iommu_dev,
			iova, data->iova_size);
		iommu_dev->ch_type = SPRD_IOMMU_CH_TYPE_INVALID;
		IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
	} else {
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
		ret = 0;
	}

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_unmap);

int sprd_iommu_unmap_with_idx(
		struct device *dev,
		struct sprd_iommu_unmap_data *data, int idx)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev = NULL;
	bool be_free = false;
	unsigned long flag = 0;
	bool valid_iova = false;
	bool valid_buf = false;
	void *buf;
	unsigned long iova;

	if (dev == NULL || data == NULL) {
		IOMMU_ERR("null parameter err! dev %p data %p\n", dev, data);
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode_with_idx(dev, idx);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev idx %d\n", idx);
		return -EINVAL;
	}

	spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

	/*check iova legal*/
	valid_iova = sprd_iommu_target_iova_find_buf(iommu_dev,
				data->iova_addr, data->iova_size, &buf);
	if (valid_iova) {
		iova = data->iova_addr;
	} else {
		valid_buf = sprd_iommu_target_buf_find_iova(iommu_dev,
					data->buf,
					data->iova_size, &iova);
		if (valid_buf) {
			buf = data->buf;
			data->iova_addr = iova;
		} else {
			IOMMU_ERR("%s illegal error iova 0x%lx buf %p size 0x%zx\n",
				iommu_dev->init_data->name,
				data->iova_addr, data->buf,
				data->iova_size);
			ret = -EINVAL;
			goto out;
		}
	}

	sprd_iommu_remove_sg_iova(iommu_dev, iova, &be_free);
	if (be_free) {
		iommu_dev->ch_type = data->ch_type;
		iommu_dev->channel_id = data->channel_id;
		ret = iommu_dev->ops->iova_unmap(iommu_dev,
				iova, data->iova_size);
		if (ret)
			IOMMU_ERR("%s error iova 0x%lx 0x%zx buf %p\n",
				iommu_dev->init_data->name,
				iova, data->iova_size, buf);
		iommu_dev->map_count--;
		iommu_dev->ops->iova_free(iommu_dev,
			iova, data->iova_size);
		iommu_dev->ch_type = SPRD_IOMMU_CH_TYPE_INVALID;
		IOMMU_DEBUG("%s iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
	} else {
		IOMMU_DEBUG("%s cached iova 0x%lx size 0x%zx buf %p\n",
			  iommu_dev->init_data->name,
			  iova, data->iova_size, buf);
		ret = 0;
	}

out:
	spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_unmap_with_idx);

int sprd_iommu_unmap_orphaned(struct sprd_iommu_unmap_data *data)
{
	int ret = 0;
	struct sprd_iommu_dev *iommu_dev;
	unsigned long iova;
	unsigned long flag = 0;
	int i;

	if (data == NULL) {
		IOMMU_ERR("null parameter error! data %p\n", data);
		return -EINVAL;
	}

	for (i = 0; i < SPRD_IOMMU_MAX; i++) {
		iommu_dev = sprd_iommu_list[i].iommu_dev;
		if (!iommu_dev)
			continue;

		spin_lock_irqsave(&iommu_dev->pgt_lock, flag);

		ret = sprd_iommu_clear_sg_iova(iommu_dev, data->buf,
						(unsigned long)(data->table),
						data->iova_size, &iova);
		if (ret) {
			ret = iommu_dev->ops->iova_unmap_orphaned(iommu_dev,
							 iova, data->iova_size);
			iommu_dev->map_count--;
			iommu_dev->ops->iova_free(iommu_dev, iova, data->iova_size);
			IOMMU_ERR("%s iova leak error, buf %p id %d iova 0x%lx size 0x%zx\n",
				iommu_dev->init_data->name, data->buf, data->dev_id,
				iova, data->iova_size);
		}

		spin_unlock_irqrestore(&iommu_dev->pgt_lock, flag);
	}

	return ret;
}

static int sprd_iommu_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct sprd_iommu_unmap_data unmap_data = {0};

	unmap_data.buf = data;
	unmap_data.table = &((struct system_heap_buffer *)data)->sg_table;
	unmap_data.iova_size = ((struct system_heap_buffer *)data)->len;
	unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
	sprd_iommu_unmap_orphaned(&unmap_data);

	return NOTIFY_OK;
}

int sprd_iommu_suspend(struct device *dev)
{
	struct sprd_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (dev == NULL) {
		IOMMU_ERR("null parameter err\n");
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	mutex_lock(&iommu_dev->status_mutex);
	if (iommu_dev->status_count) {
		iommu_dev->ops->suspend(iommu_dev);
		iommu_dev->status_count--;
	} else
		IOMMU_ERR("%s have not enable!\n", iommu_dev->init_data->name);

	mutex_unlock(&iommu_dev->status_mutex);
	return ret;
}

int sprd_iommu_resume(struct device *dev)
{
	struct sprd_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (dev == NULL) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	mutex_lock(&iommu_dev->status_mutex);
	iommu_dev->ops->resume(iommu_dev);
	iommu_dev->status_count++;
	mutex_unlock(&iommu_dev->status_mutex);

	return ret;
}

int sprd_iommu_restore(struct device *dev)
{
	struct sprd_iommu_dev *iommu_dev = NULL;
	int ret = 0;

	if (dev == NULL) {
		IOMMU_ERR("null parameter err!\n");
		return -EINVAL;
	}

	if (!sprd_iommu_is_dev_valid_master(dev)) {
		IOMMU_ERR("illegal master\n");
		return -EINVAL;
	}

	iommu_dev = sprd_iommu_get_subnode(dev);
	if (iommu_dev == NULL) {
		IOMMU_ERR("get null iommu dev\n");
		return -EINVAL;
	}

	if (iommu_dev->ops->restore)
		iommu_dev->ops->restore(iommu_dev);
	else
		ret = -1;

	sprd_iommu_pool_show(iommu_dev);
	return ret;
}
EXPORT_SYMBOL(sprd_iommu_restore);

int sprd_iommu_set_cam_bypass(bool vaor_bp_en)
{
	struct sprd_iommu_dev *iommu_dev;
	int ret = 0;

	iommu_dev = sprd_iommu_list[SPRD_IOMMU_DCAM].iommu_dev;
	if (iommu_dev->ops->set_bypass)
		iommu_dev->ops->set_bypass(iommu_dev, vaor_bp_en);
	else
		ret = -1;

	iommu_dev = sprd_iommu_list[SPRD_IOMMU_ISP].iommu_dev;
	if (!strcmp(iommu_dev->init_data->name, "unisoc,iommuexpk2-isp"))
		return ret;
	if (iommu_dev->ops->set_bypass)
		iommu_dev->ops->set_bypass(iommu_dev, vaor_bp_en);
	else
		ret = -1;

	return ret;
}
EXPORT_SYMBOL(sprd_iommu_set_cam_bypass);

static int sprd_iommu_get_resource(struct device_node *np,
				struct sprd_iommu_init_data *pdata)
{
	int err = 0;
	uint32_t val;
	struct resource res;
	struct device_node *np_memory;
	uint32_t out_values[4];

	err = of_address_to_resource(np, 0, &res);
	if (err < 0)
		return err;

	IOMMU_INFO("pgt_base phy:0x%lx\n", (unsigned long)(res.start));
	pdata->pgt_base = (unsigned long)ioremap(res.start,
		resource_size(&res));
	BUG_ON(pdata->pgt_base == 0);
	/*sharkl2 pgt_size is va range*/
	pdata->pgt_size = resource_size(&res);
	IOMMU_INFO("pgt_base:%lx,pgt_size:%zx\n", pdata->pgt_base,
		pdata->pgt_size);

	err = of_address_to_resource(np, 1, &res);
	if (err < 0)
		goto err_pgt_base;

	/*sharkl2 ctrl_reg is iommu base reg addr*/
	IOMMU_INFO("ctrl_reg phy:0x%lx\n", (unsigned long)(res.start));
	pdata->ctrl_reg = (unsigned long)ioremap(res.start,
		resource_size(&res));
	BUG_ON(!pdata->ctrl_reg);
	IOMMU_INFO("ctrl_reg:0x%lx\n", pdata->ctrl_reg);

	err = of_property_read_u32(np, "iova-base", &val);
	if (err < 0)
		goto err_ctrl_reg;

	pdata->iova_base = val;
	err = of_property_read_u32(np, "iova-size", &val);
	if (err < 0)
		goto err_ctrl_reg;

	pdata->iova_size = val;

	err = of_property_read_u32(np, "phys-offset", &val);
	if (!err) {
		IOMMU_INFO("phys-offset: 0x%x\n", val);
		pdata->phys_offset = val;
	}
	IOMMU_INFO("iova_base:0x%lx,iova_size:%zx\n",
		pdata->iova_base,
		pdata->iova_size);

	err = of_property_read_u32(np, "sprd,reserved-fault-page", &val);
	if (err) {
		unsigned long page =  0;

		page = __get_free_page(GFP_KERNEL);
		if (page)
			pdata->fault_page = virt_to_phys((void *)page);
		else
			pdata->fault_page = 0;

		IOMMU_INFO("fault_page: 0x%lx\n", pdata->fault_page);
	} else {
		IOMMU_INFO("reserved fault page phy:0x%x\n", val);
		pdata->fault_page = val;
	}

	err = of_property_read_u32(np, "sprd,reserved-rr-page", &val);
	if (err) {
		IOMMU_INFO("no reserved rr page addr\n");
		pdata->re_route_page = 0;
	} else {
		IOMMU_INFO("reserved rr page phy:0x%x\n", val);
		pdata->re_route_page = val;
	}

	/*get mmu page table reserved memory*/
	np_memory = of_parse_phandle(np, "memory-region", 0);
	if (!np_memory) {
		pdata->pagt_base_ddr = 0;
		pdata->pagt_ddr_size = 0;
	} else {
#ifdef CONFIG_64BIT
		err = of_property_read_u32_array(np_memory, "reg",
				out_values, 4);
		if (!err) {
			pdata->pagt_base_ddr = out_values[0];
			pdata->pagt_base_ddr =
					pdata->pagt_base_ddr << 32;
			pdata->pagt_base_ddr |= out_values[1];

			pdata->pagt_ddr_size = out_values[2];
			pdata->pagt_ddr_size =
					pdata->pagt_ddr_size << 32;
			pdata->pagt_ddr_size |= out_values[3];
		} else {
			pdata->pagt_base_ddr = 0;
			pdata->pagt_ddr_size = 0;
		}
#else
		err = of_property_read_u32_array(np_memory, "reg",
				out_values, 2);
		if (!err) {
			pdata->pagt_base_ddr = out_values[0];
			pdata->pagt_ddr_size = out_values[1];
		} else {
			pdata->pagt_base_ddr = 0;
			pdata->pagt_ddr_size = 0;
		}
#endif
	}

	return 0;

err_ctrl_reg:
	iounmap((void __iomem *)pdata->ctrl_reg);

err_pgt_base:
	iounmap((void __iomem *)pdata->pgt_base);

	return err;
}

static void sprd_iommu_put_resource(struct device_node *np,
				struct sprd_iommu_init_data *pdata)
{
	int err = 0;
	uint32_t val;

	err = of_property_read_u32(np, "sprd,reserved-fault-page", &val);
	if (err && pdata->fault_page)
		__free_page(phys_to_virt(pdata->fault_page));

	if (pdata->ctrl_reg)
		iounmap((void __iomem *)pdata->ctrl_reg);

	if (pdata->pgt_base)
		iounmap((void __iomem *)pdata->pgt_base);
}

static int sprd_iommu_pt_cma_alloc(struct sprd_iommu_dev *iommu_dev)
{
	void *iommu_pt_addr_v = NULL;
	dma_addr_t iommu_pt_addr_p;
	struct sprd_iommu_init_data *pdata = iommu_dev->init_data;
	size_t iommu_pt_size = pdata->iova_size / MMU_MAPING_PAGESIZE * 4;

	iommu_pt_addr_v = dma_alloc_wc(iommu_dev->drv_dev,
			iommu_pt_size, &iommu_pt_addr_p, GFP_KERNEL);
	if (!iommu_pt_addr_v)
		return -ENOMEM;

	pdata->pagt_base_virt = (unsigned long)iommu_pt_addr_v;
	pdata->pagt_base_ddr = (unsigned long)iommu_pt_addr_p;
	pdata->pagt_ddr_size = (unsigned long)iommu_pt_size;

	IOMMU_INFO("%s pagetable: virt[0x%lx], phy[0x%lx], size:0x%lx\n",
			pdata->name,
			pdata->pagt_base_virt,
			pdata->pagt_base_ddr,
			pdata->pagt_ddr_size);

	return 0;
}

static void sprd_iommu_pt_cma_free(struct sprd_iommu_dev *iommu_dev)
{
	struct sprd_iommu_init_data *pdata = iommu_dev->init_data;

	if (pdata->pagt_base_virt)
		return dma_free_wc(iommu_dev->drv_dev,
				(size_t)pdata->pagt_ddr_size,
				(void *)pdata->pagt_base_virt,
				(dma_addr_t)pdata->pagt_base_ddr);
}

static int sprd_iommu_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_iommu_dev *iommu_dev = NULL;
	struct sprd_iommu_init_data *pdata = NULL;
	static bool probe_first_time = true;

	IOMMU_INFO("start\n");

	iommu_dev = kzalloc(sizeof(struct sprd_iommu_dev), GFP_KERNEL);
	if (iommu_dev == NULL) {
		IOMMU_ERR("fail to kzalloc\n");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct sprd_iommu_init_data), GFP_KERNEL);
	if (pdata == NULL) {
		IOMMU_ERR("fail to kzalloc\n");
		kfree(iommu_dev);
		return -ENOMEM;
	}

	pdata->id = (int)((enum IOMMU_ID)
		((of_match_node(sprd_iommu_ids, np))->data));

	switch (pdata->id) {
	/*for sharkle iommu*/
	case IOMMU_EXLE_VSP:
	case IOMMU_EXLE_DCAM:
	case IOMMU_EXLE_CPP:
	case IOMMU_EXLE_GSP:
	case IOMMU_EXLE_JPG:
	case IOMMU_EXLE_DISP:
	case IOMMU_EXLE_ISP:
	{
		pdata->iommuex_rev = 7;
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		if (pdata->id == IOMMU_EXLE_GSP)
			pdata->id = IOMMU_EX_GSP;
		else if (pdata->id == IOMMU_EXLE_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_EXLE_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_EXLE_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_EXLE_ISP)
			pdata->id = IOMMU_EX_ISP;
		else if (pdata->id == IOMMU_EXLE_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_EXLE_JPG)
			pdata->id = IOMMU_EX_JPG;

		break;
	}
	/*for pike2 iommu*/
	case IOMMU_EXPK2_VSP:
	case IOMMU_EXPK2_DCAM:
	case IOMMU_EXPK2_CPP:
	case IOMMU_EXPK2_GSP:
	case IOMMU_EXPK2_JPG:
	case IOMMU_EXPK2_DISP:
	case IOMMU_EXPK2_ISP:
	{
		pdata->iommuex_rev = 8;
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		if (pdata->id == IOMMU_EXPK2_GSP)
			pdata->id = IOMMU_EX_GSP;
		else if (pdata->id == IOMMU_EXPK2_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_EXPK2_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_EXPK2_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_EXPK2_ISP)
			pdata->id = IOMMU_EX_ISP;
		else if (pdata->id == IOMMU_EXPK2_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_EXPK2_JPG)
			pdata->id = IOMMU_EX_JPG;

		break;
	}
	/*for sharkl3 iommu*/
	case IOMMU_EXL3_VSP:
	case IOMMU_EXL3_DCAM:
	case IOMMU_EXL3_CPP:
	case IOMMU_EXL3_JPG:
	case IOMMU_EXL3_DISP:
	case IOMMU_EXL3_ISP:
	{
		pdata->iommuex_rev = 9;
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		if (pdata->id == IOMMU_EXL3_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_EXL3_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_EXL3_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_EXL3_ISP)
			pdata->id = IOMMU_EX_ISP;
		else if (pdata->id == IOMMU_EXL3_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_EXL3_JPG)
			pdata->id = IOMMU_EX_JPG;

		break;
	}
	/*for sharkl5 iommu*/
	case IOMMU_EXL5_VSP:
	case IOMMU_EXL5_DCAM:
	case IOMMU_EXL5_CPP:
	case IOMMU_EXL5_JPG:
	case IOMMU_EXL5_DISP:
	case IOMMU_EXL5_ISP:
	case IOMMU_EXL5_FD:
	{
		pdata->iommuex_rev = 10;
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		if (pdata->id == IOMMU_EXL5_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_EXL5_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_EXL5_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_EXL5_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_EXL5_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_EXL5_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_EXL5_FD)
			pdata->id = IOMMU_EX_FD;


		break;
	}
	/*for roc1 iommu*/
	case IOMMU_EXROC1_VSP:
	case IOMMU_EXROC1_DCAM:
	case IOMMU_EXROC1_CPP:
	case IOMMU_EXROC1_JPG:
	case IOMMU_EXROC1_DISP:
	case IOMMU_EXROC1_ISP:
	case IOMMU_EXROC1_FD:
	case IOMMU_EXROC1_AI:
	case IOMMU_EXROC1_EPP:
	case IOMMU_EXROC1_EDP:
	{
		pdata->iommuex_rev = 11;
		iommu_dev->ops = &sprd_iommuex_hw_ops;
		if (pdata->id == IOMMU_EXROC1_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_EXROC1_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_EXROC1_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_EXROC1_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_EXROC1_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_EXROC1_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_EXROC1_FD)
			pdata->id = IOMMU_EX_FD;
		else if (pdata->id == IOMMU_EXROC1_AI)
			pdata->id = IOMMU_EX_AI;
		else if (pdata->id == IOMMU_EXROC1_EPP)
			pdata->id = IOMMU_EX_EPP;
		else if (pdata->id == IOMMU_EXROC1_EDP)
			pdata->id = IOMMU_EX_EDP;

		break;
	}
	/*for roc1 iommu*/
	case IOMMU_VAUL5P_VSP:
	case IOMMU_VAUL5P_DCAM:
	case IOMMU_VAUL5P_CPP:
	case IOMMU_VAUL5P_JPG:
	case IOMMU_VAUL5P_DISP:
	case IOMMU_VAUL5P_ISP:
	case IOMMU_VAUL5P_FD:
	case IOMMU_VAUL5P_AI:
	case IOMMU_VAUL5P_EPP:
	case IOMMU_VAUL5P_EDP:
	case IOMMU_VAUL5P_IDMA:
	case IOMMU_VAUL5P_VDMA:
	{
		pdata->iommuex_rev = 12;
		iommu_dev->ops = &sprd_iommuvau_hw_ops;
		if (pdata->id == IOMMU_VAUL5P_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_VAUL5P_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_VAUL5P_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_VAUL5P_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_VAUL5P_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_VAUL5P_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_VAUL5P_FD)
			pdata->id = IOMMU_EX_FD;
		else if (pdata->id == IOMMU_VAUL5P_AI)
			pdata->id = IOMMU_EX_AI;
		else if (pdata->id == IOMMU_VAUL5P_EPP)
			pdata->id = IOMMU_EX_EPP;
		else if (pdata->id == IOMMU_VAUL5P_EDP)
			pdata->id = IOMMU_EX_EDP;
		else if (pdata->id == IOMMU_VAUL5P_IDMA)
			pdata->id = IOMMU_EX_IDMA;
		else if (pdata->id == IOMMU_VAUL5P_VDMA)
			pdata->id = IOMMU_EX_VDMA;


		break;
	}
	case IOMMU_VAUL6_VSP:
	case IOMMU_VAUL6_DCAM:
	case IOMMU_VAUL6_CPP:
	case IOMMU_VAUL6_JPG:
	case IOMMU_VAUL6_DISP:
	case IOMMU_VAUL6_ISP:
	case IOMMU_VAUL6_FD:
	{
		pdata->iommuex_rev = 13;
		iommu_dev->ops = &sprd_iommuvau_hw_ops;
		if (pdata->id == IOMMU_VAUL6_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_VAUL6_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_VAUL6_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_VAUL6_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_VAUL6_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_VAUL6_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_VAUL6_FD)
			pdata->id = IOMMU_EX_FD;
		break;
	}
	case IOMMU_VAUL6P_GSP:
	case IOMMU_VAUL6P_GSP1:
	case IOMMU_VAUL6P_VSP:
	case IOMMU_VAUL6P_VSP1:
	case IOMMU_VAUL6P_VSP2:
	case IOMMU_VAUL6P_DCAM:
	case IOMMU_VAUL6P_DCAM1:
	case IOMMU_VAUL6P_CPP:
	case IOMMU_VAUL6P_JPG:
	case IOMMU_VAUL6P_DISP:
	case IOMMU_VAUL6P_DISP1:
	case IOMMU_VAUL6P_ISP:
	case IOMMU_VAUL6P_FD:
	case IOMMU_VAUL6P_AI:
	case IOMMU_VAUL6P_EPP:
	case IOMMU_VAUL6P_EDP:
	case IOMMU_VAUL6P_IDMA:
	case IOMMU_VAUL6P_VDMA:
	{
		pdata->iommuex_rev = 14;
		iommu_dev->ops = &sprd_iommuvau_hw_ops;
		if (pdata->id == IOMMU_VAUL6P_GSP)
			pdata->id = IOMMU_EX_GSP;
		else if (pdata->id == IOMMU_VAUL6P_GSP1)
			pdata->id = IOMMU_EX_GSP1;
		else if (pdata->id == IOMMU_VAUL6P_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_VAUL6P_DISP1)
			pdata->id = IOMMU_EX_DISP1;
		else if (pdata->id == IOMMU_VAUL6P_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_VAUL6P_VSP1)
			pdata->id = IOMMU_EX_VSP1;
		else if (pdata->id == IOMMU_VAUL6P_VSP2)
			pdata->id = IOMMU_EX_VSP2;
		else if (pdata->id == IOMMU_VAUL6P_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_VAUL6P_DCAM1)
			pdata->id = IOMMU_EX_DCAM1;
		else if (pdata->id == IOMMU_VAUL6P_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_VAUL6P_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_VAUL6P_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_VAUL6P_FD)
			pdata->id = IOMMU_EX_FD;
		else if (pdata->id == IOMMU_VAUL6P_AI)
			pdata->id = IOMMU_EX_AI;
		else if (pdata->id == IOMMU_VAUL6P_EPP)
			pdata->id = IOMMU_EX_EPP;
		else if (pdata->id == IOMMU_VAUL6P_EDP)
			pdata->id = IOMMU_EX_EDP;
		else if (pdata->id == IOMMU_VAUL6P_IDMA)
			pdata->id = IOMMU_EX_IDMA;
		else if (pdata->id == IOMMU_VAUL6P_VDMA)
			pdata->id = IOMMU_EX_VDMA;
		break;
	}
	/* QogirN6Lite */
	case IOMMU_VAUN6L_GSP:
	case IOMMU_VAUN6L_GSP1:
	case IOMMU_VAUN6L_VSP:
	case IOMMU_VAUN6L_VSP1:
	case IOMMU_VAUN6L_VSP2:
	case IOMMU_VAUN6L_DCAM:
	case IOMMU_VAUN6L_DCAM1:
	case IOMMU_VAUN6L_CPP:
	case IOMMU_VAUN6L_JPG:
	case IOMMU_VAUN6L_DISP:
	case IOMMU_VAUN6L_DISP1:
	case IOMMU_VAUN6L_ISP:
	case IOMMU_VAUN6L_FD:
	case IOMMU_VAUN6L_AI:
	case IOMMU_VAUN6L_EPP:
	case IOMMU_VAUN6L_EDP:
	case IOMMU_VAUN6L_IDMA:
	case IOMMU_VAUN6L_VDMA:
	{
		pdata->iommuex_rev = 15;
		iommu_dev->ops = &sprd_iommuvau_hw_ops;
		if (pdata->id == IOMMU_VAUN6L_GSP)
			pdata->id = IOMMU_EX_GSP;
		else if (pdata->id == IOMMU_VAUN6L_GSP1)
			pdata->id = IOMMU_EX_GSP1;
		else if (pdata->id == IOMMU_VAUN6L_DISP)
			pdata->id = IOMMU_EX_DISP;
		else if (pdata->id == IOMMU_VAUN6L_DISP1)
			pdata->id = IOMMU_EX_DISP1;
		else if (pdata->id == IOMMU_VAUN6L_VSP)
			pdata->id = IOMMU_EX_VSP;
		else if (pdata->id == IOMMU_VAUN6L_VSP1)
			pdata->id = IOMMU_EX_VSP1;
		else if (pdata->id == IOMMU_VAUN6L_VSP2)
			pdata->id = IOMMU_EX_VSP2;
		else if (pdata->id == IOMMU_VAUN6L_DCAM)
			pdata->id = IOMMU_EX_DCAM;
		else if (pdata->id == IOMMU_VAUN6L_DCAM1)
			pdata->id = IOMMU_EX_DCAM1;
		else if (pdata->id == IOMMU_VAUN6L_ISP)
			pdata->id = IOMMU_EX_NEWISP;
		else if (pdata->id == IOMMU_VAUN6L_CPP)
			pdata->id = IOMMU_EX_CPP;
		else if (pdata->id == IOMMU_VAUN6L_JPG)
			pdata->id = IOMMU_EX_JPG;
		else if (pdata->id == IOMMU_VAUN6L_FD)
			pdata->id = IOMMU_EX_FD;
		else if (pdata->id == IOMMU_VAUN6L_AI)
			pdata->id = IOMMU_EX_AI;
		else if (pdata->id == IOMMU_VAUN6L_EPP)
			pdata->id = IOMMU_EX_EPP;
		else if (pdata->id == IOMMU_VAUN6L_EDP)
			pdata->id = IOMMU_EX_EDP;
		else if (pdata->id == IOMMU_VAUN6L_IDMA)
			pdata->id = IOMMU_EX_IDMA;
		else if (pdata->id == IOMMU_VAUN6L_VDMA)
			pdata->id = IOMMU_EX_VDMA;
		break;
	}
	default:
	{
		IOMMU_ERR("unknown iommu id %d\n", pdata->id);
		err = -ENOMEM;
		goto errout;
	}
	}

	err = sprd_iommu_get_resource(np, pdata);
	if (err) {
		IOMMU_ERR("no reg of property specified\n");
		goto errout;
	}

	/*If ddr frequency >= 500Mz, iommu need enable div2 frequency,
	 * because of limit of iommu iram frq.*/
	iommu_dev->div2_frq = 500;
	iommu_dev->light_sleep_en = false;
	iommu_dev->drv_dev = &pdev->dev;
	iommu_dev->init_data = pdata;
	iommu_dev->map_count = 0;
	iommu_dev->status_count = 0;
	iommu_dev->ch_type = SPRD_IOMMU_CH_TYPE_INVALID;
	iommu_dev->channel_id = 0;
	iommu_dev->init_data->name = (char *)(
		(of_match_node(sprd_iommu_ids, np))->compatible
		);

	err = sprd_iommu_pt_cma_alloc(iommu_dev);
	if (err) {
		IOMMU_ERR("%s: Failed to get pagetable memory\n", pdata->name);
		goto err_alloc;
	}

	err = iommu_dev->ops->init(iommu_dev, pdata);
	if (err) {
		IOMMU_ERR("iommu %s : failed init %d.\n", pdata->name, err);
		goto errpt;
	}

	atomic_set(&iommu_dev->iommu_dev_cnt, 0);
	spin_lock_init(&iommu_dev->pgt_lock);
	mutex_init(&iommu_dev->status_mutex);
	memset(&iommu_dev->sg_pool, 0, sizeof(struct sprd_iommu_sg_pool));
	sprd_iommu_sysfs_create(iommu_dev, iommu_dev->init_data->name);
	platform_set_drvdata(pdev, iommu_dev);

	np->data  = iommu_dev;
	sprd_iommu_set_list(iommu_dev);
	if (probe_first_time) {
		iommu_dev->sprd_iommu_nb.notifier_call = sprd_iommu_notify_callback;
		err = blocking_notifier_chain_register(&sprd_iommu_notif_chain,
					       &iommu_dev->sprd_iommu_nb);
		probe_first_time = false;
	}
	pm_runtime_enable(&pdev->dev);
	IOMMU_INFO("%s end\n", iommu_dev->init_data->name);
	return 0;


errpt:
	sprd_iommu_pt_cma_free(iommu_dev);
err_alloc:
	sprd_iommu_put_resource(np, pdata);
errout:
	kfree(iommu_dev);
	kfree(pdata);
	return err;
}

static int sprd_iommu_remove(struct platform_device *pdev)
{
	struct sprd_iommu_dev *iommu_dev = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;

	sprd_iommu_sysfs_destroy(iommu_dev, iommu_dev->init_data->name);
	iommu_dev->ops->exit(iommu_dev);
	sprd_iommu_pt_cma_free(iommu_dev);
	sprd_iommu_put_resource(np, iommu_dev->init_data);

	kfree(iommu_dev->init_data);
	iommu_dev->init_data = NULL;

	kfree(iommu_dev);
	return 0;
}

static struct platform_driver sprd_iommu_driver = {
	.probe = sprd_iommu_probe,
	.remove = sprd_iommu_remove,
	.driver = {
		.name = "sprd_iommu_drv",
		.of_match_table = sprd_iommu_ids,
	},
};

module_platform_driver(sprd_iommu_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc IOMMU Driver");
MODULE_AUTHOR("Erick Chen <sheng.xu@unisoc.com>");
