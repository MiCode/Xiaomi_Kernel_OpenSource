/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_SIP_H
#define _UFS_MEDIATEK_SIP_H

#include <linux/soc/mediatek/mtk_sip_svc.h>

/*
 * SiP commands
 */
#define MTK_SIP_UFS_CONTROL               MTK_SIP_SMC_CMD(0x276)
#define UFS_MTK_SIP_VA09_PWR_CTRL         BIT(0)
#define UFS_MTK_SIP_DEVICE_RESET          BIT(1)
#define UFS_MTK_SIP_CRYPTO_CTRL           BIT(2)
#define UFS_MTK_SIP_REF_CLK_NOTIFICATION  BIT(3)
#define UFS_MTK_SIP_HOST_PWR_CTRL         BIT(5)
#define UFS_MTK_SIP_GET_VCC_INFO          BIT(6)

enum SIP_HOST_PWR_OPT {
	HOST_PWR_HCI,
	HOST_PWR_MPHY
};

/*
 * SMC call wapper function
 */
#define _ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, v6) \
		arm_smccc_smc(MTK_SIP_UFS_CONTROL, \
				  cmd, v1, v2, v3, v4, v5, v6, &(res))

#define _ufs_mtk_smc_0(cmd, res) \
	_ufs_mtk_smc(cmd, res, 0, 0, 0, 0, 0, 0)

#define _ufs_mtk_smc_1(cmd, res, v1) \
	_ufs_mtk_smc(cmd, res, v1, 0, 0, 0, 0, 0)

#define _ufs_mtk_smc_2(cmd, res, v1, v2) \
	_ufs_mtk_smc(cmd, res, v1, v2, 0, 0, 0, 0)

#define _ufs_mtk_smc_3(cmd, res, v1, v2, v3) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, 0, 0, 0)

#define _ufs_mtk_smc_4(cmd, res, v1, v2, v3, v4) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, 0, 0)

#define _ufs_mtk_smc_5(cmd, res, v1, v2, v3, v4, v5) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, 0)

#define _ufs_mtk_smc_6(cmd, res, v1, v2, v3, v4, v5, v6) \
	_ufs_mtk_smc(cmd, res, v1, v2, v3, v4, v5, v6)

#define _ufs_mtk_smc_selector(cmd, res, v1, v2, v3, v4, v5, v6, FUNC, ...) FUNC

#define ufs_mtk_smc(...) \
	_ufs_mtk_smc_selector(__VA_ARGS__, \
	_ufs_mtk_smc_6(__VA_ARGS__), \
	_ufs_mtk_smc_5(__VA_ARGS__), \
	_ufs_mtk_smc_4(__VA_ARGS__), \
	_ufs_mtk_smc_3(__VA_ARGS__), \
	_ufs_mtk_smc_2(__VA_ARGS__), \
	_ufs_mtk_smc_1(__VA_ARGS__), \
	_ufs_mtk_smc_0(__VA_ARGS__) \
	)

/* Sip UFS GET VCC INFO */
enum {
	VCC_NONE = 0,
	VCC_1,
	VCC_2
};

/* Sip kernel interface */
#define ufs_mtk_va09_pwr_ctrl(res, on) \
	ufs_mtk_smc(UFS_MTK_SIP_VA09_PWR_CTRL, res, on)

#define ufs_mtk_crypto_ctrl(res, enable) \
	ufs_mtk_smc(UFS_MTK_SIP_CRYPTO_CTRL, res, enable)

#define ufs_mtk_ref_clk_notify(on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_REF_CLK_NOTIFICATION, res, on)

#define ufs_mtk_device_reset_ctrl(high, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_RESET, res, high)

#define ufs_mtk_host_pwr_ctrl(opt, on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_HOST_PWR_CTRL, res, opt, on)

#define ufs_mtk_get_vcc_info(res) \
	ufs_mtk_smc(UFS_MTK_SIP_GET_VCC_INFO, res)

#define ufs_mtk_device_pwr_ctrl(on, ufs_version, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_PWR_CTRL, res, on, ufs_version)

#endif /* !_UFS_MEDIATEK_SIP_H */
