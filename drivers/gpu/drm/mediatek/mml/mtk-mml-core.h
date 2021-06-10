/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */


#ifndef __MTK_MML_CORE_H__
#define __MTK_MML_CORE_H__

#include <linux/atomic.h>
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
#define MML_MAX_PATH_NODES	10
#define MML_MAX_PATH_CACHES	6

struct mml_topology_cache;
struct mml_frame_config;
struct mml_task;
struct mml_tile_output;

struct mml_task_ops {
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
};

struct mml_cap {
	enum mml_mode target;
	enum mml_mode running;
};

struct mml_path_node {
	u8 id;
	struct mml_path_node *prev;
	struct mml_path_node *next[MML_MAX_OUTPUTS];
	struct mml_comp *comp;
	u8 out_idx;

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

	/* Nodes of this path, each node may link to others as a tree.
	 * Some of nodes like mmlsys, mutex, may not link to others.
	 */
	struct mml_path_node nodes[MML_MAX_PATH_NODES];
	u8 node_cnt;

	/* special ptr for mmlsys and mutex */
	struct mml_comp *mmlsys;
	u8 mmlsys_idx;
	struct mml_comp *mutex;
	u8 mutex_idx;

	/* Index of engine array,
	 * which represent only engines in data path.
	 * These engines join tile driver calculate.
	 */
	u8 tile_engines[MML_MAX_PATH_NODES];
	u8 tile_engine_cnt;

	/* Describe which engine is out0 and which is out1 */
	u32 out_engine_ids[MML_MAX_OUTPUTS];

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

struct mml_comp_config {
	u8 pipe;
	const struct mml_path_node *node;
	u8 node_idx;

	/* The component private data. Components can store list of labels or
	 * more info for specific component data in this ptr.
	 */
	void *data;
};

struct mml_label {
	u32 offset;
	u32 val;
};

struct mml_pipe_cache {
	/* make command cache labels for reuse command */
	struct mml_label *labels;
	u32 label_cnt;
	u32 label_idx;

	/* Fillin when core call prepare. Use in prepare and make command */
	struct mml_comp_config cfg[MML_MAX_PATH_NODES];
};

struct mml_frame_config {
	struct list_head entry;
	struct mml_frame_info info;
	struct mutex task_mutex;
	struct list_head tasks;
	struct list_head await_tasks;
	struct list_head done_tasks;
	u8 done_task_cnt;
	bool dual;
	bool alpharot;

	/* platform driver */
	struct mml_dev *mml;

	/* drm adaptor */
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;

	/* workqueue */
	struct workqueue_struct *wq_config[MML_PIPE_CNT];
	struct workqueue_struct *wq_wait;

	/* cache for pipe and component private data for this config */
	struct mml_pipe_cache cache[MML_PIPE_CNT];

	/* topology */
	const struct mml_topology_path *path[MML_PIPE_CNT];

	/* tile */
	struct mml_tile_output *tile_output[MML_PIPE_CNT];
};

struct mml_file_buf {
	struct file *f[MML_MAX_PLANES];
	u32 size[MML_MAX_PLANES];
	u64 iova[MML_MAX_PLANES];
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
	struct cmdq_pkt *pkts[MML_PIPE_CNT];

	/* workqueue */
	struct work_struct work_config[2];
	struct work_struct work_wait;
	atomic_t pipe_done;
};

struct mml_comp_tile_ops {
	u32 (*get_out_w)(struct mml_comp_config *ccfg);
	u32 (*get_out_h)(struct mml_comp_config *ccfg);
};

struct mml_comp_config_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *priv);
	u32 (*get_label_count)(struct mml_comp *comp, struct mml_task *task);
	/* op to make command in frame change case */
	s32 (*init)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *priv);
	s32 (*frame)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *priv);
	s32 (*tile)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *priv, u8 idx);
	s32 (*mutex)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *priv);
	s32 (*wait)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *priv);
	s32 (*post)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *priv);
	/* op to make command in reuse case */
	s32 (*reframe)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *priv);
	s32 (*repost)(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *priv);
};

struct mml_comp_hw_ops {
	s32 (*pw_enable)(struct mml_comp *comp);
	s32 (*pw_disable)(struct mml_comp *comp);
	s32 (*clk_enable)(struct mml_comp *comp);
	s32 (*clk_disable)(struct mml_comp *comp);
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

struct mml_tile_region {
	u16 xs;
	u16 xe;
	u16 ys;
	u16 ye;
};

struct mml_tile_offset {
	u16 x;
	u16 y;
	u32 x_sub;
	u32 y_sub;
};

struct mml_tile_engine {
	/* component id for dump */
	u8 comp_id;

	/* tile input/output region */
	struct mml_tile_region in;
	struct mml_tile_region out;

	struct mml_tile_offset luma;
	struct mml_tile_offset chroma;
};

struct mml_tile_config {
	/* index of the tile */
	u16 tile_no;

	/* current horizontal tile number, from left to right */
	u16 h_tile_no;
	/* current vertical tile number, from top to bottom */
	u16 v_tile_no;

	/* align with tile_engine_cnt */
	u8 engine_cnt;
	struct mml_tile_engine tile_engines[MML_MAX_PATH_NODES];
};

struct mml_tile_output {
	/* total how many tiles */
	u16 tile_cnt;
	struct mml_tile_config *tiles;
};

/* task_comp - helper inline func which use pipe id and node index to get
 * mml_comp instance inside topology.
 *
 * @task:	The mml_task contains frame config.
 * @pipe:	Pipe ID, 0 or 1.
 * @node_idx:	Node index to mml_topology_path->nodes array.
 *
 * Return:	mml_comp struct pointer to related engine
 */
static inline struct mml_comp *task_comp(struct mml_task *task, u8 pipe,
					 u8 node_idx)
{
	return task->config->path[pipe]->nodes[node_idx].comp;
}

/*
 * mml_topology_register_ip - Register topology operation to node list in core
 * by giving a name. Core will later call init with IP name to match one of
 * operation node in list.
 *
 * @ip:	name of IP, like mt6983
 * @op: operations of specific IP
 *
 * Return:
 */
int mml_topology_register_ip(const char *ip, const struct mml_topology_ops *op);

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

/*
 * mml_core_deinit_config - destroy meta or content store in frame config
 * which allocated in core.
 *
 * @config: The frame config to be deinit.
 */
void mml_core_deinit_config(struct mml_frame_config *config);

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
