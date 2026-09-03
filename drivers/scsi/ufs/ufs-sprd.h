/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_
#include <linux/platform_device.h>

#define UFS_MAX_GENERAL_LUN	8
#define SEGMENT_SIZE (512)

//for example 0x9/0xa == 0,so we need (0x9*100*4Mbyte/0xa/100)
#define PER_100  (100)
#define SIZE_1K  (1024)

enum ufs_sprd_caps {
	/*
	 * DWC Ufshc waits for the software to read the IS register and clear it,
	 * and then read HCS register. Only when the software has read these registers
	 * in proper sequence (clear IS register and then read HCS register), ufshc
	 * drives LP_pwr_gate to 1.
	 * This capability indicates that access to ufshc is forbidden after
	 * entering H8.
	 */
	UFS_SPRD_CAP_ACC_FORBIDDEN_AFTER_H8_EE         = 1 << 0,
};

struct ufs_sprd_host {
	struct ufs_hba *hba;
	struct scsi_device *sdev_ufs_rpmb;
	struct scsi_device *sdev_ufs_lu[UFS_MAX_GENERAL_LUN];
	enum ufs_sprd_caps caps;
	void *ufs_priv_data;

	bool ffu_is_process;

	bool debug_en;
	/* Panic when UFS encounters an error. */
	bool err_panic;
	/* Set when entering calibration mode. */
	bool cali_mode_enable;

	u8 pre_eol_info;
	u8 life_time_est_typ_a;
	u8 life_time_est_typ_b;

	struct completion pwm_async_done;
	struct completion hs_async_done;
	/* gic enable register address */
	void __iomem *gic_reg_enable;

	/*
	 * Records the IOCTL CMD being executed, used for ums9230 to change pwr mode
	 * to LS during cali.
	 */
	u32 ioctl_status;

	int (*check_stat_after_suspend)(struct ufs_hba *hba,
					enum ufs_notify_change_status);
	void (*priv_vh_send_uic)(struct ufs_hba *hba, struct uic_command *ucmd, int str);
};

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};


#define AUTO_H8_IDLE_TIME_10MS 0x1001

#define UFSHCI_VERSION_30	0x00000300 /* 3.0 */
#define UFSHCI_VERSION_21	0x00000210 /* 2.1 */

#define UFS_IOCTL_ENTER_MODE    0x5395
#define UFS_IOCTL_AFC_EXIT      0x5396

#define PWM_MODE_VAL    0x22
#define HS_MODE_VAL     0x11
#define PA_PeerRxHsAdaptInitial	0x15d3

int ufs_sprd_get_syscon_reg(struct device_node *np,
			    struct syscon_ufs *reg, const char *name);
void ufs_sprd_get_gic_reg(struct ufs_hba *hba);
void ufs_sprd_print_gic_reg(struct ufs_hba *hba);
int get_boot_mode(struct ufs_hba *hba);
int ufs_efuse_calib_data(struct platform_device *pdev,
				const char *cell_name);

extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9620_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9621_vops;
extern const struct ufs_hba_variant_ops ufs_hba_sprd_ums9230_vops;
extern struct ufs_perf_s *ufs_perf;

#endif/* _UFS_SPRD_H_ */
