// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "q6_init.h"

static int __init audio_q6_init(void)
{
	adsp_err_init();
	audio_cal_init();
	rtac_init();
	adm_init();
	afe_init();
	spk_params_init();
	q6asm_init();
	q6lsm_init();
	voice_init();
	core_init();
	msm_audio_ion_init();
	audio_slimslave_init();
	avtimer_init();
#ifdef CONFIG_MSM_CSPL
	crus_sp_init();
#endif
/* for elus start */
#ifdef CONFIG_ELUS_PROXIMITY
	elliptic_driver_init();
#endif
/* for elus end */
	msm_mdf_init();
	voice_mhi_init();
	digital_cdc_rsc_mgr_init();
/* for mius start */
#ifdef CONFIG_MIUS_PROXIMITY
	mius_driver_init();
#endif
/* for mius end */
	return 0;
}

static void __exit audio_q6_exit(void)
{
	digital_cdc_rsc_mgr_exit();
	msm_mdf_exit();
#ifdef CONFIG_MSM_CSPL
	crus_sp_exit();
#endif
	avtimer_exit();
	audio_slimslave_exit();
	msm_audio_ion_exit();
	core_exit();
	voice_exit();
	q6lsm_exit();
	q6asm_exit();
	afe_exit();
	spk_params_exit();
	adm_exit();
	rtac_exit();
	audio_cal_exit();
/* for elus start */
#ifdef CONFIG_ELUS_PROXIMITY
	elliptic_driver_exit();
#endif
/* for elus end */
	adsp_err_exit();
	voice_mhi_exit();
/* for mius start */
#ifdef CONFIG_MIUS_PROXIMITY
	mius_driver_exit();
#endif
/* for mius end */
}

module_init(audio_q6_init);
module_exit(audio_q6_exit);
MODULE_DESCRIPTION("Q6 module");
MODULE_LICENSE("GPL v2");
