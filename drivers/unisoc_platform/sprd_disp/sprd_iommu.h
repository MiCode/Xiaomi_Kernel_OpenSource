/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_IOMMU_H
#define _SPRD_IOMMU_H

#include <linux/scatterlist.h>
#include <asm/cacheflush.h>

#define GSP_VSP_IOMMU_SIZE		(2 * 256 * 1024)
#define DCAM_ISP_IOMMU_SIZE	(26 * 100 * 1024)
#define CPP_JPEG_IOMMU_SIZE		(12 * 100 * 1024)
#define DISPC_IOMMU_SIZE		(5 * 100 * 1024)

struct sprd_iommu_init_data {
	int id;                        /* iommu id */
	char *name;                    /* iommu name */

	unsigned long iova_base;            /* io virtual address base */
	size_t iova_size;            /* io virtual address size */
	unsigned long pgt_base;             /* iommu page table base address */
	size_t pgt_size;             /* iommu page table array size */
	unsigned long ctrl_reg;             /* iommu control register */
	unsigned long fault_page;
	unsigned long re_route_page;
	unsigned int iommu_rev;

	/*add for 9860 interlace ddr*/
	/*iommu reserved memory of pf page table*/
	unsigned long pagt_base_ddr;
	unsigned long pagt_base_virt;
	unsigned long pagt_ddr_size;
	/*for shakrl2/isharl2*/
	unsigned int iommuex_rev;
};

#define SPRD_MAX_SG_CACHED_CNT 1024

enum sg_pool_status {
	SG_SLOT_FREE = 0,
	SG_SLOT_USED,
};

enum sprd_iommu_chtype {
	SPRD_IOMMU_PF_CH_READ = 0x100,/*prefetch channel only support read*/
	SPRD_IOMMU_PF_CH_WRITE,/* prefetch channel only support write*/
	SPRD_IOMMU_FM_CH_RW,/*fullmode channel support w/r in one channel*/
	SPRD_IOMMU_EX_CH_READ,/*channel only support read, only ISP use now*/
	SPRD_IOMMU_EX_CH_WRITE,/*channel only support read, only ISP use now*/
	SPRD_IOMMU_CH_TYPE_INVALID, /*unsupported channel type*/
};

enum sprd_iommu_id {
	SPRD_IOMMU_VSP,
	SPRD_IOMMU_DCAM,
	SPRD_IOMMU_DCAM1,
	SPRD_IOMMU_CPP,
	SPRD_IOMMU_GSP,
	SPRD_IOMMU_GSP1,
	SPRD_IOMMU_JPG,
	SPRD_IOMMU_DISP,
	SPRD_IOMMU_DISP1,
	SPRD_IOMMU_ISP,
	SPRD_IOMMU_ISP1,
	SPRD_IOMMU_FD,
	SPRD_IOMMU_AI,
	SPRD_IOMMU_EPP,
	SPRD_IOMMU_EDP,
	SPRD_IOMMU_IDMA,
	SPRD_IOMMU_VDMA,
	SPRD_IOMMU_MAX,
};

struct sprd_iommu_sg_rec {
	unsigned long sg_table_addr;
	void *buf_addr;
	unsigned long iova_addr;
	unsigned long iova_size;
	enum sg_pool_status status;
	int map_usrs;
};

struct sprd_iommu_sg_pool {
	int pool_cnt;
	struct sprd_iommu_sg_rec slot[SPRD_MAX_SG_CACHED_CNT];
};

struct sprd_iommu_dev {
	struct sprd_iommu_init_data *init_data;
	struct gen_pool *pool;
	struct gen_pool *pf_ch_pool;
	struct sprd_iommu_ops *ops;
	struct clk *mmu_mclock;
	struct clk *mmu_pclock;
	struct clk *mmu_clock;
	struct clk *mmu_axiclock;
	atomic_t iommu_dev_cnt;

	void *private;
	unsigned int map_count;
	unsigned int div2_frq;
	bool light_sleep_en;
	spinlock_t pgt_lock;
	unsigned int status_count;
	struct mutex status_mutex;

	struct sprd_iommu_sg_pool sg_pool;
	struct device *drv_dev;
	unsigned long mmupf_iovaarray[SPRD_MAX_SG_CACHED_CNT];

	enum sprd_iommu_chtype ch_type;
	u32 channel_id;
	int id;
	int org_id;
};

/*map arguments for kernel space*/
struct sprd_iommu_map_data {
	struct sg_table *table;
	void *buf;
	size_t iova_size;
	enum sprd_iommu_chtype ch_type;
	unsigned int sg_offset;
	unsigned long iova_addr;/*output*/
};

struct sprd_iommu_unmap_data {
	unsigned long iova_addr;
	size_t iova_size;
	enum sprd_iommu_chtype ch_type;
	struct sg_table *table;
	void *buf;
	u32 channel_id;
	int dev_id;
};

struct sprd_iommu_list_data {
	enum sprd_iommu_id iommu_id;
	bool enabled;
	struct sprd_iommu_dev *iommu_dev;
};

/*kernel API for Iommu map/unmap*/
#if IS_ENABLED(CONFIG_UNISOC_IOMMU)
int sprd_iommu_attach_device(struct device *dev);
int sprd_iommu_dettach_device(struct device *dev);

int sprd_iommu_map(struct device *dev,
		struct sprd_iommu_map_data *data);
int sprd_iommu_map_single_page(struct device *dev,
		struct sprd_iommu_map_data *data);
int sprd_iommu_map_with_idx(struct device *dev,
		struct sprd_iommu_map_data *data, int idx);
int sprd_iommu_unmap(struct device *dev,
		struct sprd_iommu_unmap_data *data);
int sprd_iommu_unmap_with_idx(struct device *dev,
		struct sprd_iommu_unmap_data *data, int idx);
int sprd_iommu_unmap_orphaned(struct sprd_iommu_unmap_data *data);
int sprd_iommu_suspend(struct device *dev);
int sprd_iommu_resume(struct device *dev);
int sprd_iommu_restore(struct device *dev);
int sprd_iommu_set_cam_bypass(bool vaor_bp_en);
#else
static inline int sprd_iommu_attach_device(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_dettach_device(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_map(struct device *dev,
					struct sprd_iommu_map_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_unmap(struct device *dev,
			struct sprd_iommu_unmap_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_unmap_orphaned(struct sprd_iommu_unmap_data *data)
{
	return -ENODEV;
}

static inline int sprd_iommu_suspend(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_resume(struct device *dev)
{
	return -ENODEV;
}

static inline int sprd_iommu_restore(struct device *dev)
{
	return -ENODEV;
}
#endif

struct sprd_iommu_ops {
	int (*init)(struct sprd_iommu_dev *dev, struct sprd_iommu_init_data *data);
	int (*exit)(struct sprd_iommu_dev *dev);
	unsigned long (*iova_alloc)(struct sprd_iommu_dev *dev, size_t iova_length, struct sprd_iommu_map_data *iommu_map_data);
	void (*iova_free)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*iova_map)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length, struct sg_table *table, struct sprd_iommu_map_data *iommu_map_data);
	int (*iova_unmap)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
	int (*backup)(struct sprd_iommu_dev *dev);
	int (*restore)(struct sprd_iommu_dev *dev);
	int (*set_bypass)(struct sprd_iommu_dev *dev, bool vaor_bp_en);
	void (*enable)(struct sprd_iommu_dev *dev);
	void (*disable)(struct sprd_iommu_dev *dev);
	int (*dump)(struct sprd_iommu_dev *dev, void *data);
	void (*open)(struct sprd_iommu_dev *dev);
	void (*release)(struct sprd_iommu_dev *dev);
	void (*pgt_show)(struct sprd_iommu_dev *dev);
	int (*suspend)(struct sprd_iommu_dev *dev);
	int (*resume)(struct sprd_iommu_dev *dev);
	int (*iova_unmap_orphaned)(struct sprd_iommu_dev *dev, unsigned long iova, size_t iova_length);
};

enum IOMMU_ID {
	/*for sharkl2 iommu*/
	IOMMU_EX_VSP,
	IOMMU_EX_DCAM,
	IOMMU_EX_CPP,
	IOMMU_EX_GSP,
	IOMMU_EX_GSP1,
	IOMMU_EX_JPG,
	IOMMU_EX_DISP,
	IOMMU_EX_DISP1,
	IOMMU_EX_ISP,
	IOMMU_EX_NEWISP,
	IOMMU_EX_FD,
	IOMMU_EX_AI,
	IOMMU_EX_EPP,
	IOMMU_EX_EDP,
	IOMMU_EX_IDMA,
	IOMMU_EX_VDMA,
	/*for sharkle*/
	IOMMU_EXLE_VSP,
	IOMMU_EXLE_DCAM,
	IOMMU_EXLE_CPP,
	IOMMU_EXLE_GSP,
	IOMMU_EXLE_JPG,
	IOMMU_EXLE_DISP,
	IOMMU_EXLE_ISP,
	/*for pike2*/
	IOMMU_EXPK2_VSP,
	IOMMU_EXPK2_DCAM,
	IOMMU_EXPK2_CPP,
	IOMMU_EXPK2_GSP,
	IOMMU_EXPK2_JPG,
	IOMMU_EXPK2_DISP,
	IOMMU_EXPK2_ISP,
	/*for sharkl3*/
	IOMMU_EXL3_VSP,
	IOMMU_EXL3_DCAM,
	IOMMU_EXL3_CPP,
	IOMMU_EXL3_JPG,
	IOMMU_EXL3_DISP,
	IOMMU_EXL3_ISP,
	/*for sharkl5*/
	IOMMU_EXL5_VSP,
	IOMMU_EXL5_DCAM,
	IOMMU_EXL5_CPP,
	IOMMU_EXL5_JPG,
	IOMMU_EXL5_DISP,
	IOMMU_EXL5_ISP,
	IOMMU_EXL5_FD,
	/*for roc1*/
	IOMMU_EXROC1_VSP,
	IOMMU_EXROC1_DCAM,
	IOMMU_EXROC1_CPP,
	IOMMU_EXROC1_JPG,
	IOMMU_EXROC1_DISP,
	IOMMU_EXROC1_ISP,
	IOMMU_EXROC1_FD,
	IOMMU_EXROC1_AI,
	IOMMU_EXROC1_EPP,
	IOMMU_EXROC1_EDP,
	/*for sharkl5pro*/
	IOMMU_VAUL5P_VSP,
	IOMMU_VAUL5P_DCAM,
	IOMMU_VAUL5P_CPP,
	IOMMU_VAUL5P_JPG,
	IOMMU_VAUL5P_DISP,
	IOMMU_VAUL5P_ISP,
	IOMMU_VAUL5P_FD,
	IOMMU_VAUL5P_AI,
	IOMMU_VAUL5P_EPP,
	IOMMU_VAUL5P_EDP,
	IOMMU_VAUL5P_IDMA,
	IOMMU_VAUL5P_VDMA,
	/*for sharkl6*/
	IOMMU_VAUL6_VSP,
	IOMMU_VAUL6_DCAM,
	IOMMU_VAUL6_CPP,
	IOMMU_VAUL6_JPG,
	IOMMU_VAUL6_DISP,
	IOMMU_VAUL6_ISP,
	IOMMU_VAUL6_FD,
	IOMMU_VAUL6P_VSP,
	IOMMU_VAUL6P_DCAM,
	IOMMU_VAUL6P_CPP,
	IOMMU_VAUL6P_GSP,
	IOMMU_VAUL6P_GSP1,
	IOMMU_VAUL6P_JPG,
	IOMMU_VAUL6P_DISP,
	IOMMU_VAUL6P_DISP1,
	IOMMU_VAUL6P_ISP,
	IOMMU_VAUL6P_FD,
	IOMMU_VAUL6P_AI,
	IOMMU_VAUL6P_EPP,
	IOMMU_VAUL6P_EDP,
	IOMMU_VAUL6P_IDMA,
	IOMMU_VAUL6P_VDMA,
	IOMMU_MAX,
};
extern struct sprd_iommu_ops sprd_iommuex_hw_ops;
extern struct sprd_iommu_ops sprd_iommuvau_hw_ops;

#define IOMMU_TAG "sprd_iommu: "

#define IOMMU_DEBUG(fmt, ...) \
	pr_debug(IOMMU_TAG  pr_fmt(fmt), ##__VA_ARGS__)

#define IOMMU_ERR(fmt, ...) \
	pr_err(IOMMU_TAG  pr_fmt(fmt), ##__VA_ARGS__)

#define IOMMU_INFO(fmt, ...) \
	pr_info(IOMMU_TAG  pr_fmt(fmt), ##__VA_ARGS__)

#define IOMMU_WARN(fmt, ...) \
	pr_warn(IOMMU_TAG  pr_fmt(fmt), ##__VA_ARGS__)


#endif
