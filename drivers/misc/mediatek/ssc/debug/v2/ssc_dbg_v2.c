// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <ssc_module.h>
#include <mt-plat/ssc.h>
#include "ssc_sysfs.h"

static char *ssc_sysfs_ctrl_str[PW_SSC_REG_NUM] = {
	[PW_SSC_BASIC_SET] = "basic_set",
	[PW_SSC_SRAM_SW_REQ1] = "sram_sw_req1",
	[PW_SSC_SRAM_SW_REQ2] = "sram_sw_req2",
	[PW_SSC_SRAM_SW_REQ3] = "sram_sw_req3",
	[PW_SSC_SRAM_SW_REQ4] = "sram_sw_req4",
	[PW_SSC_VGPU_SET] = "vgpu_set",
	[PW_SSC_VISP_SET] = "visp_set",
	[PW_SSC_VCORE_SET] = "vcore_set",
	[PW_SSC_FORCE_SET] = "force_set",
	[PW_SSC_FORCE_CUR] = "force_cur",
	[PW_SSC_FORCE_TAR] = "force_tar",
	[PW_SSC_VSRAM_STA] = "vsram_sta",
	[PW_SSC_VGPU_STA] = "vgpu_sta",
	[PW_SSC_VISP_STA] = "visp_sta",
	[PW_SSC_VCORE_STA] = "vcore_sta",
	[PW_SSC_MUMTAS_STA] = "mumtas_sta",
	[PW_SSC_MUMTAS_SET] = "mumtas_set",
	[PW_SSC_MUMTAS_CLR] = "mumtas_clr",
	[PW_SSC_VSRAM_MASK] = "vsram_mask",
	[PW_SSC_VGPU_MASK] = "vgpu_mask",
	[PW_SSC_VISP_MASK] = "visp_mask",
	[PW_SSC_VCORE_MASK] = "vcore_mask",
	[PW_SSC_RESERVED] = "reserved",
	[PW_SSC_VGPU_RETRY] = "vgpu_retry",
	[PW_SSC_VISP_RETRY] = "visp_retry",
	[PW_SSC_VCORE_RETRY] = "vcore_retry",
	[PW_SSC_TIMEOUT_1] = "timeout_1",
	[PW_SSC_TIMEOUT_2] = "timeout_2",
	[PW_SSC_TIMEOUT_STA] = "timeout_sta",
	[PW_SSC_IRQ_SET] = "irq_set",
	[PW_SSC_IRQ_SET] = "irq_set",
	[PW_SSC_VAPU_SET] = "vapu_set",
	[PW_SSC_VAPU_STA] = "vapu_sta",
	[PW_SSC_VAPU_MASK] = "vapu_mask",
	[PW_SSC_VAPU_RETRY] = "vapu_retry",
	[PW_SSC_FORCE_CUR_2] = "force_cur",
	[PW_SSC_TIMEOUT_3] = "timeout_3",
	[PW_SSC_VSRAM_STA2] = "vsram_sta2",
	[PW_SSC_APU_MASK] = "apu_mask",
	[PW_SSC_GPU_MASK] = "gpu_mask",
};

static ssize_t ssc_ctrl_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)

{
	char *p = buf;
	size_t mSize = 0;
	int i;

	for (i = 0; i < PW_SSC_REG_NUM; i++) {
		mSize += scnprintf(p + mSize, PAGE_SIZE - mSize,
			"%s = 0x%zx\n",
			ssc_sysfs_ctrl_str[i],
			ssc_smc(SSC_REGISTER_ACCESS, SSC_ACT_READ, i, 0));
	}

	WARN_ON(PAGE_SIZE - mSize <= 0);

	return mSize;
}

static ssize_t ssc_ctrl_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 val;
	char cmd[64];
	int i;

	if (sscanf(buf, "%63s %x", cmd, &val) != 2)
		return -EINVAL;

	pr_info("[SSC] sysfs ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	for (i = 0 ; i < PW_SSC_REG_NUM; i++) {
		if (!strcmp(cmd, ssc_sysfs_ctrl_str[i])) {
			ssc_smc(SSC_REGISTER_ACCESS, SSC_ACT_WRITE,
				i, val);
			break;
		}
	}

	return count;
}
DEFINE_ATTR_RW(ssc_ctrl);

static ssize_t ssc_sw_req_show(struct kobject *kobj,
	 struct kobj_attribute *attr, char *buf)

{
	char *p = buf;
	size_t mSize = 0;

	mSize += scnprintf(p + mSize, PAGE_SIZE - mSize,
		"0x%zx\n",
		ssc_smc(SSC_REGISTER_ACCESS, SSC_ACT_READ, PW_SSC_SRAM_SW_REQ2, 0));

	WARN_ON(PAGE_SIZE - mSize <= 0);

	return mSize;
}

static ssize_t ssc_sw_req_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	u32 val;

	if (kstrtouint(buf, 10, &val) < 0)
		return -EINVAL;

	pr_info("[SSC] sw_req: val = 0x%x\n", val);

	ssc_smc(SSC_SW_REQ, val, 0, 0);

	return count;
}
DEFINE_ATTR_RW(ssc_sw_req);

static int __init ssc_v2_init(void)
{
	int ret;

	ret = sysfs_create_file(ssc_kobj, __ATTR_OF(ssc_ctrl));
	ret = sysfs_create_file(ssc_kobj, __ATTR_OF(ssc_sw_req));

	return 0;
}
static void __exit ssc_v2_exit(void)
{
	sysfs_remove_file(ssc_kobj, __ATTR_OF(ssc_ctrl));
	sysfs_remove_file(ssc_kobj, __ATTR_OF(ssc_sw_req));
}


module_init(ssc_v2_init);
module_exit(ssc_v2_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ssc v2 debug module");
MODULE_AUTHOR("MediaTek Inc.");

