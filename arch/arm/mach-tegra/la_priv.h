/*
 * arch/arm/mach-tegra/la_priv.h
 *
 * Copyright (C) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_LA_PRIV_H_
#define _MACH_TEGRA_LA_PRIV_H_

#define ENABLE_LA_DEBUG		0

#define la_debug(fmt, ...) \
do { \
	if (ENABLE_LA_DEBUG) { \
		printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__); \
	} \
} while (0)

#define MASK(x) \
	((0xFFFFFFFFUL >> (31 - (1 ? x) + (0 ? x))) << (0 ? x))
#define SHIFT(x) \
	(0 ? x)
#define ID(id) \
	TEGRA_LA_##id

#define VALIDATE_ID(id, p) \
do { \
	if (id >= TEGRA_LA_MAX_ID || (p)->id_to_index[(id)] == 0xFFFF) { \
		WARN_ONCE(1, "%s: invalid Id=%d", __func__, (id)); \
		return -EINVAL; \
	} \
	BUG_ON((p)->la_info_array[(p)->id_to_index[(id)]].id != (id)); \
} while (0)

#define VALIDATE_BW(bw_in_mbps) \
do { \
	if (bw_in_mbps >= 4096) \
		return -EINVAL; \
} while (0)

#define VALIDATE_THRESHOLDS(tl, tm, th) \
do { \
	if ((tl) > 100 || (tm) > 100 || (th) > 100) \
		return -EINVAL; \
} while (0)

#define LAST_DISP_CLIENT_ID	ID(DISPLAYD)
#define NUM_DISP_CLIENTS	(LAST_DISP_CLIENT_ID - FIRST_DISP_CLIENT_ID + 1)
#define DISP_CLIENT_ID(id)	(ID(id) - FIRST_DISP_CLIENT_ID)

#define FIRST_CAMERA_CLIENT_ID	ID(VI_W)
#define LAST_CAMERA_CLIENT_ID	ID(ISP_WBB)
#define NUM_CAMERA_CLIENTS	(LAST_CAMERA_CLIENT_ID - \
				FIRST_CAMERA_CLIENT_ID + \
				1)
#define CAMERA_IDX(id)		(ID(id) - FIRST_CAMERA_CLIENT_ID)
#define CAMERA_LA_IDX(id)	(id - FIRST_CAMERA_CLIENT_ID)
#define AGG_CAMERA_ID(id)	TEGRA_LA_AGG_CAMERA_##id

#define T12X_MC_LA_MAX_VALUE	255


/* The following enum defines IDs for aggregated camera clients. In some cases
   we have to deal with groups of camera clients rather than individual
   clients. */
enum agg_camera_client_id {
	TEGRA_LA_AGG_CAMERA_VE = 0,
	TEGRA_LA_AGG_CAMERA_VE2,
	TEGRA_LA_AGG_CAMERA_ISP,
	TEGRA_LA_AGG_CAMERA_NUM_CLIENTS
};

struct la_client_info {
	unsigned int fifo_size_in_atoms;
	unsigned int expiration_in_ns;	/* worst case expiration value */
	void *reg_addr;
	unsigned long mask;
	unsigned long shift;
	enum tegra_la_id id;
	char *name;
	bool scaling_supported;
	unsigned int init_la;		/* initial la to set for client */
	unsigned int la_set;
	unsigned int la_ref_clk_mhz;
};

struct agg_camera_client_info {
	unsigned int bw_fp;
	unsigned int frac_fp;
	unsigned int ptsa_min;
	unsigned int ptsa_max;
	bool is_hiso;
};

struct la_scaling_info {
	unsigned int threshold_low;
	unsigned int threshold_mid;
	unsigned int threshold_high;
	int scaling_ref_count;
	int actual_la_to_set;
	int la_set;
};

struct la_scaling_reg_info {
	enum tegra_la_id id;
	void *tl_reg_addr;
	unsigned int tl_mask;
	unsigned int tl_shift;
	void *tm_reg_addr;
	unsigned int tm_mask;
	unsigned int tm_shift;
	void *th_reg_addr;
	unsigned int th_mask;
	unsigned int th_shift;
};

struct ptsa_info {
	unsigned int dis_ptsa_rate;
	unsigned int dis_ptsa_min;
	unsigned int dis_ptsa_max;
	unsigned int disb_ptsa_rate;
	unsigned int disb_ptsa_min;
	unsigned int disb_ptsa_max;
	unsigned int ve_ptsa_rate;
	unsigned int ve_ptsa_min;
	unsigned int ve_ptsa_max;
	unsigned int ve2_ptsa_rate;
	unsigned int ve2_ptsa_min;
	unsigned int ve2_ptsa_max;
	unsigned int ring2_ptsa_rate;
	unsigned int ring2_ptsa_min;
	unsigned int ring2_ptsa_max;
	unsigned int bbc_ptsa_rate;
	unsigned int bbc_ptsa_min;
	unsigned int bbc_ptsa_max;
	unsigned int mpcorer_ptsa_rate;
	unsigned int mpcorer_ptsa_min;
	unsigned int mpcorer_ptsa_max;
	unsigned int smmu_ptsa_rate;
	unsigned int smmu_ptsa_min;
	unsigned int smmu_ptsa_max;
	unsigned int ring1_ptsa_rate;
	unsigned int ring1_ptsa_min;
	unsigned int ring1_ptsa_max;

	unsigned int dis_extra_snap_level;
	unsigned int heg_extra_snap_level;
	unsigned int ptsa_grant_dec;
	unsigned int bbcll_earb_cfg;

	unsigned int isp_ptsa_rate;
	unsigned int isp_ptsa_min;
	unsigned int isp_ptsa_max;
	unsigned int a9avppc_ptsa_min;
	unsigned int a9avppc_ptsa_max;
	unsigned int avp_ptsa_min;
	unsigned int avp_ptsa_max;
	unsigned int r0_dis_ptsa_min;
	unsigned int r0_dis_ptsa_max;
	unsigned int r0_disb_ptsa_min;
	unsigned int r0_disb_ptsa_max;
	unsigned int vd_ptsa_min;
	unsigned int vd_ptsa_max;
	unsigned int mse_ptsa_min;
	unsigned int mse_ptsa_max;
	unsigned int gk_ptsa_min;
	unsigned int gk_ptsa_max;
	unsigned int vicpc_ptsa_min;
	unsigned int vicpc_ptsa_max;
	unsigned int apb_ptsa_min;
	unsigned int apb_ptsa_max;
	unsigned int pcx_ptsa_min;
	unsigned int pcx_ptsa_max;
	unsigned int host_ptsa_min;
	unsigned int host_ptsa_max;
	unsigned int ahb_ptsa_min;
	unsigned int ahb_ptsa_max;
	unsigned int sax_ptsa_min;
	unsigned int sax_ptsa_max;
	unsigned int aud_ptsa_min;
	unsigned int aud_ptsa_max;
	unsigned int sd_ptsa_min;
	unsigned int sd_ptsa_max;
	unsigned int usbx_ptsa_min;
	unsigned int usbx_ptsa_max;
	unsigned int usbd_ptsa_min;
	unsigned int usbd_ptsa_max;
	unsigned int ftop_ptsa_min;
	unsigned int ftop_ptsa_max;
};


struct la_chip_specific {
	int ns_per_tick;
	int atom_size;
	int la_max_value;
	spinlock_t lock;
	int la_info_array_size;
	struct la_client_info *la_info_array;
	unsigned short id_to_index[ID(MAX_ID) + 1];
	unsigned int disp_bw_array[NUM_DISP_CLIENTS];
	struct disp_client disp_clients[NUM_DISP_CLIENTS];
	unsigned int bbc_bw_array[ID(BBCLLR) - ID(BBCR) + 1];
	unsigned int camera_bw_array[NUM_CAMERA_CLIENTS];
	struct agg_camera_client_info
			agg_camera_array[TEGRA_LA_AGG_CAMERA_NUM_CLIENTS];
	struct la_scaling_info scaling_info[ID(MAX_ID)];
	int la_scaling_enable_count;
	struct dentry *latency_debug_dir;
	struct ptsa_info ptsa_info;
	bool disable_la;
	bool disable_ptsa;
	struct la_to_dc_params la_params;
	bool disable_disp_ptsa;
	bool disable_bbc_ptsa;

	void (*init_ptsa)(void);
	void (*update_display_ptsa_rate)(unsigned int *disp_bw_array);
	int (*update_camera_ptsa_rate)(enum tegra_la_id id,
					unsigned int bw_mbps,
					int is_hiso);
	int (*set_disp_la)(enum tegra_la_id id,
				unsigned int bw_mbps,
				struct dc_to_la_params disp_params);
	int (*set_la)(enum tegra_la_id id, unsigned int bw_mbps);
	int (*enable_la_scaling)(enum tegra_la_id id,
				unsigned int threshold_low,
				unsigned int threshold_mid,
				unsigned int threshold_high);
	void (*disable_la_scaling)(enum tegra_la_id id);
	int (*suspend)(void);
	void (*resume)(void);
};

void tegra_la_get_t3_specific(struct la_chip_specific *cs);
void tegra_la_get_t14x_specific(struct la_chip_specific *cs);
void tegra_la_get_t11x_specific(struct la_chip_specific *cs);
void tegra_la_get_t12x_specific(struct la_chip_specific *cs);

#endif /* _MACH_TEGRA_LA_PRIV_H_ */
