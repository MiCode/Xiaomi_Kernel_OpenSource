/*
* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include "machine_615x_init.h"

static int __init audio_machine_615x_init(void)
{
	sm6150_init();
	return 0;
}

static void audio_machine_615x_exit(void)
{
	sm6150_exit();
}

module_init(audio_machine_615x_init);
module_exit(audio_machine_615x_exit);

MODULE_DESCRIPTION("Audio Machine 615X Driver");
MODULE_LICENSE("GPL v2");
