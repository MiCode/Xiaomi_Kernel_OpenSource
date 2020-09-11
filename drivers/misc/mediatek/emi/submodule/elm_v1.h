/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
