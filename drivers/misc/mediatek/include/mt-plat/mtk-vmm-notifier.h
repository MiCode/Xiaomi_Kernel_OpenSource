/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __MTK_VMM_NOTIFIER__
#define __MTK_VMM_NOTIFIER__

/**
 * @brief Notifiy isp status
 *
 * @param openIsp 1: isp on 0: isp off
 * @return int
 */
int vmm_isp_ctrl_notify(int openIsp);

#endif
