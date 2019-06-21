/*
Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include "machine_815x_init.h"

static int __init audio_machine_815x_init(void)
{
	sm8150_init();
	sa8155_init();
	return 0;
}

static void audio_machine_815x_exit(void)
{
	sm8150_exit();
	sa8155_exit();
}

module_init(audio_machine_815x_init);
module_exit(audio_machine_815x_exit);

MODULE_DESCRIPTION("Audio Machine 815X Driver");
MODULE_LICENSE("GPL v2");
