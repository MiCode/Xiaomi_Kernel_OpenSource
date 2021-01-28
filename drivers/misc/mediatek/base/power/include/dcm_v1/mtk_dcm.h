/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_DCM_H__
#define __MTK_DCM_H__

int mt_dcm_init(void);
void mt_dcm_disable(void);
void mt_dcm_restore(void);

/* unit of frequency is MHz */
extern int sync_dcm_set_cci_freq(unsigned int cci);
extern int sync_dcm_set_mp0_freq(unsigned int mp0);
extern int sync_dcm_set_mp1_freq(unsigned int mp1);
extern int sync_dcm_set_mp2_freq(unsigned int mp2);

#endif /* #ifndef __MTK_DCM_H__ */

