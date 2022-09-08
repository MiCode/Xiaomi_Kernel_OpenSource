/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DEV_H_
#define _MTK_DIP_DEV_H_

#include <linux/completion.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/remoteproc.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#include "mtk_imgsys-hw.h"
#include "mtk_imgsys-module.h"
#include "mtkdip.h"
#include "mtk-interconnect.h"
#include "mtk_imgsys-worker.h"

#define MTK_IMGSYS_PIPE_ID_PREVIEW				0
#define MTK_IMGSYS_PIPE_ID_CAPTURE				1
#define MTK_IMGSYS_PIPE_ID_REPROCESS			2
#define MTK_IMGSYS_PIPE_ID_TOTAL_NUM			1

#define MTK_DIP_OUTPUT_MIN_WIDTH		2U
#define MTK_DIP_OUTPUT_MIN_HEIGHT		2U
#define MTK_DIP_OUTPUT_MAX_WIDTH		5376U
#define MTK_DIP_OUTPUT_MAX_HEIGHT		4032U
#define MTK_DIP_CAPTURE_MIN_WIDTH		2U
#define MTK_DIP_CAPTURE_MIN_HEIGHT		2U
#define MTK_DIP_CAPTURE_MAX_WIDTH		5376U
#define MTK_DIP_CAPTURE_MAX_HEIGHT		4032U

#define MTK_DIP_DEV_DIP_MEDIA_MODEL_NAME	"MTK-ISP-DIP-V4L2"
#define MTK_DIP_DEV_DIP_PREVIEW_NAME \
	MTK_DIP_DEV_DIP_MEDIA_MODEL_NAME
#define MTK_DIP_DEV_DIP_CAPTURE_NAME		"MTK-ISP-DIP-CAP-V4L2"
#define MTK_DIP_DEV_DIP_REPROCESS_NAME		"MTK-ISP-DIP-REP-V4L2"

#define MTK_DIP_DEV_META_BUF_DEFAULT_SIZE	(1024 * 128)
#define MTK_DIP_DEV_META_BUF_POOL_MAX_SIZE	(1024 * 1024 * 16)
#define MTK_IMGSYS_OPP_SET			2
#define MTK_IMGSYS_CLK_LEVEL_CNT		5
#define MTK_IMGSYS_DVFS_GROUP			4
#define MTK_IMGSYS_QOS_GROUP			2

#define MTK_IMGSYS_LOG_LENGTH			1024

extern unsigned int nodes_num;

#define	MTK_IMGSYS_VIDEO_NODE_SIGDEV_NORM_OUT	(nodes_num - 1)
#define	MTK_IMGSYS_VIDEO_NODE_SIGDEV_OUT	(nodes_num - 2)
#define	MTK_IMGSYS_VIDEO_NODE_CTRLMETA_OUT	(nodes_num - 3)
#define	MTK_IMGSYS_VIDEO_NODE_TUNING_OUT	(nodes_num - 4)


enum imgsys_user_state {
	DIP_STATE_INIT	= 0,
	DIP_STATE_STREAMON,
	DIP_STATE_STREAMOFF
};

enum mtk_imgsys_pixel_mode {
	mtk_imgsys_pixel_mode_default = 0,
	mtk_imgsys_pixel_mode_1,
	mtk_imgsys_pixel_mode_2,
	mtk_imgsys_pixel_mode_4,
	mtk_imgsys_pixel_mode_num,
};

struct mtk_imgsys_dev_format {
	u32 format;
	u32 mdp_color;
	u8 depth[VIDEO_MAX_PLANES];
	u8 row_depth[VIDEO_MAX_PLANES];
	u8 num_planes;
	u8 num_cplanes;
	u32 flags;
	u32 buffer_size;
	u8 pass_1_align;
};

// desc added {
#define HBITS (32)
struct mtk_imgsys_dma_buf_iova_list {
	struct list_head list;
	struct hlist_head hlists[HBITS];
	spinlock_t lock;
	struct mutex mlock;
};

struct mtk_imgsys_dma_buf_iova_get_info {
	s32 ionfd;
	dma_addr_t dma_addr;
	/* ION case only */
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct list_head list_entry;
	struct hlist_node hnode;
};
// } desc added

struct mtk_imgsys_dev_buffer {
	struct vb2_v4l2_buffer vbb;
	struct v4l2_format fmt;
	const struct mtk_imgsys_dev_format *dev_fmt;
	int pipe_job_id;
	dma_addr_t isp_daddr[VB2_MAX_PLANES];
	dma_addr_t scp_daddr[VB2_MAX_PLANES];
	u64 va_daddr[VB2_MAX_PLANES];
	__u32 dma_port;
	struct mtk_imgsys_crop crop;
	struct v4l2_rect compose;
	__u32 rotation;
	__u32 hflip;
	__u32 vflip;
	__u32 dataofst;
	struct list_head list;

// desc added {
//Keep dmabuf used in latter for put kva
	struct dma_buf *dma_buf_putkva;
	struct mtk_imgsys_dma_buf_iova_list iova_map_table;
// } desc added
};

struct mtk_imgsys_pipe_desc {
	char *name;
	int id;
	struct mtk_imgsys_video_device_desc *queue_descs;
	int total_queues;
};

struct mtk_imgsys_video_device_desc {
	int id;
	char *name;
	u32 buf_type;
	u32 cap;
	int smem_alloc;
	int supports_ctrls;
	const struct mtk_imgsys_dev_format *fmts;
	int num_fmts;
	char *description;
	int default_width;
	int default_height;
	unsigned int dma_port;
	const struct v4l2_frmsizeenum *frmsizeenum;
	const struct v4l2_ioctl_ops *ops;
	const struct vb2_ops *vb2_ops;
	u32 flags;
	int default_fmt_idx;
};

struct mtk_imgsys_dev_queue {
	struct vb2_queue vbq;
	/* Serializes vb2 queue and video device operations */
	struct mutex lock;
	const struct mtk_imgsys_dev_format *dev_fmt;
};

struct mtk_imgsys_video_device {
	struct video_device vdev;
	struct mtk_imgsys_dev_queue dev_q;
	struct v4l2_format vdev_fmt;
	struct media_pad vdev_pad;
	struct v4l2_mbus_framefmt pad_fmt;
	struct v4l2_ctrl_handler ctrl_handler;
	u32 flags;
	const struct mtk_imgsys_video_device_desc *desc;
	struct list_head buf_list;
	struct v4l2_rect crop;
	struct v4l2_rect compose;
	int rotation;
	/* protect the in-device buffer list */
	spinlock_t buf_list_lock;
};

struct mtk_imgsys_pipe {
	struct mtk_imgsys_dev *imgsys_dev;
	struct mtk_imgsys_video_device *nodes;
	unsigned long long nodes_streaming;
	unsigned long long nodes_enabled;
	int streaming;
	struct media_pad *subdev_pads;
	struct media_pipeline pipeline;
	struct v4l2_subdev subdev;
	struct v4l2_subdev_fh *fh;
	atomic_t pipe_job_sequence;
	struct list_head pipe_job_pending_list;
	spinlock_t pending_job_lock;
	int num_pending_jobs;
	struct list_head pipe_job_running_list;
	spinlock_t running_job_lock;
	int num_jobs;
	/* Serializes pipe's stream on/off and buffers enqueue operations */
	struct mutex lock;
	//spinlock_t job_lock; /* protect the pipe job list */
	//struct mutex job_lock;
	const struct mtk_imgsys_pipe_desc *desc;
	struct mtk_imgsys_dma_buf_iova_list iova_cache;
	struct init_info init_info;
};

struct imgsys_event_status {
	__u32 req_fd;
	__u32 frame_number;
};

struct imgsys_user_list {
	// list for mtk_imgsys_user
	struct list_head list;
	struct mutex user_lock;
};

struct mtk_imgsys_dvfs {
	struct device *dev;
	struct regulator *reg;
	unsigned int clklv_num[MTK_IMGSYS_OPP_SET];
	unsigned int clklv[MTK_IMGSYS_OPP_SET][MTK_IMGSYS_CLK_LEVEL_CNT];
	unsigned int voltlv[MTK_IMGSYS_OPP_SET][MTK_IMGSYS_CLK_LEVEL_CNT];
	unsigned int clklv_idx[MTK_IMGSYS_OPP_SET];
	unsigned int clklv_target[MTK_IMGSYS_OPP_SET];
	unsigned int cur_volt;
	unsigned long pixel_size[MTK_IMGSYS_DVFS_GROUP];
	unsigned long freq;
	unsigned int vss_task_cnt;
	unsigned int smvr_task_cnt;
	unsigned int stream_4k60_task_cnt;
};

struct mtk_imgsys_qos_path {
	struct icc_path *path;	/* cmdq event enum value */
	char dts_name[256];
	unsigned long long bw;
};

struct mtk_imgsys_qos {
	struct device *dev;
	struct mtk_imgsys_qos_path *qos_path;
	unsigned long bw_total[MTK_IMGSYS_DVFS_GROUP][MTK_IMGSYS_QOS_GROUP];
	unsigned long ts_total[MTK_IMGSYS_DVFS_GROUP];
	unsigned long req_cnt;
	bool isIdle;
};

struct gce_work {
	struct list_head entry;
	struct work_pool *pool;
	struct imgsys_work work;
	struct mtk_imgsys_request *req;
	void *req_sbuf_kva;
};

#define GCE_WORK_NR (128)
struct work_pool {
	atomic_t num;
	struct list_head free_list;
	struct list_head used_list;
	spinlock_t lock;
	wait_queue_head_t waitq;
	void *_cookie;
	struct kref kref;
};

#define RUNNER_WQ_NR (4)
typedef void (*debug_dump)(struct mtk_imgsys_dev *imgsys_dev, 		\
	const struct module_ops *imgsys_modules, int imgsys_module_num,	\
	unsigned int hw_comb);

struct mtk_imgsys_dev {
	struct device *dev;
	struct device *dev_Me;
	struct resource *imgsys_resource;
	struct media_device mdev;
	struct v4l2_device v4l2_dev;
	struct mtk_imgsys_pipe imgsys_pipe[MTK_IMGSYS_PIPE_ID_TOTAL_NUM];
	struct clk_bulk_data *clks;
	int num_clks;
	int num_mods;
	struct workqueue_struct *enqueue_wq;
	struct workqueue_struct *composer_wq;
	struct workqueue_struct *mdp_wq[RUNNER_WQ_NR];
	struct imgsys_queue runnerque;
	wait_queue_head_t flushing_waitq;

	/* CCU control flow */
	struct rproc *rproc_ccu_handle;
	/* larb control */
	struct device **larbs;
	unsigned int larbs_num;

	struct work_pool gwork_pool;
	atomic_t num_composing;	/* increase after ipi */
	/*MDP/GCE callback workqueue */
	struct workqueue_struct *mdpcb_wq;
	/* for MDP driver  */
	struct platform_device *mdp_pdev;
	/* for SCP driver  */
	struct platform_device *scp_pdev;
	struct rproc *rproc_handle;
	struct mtk_imgsys_hw_working_buf_list imgsys_freebufferlist;
	struct mtk_imgsys_hw_working_buf_list imgsys_usedbufferlist;
	dma_addr_t working_buf_mem_scp_daddr;
	void *working_buf_mem_vaddr;
	dma_addr_t working_buf_mem_isp_daddr;
	int working_buf_mem_size;
	struct mtk_imgsys_hw_subframe working_buf[DIP_SUB_FRM_DATA_NUM];
	/* increase after enqueue */
	atomic_t imgsys_enqueue_cnt;
	/* increase after stream on, decrease when stream off */
	int imgsys_stream_cnt;
	int abnormal_stop;
	/* To serialize request opertion to DIP co-procrosser and hadrware */
	struct mutex hw_op_lock;
	/* To restrict the max number of request be send to SCP */
	struct semaphore sem;
	struct imgsys_user_list imgsys_users;
	uint32_t reg_table_size;
	uint32_t isp_version;
	const struct mtk_imgsys_pipe_desc *cust_pipes;
	const struct module_ops *modules;
	struct mtk_imgsys_dvfs dvfs_info;
	struct mtk_imgsys_qos qos_info;
	struct mutex dvfs_qos_lock;
	struct mutex power_ctrl_lock;
	debug_dump dump;
	atomic_t imgsys_user_cnt;
	struct kref init_kref;
};

/* contained in struct mtk_imgsys_user's done_list */
struct done_frame_pack {
	struct frame_param_pack pack;
	struct list_head done_entry;
};

struct mtk_imgsys_user {
	struct list_head entry;
	u16 id;
	struct v4l2_fh *fh;
	enum imgsys_user_enum user_enum;

	/* used by batch mode only */
	wait_queue_head_t enque_wq;
	struct list_head done_list;  /* contains struct done_frame_pack */
	bool dqdonestate; /*batchmode use to check dqdonestate*/
	wait_queue_head_t done_wq;

	// ToDo: should also sync with standard mode
	enum imgsys_user_state state;
	spinlock_t lock;
};

struct mtk_imgsys_time_state {
	int req_fd;
	u64 time_qbuf;
	u64 time_qreq;
	u64 time_composingStart;
	u64 time_composingEnd;
	u64 time_iovaworkp;
	u64 time_qw2composer;
	u64 time_compfuncStart;
	u64 time_ipisendStart;
	u64 time_reddonescpStart;
	u64 time_doframeinfo;
	u64 time_qw2runner;
	u64 time_runnerStart;
	u64 time_send2cmq;
	u64 time_cmqret;
	u64 time_sendtask;
	u64 time_mdpcbStart;
	u64 time_notifyStart;
	u64 time_unmapiovaEnd;
	u64 time_notify2vb2done;
};

struct scp_work {
	struct work_struct work;
	struct mtk_imgsys_request *req;
	void *req_sbuf_kva;
};

struct gce_timeout_work {
	struct work_struct work;
	struct mtk_imgsys_request *req;
	void *req_sbuf_kva;
	void *pipe;
	uint32_t fail_uinfo_idx;
	int8_t fail_isHWhang;
};

struct gce_cb_work {
	struct work_struct work;
	u32 reqfd;
	void *req_sbuf_kva;
	void *pipe;
};

struct req_frameparam {
	u32		index;
	u32		frame_no;
	u64		timestamp;
	u8		state;
	u8		num_inputs;
	u8		num_outputs;
	struct img_sw_addr	self_data;
} __attribute__ ((__packed__));

struct req_frame {
	struct req_frameparam frameparam;
};

struct mtk_imgsys_request {
	struct media_request req;
	struct mtk_imgsys_pipe *imgsys_pipe;
	struct completion done;
	bool used;
	int id;
	//all devicebuf use the same fd and va
	int buf_fd;
	u64 buf_va_daddr;
	bool buf_same;
	//
	struct mtk_imgsys_dev_buffer
				**buf_map;
	struct list_head list;
	struct req_frame img_fparam;
	struct work_struct fw_work;
	struct work_struct iova_work;
	/* It is used only in timeout handling flow */
	struct work_struct mdpcb_work;
	struct mtk_imgsys_hw_subframe *working_buf;
	atomic_t buf_count;
	atomic_t swfrm_cnt;
	struct mtk_imgsys_time_state tstate;
#ifdef BATCH_MODE_V3
	// V3 added {
	/* Batch mode unprocessed_count */
	int unprocessed_count;
	struct done_frame_pack *done_pack;
	struct mtk_imgsys_hw_working_buf_list working_buf_list;
	struct mtk_imgsys_hw_working_buf_list scp_done_list;
	struct mtk_imgsys_hw_working_buf_list runner_done_list;
	struct mtk_imgsys_hw_working_buf_list mdp_done_list;
	bool is_batch_mode;
	// } V3 added
#endif
};

#ifdef BATCH_MODE_V3
// V3 added {
struct mtk_mdpcb_work {
	struct work_struct		frame_work;
	struct mtk_imgsys_request		*req;
};

struct mtk_runner_work {
	struct work_struct		frame_work;
	struct mtk_imgsys_request		*req;
};
// } V3 added
#endif

int mtk_imgsys_dev_media_register(struct device *dev,
			       struct media_device *media_dev);

void mtk_imgsys_dev_v4l2_release(struct mtk_imgsys_dev *imgsys_dev);

int mtk_imgsys_pipe_v4l2_register(struct mtk_imgsys_pipe *pipe,
			       struct media_device *media_dev,
			       struct v4l2_device *v4l2_dev);

void mtk_imgsys_pipe_v4l2_unregister(struct mtk_imgsys_pipe *pipe);

int mtk_imgsys_pipe_init(struct mtk_imgsys_dev *imgsys_dev,
			struct mtk_imgsys_pipe *pipe,
			const struct mtk_imgsys_pipe_desc *setting);

int mtk_imgsys_pipe_release(struct mtk_imgsys_pipe *pipe);

struct mtk_imgsys_request *
mtk_imgsys_pipe_get_running_job(struct mtk_imgsys_pipe *pipe,
			     int id);

void mtk_imgsys_pipe_remove_job(struct mtk_imgsys_request *req);

int mtk_imgsys_pipe_next_job_id(struct mtk_imgsys_pipe *pipe);

#ifdef BATCH_MODE_V3
int mtk_imgsys_pipe_next_job_id_batch_mode(struct mtk_imgsys_pipe *pipe,
		unsigned short user_id);
#endif
struct fd_kva_list_t *get_fd_kva_list(void);
struct buf_va_info_t *get_first_sd_buf(void);

void mtk_imgsys_pipe_debug_job(struct mtk_imgsys_pipe *pipe,
			    struct mtk_imgsys_request *req);

void mtk_imgsys_pipe_job_finish(struct mtk_imgsys_request *req,
			     enum vb2_buffer_state vbf_state);

const struct mtk_imgsys_dev_format *
mtk_imgsys_pipe_find_fmt(struct mtk_imgsys_pipe *pipe,
		      struct mtk_imgsys_video_device *node,
		      u32 format);

void mtk_imgsys_pipe_try_fmt(struct mtk_imgsys_pipe *pipe,
			  struct mtk_imgsys_video_device *node,
			  struct v4l2_format *fmt,
			  const struct v4l2_format *ufmt,
			  const struct mtk_imgsys_dev_format *dfmt);

void mtk_imgsys_pipe_load_default_fmt(struct mtk_imgsys_pipe *pipe,
				   struct mtk_imgsys_video_device *node,
				   struct v4l2_format *fmt_to_fill);

bool is_desc_mode(struct mtk_imgsys_request *req);

int is_singledev_mode(struct mtk_imgsys_request *req);

bool is_desc_fmt(const struct mtk_imgsys_dev_format *dev_fmt);

void mtk_imgsys_desc_ipi_params_config(struct mtk_imgsys_request *req);

void mtk_imgsys_std_ipi_params_config(struct mtk_imgsys_request *req);

void mtk_imgsys_singledevice_ipi_params_config(struct mtk_imgsys_request *req);

void mtk_imgsys_pipe_try_enqueue(struct mtk_imgsys_pipe *pipe);

void mtk_imgsys_hw_enqueue(struct mtk_imgsys_dev *imgsys_dev,
			struct mtk_imgsys_request *req);

int mtk_imgsys_hw_streamoff(struct mtk_imgsys_pipe *pipe);

int mtk_imgsys_hw_streamon(struct mtk_imgsys_pipe *pipe);

static inline struct mtk_imgsys_pipe*
mtk_imgsys_dev_get_pipe(struct mtk_imgsys_dev *imgsys_dev, unsigned int pipe_id)
{
	if (pipe_id >= MTK_IMGSYS_PIPE_ID_TOTAL_NUM)
		return NULL;

	return &imgsys_dev->imgsys_pipe[pipe_id];
}

static inline struct mtk_imgsys_video_device*
mtk_imgsys_file_to_node(struct file *file)
{
	return container_of(video_devdata(file),
			    struct mtk_imgsys_video_device, vdev);
}

static inline struct mtk_imgsys_pipe*
mtk_imgsys_subdev_to_pipe(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mtk_imgsys_pipe, subdev);
}

static inline struct mtk_imgsys_dev*
mtk_imgsys_mdev_to_dev(struct media_device *mdev)
{
	return container_of(mdev, struct mtk_imgsys_dev, mdev);
}

static inline struct mtk_imgsys_video_device*
mtk_imgsys_vbq_to_node(struct vb2_queue *vq)
{
	return container_of(vq, struct mtk_imgsys_video_device, dev_q.vbq);
}

static inline struct mtk_imgsys_dev_buffer*
mtk_imgsys_vb2_buf_to_dev_buf(struct vb2_buffer *vb)
{
	return container_of(vb, struct mtk_imgsys_dev_buffer, vbb.vb2_buf);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_media_req_to_imgsys_req(struct media_request *req)
{
	return container_of(req, struct mtk_imgsys_request, req);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_hw_fw_work_to_req(struct work_struct *fw_work)
{
	return container_of(fw_work, struct mtk_imgsys_request, fw_work);
}

static inline struct mtk_imgsys_request*
mtk_imgsys_hw_mdp_work_to_req(struct work_struct *mdp_work)
{
	struct scp_work *swork  = container_of(mdp_work, struct scp_work, work);

	return swork->req;
}

static inline struct mtk_imgsys_request *
mtk_imgsys_hw_mdpcb_work_to_req(struct work_struct *mdpcb_work)
{
	struct scp_work *swork  = container_of(mdpcb_work, struct scp_work, work);

	return swork->req;
}

static inline struct mtk_imgsys_request *
mtk_imgsys_hw_timeout_work_to_req(struct work_struct *gcetimeout_work)
{
	struct gce_timeout_work *gwork  = container_of(gcetimeout_work,
		struct gce_timeout_work, work);

	return gwork->req;
}

#ifdef GCE_DONE_USE_REQ
static inline struct mtk_imgsys_request *
mtk_imgsys_hw_gce_done_work_to_req(struct work_struct *gcecb_work)
{
	struct gce_cb_work *gwork  = container_of(gcecb_work, struct gce_cb_work, work);

	return gwork->req;
}
#endif
static inline int mtk_imgsys_buf_is_meta(u32 type)
{
	return type == V4L2_BUF_TYPE_META_CAPTURE ||
		type == V4L2_BUF_TYPE_META_OUTPUT;
}

static inline int mtk_imgsys_pipe_get_pipe_from_job_id(int id)
{
	return (id >> 16) & 0x0000FFFF;
}

static inline void
mtk_imgsys_wbuf_to_ipi_img_sw_addr(struct img_sw_addr *ipi_addr,
				struct mtk_imgsys_hw_working_buf *wbuf)
{
	ipi_addr->va = (u64)wbuf->vaddr;
	ipi_addr->pa = (u32)wbuf->scp_daddr;
}

int mtk_imgsys_hw_working_buf_pool_init(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_hw_working_buf_pool_release(struct mtk_imgsys_dev *imgsys_dev);
struct mtk_imgsys_hw_subframe*
imgsys_working_buf_alloc_helper(struct mtk_imgsys_dev *imgsys_dev);

int mtk_imgsys_can_enqueue(struct mtk_imgsys_dev *imgsys_dev,
			int unprocessedcnt);
void mtk_imgsys_desc_map_iova(struct mtk_imgsys_request *req);
void mtk_imgsys_sd_desc_map_iova(struct mtk_imgsys_request *req);

#ifdef BATCH_MODE_V3
bool is_batch_mode(struct mtk_imgsys_request *req);
#endif

void mtk_imgsys_put_dma_buf(struct dma_buf *dma_buf,
				struct dma_buf_attachment *attach,
				struct sg_table *sgt);
void mtk_imgsys_mod_get(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_mod_put(struct mtk_imgsys_dev *imgsys_dev);

/***************************************************************************/
void flush_fd_kva_list(struct mtk_imgsys_dev *imgsys_dev);
/*
 * macro define for list usage
 */
#define DECLARE_VLIST(type) \
struct type ## _list { \
	struct type node; \
	struct list_head link; \
}

/*
 * vlist_node_of - get the pointer to the node which has specific vlist
 * @ptr:    the pointer to struct list_head
 * @type:   the type of list node
 */
#define vlist_node_of(ptr, type) ({ \
	const struct list_head *mptr = (ptr); \
	(type *)((char *)mptr - offsetof(type ## _list, link)); })

/*
 * vlist_link - get the pointer to struct list_head
 * @ptr:    the pointer to struct vlist
 * @type:   the type of list node
 */
#define vlist_link(ptr, type) \
	((struct list_head *)((char *)ptr + offsetof(type ## _list, link)))

/*
 * vlist_type - get the type of struct vlist
 * @type:   the type of list node
 */
#define vlist_type(type) type ## _list

/*
 * vlist_node - get the pointer to the node of vlist
 * @ptr:    the pointer to struct vlist
 * @type:   the type of list node
 */
#define vlist_node(ptr, type) ((type *) ptr)

struct timeval {
	__kernel_old_time_t	tv_sec;		/* seconds */
	__kernel_suseconds_t	tv_usec;	/* microseconds */
};

struct swfrm_info_t {
	uint32_t req_sbuf_goft;
	int swfrminfo_ridx;
	int request_fd;
	int request_no;
	int frame_no;
	uint64_t frm_owner;
	uint8_t is_secReq;
	int fps;
	int cb_frmcnt;
	int total_taskcnt;
	int exp_totalcb_cnt;
	int handle;
	uint64_t req_vaddr;
	int sync_id;
	int total_frmnum;
	struct img_swfrm_info user_info[TIME_MAX];
	uint8_t is_earlycb;
	int earlycb_sidx;
	uint8_t is_lastfrm;
	int8_t group_id;
	int8_t batchnum;
	int8_t is_sent;	/*check the frame is sent to gce or not*/
	void *req;		/*mtk_dip_request*/
	void *pipe;
	uint32_t fail_uinfo_idx;
	int8_t fail_isHWhang;
	struct timeval eqtime;
	int chan_id;
	char *hw_ts_log;
};
#define HWTOKEN_MAX 100
struct cleartoken_info_t {
	int clearnum;
	int token[HWTOKEN_MAX];
};

#define REQ_FD_MAX 65536
struct reqfd_cbinfo_t {
	int req_fd;
	int req_no;
	int frm_no;
	uint64_t frm_owner;
	int exp_cnt;
	int cur_cnt;
};
DECLARE_VLIST(reqfd_cbinfo_t);
struct reqfd_cbinfo_list_t {
	struct mutex mymutex;
	struct list_head mylist;
};

struct info_list_t {
	struct mutex mymutex;
	/* pthread_cond_t cond; */
	struct list_head configed_list;
	struct list_head fail_list;
};

DECLARE_VLIST(swfrm_info_t);

struct buf_va_info_t {
	int buf_fd;
	unsigned long kva;
	void *dma_buf_putkva;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
};

struct fd_kva_list_t {
	struct mutex mymutex;
	struct list_head mylist;
};
DECLARE_VLIST(buf_va_info_t);
#endif /* _MTK_DIP_DEV_H_ */
