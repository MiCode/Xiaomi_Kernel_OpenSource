// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "audio_utils.h"

spinlock_t enc_dec_lock;

static int __init audio_native_init(void)
{
	aac_in_init();
	amrnb_in_init();
	amrwb_in_init();
	audio_aac_init();
	audio_alac_init();
	audio_amrnb_init();
	audio_amrwb_init();
	audio_amrwbplus_init();
	audio_ape_init();
	audio_evrc_init();
	audio_g711alaw_init();
	audio_g711mlaw_init();
	audio_effects_init();
	audio_mp3_init();
	audio_multiaac_init();
	audio_qcelp_init();
	audio_wma_init();
	audio_wmapro_init();
	evrc_in_init();
	g711alaw_in_init();
	g711mlaw_in_init();
	qcelp_in_init();
	spin_lock_init(&enc_dec_lock);
	return 0;
}

static void __exit audio_native_exit(void)
{
	aac_in_exit();
	amrnb_in_exit();
	amrwb_in_exit();
	audio_aac_exit();
	audio_alac_exit();
	audio_amrnb_exit();
	audio_amrwb_exit();
	audio_amrwbplus_exit();
	audio_ape_exit();
	audio_evrc_exit();
	audio_g711alaw_exit();
	audio_g711mlaw_exit();
	audio_effects_exit();
	audio_mp3_exit();
	audio_multiaac_exit();
	audio_qcelp_exit();
	audio_wma_exit();
	audio_wmapro_exit();
	evrc_in_exit();
	g711alaw_in_exit();
	g711mlaw_in_exit();
	qcelp_in_exit();
}

module_init(audio_native_init);
module_exit(audio_native_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Native Encoder/Decoder module");
