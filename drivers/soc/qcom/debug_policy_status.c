/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: debug_policy_status.c
 * Descrviption: get debug policy flag function.
 * Author: wanghua3@xiaomi.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <soc/qcom/scm.h>
#include <linux/of_device.h>

#define TZ_DUMP_SECURITY_ALLOWS_MEM_DUMP 0x10
#define DP_PROP_BUFF_SIZE 32

static void update_cmdline(int32_t dp_flag)
{
	char *prop_dp = NULL;
	char buff_prop[DP_PROP_BUFF_SIZE] = {'\0'};

	sprintf(buff_prop, " androidboot.dp=0x%1X", dp_flag);
	prop_dp = (char *)strnstr(saved_command_line, "androidboot.dp=" ,strlen(saved_command_line));

	if(prop_dp)
	{
		memcpy(prop_dp - 1, buff_prop, strlen(buff_prop));
	}
	else
		strcat(saved_command_line, buff_prop);
}

static int32_t get_sm_dp_info(void)
{
	struct scm_desc desc = {0};
	int ret;
	int dp_flag = 0x0;
	desc.args[0] = 0;
	desc.arginfo = 0;

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_UTIL, TZ_DUMP_SECURITY_ALLOWS_MEM_DUMP), &desc);
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

static int __init dp_flag_init(void)
{
	update_cmdline(get_sm_dp_info());
	return 0;
}

module_init(dp_flag_init);

MODULE_DESCRIPTION("Read debug policy flags to update property");
MODULE_LICENSE("GPL v2");
