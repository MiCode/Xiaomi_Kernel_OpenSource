/*
 * Copyright (C) 2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __NVC_UTILITIES_H__
#define __NVC_UTILITIES_H__

#include <linux/of.h>
#include <media/nvc.h>
#include <media/nvc_image.h>

int nvc_imager_parse_caps(struct device_node *np,
		struct nvc_imager_cap *imager_cap);

unsigned long nvc_imager_get_mclk(const struct nvc_imager_cap *cap,
				  const struct nvc_imager_cap *cap_default,
				  int profile);

#endif /* __NVC_UTILITIES_H__ */

