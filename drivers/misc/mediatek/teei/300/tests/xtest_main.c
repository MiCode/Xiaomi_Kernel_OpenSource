/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/bitops.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/gameport.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/timex.h>
#include <linux/types.h>

#define IMSG_TAG "[tz_test_x]"
#include <imsg_log.h>
/* errors defined and used by TA/DRV test cases only */
#include "teei_internal_types.h"
#include <tee_client_api.h>
#include "xtest_main.h"
int my_run_case(int id)
{
#define JECASE(src, tgt) \
{ \
	if (src == tgt) { \
		ret = _CASE(tgt)(); \
	} \
}

	int ret = TEE_ERROR_UNKNOWN_COMMAND;

	case_res = 0;
	JECASE(id, 1001);
	JECASE(id, 1002);
	JECASE(id, 1003);
	JECASE(id, 1004);
	JECASE(id, 1005);
	JECASE(id, 1010);
	JECASE(id, 1011);
	JECASE(id, 1012);
	JECASE(id, 1013);
	JECASE(id, 1020);
	JECASE(id, 1021);
	JECASE(id, 1022);
	JECASE(id, 1023);
	JECASE(id, 1030);
	JECASE(id, 1031);
	JECASE(id, 1032);
	JECASE(id, 1033);
	JECASE(id, 1034);
	JECASE(id, 1035);
	JECASE(id, 1036);
	JECASE(id, 1037);
	JECASE(id, 1040);
	JECASE(id, 1041);
	JECASE(id, 1042);
	JECASE(id, 1043);
	JECASE(id, 1044);
	JECASE(id, 1045);
	JECASE(id, 1046);
	JECASE(id, 1047);
	JECASE(id, 1100);
	JECASE(id, 1101);
	JECASE(id, 1102);
	JECASE(id, 1200);
	JECASE(id, 1201);
	JECASE(id, 1203);
	JECASE(id, 1204);
	JECASE(id, 1206);
	JECASE(id, 1207);
	JECASE(id, 1208);
	JECASE(id, 1209);
	JECASE(id, 1210);
	JECASE(id, 1212);
	JECASE(id, 1213);
	JECASE(id, 1214);
	JECASE(id, 1215);
	JECASE(id, 1216);
	JECASE(id, 1217);
	JECASE(id, 1218);
	JECASE(id, 1219);
	JECASE(id, 1300);
	JECASE(id, 1301);
	JECASE(id, 1302);
	JECASE(id, 1303);
	JECASE(id, 1304);
	JECASE(id, 1305);
	JECASE(id, 1306);
	JECASE(id, 1307);
	JECASE(id, 1308);
	JECASE(id, 1400);
	JECASE(id, 1401);
	JECASE(id, 1402);
	JECASE(id, 1403);
	JECASE(id, 1404);
	JECASE(id, 1405);
	JECASE(id, 1406);
	JECASE(id, 1407);
	JECASE(id, 1408);
	JECASE(id, 1409);
	JECASE(id, 1410);
	JECASE(id, 1411);
	JECASE(id, 1412);
	JECASE(id, 1800);
	JECASE(id, 1801);
	JECASE(id, 1802);
	JECASE(id, 1803);
	JECASE(id, 3030);
	JECASE(id, 3040);
	JECASE(id, 3050);
	JECASE(id, 3051);
	JECASE(id, 3052);
	JECASE(id, 3053);
	return ret;
}

#define BTA_LOADER_HOSTNAME "bta_loader"

int kernel_ca_test(struct tzdrv_test_data *param)
{
	int ret;
	int cmd = param->params[0];

	TEEC_InitializeContext(BTA_LOADER_HOSTNAME, &xtest_isee_ctx_1000);
	ret = my_run_case(cmd - 1000);
	TEEC_FinalizeContext(&xtest_isee_ctx_1000);
	IMSG_INFO("%s (%d)=(0x%x)\n", __func__, cmd, ret);
	return ret;
}

int secure_drv_test(struct tzdrv_test_data *param)
{
	int ret;
	int cmd = param->params[0];

	TEEC_InitializeContext(BTA_LOADER_HOSTNAME, &xtest_isee_ctx_3000);
	ret = my_run_case(cmd);
	TEEC_FinalizeContext(&xtest_isee_ctx_3000);
	IMSG_INFO("%s (%d)=(0x%x)\n", __func__, cmd, ret);
	return ret;
}
