/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_INFO_H_
#define _MT_PMIC_INFO_H_

/*
 * The CHIP INFO
 */
#define PMIC6355_E1_CID_CODE    0x5510
#define PMIC6355_E2_CID_CODE    0x5520
#define PMIC6355_E3_CID_CODE    0x5530

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define IPIMB
#endif

extern unsigned int pmic_ipi_test_code(void);

/*
 * Debugfs
 */
#define PMICTAG                "[PMIC] "
extern unsigned int gPMICDbgLvl;

#define PMIC_LOG_DBG     4
#define PMIC_LOG_INFO    3
#define PMIC_LOG_NOT     2
#define PMIC_LOG_WARN    1
#define PMIC_LOG_ERR     0

#define PMICLOG(fmt, arg...) do { \
	if (gPMICDbgLvl >= PMIC_LOG_DBG) \
		pr_notice(PMICTAG "%s: " fmt, __func__, ##arg); \
} while (0)


/* MT6355 Export API */
extern unsigned int pmic_scp_set_vcore(unsigned int voltage);
extern unsigned int pmic_scp_set_vsram_vcore(unsigned int voltage);
extern unsigned int enable_vsram_vcore_hw_tracking(unsigned int en);

#endif				/* _MT_PMIC_INFO_H_ */
