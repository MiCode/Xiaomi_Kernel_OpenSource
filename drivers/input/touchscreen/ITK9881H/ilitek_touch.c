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

void ilitek_dump_data(void *data, int type, int len, int row_len, const char *name)
{
	int i, row = 31;
	u8 *p8 = NULL;
	s32 *p32 = NULL;

	if (!ipio_debug_level)
		return;

	if (row_len > 0)
		row = row_len;

	if (data == NULL) {
		ipio_err("The data going to dump is NULL\n");
		return;
	}

	pr_cont("\n\n");
	pr_cont("ILITEK: Dump %s data\n", name);
	pr_cont("ILITEK: ");

	if (type == 8)
		p8 = (u8 *) data;
	if (type == 32 || type == 10)
		p32 = (s32 *) data;

	for (i = 0; i < len; i++) {
		if (type == 8)
			pr_cont(" %4x ", p8[i]);
		else if (type == 32)
			pr_cont(" %4x ", p32[i]);
		else if (type == 10)
			pr_cont(" %4d ", p32[i]);
		if ((i % row) == row - 1) {
			pr_cont("\n");
			pr_cont("ILITEK: ");
		}
	}
	pr_cont("\n\n");
}

static void dma_clear_reg_setting(void)
{
	ipio_info("[Clear register setting]\n");

	ipio_info("interrupt t0/t1 enable flag\n");
	if (ilitek_ice_mode_bit_mask_write(INTR32_ADDR, INTR32_reg_t0_int_en, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR32_reg_t0_int_en, INTR32_ADDR);
	if (ilitek_ice_mode_bit_mask_write(INTR32_ADDR, INTR32_reg_t1_int_en, (0 << 25)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR32_reg_t1_int_en, INTR32_ADDR);

	ipio_info("clear tdi_err_int_flag\n");
	if (ilitek_ice_mode_bit_mask_write(INTR2_ADDR, INTR2_tdi_err_int_flag_clear, (1 << 18)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR2_tdi_err_int_flag_clear, INTR2_ADDR);

	ipio_info("clear dma channel 0 src1 info\n");
	if (ilitek_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00000000, 4) < 0)
		ipio_err("Write 0x00000000 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ilitek_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ipio_err("Write 0x0 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ilitek_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format, DMA50_ADDR);
	if (ilitek_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_en, (1 << 31)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	ipio_info("clear dma channel 0 src2 info\n");
	if (ilitek_ice_mode_bit_mask_write(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA52_reg_dma_ch0_src2_en, DMA52_ADDR);

	ipio_info("clear dma channel 0 trafer info\n");
	if (ilitek_ice_mode_write(DMA55_reg_dma_ch0_trafer_counts, 0x00000000, 4) < 0)
		ipio_err("Write 0x00000000 at %x failed\n", DMA55_reg_dma_ch0_trafer_counts);
	if (ilitek_ice_mode_bit_mask_write(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA55_reg_dma_ch0_trafer_mode, DMA55_ADDR);

	ipio_info("clear dma channel 0 trigger select\n");
	if (ilitek_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (0 << 16)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);

	if (ilitek_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	ipio_info("clear dma flash setting\n");
	ilitek_tddi_flash_clear_dma();
}

static void dma_trigger_reg_setting(u32 reg_dest_addr, u32 flash_start_addr, u32 copy_size)
{
	ipio_info("set dma channel 0 clear\n");
	if (ilitek_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_start_clear, (1 << 25)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA48_reg_dma_ch0_start_clear, DMA48_ADDR);

	ipio_info("set dma channel 0 src1 info\n");
	if (ilitek_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00041010, 4) < 0)
		ipio_err("Write 0x00041010 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ilitek_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ipio_err("Write 0x00 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ilitek_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format, DMA50_ADDR);
	if (ilitek_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_en, (1 << 31)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	ipio_info("set dma channel 0 src2 info\n");
	if (ilitek_ice_mode_bit_mask_write(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA52_reg_dma_ch0_src2_en, DMA52_ADDR);

	ipio_info("set dma channel 0 dest info\n");
	if (ilitek_ice_mode_write(DMA53_reg_dma_ch0_dest_addr, reg_dest_addr, 3) < 0)
		ipio_err("Write %x at %x failed\n", reg_dest_addr, DMA53_reg_dma_ch0_dest_addr);
	if (ilitek_ice_mode_write(DMA54_reg_dma_ch0_dest_step_inc, 0x01, 1) < 0)
		ipio_err("Write 0x01 at %x failed\n", DMA54_reg_dma_ch0_dest_step_inc);
	if (ilitek_ice_mode_bit_mask_write(DMA54_ADDR, DMA54_reg_dma_ch0_dest_format, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA54_reg_dma_ch0_dest_format, DMA54_ADDR);
	if (ilitek_ice_mode_bit_mask_write(DMA54_ADDR, DMA54_reg_dma_ch0_dest_en, (1 << 31)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA54_reg_dma_ch0_dest_en, DMA54_ADDR);

	ipio_info("set dma channel 0 trafer info\n");
	if (ilitek_ice_mode_write(DMA55_reg_dma_ch0_trafer_counts, copy_size, 4) < 0)
		ipio_err("Write %x at %x failed\n", copy_size, DMA55_reg_dma_ch0_trafer_counts);
	if (ilitek_ice_mode_bit_mask_write(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA55_reg_dma_ch0_trafer_mode, DMA55_ADDR);

	ipio_info("set dma channel 0 int info\n");
	if (ilitek_ice_mode_bit_mask_write(INTR33_ADDR, INTR33_reg_dma_ch0_int_en, (1 << 17)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR33_reg_dma_ch0_int_en, INTR33_ADDR);

	ipio_info("set dma channel 0 trigger select\n");
	if (ilitek_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (1 << 16)) < 0)
		ipio_err("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);

	ipio_info("set dma flash setting, FlashAddr = 0x%x\n", flash_start_addr);
	ilitek_tddi_flash_dma_write(flash_start_addr, (flash_start_addr+copy_size), copy_size);

	ipio_info("clear flash and dma ch0 int flag\n");
	if (ilitek_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);
	if (ilitek_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_dma_ch0_int_flag, (1 << 17)) < 0)
		ipio_err("Write %lu at %x failed\n", INTR1_reg_dma_ch0_int_flag, INTR1_ADDR);
	if (ilitek_ice_mode_bit_mask_write(0x041013, BIT(0), 1) < 0) //patch
		ipio_err("Write %lu at %x failed\n", BIT(0), 0x041013);

	/* DMA Trigger */
	if (ilitek_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ipio_err("Trigger DMA failed\n");
	/* waiting for fw reload code completed. */
	mdelay(30);

	/* CS High */
	if (ilitek_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
		ipio_err("Pull CS High failed\n");
	/* waiting for CS status done */
	mdelay(10);
}

int ilitek_tddi_move_mp_code_flash(void)
{
	int ret = 0;
	u32 mp_text_size = 0, mp_andes_init_size = 0;
	u32 mp_flash_addr, mp_size, overlay_start_addr, overlay_end_addr;
	bool dma_trigger_enable = 0;
	u8 cmd[16] = {0};

	cmd[0] = P5_X_MODE_CONTROL;
	cmd[1] = P5_X_FW_TEST_MODE;
	ret = idev->write(cmd, 2);
	if (ret < 0)
		goto out;

	cmd[0] = P5_X_MP_TEST_MODE_INFO;
	ret = idev->write(cmd, 1);
	if (ret < 0)
		goto out;

	memset(cmd, 0, sizeof(cmd));

	ipio_info("read mp info length = %d\n", idev->protocol->mp_info_len);
	ret = idev->read(cmd, idev->protocol->mp_info_len);
	if (ret < 0)
		goto out;

	ilitek_dump_data(cmd, 8, idev->protocol->mp_info_len, 0, "MP overlay info");

	dma_trigger_enable = 0;

	mp_flash_addr = cmd[3] + (cmd[2] << 8) + (cmd[1] << 16);
	mp_size = cmd[6] + (cmd[5] << 8) + (cmd[4] << 16);
	overlay_start_addr = cmd[9] + (cmd[8] << 8) + (cmd[7] << 16);
	overlay_end_addr = cmd[12] + (cmd[11] << 8) + (cmd[10] << 16);

	if (overlay_start_addr != 0x0 && overlay_end_addr != 0x0
		&& cmd[0] == P5_X_MP_TEST_MODE_INFO)
		dma_trigger_enable = 1;

	ipio_info("MP info Overlay: Enable = %d, addr = 0x%x ~ 0x%x, flash addr = 0x%x, mp size = 0x%x\n",
		dma_trigger_enable, overlay_start_addr,
		overlay_end_addr, mp_flash_addr, mp_size);

	/* Check if ic is ready switching test mode from demo mode */
	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
	ret = ilitek_tddi_ic_check_busy(50, 50); /* Set busy as 0x41 */
	if (ret < 0)
		goto out;

	ret = ilitek_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0)
		goto out;

	if (dma_trigger_enable) {
		mp_andes_init_size = overlay_start_addr;
		mp_text_size = (mp_size - overlay_end_addr) + 1;
		ipio_info("MP andes init size = %d , MP text size = %d\n", mp_andes_init_size, mp_text_size);

		dma_clear_reg_setting();

		ipio_info("[Move ANDES.INIT to DRAM]\n");
		dma_trigger_reg_setting(0, mp_flash_addr, mp_andes_init_size);	 /* DMA ANDES.INIT */

		dma_clear_reg_setting();

		ipio_info("[Move MP.TEXT to DRAM]\n");
		dma_trigger_reg_setting(overlay_end_addr, (mp_flash_addr + overlay_start_addr), mp_text_size);

		dma_clear_reg_setting();
	} else {
		/* DMA Trigger */
		if (ilitek_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
			ipio_err("Trigger DMA failed\n");
		/* waiting for fw reload code completed. */
		mdelay(30);

		/* CS High */
		if (ilitek_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
			ipio_err("Pull CS High failed\n");
		/* waiting for CS status done */
		mdelay(10);
	}

	if (ilitek_tddi_reset_ctrl(TP_IC_CODE_RST) < 0)
		ipio_err("IC Code reset failed during moving mp code\n");

	ret = ilitek_ice_mode_ctrl(DISABLE, OFF);
	if (ret < 0)
		goto out;

	/* Check if ic is already in test mode */
	idev->actual_tp_mode = P5_X_FW_TEST_MODE; /* set busy as 0x51 */
	ret = ilitek_tddi_ic_check_busy(300, 50);
	if (ret < 0)
		ipio_err("Check cdc timeout failed after moved mp code\n");

out:
	return ret;
}

int ilitek_tddi_move_mp_code_iram(void)
{
	ipio_info("Download MP code to iram\n");
	return ilitek_tddi_fw_upgrade_handler(NULL);
}

int ilitek_tddi_move_gesture_code_flash(int mode)
{
	u8 tp_mode = P5_X_FW_GESTURE_MODE;

	ipio_info();
	return ilitek_tddi_switch_mode(&tp_mode);
}

int ilitek_tddi_move_gesture_code_iram(int mode)
{
	int i;
	int timeout = 10;
	u8 tp_mode = P5_X_FW_GESTURE_MODE;
	u8 cmd[3] = {0};

	ipio_info();

	if (ilitek_tddi_ic_func_ctrl("lpwg", 0x3) < 0)
		ipio_err("write gesture flag failed\n");

	if (ilitek_tddi_switch_mode(&tp_mode) < 0)
		ipio_err("Switch to gesture mode failed during moving code\n");

	for (i = 0; i < timeout; i++) {
		/* Prepare Check Ready */
		cmd[0] = P5_X_READ_DATA_CTRL;
		cmd[1] = 0xA;
		cmd[2] = 0x5;
		idev->write(cmd, 2);

		mdelay(10);

		/* Check ready for load code */
		cmd[0] = 0x1;
		cmd[1] = 0xA;
		cmd[2] = 0x5;
		if ((idev->write(cmd, 3)) < 0)
			ipio_err("write 0x1,0xA,0x5 error");

		if ((idev->read(cmd, 1)) < 0)
			ipio_err("read gesture ready byte error\n");

		ipio_debug("gesture ready byte = 0x%x\n", cmd[0]);
		if (cmd[0] == 0x91) {
			ipio_info("Gesture check fw ready\n");
			break;
		}
	}

	if (i >= timeout) {
		ipio_err("Gesture is not ready (0x%x), try to run its recovery\n", cmd[0]);
		ilitek_tddi_gesture_recovery();
		return 0;
	}

	if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
		ipio_err("FW upgrade failed during moving code\n");

	/* FW star run gestrue code cmd */
	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x6;
	if ((idev->write(cmd, 3)) < 0)
		ipio_err("write 0x1,0xA,0x6 error");
	return 0;
}

u8 ilitek_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8) ((-sum) & 0xFF);
}

void ilitek_tddi_touch_esd_gesture_flash(void)
{
	int retry = 100;
	u32 answer = 0;

	if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
		ipio_err("Enable ice mode failed during gesture recovery\n");

	ipio_info("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
		I2C_ESD_GESTURE_PWD_ADDR, I2C_ESD_GESTURE_RUN);

	/* write a special password to inform FW go back into gesture mode */
	if (ilitek_ice_mode_write(I2C_ESD_GESTURE_PWD_ADDR, ESD_GESTURE_PWD, 4) < 0)
		ipio_err("write password failed\n");

	/* HW reset gives effect to FW receives password successed */
	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
	if (ilitek_tddi_reset_ctrl(idev->reset) < 0)
		ipio_err("TP Reset failed during gesture recovery\n");
	/* waiting for FW reloading code */
	msleep(100);

	if (ilitek_ice_mode_ctrl(ENABLE, ON) < 0)
		ipio_err("Enable ice mode failed during gesture recovery\n");

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		if (ilitek_ice_mode_read(I2C_ESD_GESTURE_PWD_ADDR, &answer, sizeof(u32)) < 0)
			ipio_err("Read gesture answer error\n");
		if (answer != I2C_ESD_GESTURE_RUN)
			ipio_info("answer = 0x%x != (0x%x)\n", answer, I2C_ESD_GESTURE_RUN);
		msleep(10);
		retry--;
	} while (answer != I2C_ESD_GESTURE_RUN && retry > 0);

	if (retry <= 0)
		ipio_err("Enter gesture failed\n");
	else
		ipio_info("Enter gesture successfully\n");

	if (ilitek_ice_mode_ctrl(DISABLE, ON) < 0)
		ipio_err("Disable ice mode failed during gesture recovery\n");

	idev->gesture_move_code(idev->gesture_mode);
}

void ilitek_tddi_touch_esd_gesture_iram(void)
{
	int retry = 100;
	u32 answer = 0;
	u8 cmd[3] = {0};

	if (ilitek_ice_mode_ctrl(ENABLE, OFF) < 0)
		ipio_err("Enable ice mode failed during gesture recovery\n");

	ipio_info("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
		SPI_ESD_GESTURE_PWD_ADDR, SPI_ESD_GESTURE_RUN);

	/* write a special password to inform FW go back into gesture mode */
	if (ilitek_ice_mode_write(SPI_ESD_GESTURE_PWD_ADDR, ESD_GESTURE_PWD, 4) < 0)
		ipio_err("write password failed\n");

	/* Host download gives effect to FW receives password successed */
	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;
	if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
		ipio_err("FW upgrade failed during gesture recovery\n");

	if (ilitek_ice_mode_ctrl(ENABLE, ON) < 0)
		ipio_err("Enable ice mode failed during gesture recovery\n");

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		if (ilitek_ice_mode_read(SPI_ESD_GESTURE_PWD_ADDR, &answer, sizeof(u32)) < 0)
			ipio_err("Read gesture answer error\n");

		if (answer != SPI_ESD_GESTURE_RUN)
			ipio_info("answer = 0x%x != (0x%x)\n", answer, SPI_ESD_GESTURE_RUN);
		msleep(10);
	} while (answer != SPI_ESD_GESTURE_RUN && --retry > 0);

	if (retry <= 0)
		ipio_err("Enter gesture failed\n");
	else
		ipio_info("Enter gesture successfully\n");

	if (ilitek_ice_mode_ctrl(DISABLE, ON) < 0)
		ipio_err("Disable ice mode failed during gesture recovery\n");

	idev->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	if (ilitek_tddi_fw_upgrade_handler(NULL) < 0)
		ipio_err("FW upgrade failed during gesture recovery\n");

	/* FW star run gestrue code cmd */
	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x6;
	if ((idev->write(cmd, sizeof(cmd))) < 0)
		ipio_err("write 0x1,0xA,0x6 error");

}

int ilitek_tddi_debug_report_alloc(void)
{
	int i, ret = 0;
	int row_size = 2 * K + 1, col_size = 2 * K + 1;
	ipio_debug("row_size idev->debug_node_open = %d ", idev->debug_node_open);
	if (!idev->debug_node_open && !idev->debug_buf)
		return ret;

	if (!idev->debug_node_open && idev->debug_buf != NULL)
		goto out;

	if (idev->debug_node_open && !idev->debug_buf) {
		idev->debug_buf = (unsigned char **)kzalloc(row_size * sizeof(unsigned char *), GFP_KERNEL);
		if (ERR_ALLOC_MEM(idev->debug_buf)) {
			ipio_err("Failed to allocate debug_buf mem, %ld\n", PTR_ERR(idev->debug_buf));
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < row_size; i++) {
			idev->debug_buf[i] = (unsigned char *)kzalloc(col_size * sizeof(unsigned char), GFP_KERNEL);
			if (ERR_ALLOC_MEM(idev->debug_buf[i])) {
				ipio_err("Failed to allocate debug_buf[%d] mem, %ld\n", i, PTR_ERR(idev->debug_buf[i]));
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	return ret;

out:
	/* Note that it might be freed by next touch event */
	if (idev->debug_buf != NULL) {
		idev->debug_data_frame = 0;
		for (i = 0; i < row_size; i++) {
			if (idev->debug_buf[i] != NULL) {
				kfree(idev->debug_buf[i]);
				idev->debug_buf[i] = NULL;
			}
		}
		kfree(idev->debug_buf);
		idev->debug_buf = NULL;
	}
	return ret;
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
	mutex_lock(&idev->debug_mutex);

	if (ilitek_tddi_debug_report_alloc() < 0)
		goto out;

	if (!idev->netlink && !idev->debug_node_open)
		goto out;

	/* Send data to netlink */
	if (idev->netlink) {
		netlink_reply_msg(buf, len);
		goto out;
	}

	/* Sending data to apk via the node of debug_message node */
	if (idev->debug_node_open && idev->debug_buf != NULL) {
		ipio_debug("idev->debug_buf[idev->debug_data_frame] = %s", idev->debug_buf[idev->debug_data_frame]);
		memset(idev->debug_buf[idev->debug_data_frame], 0x00, (u8)sizeof(u8) * 2048);
		ipio_memcpy(idev->debug_buf[idev->debug_data_frame], buf, len, 2048);
		idev->debug_data_frame++;
		if (idev->debug_data_frame > 1)
			ipio_debug("idev->debug_data_frame = %d\n", idev->debug_data_frame);
		if (idev->debug_data_frame > 1023) {
			ipio_err("idev->debug_data_frame = %d > 1024\n",
				idev->debug_data_frame);
			idev->debug_data_frame = 1023;
		}
		wake_up(&(idev->inq));
		goto out;
	}

out:
	mutex_unlock(&idev->debug_mutex);
}

void ilitek_tddi_touch_press(u16 x, u16 y, u16 pressure, u16 id)
{
	ipio_debug("Touch Press: id = %d, x = %d, y = %d, p = %d\n", id, x, y, pressure);

	if (MT_B_TYPE) {
		input_mt_slot(idev->input, id);
		input_mt_report_slot_state(idev->input, MT_TOOL_FINGER, true);
		input_report_abs(idev->input, ABS_MT_POSITION_X, x);
		input_report_abs(idev->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(idev->input, ABS_MT_PRESSURE, pressure);
	} else {
		input_report_key(idev->input, BTN_TOUCH, 1);
		input_report_abs(idev->input, ABS_MT_TRACKING_ID, id);
		input_report_abs(idev->input, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(idev->input, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(idev->input, ABS_MT_POSITION_X, x);
		input_report_abs(idev->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(idev->input, ABS_MT_PRESSURE, pressure);

		input_mt_sync(idev->input);
	}
}

void ilitek_tddi_touch_release(u16 x, u16 y, u16 id)
{
	ipio_debug("Touch Release: id = %d, x = %d, y = %d\n", id, x, y);

	if (MT_B_TYPE) {
		input_mt_slot(idev->input, id);
		input_mt_report_slot_state(idev->input, MT_TOOL_FINGER, false);
	} else {
		input_report_key(idev->input, BTN_TOUCH, 0);
		input_mt_sync(idev->input);
	}
}

void ilitek_tddi_touch_release_all_point(void)
{
	int i;

	if (MT_B_TYPE) {
		for (i = 0 ; i < MAX_TOUCH_NUM; i++)
			ilitek_tddi_touch_release(0, 0, i);

		input_report_key(idev->input, BTN_TOUCH, 0);
		input_report_key(idev->input, BTN_TOOL_FINGER, 0);
	} else {
		ilitek_tddi_touch_release(0, 0, 0);
	}
	input_sync(idev->input);
}

static struct ilitek_touch_info touch_info[MAX_TOUCH_NUM];

void ilitek_tddi_report_ap_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	idev->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF)
			&& (buf[(4 * i) + 3] == 0xFF)) {
			if (MT_B_TYPE)
				idev->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
		yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));

		touch_info[idev->finger].x = xop * idev->panel_wid / TPD_WIDTH;


		touch_info[idev->finger].y = yop * idev->panel_hei / TPD_HEIGHT;
		touch_info[idev->finger].id = i;

		if (MT_PRESSURE)
			touch_info[idev->finger].pressure = buf[(4 * i) + 4];
		else
			touch_info[idev->finger].pressure = 1;

		ipio_debug("original x = %d, y = %d\n", xop, yop);
		idev->finger++;
		if (MT_B_TYPE)
			idev->curt_touch[i] = 1;
	}

	ipio_debug("figner number = %d, LastTouch = %d\n", idev->finger, idev->last_touch);

	if (idev->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < idev->finger; i++) {
				input_report_key(idev->input, BTN_TOUCH, 1);
				ilitek_tddi_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(idev->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (idev->curt_touch[i] == 0 && idev->prev_touch[i] == 1)
					ilitek_tddi_touch_release(0, 0, i);
				idev->prev_touch[i] = idev->curt_touch[i];
			}
		} else {
			for (i = 0; i < idev->finger; i++)
				ilitek_tddi_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(idev->input);
		idev->last_touch = idev->finger;
	} else {
		if (idev->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (idev->curt_touch[i] == 0 && idev->prev_touch[i] == 1)
						ilitek_tddi_touch_release(0, 0, i);
					idev->prev_touch[i] = idev->curt_touch[i];
				}
				input_report_key(idev->input, BTN_TOUCH, 0);
				input_report_key(idev->input, BTN_TOOL_FINGER, 0);
			} else {
				ilitek_tddi_touch_release(0, 0, 0);
			}
			input_sync(idev->input);
			idev->last_touch = 0;
		}
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ilitek_tddi_report_debug_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	idev->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(3 * i) + 5] == 0xFF) && (buf[(3 * i) + 6] == 0xFF)
			&& (buf[(3 * i) + 7] == 0xFF)) {
			if (MT_B_TYPE)
				idev->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(3 * i) + 5] & 0xF0) << 4) | (buf[(3 * i) + 6]));
		yop = (((buf[(3 * i) + 5] & 0x0F) << 8) | (buf[(3 * i) + 7]));

		touch_info[idev->finger].x = xop * idev->panel_wid / TPD_WIDTH;
		touch_info[idev->finger].y = yop * idev->panel_hei / TPD_HEIGHT;
		touch_info[idev->finger].id = i;

		if (MT_PRESSURE)
			touch_info[idev->finger].pressure = buf[(4 * i) + 4];
		else
			touch_info[idev->finger].pressure = 1;

		ipio_debug("original x = %d, y = %d\n", xop, yop);
		idev->finger++;
		if (MT_B_TYPE)
			idev->curt_touch[i] = 1;
	}

	ipio_debug("figner number = %d, LastTouch = %d\n", idev->finger, idev->last_touch);

	if (idev->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < idev->finger; i++) {
				input_report_key(idev->input, BTN_TOUCH, 1);
				ilitek_tddi_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(idev->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (idev->curt_touch[i] == 0 && idev->prev_touch[i] == 1)
					ilitek_tddi_touch_release(0, 0, i);
				idev->prev_touch[i] = idev->curt_touch[i];
			}
		} else {
			for (i = 0; i < idev->finger; i++)
				ilitek_tddi_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(idev->input);
		idev->last_touch = idev->finger;
	} else {
		if (idev->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (idev->curt_touch[i] == 0 && idev->prev_touch[i] == 1)
						ilitek_tddi_touch_release(0, 0, i);
					idev->prev_touch[i] = idev->curt_touch[i];
				}
				input_report_key(idev->input, BTN_TOUCH, 0);
				input_report_key(idev->input, BTN_TOOL_FINGER, 0);
			} else {
				ilitek_tddi_touch_release(0, 0, 0);
			}
			input_sync(idev->input);
			idev->last_touch = 0;
		}
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ilitek_tddi_report_gesture_mode(u8 *buf, int len)
{
	ipio_info("gesture code = 0x%x\n", buf[1]);

	switch (buf[1]) {
	case GESTURE_DOUBLECLICK:
		ipio_info("Double Click key event\n");
		input_report_key(idev->input, KEY_GUSTURE_WAKEUP, 1);
		input_sync(idev->input);
		input_report_key(idev->input, KEY_GUSTURE_WAKEUP, 0);
		input_sync(idev->input);
		break;
	case GESTURE_LEFT:
		break;
	case GESTURE_RIGHT:
		break;
	case GESTURE_UP:
		break;
	case GESTURE_DOWN:
		break;
	case GESTURE_O:
		break;
	case GESTURE_W:
		break;
	case GESTURE_M:
		break;
	case GESTURE_E:
		break;
	case GESTURE_S:
		break;
	case GESTURE_V:
		break;
	case GESTURE_Z:
		break;
	case GESTURE_C:
		break;
	case GESTURE_F:
		break;
	case P5_X_LARGE_AREA_PRESS_PACKET_ID:
		ipio_info(" Gesture LARGE_AREA PRESS_PACKET_ID \n");
		break;
	case P5_X_LARGE_AREA_RELEASE_PACKET_ID:
		ipio_info(" Gesture LARGE_AREA RELEASE_PACKET_ID \n");
		break;
	default:
		break;
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ilitek_tddi_report_i2cuart_mode(u8 *buf, int len)
{
	int type = buf[3] & 0x0F;
	int need_read_len = 0, one_data_bytes = 0;
	int actual_len = len - 5;
	int uart_len;
	u8 *uart_buf, *total_buf;

	ipio_debug("data[3] = %x, type = %x, actual_len = %d\n",
					buf[3], type, actual_len);

	need_read_len = buf[1] * buf[2];

	if (type == 0 || type == 1 || type == 6) {
		one_data_bytes = 1;
	} else if (type == 2 || type == 3) {
		one_data_bytes = 2;
	} else if (type == 4 || type == 5) {
		one_data_bytes = 4;
	}

	need_read_len =  need_read_len * one_data_bytes + 1;
	ipio_debug("need_read_len = %d  one_data_bytes = %d\n", need_read_len, one_data_bytes);

	if (need_read_len > actual_len) {
		uart_len = need_read_len - actual_len;
		ipio_debug("uart len = %d\n", uart_len);

		uart_buf = kcalloc(uart_len, sizeof(u8), GFP_KERNEL);
		if (ERR_ALLOC_MEM(uart_buf)) {
			ipio_err("Failed to allocate uart_buf memory %ld\n", PTR_ERR(uart_buf));
			return;
		}

		if (idev->read(uart_buf, uart_len) < 0) {
			ipio_err("i2cuart read data failed\n");
			return;
		}

		total_buf = kcalloc(len + uart_len, sizeof(u8), GFP_KERNEL);
		if (ERR_ALLOC_MEM(total_buf)) {
			ipio_err("Failed to allocate total_buf memory %ld\n", PTR_ERR(total_buf));
			return;
		}
		memcpy(total_buf, buf, len);
		memcpy(total_buf + len, uart_buf, uart_len);
		ilitek_tddi_touch_send_debug_data(total_buf, len + uart_len);
		return;
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}
