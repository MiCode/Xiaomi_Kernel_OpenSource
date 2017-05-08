/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __Q6COMMON_H__
#define __Q6COMMON_H__

#include <linux/qdsp6v2/apr.h>

void q6common_update_instance_id_support(bool supported);
bool q6common_is_instance_id_supported(void);

#endif /* __Q6COMMON_H__ */
