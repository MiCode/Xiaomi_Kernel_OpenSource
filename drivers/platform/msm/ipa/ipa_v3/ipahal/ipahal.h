/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPAHAL_H_
#define _IPAHAL_H_

#include <linux/msm_ipa.h>

int ipahal_init(enum ipa_hw_type ipa_hw_type, void __iomem *base);

void ipahal_destroy(void);

#endif /* _IPAHAL_H_ */
