/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
#include <linux/string.h>
#include <linux/wait.h>

#include "lcm_define.h"
#include "lcm_drv.h"
#include "lcm_util.h"


static enum LCM_STATUS _lcm_util_check_data(char type,
	const struct LCM_DATA_T1 *t1)
{
	switch (type) {
	case LCM_UTIL_RESET:
		switch (t1->data) {
		case LCM_UTIL_RESET_LOW:
		case LCM_UTIL_RESET_HIGH:
			break;

		default:
			return LCM_STATUS_ERROR;
		}
		break;

	case LCM_UTIL_MDELAY:
	case LCM_UTIL_UDELAY:
	case LCM_UTIL_RAR:
		/* no limitation */
		break;

	default:
		return LCM_STATUS_ERROR;
	}


	return LCM_STATUS_OK;
}


static enum LCM_STATUS _lcm_util_check_write_cmd_v1(
	const struct LCM_DATA_T5 *t5)
{
	if (t5 == NULL)
		return LCM_STATUS_ERROR;
	if (t5->size == 0)
		return LCM_STATUS_ERROR;

	return LCM_STATUS_OK;
}


static enum LCM_STATUS _lcm_util_check_write_cmd_v2(
	const struct LCM_DATA_T3 *t3)
{
	if (t3 == NULL)
		return LCM_STATUS_ERROR;
	if ((t3->size > 0) && (t3->data == NULL))
		return LCM_STATUS_ERROR;

	return LCM_STATUS_OK;
}


static enum LCM_STATUS _lcm_util_check_write_cmd_v23(
	const struct LCM_DATA_T3 *t3)
{
	if (t3 == NULL)
		return LCM_STATUS_ERROR;

	return LCM_STATUS_OK;
}


static enum LCM_STATUS _lcm_util_check_read_cmd_v2(
	const struct LCM_DATA_T4 *t4)
{
	if (t4 == NULL)
		return LCM_STATUS_ERROR;

	return LCM_STATUS_OK;
}


enum LCM_STATUS lcm_util_set_data(const struct LCM_UTIL_FUNCS *lcm_util,
	char type, struct LCM_DATA_T1 *t1)
{
	/* check parameter is valid */
	if (_lcm_util_check_data(type, t1) == LCM_STATUS_OK) {
		switch (type) {
		case LCM_UTIL_RESET:
			lcm_util->set_reset_pin((unsigned int)t1->data);
			break;

		case LCM_UTIL_MDELAY:
			lcm_util->mdelay((unsigned int)t1->data);
			break;

		case LCM_UTIL_UDELAY:
			lcm_util->udelay((unsigned int)t1->data);
			break;

		case LCM_UTIL_RAR:
			lcm_util->rar((unsigned int)t1->data);
			break;

		default:
			pr_debug("[LCM][ERROR] %s/%d: %d\n",
				__func__, __LINE__, type);
			return LCM_STATUS_ERROR;
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%x, 0x%x\n",
			__func__, __LINE__, type, t1->data);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}


enum LCM_STATUS lcm_util_set_write_cmd_v1(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T5 *t5,
	unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd[32];

	/* check parameter is valid */
	if (_lcm_util_check_write_cmd_v1(t5) == LCM_STATUS_OK) {
		memset(cmd, 0x0, sizeof(unsigned int) * 32);
		for (i = 0; i < t5->size; i++)
			cmd[i] = (t5->cmd[i * 4 + 3] << 24)
				| (t5->cmd[i * 4 + 2] << 16)
				| (t5->cmd[i * 4 + 1] << 8)
				| (t5->cmd[i * 4]);
		lcm_util->dsi_set_cmdq(cmd, (unsigned int)t5->size,
			force_update);
	} else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%p, %d\n",
			__func__, __LINE__, t5->cmd, t5->size);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}

enum LCM_STATUS lcm_util_set_write_cmd_v11(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T5 *t5,
	unsigned char force_update, void *cmdq)
{
	unsigned int i;
	unsigned int cmd[32];

	if (lcm_util->dsi_set_cmdq_V11 == NULL)
		return LCM_STATUS_OK;

	/* check parameter is valid */
	if (_lcm_util_check_write_cmd_v1(t5) == LCM_STATUS_OK) {
		memset(cmd, 0x0, sizeof(unsigned int) * 32);
		for (i = 0; i < t5->size; i++)
			cmd[i] = (t5->cmd[i * 4 + 3] << 24)
				| (t5->cmd[i * 4 + 2] << 16)
				| (t5->cmd[i * 4 + 1] << 8)
				| (t5->cmd[i * 4]);
			lcm_util->dsi_set_cmdq_V11(cmdq, cmd,
				(unsigned int)t5->size, force_update);
	} else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%p, %d\n",
			__func__, __LINE__, t5->cmd, t5->size);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}



enum LCM_STATUS lcm_util_set_write_cmd_v2(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T3 *t3,
	unsigned char force_update)
{
	/* check parameter is valid */
	if (_lcm_util_check_write_cmd_v2(t3) == LCM_STATUS_OK) {
		if (t3->cmd == LCM_UTIL_WRITE_CMD_V2_NULL) {
			lcm_util->dsi_set_null((unsigned char)t3->cmd,
				(unsigned char)t3->size,
				(unsigned char *)t3->data, force_update);
		} else {
			lcm_util->dsi_set_cmdq_V2((unsigned char)t3->cmd,
				(unsigned char)t3->size,
				(unsigned char *)t3->data, force_update);
		}
	} else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%x, %d, 0x%p\n",
			__func__, __LINE__, t3->cmd, t3->size, t3->data);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}


enum LCM_STATUS lcm_util_set_write_cmd_v23(
	const struct LCM_UTIL_FUNCS *lcm_util, void *handle,
	struct LCM_DATA_T3 *t3, unsigned char force_update)
{
	/* check parameter is valid */
	if (_lcm_util_check_write_cmd_v23(t3) == LCM_STATUS_OK)
		lcm_util->dsi_set_cmdq_V23(handle, (unsigned char)t3->cmd,
			(unsigned char)t3->size,
			(unsigned char *)t3->data, force_update);
	else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%x, %d, 0x%p\n",
			__func__, __LINE__, t3->cmd, t3->size, t3->data);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}


enum LCM_STATUS lcm_util_set_read_cmd_v2(
	const struct LCM_UTIL_FUNCS *lcm_util, struct LCM_DATA_T4 *t4,
	unsigned int *compare)
{
	if (compare == NULL) {
		pr_debug("[LCM][ERROR] %s/%d: NULL parameter\n",
			__func__, __LINE__);
		return LCM_STATUS_ERROR;
	}

	*compare = 0;

	/* check parameter is valid */
	if (_lcm_util_check_read_cmd_v2(t4) == LCM_STATUS_OK) {
		unsigned char buffer[4];

		lcm_util->dsi_dcs_read_lcm_reg_v2(
			(unsigned char)t4->cmd, buffer, 4);

		if (buffer[(unsigned int)t4->location] ==
			((unsigned char)t4->data))
			*compare = 1;
	} else {
		pr_debug("[LCM][ERROR] %s/%d: 0x%x, %d, 0x%x\n",
			__func__, __LINE__, t4->cmd, t4->location, t4->data);
		return LCM_STATUS_ERROR;
	}

	return LCM_STATUS_OK;
}
#endif
