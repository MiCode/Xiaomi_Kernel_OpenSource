/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef __WCD_IRQ_H_
#define __WCD_IRQ_H_

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>

struct wcd_irq_info {
	struct regmap_irq_chip *wcd_regmap_irq_chip;
	char *codec_name;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_chip;
	struct device *dev;
};

#if (IS_ENABLED(CONFIG_WCD9XXX_CODEC_CORE) | \
	IS_ENABLED(CONFIG_WCD9XXX_CODEC_CORE_V2))
int wcd_irq_init(struct wcd_irq_info *irq_info, struct irq_domain **virq);
int wcd_irq_exit(struct wcd_irq_info *irq_info, struct irq_domain *virq);
int wcd_request_irq(struct wcd_irq_info *irq_info, int irq, const char *name,
			irq_handler_t handler, void *data);
void wcd_free_irq(struct wcd_irq_info *irq_info, int irq, void *data);
void wcd_enable_irq(struct wcd_irq_info *irq_info, int irq);
void wcd_disable_irq(struct wcd_irq_info *irq_info, int irq);
#else
static inline int wcd_irq_init(struct wcd_irq_info *irq_info,
			       struct irq_domain **virq)
{
	return 0;
};
static inline int wcd_irq_exit(struct wcd_irq_info *irq_info,
			       struct irq_domain *virq)
{
	return 0;
};
static inline int wcd_request_irq(struct wcd_irq_info *irq_info,
				  int irq, const char *name,
				  irq_handler_t handler, void *data)
{
	return 0;
};
static inline void wcd_free_irq(struct wcd_irq_info *irq_info, int irq, void *data);
{
};
static inline void wcd_enable_irq(struct wcd_irq_info *irq_info, int irq);
{
};
static inline void wcd_disable_irq(struct wcd_irq_info *irq_info, int irq);
{
};
#endif
#endif /* __WCD_IRQ_H_ */
