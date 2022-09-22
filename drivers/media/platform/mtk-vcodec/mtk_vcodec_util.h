/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*/

#ifndef _MTK_VCODEC_UTIL_H_
#define _MTK_VCODEC_UTIL_H_

#include <aee.h>
#include <linux/types.h>
#include <linux/dma-direction.h>
#include <linux/mtk_vcu_controls.h>
#include "vcodec_ipi_msg.h"
// include vcp header to make build pass even if CONFIG_MTK_TINYSYS_VCP_SUPPORT is off
// dynamic to switch vcp or vcu path by "mtk_vcodec_vcp"
//#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_helper.h"
//#endif
#if IS_ENABLED(CONFIG_VIDEO_MEDIATEK_VCU)
#include "mtk_vcu.h"
#endif
#include <linux/trace_events.h>

/* #define FPGA_PWRCLK_API_DISABLE */
/* #define FPGA_INTERRUPT_API_DISABLE */

#define mem_slot_range 100*1024ULL //100KB

#define CODEC_ALLOCATE_MAX_BUFFER_SIZE 0x10000000UL /*256MB, sync with mtk_vcodec_mem.h*/

#define LOG_PARAM_INFO_SIZE 64
#define LOG_PROPERTY_SIZE 1024
#define ROUND_N(X, N)   (((X) + ((N)-1)) & (~((N)-1)))    //only for N is exponential of 2

struct mtk_vcodec_mem {
	size_t length;
	size_t size;
	size_t data_offset;
	void *va;
	dma_addr_t dma_addr;
	struct dma_buf *dmabuf;
	__u32 flags;
	__u32 index;
	__s64 buf_fd;
};

/**
 * struct vdec_fb_status  - decoder frame buffer status
 * @FB_ST_INIT        : initial state
 * @FB_ST_DISPLAY       : frmae buffer is ready to be displayed
 * @FB_ST_FREE          : frame buffer is not used by decoder any more
 */
enum vdec_fb_status {
	FB_ST_INIT              = 0,
	FB_ST_DISPLAY           = (1 << 0),
	FB_ST_FREE              = (1 << 1),
	FB_ST_EOS               = (1 << 2),
	FB_ST_NO_GENERATED      = (1 << 3)
};

/**
 * enum flags  - decoder different operation types
 * @NO_CAHCE_FLUSH	: no need to proceed cache flush
 * @NO_CAHCE_INVALIDATE	: no need to proceed cache invalidate
 * @CROP_CHANGED	: frame buffer crop changed
 * @REF_FREED	: frame buffer is reference freed
 */
enum mtk_vcodec_flags {
	NO_CAHCE_CLEAN = 1,
	NO_CAHCE_INVALIDATE = 1 << 1,
	CROP_CHANGED = 1 << 2,
	REF_FREED = 1 << 3
};

struct mtk_vcodec_msgq {
	struct list_head head;
	wait_queue_head_t wq;
	spinlock_t lock;
	atomic_t cnt;
};

struct mtk_vcodec_msg_node {
	struct share_obj ipi_data;
	struct list_head list;
};

struct mtk_vcodec_log_param {
	char param_key[LOG_PARAM_INFO_SIZE];
	char param_val[LOG_PARAM_INFO_SIZE];
	struct list_head list;
};

enum mtk_vcodec_log_index {
	MTK_VCODEC_LOG_INDEX_LOG = 1,
	MTK_VCODEC_LOG_INDEX_PROP = 1 << 1
};

struct mtk_vcodec_ctx;
struct mtk_vcodec_dev;

extern int mtk_v4l2_dbg_level;
extern bool mtk_vcodec_dbg;
extern bool mtk_vcodec_perf;
extern int mtk_vcodec_vcp;
extern char *mtk_vdec_property;
extern char *mtk_venc_property;
extern char mtk_vdec_property_prev[LOG_PROPERTY_SIZE];
extern char mtk_venc_property_prev[LOG_PROPERTY_SIZE];
extern char *mtk_vdec_vcp_log;
extern char mtk_vdec_vcp_log_prev[LOG_PROPERTY_SIZE];
extern char *mtk_venc_vcp_log;
extern char mtk_venc_vcp_log_prev[LOG_PROPERTY_SIZE];
extern int mtk_vdec_sw_mem_sec;

#define DEBUG   1
#define VCU_FPTR(x) (vcu_func.x)

#if defined(DEBUG)

#define mtk_v4l2_debug(level, fmt, args...)                              \
	do {                                                             \
		if (((mtk_v4l2_dbg_level) & (level)) == (level))           \
			pr_notice("[MTK_V4L2] level=%d %s(),%d: " fmt "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

#define mtk_v4l2_err(fmt, args...)                \
	pr_notice("[MTK_V4L2][ERROR] %s:%d: " fmt "\n", __func__, __LINE__, \
		   ##args)


#define mtk_v4l2_debug_enter()  mtk_v4l2_debug(8, "+")
#define mtk_v4l2_debug_leave()  mtk_v4l2_debug(8, "-")

#define mtk_vcodec_debug(h, fmt, args...)                               \
	do {                                                            \
		if (mtk_vcodec_dbg)                                  \
			pr_notice("[MTK_VCODEC][%d]: %s() " fmt "\n",     \
				((struct mtk_vcodec_ctx *)h->ctx)->id,  \
				__func__, ##args);                      \
	} while (0)

#define mtk_vcodec_perf_log(fmt, args...)                               \
	do {                                                            \
		if (mtk_vcodec_perf)                          \
			pr_info("[MTK_PERF] " fmt "\n", ##args);        \
	} while (0)


#define mtk_vcodec_err(h, fmt, args...)                                 \
	pr_notice("[MTK_VCODEC][ERROR][%d]: %s() " fmt "\n",               \
		   ((struct mtk_vcodec_ctx *)h->ctx)->id, __func__, ##args)

#define mtk_vcodec_debug_enter(h)  mtk_vcodec_debug(h, "+")
#define mtk_vcodec_debug_leave(h)  mtk_vcodec_debug(h, "-")

#else

#define mtk_v4l2_debug(level, fmt, args...)
#define mtk_v4l2_err(fmt, args...)
#define mtk_v4l2_debug_enter()
#define mtk_v4l2_debug_leave()

#define mtk_vcodec_debug(h, fmt, args...)
#define mtk_vcodec_err(h, fmt, args...)
#define mtk_vcodec_debug_enter(h)
#define mtk_vcodec_debug_leave(h)

#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define v4l2_aee_print(string, args...) do {\
	char vcu_name[100];\
	int ret;\
	ret = snprintf(vcu_name, 100, "[MTK_V4L2] "string, ##args); \
	if (ret > 0)\
		pr_notice("[MTK_V4L2] error:"string, ##args);  \
		aee_kernel_warning_api(__FILE__, __LINE__, \
			DB_OPT_MMPROFILE_BUFFER | DB_OPT_NE_JBT_TRACES, \
			vcu_name, "[MTK_V4L2] error:"string, ##args); \
	} while (0)
#else
#define v4l2_aee_print(string, args...) \
		pr_notice("[MTK_V4L2] error:"string, ##args)

#endif

static __attribute__((used)) unsigned int time_ms_s[2][2], time_ms_e[2][2];
#define time_check_start(is_enc, id) {\
		if (is_enc >= 0 && id >= 0) \
			time_ms_s[is_enc][id] = jiffies_to_msecs(jiffies); \
	}
#define time_check_end(is_enc, id, timeout_ms) do { \
		if (is_enc < 0 || id < 0) \
			break; \
		time_ms_e[is_enc][id]  = jiffies_to_msecs(jiffies); \
		if ((time_ms_e[is_enc][id] - time_ms_s[is_enc][id]) \
			> timeout_ms || \
			mtk_vcodec_perf) \
			pr_info("[V4L2][Info] %s L:%d take %u timeout %u ms", \
				__func__, __LINE__, \
				time_ms_e[is_enc][id] - time_ms_s[is_enc][id], \
				timeout_ms); \
	} while (0)

#if 1
unsigned long vcodec_get_tracing_mark(void);
#define vcodec_trace_begin(fmt, args...)
#define vcodec_trace_end()
#define vcodec_trace_count(name, count)
#else

#define vcodec_trace_begin(fmt, args...) do { \
			preempt_disable(); \
			event_trace_printk(vcodec_get_tracing_mark(), \
				"B|%d|"fmt"\n", current->tgid, ##args); \
			preempt_enable();\
		} while (0)

#define vcodec_trace_end() do { \
			preempt_disable(); \
			event_trace_printk(vcodec_get_tracing_mark(), "E\n"); \
			preempt_enable(); \
		} while (0)

#define vcodec_trace_count(name, count) do { \
			preempt_disable(); \
			event_trace_printk(vcodec_get_tracing_mark(), \
				"C|%d|%s|%d\n", current->tgid, name, count); \
			preempt_enable();\
		} while (0)
#endif

enum mtk_put_buffer_type {
	PUT_BUFFER_WORKER = -1,
	PUT_BUFFER_CALLBACK = 0,
};

void __iomem *mtk_vcodec_get_dec_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx);
void __iomem *mtk_vcodec_get_enc_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx);
int mtk_vcodec_mem_alloc(struct mtk_vcodec_ctx *data,
	struct mtk_vcodec_mem *mem);
void mtk_vcodec_mem_free(struct mtk_vcodec_ctx *data,
	struct mtk_vcodec_mem *mem);
void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx, unsigned int hw_id);
struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *dev,
	unsigned int hw_id);
void mtk_vcodec_add_ctx_list(struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_del_ctx_list(struct mtk_vcodec_ctx *ctx);
struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx);
int mtk_vdec_put_fb(struct mtk_vcodec_ctx *ctx, enum mtk_put_buffer_type type, bool no_need_put);
void mtk_enc_put_buf(struct mtk_vcodec_ctx *ctx);
int v4l2_m2m_buf_queue_check(struct v4l2_m2m_ctx *m2m_ctx,
		void *vbuf);
int mtk_dma_sync_sg_range(const struct sg_table *sgt,
	struct device *dev, unsigned int size,
	enum dma_data_direction direction);
void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
int mtk_vcodec_alloc_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment **attach, struct sg_table **sgt);
int mtk_vcodec_free_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt);
#endif

void mtk_vcodec_set_log(struct mtk_vcodec_dev *dev, const char *val,
	enum mtk_vcodec_log_index log_index);

long long div_64(long long a, long long b);

#endif /* _MTK_VCODEC_UTIL_H_ */
