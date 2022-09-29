/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_SMI_DEBUG_H
#define __MTK_SMI_DEBUG_H

#define MAX_MON_REQ	(4)

enum smi_mon_id {
	SMI_BW_MET,
	SMI_BW_BUS,
	SMI_BW_IMGSYS,
};

#if IS_ENABLED(CONFIG_MTK_SMI)

int mtk_smi_dbg_register_notifier(struct notifier_block *nb);
int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb);
int mtk_smi_dbg_register_force_on_notifier(struct notifier_block *nb);
int mtk_smi_dbg_unregister_force_on_notifier(struct notifier_block *nb);
s32 smi_monitor_start(struct device *dev, u32 common_id, u32 commonlarb_id[MAX_MON_REQ],
			u32 flag[MAX_MON_REQ], enum smi_mon_id mon_id);
s32 smi_monitor_stop(struct device *dev, u32 common_id,
			u32 *bw, enum smi_mon_id mon_id);
#else

static inline int mtk_smi_dbg_register_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_register_force_on_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int mtk_smi_dbg_unregister_force_on_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline s32 smi_monitor_start(struct device *dev, u32 common_id,
		u32 commonlarb_id[MAX_MON_REQ], u32 flag[MAX_MON_REQ], enum smi_mon_id mon_id)
{
	return 0;
}

static inline s32 smi_monitor_stop(struct device *dev, u32 common_id,
				u32 *bw, enum smi_mon_id mon_id)
{
	return 0;
}

#endif /* CONFIG_MTK_SMI */

#endif /* __MTK_SMI_DEBUG_H */

