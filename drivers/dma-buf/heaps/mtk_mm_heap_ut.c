// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF mtk_mm heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 * Copyright (C) 2021 MediaTek Inc.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 */

#define pr_fmt(fmt) "[MTK_DMABUF_HEAP: UT] "fmt

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device/driver.h>
#include <linux/mod_devicetable.h>
#include <linux/sizes.h>
#include "deferred-free-helper.h"
#include <uapi/linux/dma-heap.h>
#include <linux/sched/clock.h>
#include <linux/of_device.h>

#include "mtk_heap_debug.h"

#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

struct proc_dir_entry *mtk_heap_proc_root;
#endif

const char *heap_name_list[] = {"mtk_mm", "mtk_mm-uncached",
				"mtk_svp_region-uncached",
				"mtk_prot_region-uncached"};

static struct platform_device *ut_iommu0_pdev;
static struct platform_device *ut_iommu1_pdev;


#if IS_ENABLED(CONFIG_PROC_FS)
#define DMABUF_T_DECLARE_NUM(ENUM) ENUM,
#define DMABUF_T_DECLARE_STR(ENUM) #ENUM,

#define DECLARE_DMABUF_T_CMD(EXPR)    \
	EXPR(DMABUF_T_ALLOC)          \
	EXPR(DMABUF_T_ALLOC_FD)       \
	EXPR(DMABUF_T_MAP_IOVA)       \
	EXPR(DMABUF_T_MAP_KVA)        \
	EXPR(DMABUF_T_CACHE)          \
	EXPR(DMABUF_T_END)            \
/* add one blank line */

#define dma_buf_dump(file, fmt, args...)               \
	do {                                           \
		if (file)                              \
			seq_printf(file, fmt, ##args); \
		else                                   \
			pr_info(fmt, ##args);          \
	} while(0)

enum DMA_HEAP_T_CMD { DECLARE_DMABUF_T_CMD(DMABUF_T_DECLARE_NUM) };
static const char *DMA_HEAP_T_CMD_STR[] = { DECLARE_DMABUF_T_CMD(DMABUF_T_DECLARE_STR) };

static void mtk_dmabuf_dump_heap(struct dma_heap *heap, struct seq_file *s) {
	struct mtk_heap_priv_info *heap_priv = NULL;

	if (!heap)
		return;
	heap_priv = mtk_heap_priv_get(heap);
	if (!heap_priv || !heap_priv->show)
		return;

	heap_priv->show(heap, s);
}

/* dump all heaps */
static void mtk_dmabuf_dump_all(struct seq_file *s) {
	int i;
	struct dma_heap *heap = NULL;

	for (i = 0; i < ARRAY_SIZE(heap_name_list); i++) {
		heap = dma_heap_find(heap_name_list[i]);
		if (!heap) {
			pr_info("couldn't find heap[%d]:%s\n",
				i, heap_name_list[i]);
			continue;
		}

		mtk_dmabuf_dump_heap(heap, s);
		dma_heap_put(heap);
	}
}


#define CMDLINE_LEN   (30)
static ssize_t dma_heap_proc_write(struct file *file, const char *buf,
			       size_t count, loff_t *data)
{
	struct dma_heap *heap = PDE_DATA(file->f_inode);
	char cmdline[CMDLINE_LEN];
	enum DMA_HEAP_T_CMD cmd = DMABUF_T_END;
	int ret = 0;
	int test_sz[] = {SZ_16M, SZ_128M};

	if (count >= CMDLINE_LEN)
		return -EINVAL;

	if (copy_from_user(cmdline, buf, count))
		return -EINVAL;

	cmdline[count] = 0;

	/* input str from cmd.exe will have \n, no need \n here */
	pr_info("%s #%d: heap_name:%s, set info:%s",
		__func__, __LINE__, dma_heap_get_name(heap),
		cmdline);

	if (strncmp(cmdline, "test:", strlen("test:"))) {
		pr_info("cmd error:%s\n", cmdline);
		return -EINVAL;
	}

	ret = sscanf(cmdline, "test:%d", &cmd);
	if (ret < 0) {
		pr_info("cmd error:%s\n", cmdline);
		return -EINVAL;
	}


	if (cmd < DMABUF_T_END)
		pr_info("%s: test case: %s start======\n",
			__func__, DMA_HEAP_T_CMD_STR[cmd]);

	switch (cmd) {
	case DMABUF_T_ALLOC:
	{
		struct dma_buf* dmabuf = NULL;
		int i;

		for (i = 0; i < ARRAY_SIZE(test_sz); i++) {
			dmabuf = dma_heap_buffer_alloc(heap,
					test_sz[i], 
					DMA_HEAP_VALID_FD_FLAGS,
					DMA_HEAP_VALID_HEAP_FLAGS);
			if (IS_ERR(dmabuf))
				continue;

			pr_info("alloc pass, sz:%d, heap:%s buf_exp_name:%s\n",
				test_sz[i],
				dma_heap_get_name(heap),
				dmabuf->exp_name);
			pr_info("dmabuf_show 1, %d\n", __LINE__);
			mtk_dmabuf_dump_all(NULL);
			dma_heap_buffer_free(dmabuf);
			pr_info("dmabuf_show 2, %d\n", __LINE__);
			mtk_dmabuf_dump_all(NULL);
		}
	}
	break;
	case DMABUF_T_ALLOC_FD:
	{
		struct dma_buf* dmabuf = NULL;
		int i, fd = 0;;

		for (i = 0; i < ARRAY_SIZE(test_sz); i++) {
			fd = dma_heap_bufferfd_alloc(heap,
					test_sz[i],
					DMA_HEAP_VALID_FD_FLAGS,
					DMA_HEAP_VALID_HEAP_FLAGS);
			if (fd < 0)
				continue;

			dmabuf = dma_buf_get(fd);
			if (IS_ERR(dmabuf))
				continue;

			pr_info("alloc pass, fd:%d sz:%d, heap:%s buf_exp_name:%s\n",
				fd, test_sz[i],
				dma_heap_get_name(heap),
				dmabuf->exp_name);
			pr_info("dmabuf_show 1, %d\n", __LINE__);
			mtk_dmabuf_dump_all(NULL);
			dma_heap_buffer_free(dmabuf);
			pr_info("dmabuf_show 2, %d\n", __LINE__);
			mtk_dmabuf_dump_all(NULL);
		}
	}
	break;
	case DMABUF_T_MAP_IOVA:
	{
		struct dma_buf* buf = NULL;
		int i = 0;
		struct dma_buf_attachment *a1, *a2;
		struct sg_table *sgt1, *sgt2;

		for (i = 0; i < ARRAY_SIZE(test_sz); i++) {
			buf = dma_heap_buffer_alloc(heap,
					test_sz[i], 
					DMA_HEAP_VALID_FD_FLAGS,
					DMA_HEAP_VALID_HEAP_FLAGS);
			if (IS_ERR(buf)) {
				pr_info("alloc fail, sz:%d, heap:%s buf_exp_name:%s\n",
					test_sz[i],
					dma_heap_get_name(heap),
					buf->exp_name);
				pr_info("dmabuf_show 1, %d\n", __LINE__);
				mtk_dmabuf_dump_all(NULL);
				return 0;
			}
			a1 = dma_buf_attach(buf, &ut_iommu0_pdev->dev);
			a2 = dma_buf_attach(buf, &ut_iommu1_pdev->dev);

			sgt1 = dma_buf_map_attachment(a1, DMA_BIDIRECTIONAL);
			pr_info("map1 done, 0x%lx\n", sgt1);
			pr_info("iova1: 0x%lx\n", sg_dma_address(sgt1->sgl));
			mtk_dmabuf_dump_all(NULL);

			sgt2 = dma_buf_map_attachment(a2, DMA_BIDIRECTIONAL);
			pr_info("map2 done 0x%lx\n", sgt2);
			pr_info("iova2: 0x%lx\n", sg_dma_address(sgt2->sgl));
			mtk_dmabuf_dump_all(NULL);

			dma_buf_unmap_attachment(a1, sgt1, DMA_BIDIRECTIONAL);
			pr_info("unmap iova1 done\n");
			mtk_dmabuf_dump_all(NULL);

			dma_buf_unmap_attachment(a2, sgt2, DMA_BIDIRECTIONAL);
			pr_info("unmap iova2 done\n");
			mtk_dmabuf_dump_all(NULL);

			dma_buf_detach(buf, a1);
			pr_info("detach a1\n");
			mtk_dmabuf_dump_all(NULL);

			dma_buf_detach(buf, a2);
			pr_info("detach a2\n");
			mtk_dmabuf_dump_all(NULL);

			dma_buf_put(buf);
			mtk_dmabuf_dump_all(NULL);
			
		}
	}
	break;

	case DMABUF_T_MAP_KVA:
	{
		
	}
	break;

	case DMABUF_T_CACHE:
	{
		
	}
	break;

	default:
		pr_info("error cmd:%d\n", cmd);
		break;
	}

	if (cmd < DMABUF_T_END)
		pr_info("%s: test case: end======\n",
			__func__, DMA_HEAP_T_CMD_STR[cmd]);


	return count;
}
#undef CMDLINE_LEN


static int dma_heap_proc_show(struct seq_file *s, void *v)
{
	struct dma_heap *heap;

	if (!s)
		return -EINVAL;

	heap = (struct dma_heap *)s->private;
	mtk_dmabuf_dump_heap(heap, s);
	return 0;
}

static int dma_heap_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_heap_proc_show, PDE_DATA(inode));
}

static const struct proc_ops dma_heap_proc_fops = {
	.proc_open = dma_heap_proc_open,
	.proc_read = seq_read,
	.proc_write = dma_heap_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int dma_buf_init_procfs(void)
{
	struct proc_dir_entry *proc_file;
	int i = 0;
	char dbg_name[30];
	struct dma_heap* heap;


	mtk_heap_proc_root = proc_mkdir("dma_heap_dbg", NULL);
	if (!mtk_heap_proc_root) {
		pr_info("%s failed to create procfs root dir.\n", __func__);
		return -1;
	}

	for (; i < ARRAY_SIZE(heap_name_list); i++) {
		heap = dma_heap_find(heap_name_list[i]);
		if (!heap)
			return -EINVAL;

		memset(dbg_name, 0, sizeof(dbg_name));
		scnprintf(dbg_name, sizeof(dbg_name), "%s-debug",
			  dma_heap_get_name(heap));

		proc_file = proc_create_data(dbg_name,
					     S_IFREG | 0664,
					     mtk_heap_proc_root,
					     &dma_heap_proc_fops,
					     heap);
		if (!proc_file) {
			pr_info("Failed to create %s\n",
				dbg_name);
			return -2;
		} else
			pr_info("create debug file for %s\n", dma_heap_get_name(heap));
	}

	return 0;
}

static void dma_buf_uninit_procfs(void)
{
	proc_remove(mtk_heap_proc_root);
}
#else
static inline int dma_buf_init_procfs(void)
{
	return 0;
}
static inline void dma_buf_uninit_procfs(void)
{
}
#endif


static const int ut0_data = 1;
static const int ut1_data = 2;

static int mtk_dma_heap_ut_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct dma_heap* heap;
	int i = 0;
	int *driver_data;

	pr_info("probe %s\n", dev_name(&pdev->dev));

	driver_data = (int *)of_device_get_match_data(&pdev->dev);
	if (*driver_data == ut0_data) {
		ut_iommu0_pdev = pdev;
		return 0;
	}

	ut_iommu1_pdev = pdev;
	/* find each dma_heap and bind to out ut device */

	for (i = 0; i < ARRAY_SIZE(heap_name_list); i++) {
		heap = dma_heap_find(heap_name_list[i]);
		if (!heap)
			return -EPROBE_DEFER;
	}


	ret = dma_buf_init_procfs();
	if (ret) {
		pr_info("%s fail\n", __func__);
		return ret;
	}
	return 0;
}

static int mtk_dma_heap_ut_remove(struct platform_device *pdev) {
	dma_buf_uninit_procfs();
	return 0;
}

static const struct of_device_id mtk_dmabufheap_iommu0_of_ids[] = {
	{
		.compatible = "mediatek, mtk_dmabufheap, iommu0",
		.data = &ut0_data,
	},
};

static const struct of_device_id mtk_dmabufheap_iommu1_of_ids[] = {
	{
		.compatible = "mediatek, mtk_dmabufheap, iommu1",
		.data = &ut1_data,
	},
};



static struct platform_driver mtk_dma_heap_ut_drv = {
	.probe = mtk_dma_heap_ut_probe,
	.remove = mtk_dma_heap_ut_remove,
	.driver = {
		.name = "dmabufheap_debug, iommu0",
		.of_match_table = mtk_dmabufheap_iommu0_of_ids,
	},
};

static struct platform_driver mtk_dma_heap_ut_drv2 = {
	.probe = mtk_dma_heap_ut_probe,
	.remove = mtk_dma_heap_ut_remove,
	.driver = {
		.name = "dmabufheap_debug, iommu1",
		.of_match_table = mtk_dmabufheap_iommu1_of_ids,
	},
};


static int __init mtk_dma_heap_ut(void)
{
	pr_info("GM-T %s\n", __func__);

	if (platform_driver_register(&mtk_dma_heap_ut_drv2)) {
		pr_info("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}


	if (platform_driver_register(&mtk_dma_heap_ut_drv)) {
		pr_info("%s platform driver register failed.\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static void __exit mtk_dma_heap_ut_exit(void)
{
	platform_driver_unregister(&mtk_dma_heap_ut_drv);
	return;
}
module_init(mtk_dma_heap_ut);
module_exit(mtk_dma_heap_ut_exit);
MODULE_LICENSE("GPL v2");
