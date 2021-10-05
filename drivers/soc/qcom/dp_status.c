/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2020. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: debug_policy_status.c
 * Descrviption: get debug policy flag function.
 * Author: xuhaiwang@xiaomi.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <soc/qcom/qseecom_scm.h>
#include <linux/of_device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#define TZ_DUMP_SECURITY_ALLOWS_MEM_DUMP 0x10
#define DP_PROP_BUFF_SIZE 64

static int32_t get_sm_dp_info(void)
{
	struct scm_desc desc = {0};
	int ret;
	int dp_flag = 0x0;
	desc.args[0] = 0;
	desc.arginfo = 0;

	ret = qcom_scm_qseecom_call(SCM_SIP_FNID(SCM_SVC_UTIL, TZ_DUMP_SECURITY_ALLOWS_MEM_DUMP), &desc);
	if(!ret){
		if(1 == desc.ret[0])
		{
			dp_flag = 0xf & desc.ret[2];
		}
	}else{
		pr_err("%s : SCM call failed.\n", __func__);
	}
	pr_info("read debug policy flags  : desc.ret[0] = %x desc.ret[1] = %x desc.ret[2] = %x  dp_flag = %x\n",desc.ret[0], desc.ret[1], desc.ret[2] ,dp_flag);

	return dp_flag;
}

static int dp_status_proc_show(struct seq_file *m, void *v)
{
	char buff_prop[DP_PROP_BUFF_SIZE] = {'\0'};
	sprintf(buff_prop, "0x%1X",get_sm_dp_info());
	seq_puts(m, buff_prop);
	return 0 ;
}

static int __init dp_flag_init(void)
{
	proc_create_single("dp_flag", 0, NULL, dp_status_proc_show);
	return 0;
}

module_init(dp_flag_init);

MODULE_DESCRIPTION("Read debug policy flags to update property");
MODULE_LICENSE("GPL v2");
