/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __MNTL_OPS_H__
#define __MNTL_OPS_H__

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/uaccess.h>
#include <asm/div64.h>
#include <linux/miscdevice.h>
#include <linux/rtc.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include "mtk_nand_chip.h"
#include "nandx_bmt.h"

/* #define MTK_FORCE_READ_FULL_PAGE */

extern bool tlc_snd_phyplane;
extern enum NFI_TLC_PG_CYCLE tlc_program_cycle;
extern bool tlc_lg_left_plane;
extern struct mtk_nand_host *host;
extern void dump_nfi(void);

/*
 * For mntl init from mnb partition
 * This function is defined in kernel/module.c
 */
extern int init_module_mem(void *buf, int size);

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE  (1)
#endif

#ifndef NULL
#define NULL  (0)
#endif

/**********  PMT Related ***********/
#define FTL_PARTITION_NAME	"userdata"

struct nand_ftl_partition_info {
	unsigned int start_block;	/* Number of data blocks */
	unsigned int total_block;	/* Number of block */
	unsigned int slc_ratio;	/* FTL SLC ratio here */
	unsigned int slc_block;	/* FTL SLC ratio here */
};

enum operation_types {
	MTK_NAND_OP_READ = 0,
	MTK_NAND_OP_WRITE,
	MTK_NAND_OP_ERASE,
};

struct list_node {
	struct list_node *next;
};

#define containerof(ptr, type, member) \
	((type *)((unsigned long)(ptr) - __builtin_offsetof(type, member)))

struct mtk_nand_chip_operation {
	struct mtk_nand_chip_info *info;	/* Data info */
	enum operation_types types;
	/* Operation type, 0: Read, 1: write, 2:Erase */
	int block;
	int page;
	int offset;
	int size;
	bool more;
	unsigned char *data_buffer;
	unsigned char *oob_buffer;
	mtk_nand_callback_func callback;
	void *userdata;
};

struct nand_work {
	struct list_node list;
	struct mtk_nand_chip_operation ops;
};

enum worklist_type {
	LIST_ERASE = 0,
	LIST_SLC_WRITE,
	LIST_NS_WRITE,		/* none slc write list: mlc or tlc */
};

struct worklist_ctrl;

typedef unsigned int (*get_ready_count) (struct mtk_nand_chip_info *info,
					 struct worklist_ctrl *list_ctrl,
					 int total);

typedef unsigned int (*process_list_data) (struct mtk_nand_chip_info *info,
					   struct worklist_ctrl *list_ctrl,
					   int count);

struct worklist_ctrl {
	struct mutex sync_lock;
	spinlock_t list_lock;
	enum worklist_type type;
	struct list_node head;
	int total_num;
	/* last write error block list, the num is plane_num */
	int *ewrite;
	get_ready_count get_ready_count_func;
	process_list_data process_data_func;
};

struct err_para {
	int rate;
	int count;		/*max count */
	int block;
	int page;
};
struct sim_err {
	struct err_para erase_fail;
	struct err_para write_fail;
	struct err_para read_fail;
	struct err_para bitflip_fail;
	struct err_para bad_block;
};

struct open_block {
	spinlock_t lock;
	int max;
	int *array;
	struct wakeup_source *ws;
};

struct mtk_nand_data_info {
	struct data_bmt_struct bmt;
	struct mtk_nand_chip_bbt_info chip_bbt;
	struct mtk_nand_chip_info chip_info;
	struct nand_ftl_partition_info partition_info;

	struct worklist_ctrl elist_ctrl;
	struct worklist_ctrl swlist_ctrl;
	struct worklist_ctrl wlist_ctrl;
	struct completion ops_ctrl;
	struct task_struct *nand_bgt;

	struct sim_err err;
	struct task_struct *blk_thread;
	struct open_block open;
};

enum TLC_MULTI_PROG_MODE {
	MULTI_BLOCK = 0,
	BLOCK0_ONLY,
	BLOCK1_ONLY,
};

#if defined(CONFIG_PWR_LOSS_MTK_SPOH)
struct mvg_case_stack {
	char gname[63];
	char cname[63];
	struct mvg_case_stack *next;
};
extern int mvg_current_case_check(void);
#endif

int nandx_mntl_ops_init(void);
int nandx_mntl_data_info_alloc(void);
void nandx_mntl_data_info_free(void);
u32 get_ftl_row_addr(struct mtk_nand_chip_info *info,
		     unsigned int block, unsigned int page);
int init_mntl_module(void);

#endif				/* __MNTL_OPS_H__ */
