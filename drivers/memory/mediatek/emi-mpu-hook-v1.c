// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <emi_mpu.h>
#include <linux/bits.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ratelimit.h>

static bool axi_id_is_gpu(unsigned int axi_id)
{
	unsigned int port;
	unsigned int id;

	port = axi_id & (BIT_MASK(3) - 1);
	id = axi_id >> 3;

	if (port == 6 && ((id & 0x7F80) == 0x2000))
		return true;
	else if (port == 7 && ((id & 0xFF81) == 0x2001))
		return true;
	else
		return false;
}

static irqreturn_t emi_mpu_isr_hook(unsigned int emi_id,
					struct reg_info_t *dump,
					unsigned int leng)
{
	int i;
	unsigned int srinfo_r = 0, axi_id_r = 0, err_case_r = 0;
	unsigned int srinfo_w = 0, axi_id_w = 0, err_case_w = 0;
	bool bypass;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	for (i = 0; i < leng; i++) {
		if (dump[i].offset == 0x1D8)
			srinfo_w = dump[i].value;
		else if (dump[i].offset == 0x3D8)
			srinfo_r = dump[i].value;

		if (dump[i].offset == 0x1D0)
			err_case_w = dump[i].value;
		else if (dump[i].offset == 0x3D0)
			err_case_r = dump[i].value;

		if (srinfo_w == 3) {
			if (dump[i].offset == 0x1E4) {
				axi_id_w |= dump[i].value & (BIT_MASK(16) - 1);
			} else if (dump[i].offset == 0x1E8) {
				axi_id_w |=
				(dump[i].value & (BIT_MASK(4) - 1)) << 16;
			}
		} else if (srinfo_r == 3) {
			if (dump[i].offset == 0x3E4) {
				axi_id_r |= dump[i].value & (BIT_MASK(16) - 1);
			} else if (dump[i].offset == 0x3E8) {
				axi_id_r |=
				(dump[i].value & (BIT_MASK(4) - 1)) << 16;
			}
		}
	}

	if (srinfo_r == 3 && !axi_id_is_gpu(axi_id_r))
		bypass = true;
	else if (srinfo_w == 3 && !axi_id_is_gpu(axi_id_w))
		bypass = true;
	else if (err_case_w == 0 && err_case_r == 0)
		bypass = true;
	else
		bypass = false;

	if (bypass == true) {
		if (__ratelimit(&ratelimit)) {
			pr_info("[MIUMPU]srinfo_r %d, axi_id_r %d\n",
				srinfo_r, axi_id_r);
			pr_info("[MIUMPU]srinfo_w %d, axi_id_w %d\n",
				srinfo_w, axi_id_w);
			pr_info("To bypass this violation\n");
		}
	}

	return (bypass) ? IRQ_HANDLED : IRQ_NONE;
}

static __init int emi_mpu_mt6983_init(void)
{
	int ret;

	pr_info("emi_mpu_mt6983 was loaded\n");

	ret = mtk_emimpu_isr_hook_register(emi_mpu_isr_hook);
	if (ret)
		pr_err("Failed to register the EMI MPU ISR hook function\n");

	return 0;
}

module_init(emi_mpu_mt6983_init);

MODULE_DESCRIPTION("MediaTek EMI MPU MT6983 Driver");
MODULE_LICENSE("GPL v2");
