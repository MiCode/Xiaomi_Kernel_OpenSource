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
	unsigned int i;

	port = axi_id & (BIT_MASK(3) - 1);
	id = axi_id >> 3;

	for (i = 0; i < global_emi_mpu->bypass_axi_num; i++) {
		/* master is MFG_MPU(GPU MPU) */
		if (port == global_emi_mpu->bypass_axi[i].port
		&& ((id & global_emi_mpu->bypass_axi[i].axi_mask)
		== global_emi_mpu->bypass_axi[i].axi_value))
			return true;
	}

	return false;
}
int bypass_info(unsigned int offset)
{
	unsigned int i;

	for (i = 0; i < global_emi_mpu->bypass_miu_reg_num; i++) {
		if (offset == global_emi_mpu->bypass_miu_reg[i])
			return i;
	}
	return -1;
}

static irqreturn_t emi_mpu_isr_hook(unsigned int emi_id,
					struct reg_info_t *dump,
					unsigned int leng)
{
	int i;
	unsigned int srinfo_r = 0, axi_id_r = 0;
	unsigned int srinfo_w = 0, axi_id_w = 0;
	bool bypass;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 3);

	for (i = 0; i < leng; i++) {
		switch (bypass_info(dump[i].offset)) {
		case WRITE_SRINFO:
			srinfo_w = dump[i].value;
			break;
		case READ_SRINFO:
			srinfo_r = dump[i].value;
			break;
		case WRITE_AXI:
			if (srinfo_w == 3)
				axi_id_w |= (dump[i].value & (BIT_MASK(20) - 1));
			break;
		case READ_AXI:
			if (srinfo_r == 3)
				axi_id_r |= (dump[i].value & (BIT_MASK(20) - 1));
			break;
		case WRITE_AXI_MSB:
			if (srinfo_w == 3) {
				axi_id_w &= (BIT_MASK(16) - 1);
				axi_id_w |=
					((dump[i].value & (BIT_MASK(4) - 1)) << 16);
			}
			break;
		case READ_AXI_MSB:
			if (srinfo_r == 3) {
				axi_id_r &= (BIT_MASK(16) - 1);
				axi_id_r |=
					((dump[i].value & (BIT_MASK(4) - 1)) << 16);
			}
			break;
		default:
			break;
		}
	}

	if (srinfo_r == 3 && !axi_id_is_gpu(axi_id_r))
		bypass = true;
	else if (srinfo_w == 3 && !axi_id_is_gpu(axi_id_w))
		bypass = true;
	else
		bypass = false;

	if (bypass == true) {
		if (__ratelimit(&ratelimit)) {
			pr_info("srinfo_r %d, axi_id_r 0x%x\n",
				srinfo_r, axi_id_r);
			pr_info("srinfo_w %d, axi_id_w 0x%x\n",
				srinfo_w, axi_id_w);
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
