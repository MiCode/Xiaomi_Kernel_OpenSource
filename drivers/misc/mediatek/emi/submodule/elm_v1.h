/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#ifndef __ELM_H__
#define __ELM_H__

extern void mt_elm_init(void __iomem *elm_base);
extern unsigned int disable_emi_dcm(void);
extern void restore_emi_dcm(unsigned int emi_dcm_status);
extern void turn_on_elm(void);
extern void turn_off_elm(void);
extern void reset_elm(void);
extern void dump_elm(char *buf, unsigned int leng);
extern void save_debug_reg(void);
extern void suspend_elm(void);
extern void resume_elm(void);
extern void dump_last_bm(char *buf, unsigned int leng);

#endif /* __ELM_H__ */
