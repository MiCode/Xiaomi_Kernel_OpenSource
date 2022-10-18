/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "dp_aux_bridge.h"

static DEFINE_MUTEX(dp_aux_bridge_lock);
static LIST_HEAD(du_aux_bridge_list);

int dp_aux_add_bridge(struct dp_aux_bridge *bridge)
{
	mutex_lock(&dp_aux_bridge_lock);
	list_add_tail(&bridge->head, &du_aux_bridge_list);
	mutex_unlock(&dp_aux_bridge_lock);

	return 0;
}

#ifdef CONFIG_OF
struct dp_aux_bridge *of_dp_aux_find_bridge(struct device_node *np)
{
	struct dp_aux_bridge *bridge;

	mutex_lock(&dp_aux_bridge_lock);

	list_for_each_entry(bridge, &du_aux_bridge_list, head) {
		if (bridge->of_node == np) {
			mutex_unlock(&dp_aux_bridge_lock);
			return bridge;
		}
	}

	mutex_unlock(&dp_aux_bridge_lock);
	return NULL;
}
#endif

