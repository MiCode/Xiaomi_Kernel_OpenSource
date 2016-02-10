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

#ifndef _IPAHAL_I_H_
#define _IPAHAL_I_H_

#define IPAHAL_DRV_NAME "ipahal"
#define IPAHAL_DBG(fmt, args...) \
	pr_debug(IPAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPAHAL_ERR(fmt, args...) \
	pr_err(IPAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

/*
 * struct ipahal_context - HAL global context data
 * @hw_type: IPA H/W type/version.
 * @base: Base address to be used for accessing IPA memory. This is
 *  I/O memory mapped address.
 */
struct ipahal_context {
	enum ipa_hw_type hw_type;
	void __iomem *base;
};

extern struct ipahal_context *ipahal_ctx;

#endif /* _IPAHAL_I_H_ */
