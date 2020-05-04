/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */


#ifndef __DRV_CLK_FHCTL_DEBUG_H
#define __DRV_CLK_FHCTL_DEBUG_H

#if defined(CONFIG_COMMON_CLK_MTK_FREQ_HOPPING_DEBUG)
void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl);
void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl);
#else
static inline void mt_fhctl_init_debugfs(struct mtk_fhctl *fhctl)
{
}
static inline void mt_fhctl_exit_debugfs(struct mtk_fhctl *fhctl)
{
}
#endif

#endif /* __DRV_CLK_FHCTL_DEBUG_H */

