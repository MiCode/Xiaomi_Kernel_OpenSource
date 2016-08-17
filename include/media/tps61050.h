/* Copyright (C) 2011 NVIDIA Corporation.
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

#ifndef __TPS61050_H__
#define __TPS61050_H__

#include <media/nvc_torch.h>

#define TPS61050_MAX_TORCH_LEVEL	7
#define TPS61050_MAX_FLASH_LEVEL	8

struct tps61050_platform_data {
	unsigned cfg; /* use the  NVC_CFG_ defines */
	unsigned num; /* see implementation notes in driver */
	unsigned sync; /* see implementation notes in driver */
	const char *dev_name; /* see implementation notes in driver */
	struct nvc_torch_pin_state (*pinstate); /* see notes in driver */
	unsigned max_amp_torch; /* see implementation notes in driver */
	unsigned max_amp_flash; /* see implementation notes in driver */
};

#endif /* __TPS61050_H__ */
