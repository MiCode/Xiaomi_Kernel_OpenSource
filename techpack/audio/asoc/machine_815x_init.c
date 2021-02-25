// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "machine_815x_init.h"

static int __init audio_machine_815x_init(void)
{
	sm8150_init();
	return 0;
}

static void audio_machine_815x_exit(void)
{
	sm8150_exit();
}

module_init(audio_machine_815x_init);
module_exit(audio_machine_815x_exit);

MODULE_DESCRIPTION("Audio Machine 815X Driver");
MODULE_LICENSE("GPL v2");
