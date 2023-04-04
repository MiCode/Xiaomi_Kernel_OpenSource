/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __EMI_ELM_H__
#define __EMI_ELM_H__

bool is_elm_enabled(void);
void enable_elm(void);
void disable_elm(void);

void elm_dump(char *buf, unsigned int leng);
void suspend_elm(void);
void resume_elm(void);

void elm_init(void);

#endif  /* !__EMI_ELM_H__ */
