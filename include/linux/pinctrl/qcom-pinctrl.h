/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_PINCTRL_MSM_H__
#define __LINUX_PINCTRL_MSM_H__

/* APIS to access qup_i3c registers */
int msm_qup_write(u32 mode, u32 val);
int msm_qup_read(u32 mode);


#endif /* __LINUX_PINCTRL_MSM_H__ */
