/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef MTK_TYPEC_MUX_SWITCH_H
#define MTK_TYPEC_MUX_SWITCH_H

#include <linux/usb/typec_mux.h>

struct typec_switch *mtk_typec_switch_register(struct device *dev,
			const struct typec_switch_desc *desc);
void mtk_typec_switch_unregister(struct typec_switch *sw);
struct typec_mux *mtk_typec_mux_register(struct device *dev,
			const struct typec_mux_desc *desc);
void mtk_typec_mux_unregister(struct typec_mux *mux);

extern void usb3_switch_set(int orientation);
#endif	/* MTK_TYPEC_MUX_SWITCH_H */
