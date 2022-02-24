/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#ifndef _MTK_SECURE_API_H_
#define _MTK_SECURE_API_H_

#include <linux/kernel.h>
#include <linux/arm-smccc.h>

/* Error Code */
#define SIP_SVC_E_SUCCESS               0
#define SIP_SVC_E_NOT_SUPPORTED         -1
#define SIP_SVC_E_INVALID_PARAMS        -2
#define SIP_SVC_E_INVALID_Range         -3
#define SIP_SVC_E_PERMISSION_DENY       -4

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_AARCH_BIT			0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT			0x00000000
#endif

/*	0x82000200 -	0x820003FF &	0xC2000300 -	0xC20003FF */
/* Debug feature and ATF related */
#define MTK_SIP_KERNEL_WDT \
	(0x82000200 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_GIC_DUMP \
	(0x82000201 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_TIME_SYNC \
	(0x82000202 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AEE_DUMP \
	(0x82000203 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_ATF_DEBUG \
	(0x82000204 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_DFD \
	(0x82000205 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_CCCI_GET_INFO \
	(0x82000206 | MTK_SIP_SMC_AARCH_BIT)
/* v1.3 compatible */
#define MTK_SIP_KERNEL_DISABLE_DFD \
	(0x8200020A | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SCP_RESET \
	(0x8200020B | MTK_SIP_SMC_AARCH_BIT)

/* CPU operations */
#define MTK_SIP_POWER_DOWN_CLUSTER \
	(0x82000210 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_POWER_UP_CLUSTER \
	(0x82000211 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_POWER_DOWN_CORE	\
	(0x82000212 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_POWER_UP_CORE \
	(0x82000213 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_POWER_FLOW_DEBUG \
	(0x82000214 | MTK_SIP_SMC_AARCH_BIT)
/* Backward compatible */
#define MTK_SIP_KERNEL_MSG \
	MTK_SIP_POWER_FLOW_DEBUG

/* SPM related SMC call */
#define MTK_SIP_KERNEL_SPM_SUSPEND_ARGS \
	(0x82000220 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_FIRMWARE_STATUS \
	(0x82000221 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_IRQ0_HANDLER	\
	(0x82000222 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_AP_MDSRC_REQ	\
	(0x82000223 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS \
	(0x82000224 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_LEGACY_SLEEP	\
	(0x82000225 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_SPM_VCOREFS_ARGS	\
	(0x82000226 | MTK_SIP_SMC_AARCH_BIT)

/* SPM deepidle related SMC call */
#define MTK_SIP_KERNEL_SPM_DPIDLE_ARGS \
	(0x82000227 | MTK_SIP_SMC_AARCH_BIT)

/* SPM SODI related SMC call */
#define MTK_SIP_KERNEL_SPM_SODI_ARGS \
	(0x82000228 | MTK_SIP_SMC_AARCH_BIT)

/* SPM sleep deepidle related SMC call */
#define MTK_SIP_KERNEL_SPM_SLEEP_DPIDLE_ARGS \
	(0x82000229 | MTK_SIP_SMC_AARCH_BIT)
/* SPM ARGS */
#define MTK_SIP_KERNEL_SPM_ARGS	\
	(0x8200022A | MTK_SIP_SMC_AARCH_BIT)

/* SPM get pwr_ctrl args */
#define MTK_SIP_KERNEL_SPM_GET_PWR_CTRL_ARGS \
	(0x8200022B | MTK_SIP_SMC_AARCH_BIT)

/* SPM dummy read */
#define MTK_SIP_KERNEL_SPM_DUMMY_READ \
	(0x8200022C | MTK_SIP_SMC_AARCH_BIT)

/* SPM Check security CG (for deepidle/SODI) */
#define MTK_SIP_KERNEL_CHECK_SECURE_CG \
	(0x8200022D | MTK_SIP_SMC_AARCH_BIT)

/* SPM LP related SMC call */
#define MTK_SIP_KERNEL_SPM_LP_ARGS \
	(0x8200022E | MTK_SIP_SMC_AARCH_BIT)

/* SPM DCS S1 control */
#define MTK_SIP_KERNEL_SPM_DCS_S1 \
	(0x8200022F | MTK_SIP_SMC_AARCH_BIT)

/* Low power Misc. 1 SMC call,
 * 0x82000230 -	0x8200023F &	0xC2000230 -	0xC200023F
 */
/* DCM SMC call */
#define MTK_SIP_KERNEL_DCM \
	(0x82000230 | MTK_SIP_SMC_AARCH_BIT)
/* MCDI related SMC call */
#define MTK_SIP_KERNEL_MCDI_ARGS \
	(0x82000231 | MTK_SIP_SMC_AARCH_BIT)
/* SCP DVFS related SMC call */
#define MTK_SIP_KERNEL_SCP_DVFS_CTRL \
	(0x82000232 | MTK_SIP_SMC_AARCH_BIT)
/* v1.3 compatible */
#define MTK_SIP_KERNEL_MCUPM_FW_LOAD_STATUS	\
	(0x82000234 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_PMU_EVA \
	(0x82000235 | MTK_SIP_SMC_AARCH_BIT)

/* AMMS related SMC call */
#define MTK_SIP_KERNEL_AMMS_GET_FREE_ADDR \
	(0x82000250 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_GET_FREE_LENGTH \
	(0x82000251 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_GET_PENDING \
	(0x82000252 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_ACK_PENDING \
	(0x82000253 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_MD_POS_ADDR \
	(0x82000254 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_MD_POS_LENGTH \
	(0x82000255 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_GET_MD_POS_ADDR \
	(0x82000256 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_GET_MD_POS_LENGTH \
	(0x82000257 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_GET_SEQ_ID \
	(0x82000258 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_AMMS_POS_STRESS_TOUCH\
	(0x82000259 | MTK_SIP_SMC_AARCH_BIT)



/* Security related SMC call */
/* EMI MPU */
#define MTK_SIP_KERNEL_EMIMPU_WRITE (0x82000260 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_EMIMPU_READ  (0x82000261 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_EMIMPU_SET \
	(0x82000262 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_EMIMPU_CLEAR \
	(0x82000263 | MTK_SIP_SMC_AARCH_BIT)
/* DEVMPU */
#define MTK_SIP_KERNEL_DEVMPU_VIO_GET \
	(0x82000264 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_DEVMPU_PERM_GET \
	(0x82000265 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_DEVMPU_VIO_CLR \
	(0x82000268 | MTK_SIP_SMC_AARCH_BIT)
/* TRNG */
#define MTK_SIP_KERNEL_GET_RND \
	(0x8200026A | MTK_SIP_SMC_AARCH_BIT)
/* DEVAPC/SRAMROM */
#define MTK_SIP_KERNEL_DAPC_PERM_GET \
	(0x8200026B | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_CLR_SRAMROM_VIO \
	(0x8200026C | MTK_SIP_SMC_AARCH_BIT)
/* v1.4 compatible */
#define MTK_SIP_KERNEL_DAPC_DUMP \
	(0x8200026D | MTK_SIP_SMC_AARCH_BIT)
/* v1.3 compatible */
#define MTK_SIP_KERNEL_DAPC_INIT \
	(0x8200026E | MTK_SIP_SMC_AARCH_BIT)

/* Storage Encryption related SMC call */
/* HW FDE related SMC call */
#define MTK_SIP_KERNEL_HW_FDE_UFS_CTL \
	(0x82000270 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_HW_FDE_AES_INIT \
	(0x82000271 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_HW_FDE_KEY   \
	(0x82000272 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_HW_FDE_MSDC_CTL \
	(0x82000273 | MTK_SIP_SMC_AARCH_BIT)
/* HIE related SMC call */
#define MTK_SIP_KERNEL_CRYPTO_HIE_CFG_REQUEST \
	(0x82000274 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_CRYPTO_HIE_INIT \
	(0x82000275 | MTK_SIP_SMC_AARCH_BIT)
/* UFS generic SMC call */
#define MTK_SIP_KERNEL_UFS_CTL \
	(0x82000276 | MTK_SIP_SMC_AARCH_BIT)
/* v1.3 compatible */
/* LPDMA SMC calls */
#define MTK_SIP_KERNEL_LPDMA_WRITE \
	(0x82000277 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_LPDMA_READ \
	(0x82000278 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_LPDMA_GET \
	(0x82000279 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_DRAM_DCS_CHB \
	(0x8200027A | MTK_SIP_SMC_AARCH_BIT)

/* Platform related SMC call */
#define MTK_SIP_KERNEL_CACHE_FLUSH_FIQ \
	(0x82000280 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_CACHE_FLUSH_INIT \
	(0x82000281 | MTK_SIP_SMC_AARCH_BIT)

#define MTK_SIP_KERNEL_CACHE_FLUSH_BY_SF \
	(0x82000283 | MTK_SIP_SMC_AARCH_BIT)

/* v1.3 compatible */
#define MTK_SIP_KERNEL_L2_SHARING \
	(0x82000286 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_MCUSYS_WRITE \
	(0x82000287 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT \
	(0x82000288 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_MCSI_A_WRITE \
	(0x82000289 | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_MCSI_A_READ \
	(0x8200028A | MTK_SIP_SMC_AARCH_BIT)
#define MTK_SIP_KERNEL_MCSI_NS_ACCESS \
	(0x8200028B | MTK_SIP_SMC_AARCH_BIT)

/* Pheripheral related SMC call */
#define MTK_SIP_KERNEL_I2C_SEC_WRITE \
	(0x820002A0 | MTK_SIP_SMC_AARCH_BIT)

/* v1.4 Compatible */
/* M4U related SMC call */
#define MTK_M4U_DEBUG_DUMP \
	(0x820002B0 | MTK_SIP_SMC_AARCH_BIT)
/* APU related SMC call */
#define MTK_APU_VCORE_CG_CTL \
	(0x820002B1 | MTK_SIP_SMC_AARCH_BIT)

/* GIC related SMC call */
#define MTK_SIP_GIC_CONTROL \
	(0x82000501 | MTK_SIP_SMC_AARCH_BIT)

/* Debug related SMC call */
#define MTK_SIP_CCCI_CONTROL \
	(0x82000505 | MTK_SIP_SMC_AARCH_BIT)

/* MTK LPM */
#define MTK_SIP_MTK_LPM_CONTROL \
	(0x82000507 | MTK_SIP_SMC_AARCH_BIT)

/* TEEs related SMC call */
#define MTK_SIP_KERNEL_TEE_CONTROL \
	(0x82000516 | MTK_SIP_SMC_AARCH_BIT)

/* AUDIO related SMC call */
#define MTK_SIP_AUDIO_CONTROL \
	(0x82000517 | MTK_SIP_SMC_AARCH_BIT)

/* cmdq related SMC call */
#define MTK_SIP_CMDQ_CONTROL \
	(0x82000518 | MTK_SIP_SMC_AARCH_BIT)

/* APUSYS MNoC related SMC call */
#define MTK_SIP_APUSYS_MNOC_CONTROL \
	(0x82000519 | MTK_SIP_SMC_AARCH_BIT)

#define MTK_SIP_MMSRAM_CONTROL \
	(0x8200051D | MTK_SIP_SMC_AARCH_BIT)

/* APUSYS control related SMC call */
#define MTK_SIP_APUSYS_CONTROL \
	(0x8200051E | MTK_SIP_SMC_AARCH_BIT)

/* CACHE control related SMC call */
#define MTK_SIP_CACHE_CONTROL \
	(0x8200051F | MTK_SIP_SMC_AARCH_BIT)

/* TMEM */
#define MTK_SIP_TMEM_CONTROL \
	(0x82000524 | MTK_SIP_SMC_AARCH_BIT)

/* APUPWR control related SMC call */
#define MTK_SIP_APUPWR_CONTROL \
	(0x82000526 | MTK_SIP_SMC_AARCH_BIT)

#define mtk_idle_smc_impl(p1, p2, p3, p4, p5, res) \
	arm_smccc_smc(p1, p2, p3, p4,\
	p5, 0, 0, 0, &res)

#ifndef mt_secure_call
#define mt_secure_call(x1, x2, x3, x4, x5) ({\
	struct arm_smccc_res res;\
		mtk_idle_smc_impl(x1, x2, x3, x4, x5, res);\
		res.a0; })
#endif

#define mt_secure_call_ret1(_fun_id, _arg0, _arg1, _arg2, _arg3) \
	mt_secure_call_all(_fun_id, _arg0, _arg1, _arg2, _arg3, 0, 0, 0)
#define mt_secure_call_ret2(_fun_id, _arg0, _arg1, _arg2, _arg3, _r1) \
	mt_secure_call_all(_fun_id, _arg0, _arg1, _arg2, _arg3, _r1, 0, 0)
#define mt_secure_call_ret3(_fun_id, _arg0, _arg1, _arg2, _arg3, _r1, _r2) \
	mt_secure_call_all(_fun_id, _arg0, _arg1, _arg2, _arg3, _r1, _r2, 0)
#define mt_secure_call_ret4(_fun_id, _arg0,\
	_arg1, _arg2, _arg3, _r1, _r2, _r3) \
	mt_secure_call_all(_fun_id, _arg0, _arg1, _arg2, _arg3, _r1, _r2, _r3)

#define emi_mpu_smc_write(offset, val) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_WRITE, offset, val, 0, 0)

#define emi_mpu_smc_read(offset) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_READ, offset, 0, 0, 0)

#ifdef CONFIG_ARM64
#define emi_mpu_smc_set(start, end, region_permission) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_SET, start, end, region_permission, 0)
#else
#define emi_mpu_smc_set(start, end, apc8, apc0) \
	mt_secure_call_all(MTK_SIP_KERNEL_EMIMPU_SET,\
	start, end, apc8, apc0, 0, 0, 0)
#endif

#define emi_mpu_smc_clear(region) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_CLEAR, region, 0, 0, 0)

#define emi_mpu_smc_protect(start, end, apc) \
mt_secure_call(MTK_SIP_KERNEL_EMIMPU_SET, start, end, apc, 0)

/* v1.3 API Compatible  */
#define dram_smc_dcs(OnOff) \
mt_secure_call(MTK_SIP_KERNEL_DRAM_DCS_CHB, OnOff, 0, 0, 0)

#define lpdma_smc_write(offset, val) \
mt_secure_call(MTK_SIP_KERNEL_LPDMA_WRITE, offset, val, 0, 0)

#define lpdma_smc_read(offset) \
mt_secure_call(MTK_SIP_KERNEL_LPDMA_READ, offset, 0, 0, 0)

#define lpdma_smc_get_mode() \
mt_secure_call(MTK_SIP_KERNEL_LPDMA_GET, 0, 0, 0, 0)

#define mcsi_a_smc_read_phy(addr) \
mt_secure_call(MTK_SIP_KERNEL_MCSI_A_READ, addr, 0, 0, 0)

#define mcsi_a_smc_write_phy(addr, val) \
mt_secure_call(MTK_SIP_KERNEL_MCSI_A_WRITE, addr, val, 0, 0)

#define dcm_smc_msg(init_type) \
mt_secure_call(MTK_SIP_KERNEL_DCM, init_type, 0, 0, 0)

#define dcm_smc_read_cnt(type) \
mt_secure_call(MTK_SIP_KERNEL_DCM, type, 1, 0, 0)

#define CONFIG_MCUSYS_WRITE_PROTECT

#if defined(CONFIG_MCUSYS_WRITE_PROTECT) && \
	(defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI))

#define kernel_smc_msg(x1, x2, x3) \
	mt_secure_call(MTK_SIP_KERNEL_MSG, x1, x2, x3, 0)
#ifndef mcsi_reg_read
#define mcsi_reg_read(offset) \
	mt_secure_call(MTK_SIP_KERNEL_MCSI_NS_ACCESS, 0, offset, 0, 0)

#define mcsi_reg_write(val, offset) \
	mt_secure_call(MTK_SIP_KERNEL_MCSI_NS_ACCESS, 1, offset, val, 0)
#endif
#define mcsi_reg_set_bitmask(val, offset) \
	mt_secure_call(MTK_SIP_KERNEL_MCSI_NS_ACCESS, 2, offset, val, 0)

#define mcsi_reg_clear_bitmask(val, offset) \
	mt_secure_call(MTK_SIP_KERNEL_MCSI_NS_ACCESS, 3, offset, val, 0)

#ifdef CONFIG_ARM64		/* Kernel 64 */
#define mcusys_smc_write(virt_addr, val) \
	mt_reg_sync_writel(val, virt_addr)

#define mcusys_smc_write_phy(addr, val) \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, addr, val, 0, 0)

#define mcusys_access_count() \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT, 0, 0, 0, 0)

#else				/* Kernel 32 */
#define SMC_IO_VIRT_TO_PHY(addr) (addr-0xF0000000+0x10000000)
#define mcusys_smc_write(virt_addr, val) \
	mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, \
		SMC_IO_VIRT_TO_PHY(virt_addr), val, 0, 0)

#define mcusys_smc_write_phy(addr, val) \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_WRITE, addr, val, 0, 0)

#define mcusys_access_count() \
mt_secure_call(MTK_SIP_KERNEL_MCUSYS_ACCESS_COUNT, 0, 0, 0, 0)
#endif

#else
#define mcusys_smc_write(virt_addr, val) \
	mt_reg_sync_writel(val, virt_addr)
#define mcusys_access_count()               (0)
#define kernel_smc_msg(x1, x2, x3)
#endif

#endif				/* _MTK_SECURE_API_H_ */
