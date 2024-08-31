/*
 * Broadcom Dongle Host Driver (DHD)
 * Header file for DHD Implementation for ULP mode opernation.
 *
 * Portions of this code are copyright (c) 2023 Cypress Semiconductor Corporation
 *
 * Copyright (C) 1999-2018, Broadcom Corporation
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: $
 */

#ifndef _DHD_ULP_H_
#define _DHD_ULP_H_

#if defined(DHD_ULP)

#ifdef DHD_HUDI
#define D11_BASE_ADDR			0x18001000
#define D11_AXI_BASE_ADDR		0xE8000000
#define D11_SHM_BASE_ADDR		(D11_AXI_BASE_ADDR + 0x4000)

#define D11REG_ADDR(offset)		(D11_BASE_ADDR + offset)
#define D11IHR_ADDR(offset)		(D11_AXI_BASE_ADDR + 0x400 + (2 * offset))
#define D11SHM_ADDR(offset)		(D11_SHM_BASE_ADDR + offset)

/* MacControl register */
#define D11_MACCONTROL_REG		D11REG_ADDR(0x120)
#define D11_MACCONTROL_REG_WAKE		(1 << 26)

#define DHD_ULP_HUDI_PROC_DONE_TIME	10000

/* SHM offsets */
#define M_DS1_CTRL_SDIO(ptr)		(ptr)->ulp_shm_offset->m_ulp_ctrl_sdio
#define M_WAKEEVENT_IND(ptr)		(ptr)->ulp_shm_offset->m_ulp_wakeevt_ind
#define M_ULP_WAKE_IND(ptr)			(ptr)->ulp_shm_offset->m_ulp_wakeind

/* Following are the offsets in M_DRVR_UCODE_IF_PTR block. Start address of
 * M_DRVR_UCODE_IF_PTR block is present in M_DRVR_UCODE_IF_PTR.
 */
#define M_ULP_WAKE_IND			(0x2 * 2)

/* M_ULP_WAKE_IND bits */
#define ULP_WAKE_IND_WATCHDOG_EXP	(1 << 0)
#define ULP_WAKE_IND_FCBS_ERROR		(1 << 1)
#define ULP_WAKE_IND_RE_TRANSMIT_ERR	(1 << 2)
#define ULP_WAKE_IND_HOST_WKUP		(1 << 3)
#define ULP_WAKE_IND_INVALID_FCBS_BLK	(1 << 4)

#define	C_DS1_CTRL_SDIO_DS1_SLEEP	(1 << 0)
#define	C_DS1_CTRL_SDIO_MAC_ON		(1 << 1)
#define	C_DS1_CTRL_SDIO_RADIO_PHY_ON	(1 << 2)
#define	C_DS1_CTRL_SDIO_DS1_EXIT	(1 << 3)
#define	C_DS1_CTRL_PROC_DONE		(1 << 8)
#define	C_DS1_CTRL_REQ_VALID		(1 << 9)

#define D11SHM_WR(sdh, offset, val) \
	do { \
		DHD_ERROR(("%s: %d: D11SHM_WR addr: 0x%08x, val: 0x%08x\n", \
			__func__, __LINE__, (uint32)D11SHM_ADDR(offset), (uint32) val)); \
		if (bcmsdh_reg_write(sdh, D11SHM_ADDR(offset), 2, val)) { \
			DHD_ERROR(("%s: %d: D11SHM_WR failed! addr: 0x%08x, val: 0x%08x\n", \
				__func__, __LINE__, (uint32)D11SHM_ADDR(offset), (uint32) val)); \
		} \
	} while (0);

#define D11SHM_RD(sdh, offset) \
	bcmsdh_reg_read(sdh, D11SHM_ADDR(offset), 2)

#define D11REG_WR(sdh, addr, val) \
	bcmsdh_reg_write(sdh, addr, 4, val)

#define D11REG_RD(sdh, addr) \
	bcmsdh_reg_read(sdh, addr, 4)

#define SET_D11HOSTWAKE(sdh, val32) \
	do {\
		val32 = D11REG_RD(sdh, D11_MACCONTROL_REG); \
		DHD_ERROR(("%s: %d:before: maccontrol: 0x%08x\n", __func__, __LINE__, val32)); \
		val32 = val32 | D11_MACCONTROL_REG_WAKE; \
		D11REG_WR(sdh, D11_MACCONTROL_REG, val32); \
		val32 = D11REG_RD(sdh, D11_MACCONTROL_REG); \
		DHD_ERROR(("%s: %d:after: maccontrol: 0x%08x\n", __func__, __LINE__, val32)); \
	} while (0);

#define CLR_D11HOSTWAKE(sdh, val32) \
	do {\
		val32 = D11REG_RD(sdh, D11_MACCONTROL_REG); \
		val32 = val32 & (~D11_MACCONTROL_REG_WAKE); \
		D11REG_WR(sdh, D11_MACCONTROL_REG, val32); \
	} while (0);

#endif /* DHD_HUDI */

/* DHD ULP tx/rx transition states */
enum dhd_ulp_states {
	DHD_ULP_DISABLED,
	DHD_ULP_READY,
	DHD_ULP_MAX_STATE
};

enum dhd_ulp_paths {
	DHD_ULP_NO_PATH = 0,
	DHD_ULP_TX_DATA = 1,
	DHD_ULP_TX_CTRL = 2,
	DHD_ULP_RX = 3
};

extern bool dhd_ulp_pre_redownload_check(dhd_pub_t *dhdp, bcmsdh_info_t *sdh, uint32 hmbdata);
extern enum dhd_ulp_states dhd_ulp_get_ulp_state(dhd_pub_t *dhdp);
extern void dhd_ulp_set_ulp_state(dhd_pub_t *dhdp, enum dhd_ulp_states ulp_state);
extern int dhd_ulp_f2_ready(dhd_pub_t *dhdp, bcmsdh_info_t *sdh);
extern void dhd_ulp_set_path(dhd_pub_t *dhdp, int path);
extern void dhd_ulp_reset_ulp_ready_counter(dhd_pub_t *dhdp);
extern int dhd_ulp_check_ulp_request(dhd_pub_t *dhdp, void *buf);
extern bool dhd_ulp_init(osl_t *osh, dhd_pub_t *dhdp);
extern void dhd_ulp_deinit(osl_t *osh, dhd_pub_t *dhdp);
#ifdef DHD_DEBUG
extern void dhd_ulp_save_console_interval(dhd_pub_t *dhdp);
extern void dhd_ulp_restore_console_interval(dhd_pub_t *dhdp);
#endif /* DHD_DEBUG */
extern void dhd_ulp_disable_cached_sbwad(dhd_pub_t *dhdp);
extern void dhd_ulp_enable_cached_sbwad(dhd_pub_t *dhdp, bcmsdh_info_t *sdh);
extern void dhd_ulp_preinit(dhd_pub_t *dhdp);
#endif /* DHD_ULP */

#endif /* _DHD_ULP_H_ */
