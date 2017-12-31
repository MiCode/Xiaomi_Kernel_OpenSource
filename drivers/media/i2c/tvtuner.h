/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __TV_TUNER_H__
#define __TV_TUNER_H__

#define PREFIX		"tv_tuner: "

#define TUNER_DEBUG(str, args...)		pr_debug(PREFIX str, ##args)
#define TUNER_ERROR(str, args...)		pr_err(PREFIX str, ##args)

#endif
