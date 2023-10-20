#ifndef __VCODEC_DVFS_H__
#define __VCODEC_DVFS_H__

#include <linux/types.h>
#include <linux/list.h>
#include "vcodec_ipi_msg.h"

#define DEFAULT_VENC_CONFIG -1000
#define MAX_VCODEC_FREQ 9999
#define MAX_OP_CNT 5

struct mtk_vcodec_dev;
struct mtk_vcodec_ctx;

/* performance point per config */
struct vcodec_perf {
	u8 codec_type;
	u32 codec_fmt;
	s32 config;
	u32 cy_per_mb_1; /* for I/P */
	u32 cy_per_mb_2; /* for I/P/B */
};

/* config selection criteria */
struct vcodec_config {
	u8 codec_type;
	u32 codec_fmt;
	u32 mb_thresh; /* applicable mb threshold */
	s32 config_1; /* low power config */
	s32 config_2; /* hgih quality config */
};

struct vcodec_op_rate {
	u8 codec_type;
	u32 codec_fmt;
	u32 max_op_cnt;
	u32 pixel_per_frame[MAX_OP_CNT];
	u32 max_op_rate[MAX_OP_CNT];
};

/* instance info for dvfs */
struct vcodec_inst {
	int id;
	u8 codec_type;
	u32 codec_fmt;
	u32 core_cnt;
	s32 config;
	u32 b_frame;
	u32 wp;
	s32 op_rate;
	s32 priority;
	u32 width;
	u32 height;
	u64 last_access;
	struct list_head list;
};

/* dvfs policies  */
struct dvfs_params {
	u8 codec_type;
	u8 allow_oc;		/* allow oc freq */
	u8 per_frame_adjust;	/* do per frame adjust dvfs / pmqos */
	u8 per_frame_adjust_op_rate; /* per frame adjust threshold */
	u32 min_freq;		/* min freq */
	u32 normal_max_freq;	/* normal max freq (no oc) */
	u32 freq_sum;		/* summation of all instances */
	u32 target_freq;	/* target freq */
	u8 lock_cnt[MTK_VDEC_HW_NUM]; /* lock cnt */
	u8 frame_need_update;	/* this frame begin / end needs update */
};

struct vcodec_inst *get_inst(struct mtk_vcodec_ctx *ctx);
bool need_update(struct mtk_vcodec_ctx *ctx);
bool remove_update(struct mtk_vcodec_ctx *ctx);
u32 match_avail_freq(struct mtk_vcodec_dev *dev, int codec_type, u64 freq);
void update_freq(struct mtk_vcodec_dev *dev, int codec_type);

#endif
