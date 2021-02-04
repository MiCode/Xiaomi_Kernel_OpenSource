/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 */
#include "ilitek.h"

/* Debug level */
bool ipio_debug_level = DEBUG_OUTPUT;
EXPORT_SYMBOL(ipio_debug_level);

static struct workqueue_struct *esd_wq;
static struct workqueue_struct *bat_wq;
static struct delayed_work esd_work;
static struct delayed_work bat_work;
struct work_struct resume_work;
bool is_lcd_resume;
int ilitek_tddi_mp_test_handler(char *apk, bool lcm_on)
{
	int ret = 0;
	u8 tp_mode = P5_X_FW_TEST_MODE;

	if (atomic_read(&idev->fw_stat)) {
		ipio_err("fw upgrade processing, ignore\n");
		return 0;
	}

	if (!idev->chip->open_c_formula ||
		!idev->chip->open_sp_formula) {
		ipio_err("formula is null\n");
		return -EINVAL;
	}

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	mutex_lock(&idev->touch_mutex);
	atomic_set(&idev->mp_stat, ENABLE);

	if (idev->actual_tp_mode != P5_X_FW_TEST_MODE) {
		if (ilitek_tddi_switch_mode(&tp_mode) < 0)
			goto out;
	}

	ret = ilitek_tddi_mp_test_main(apk, lcm_on);

out:
	/*
	 * If there's running mp test with lcm off, we suspose that
	 * users will soon call resume from suspend. TP mode will be changed
	 * from MP to AP mode until resume finished.
	 */
	if (!lcm_on) {
		atomic_set(&idev->mp_stat, DISABLE);
		mutex_unlock(&idev->touch_mutex);
		return ret;
	}

	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
	if (idev->fw_upgrade_mode == UPGRADE_IRAM)
		ilitek_tddi_fw_upgrade_handler(NULL);
	else
		ilitek_tddi_reset_ctrl(idev->reset);

	atomic_set(&idev->mp_stat, DISABLE);
	mutex_unlock(&idev->touch_mutex);
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	return ret;
}

int ilitek_tddi_switch_mode(u8 *data)
{
	int ret = 0, mode;
	u8 cmd[4] = {0};

	if (!data) {
		ipio_err("data is null\n");
		return -EINVAL;
	}

	atomic_set(&idev->tp_sw_mode, START);

	mode = data[0];
	idev->actual_tp_mode = mode;

	switch (idev->actual_tp_mode) {
	case P5_X_FW_DEMO_MODE:
		ipio_info("Switch to Demo mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = idev->write(cmd, 2);
		if (ret < 0) {
			ipio_err("Failed to switch demo mode, do reset/reload instead\n");
			if (idev->fw_upgrade_mode == UPGRADE_IRAM) {
				if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
					ipio_err("FW upgrade failed\n");
				break;
			}
			ret = ilitek_tddi_reset_ctrl(idev->reset);
			if (ret < 0)
				ipio_err("TP Reset failed\n");
		}
		break;
	case P5_X_FW_DEBUG_MODE:
		ipio_info("Switch to Debug mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = idev->write(cmd, 2);
		if (ret < 0)
			ipio_err("Failed to switch Debug mode\n");
		break;
	case P5_X_FW_GESTURE_MODE:
		ipio_info("Switch to Gesture mode, lpwg cmd = %d\n",  idev->gesture_mode);
		ret = ilitek_tddi_ic_func_ctrl("lpwg", idev->gesture_mode);
		break;
	case P5_X_FW_TEST_MODE:
		ipio_info("Switch to Test mode\n");
		ret = idev->mp_move_code();
		break;
	case P5_X_FW_DEMO_DEBUG_INFO_MODE:
		ipio_info("Switch to demo debug info mode\n");
		cmd[0] = P5_X_MODE_CONTROL;
		cmd[1] = mode;
		ret = idev->write(cmd, 2);
		if (ret < 0)
			ipio_err("Failed to switch debug info mode\n");
		break;
	case P5_X_FW_SOP_FLOW_MODE:
		ipio_info("Not implemented SOP flow mode yet\n");
		break;
	case P5_X_FW_ESD_MODE:
		ipio_info("Not implemented ESD mode yet\n");
		break;
	default:
		ipio_err("Unknown TP mode: %x\n", mode);
		ret = -1;
		break;
	}

	if (ret < 0)
		ipio_err("Switch mode failed\n");

	ipio_debug("Actual TP mode = %d\n", idev->actual_tp_mode);
	atomic_set(&idev->tp_sw_mode, END);
	return ret;
}

void ilitek_tddi_gesture_recovery(void)
{
	bool lock = mutex_is_locked(&idev->touch_mutex);

	atomic_set(&idev->esd_stat, START);

	if (!lock)
		mutex_lock(&idev->touch_mutex);

	ipio_info("Doing gesture recovery\n");
	idev->ges_recover();

	if (!lock)
		mutex_unlock(&idev->touch_mutex);

	atomic_set(&idev->esd_stat, END);
}

void ilitek_tddi_spi_recovery(void)
{
	bool lock = mutex_is_locked(&idev->touch_mutex);

	atomic_set(&idev->esd_stat, START);

	if (!lock)
		mutex_lock(&idev->touch_mutex);

	ipio_info("Doing spi recovery\n");
	if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
		ipio_err("FW upgrade failed\n");

	if (!lock)
		mutex_unlock(&idev->touch_mutex);

	atomic_set(&idev->esd_stat, END);
}

int ilitek_tddi_wq_esd_spi_check(void)
{
	u8 tx = SPI_WRITE, rx = 0;

	idev->spi_write_then_read(idev->spi, &tx, 1, &rx, 1);
	ipio_debug("spi esd check = 0x%x\n", rx);
	if (rx != SPI_ACK) {
		ipio_err("rx = 0x%x\n", rx);
		return -EINVAL;
	}
	return 0;
}

int ilitek_tddi_wq_esd_i2c_check(void)
{
	ipio_debug("");
	return 0;
}

static void ilitek_tddi_wq_esd_check(struct work_struct *work)
{
	if (idev->esd_recover() < 0) {
		ipio_err("SPI ACK failed, doing spi recovery\n");
		ilitek_tddi_spi_recovery();
		return;
	}
	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
}

static int read_power_status(u8 *buf)
{
	struct file *f = NULL;
	mm_segment_t old_fs;
	ssize_t byte = 0;
	struct filename *vts_name;

	old_fs = get_fs();
	set_fs(get_ds());

	vts_name = getname_kernel(POWER_STATUS_PATH);
	f = file_open_name(vts_name, O_RDONLY, 0);
	if (ERR_ALLOC_MEM(f)) {
		ipio_err("Failed to open %s\n", POWER_STATUS_PATH);
		return -EINVAL;
	}

	f->f_op->llseek(f, 0, SEEK_SET);
	byte = f->f_op->read(f, buf, 20, &f->f_pos);

	ipio_debug("Read %d bytes\n", (int)byte);

	set_fs(old_fs);
	filp_close(f, NULL);
	return 0;
}

static void ilitek_tddi_wq_bat_check(struct work_struct *work)
{
	u8 str[20] = {0};
	static int charge_mode;

	if (read_power_status(str) < 0)
		ipio_err("Read power status failed\n");

	ipio_debug("Batter Status: %s\n", str);

	if (strnstr(str, "Charging", 0) != NULL || strnstr(str, "Full", 0) != NULL
		|| strnstr(str, "Fully charged", 0) != NULL) {
		if (charge_mode != 1) {
			ipio_debug("Charging mode\n");
			if (ilitek_tddi_ic_func_ctrl("plug", DISABLE) < 0) // plug in
				ipio_err("Write plug in failed\n");
			charge_mode = 1;
		}
	} else {
		if (charge_mode != 2) {
			ipio_debug("Not charging mode\n");
			if (ilitek_tddi_ic_func_ctrl("plug", ENABLE) < 0) // plug out
				ipio_err("Write plug out failed\n");
			charge_mode = 2;
		}
	}
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
}

void ilitek_tddi_wq_ctrl(int type, int ctrl)
{
	switch (type) {
	case WQ_ESD:
		if (ENABLE_WQ_ESD) {
			if (!esd_wq) {
				ipio_err("wq esd is null\n");
				break;
			}
			idev->wq_esd_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ipio_debug("execute esd check\n");
				if (!queue_delayed_work(esd_wq, &esd_work, msecs_to_jiffies(WQ_ESD_DELAY)))
					ipio_debug("esd check was already on queue\n");
			} else {
				cancel_delayed_work_sync(&esd_work);
				flush_workqueue(esd_wq);
				ipio_debug("cancel esd wq\n");
			}
		}
		break;
	case WQ_BAT:
		if (ENABLE_WQ_BAT) {
			if (!bat_wq) {
				ipio_err("WQ BAT is null\n");
				break;
			}
			idev->wq_bat_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ipio_debug("execute bat check\n");
				if (!queue_delayed_work(bat_wq, &bat_work, msecs_to_jiffies(WQ_BAT_DELAY)))
					ipio_debug("bat check was already on queue\n");
			} else {
				cancel_delayed_work_sync(&bat_work);
				flush_workqueue(bat_wq);
				ipio_debug("cancel bat wq\n");
			}
		}
		break;
	default:
		ipio_err("Unknown WQ type, %d\n", type);
		break;
	}
}

static void ilitek_tddi_wq_init(void)
{
	esd_wq = alloc_workqueue("esd_check", WQ_MEM_RECLAIM, 0);
	bat_wq = alloc_workqueue("bat_check", WQ_MEM_RECLAIM, 0);

	WARN_ON(!esd_wq);
	WARN_ON(!bat_wq);

	INIT_DELAYED_WORK(&esd_work, ilitek_tddi_wq_esd_check);
	INIT_DELAYED_WORK(&bat_work, ilitek_tddi_wq_bat_check);
}

int ilitek_tddi_sleep_handler(int mode)
{
	int ret = 0;
	ilitek_plat_irq_disable();
	mutex_lock(&idev->touch_mutex);
	atomic_set(&idev->tp_sleep, START);

	if (atomic_read(&idev->fw_stat) ||
		atomic_read(&idev->mp_stat)) {
		ipio_info("fw upgrade or mp still running, ignore sleep requst\n");
		atomic_set(&idev->tp_sleep, END);
		mutex_unlock(&idev->touch_mutex);
		return 0;
	}

	ipio_debug("Sleep Mode = %d\n", mode);
	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	switch (mode) {
	case TP_SUSPEND:
		ipio_info("TP normal suspend start\n");
		if (ilitek_tddi_ic_func_ctrl("sense", DISABLE) < 0)
			ipio_err("Write sense stop cmd failed\n");
		if (ilitek_tddi_ic_check_busy(50, 50) < 0)
			ipio_err("Check busy timeout during suspend\n");

		if (idev->gesture) {
			if (idev->actual_tp_mode == P5_X_FW_DEBUG_MODE)
				idev->gesture_debug = true;
			if (idev->gesture_move_code(idev->gesture_mode) < 0)
				ipio_err("Move gesture code failed\n");
			enable_irq_wake(idev->irq_num);
			ilitek_plat_irq_enable();
		} else {
			if (ilitek_tddi_ic_func_ctrl("sleep", DEEP_SLEEP_IN) < 0)
				ipio_err("Write sleep in cmd failed\n");
		}
		ipio_info("TP normal suspend end\n");
		break;
	case TP_DEEP_SLEEP:
		ipio_info("TP deep suspend start\n");
		ilitek_tddi_ic_func_ctrl("sense", DISABLE);
		ilitek_tddi_ic_check_busy(50, 50);
		ilitek_tddi_ic_func_ctrl("sleep", DEEP_SLEEP_IN);
		ipio_info("TP deep suspend end\n");
		break;
	case TP_RESUME:
		ipio_info("TP resume start\n");
		if (idev->gesture) {
			if (!idev->irq_num) {
				ipio_err("gpio_to_irq (%d) is incorrect\n", idev->irq_num);
			} else {
				disable_irq_wake(idev->irq_num);
			}
		}
		/* Set tp as demo mode and reload code if it's iram. */
		idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
		if (idev->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
				ipio_err("FW upgrade failed during resume\n");
		} else {
			if (ilitek_tddi_reset_ctrl(idev->reset) < 0)
				ipio_err("TP Reset failed during resume\n");
		}
		ilitek_plat_irq_enable();
		ipio_info("TP resume end\n");
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
		break;
	default:
		ipio_err("Unknown sleep mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	ilitek_tddi_touch_release_all_point();
	atomic_set(&idev->tp_sleep, END);
	mutex_unlock(&idev->touch_mutex);
	return ret;
}

void ilitek_resume_work_handler(struct work_struct *p_work)
{
    ipio_debug("LCD download code start \n");

	mutex_lock(&idev->touch_mutex);

	/* Set tp as demo mode and reload code if it's iram. */
	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
	if (idev->fw_upgrade_mode == UPGRADE_IRAM)
		ilitek_tddi_fw_upgrade_handler(NULL);
	else
		ilitek_tddi_reset_ctrl(idev->reset);

	mutex_unlock(&idev->touch_mutex);

	ipio_debug("LCD download code end\n");
}


int ilitek_tddi_fw_upgrade_handler(void *data)
{
	int ret = 0;
	bool get_lock = false;

	atomic_set(&idev->fw_stat, START);

	if (!atomic_read(&idev->tp_sleep) &&
		!atomic_read(&idev->mp_stat) &&
		!atomic_read(&idev->esd_stat)) {
		ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
		ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);
	}

	if (!mutex_is_locked(&idev->touch_mutex)) {
		mutex_lock(&idev->touch_mutex);
		get_lock = true;
		ipio_info("get touch lock\n");
	}

	if (idev->fw_upgrade_mode == UPGRADE_FLASH) {
		ipio_info("Get current fw/protocol ver before upgrade fw\n");
		ilitek_tddi_ic_get_protocl_ver();
		ilitek_tddi_ic_get_fw_ver();
	}

	idev->fw_update_stat = 0;
	ret = ilitek_tddi_fw_upgrade(idev->fw_upgrade_mode, HEX_FILE, idev->fw_open);
	if (ret != 0)
		idev->fw_update_stat = -1;
	else
		idev->fw_update_stat = 100;

	ipio_info("Flash FW completed ... update TP/FW info\n");
	ilitek_tddi_ic_get_protocl_ver();
	ilitek_tddi_ic_get_fw_ver();
	ilitek_tddi_ic_get_core_ver();
	ilitek_tddi_ic_get_tp_info();
	ilitek_tddi_ic_get_panel_info();
	if (atomic_read(&idev->input_stat)) {
		ilitek_plat_input_register();
		atomic_set(&idev->input_stat, DISABLE);
	}
	device_init_wakeup(&idev->input->dev, 1);

	if (!atomic_read(&idev->tp_sleep) &&
		!atomic_read(&idev->mp_stat) &&
		!atomic_read(&idev->esd_stat)) {
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	}

	atomic_set(&idev->fw_stat, END);

	if (get_lock)
		mutex_unlock(&idev->touch_mutex);
	return ret;
}

void ilitek_tddi_report_handler(void)
{
	int ret = 0, pid = 0;
	u8 *buf = NULL, checksum = 0;
	int rlen = 0, buf_size = 0;
	u16 self_key = 2;
	int tmp = ipio_debug_level;

	/* Just in case these stats couldn't be blocked in top half context */
	if (!idev->report || atomic_read(&idev->tp_reset) ||
		atomic_read(&idev->fw_stat) || atomic_read(&idev->tp_sw_mode) ||
		atomic_read(&idev->mp_stat) || atomic_read(&idev->tp_sleep)) {
		ipio_info("ignore report request\n");
		return;
	}

	if (idev->irq_after_recovery) {
		ipio_info("ignore int triggered by recovery\n");
		idev->irq_after_recovery = false;
		return;
	}

	switch (idev->actual_tp_mode) {
	case P5_X_FW_DEMO_MODE:
		rlen = P5_X_DEMO_MODE_PACKET_LENGTH;
		break;
	case P5_X_FW_DEBUG_MODE:
		rlen = (2 * idev->xch_num * idev->ych_num) + (idev->stx * 2) + (idev->srx * 2);
		rlen += 2 * self_key + (8 * 2) + 1 + 35;
		break;
	case P5_X_FW_GESTURE_MODE:
		ipio_info("black gesture process wake lock\n");
		__pm_stay_awake(idev->gesture_process_ws);
		mdelay(40);
		if (idev->gesture_debug == true)
			rlen = (2 * idev->xch_num * idev->ych_num) + (idev->stx * 2) + (idev->srx * 2) + 2 * self_key + (8 * 2) + 1 + 35;
		else if (idev->gesture_mode == P5_X_FW_GESTURE_INFO_MODE)
			rlen = P5_X_GESTURE_INFO_LENGTH;
		else
			rlen = P5_X_GESTURE_NORMAL_LENGTH;
		break;
	case P5_X_FW_DEMO_DEBUG_INFO_MODE:
		/*only suport SPI interface now, so defult use size 1024 buffer*/
		rlen = 1024;
		break;
	default:
		ipio_err("Unknown fw mode, %d\n", idev->actual_tp_mode);
		rlen = 0;
		break;
	}

	ipio_debug("Packget length = %d\n", rlen);
	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	if (!rlen) {
		ipio_err("Length of packet is invaild\n");
		goto out;
	}

	buf_size = (idev->fw_uart_en == DISABLE) ? rlen : 2048;

	buf = kcalloc(buf_size, sizeof(u8), GFP_ATOMIC);
	if (ERR_ALLOC_MEM(buf)) {
		ipio_err("Failed to allocate packet memory, %ld\n", PTR_ERR(buf));
		return;
	}

	ret = idev->read(buf, rlen);
	if (ret < 0) {
		ipio_err("Read report packet failed, ret = %d\n", ret);
		if (idev->actual_tp_mode == P5_X_FW_GESTURE_MODE && idev->gesture) {
			ipio_err("Gesture failed, doing gesture recovery\n");
			ilitek_tddi_gesture_recovery();
			idev->irq_after_recovery = true;
		} else if (ret == DO_SPI_RECOVER) {
			ipio_err("SPI ACK failed, doing spi recovery\n");
			ilitek_tddi_spi_recovery();
			idev->irq_after_recovery = true;
		}
		goto out;
	}

	ipio_debug("Read length = %d\n", (ret));
	if (ret != rlen) {
		ipio_info("Read ret length = %d, Packget length = %d\n", (ret), rlen);
		ret = rlen;
	}

	rlen = ret;
	if (rlen > 2048) {
		goto recover;
	}

	ilitek_dump_data(buf, 8, rlen, 0, "finger report");

	checksum = ilitek_calc_packet_checksum(buf, rlen - 1);

	if (checksum != buf[rlen-1] && idev->fw_uart_en == DISABLE) {
		ipio_err("Wrong checksum, checksum = %x, buf = %x, len = %d\n", checksum, buf[rlen-1], rlen);
		ipio_debug_level = DEBUG_ALL;
		ilitek_dump_data(buf, 8, rlen, 0, "finger report with wrong");
		ipio_debug_level = tmp;
		goto out;
	}

	pid = buf[0];
	ipio_debug("Packet ID = %x\n", pid);

	switch (pid) {
	case P5_X_LARGE_AREA_PRESS_PACKET_ID:
		ipio_info(" Demo LARGE_AREA PRESS_PACKET_ID \n");
#if 0
		input_report_key(idev->input, 523, 1);
		input_sync(idev->input);
		input_report_key(idev->input, 523, 0);
		input_sync(idev->input);
#endif
		break;
	case P5_X_LARGE_AREA_RELEASE_PACKET_ID:
		ipio_info(" Demo LARGE_AREA RELEASE_PACKET_ID \n");
		break;
	case P5_X_DEMO_PACKET_ID:
		ilitek_tddi_report_ap_mode(buf, rlen);
		break;
	case P5_X_DEBUG_PACKET_ID:
		ilitek_tddi_report_debug_mode(buf, rlen);
		break;
	case P5_X_I2CUART_PACKET_ID:
		ilitek_tddi_report_i2cuart_mode(buf, rlen);
		break;
	case P5_X_GESTURE_PACKET_ID:
		ilitek_tddi_report_gesture_mode(buf, rlen);
		__pm_relax(idev->gesture_process_ws);
		ipio_info("black gesture process wake unlock\n");
		break;
	default:
		ipio_err("Unknown packet id, %x\n", pid);
		break;
	}

out:
	if (!(idev->actual_tp_mode == P5_X_FW_GESTURE_MODE)) {
		ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
		ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	}
recover:
	ipio_kfree((void **)&buf);
}

int ilitek_tddi_reset_ctrl(int mode)
{
	int ret = 0;

	atomic_set(&idev->tp_reset, START);

	if (mode != TP_IC_CODE_RST)
		ilitek_tddi_ic_check_otp_prog_mode();

	switch (mode) {
	case TP_IC_CODE_RST:
		ipio_info("TP IC Code RST \n");
		ret = ilitek_tddi_ic_code_reset();
		if (ret < 0)
			ipio_err("IC Code reset failed\n");
		break;
	case TP_IC_WHOLE_RST:
		ipio_info("TP IC whole RST\n");
		ret = ilitek_tddi_ic_whole_reset();
		if (ret < 0)
			ipio_err("IC whole reset failed\n");
		break;
	case TP_HW_RST_ONLY:
		ipio_info("TP HW RST\n");
		ilitek_plat_tp_reset();
		break;
	default:
		ipio_err("Unknown reset mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	/*
	 * Since OTP must be folloing with reset, except for code rest,
	 * the stat of ice mode should be set as 0.
	 */
	if (mode != TP_IC_CODE_RST)
		atomic_set(&idev->ice_stat, DISABLE);
	idev->fw_uart_en = DISABLE;
	idev->gesture_debug = false;
	atomic_set(&idev->tp_reset, END);
	return ret;
}

int ilitek_tddi_init(void)
{
	struct task_struct *fw_boot_th;

	ipio_info("ilitek tddi main init\n");

	mutex_init(&idev->touch_mutex);
	mutex_init(&idev->debug_mutex);
	mutex_init(&idev->debug_read_mutex);
	init_waitqueue_head(&(idev->inq));
	spin_lock_init(&idev->irq_spin);

	atomic_set(&idev->irq_stat, DISABLE);
	atomic_set(&idev->ice_stat, DISABLE);
	atomic_set(&idev->tp_reset, END);
	atomic_set(&idev->fw_stat, END);
	atomic_set(&idev->mp_stat, DISABLE);
	atomic_set(&idev->tp_sleep, END);
	atomic_set(&idev->mp_int_check, DISABLE);
	atomic_set(&idev->esd_stat, END);
	atomic_set(&idev->input_stat, ENABLE);
	atomic_set(&idev->first_starting_up_reset_stat, ENABLE);


	ilitek_tddi_ic_init();
	ilitek_tddi_wq_init();

	/* Must do hw reset once in first time for work normally if tp reset is avaliable */
	if (!TDDI_RST_BIND)
		if (ilitek_tddi_reset_ctrl(idev->reset) < 0)
			ipio_err("TP Reset failed during init\n");

	idev->do_otp_check = ENABLE;
	idev->fw_uart_en = DISABLE;
	idev->force_fw_update = DISABLE;

	if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
		ipio_err("Enable ice mode failed during init\n");

	if (ilitek_tddi_ic_get_info() < 0) {
		ipio_err("Not found ilitek chips\n");
		return -ENODEV;
	}
	ilitek_tddi_node_init();

	ilitek_ice_mode_ctrl(DISABLE, OFF);

	ilitek_tddi_fw_read_flash_info(idev->fw_upgrade_mode);

	fw_boot_th = kthread_run(ilitek_tddi_fw_upgrade_handler, NULL, "ili_fw_boot");
	if (fw_boot_th == (struct task_struct *)ERR_PTR) {
		fw_boot_th = NULL;
		WARN_ON(!fw_boot_th);
		ipio_err("Failed to create fw upgrade thread\n");
	}

	INIT_WORK(&resume_work, ilitek_resume_work_handler);
	return 0;
}

void ilitek_call_resume_work(void)
{
	int ret = 0;
	ipio_debug("resume schedule work start ... \n");

	mutex_lock(&idev->touch_mutex);

	ilitek_tddi_wq_ctrl(WQ_ESD, DISABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, DISABLE);

	ilitek_tddi_reset_ctrl(idev->reset);
	ret = ilitek_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0) {
		is_lcd_resume = false;
	} else {
		is_lcd_resume = true;
	}
	mutex_unlock(&idev->touch_mutex);
	ret = schedule_work(&resume_work);
	if (ret < 0) {
		ipio_err("start touch_resume_workqueue failed\n");
	}
	ilitek_tddi_touch_release_all_point();
	ilitek_plat_irq_enable();

	ilitek_tddi_wq_ctrl(WQ_ESD, ENABLE);
	ilitek_tddi_wq_ctrl(WQ_BAT, ENABLE);
	ipio_debug("resume schedule work end ... \n");
}

void ilitek_tddi_dev_remove(void)
{
	ipio_info("remove ilitek dev\n");

	if (!idev)
		return;

	gpio_free(idev->tp_int);
	gpio_free(idev->tp_rst);

	if (esd_wq != NULL) {
		cancel_delayed_work_sync(&esd_work);
		flush_workqueue(esd_wq);
		destroy_workqueue(esd_wq);
	}
	if (bat_wq != NULL) {
		cancel_delayed_work_sync(&bat_work);
		flush_workqueue(bat_wq);
		destroy_workqueue(bat_wq);
	}

	ilitek_tddi_interface_dev_exit(idev->hwif);
}

int ilitek_tddi_dev_init(struct ilitek_hwif_info *hwif)
{
	ipio_info("TP Interface: %s\n", (hwif->bus_type == BUS_I2C) ? "I2C" : "SPI");
	return ilitek_tddi_interface_dev_init(hwif);
}
