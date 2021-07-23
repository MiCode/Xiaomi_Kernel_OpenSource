/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVMPU_EMI_H__
#define __DEVMPU_EMI_H__

#define EMI_MPUS_OFFSET		   (0x1F0)
#define EMI_MPUT_OFFSET		   (0x1F8)
#define EMI_MPUT_2ND_OFFSET    (0x1FC)

int devmpu_regist_emi(void);

#endif /* __DEVMPU_EMI_H__ */
