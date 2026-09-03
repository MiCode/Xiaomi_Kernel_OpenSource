// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef __SPRD_DVFS_APSYS_H__
#define __SPRD_DVFS_APSYS_H__

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>

#include <drm/drm_crtc.h>

typedef enum {
	DVFS_WORK = 0,
	DVFS_IDLE,
} set_freq_type;

struct apsys_dvfs_coffe {
	u32 sw_dvfs_en;
	u32 dvfs_hold_en;
	u32 dvfs_wait_window;
	u32 dvfs_min_volt;
	u32 dvfs_force_en;
	u32 dvfs_auto_gate;
	u32 sw_cgb_enable;
};

struct ip_dvfs_coffe {
	u32 gfree_wait_delay;
	u32 freq_upd_hdsk_en;
	u32 freq_upd_delay_en;
	u32 freq_upd_en_byp;
	u32 sw_trig_en;
	u32 hw_dfs_en;
	u32 work_index_def;
	u32 idle_index_def;
};

struct ip_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
	char *volt_val;
};

struct ip_dvfs_status {
	char *apsys_cur_volt;
	char *dpu_vote_volt;
	char *gsp0_vote_volt;
	char *gsp1_vote_volt;
	char *vsp_vote_volt;
	char *vpuenc_vote_volt;
	char *vdsp_vote_volt;
	char *dpu_cur_freq;
	char *gsp0_cur_freq;
	char *gsp1_cur_freq;
	char *vsp_cur_freq;
	char *vpuenc_cur_freq;
	char *vdsp_cur_freq;
	u32 vdsp_edap_div;
	u32 vdsp_m0_div;
};

struct apsys_dev {
	struct device dev;
	unsigned long base;

	struct apsys_dvfs_coffe dvfs_coffe;
	const struct apsys_dvfs_ops *dvfs_ops;

	unsigned long apsys_base;
	unsigned long top_base;
	struct regmap *aon_base;
	struct mutex reg_lock;
};

struct apsys_dvfs_ops {
	/* apsys common ops */
	int (*parse_dt)(struct apsys_dev *apsys, struct device_node *np);
	void (*dvfs_init)(struct apsys_dev *apsys);
	void (*apsys_hold_en)(struct apsys_dev *apsys, u32 hold_en);
	void (*apsys_force_en)(struct apsys_dev *apsys, u32 force_en);
	void (*apsys_auto_gate)(struct apsys_dev *apsys, u32 gate_sel);
	void (*apsys_wait_window)(struct apsys_dev *apsys, u32 wait_window);
	void (*apsys_min_volt)(struct apsys_dev *apsys, u32 min_volt);

	/* top common ops */
	void (*top_dvfs_init)(struct apsys_dev *apsys);
	int (*top_cur_volt)(struct apsys_dev *apsys);
};

struct sprd_apsys_dvfs_ops {
	const struct apsys_dvfs_ops *apsys_ops;
	const char *version;
};

struct sprd_dpu_crtc {
	struct device dev;
	struct mutex dpu_gsp_lock;
	struct drm_crtc *crtc;
};

struct apsys_dev *find_apsys_device_by_name(char *name);
int n6pro_soc_ver_id_check(void);
bool get_display_power_status(void);

#ifdef CONFIG_DRM_SPRD_GSP_DVFS
extern struct platform_driver gsp_dvfs_driver;
#endif
extern struct platform_driver dpu_dvfs_driver;
extern struct platform_driver vsp_dvfs_driver;
#ifdef CONFIG_DVFS_APSYS_VDSP
extern struct platform_driver vdsp_dvfs_driver;
#endif

#ifdef CONFIG_DRM_SPRD_GSP_DVFS
extern struct devfreq_governor gsp_devfreq_gov;
#endif
extern struct devfreq_governor vsp_devfreq_gov;
extern struct devfreq_governor dpu_devfreq_gov;
#ifdef CONFIG_DVFS_APSYS_VDSP
extern struct devfreq_governor vdsp_devfreq_gov;
#endif

extern struct regmap *regmap_aon_base;

extern const struct apsys_dvfs_ops sharkl5pro_apsys_dvfs_ops;
extern const struct apsys_dvfs_ops sharkl5_apsys_dvfs_ops;
extern const struct apsys_dvfs_ops qogirl6_apsys_dvfs_ops;
extern const struct apsys_dvfs_ops qogirn6pro_apsys_dvfs_ops;
extern const struct apsys_dvfs_ops qogirn6lite_apsys_dvfs_ops;
extern const struct apsys_dvfs_ops roc1_apsys_dvfs_ops;

extern int dpu_vsp_dvfs_check_clkeb(void);
#endif /* __SPRD_DVFS_APSYS_H__ */
