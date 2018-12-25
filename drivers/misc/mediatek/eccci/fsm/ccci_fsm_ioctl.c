/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mt-plat/mtk_battery.h>
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
#include <mt-plat/env.h>
#endif
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
#include <mt-plat/env.h>
#endif
#include "ccci_fsm_internal.h"

signed int __weak battery_get_bat_voltage(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

static int fsm_md_data_ioctl(int md_id, unsigned int cmd, unsigned long arg)
{
	int ret = 0, retry;
	int data;
	char buffer[64];
	unsigned int sim_slot_cfg[4];
	struct ccci_per_md *per_md_data = ccci_get_per_md_data(md_id);
	struct ccci_per_md *other_per_md_data
			= ccci_get_per_md_data(GET_OTHER_MD_ID(md_id));

	switch (cmd) {
	case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
#if (MD_GENERATION < 6292)
		if (copy_to_user((void __user *)arg, "DHL", sizeof("DHL"))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_GET_MD_PROTOCOL_TYPE: copy_from_user fail\n");
			return -EFAULT;
		}
#else
		snprintf(buffer, sizeof(buffer), "%d", MD_GENERATION);
		if (copy_to_user((void __user *)arg, MD_PLATFORM_INFO,
				sizeof(MD_PLATFORM_INFO))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_GET_MD_PROTOCOL_TYPE: copy_from_user fail\n");
			return -EFAULT;
		}
#endif
		break;
	case CCCI_IOC_SEND_BATTERY_INFO:
		data = (int)battery_get_bat_voltage();
		CCCI_NORMAL_LOG(md_id, FSM, "get bat voltage %d\n", data);
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_GET_BATTERY_INFO, data, 1);
		break;
	case CCCI_IOC_GET_RAT_STR:
		ret = ccci_get_rat_str_from_drv(md_id,
				(char *)buffer, sizeof(buffer));
		if (ret < 0)
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_GET_RAT_STR: gen str fail %d\n",
				(int)ret);
		else {
			if (copy_to_user((void __user *)arg, (char *)buffer,
						strlen((char *)buffer) + 1)) {
				CCCI_ERROR_LOG(md_id, FSM,
					"CCCI_IOC_GET_RAT_STR: copy_from_user fail\n");
				ret = -EFAULT;
			}
		}
		break;
	case CCCI_IOC_SET_RAT_STR:
		if (strncpy_from_user((char *)buffer,
				(void __user *)arg, sizeof(buffer))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_SET_RAT_STR: copy_from_user fail\n");
			ret = -EFAULT;
			break;
		}
		ccci_set_rat_str_to_drv(md_id, (char *)buffer);
		break;
	case CCCI_IOC_GET_EXT_MD_POST_FIX:
		if (copy_to_user((void __user *)arg,
				per_md_data->img_post_fix, IMG_POSTFIX_LEN)) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_GET_EXT_MD_POST_FIX: copy_to_user fail\n");
			ret = -EFAULT;
		}
		break;

	case CCCI_IOC_DL_TRAFFIC_CONTROL:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int)))
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_DL_TRAFFIC_CONTROL: copy_from_user fail\n");
		if (data == 1)
			;/* turn off downlink queue */
		else if (data == 0)
			;/* turn on donwlink queue */
		else
		;
		ret = 0;
		break;
#ifdef CONFIG_MTK_SIM_LOCK_POWER_ON_WRITE_PROTECT
	case CCCI_IOC_SIM_LOCK_RANDOM_PATTERN:
		if (copy_from_user(&val, (void __user *)arg,
				sizeof(unsigned int)))
			CCCI_ERROR_LOG(md_id, FSM,
			"CCCI_IOC_SIM_LOCK_RANDOM_PATTERN: copy_from_user fail\n");

		CCCI_NORMAL_LOG(md_id, FSM,
			"get SIM lock random pattern %x\n", data);

		snprintf(buffer, sizeof(buffer), "%x", data);
		set_env("sml_sync", buffer);
		break;
#endif
	case CCCI_IOC_SET_MD_BOOT_MODE:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_SET_MD_BOOT_MODE: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, FSM,
				"set MD boot mode to %d\n", data);
			per_md_data->md_boot_mode = data;
			if (other_per_md_data)
				other_per_md_data->md_boot_mode = data;
		}
		break;
	case CCCI_IOC_GET_MD_BOOT_MODE:
		ret = put_user((unsigned int)per_md_data->md_boot_mode,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_MD_INFO:
		ret = put_user(
		(unsigned int)per_md_data->img_info[IMG_MD].img_info.version,
		(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SET_BOOT_DATA:
		if (copy_from_user(&per_md_data->md_boot_data,
			(void __user *)arg,
			sizeof(per_md_data->md_boot_data))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_SET_BOOT_DATA: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			if (per_md_data->md_boot_data[MD_CFG_DUMP_FLAG]
				!= MD_DBG_DUMP_INVALID
				&&
				(per_md_data->md_boot_data[MD_CFG_DUMP_FLAG]
				& 1 << MD_DBG_DUMP_PORT)) {
				/*port traffic use 0x6000_000x
				 * as port dump flag
				 */
				ccci_port_set_traffic_flag(md_id,
				per_md_data->md_boot_data[MD_CFG_DUMP_FLAG]);
				per_md_data->md_boot_data[MD_CFG_DUMP_FLAG]
					= MD_DBG_DUMP_INVALID;
			}
			ret = ccci_md_set_boot_data(md_id,
					per_md_data->md_boot_data,
					ARRAY_SIZE(per_md_data->md_boot_data));
			if (ret < 0) {
				CCCI_ERROR_LOG(md_id, FSM,
					"ccci_set_md_boot_data return fail %d\n",
					ret);
				ret = -EFAULT;
			}
		}
		break;
	case CCCI_IOC_SIM_SWITCH:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_BOOTUP_LOG(md_id, FSM,
				"CCCI_IOC_SIM_SWITCH: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			switch_sim_mode(md_id, (char *)&data, sizeof(data));
			CCCI_BOOTUP_LOG(md_id, FSM,
				"CCCI_IOC_SIM_SWITCH(%x): %d\n", data, ret);
		}
		break;
	case CCCI_IOC_SIM_SWITCH_TYPE:
		data = get_sim_switch_type();
		CCCI_BOOTUP_LOG(md_id, FSM,
			"CCCI_IOC_SIM_SWITCH_TYPE: 0x%x\n", data);
		ret = put_user(data, (unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_SIM_TYPE:
		if (per_md_data->sim_type == 0xEEEEEEEE)
			CCCI_BOOTUP_LOG(md_id, FSM,
				"md has not send sim type yet(0x%x)",
				per_md_data->sim_type);
		else
			CCCI_BOOTUP_LOG(md_id, FSM,
				"md has send sim type(0x%x)",
				per_md_data->sim_type);
		ret = put_user(per_md_data->sim_type,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_ENABLE_GET_SIM_TYPE:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_ENABLE_GET_SIM_TYPE: 0x%x\n", data);
			ret = ccci_port_send_msg_to_md(md_id,
					CCCI_SYSTEM_TX, MD_SIM_TYPE, data, 1);
		}
		break;
	case CCCI_IOC_RELOAD_MD_TYPE:
		data = 0;
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_RELOAD_MD_TYPE: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_RELOAD_MD_TYPE: 0x%x\n", data);
			/* add md type check to
			 * avoid it being changed to illegal value
			 */
			if (check_md_type(data) > 0) {
				if (set_modem_support_cap(md_id, data) == 0)
					per_md_data->config.load_type = data;
			} else {
				CCCI_ERROR_LOG(md_id, FSM,
				"invalid MD TYPE: 0x%x\n", data);
			}
		}
		break;
	case CCCI_IOC_SET_MD_IMG_EXIST:
		if (copy_from_user(&per_md_data->md_img_exist,
				(void __user *)arg,
				sizeof(per_md_data->md_img_exist))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_SET_MD_IMG_EXIST: copy_from_user fail\n");
			ret = -EFAULT;
		}
		per_md_data->md_img_type_is_set = 1;
		CCCI_BOOTUP_LOG(md_id, FSM,
			"CCCI_IOC_SET_MD_IMG_EXIST: set done!\n");
		break;
	case CCCI_IOC_GET_MD_IMG_EXIST:
		data = get_md_type_from_lk(md_id);
		if (data) {
			memset(&per_md_data->md_img_exist, 0,
				sizeof(per_md_data->md_img_exist));
			per_md_data->md_img_exist[0] = data;
			CCCI_BOOTUP_LOG(md_id, FSM,
				"LK md_type: %d, image num:1\n", data);
		} else {
			CCCI_BOOTUP_LOG(md_id, FSM,
				"CCCI_IOC_GET_MD_IMG_EXIST: waiting set\n");
			while (per_md_data->md_img_type_is_set == 0)
				msleep(200);
		}
		CCCI_BOOTUP_LOG(md_id, FSM,
			"CCCI_IOC_GET_MD_IMG_EXIST: waiting set done!\n");
		if (copy_to_user((void __user *)arg,
			&per_md_data->md_img_exist,
			sizeof(per_md_data->md_img_exist))) {
			CCCI_ERROR_LOG(md_id, FSM,
			"CCCI_IOC_GET_MD_IMG_EXIST: copy_to_user fail!\n");
			ret = -EFAULT;
		}
		break;
	case CCCI_IOC_GET_MD_TYPE:
		retry = 600;
		do {
			data = get_legacy_md_type(md_id);
			if (data)
				break;
			msleep(500);
			retry--;
		} while (retry);
		CCCI_NORMAL_LOG(md_id, FSM,
			"CCCI_IOC_GET_MD_TYPE: %d!\n", data);
		ret = put_user((unsigned int)data,
			(unsigned int __user *)arg);
		break;
	case CCCI_IOC_STORE_MD_TYPE:
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(unsigned int))) {
			CCCI_BOOTUP_LOG(md_id, FSM,
			"CCCI_IOC_STORE_MD_TYPE: copy_from_user fail\n");
			ret = -EFAULT;
			break;
		}
		per_md_data->config.load_type_saving = data;

		CCCI_BOOTUP_LOG(md_id, FSM,
			"storing md type(%d) in kernel space!\n",
			per_md_data->config.load_type_saving);
		if (per_md_data->config.load_type_saving >= 1
			&& per_md_data->config.load_type_saving
			<= MAX_IMG_NUM) {
			if (per_md_data->config.load_type_saving
				!= per_md_data->config.load_type)
				CCCI_BOOTUP_LOG(md_id, FSM,
					"Maybe Wrong: md type storing not equal with current setting!(%d %d)\n",
					per_md_data->config.load_type_saving,
					per_md_data->config.load_type);
		} else {
			CCCI_BOOTUP_LOG(md_id, FSM,
				"store md type fail: invalid md type(0x%x)\n",
				per_md_data->config.load_type_saving);
			ret = -EFAULT;
		}
		if (ret == 0)
			fsm_monitor_send_message(md_id,
				CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
		break;
	case CCCI_IOC_GET_MD_TYPE_SAVING:
		ret = put_user(per_md_data->config.load_type_saving,
				(unsigned int __user *)arg);
		break;
	case CCCI_IOC_SEND_ICUSB_NOTIFY:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_SEND_ICUSB_NOTIFY: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			ret = ccci_port_send_msg_to_md(md_id,
			CCCI_SYSTEM_TX, MD_ICUSB_NOTIFY, data, 1);
		}
		break;
	case CCCI_IOC_UPDATE_SIM_SLOT_CFG:
		if (copy_from_user(&sim_slot_cfg, (void __user *)arg,
				sizeof(sim_slot_cfg))) {
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_UPDATE_SIM_SLOT_CFG: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			int need_update;

			data = get_sim_switch_type();
			CCCI_NORMAL_LOG(md_id, FSM,
				"CCCI_IOC_UPDATE_SIM_SLOT_CFG get s0:%d s1:%d s2:%d s3:%d\n",
				sim_slot_cfg[0], sim_slot_cfg[1],
				sim_slot_cfg[2], sim_slot_cfg[3]);
			need_update = sim_slot_cfg[0];
			per_md_data->sim_setting.sim_mode = sim_slot_cfg[1];
			per_md_data->sim_setting.slot1_mode = sim_slot_cfg[2];
			per_md_data->sim_setting.slot2_mode = sim_slot_cfg[3];
			data = ((data << 16)
					| per_md_data->sim_setting.sim_mode);
			switch_sim_mode(md_id, (char *)&data, sizeof(data));
			fsm_monitor_send_message(md_id,
			CCCI_MD_MSG_CFG_UPDATE, need_update);
			ret = 0;
		}
		break;
	case CCCI_IOC_STORE_SIM_MODE:
		if (copy_from_user(&data, (void __user *)arg,
			sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, FSM,
			"CCCI_IOC_STORE_SIM_MODE: copy_from_user fail\n");
			ret = -EFAULT;
			break;
		}
		CCCI_NORMAL_LOG(md_id, FSM,
		"store sim mode(%x) in kernel space!\n", data);
		if (per_md_data->sim_setting.sim_mode != data) {
			per_md_data->sim_setting.sim_mode = data;
			fsm_monitor_send_message(md_id,
			CCCI_MD_MSG_CFG_UPDATE, 1);
		} else {
			CCCI_ERROR_LOG(md_id, FSM,
			"same sim mode as last time(0x%x)\n", data);
		}
		break;
	case CCCI_IOC_GET_SIM_MODE:
		CCCI_NORMAL_LOG(md_id, FSM,
		"get sim mode ioctl called by %s\n", current->comm);
		ret = put_user(per_md_data->sim_setting.sim_mode,
		(unsigned int __user *)arg);
		break;
	case CCCI_IOC_GET_CFG_SETTING:
		if (copy_to_user((void __user *)arg,
			&per_md_data->sim_setting,
			sizeof(struct ccci_sim_setting))) {
			CCCI_NORMAL_LOG(md_id, FSM,
			"CCCI_IOC_GET_CFG_SETTING: copy_to_user fail\n");
			ret = -EFAULT;
		}
		break;
	case CCCI_IOC_GET_AT_CH_NUM:
		{
			unsigned int at_ch_num = 4; /*default value*/
			struct ccci_runtime_feature *rt_feature = NULL;

			rt_feature = ccci_md_get_rt_feature_by_id(md_id,
				AT_CHANNEL_NUM, 1);
			if (rt_feature)
				ret = ccci_md_parse_rt_feature(md_id,
				rt_feature, &at_ch_num, sizeof(at_ch_num));
			else
				CCCI_ERROR_LOG(md_id, FSM,
					"get AT_CHANNEL_NUM fail\n");

			CCCI_NORMAL_LOG(md_id, FSM,
				"get at_ch_num = %u\n", at_ch_num);
			ret = put_user(at_ch_num,
					(unsigned int __user *)arg);
			break;
		}

	default:
		ret = -ENOTTY;
		break;
	}
	return ret;
}

long ccci_fsm_ioctl(int md_id, unsigned int cmd, unsigned long arg)
{
	struct ccci_fsm_ctl *ctl = fsm_get_entity_by_md_id(md_id);
	int ret = 0;
	enum MD_STATE_FOR_USER state_for_user;
	struct siginfo sig_info;
	unsigned int data, sig, pid;

	if (!ctl)
		return -EINVAL;

	switch (cmd) {
	case CCCI_IOC_GET_MD_STATE:
		state_for_user = ccci_fsm_get_md_state_for_user(md_id);
		if (state_for_user >= 0) {
			ret = put_user((unsigned int)state_for_user,
					(unsigned int __user *)arg);
		} else {
			CCCI_ERROR_LOG(md_id, FSM,
				"Get MD state fail: %d\n", state_for_user);
			ret = state_for_user;
		}
		break;
	case CCCI_IOC_GET_OTHER_MD_STATE:
		state_for_user =
		ccci_fsm_get_md_state_for_user(GET_OTHER_MD_ID(md_id));
		if (state_for_user >= 0) {
			ret = put_user((unsigned int)state_for_user,
					(unsigned int __user *)arg);
		} else {
			CCCI_ERROR_LOG(md_id, FSM,
				"Get other MD state fail: %d\n",
				state_for_user);
			ret = state_for_user;
		}
		break;
	case CCCI_IOC_MD_RESET:
		CCCI_NORMAL_LOG(md_id, FSM,
			"MD reset ioctl called by %s\n", current->comm);
		ret = fsm_monitor_send_message(ctl->md_id,
			CCCI_MD_MSG_RESET_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_RESET_REQUEST, 0);
		break;
	case CCCI_IOC_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(md_id, FSM,
			"MD force assert ioctl called by %s\n", current->comm);
		ret = ccci_md_force_assert(md_id,
			MD_FORCE_ASSERT_BY_USER_TRIGGER, NULL, 0);
		break;
	case CCCI_IOC_SEND_STOP_MD_REQUEST:
		CCCI_NORMAL_LOG(md_id, FSM,
			"MD stop request ioctl called by %s\n", current->comm);
		ret = fsm_monitor_send_message(ctl->md_id,
			CCCI_MD_MSG_FORCE_STOP_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_FORCE_STOP_REQUEST, 0);
		break;
	case CCCI_IOC_SEND_START_MD_REQUEST:
		CCCI_NORMAL_LOG(md_id, FSM,
			"MD start request ioctl called by %s\n", current->comm);
		ret = fsm_monitor_send_message(ctl->md_id,
			CCCI_MD_MSG_FORCE_START_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_FORCE_START_REQUEST, 0);
		break;
	case CCCI_IOC_DO_START_MD:
		CCCI_NORMAL_LOG(md_id, FSM,
			"MD start ioctl called by %s\n", current->comm);
		ret = fsm_append_command(ctl, CCCI_COMMAND_START,
			FSM_CMD_FLAG_WAIT_FOR_COMPLETE);
		break;
	case CCCI_IOC_DO_STOP_MD:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"CCCI_IOC_DO_STOP_MD: copy_from_user fail\n");
			ret = -EFAULT;
		} else {
			CCCI_NORMAL_LOG(md_id, FSM,
				"MD stop ioctl called by %s %d\n",
				current->comm, data);
			ret = fsm_append_command(ctl, CCCI_COMMAND_STOP,
					FSM_CMD_FLAG_WAIT_FOR_COMPLETE |
					((data ? MD_FLIGHT_MODE_ENTER
					: MD_FLIGHT_MODE_NONE)
					== MD_FLIGHT_MODE_ENTER ?
					FSM_CMD_FLAG_FLIGHT_MODE : 0));
		}
		break;
	case CCCI_IOC_ENTER_DEEP_FLIGHT:
		CCCI_NORMAL_LOG(md_id, FSM,
		"MD enter flight mode ioctl called by %s\n", current->comm);
		ret = fsm_monitor_send_message(ctl->md_id,
				CCCI_MD_MSG_FLIGHT_STOP_REQUEST, 0);
		break;
	case CCCI_IOC_LEAVE_DEEP_FLIGHT:
		CCCI_NORMAL_LOG(md_id, FSM,
		"MD leave flight mode ioctl called by %s\n", current->comm);
		__pm_wakeup_event(&ctl->wakelock, jiffies_to_msecs(10 * HZ));
		ret = fsm_monitor_send_message(ctl->md_id,
				CCCI_MD_MSG_FLIGHT_START_REQUEST, 0);
		break;
	case CCCI_IOC_ENTER_DEEP_FLIGHT_ENHANCED:
		CCCI_NORMAL_LOG(md_id, FSM,
		"MD enter flight mode enhanced ioctl called by %s\n",
		current->comm);
		ret = fsm_monitor_send_message(ctl->md_id,
				CCCI_MD_MSG_FLIGHT_STOP_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_FLIGHT_STOP_REQUEST, 0);
		break;
	case CCCI_IOC_LEAVE_DEEP_FLIGHT_ENHANCED:
		CCCI_NORMAL_LOG(md_id, FSM,
		"MD leave flight mode enhanced ioctl called by %s\n",
		current->comm);
		__pm_wakeup_event(&ctl->wakelock, jiffies_to_msecs(10 * HZ));
		ret = fsm_monitor_send_message(ctl->md_id,
				CCCI_MD_MSG_FLIGHT_START_REQUEST, 0);
		fsm_monitor_send_message(GET_OTHER_MD_ID(ctl->md_id),
			CCCI_MD_MSG_FLIGHT_START_REQUEST, 0);
		break;
	case CCCI_IOC_SET_EFUN:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_ERROR_LOG(md_id, FSM,
				"set efun fail: copy_from_user fail\n");
			ret = -EFAULT;
			break;
		}
		CCCI_NORMAL_LOG(md_id, FSM, "EFUN set to %d\n", data);
		if (data == 0)
			ccci_md_soft_stop(md_id, data);
		else if (data != 0)
			ccci_md_soft_start(md_id, data);
		break;
	case CCCI_IOC_MDLOG_DUMP_DONE:
		CCCI_NORMAL_LOG(md_id, FSM,
		"MD logger dump done ioctl called by %s\n", current->comm);
		ctl->ee_ctl.mdlog_dump_done = 1;
		break;
	case CCCI_IOC_RESET_MD1_MD3_PCCIF:
		ccci_md_reset_pccif(md_id);
		break;
	case CCCI_IOC_SEND_SIGNAL_TO_USER:
		if (copy_from_user(&data, (void __user *)arg,
				sizeof(unsigned int))) {
			CCCI_NORMAL_LOG(md_id, FSM,
				"signal to RILD fail: copy_from_user fail\n");
			ret = -EFAULT;
			break;
		}
		sig = (data >> 16) & 0xFFFF;
		pid = data & 0xFFFF;
		sig_info.si_signo = sig;
		sig_info.si_code = SI_KERNEL;
		sig_info.si_pid = current->pid;
		sig_info.si_uid = __kuid_val(current->cred->uid);
		ret = kill_proc_info(SIGUSR2, &sig_info, pid);
		CCCI_NORMAL_LOG(md_id, FSM,
			"send signal %d to rild %d ret=%d\n", sig, pid, ret);
		break;
	case CCCI_IOC_GET_MD_EX_TYPE:
		ret = put_user((unsigned int)ctl->ee_ctl.ex_type,
				(unsigned int __user *)arg);
		CCCI_NORMAL_LOG(md_id, FSM,
			"get modem exception type=%d ret=%d\n",
			ctl->ee_ctl.ex_type, ret);
		break;
	default:
		ret = fsm_md_data_ioctl(md_id, cmd, arg);
		break;
	}
	return ret;
}

