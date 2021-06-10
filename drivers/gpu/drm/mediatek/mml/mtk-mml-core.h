/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */


#ifndef __MTK_MML_CORE_H__
#define __MTK_MML_CORE_H__

#include <linux/file.h>
#include <linux/list.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>
#include <linux/mailbox_client.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/workqueue.h>

#include "mtk-mml.h"
#include "mtk-mml-driver.h"

extern int mtk_mml_msg;
extern s32 cmdq_pkt_flush_async(struct cmdq_pkt *pkt,
    cmdq_async_flush_cb cb, void *data);

#define mml_msg(fmt, args...) \
do { \
	if (mtk_mml_msg) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_log(fmt, args...) \
do { \
	pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_err(fmt, args...) \
do { \
	pr_notice("[mml][err]" fmt "\n", ##args); \
} while (0)

#define MML_PIPE_CNT		2
#define MML_MAX_PATH_ENGINES	10
#define MML_MAX_PATH_CACHES	6
#define MML_ENGINE_INOUT	3

struct mml_topology_cache;
struct mml_frame_config;
struct mml_task;

struct mml_task_ops {
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
};

struct mml_cap {
	enum mml_mode target;
	enum mml_mode running;
};

struct mml_engine {
	u32 id;
	struct mml_engine *prev;
	struct mml_engine *next[MML_MAX_OUTPUTS];
	struct mml_comp *comp;

	/* index from engine array to tile_engines
	 * in mml_topology_path structure.
	 */
	u8 tile_eng_idx;
};

struct mml_topology_info {
	enum mml_mode mode;
	u8 dst_cnt;
	struct mml_pq_config pq;
};

struct mml_topology_path {
	struct mml_topology_info info;
	bool alpharot;

	/* Engines of this path, each engine may link to others as a tree.
	 * Some of engine like mmlsys, mutex, may not link to others.
	 */
	struct mml_engine engines[MML_MAX_PATH_ENGINES];
	u8 engine_cnt;

	/* special ptr for mmlsys and mutex */
	struct mml_comp *mmlsys;
	u8 mmlsys_eid;
	struct mml_comp *mutex;
	u8 mutex_eid;

	/* Index of engine array,
	 * which represent only engines in data path.
	 * These engines join tile driver calculate.
	 */
	u8 tile_engines[MML_MAX_PATH_ENGINES];
	u8 tile_engine_cnt;

	/* Describe which engine is out0 and which is out1 */
	u32 out_engine_id[MML_MAX_OUTPUTS];

	/* cmdq client to compose command */
	struct cmdq_client *clt;
};

struct mml_topology_ops {
	s32 (*init_cache)(struct mml_dev *mml,
			  struct mml_topology_cache *cache,
			  struct cmdq_client **clts,
			  u8 clt_cnt);
	s32 (*select)(struct mml_topology_cache *cache,
		      struct mml_frame_config *cfg);
};

struct mml_topology_cache {
	const struct mml_topology_ops *op;
	struct mml_topology_path path[MML_MAX_PATH_CACHES];
	u32 cnt;
};

struct mml_frame_config {
	struct list_head entry;
	struct mml_frame_info info;
	struct mutex task_mutex;
	struct list_head tasks;
	struct list_head done_tasks;
	bool dual;
	bool alpharot;

	/* platform driver */
	struct mml_dev *mml;

	/* drm adaptor */
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;

	/* workqueue */
	struct workqueue_struct *wq_config[2];
	struct workqueue_struct *wq_wait;

	/* topology */
	const struct mml_topology_path *path[MML_PIPE_CNT];
};

struct mml_file_buf {
	struct file *f[MML_MAX_PLANES];
	u32 size[MML_MAX_PLANES];
	u8 cnt;
	struct file *fence;
	u32 usage;
};

struct mml_task_buffer {
	struct mml_file_buf src;
	struct mml_file_buf dest[MML_MAX_OUTPUTS];
	u8 dest_cnt;
};

enum mml_task_state {
	MML_TASK_INITIAL,
	MML_TASK_REUSE,
	MML_TASK_RUNNING,
	MML_TASK_IDLE
};

struct mml_task {
	struct list_head entry;
	struct mml_job job;
	struct mml_frame_config *config;
	struct mml_task_buffer buf;
	struct timespec64 end_time;
	struct file *fence;
	enum mml_task_state state;

	/* mml context */
	void *ctx;

	/* command */
	struct cmdq_pkt pkts[MML_PIPE_CNT];

	/* workqueue */
	struct work_struct work_config[2];
	struct work_struct work_wait;
};

struct mml_comp;

struct mml_comp_tile_ops {

};

struct mml_comp_config_ops {

};

struct mml_comp_hw_ops {

};

struct mml_comp_debug_ops {

};

struct mml_comp {
	u32 id;
	void __iomem *base;
	phys_addr_t base_pa;
	struct clk *clks[2];
	struct device *larb_dev;
	bool bound;
	const struct mml_comp_tile_ops *tile_ops;
	const struct mml_comp_config_ops *config_ops;
	const struct mml_comp_hw_ops *hw_ops;
	const struct mml_comp_debug_ops *debug_ops;
};

/*
 * mml_topology_register_ip - Register topology operation to node list in core
 * by giving a name. Core will later call init with IP name to match one of
 * operation node in list.
 *
 * @ip:	name of IP, like mt6983
 * @op: operations of specific IP
 */
void mml_topology_register_ip(const char *ip, const struct mml_topology_ops *op);

/*
 * mml_topology_unregister_ip - Unregister topology operation in node list in
 * core by giving a name. Note all config related to this topology MUST
 * shutdown before unregister.
 *
 * @ip:	name of IP, like mt6983
 */
void mml_topology_unregister_ip(const char *ip);

/*
 * mml_topology_create - Create cache structure and set one of platform
 * topology to giving platform device.
 *
 * @mml:	mml driver private data
 * @pdev:	platform device of mml
 * @clts:	The cmdq clitns array. Client instance will assign to path.
 * @clts_cnt:	Count of cmdq client.
 *
 * Return: Topology cache struct which alloc by giving pdev.
 */
struct mml_topology_cache *mml_topology_create(struct mml_dev *mml,
					       struct platform_device *pdev,
					       struct cmdq_client **clts,
					       u8 clts_cnt);

/**
 * mml_core_create_task -
 *
 * Return:
 */
struct mml_task *mml_core_create_task(void);

/**
 * mml_core_destroy_task -
 * @task:
 *
 */
void mml_core_destroy_task(struct mml_task *task);

/**
 * mml_core_submit_task -
 * @frame_config:
 * @task:
 *
 * Return:
 */
s32 mml_core_submit_task(struct mml_frame_config *frame_config,
			 struct mml_task *task);


#endif	/* __MTK_MML_CORE_H__ */
