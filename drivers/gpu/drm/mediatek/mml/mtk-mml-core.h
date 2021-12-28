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
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/mtk-cmdq-mailbox-ext.h>
#include <linux/mailbox_client.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <mtk-interconnect.h>
#include <cmdq-util.h>
#include "mtk-mml.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-pq.h"

extern int mtk_mml_msg;

extern int mml_cmdq_err;

#define mml_msg(fmt, args...) \
do { \
	if (mtk_mml_msg) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

#define mml_log(fmt, args...) \
do { \
	pr_notice("[mml]" fmt "\n", ##args); \
	if (mml_cmdq_err) \
		cmdq_util_error_save("[mml]"fmt"\n", ##args); \
} while (0)

#define mml_err(fmt, args...) \
do { \
	pr_notice("[mml][err]" fmt "\n", ##args); \
	if (mml_cmdq_err) \
		cmdq_util_error_save("[mml]"fmt"\n", ##args); \
} while (0)

/* mml ftrace */
extern int mml_trace;

#define MML_TTAG_OVERDUE	"mml_endtime_overdue"
#define MML_TID_IRQ		0	/* trace on <idle>-0 process */

#define mml_trace_begin_tid(tid, fmt, args...) \
	tracing_mark_write("B|%d|" fmt "\n", tid, ##args)

#define mml_trace_begin(fmt, args...) \
	mml_trace_begin_tid(current->tgid, fmt, ##args)

#define mml_trace_end() \
	tracing_mark_write("E\n")

#define mml_trace_c(tag, c) \
	tracing_mark_write("C|%d|%s|%d\n", current->tgid, tag, c)

#define mml_trace_tag_start(tag) mml_trace_c(tag, 1)

#define mml_trace_tag_end(tag) mml_trace_c(tag, 0)

#define mml_trace_ex_begin(fmt, args...) do { \
	if (mml_trace) \
		mml_trace_begin(fmt, ##args); \
} while (0)

#define mml_trace_ex_end() do { \
	if (mml_trace) \
		mml_trace_end(); \
} while (0)

/* mml pq control */
extern int mml_pq_disable;

/* mml slt */
extern int mml_slt;

/* racing mode ut and debug */
extern int mml_racing_ut;
extern int mml_racing_timeout;
extern int mml_racing_urgent;
extern int mml_racing_wdone_eoc;

#define MML_PIPE_CNT		2
#define MML_MAX_PATH_NODES	16 /* must align MAX_TILE_FUNC_NO in tile_driver.h */
#define MML_MAX_PATH_CACHES	16
#define MML_MAX_CMDQ_CLTS	4
#define MML_MAX_OPPS		5
#define MML_MAX_TPUT		800
#define MML_CMDQ_NEXT_SPR	(CMDQ_GPR_CNT_ID + CMDQ_GPR_R09)
#define MML_CMDQ_NEXT_SPR2	(CMDQ_GPR_CNT_ID + CMDQ_GPR_R11)
#define MML_CMDQ_ROUND_SPR	CMDQ_THR_SPR_IDX3
#define MML_ROUND_SPR_INIT	0x8000
#define MML_NEXTSPR_NEXT	BIT(0)
#define MML_NEXTSPR_DUAL	BIT(1)


struct mml_topology_cache;
struct mml_frame_config;
struct mml_task;
struct mml_tile_output;

struct mml_task_ops {
	void (*queue)(struct mml_task *task, u32 pipe);
	void (*submit_done)(struct mml_task *task);
	void (*frame_done)(struct mml_task *task);
	/* optional: adaptor may use frame_done to handle error */
	void (*frame_err)(struct mml_task *task);
	s32 (*dup_task)(struct mml_task *task, u32 pipe);
	struct mml_tile_cache *(*get_tile_cache)(struct mml_task *task, u32 pipe);
	void (*kt_setsched)(void *adaptor_ctx);
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

	/* sw workaround, cmdq use pipe to decide which hardware to be config */
	u8 hw_pipe;

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
	u8 clt_id;
	struct cmdq_client *clt;

	/* Path configurations */
	u8 mux_group;
	union {
		u64 reset_bits;
		struct {
			u32 reset0;
			u32 reset1;
		};
	};
};

struct mml_topology_ops {
	enum mml_mode (*query_mode)(struct mml_dev *mml,
				    struct mml_frame_info *info);
	s32 (*init_cache)(struct mml_dev *mml,
			  struct mml_topology_cache *cache,
			  struct cmdq_client **clts,
			  u32 clt_cnt);
	s32 (*select)(struct mml_topology_cache *cache,
		      struct mml_frame_config *cfg);
	struct cmdq_client *(*get_racing_clt)(struct mml_topology_cache *cache,
					      u32 pipe);
};

struct mml_path_client {
	/* running tasks on same cients from all configs */
	struct list_head tasks;
	/* lock to this client */
	struct mutex clt_mutex;
	/* current throughput */
	u32 throughput;
};

struct mml_topology_cache {
	const struct mml_topology_ops *op;
	struct mml_topology_path paths[MML_MAX_PATH_CACHES];
	struct mml_path_client path_clts[MML_MAX_CMDQ_CLTS];
	struct regulator *reg;
	u32 opp_cnt;
	u32 opp_speeds[MML_MAX_OPPS];
	int opp_volts[MML_MAX_OPPS];
	u64 freq_max;
	struct mutex qos_mutex;
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
	u32 label_cnt;

	/* Fillin when core call prepare. Use in prepare and make command */
	struct mml_comp_config cfg[MML_MAX_PATH_NODES];

	/* qos part */
	u32 total_datasize;
	u32 max_pixel;
};

struct mml_frame_config {
	struct list_head entry;
	struct mml_frame_info info;
	struct mml_frame_size frame_out[MML_MAX_OUTPUTS];
	struct list_head tasks;
	struct list_head await_tasks;
	struct list_head done_tasks;
	u32 run_task_cnt;
	u32 await_task_cnt;
	u8 done_task_cnt;
	/* mutex to join operations of task pipes, like buffer flush */
	struct mutex pipe_mutex;
	struct kref ref;

	/* display parameter */
	bool disp_dual;
	bool disp_vdo;

	/* platform driver */
	void *ctx;
	struct mml_dev *mml;

	/* adaptor */
	u32 last_jobid;

	/* core */
	const struct mml_task_ops *task_ops;

	/* workqueue for handling slow part of task done */
	struct workqueue_struct *wq_done;

	/* kthread worker for task done, assign from ctx */
	struct kthread_worker *ctx_kt_done;

	/* use on context wq_destroy */
	struct work_struct work_destroy;

	/* cache for pipe and component private data for this config */
	struct mml_pipe_cache cache[MML_PIPE_CNT];

	/* topology */
	const struct mml_topology_path *path[MML_PIPE_CNT];
	bool dual:1;
	bool alpharot:1;
	bool shadow:1;
	bool framemode:1;
	bool nocmd:1;

	/* tile */
	struct mml_tile_output *tile_output[MML_PIPE_CNT];
};

struct mml_dma_buf {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table* sgt;

	u64 iova;
	void *va;
};

struct mml_file_buf {
	/* dma buf heap */
	struct mml_dma_buf dma[MML_MAX_PLANES];
	u32 size[MML_MAX_PLANES];
	u8 cnt;
	struct dma_fence *fence;
	u32 usage;
	u64 map_time;
	u64 unmap_time;

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

struct mml_task_reuse {
	struct cmdq_reuse *labels;
	u32 label_idx;
};

/* pipe info for mml_task */
struct mml_task_pipe {
	struct mml_task *task;	/* back to task */
	struct list_head entry_clt;
	u32 throughput;
	u32 bandwidth;
};

struct mml_task {
	struct list_head entry;
	struct mml_job job;
	struct mml_frame_config *config;
	struct mml_task_buffer buf;
	struct mml_pq_param pq_param[MML_MAX_OUTPUTS];
	struct timespec64 submit_time;
	struct timespec64 end_time;
	struct dma_fence *fence;
	enum mml_task_state state;
	struct kref ref;
	struct mml_task_pipe pipe[MML_PIPE_CNT];

	/* mml context */
	void *ctx;
	void *cb_param;

	/* command */
	struct cmdq_pkt *pkts[MML_PIPE_CNT];

	/* make command cache labels for reuse command */
	struct mml_task_reuse reuse[MML_PIPE_CNT];

	/* config and done on thread */
	struct work_struct work_config[MML_PIPE_CNT];
	struct work_struct wq_work_done;
	struct kthread_work kt_work_done;
	atomic_t pipe_done;

	/* mml pq task */
	struct mml_pq_task *pq_task;
};

struct tile_func_block;
union mml_tile_data;

struct mml_comp_tile_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg,
		       struct tile_func_block *func,
		       union mml_tile_data *data);
};

struct mml_comp_config_ops {
	s32 (*prepare)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg);
	s32 (*buf_map)(struct mml_comp *comp, struct mml_task *task,
		       const struct mml_path_node *node);
	void (*buf_unmap)(struct mml_comp *comp, struct mml_task *task,
			  const struct mml_path_node *node);
	s32 (*buf_prepare)(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg);
	void (*buf_unprepare)(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg);
	u32 (*get_label_count)(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg);
	/* op to make command in frame change case */
	s32 (*init)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg);
	s32 (*frame)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg);
	s32 (*tile)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg, u32 idx);
	s32 (*mutex)(struct mml_comp *comp, struct mml_task *task,
		     struct mml_comp_config *ccfg);
	s32 (*wait)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg, u32 idx);
	s32 (*post)(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg);
	/* op to make command in reuse case */
	s32 (*reframe)(struct mml_comp *comp, struct mml_task *task,
		       struct mml_comp_config *ccfg);
	s32 (*repost)(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg);
};

struct mml_comp_hw_ops {
	s32 (*pw_enable)(struct mml_comp *comp);
	s32 (*pw_disable)(struct mml_comp *comp);
	s32 (*clk_enable)(struct mml_comp *comp);
	s32 (*clk_disable)(struct mml_comp *comp);
	u32 (*qos_datasize_get)(struct mml_task *task,
				struct mml_comp_config *ccfg);
	u32 (*qos_format_get)(struct mml_task *task,
			      struct mml_comp_config *ccfg);
	void (*qos_set)(struct mml_comp *comp, struct mml_task *task,
			struct mml_comp_config *ccfg, u32 throughput, u32 tput_up);
	void (*qos_clear)(struct mml_comp *comp);
	void (*task_done)(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg);
};

struct mml_comp_debug_ops {
	void (*dump)(struct mml_comp *comp);
	void (*reset)(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe);
};

struct mml_comp {
	u32 id;
	u32 sub_idx;
	void __iomem *base;
	phys_addr_t base_pa;
	struct clk *clks[2];
	struct device *larb_dev;
	phys_addr_t larb_base;
	u32 larb_port;
	s32 pw_cnt;
	s32 clk_cnt;
	struct icc_path *icc_path;
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

	/* assign by wrot, end of current tile line */
	bool eol;
};

/* array size must align MAX_TILE_FUNC_NO in tile_driver.h */
struct mml_tile_cache {
	void *func_list[MML_MAX_PATH_NODES];
	struct mml_tile_config *tiles;
	bool ready;
};

struct mml_tile_output {
	/* total tile number */
	u16 tile_cnt;
	/* total horizontal tile number */
	u16 h_tile_cnt;
	/* total vertical tile number */
	u16 v_tile_cnt;
	struct mml_tile_config *tiles;
};

struct mml_frm_dump_data {
	const char *prefix;
	char name[50];
	void *frame;
	u32 bufsize;
	u32 size;
};

/* config_get_tile - helper inline func which uses tile index to get
 * mml_tile_engine instance inside config.
 *
 * @cfg:	The mml_frame_config contains tile output.
 * @ccfg:	The mml_comp_config of which tile engine.
 * @idx:	Tile index to mml_tile_output->tiles array.
 *
 * Return:	mml_tile_engine struct pointer to related tile and engine.
 */
static inline struct mml_tile_engine *config_get_tile(
	struct mml_frame_config *cfg, struct mml_comp_config *ccfg, u32 idx)
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
					       u32 clts_cnt);

/*
 * mml_core_get_frame_in - return input frame dump
 *
 * Return:	The frame dump instance for input frame
 */
struct mml_frm_dump_data *mml_core_get_frame_in(void);

/*
 * mml_core_get_frame_out - return input frame dump
 *
 * Return:	The frame dump instance for output frame
 */
struct mml_frm_dump_data *mml_core_get_frame_out(void);

/*
 * mml_core_get_dump_inst - return debug dump buffer with current buf size
 *
 * @size:	buffer size in bytes
 *
 * Return:	The inst buffer in string.
 */
char *mml_core_get_dump_inst(u32 *size);

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
 * mml_core_init_config - initialize data use in core.
 *
 * @cfg: The frame config to be init.
 */
void mml_core_init_config(struct mml_frame_config *cfg);

/*
 * mml_core_deinit_config - destroy meta or content store in frame config
 * which allocated in core.
 *
 * @cfg: The frame config to be deinit.
 */
void mml_core_deinit_config(struct mml_frame_config *cfg);

/**
 * mml_core_config_task - config the task in current thread
 *
 * @cfg:	the frame config to handle
 * @task:	task to execute
 */
void mml_core_config_task(struct mml_frame_config *cfg, struct mml_task *task);

/**
 * mml_core_submit_task - queue the task in config work thread
 *
 * @cfg:	the frame config to queue
 * @task:	task to execute
 */
void mml_core_submit_task(struct mml_frame_config *cfg, struct mml_task *task);

/**
 * mml_core_stop_racing - set next spr to 1 to stop current racing task
 *
 * @cfg:	the frame config to stop
 * @force:	true to use cmdq stop gce hardware thread, false to set next_spr
 *		to next only.
 */
void mml_core_stop_racing(struct mml_frame_config *cfg, bool force);

/* mml_assign - assign to reg_idx with value. Cache the label of this
 * instruction to mml_pipe_cache and record its entry into label_array.
 *
 * @pkt:	cmdq task
 * @reg_idx:	common purpose register index
 * @value:	value to write
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @cache:	task cache from mml config
 * @label_idx:	ptr to label entry point to write instruction
 *
 * return:	0 if success, error no if fail
 */
s32 mml_assign(struct cmdq_pkt *pkt, u16 reg_idx, u32 value,
	       struct mml_task_reuse *reuse,
	       struct mml_pipe_cache *cache,
	       u16 *label_idx);

/* mml_write - write to addr with value and mask. Cache the label of this
 * instruction to mml_pipe_cache and record its entry into label_array.
 *
 * @pkt:	cmdq task
 * @addr:	register addr or dma addr
 * @value:	value to write
 * @mask:	mask to value
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @cache:	task cache from mml config
 * @label_idx:	ptr to label entry point to write instruction
 *
 * return:	0 if success, error no if fail
 */
s32 mml_write(struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	      struct mml_task_reuse *reuse,
	      struct mml_pipe_cache *cache,
	      u16 *label_idx);

/* mml_update - update new value to cache, which entry index from label.
 *
 * @reuse:	label cache for cmdq_reuse from task, which caches label of
 *		this task and pipe.
 * @label_idx:	label entry point to instruction want to update
 * @value:	value to be update
 */
void mml_update(struct mml_task_reuse *reuse, u16 label_idx, u32 value);

int tracing_mark_write(char *fmt, ...);

#endif	/* __MTK_MML_CORE_H__ */
