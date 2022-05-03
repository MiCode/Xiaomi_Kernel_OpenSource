/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef APU_EXCEP_H
#define APU_EXCEP_H

#define MON_PC (0x838)
#define MON_LR (0x83c)
#define MON_SP (0x840)
#define MD32_STATUS (0x844)

#define TBUF_DBG_SEL (0x84)
#define TBUF_DBG_DAT3 (0x94)

#define DBG_EN (0x0)
#define DBG_MODE (0x4)
#define DBG_INSTR (0x10)
#define DBG_INSTR_WR (0x14)
#define DBG_WDATA (0x18)
#define DBG_WDATA_WR (0x1c)
#define DBG_RDATA (0x20)
#define DBG_READY (0x24)

#define DBG_DATA_REG_INSTR (0x800)
#define DBG_ADDR_REG_INSTR (0x801)
#define DBG_INSTR_REG_INSTR (0x802)
#define DBG_STATUS_REG_INSTR (0x803)

#define DBG_REQUEST_INSTR (0x811)
#define DBG_RESUME_INSTR (0x812)
#define DBG_RESET_INSTR (0x813)
#define DBG_STEP_INSTR (0x814)
#define DBG_EXECUTE_INSTR (0x815)
#define DBG_READ_PM (0x840)
#define DBG_READ_DM (0x842)
#define DBG_ATTACH_INSTR (0x900)
#define DBG_DEATTACH_INSTR (0x901)

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define apusys_rv_aee_warn(module, reason) \
	do { \
		char mod_name[150];\
		if (snprintf(mod_name, 150, "%s_%s", reason, module) > 0) { \
			dev_info(dev, "%s: %s\n", reason, module); \
			aee_kernel_exception(mod_name, \
				"\nCRDISPATCH_KEY:%s\n", module); \
		} else { \
			dev_info(dev, "%s: snprintf fail(%d)\n", __func__, __LINE__); \
		} \
	} while (0)
#else
#define apusys_rv_aee_warn(module, reason)
#endif

int apu_excep_init(struct platform_device *pdev, struct mtk_apu *apu);
void apu_excep_remove(struct platform_device *pdev, struct mtk_apu *apu);

#endif /* APU_EXCEP_H */
