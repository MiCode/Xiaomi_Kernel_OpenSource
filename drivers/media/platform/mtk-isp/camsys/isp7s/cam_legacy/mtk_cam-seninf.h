/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_H__
#define __MTK_CAM_SENINF_H__

#include <linux/kthread.h>
#include <linux/remoteproc.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>

#include "mtk_cam-seninf-def.h"
#include "imgsensor-user.h"
#include "mtk_cam-seninf-regs.h"
#include "mtk_cam-aov.h"

/* def V4L2_MBUS_CSI2_IS_USER_DEFINED_DATA */
#define SENINF_VC_ROUTING

#define CSI_EFUSE_SET
//#define SENINF_UT_DUMP

#define seninf_logi(_ctx, format, args...) do { \
	if ((_ctx) && (_ctx)->sensor_sd) { \
		dev_info((_ctx)->dev, "[%s][%s] " format, \
			(_ctx)->sensor_sd->name, __func__, ##args); \
	} \
} while (0)

struct seninf_ctx;

/* aov sensor use */
extern struct mtk_seninf_aov_param g_aov_param;
extern struct seninf_ctx *aov_ctx[6];

struct seninf_struct_pair {
	u32 first;
	u32 second;
};

struct seninf_mux {
	struct list_head list;
	int idx;
};

struct seninf_cam_mux {
	struct list_head list;
	int idx;
};

#define DT_REMAP_MAX_CNT 4
struct seninf_vcinfo {
	struct seninf_vc vc[SENINF_VC_MAXCNT];
	int cnt;
};

struct seninf_dfs {
	struct device *dev;
	struct regulator *reg;
	unsigned long *freqs;
	unsigned long *volts;
	int cnt;
};

struct mtk_seninf_work {
	struct kthread_work work;
	struct kthread_delayed_work dwork;
	struct seninf_ctx *ctx;
	int do_sensor_stream_on;
	union work_data_t {
		unsigned int sof;
		void *data_ptr;
	} data;
};

struct seninf_core {
	struct device *dev;
	int pm_domain_cnt;
	struct device **pm_domain_devs;
	struct clk *clk[CLK_MAXCNT];
	struct seninf_dfs dfs;
	struct list_head list;
	struct list_head list_mux;
	struct seninf_struct_pair mux_range[TYPE_MAX_NUM];
	struct seninf_mux mux[SENINF_MUX_NUM];
#ifdef SENINF_DEBUG
	struct list_head list_cam_mux;
	struct seninf_struct_pair cammux_range[TYPE_MAX_NUM];
	struct seninf_cam_mux cam_mux[SENINF_CAM_MUX_NUM];
#endif
	struct mutex mutex;
	void __iomem *reg_if;
	void __iomem *reg_ana;
	int refcnt;

	/* CCU control flow */
	phandle rproc_ccu_phandle;
	struct rproc *rproc_ccu_handle;

	/* platform properties */
	int cphy_settle_delay_dt;
	int dphy_settle_delay_dt;
	int settle_delay_ck;
	int hs_trail_parameter;

	spinlock_t spinlock_irq;

	struct kthread_worker seninf_worker;
	struct task_struct *seninf_kworker_task;

	/* record pid */
	struct pid *pid;
	/* mipi error detection count */
	unsigned int detection_cnt;
	/* enable csi irq flag */
	unsigned int csi_irq_en_flag;
	/* enable vsync irq flag */
	unsigned int vsync_irq_en_flag;

	/* aov sensor use */
	int pwr_refcnt_for_aov;
	int aov_sensor_id;
	int current_sensor_id;

	/* debug flag for vsync */
	u32 *seninf_vsync_debug_flag;

	/* debug flag for aov csi clk */
	enum mtk_cam_seninf_csi_clk_for_param aov_csi_clk_switch_flag;
	/* abnormal deinit flag */
	u32 aov_abnormal_deinit_flag;
	u32 aov_abnormal_deinit_usr_fd_kill_flag;
	/* abnormal init flag */
	u32 aov_abnormal_init_flag;
};

struct seninf_ctx {
	struct v4l2_subdev subdev;
	struct v4l2_async_notifier notifier;
	struct device *dev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad pads[PAD_MAXCNT];
	struct v4l2_subdev_format fmt[PAD_MAXCNT];
	struct seninf_core *core;
	struct list_head list;

	int port;
	int portNum;
	int portA;
	int portB;
	int num_data_lanes;
	s64 mipi_pixel_rate;
	s64 buffered_pixel_rate;
	s64 customized_pixel_rate;

	unsigned int is_4d1c:1;
	unsigned int is_cphy:1;
	unsigned int is_test_model:4;
	unsigned int is_aov_test_model;
	unsigned int is_aov_real_sensor;
#ifdef SENINF_DEBUG
	unsigned int is_test_streamon:1;
#endif
#ifdef CSI_EFUSE_SET
	unsigned int m_csi_efuse;
#endif
	unsigned int is_secure:1;
	unsigned int SecInfo_addr;
	int seninfIdx;
	int pad2cam[PAD_MAXCNT][MAX_DEST_NUM];
	int pad_tag_id[PAD_MAXCNT][MAX_DEST_NUM];

	/* remote sensor */
	struct v4l2_subdev *sensor_sd;
	int sensor_pad_idx;

	/* provided by sensor */
	struct seninf_vcinfo vcinfo;
	u16 vc_group[VC_CH_GROUP_MAX_NUM];
	int fsync_vsync_src_pad; // e.g., raw, 3A-meta(general-embedded)
	int fps_n;
	int fps_d;

	/* dfs */
	int isp_freq;

	void __iomem *reg_ana_csi_rx[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_dphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_ana_cphy_top[CSI_PORT_MAX_NUM];
	void __iomem *reg_if_top;
	void __iomem *reg_if_ctrl[SENINF_NUM];
	void __iomem *reg_if_cam_mux;
	void __iomem *reg_if_cam_mux_gcsr;
	void __iomem *reg_if_cam_mux_pcsr[SENINF_CAM_MUX_NUM];
	void __iomem *reg_if_tg[SENINF_NUM];
	void __iomem *reg_if_csi2[SENINF_NUM];
	void __iomem *reg_if_mux[SENINF_MUX_NUM];

	/* resources */
	struct list_head list_mux;
	struct list_head list_cam_mux;
	struct seninf_mux *mux_by[VC_CH_GROUP_MAX_NUM][TYPE_MAX_NUM];

	/* flags */
	unsigned int csi_streaming:1;
	unsigned int streaming:1;

	int seninf_dphy_settle_delay_dt;
	int cphy_settle_delay_dt;
	int dphy_settle_delay_dt;
	int settle_delay_ck;
	int hs_trail_parameter;
	/*sensor mode customized csi params*/
	struct mtk_csi_param csi_param;

	int open_refcnt;
	struct mutex mutex;

	/* csi irq */
	unsigned int data_not_enough_cnt;
	unsigned int err_lane_resync_cnt;
	unsigned int crc_err_cnt;
	unsigned int ecc_err_double_cnt;
	unsigned int ecc_err_corrected_cnt;
	/* seninf_mux fifo overrun irq */
	unsigned int fifo_overrun_cnt;
	/* cam_mux h/v size irq */
	unsigned int size_err_cnt;
	/* error flag */
	unsigned int data_not_enough_flag;
	unsigned int err_lane_resync_flag;
	unsigned int crc_err_flag;
	unsigned int ecc_err_double_flag;
	unsigned int ecc_err_corrected_flag;
	unsigned int fifo_overrun_flag;
	unsigned int size_err_flag;
	unsigned int dbg_timeout;
	unsigned int dbg_last_dump_req;

	/* cammux switch debug element */
	struct mtk_cam_seninf_mux_param *dbg_chmux_param;
};

#endif
