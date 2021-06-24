// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt)    "mtk_iommu: test " fmt

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/export.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <uapi/linux/dma-heap.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mtk_heap.h"

#define DEFINE_PROC_ATTRIBUTE(__fops, __get, __set, __fmt)		  \
static int __fops ## _open(struct inode *inode, struct file *file)	  \
{									  \
	struct inode local_inode = *inode;				  \
									  \
	local_inode.i_private = PDE_DATA(inode);			  \
	__simple_attr_check_format(__fmt, 0ull);			  \
	return simple_attr_open(&local_inode, file, __get, __set, __fmt); \
}									  \
static const struct proc_ops __fops = {					  \
	.proc_open	 = __fops ## _open,				  \
	.proc_release    = simple_attr_release,				  \
	.proc_read	 = simple_attr_read,				  \
	.proc_write	 = simple_attr_write,				  \
	.proc_lseek	 = generic_file_llseek,				  \
}									  \

enum DMABUF_HEAP {
	TEST_REQ_SVP_REGION,
	TEST_REQ_PROT_REGION0,
	TEST_REQ_PROT_REGION1,
	TEST_REQ_PROT_REGION2,
	TEST_REQ_WFD,
	TEST_REQ_HAPP,
	TEST_REQ_HAPP_EXTRA,
	TEST_REQ_SDSP,
	TEST_REQ_SDSP_SHARED,
	TEST_REQ_2D_FR,
	TEST_REQ_TUI,
	TEST_REQ_SVP_PAGE,
	TEST_REQ_PROT_PAGE,
	TEST_REQ_SAPU_DATA_SHM,
	TEST_REQ_SAPU_ENGINE_SHM,
	TEST_MTK_NORMAL,
	TEST_MTK_HEAP_NUM
};

struct heap_name {
	enum DMABUF_HEAP id;
	char *name;
};

static struct heap_name heap_obj[] = {
	[TEST_REQ_SVP_REGION] = {
		.id = TEST_REQ_SVP_REGION,
		.name = "svp_region_heap",
	},
	[TEST_REQ_PROT_REGION0] = {
		.id = TEST_REQ_PROT_REGION0,
		.name = "prot_region_heap0",
	},
	[TEST_REQ_PROT_REGION1] = {
		.id = TEST_REQ_PROT_REGION1,
		.name = "prot_region_heap1",
	},
	[TEST_REQ_PROT_REGION2] = {
		.id = TEST_REQ_PROT_REGION2,
		.name = "prot_region_heap2",
	},
	[TEST_REQ_WFD] = {
		.id = TEST_REQ_WFD,
		.name = "wfd_heap",
	},
	[TEST_REQ_HAPP] = {
		.id = TEST_REQ_HAPP,
		.name = "happ_heap",
	},
	[TEST_REQ_HAPP_EXTRA] = {
		.id = TEST_REQ_HAPP_EXTRA,
		.name = "happ_extra_heap",
	},
	[TEST_REQ_SDSP] = {
		.id = TEST_REQ_SDSP,
		.name = "sdsp_heap",
	},
	[TEST_REQ_SDSP_SHARED] = {
		.id = TEST_REQ_SDSP_SHARED,
		.name = "sdsp_shared_heap",
	},
	[TEST_REQ_2D_FR] = {
		.id = TEST_REQ_2D_FR,
		.name = "2D_FR_heap",
	},
	[TEST_REQ_TUI] = {
		.id = TEST_REQ_TUI,
		.name = "TUI_heap",
	},
	[TEST_REQ_SVP_PAGE] = {
		.id = TEST_REQ_SVP_PAGE,
		.name = "svp_page_heap",
	},
	[TEST_REQ_PROT_PAGE] = {
		.id = TEST_REQ_PROT_PAGE,
		.name = "prot_page_heap",
	},
	[TEST_REQ_SAPU_DATA_SHM] = {
		.id = TEST_REQ_SAPU_DATA_SHM,
		.name = "sapu_data_shm_heap",
	},
	[TEST_REQ_SAPU_ENGINE_SHM] = {
		.id = TEST_REQ_SAPU_ENGINE_SHM,
		.name = "sapu_engine_shm_heap",
	},
	[TEST_MTK_NORMAL] = {
		.id = TEST_MTK_NORMAL,
		.name = "mm_heap",
	},
};

struct dmabuf_map {
	int				id;
	struct dma_buf_attachment	*attach;
	struct sg_table                 *table;
	dma_addr_t			dma_address;
	struct list_head		map_node;
};

struct dmabuf_info {
	int				id;
	size_t				size;
	struct dma_buf			*dmabuf;
	struct list_head		buf_node;
	struct list_head		map_head;
	struct mutex			map_lock;
};

struct dmabuf_data {
	struct device			*dev;
	struct dma_heap			*heap;
	struct mutex			buf_lock;
	struct list_head		buf_head;
};

struct iommu_test_debug_data {
	struct proc_dir_entry		*debug_root;
};

static struct iommu_test_debug_data *debug_data;
static struct dmabuf_data dmabuf_data_obj[TEST_MTK_HEAP_NUM];

static int dmabuf_id, attach_id;
static size_t test_size = (6 * 1024 * 1024);
static enum DMABUF_HEAP test_heap = TEST_REQ_PROT_REGION0;//MTK_NORMAL;

static struct dmabuf_map *dmabuf_map_alloc(int id)
{
	struct dmabuf_map *map_obj = NULL;

	map_obj = kzalloc(sizeof(struct dmabuf_map), GFP_KERNEL);
	if (map_obj == NULL)
		return NULL;

	INIT_LIST_HEAD(&(map_obj->map_node));
	map_obj->id = id;

	return map_obj;
}

static struct dmabuf_info *dmabuf_info_alloc(int buf_id)
{
	struct dmabuf_info *info_obj = NULL;

	info_obj = kzalloc(sizeof(struct dmabuf_info), GFP_KERNEL);
	if (info_obj == NULL)
		return NULL;

	INIT_LIST_HEAD(&(info_obj->buf_node));
	INIT_LIST_HEAD(&(info_obj->map_head));
	mutex_init(&info_obj->map_lock);
	info_obj->id = buf_id;

	return info_obj;
}

static void add_dmabuf_info(struct dmabuf_data *data, struct dmabuf_info *plist)
{
	mutex_lock(&(data->buf_lock));
	list_add(&(plist->buf_node), &(data->buf_head));
	mutex_unlock(&(data->buf_lock));
}

static void add_dmabuf_map(struct dmabuf_info *data, struct dmabuf_map *plist)
{
	mutex_lock(&(data->map_lock));
	list_add(&(plist->map_node), &(data->map_head));
	mutex_unlock(&(data->map_lock));
}

/*
 * if @dmabuf = NULL, we need to find dmabuf_info by @id
 * if @dmabuf != NULL, we need to find dmabuf_info by @dmabuf
 */
static struct dmabuf_info *find_dmabuf_info(struct dmabuf_data *data, int id)
{
	struct list_head *plist_tmp;
	struct dmabuf_info *obj_info;

	mutex_lock(&(data->buf_lock));
	list_for_each(plist_tmp, &(data->buf_head)) {
		obj_info = container_of(plist_tmp, struct dmabuf_info, buf_node);
		if (obj_info->id == id)
			break;
	}
	if (plist_tmp == &(data->buf_head)) {
		pr_info("%s fail, heap:%s, buf_id:%d\n",
			__func__, dma_heap_get_name(data->heap), id);
		mutex_unlock(&(data->buf_lock));
		return NULL;
	}
	mutex_unlock(&(data->buf_lock));

	return obj_info;
}

static struct dmabuf_map *find_dmabuf_map(struct dmabuf_info *data, int attach_id)
{
	struct list_head *map_tmp;
	struct dmabuf_map *map_obj;

	mutex_lock(&(data->map_lock));
	list_for_each(map_tmp, &(data->map_head)) {
		map_obj = container_of(map_tmp, struct dmabuf_map, map_node);
		if (map_obj->id == attach_id)
			break;
	}

	if (map_tmp == &(data->map_head)) {
		pr_info("%s fail, attach_id:%d\n", __func__, attach_id);
		mutex_unlock(&(data->map_lock));
		return NULL;
	}
	mutex_unlock(&(data->map_lock));

	return map_obj;
}

static int check_dmabuf_id(struct dmabuf_data *data, int alloc_id)
{
	struct list_head *plist_tmp;
	struct dmabuf_info *obj_info;

	mutex_lock(&(data->buf_lock));
	list_for_each(plist_tmp, &(data->buf_head)) {
		obj_info = container_of(plist_tmp, struct dmabuf_info, buf_node);
		if (obj_info->id == alloc_id)
			break;
	}

	if (plist_tmp == &(data->buf_head)) {
		pr_info("%s success, alloc_id(%d) can be used\n",
			__func__, alloc_id);
		mutex_unlock(&(data->buf_lock));
		return 0;
	}
	mutex_unlock(&(data->buf_lock));
	pr_info("%s fail, alloc_id(%d) has aleady exist!! dmabuf:0x%lx, sz:0x%zx\n",
		__func__, alloc_id, obj_info->dmabuf, obj_info->size);

	return -EINVAL;
}

static int check_attach_id(struct dmabuf_info *data, int id)
{
	struct list_head *plist_tmp;
	struct dmabuf_map *map_obj;

	mutex_lock(&(data->map_lock));
	list_for_each(plist_tmp, &(data->map_head)) {
		map_obj = container_of(plist_tmp, struct dmabuf_map, map_node);
		if (map_obj->id == id)
			break;
	}

	if (plist_tmp == &(data->map_head)) {
		pr_info("%s success, attach_id(%d) can be used\n", __func__, id);
		mutex_unlock(&(data->map_lock));
		return 0;
	}
	mutex_unlock(&(data->map_lock));
	pr_info("%s fail, attach_id(%d) has aleady exist\n",
		__func__, id);

	return -EINVAL;
}

static void del_dmabuf_info(struct dmabuf_info *info_obj)
{
	list_del(&info_obj->buf_node);
	kfree(info_obj);
}

static void del_dmabuf_map(struct dmabuf_map *map_obj)
{
	list_del(&map_obj->map_node);
	kfree(map_obj);
}

static void dump_dmabuf_info_map(enum DMABUF_HEAP heap)
{
	struct list_head *info_tmp, *map_tmp;
	struct dmabuf_info *info_obj;
	struct dmabuf_map *map_obj;
	struct dmabuf_data *data = &dmabuf_data_obj[heap];

	mutex_lock(&(data->buf_lock));
	list_for_each(info_tmp, &(data->buf_head)) {
		info_obj = container_of(info_tmp, struct dmabuf_info, buf_node);
		pr_info("%s 1, info_obj:0x%lx, buf_id:%d, dmabuf:0x%lx, sz:0x%zx, heap:%s\n",
			__func__, (unsigned long)info_obj, info_obj->id,
			(unsigned long)info_obj->dmabuf, info_obj->size, heap_obj[heap].name);
		mutex_lock(&(info_obj->map_lock));
		list_for_each(map_tmp, &(info_obj->map_head)) {
			map_obj = container_of(map_tmp, struct dmabuf_map, map_node);
			pr_info("%s 2, map_obj:0x%lx, attach_id:%d, attach:0x%lx, iova:%pa\n",
				__func__, (unsigned long)map_obj, map_obj->id,
				(unsigned long)map_obj->attach, &map_obj->dma_address);
		}
		mutex_unlock(&(info_obj->map_lock));
	}
	mutex_unlock(&(data->buf_lock));
}

static void dmabuf_heap_alloc_test(size_t size, enum DMABUF_HEAP heap, int buf_id)
{
	int ret;
	struct dmabuf_data *data_obj;
	struct dmabuf_info *info_obj;

	pr_info("%s start, heap:%s(%d)\n", __func__, heap_obj[heap].name, heap);

	ret = check_dmabuf_id(&dmabuf_data_obj[heap], buf_id);
	if (ret) {
		pr_info("%s failed, buf_id can not be used:%d\n", __func__, buf_id);
		return;
	}

	switch (heap) {
	case TEST_MTK_NORMAL:
		data_obj = &dmabuf_data_obj[TEST_MTK_NORMAL];
		data_obj->heap = dma_heap_find("mtk_mm");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_mm failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_REGION0:
		data_obj = &dmabuf_data_obj[TEST_REQ_PROT_REGION0];
		data_obj->heap = dma_heap_find("mtk_prot_region-uncached");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_prot_region-uncached failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_REGION1:
		data_obj = &dmabuf_data_obj[TEST_REQ_PROT_REGION1];
		data_obj->heap = dma_heap_find("mtk_prot_region-uncached");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_prot_region-uncached failed!!\n", __func__);
			return;
		}
		break;
	case TEST_REQ_PROT_REGION2:
		data_obj = &dmabuf_data_obj[TEST_REQ_PROT_REGION2];
		data_obj->heap = dma_heap_find("mtk_prot_region-uncached");
		if (!data_obj->heap) {
			pr_info("%s, find mtk_prot_region-uncached failed!!\n", __func__);
			return;
		}
		break;
	default:
		pr_info("%s failed, heap type in invalid:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	pr_info("%s dma_heap_find done, heap:%s\n", __func__, dma_heap_get_name(data_obj->heap));
	info_obj = dmabuf_info_alloc(buf_id);
	if (!info_obj) {
		pr_info("%s dmabuf_info_alloc fail, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}
	info_obj->dmabuf = dma_heap_buffer_alloc(data_obj->heap, size,
				DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(info_obj->dmabuf)) {
		pr_info("%s, alloc buffer fail, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	info_obj->size = size;
	add_dmabuf_info(data_obj, info_obj);

	pr_info("%s success, heap:%s(%d), dmabuf_info:0x%lx, dmabuf:0x%lx, sz:0x%zx, buf_id:%d\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj,
		(unsigned long)info_obj->dmabuf, size, buf_id);
}

static void dmabuf_heap_free_test(enum DMABUF_HEAP heap, int buf_id)
{
	struct dmabuf_info *info_obj;

	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}
	pr_info("%s success, heap:%s(%d), dmabuf_info:0x%lx, dmabuf:0x%lx, buf_id:%d\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj,
		(unsigned long)info_obj->dmabuf, buf_id);

	dma_heap_buffer_free(info_obj->dmabuf);
	del_dmabuf_info(info_obj);
}

static void dmabuf_map_iova(enum DMABUF_HEAP heap, int buf_id, int attach_id)
{
	int ret;
	struct dmabuf_map *map_obj;
	struct dmabuf_info *info_obj;
	struct device *dev = dmabuf_data_obj[heap].dev;
	struct dma_buf_attachment *attach;
	struct sg_table *table;

	pr_info("%s start, heap:%s(%d), dev:%s\n", __func__,
		heap_obj[heap].name, heap, dev_name(dev));

	/* find dmabuf_info by attach buf_id from dmabuf_data */
	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}

	ret = check_attach_id(info_obj, attach_id);
	if (ret) {
		pr_info("%s failed, attach_id can not be used:%d\n", __func__, attach_id);
		return;
	}

	map_obj = dmabuf_map_alloc(attach_id);
	if (!map_obj) {
		pr_info("%s dmabuf_map_alloc fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}

	attach = dma_buf_attach(info_obj->dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_attach failed!!, heap:%s(%d)\n",
			__func__, heap_obj[heap].name, heap);
		return;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(table)) {
		pr_info("%s, dma_buf_map_attachment failed!!, heap:%s\n",
			__func__, heap_obj[heap].name);
		return;
	}

	map_obj->attach = attach;
	map_obj->table = table;
	map_obj->dma_address = sg_dma_address(table->sgl);
	add_dmabuf_map(info_obj, map_obj);
	pr_info("%s success, heap:%s(%d), dmabuf:0x%lx, map_obj:0x%lx, buf_id:%d, att_id:%d, attach:0x%lx, iova:%pa\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj->dmabuf,
		(unsigned long)map_obj, buf_id, attach_id, (unsigned long)map_obj->attach,
		&map_obj->dma_address);
}

static void dmabuf_unmap_iova(enum DMABUF_HEAP heap, int buf_id, int attach_id)
{
	struct dmabuf_info *info_obj;
	struct dmabuf_map *map_obj;

	pr_info("%s start\n", __func__);

	/* find dmabuf_info by buf_id from dmabuf_data */
	info_obj = find_dmabuf_info(&dmabuf_data_obj[heap], buf_id);
	if (!info_obj) {
		pr_info("%s find dmabuf_info fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}
	map_obj = find_dmabuf_map(info_obj, attach_id);
	if (!map_obj) {
		pr_info("%s find dmabuf_map fail, heap:%s\n",
			__func__, dma_heap_get_name(dmabuf_data_obj[heap].heap));
		return;
	}

	pr_info("%s success, heap:%s(%d), dmabuf:0x%lx, map_obj:0x%lx, buf_id:%d, att_id:%d, attach:0x%lx, iova:%pa\n",
		__func__, heap_obj[heap].name, heap, (unsigned long)info_obj->dmabuf,
		(unsigned long)map_obj, buf_id, attach_id, (unsigned long)map_obj->attach,
		&map_obj->dma_address);

	dma_buf_unmap_attachment(map_obj->attach, map_obj->table, DMA_BIDIRECTIONAL);
	dma_buf_detach(info_obj->dmabuf, map_obj->attach);

	del_dmabuf_map(map_obj);
}

enum CMD_INDEX {
	SET_SIZE_CMD = 0,
	SET_HEAP_ID_CMD = 1,
	SET_DMABUF_ID_CMD = 2,
	SET_ATTACH_ID_CMD = 3,

	ALLOC_BUF_CMD = 10,
	MAP_CMD = 11,
	UNMAP_CMD = 12,
	FREE_BUF_CMD = 13,
	DUMP_INFO_CMD = 14,
};
struct cmd_name {
	enum CMD_INDEX index;
	char *name;
};

static struct cmd_name test_cmd_name[] = {
	[SET_SIZE_CMD] = {
		.index = SET_SIZE_CMD,
		.name = "Set allocate buffer size",
	},
	[SET_HEAP_ID_CMD] = {
		.index = SET_HEAP_ID_CMD,
		.name = "Set allocate buffer heap",
	},
	[SET_DMABUF_ID_CMD] = {
		.index = SET_DMABUF_ID_CMD,
		.name = "Set dmabuf id",
	},
	[SET_ATTACH_ID_CMD] = {
		.index = SET_ATTACH_ID_CMD,
		.name = "Set attach id",
	},

	[ALLOC_BUF_CMD] = {
		.index = ALLOC_BUF_CMD,
		.name = "Allocate buffer test",
	},
	[MAP_CMD] = {
		.index = MAP_CMD,
		.name = "Map buffer test",
	},
	[UNMAP_CMD] = {
		.index = UNMAP_CMD,
		.name = "Unmap buffer test",
	},
	[FREE_BUF_CMD] = {
		.index = FREE_BUF_CMD,
		.name = "Free buffer test",
	},
	[DUMP_INFO_CMD] = {
		.index = DUMP_INFO_CMD,
		.name = "Dump buffer_info_map test",
	},
};


/*
 * dmabuf secure heap test:
 * UT-1: map heap_iova && buffer iova test   --> pass
 * step-1. alloc buffer by dmabuf_id = 0
 * --->
 * step-2. map dmabuf_id = 0 to iova by attach_id=0,1,2
 * step-3. check mapping result:
 *         1.heap region PA && size by alloc in step-1
 *         2.heap_region and dmabuf are all mapped in step-2
 *         3.heap_iova must be mappped by 1st level, not 2nd level
 *         4.all dmabuf only mapping one iova not more than one
 *
 * UT-2: alloc && map more than more buffer by normal region   --> pass
 * step-1. alloc buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 to iova by attach_id=0
 * step-3. alloc buffer by dmabuf_id = 1
 * --->	check: no need to alloc heap region PA && size(only need it in step1)
 * step-4. map dmabuf_id = 1 to iova by attach_id=1
 * --->	check: no need to run dma_map_sgtable and get iova by PA offset
 * step-5. alloc buffer by dmabuf_id = 2
 * --->	check: no need to alloc heap region PA && size(only need it in step1)
 * step-6. map dmabuf_id = 2 to iova by attach_id=2
 * --->	check: no need to run dma_map_sgtable and get iova by PA offset
 * step7. free dmabuf_id = 2,1,0
 * --->	check: only after last dmabuf free, heap_region iova will free
 *
 * UT-3: non-normal iova region test   --> pass
 * step-1. use prot_region_heap0(normal region) to alloc one buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 to iova by attach_id=0 and use prot_region_heap0
 * step-3. use prot_region_heap1(non-normal region) to alloc one buffer by dmabuf_id = 0
 * step-4. use prot_region_heap1 to map dmabuf_id = 0 to iova by attach_id=0
 * ---> check: non-normal region mapping must be run dma_map_sgtable
 * step-5. use prot_region_heap1 to free dmabuf_id = 0
 * ---> check: non-normal region unmap must be run dma_unmap_sgtable
 *
 *
 * UT-4: non-normal iova region 1MB align check   --> pass
 * step-1. set test_size not 1MB align(0x3100)
 * step-2. use prot_region_heap0(normal region) to alloc one buffer by dmabuf_id = 0
 * step-3. use prot_region_heap0 to map dmabuf_id = 0 to iova by attach_id = 0
 * ---> check: size be aligned to 0x4000 and map success
 * step-4. use prot_region_heap1(non-normal region) to alloc one buffer by dmabuf_id = 0
 * step-5. use prot_region_heap1 to map dmabuf_id = 0 to iova by attach_id = 0
 * ---> check: map fail because of not 1MB align
 *
 *
 * UT-5: non-iommu-dev test   --> pass
 * step-1. use prot_region_heap1(non-iommu dev) alloc buffer by dmabuf_id = 0
 * step-2. map dmabuf_id = 0 by attach_id=0
 * ---> check: run non-iommu mapping pass
 * step-3. unmap dmabuf_id by attach_id=0
 * ---> check: run non-iommu unmap pass
 */

static int iommu_test_debug_set(void *data, u64 input)
{
	u64 index = (input & 0xff);
	u64 val = (input >> 8);

	pr_info("%s [%s], start, input:0x%llx, index=%llu, val:%llu\n",
		__func__, test_cmd_name[index].name, input, index, val);

	switch (index) {
	case 0:
		/* adb shell "echo 0xXXXXX00 > /d/iommu/test" */
		test_size = val;
		pr_info("%s, set test_size: 0x%llx\n", __func__, test_size);
		break;
	case 1:
		/*
		 * mm_heap:     adb shell "echo 0xd01 > /d/iommu/test"
		 * prot_region: adb shell "echo 0x101 > /d/iommu/test"
		 */
		test_heap = (enum DMABUF_HEAP)val;
		pr_info("%s, set test_heap: %s(%d)(%llu)\n", __func__,
			heap_obj[test_heap], test_heap, val); //??????
		break;
	case 2:
		/*
		 * dmabuf_id=1: adb shell "echo 0x102 > /d/iommu/test"
		 * dmabuf_id=2: adb shell "echo 0x202 > /d/iommu/test"
		 * dmabuf_id=3: adb shell "echo 0x302 > /d/iommu/test"
		 */
		dmabuf_id = val;
		pr_info("%s, set dmabuf_id: %d\n", __func__, dmabuf_id);
		break;
	case 3:
		/*
		 * attach_id=1: adb shell "echo 0x103 > /d/iommu/test"
		 * attach_id=2: adb shell "echo 0x203 > /d/iommu/test"
		 * attach_id=3: adb shell "echo 0x303 > /d/iommu/test"
		 */
		attach_id = val;
		pr_info("%s, set attach_id: %d\n", __func__, attach_id);
		break;
	case 10:
		/* adb shell "echo 0xa > /d/iommu/test" */
		dmabuf_heap_alloc_test(test_size, test_heap, dmabuf_id);
		break;
	case 11:
		/* adb shell "echo 0xb > /d/iommu/test" */
		dmabuf_map_iova(test_heap, dmabuf_id, attach_id);
		break;
	case 12:
		/* adb shell "echo 0xc > /d/iommu/test" */
		dmabuf_unmap_iova(test_heap, dmabuf_id, attach_id);
		break;
	case 13:
		/* adb shell "echo 0xd > /d/iommu/test" */
		dmabuf_heap_free_test(test_heap, dmabuf_id);
		break;
	case 14:
		/* adb shell "echo 0xe > /d/iommu/test" */
		dump_dmabuf_info_map(test_heap);
		break;
	default:
		pr_info("%s error,index=%llu\n", __func__, index);
	}

	pr_info("%s [%s] done\n", __func__, test_cmd_name[index].name);

	return 0;
}

static int iommu_test_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}
DEFINE_PROC_ATTRIBUTE(iommu_test_debug_fops, iommu_test_debug_get, iommu_test_debug_set, "%llu\n");

static int iommu_test_debug_init(struct iommu_test_debug_data *data)
{
	struct proc_dir_entry *debug_file;

	data->debug_root = proc_mkdir("iommu", NULL);

	if (IS_ERR_OR_NULL(data->debug_root))
		pr_info("failed to create debug dir.\n");

	debug_file = proc_create_data("test",
		S_IFREG | 0644, data->debug_root, &iommu_test_debug_fops, NULL);

	if (IS_ERR_OR_NULL(debug_file))
		pr_info("failed to create debug files 2.\n");

	return 0;
}

static int dmabuf_mm_heap_test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_MTK_NORMAL].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_MTK_NORMAL].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_MTK_NORMAL].buf_head);
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmabuf_prot_region_test_probe0(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_PROT_REGION0].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION0].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION0].buf_head);
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmabuf_prot_region_test_probe1(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_PROT_REGION1].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION1].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION1].buf_head);
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dmabuf_prot_region_test_probe2(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));

	dmabuf_data_obj[TEST_REQ_PROT_REGION2].dev = dev;
	mutex_init(&dmabuf_data_obj[TEST_REQ_PROT_REGION2].buf_lock);
	INIT_LIST_HEAD(&dmabuf_data_obj[TEST_REQ_PROT_REGION2].buf_head);
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34));

	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));
	return 0;
}

static int dma_buf_test_probe(struct platform_device *pdev)
{
	size_t size = SZ_1M;
	struct dma_heap	*heap;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	struct device *dev = &pdev->dev;

	pr_info("%s start, dev:%s\n", __func__, dev_name(dev));

	heap = dma_heap_find("mtk_mm");
	if (!heap) {
		pr_info("%s, find mtk_mm failed!!\n", __func__);
		return -EINVAL;
	}

	dmabuf = dma_heap_buffer_alloc(heap, size,
				DMA_HEAP_VALID_FD_FLAGS, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dmabuf)) {
		pr_info("%s, alloc buffer fail, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s alloc dma-buf success, size:0x%zx, heap:%s\n",
		__func__, size, dma_heap_get_name(heap));

	attach = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_attach failed!!, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(attach)) {
		pr_info("%s, dma_buf_map_attachment failed!!, heap:%s\n", __func__, dma_heap_get_name(heap));
		return -EINVAL;
	}
	pr_info("%s map dma-buf success, size:0x%zx, heap:%s, iova:0x%lx\n",
		__func__, size, dma_heap_get_name(heap), (unsigned long)sg_dma_address(table->sgl));

	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);

	dma_heap_buffer_free(dmabuf);

	pr_info("%s done, dev:%s\n", __func__, dev_name(dev));
	return 0;
}

static int iommu_test_dom_probe(struct platform_device *pdev)
{
	#define TEST_NUM	3
	int i, ret;
	void *cpu_addr[TEST_NUM];
	dma_addr_t dma_addr[TEST_NUM];
	size_t size = (6 * SZ_1M + PAGE_SIZE * 3);

	pr_info("%s start, dev:%s\n", __func__, dev_name(&pdev->dev));
	dma_set_mask_and_coherent(&pdev->dev,DMA_BIT_MASK(34));
	for (i = 0; i < TEST_NUM; i++) {
		cpu_addr[i] = dma_alloc_attrs(&pdev->dev, size, &dma_addr[i], GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);
		pr_info("dev:%s, alloc iova success, iova:%pa, size:0x%zx\n", dev_name(&pdev->dev), &dma_addr[i], size);
	}
	for (i = 0; i < TEST_NUM; i++) {
		dma_free_attrs(&pdev->dev, size, cpu_addr[i], dma_addr[i], DMA_ATTR_WRITE_COMBINE);
		pr_info("dev:%s, free iova success, iova:%pa, size:0x%zx\n", dev_name(&pdev->dev), &dma_addr[i], size);
	}
	pr_info("%s done, dev:%s\n", __func__, dev_name(&pdev->dev));

	ret = dma_buf_test_probe(pdev);
	if (ret)
		pr_info("%s failed, dma_buf_test_probe fail, dev:%s\n", __func__, dev_name(&pdev->dev));

	return 0;
}

static const struct of_device_id iommu_test_dom_match_table[] = {
	{.compatible = "mediatek,iommu-test-dom0"},
	{.compatible = "mediatek,iommu-test-dom1"},
	{.compatible = "mediatek,iommu-test-dom2"},
	{.compatible = "mediatek,iommu-test-dom3"},
	{.compatible = "mediatek,iommu-test-dom4"},
	{.compatible = "mediatek,iommu-test-dom5"},
	{.compatible = "mediatek,iommu-test-dom6"},
	{.compatible = "mediatek,iommu-test-dom7"},
	{.compatible = "mediatek,iommu-test-dom8"},
	{.compatible = "mediatek,iommu-test-dom9"},
	{},
};

static const struct of_device_id dmabuf_prot_region0_match_table[] = {
	{.compatible = "mediatek,common-dmabuf-prot-region0"},
	{},
};

static const struct of_device_id dmabuf_prot_region1_match_table[] = {
	{.compatible = "mediatek,common-dmabuf-prot-region1"},
	{},
};

static const struct of_device_id dmabuf_prot_region2_match_table[] = {
	{.compatible = "mediatek,common-dmabuf-prot-region2"},
	{},
};

static const struct of_device_id dmabuf_normal_match_table[] = {
	{.compatible = "mediatek,common-dmabuf-normal"},
	{},
};

static struct platform_driver iommu_test_dmabuf_normal = {
	.probe = dmabuf_mm_heap_test_probe,
	.driver = {
		.name = "iommu-test-dmabuf-normal",
		.of_match_table = dmabuf_normal_match_table,
	},
};

static struct platform_driver iommu_test_dmabuf_prot_region0 = {
	.probe = dmabuf_prot_region_test_probe0,
	.driver = {
		.name = "iommu-test-dmabuf-prot-region0",
		.of_match_table = dmabuf_prot_region0_match_table,
	},
};

static struct platform_driver iommu_test_dmabuf_prot_region1 = {
	.probe = dmabuf_prot_region_test_probe1,
	.driver = {
		.name = "iommu-test-dmabuf-prot-region1",
		.of_match_table = dmabuf_prot_region1_match_table,
	},
};

static struct platform_driver iommu_test_dmabuf_prot_region2 = {
	.probe = dmabuf_prot_region_test_probe2,
	.driver = {
		.name = "iommu-test-dmabuf-prot-region2",
		.of_match_table = dmabuf_prot_region2_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom0 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom0",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom1 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom1",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom2 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom2",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom3 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom3",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom4 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom4",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom5 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom5",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom6 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom6",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom7 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom7",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom8 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom8",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver iommu_test_driver_dom9 = {
	.probe = iommu_test_dom_probe,
	.driver = {
		.name = "iommu-test-dom9",
		.of_match_table = iommu_test_dom_match_table,
	},
};

static struct platform_driver *const iommu_test_drivers[] = {
	&iommu_test_dmabuf_normal,
	&iommu_test_dmabuf_prot_region0,
	&iommu_test_dmabuf_prot_region1,
	&iommu_test_dmabuf_prot_region2,
	&iommu_test_driver_dom0,
	&iommu_test_driver_dom1,
	&iommu_test_driver_dom2,
	&iommu_test_driver_dom3,
	&iommu_test_driver_dom4,
	&iommu_test_driver_dom5,
	&iommu_test_driver_dom6,
	&iommu_test_driver_dom7,
	&iommu_test_driver_dom8,
	&iommu_test_driver_dom9,
};

static int __init iommu_test_init(void)
{
	int ret;
	int i;

	pr_info("%s+\n", __func__);

	debug_data = kmalloc(sizeof(struct iommu_test_debug_data), GFP_KERNEL);
	if (!debug_data)
		return -ENOMEM;

	iommu_test_debug_init(debug_data);

	for (i = 0; i < ARRAY_SIZE(iommu_test_drivers); i++) {
		pr_info("%s, register %d\n", __func__, i);
		ret = platform_driver_register(iommu_test_drivers[i]);
		if (ret < 0) {
			pr_err("Failed to register %s driver: %d\n",
				  iommu_test_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	pr_info("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(iommu_test_drivers[i]);

	return ret;
}

static void __exit iommu_test_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(iommu_test_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(iommu_test_drivers[i]);
}

module_init(iommu_test_init);
module_exit(iommu_test_exit);
MODULE_LICENSE("GPL v2");
