/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */


#ifndef __MTK_MML_CORE_H__
#define __MTK_MML_CORE_H__

#include <linux/atomic.h>
#include <linux/dma-fence.h>
#include <linux/file.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/mailbox_client.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/trace_events.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include "mtk-mml.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-driver.h"

extern int mtk_mml_msg;

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

/* mml ftrace */
extern int mml_trace;
#ifdef IF_ZERO // [FIXME] to check with KS team
#define mml_trace_begin(fmt, args...) do { \
	preempt_disable(); \
	event_trace_printk(mml_get_tracing_mark(), \
		"B|%d|"fmt"\n", current->tgid, ##args); \
	preempt_enable();\
} while (0)

#define mml_trace_end() do { \
	preempt_disable(); \
	event_trace_printk(mml_get_tracing_mark(), "E\n"); \
	preempt_enable(); \
} while (0)

#define mml_trace_ex_begin(fmt, args...) do { \
	if (mml_trace) \
		mml_trace_begin(fmt, ##args); \
} while (0)

#define mml_trace_ex_end() do { \
	if (mml_trace) \
		mml_trace_end(); \
} while (0)
#else
#define mml_trace_begin(fmt, args...) do {} while(0)
#define mml_trace_end() do {} while(0)
#define mml_trace_ex_begin(fmt, args...) do {} while(0)
#define mml_trace_ex_end() do {} while(0)
#endif

#define MML_PIPE_CNT		2
#define MML_MAX_PATH_NODES	12
#define MML_MAX_PATH_CACHES	6

struct mml_topology_cache;
struct mml_frame_config;
struct mml_task;
struct mml_tile_output;

struct mml_task_ops {
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
	s32 (*dup_task)(struct mml_task *task, u8 pipe);
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
	u8 tile_eng_idx;

	/* The component private data. Components can store list of labels or
	 * more info for specific component data in this ptr.
	 */
	void *data;
};

struct mml_pipe_cache {
	/* make command cache labels for reuse command */
	struct cmdq_reuse *labels;
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
	u32 run_task_cnt;
	u32 await_task_cnt;
	u8 done_task_cnt;
	bool dual;
	bool alpharot;
	struct mutex pipe_mutex;

	/* platform driver */
	struct mml_dev *mml;

	/* drm adaptor */
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;

	/* workqueue */
	struct workqueue_struct *wq_config[MML_PIPE_CNT];
	struct workqueue_struct *wq_wait;

	/* use on context wq_destroy */
	struct work_struct work_destroy;

	/* cache for pipe and component private data for this config */
	struct mml_pipe_cache cache[MML_PIPE_CNT];

	/* topology */
	const struct mml_topology_path *path[MML_PIPE_CNT];

	/* tile */
	struct mml_tile_output *tile_output[MML_PIPE_CNT];
};

struct mml_dma_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table* sgt;

	u64 iova;
};

struct mml_file_buf {
	/* dma buf heap */
	struct mml_dma_buf dma[MML_MAX_PLANES];
	u32 size[MML_MAX_PLANES];
	u8 cnt;
	struct dma_fence *fence;
	u32 usage;

	bool flush:1;
	bool invalid:1;
};

struct mml_task_buffer {
	struct mml_file_buf src;
	struct mml_file_buf dest[MML_MAX_OUTPUTS];
	u8 dest_cnt;
	bool flushed;
};

enum mml_task_state {
	MML_TASK_INITIAL,
	MML_TASK_DUPLICATE,
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
	struct dma_fence *fence;
	enum mml_task_state state;
	struct kref ref;

	/* mml context */
	void *ctx;

	/* command */
	struct cmdq_pkt *pkts[MML_PIPE_CNT];

	/* workqueue */
	struct work_struct work_config[MML_PIPE_CNT];
	struct work_struct work_wait;
	atomic_t pipe_done;
};

struct mml_comp_tile_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *priv,
		       void *ptr_func,
		       void *tile_data);
};

struct mml_comp_config_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *priv);
	s32 (*buf_map)(struct mml_comp *comp, struct mml_task *task,
		       const struct mml_path_node *node);
	s32 (*buf_prepare)(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg);
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
	void (*dump)(struct mml_comp *comp);
};

struct mml_comp {
	u32 id;
	u32 sub_idx;
	void __iomem *base;
	phys_addr_t base_pa;
	struct clk *clks[2];
	struct device *larb_dev;
	const struct mml_comp_tile_ops *tile_ops;
	const struct mml_comp_config_ops *config_ops;
	const struct mml_comp_hw_ops *hw_ops;
	const struct mml_comp_debug_ops *debug_ops;
	const char *name;
	bool bound;
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

static inline struct mml_tile_engine *config_get_tile(
	struct mml_frame_config *cfg, struct mml_comp_config *ccfg, u8 idx)
{
	struct mml_tile_engine *engines =
		cfg->tile_output[ccfg->pipe]->tiles[idx].tile_engines;

	return &engines[ccfg->tile_eng_idx];
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

/* mml_write - write to addr with value and mask. Cache the label of this
 * instruction to mml_pipe_cache and record its entry into label_array.
 *
 * @pkt:	cmdq task
 * @addr:	register addr or dma addr
 * @value:	value to write
 * @mask:	mask to value
 * @cache:	instruction cache from mml config
 * @label_idx:	ptr to label entry point to write instruction
 *
 * return:	0 if success, error no if fail
 */
s32 mml_write(struct cmdq_pkt *pkt, dma_addr_t addr, u32 value,
	u32 mask, struct mml_pipe_cache *cache, u16 *label_idx);

/* mml_update - update new value to cache, which entry index from label.
 *
 * @cache:	instruction cache from mml config
 * @label_idx:	label entry point to instruction want to update
 * @value:	value to be update
 */
void mml_update(struct mml_pipe_cache *cache, u16 label_idx, u32 value);

unsigned long mml_get_tracing_mark(void);

#endif	/* __MTK_MML_CORE_H__ */
