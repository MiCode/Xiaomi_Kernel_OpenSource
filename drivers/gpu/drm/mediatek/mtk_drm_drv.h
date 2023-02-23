/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef MTK_DRM_DRV_H
#define MTK_DRM_DRV_H

#include <drm/drm_fb_helper.h>
#include <drm/mediatek_drm.h>
#include <linux/types.h>
#include <linux/io.h>
#include <drm/drm_atomic.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_session.h"
#include "mtk_drm_helper.h"

#define MAX_CONNECTOR 3

#ifndef CONFIG_FPGA_EARLY_PORTING
#define MTK_DRM_ESD_SUPPORT
#define MTK_FB_MMDVFS_SUPPORT
#endif

#define MTK_DRM_FENCE_SUPPORT
#define MTK_DRM_CMDQ_ASYNC
#define CONFIG_MTK_DISPLAY_CMDQ
#define MTK_FILL_MIPI_IMPEDANCE

#if (defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6873)\
	|| defined(CONFIG_MACH_MT6893) ||\
	defined(CONFIG_MACH_MT6853) || \
	defined(CONFIG_MACH_MT6833) || \
	defined(CONFIG_MACH_MT6781)) &&\
	defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
#define MTK_DRM_DELAY_PRESENT_FENCE
/* Delay present fence would cause config merge */
#endif

#if defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
/*
 * MTK_DRM_DELAY_PRESENT_FENCE can not be defined,
 * but SF present fence must be enabled in platform dts
 */
#define MTK_DRM_DELAY_PRESENT_FENCE_SOF
#endif

#if defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6853)\
	|| defined(CONFIG_MACH_MT6877)
#define CONFIG_MTK_DYN_SWITCH_BY_CMD
#endif

#ifdef CONFIG_MTK_IOMMU_V2
#define CONFIG_MTK_DISPLAY_M4U
#endif

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct drm_property;
struct regmap;
struct mm_qos_request;
struct mtk_pm_qos_request;

struct mtk_atomic_state {
	struct drm_atomic_state base;
	struct list_head list;
	struct kref kref;
};

struct mtk_fake_eng_reg {
	unsigned int CG_idx;
	unsigned int CG_bit;
	bool share_port;
};

struct mtk_fake_eng_data {
	int fake_eng_num;
	const struct mtk_fake_eng_reg *fake_eng_reg;
};

struct mtk_mmsys_driver_data {
	const struct mtk_crtc_path_data *main_path_data;
	const struct mtk_crtc_path_data *ext_path_data;
	const struct mtk_crtc_path_data *third_path_data;
	enum mtk_mmsys_id mmsys_id;
	bool shadow_register;
	const struct mtk_session_mode_tb *mode_tb;
	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			struct cmdq_pkt *handle, void *data);
	const struct mtk_fake_eng_data *fake_eng_data;
	bool bypass_infra_ddr_control;
};

struct mtk_drm_lyeblob_ids {
	uint32_t lye_idx;
	uint32_t frame_weight;
	uint32_t hrt_num;
	int32_t ddp_blob_id;
	int32_t ref_cnt;
	int32_t ref_cnt_mask;
	int32_t free_cnt_mask;
	int32_t lye_plane_blob_id[MAX_CRTC][OVL_LAYER_NR];
	struct list_head list;
};

struct mtk_drm_private {
	struct drm_device *drm;
	struct device *dma_dev;
	struct device *mmsys_dev;
#if defined(CONFIG_MTK_IOMMU_V2)
	struct ion_client *client;
#endif

	struct drm_crtc *crtc[MAX_CRTC];
	unsigned int num_pipes;

	unsigned int session_id[MAX_SESSION_COUNT];
	unsigned int num_sessions;
	enum MTK_DRM_SESSION_MODE session_mode;
	atomic_t crtc_present[MAX_CRTC];
	atomic_t crtc_sf_present[MAX_CRTC];

	struct device_node *mutex_node;
	struct device *mutex_dev;
	void __iomem *config_regs;
	resource_size_t config_regs_pa;
	void __iomem *infra_regs;
	resource_size_t infra_regs_pa;
	const struct mtk_mmsys_reg_data *reg_data;
	struct device_node *comp_node[DDP_COMPONENT_ID_MAX];
	struct mtk_ddp_comp *ddp_comp[DDP_COMPONENT_ID_MAX];
	const struct mtk_mmsys_driver_data *data;

	struct {
		struct drm_atomic_state *state;
		struct work_struct work;
		struct mutex lock;
	} commit;

	struct drm_atomic_state *suspend_state;

	struct {
		struct work_struct work;
		struct list_head list;
		spinlock_t lock;
	} unreference;

	/* property */
	struct drm_property *crtc_property[MAX_CRTC][CRTC_PROP_MAX];

	struct drm_fb_helper fb_helper;
	struct kref kref_fb_buf;
	struct drm_gem_object *fbdev_bo;
	struct list_head lyeblob_head;
	struct mutex lyeblob_list_mutex;
	struct task_struct *fence_release_thread[MAX_CRTC-1];

	/* variable for repaint */
	struct {
		wait_queue_head_t wq;
		struct list_head job_queue;
		struct list_head job_pool;
	} repaint_data;

	struct mtk_drm_helper *helper_opt;

	atomic_t idle_need_repaint;
	atomic_t rollback_all;

	unsigned int top_clk_num;
	struct clk **top_clk;
	bool power_state;

	/* for rpo caps info */
	unsigned int rsz_in_max[2];

#ifdef MTK_FB_MMDVFS_SUPPORT
	struct plist_head bw_request_list;
	struct plist_head hrt_request_list;
	struct mtk_pm_qos_request ddr_opp_request;
	struct mtk_pm_qos_request mm_freq_request;
	struct mm_qos_request hrt_bw_request;
#endif
	struct pinctrl *pctrl;

#ifdef DRM_MMPATH
	int HWC_gpid; // for mmpath auto gen
#endif

	int need_vds_path_switch;
	int vds_path_switch_dirty;
	int vds_path_switch_done;
	int need_vds_path_switch_back;
	int vds_path_enable;

	bool need_cwb_path_disconnect;
	bool cwb_is_preempted;

	/* Due to 2nd display share 1 secure gce client, need store here */
	struct cmdq_client *ext_sec_client;
};

struct mtk_drm_property {
	int flags;
	char *name;
	unsigned long min;
	unsigned long max;
	unsigned long val;
};

struct repaint_job_t {
	struct list_head link;
	unsigned int type;
};

#define LCM_FPS_ARRAY_SIZE  32
struct lcm_fps_ctx_t {
	atomic_t is_inited;
	spinlock_t lock;
	atomic_t skip_update;
	unsigned int dsi_mode;
	unsigned int head_idx;
	unsigned int num;
	unsigned int fps;
	unsigned long long last_ns;
	unsigned long long array[LCM_FPS_ARRAY_SIZE];
};

#define DISP_LARB_COUNT 1
struct disp_iommu_device {
	struct platform_device *larb_pdev[DISP_LARB_COUNT];
	struct platform_device *iommu_pdev;
	unsigned int inited;
};

struct disp_iommu_device *disp_get_iommu_dev(void);

extern struct platform_driver mtk_ddp_driver;
extern struct platform_driver mtk_disp_color_driver;
extern struct platform_driver mtk_disp_ccorr_driver;
extern struct platform_driver mtk_disp_gamma_driver;
extern struct platform_driver mtk_disp_aal_driver;
extern struct platform_driver mtk_dmdp_aal_driver;
extern struct platform_driver mtk_disp_postmask_driver;
extern struct platform_driver mtk_disp_dither_driver;
extern struct platform_driver mtk_disp_ovl_driver;
extern struct platform_driver mtk_disp_rdma_driver;
extern struct platform_driver mtk_disp_wdma_driver;
extern struct platform_driver mtk_disp_rsz_driver;
extern struct platform_driver mtk_dpi_driver;
extern struct platform_driver mtk_dsi_driver;
extern struct platform_driver mtk_mipi_tx_driver;
extern struct platform_driver mtk_lvds_driver;
extern struct platform_driver mtk_lvds_tx_driver;
extern struct platform_driver mtk_disp_dsc_driver;
extern struct lcm_fps_ctx_t lcm_fps_ctx[MAX_CRTC];
extern struct platform_driver mtk_disp_merge_driver;
#ifdef CONFIG_MTK_HDMI_SUPPORT
extern struct platform_driver mtk_dp_tx_driver;
extern struct platform_driver mtk_dp_intf_driver;
#endif
extern atomic_t resume_pending;
extern wait_queue_head_t resume_wait_q;

void mtk_atomic_state_put_queue(struct drm_atomic_state *state);
void mtk_drm_fence_update(unsigned int fence_idx, unsigned int index);
void drm_trigger_repaint(enum DRM_REPAINT_TYPE type,
			 struct drm_device *drm_dev);
int mtk_drm_suspend_release_fence(struct device *dev);
void mtk_drm_suspend_release_present_fence(struct device *dev,
					   unsigned int index);
void mtk_drm_suspend_release_sf_present_fence(struct device *dev,
					      unsigned int index);
void mtk_drm_top_clk_prepare_enable(struct drm_device *drm);
void mtk_drm_top_clk_disable_unprepare(struct drm_device *drm);
struct mtk_panel_params *mtk_drm_get_lcm_ext_params(struct drm_crtc *crtc);
struct mtk_panel_funcs *mtk_drm_get_lcm_ext_funcs(struct drm_crtc *crtc);
bool mtk_drm_session_mode_is_decouple_mode(struct drm_device *dev);
bool mtk_drm_session_mode_is_mirror_mode(struct drm_device *dev);
bool mtk_drm_top_clk_isr_get(char *master);
void mtk_drm_top_clk_isr_put(char *master);
int lcm_fps_ctx_init(struct drm_crtc *crtc);
int lcm_fps_ctx_reset(struct drm_crtc *crtc);
int lcm_fps_ctx_update(unsigned long long cur_ns,
		unsigned int crtc_id, unsigned int mode);
int mtk_mipi_clk_change(struct drm_crtc *crtc, unsigned int data_rate);
void disp_drm_debug(const char *opt);
#endif /* MTK_DRM_DRV_H */
