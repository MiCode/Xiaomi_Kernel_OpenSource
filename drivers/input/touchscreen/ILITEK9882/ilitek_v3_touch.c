/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "ilitek_v3.h"

/*gesture info mode*/
struct ili_demo_debug_info_id0 {
	u32 id			: 8;
	u32 sys_powr_state_e	: 3;
	u32 sys_state_e 	: 3;
	u32 tp_state_e		: 2;

	u32 touch_palm_state	: 2;
	u32 app_an_statu_e	: 3;
	u32 app_sys_bg_err	: 1;
	u32 g_b_wrong_bg	: 1;
	u32 reserved0		: 1;

	u32 normal_mode 	: 1;
	u32 charger_mode	: 1;
	u32 glove_mode		: 1;
	u32 stylus_mode 	: 1;
	u32 multi_mode		: 1;
	u32 noise_mode		: 1;
	u32 palm_plus_mode	: 1;
	u32 floating_mode	: 1;

	u32 algo_pt_status0	: 3;
	u32 algo_pt_status1	: 3;
	u32 algo_pt_status2	: 3;
	u32 algo_pt_status3	: 3;
	u32 algo_pt_status4	: 3;
	u32 algo_pt_status5	: 3;
	u32 algo_pt_status6	: 3;
	u32 algo_pt_status7	: 3;
	u32 algo_pt_status8	: 3;
	u32 algo_pt_status9	: 3;
	u32 reserved2		: 2;

	u32 hopping_flg 	: 1;
	u32 hopping_index	: 5;
	u32 frequency_h 	: 2;
	u32 frequency_l 	: 8;
	u32 reserved3		: 8;
	u32 reserved4		: 8;

};

void ili_dump_data(void *data, int type, int len, int row_len, const char *name)
{
	int i, row = 31;
	u8 *p8 = NULL;
	s32 *p32 = NULL;
	s16 *p16 = NULL;

	if (!debug_en)
		return;

	if (row_len > 0)
		row = row_len;

	if (data == NULL) {
		ILI_ERR("The data going to dump is NULL\n");
		return;
	}

	pr_cont("\n\n");
	pr_cont("ILITEK: Dump %s data\n", name);
	pr_cont("ILITEK: ");

	if (type == 8)
		p8 = (u8 *) data;
	if (type == 32 || type == 10)
		p32 = (s32 *) data;
	if (type == 16)
		p16 = (s16 *) data;

	for (i = 0; i < len; i++) {
		if (type == 8)
			pr_cont(" %4x ", p8[i]);
		else if (type == 32)
			pr_cont(" %4x ", p32[i]);
		else if (type == 10)
			pr_cont(" %4d ", p32[i]);
		else if (type == 16)
			pr_cont(" %4d ", p16[i]);

		if ((i % row) == row - 1) {
			pr_cont("\n");
			pr_cont("ILITEK: ");
		}
	}
	pr_cont("\n\n");
}

static void dma_clear_reg_setting(void)
{
	/* 1. interrupt t0/t1 enable flag */
	if (ili_ice_mode_bit_mask_write(INTR32_ADDR, INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, INTR32_ADDR);

	/* 2. clear tdi_err_int_flag */
	if (ili_ice_mode_bit_mask_write(INTR2_ADDR, INTR2_tdi_err_int_flag_clear, (1 << 18)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR2_tdi_err_int_flag_clear, INTR2_ADDR);

	/* 3. clear dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00000000, 4) < 0)
		ILI_ERR("Write 0x00000000 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	/* 4. clear dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (0 << 16)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	/* 5. clear dma flash setting */
	ili_flash_clear_dma();
}

static void dma_trigger_reg_setting(u32 reg_dest_addr, u32 flash_start_addr, u32 copy_size)
{
	int retry = 30;
	u32 stat = 0;

	/* 1. set dma channel 0 clear */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_start_clear, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_start_clear, DMA48_ADDR);

	/* 2. set dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00041010, 4) < 0)
		ILI_ERR("Write 0x00041010 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x00 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	/* 3. set dma channel 0 src2 info */
	if (ili_ice_mode_bit_mask_write(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA52_reg_dma_ch0_src2_en, DMA52_ADDR);

	/* 4. set dma channel 0 dest info */
	if (ili_ice_mode_write(DMA53_reg_dma_ch0_dest_addr, reg_dest_addr, 3) < 0)
		ILI_ERR("Write %x at %x failed\n", reg_dest_addr, DMA53_reg_dma_ch0_dest_addr);
	if (ili_ice_mode_write(DMA54_reg_dma_ch0_dest_step_inc, 0x01, 1) < 0)
		ILI_ERR("Write 0x01 at %x failed\n", DMA54_reg_dma_ch0_dest_step_inc);

	if (ili_ice_mode_write(DMA54_ADDR, DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, DMA54_ADDR);

	/* 5. set dma channel 0 trafer info */
	if (ili_ice_mode_write(DMA55_reg_dma_ch0_trafer_counts, copy_size, 4) < 0)
		ILI_ERR("Write %x at %x failed\n", copy_size, DMA55_reg_dma_ch0_trafer_counts);
	if (ili_ice_mode_bit_mask_write(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA55_reg_dma_ch0_trafer_mode, DMA55_ADDR);

	/* 6. set dma channel 0 int info */
	if (ili_ice_mode_bit_mask_write(INTR33_ADDR, INTR33_reg_dma_ch0_int_en, (1 << 17)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR33_reg_dma_ch0_int_en, INTR33_ADDR);

	/* 7. set dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (1 << 16)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);

	/* 8. set dma flash setting */
	ili_flash_dma_write(flash_start_addr, (flash_start_addr+copy_size), copy_size);

	/* 9. clear flash and dma ch0 int flag */
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_dma_ch1_int_flag | INTR1_reg_flash_int_flag, BIT(16) | BIT(25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_dma_ch1_int_flag | INTR1_reg_flash_int_flag, INTR1_ADDR);
	if (ili_ice_mode_bit_mask_write(0x041013, BIT(0), 1) < 0) //patch
		ILI_ERR("Write %lu at %x failed\n", BIT(0), 0x041013);

	/* DMA Trigger */
	if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ILI_ERR("Trigger DMA failed\n");

	/* waiting for fw reload code completed. */
	while (retry > 0) {
		if (ili_ice_mode_read(INTR1_ADDR, &stat, sizeof(u32)) < 0) {
			ILI_ERR("Read 0x%x error\n", INTR1_ADDR);
			retry--;
			continue;
		}

		ILI_DBG("fw dma stat = %x\n", stat);

		if ((stat & BIT(16)) == BIT(16))
			break;

		retry--;
		usleep_range(1000, 1000);
	}

	if (retry <= 0)
		ILI_ERR("DMA fail: Regsiter = 0x%x Flash = 0x%x, Size = %d\n",
			reg_dest_addr, flash_start_addr, copy_size);

	/* CS High */
	if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
		ILI_ERR("Pull CS High failed\n");
	/* waiting for CS status done */
	mdelay(10);
}

int ili_move_mp_code_flash(void)
{
	int ret = 0;
	u32 mp_text_size = 0, mp_andes_init_size = 0;
	u32 mp_flash_addr, mp_size, overlay_start_addr, overlay_end_addr;
	bool dma_trigger_enable = 0;
	u8 cmd[2] = {0};
	u8 data[16] = {0};

	cmd[0] = P5_X_MP_TEST_MODE_INFO;
	ret = ilits->wrapper(cmd, sizeof(u8), data, ilits->protocol->mp_info_len, ON, OFF);
	ili_dump_data(data, 8, ilits->protocol->mp_info_len, 0, "MP overlay info");
	if (ret < 0) {
		ILI_ERR("Failed to write info cmd\n");
		goto out;
	}

	dma_trigger_enable = 0;

	mp_flash_addr = data[3] + (data[2] << 8) + (data[1] << 16);
	mp_size = data[6] + (data[5] << 8) + (data[4] << 16);
	overlay_start_addr = data[9] + (data[8] << 8) + (data[7] << 16);
	overlay_end_addr = data[12] + (data[11] << 8) + (data[10] << 16);

	if (overlay_start_addr != 0x0 && overlay_end_addr != 0x0
		&& data[0] == P5_X_MP_TEST_MODE_INFO)
		dma_trigger_enable = 1;

	ILI_INFO("MP info Overlay: Enable = %d, addr = 0x%x ~ 0x%x, flash addr = 0x%x, mp size = 0x%x\n",
		dma_trigger_enable, overlay_start_addr,
		overlay_end_addr, mp_flash_addr, mp_size);

	cmd[0] = P5_X_MODE_CONTROL;
	cmd[1] = P5_X_FW_TEST_MODE;
	ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);
	if (ret < 0)
		goto out;

	/* Check if ic is ready switching test mode from demo mode */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_ic_check_busy(50, 50, ON); /* Set busy as 0x41 */
	if (ret < 0)
		goto out;

	ret = ili_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0)
		goto out;

	if (dma_trigger_enable) {
		mp_andes_init_size = overlay_start_addr;
		mp_text_size = (mp_size - overlay_end_addr) + 1;
		ILI_INFO("MP andes init size = %d , MP text size = %d\n", mp_andes_init_size, mp_text_size);

		dma_clear_reg_setting();

		ILI_INFO("[Move ANDES.INIT to DRAM]\n");
		dma_trigger_reg_setting(0, mp_flash_addr, mp_andes_init_size);	 /* DMA ANDES.INIT */

		dma_clear_reg_setting();

		ILI_INFO("[Move MP.TEXT to DRAM]\n");
		dma_trigger_reg_setting(overlay_end_addr, (mp_flash_addr + overlay_start_addr), mp_text_size);

		dma_clear_reg_setting();
	} else {
		/* DMA Trigger */
		if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
			ILI_ERR("Trigger DMA failed\n");
		/* waiting for fw reload code completed. */
		mdelay(30);

		/* CS High */
		if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
			ILI_ERR("Pull CS High failed\n");
		/* waiting for CS status done */
		mdelay(10);
	}

	if (ili_reset_ctrl(TP_IC_CODE_RST) < 0)
		ILI_ERR("IC Code reset failed during moving mp code\n");

	ret = ili_ice_mode_ctrl(DISABLE, OFF);
	if (ret < 0)
		goto out;

	/* Check if ic is already in test mode */
	ilits->actual_tp_mode = P5_X_FW_TEST_MODE; /* set busy as 0x51 */
	ret = ili_ic_check_busy(300, 50, ON);
	if (ret < 0)
		ILI_ERR("Check cdc timeout failed after moved mp code\n");

out:
	return ret;
}

int ili_move_mp_code_iram(void)
{
	ILI_INFO("Download MP code to iram\n");
	return ili_fw_upgrade_handler(NULL);
}

int ili_proximity_near(int mode)
{
	int ret = 0;

	ilits->prox_near = true;

	switch (mode) {
	case DDI_POWER_ON:
		/*
		 * If the power of VSP and VSN keeps alive when proximity near event
		 * occures, TP can just go to sleep in.
		 */
		ret = ili_ic_func_ctrl("sleep", SLEEP_IN);
		if (ret < 0)
			ILI_ERR("Write sleep in cmd failed\n");
		break;
	case DDI_POWER_OFF:
		ILI_INFO("DDI POWER OFF, do nothing\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int ili_proximity_far(int mode)
{
	int ret = 0;
	u8 cmd[2] = {0};

	if (!ilits->prox_near) {
		ILI_INFO("No proximity near event, break\n");
		return 0;
	}

	switch (mode) {
	case WAKE_UP_GESTURE_RECOVERY:
		/*
		 * If the power of VSP and VSN has been shut down previsouly,
		 * TP should go through gesture recovery to get back.
		 */
		ili_gesture_recovery();
		break;
	case WAKE_UP_SWITCH_GESTURE_MODE:
		/*
		 * If the power of VSP and VSN keeps alive in the event of proximity near,
		 * TP can be just recovered by switching gesture mode to get back.
		 */
		cmd[0] = 0xF6;
		cmd[1] = 0x0A;

		ILI_INFO("write prepare gesture command 0xF6 0x0A\n");
		ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);
		if (ret < 0) {
			ILI_INFO("write prepare gesture command error\n");
			break;
		}

		ret = ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
		if (ret < 0)
			ILI_ERR("Switch to gesture mode failed during proximity far\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}

	ilits->prox_near = false;

	return ret;
}

int ili_move_gesture_code_flash(int mode)
{
	int ret = 0;

	/*
	 * NOTE: If functions need to be added during suspend,
	 * they must be called before gesture cmd reaches to FW.
	 */

	ILI_INFO("Gesture mode = %d\n",  mode);
	ret = ili_set_tp_data_len(mode, true, NULL);

	return ret;
}

void ili_set_gesture_symbol(void)
{
	u8 cmd[7] = {0};
	struct gesture_symbol *ptr_sym = &ilits->ges_sym;
	u8 *ptr;

	ptr = (u8 *) ptr_sym;
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0x01;
	cmd[2] = 0x0A;
	cmd[3] = 0x08;
	cmd[4] = ptr[0];
	cmd[5] = ptr[1];
	cmd[6] = ptr[2];

	ili_dump_data(cmd, 8, sizeof(cmd), 0, "Gesture symbol");

	if (ilits->wrapper(cmd, 2, NULL, 0, ON, OFF) < 0) {
		ILI_ERR("Write pre cmd failed\n");
		return;

	}

	if (ilits->wrapper(&cmd[1], (sizeof(cmd) - 1), NULL, 0, ON, OFF)) {
		ILI_ERR("Write gesture symbol fail\n");
		return;
	}

	ILI_DBG(" double_tap = %d\n", ilits->ges_sym.double_tap);
	ILI_DBG(" alphabet_line_2_top = %d\n", ilits->ges_sym.alphabet_line_2_top);
	ILI_DBG(" alphabet_line_2_bottom = %d\n", ilits->ges_sym.alphabet_line_2_bottom);
	ILI_DBG(" alphabet_line_2_left = %d\n", ilits->ges_sym.alphabet_line_2_left);
	ILI_DBG(" alphabet_line_2_right = %d\n", ilits->ges_sym.alphabet_line_2_right);
	ILI_DBG(" alphabet_w = %d\n", ilits->ges_sym.alphabet_w);
	ILI_DBG(" alphabet_c = %d\n", ilits->ges_sym.alphabet_c);
	ILI_DBG(" alphabet_E = %d\n", ilits->ges_sym.alphabet_E);
	ILI_DBG(" alphabet_V = %d\n", ilits->ges_sym.alphabet_V);
	ILI_DBG(" alphabet_O = %d\n", ilits->ges_sym.alphabet_O);
	ILI_DBG(" alphabet_S = %d\n", ilits->ges_sym.alphabet_S);
	ILI_DBG(" alphabet_Z = %d\n", ilits->ges_sym.alphabet_Z);
	ILI_DBG(" alphabet_V_down = %d\n", ilits->ges_sym.alphabet_V_down);
	ILI_DBG(" alphabet_V_left = %d\n", ilits->ges_sym.alphabet_V_left);
	ILI_DBG(" alphabet_V_right = %d\n", ilits->ges_sym.alphabet_V_right);
	ILI_DBG(" alphabet_two_line_2_bottom= %d\n", ilits->ges_sym.alphabet_two_line_2_bottom);
	ILI_DBG(" alphabet_F= %d\n", ilits->ges_sym.alphabet_F);
	ILI_DBG(" alphabet_AT= %d\n", ilits->ges_sym.alphabet_AT);

}

int ili_move_gesture_code_iram(int mode)
{
	int i, ret = 0, timeout = 10;
	u8 cmd[2] = {0};
	u8 cmd_write[3] = {0x01, 0x0A, 0x05};

	/*
	 * NOTE: If functions need to be added during suspend,
	 * they must be called before gesture cmd reaches to FW.
	 */

	ILI_INFO("Gesture code loaded by %s\n", ilits->gesture_load_code ? "driver" : "firmware");

	if (!ilits->gesture_load_code) {
		ret = ili_set_tp_data_len(mode, true, NULL);
		goto out;
	}

	/*pre-command for ili_ic_func_ctrl("lpwg", 0x3)*/
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0x1;
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Write 0xF6,0x1 failed\n");
		goto out;
	}

	ret = ili_ic_func_ctrl("lpwg", 0x3);
	if (ret < 0) {
		ILI_ERR("write gesture flag failed\n");
		goto out;
	}

	ret = ili_set_tp_data_len(mode, true, NULL);
	if (ret < 0) {
		ILI_ERR("Failed to set tp data length\n");
		goto out;
	}

	ili_irq_enable();
	/* Prepare Check Ready */
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0xA;
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Write 0xF6,0xA failed\n");
		goto out;
	}

	for (i = 0; i < timeout; i++) {
		/* Check ready for load code */
		ret = ilits->wrapper(cmd_write, sizeof(cmd_write), cmd, sizeof(u8), ON, OFF);
		ILI_DBG("gesture ready byte = 0x%x\n", cmd[0]);

		if (cmd[0] == 0x91) {
			ILI_INFO("Ready to load gesture code\n");
			break;
		}
	}
	ili_irq_disable();

	if (i >= timeout) {
		ILI_ERR("Gesture is not ready (0x%x), try to run its recovery\n", cmd[0]);
		return ili_gesture_recovery();
	}

	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("FW upgrade failed during moving code\n");
		goto out;
	}

	/* Resume gesture loader */
	ret = ili_ic_func_ctrl("lpwg", 0x6);
	if (ret < 0) {
		ILI_ERR("write resume loader error");
		goto out;
	}

out:
	return ret;
}

u8 ili_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8) ((-sum) & 0xFF);
}

int ili_touch_esd_gesture_flash(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;
	int ges_pwd_addr = I2C_ESD_GESTURE_CORE146_PWD_ADDR;
	int ges_pwd = ESD_GESTURE_CORE146_PWD;
	int ges_run = I2C_ESD_GESTURE_CORE146_RUN;
	int pwd_len = 2;

	if (ilits->chip->core_ver < CORE_VER_1460) {
		ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
		ges_pwd = ESD_GESTURE_PWD;
		ges_run = I2C_ESD_GESTURE_RUN;
		pwd_len = 4;
	}

	ret = ili_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		return ret;
	}

	ILI_INFO("ESD Gesture PWD Addr = 0x%X, PWD = 0x%X, GES_RUN = 0%X, core_ver = 0x%X\n",
		ges_pwd_addr, ges_pwd, ges_run, ilits->chip->core_ver);

	/* write a special password to inform FW go back into gesture mode */
	ret = ili_ice_mode_write(ges_pwd_addr, ges_pwd, pwd_len);
	if (ret < 0) {
		ILI_ERR("write password failed\n");
		goto out;
	}

	/* HW reset gives effect to FW receives password successed */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_reset_ctrl(ilits->reset);
	if (ret < 0) {
		ILI_ERR("TP Reset failed during gesture recovery\n");
		goto out;
	}

	ret = ili_ice_mode_ctrl(ENABLE, ON);
	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		goto out;
	}

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ret = ili_ice_mode_read(ges_pwd_addr, &answer, pwd_len);
		if (ret < 0) {
			ILI_ERR("Read gesture answer error\n");
			goto out;
		}

		if (answer != ges_run)
			ILI_INFO("ret = 0x%X, answer = 0x%X\n", answer, ges_run);

		mdelay(2);
	} while (answer != ges_run && --retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
		goto out;
	}

	ILI_INFO("0x%X Enter gesture successfully\n", answer);

out:
	if (ili_ice_mode_ctrl(DISABLE, ON) < 0) {
		ILI_ERR("Disable ice mode failed during gesture recovery\n");
	}

	if (ret >= 0) {
		ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
		ili_set_tp_data_len(ilits->gesture_mode, false, NULL);
	}

	return ret;
}

int ili_touch_esd_gesture_iram(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;
	int ges_pwd_addr = SPI_ESD_GESTURE_CORE146_PWD_ADDR;
	int ges_pwd = ESD_GESTURE_CORE146_PWD;
	int ges_run = SPI_ESD_GESTURE_CORE146_RUN;
	int pwd_len = 2;

	if (ilits->chip->core_ver < CORE_VER_1460) {
		if (ilits->chip->core_ver >= CORE_VER_1420)
			ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
		else
			ges_pwd_addr = SPI_ESD_GESTURE_PWD_ADDR;
		ges_pwd = ESD_GESTURE_PWD;
		ges_run = SPI_ESD_GESTURE_RUN;
		pwd_len = 4;
	}

	ILI_INFO("ESD Gesture PWD Addr = 0x%X, PWD = 0x%X, GES_RUN = 0%X, core_ver = 0x%X\n",
		ges_pwd_addr, ges_pwd, ges_run, ilits->chip->core_ver);

		ret = ili_ice_mode_ctrl(ENABLE, OFF);
		if (ret < 0) {
			ILI_ERR("Enable ice mode failed during gesture recovery\n");
			goto fail;
		}

	/* write a special password to inform FW go back into gesture mode */
	ret = ili_ice_mode_write(ges_pwd_addr, ges_pwd, pwd_len);
	if (ret < 0) {
		ILI_ERR("write password failed\n");
		goto fail;
	}

	/* Host download gives effect to FW receives password successed */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("FW upgrade failed during gesture recovery\n");
		goto fail;
	}

	/* Wait for fw running code finished. */
	if (ilits->info_from_hex || (ilits->chip->core_ver >= CORE_VER_1410))
		msleep(50);


	ret = ili_ice_mode_ctrl(ENABLE, ON);
	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		goto fail;
	}

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ret = ili_ice_mode_read(ges_pwd_addr, &answer, pwd_len);
		if (ret < 0) {
			ILI_ERR("Read gesture answer error\n");
			goto fail;
		}

		if (answer != ges_run)
			ILI_INFO("ret = 0x%X, answer = 0x%X\n", answer, ges_run);

		mdelay(2);
	} while (answer != ges_run && --retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
		goto fail;
	}

	ILI_INFO("Enter gesture successfully\n");

	ret = ili_ice_mode_ctrl(DISABLE, ON);
	if (ret < 0) {
		ILI_ERR("Disable ice mode failed during gesture recovery\n");
		goto fail;
	}

	ILI_INFO("Gesture code loaded by %s\n", ilits->gesture_load_code ? "driver" : "firmware");

	if (!ilits->gesture_load_code) {
		ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
		ili_set_tp_data_len(ilits->gesture_mode, false, NULL);
		goto out;
	}

	/* Load gesture code */
	ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	ili_set_tp_data_len(ilits->gesture_mode, false, NULL);
	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("Failed to load code during gesture recovery\n");
		goto fail;
	}

	/* Resume gesture loader */
	ret = ili_ic_func_ctrl("lpwg", 0x6);
	if (ret < 0) {
		ILI_ERR("write resume loader error");
		goto fail;
	}

out:
	return ret;

fail:
	ili_ice_mode_ctrl(DISABLE, ON);
	return ret;
}

void ili_demo_debug_info_id0(u8 *buf, size_t len)
{
	struct ili_demo_debug_info_id0 id0;

	ipio_memcpy(&id0, buf, sizeof(id0), len);
	ILI_INFO("id0 len = %d,strucy len = %d", (int)len, (int)sizeof(id0));

	ILI_INFO("id = %d\n", id0.id);
	ILI_INFO("app_sys_powr_state_e = %d\n", id0.sys_powr_state_e);
	ILI_INFO("app_sys_state_e = %d\n", id0.sys_state_e);
	ILI_INFO("tp_state_e = %d\n", id0.tp_state_e);

	ILI_INFO("touch_palm_state_e = %d\n", id0.touch_palm_state);
	ILI_INFO("app_an_statu_e = %d\n", id0.app_an_statu_e);
	ILI_INFO("app_sys_bg_err = %d\n", id0.app_sys_bg_err);
	ILI_INFO("g_b_wrong_bg = %d\n", id0.g_b_wrong_bg);
	ILI_INFO("reserved0 = %d\n", id0.reserved0);

	ILI_INFO("normal_mode = %d\n", id0.normal_mode);
	ILI_INFO("charger_mode = %d\n", id0.charger_mode);
	ILI_INFO("glove_mode = %d\n", id0.glove_mode);
	ILI_INFO("stylus_mode = %d\n", id0.stylus_mode);
	ILI_INFO("multi_mode = %d\n", id0.multi_mode);
	ILI_INFO("noise_mode = %d\n", id0.noise_mode);
	ILI_INFO("palm_plus_mode = %d\n", id0.palm_plus_mode);
	ILI_INFO("floating_mode = %d\n", id0.floating_mode);

	ILI_INFO("algo_pt_status0 = %d\n", id0.algo_pt_status0);
	ILI_INFO("algo_pt_status1 = %d\n", id0.algo_pt_status1);
	ILI_INFO("algo_pt_status2 = %d\n", id0.algo_pt_status2);
	ILI_INFO("algo_pt_status3 = %d\n", id0.algo_pt_status3);
	ILI_INFO("algo_pt_status4 = %d\n", id0.algo_pt_status4);
	ILI_INFO("algo_pt_status5 = %d\n", id0.algo_pt_status5);
	ILI_INFO("algo_pt_status6 = %d\n", id0.algo_pt_status6);
	ILI_INFO("algo_pt_status7 = %d\n", id0.algo_pt_status7);
	ILI_INFO("algo_pt_status8 = %d\n", id0.algo_pt_status8);
	ILI_INFO("algo_pt_status9 = %d\n", id0.algo_pt_status9);
	ILI_INFO("reserved2 = %d\n", id0.reserved2);

	ILI_INFO("hopping_flg = %d\n", id0.hopping_flg);
	ILI_INFO("hopping_index = %d\n", id0.hopping_index);
	ILI_INFO("frequency = %d\n", (id0.frequency_h << 8 | id0.frequency_l));
	ILI_INFO("reserved3 = %d\n", id0.reserved3);
	ILI_INFO("reserved4 = %d\n", id0.reserved4);

}

void ili_demo_debug_info_mode(u8 *buf, size_t len)
{
	u8 *info_ptr;
	u8 info_id, info_len;

	ili_report_ap_mode(buf, P5_X_DEMO_MODE_PACKET_LEN);
	info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN;
	info_len = info_ptr[0];
	info_id = info_ptr[1];

	ILI_INFO("info len = %d ,id = %d\n", info_len, info_id);

	ilits->demo_debug_info[info_id](&info_ptr[1], info_len);
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
	int index;

	mutex_lock(&ilits->debug_mutex);

	if (!ilits->netlink && !ilits->dnp)
		goto out;

	/* Send data to netlink */
	if (ilits->netlink) {
		ili_netlink_reply_msg(buf, len);
		goto out;
	}

	/* Sending data to apk via the node of debug_message node */
	if (ilits->dnp) {
		index = ilits->dbf;
		if (!ilits->dbl[ilits->dbf].mark) {
			ilits->dbf = ((ilits->dbf + 1) % TR_BUF_LIST_SIZE);
		} else {
			if (ilits->dbf == 0)
				index = TR_BUF_LIST_SIZE - 1;
			else
				index = ilits->dbf - 1;
		}
		if (ilits->dbl[index].data == NULL) {
			ILI_ERR("BUFFER %d error\n", index);
			goto out;
		}
		ipio_memcpy(ilits->dbl[index].data, buf, len, 2048);
		ilits->dbl[index].mark = true;
		wake_up(&(ilits->inq));
		goto out;
	}

out:
	mutex_unlock(&ilits->debug_mutex);
}

void ili_touch_press(u16 x, u16 y, u16 pressure, u16 id)
{
	ILI_DBG("Touch Press: id = %d, x = %d, y = %d, p = %d\n", id, x, y, pressure);

	if (MT_B_TYPE) {
		input_mt_slot(ilits->input, id);
		input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, true);
		input_report_abs(ilits->input, ABS_MT_POSITION_X, x);
		input_report_abs(ilits->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ilits->input, ABS_MT_PRESSURE, pressure);
	} else {
		input_report_key(ilits->input, BTN_TOUCH, 1);
		input_report_abs(ilits->input, ABS_MT_TRACKING_ID, id);
		input_report_abs(ilits->input, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(ilits->input, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(ilits->input, ABS_MT_POSITION_X, x);
		input_report_abs(ilits->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ilits->input, ABS_MT_PRESSURE, pressure);

		input_mt_sync(ilits->input);
	}
}

void ili_touch_release(u16 x, u16 y, u16 id)
{
	ILI_DBG("Touch Release: id = %d, x = %d, y = %d\n", id, x, y);

	if (MT_B_TYPE) {
		input_mt_slot(ilits->input, id);
		input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, false);
	} else {
		input_report_key(ilits->input, BTN_TOUCH, 0);
		input_mt_sync(ilits->input);
	}
}

void ili_touch_release_all_point(void)
{
	int i;

	if (MT_B_TYPE) {
		for (i = 0 ; i < MAX_TOUCH_NUM; i++)
			ili_touch_release(0, 0, i);

		input_report_key(ilits->input, BTN_TOUCH, 0);
		input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
	} else {
		ili_touch_release(0, 0, 0);
	}
	input_sync(ilits->input);
}

static struct ilitek_touch_info touch_info[MAX_TOUCH_NUM];

void ili_report_ap_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF)
			&& (buf[(4 * i) + 3] == 0xFF)) {
			if (MT_B_TYPE)
				ilits->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
		yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));

		if (ilits->trans_xy) {
			touch_info[ilits->finger].x = xop;
			touch_info[ilits->finger].y = yop;
		} else {
			touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
			touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
		}

		touch_info[ilits->finger].id = i;

		if (MT_PRESSURE)
			touch_info[ilits->finger].pressure = buf[(4 * i) + 4];
		else
			touch_info[ilits->finger].pressure = 1;

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ilits->finger++;
		if (MT_B_TYPE)
			ilits->curt_touch[i] = 1;
	}

#ifdef ROI
	ili_read_knuckle_roi_data();
#endif
	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ilits->finger; i++) {
				input_report_key(ilits->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ilits->prev_touch[i] = ilits->curt_touch[i];
			}
		} else {
			for (i = 0; i < ilits->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ilits->input);
		ilits->last_touch = ilits->finger;
	} else {
		if (ilits->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
				input_report_key(ilits->input, BTN_TOUCH, 0);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ilits->input);
			ilits->last_touch = 0;
		}
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_debug_mode_report_point(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	static u8 p[MAX_TOUCH_NUM];

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(3 * i)] == 0xFF) && (buf[(3 * i) + 1] == 0xFF)
			&& (buf[(3 * i) + 2] == 0xFF)) {
			if (MT_B_TYPE)
				ilits->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(3 * i)] & 0xF0) << 4) | (buf[(3 * i) + 1]));
		yop = (((buf[(3 * i)] & 0x0F) << 8) | (buf[(3 * i) + 2]));

		if (ilits->trans_xy) {
			touch_info[ilits->finger].x = xop;
			touch_info[ilits->finger].y = yop;
		} else {
			touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
			touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
		}

		touch_info[ilits->finger].id = i;

		if (MT_PRESSURE) {
			/*
			* Since there's no pressure data in debug mode, we make fake values
			* for android system if pressure needs to be reported.
			*/
			if (p[ilits->finger] == 1)
				touch_info[ilits->finger].pressure = p[ilits->finger] = 2;
			else
				touch_info[ilits->finger].pressure = p[ilits->finger] = 1;
		} else {
			touch_info[ilits->finger].pressure = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ilits->finger++;
		if (MT_B_TYPE)
			ilits->curt_touch[i] = 1;
	}

	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ilits->finger; i++) {
				input_report_key(ilits->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ilits->prev_touch[i] = ilits->curt_touch[i];
			}
		} else {
			for (i = 0; i < ilits->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ilits->input);
		ilits->last_touch = ilits->finger;
	} else {
		if (ilits->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
				input_report_key(ilits->input, BTN_TOUCH, 0);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ilits->input);
			ilits->last_touch = 0;
		}
	}
}

void ili_report_debug_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf + 5, len);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_debug_lite_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf + 4, len);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

#if PROXIMITY_BY_FW
static int ili_resume_cmd(void)
{
	int ret = 0;

	ret = ili_ic_func_ctrl("sleep", 0x01);
	if (ret < 0) {
		ILI_ERR("Write sleep cmd failed\n");
	}

	mdelay(50);
	ili_ic_set_ddi_reg_onepage(0x00, PAGE00_CMD11_SLEEPOUT, 0x00, OFF);
	mdelay(80);
	ili_ic_set_ddi_reg_onepage(0x00, PAGE00_CMD29_DISPLAYON, 0x00, OFF);
	mdelay(20);
	/*video start*/
	mdelay(35);

	ret = ili_ic_func_ctrl("sense", 0x01);
	if (ret < 0) {
		ILI_ERR("Write sense cmd failed\n");
	}
	return ret;
}

static int ili_suspend_cmd(void)
{
	int ret = 0;

	mdelay(10);
	ili_ic_set_ddi_reg_onepage(0x00, PAGE00_CMD28_DISPLAYOFF, 0x00, OFF);
	mdelay(20);
	ili_ic_set_ddi_reg_onepage(0x00, PAGE00_CMD10_SLEEPIN, 0x00, OFF);
	mdelay(80);
	/*VideoStop*/
	mdelay(35);

	return ret;
}

void ili_report_proximity_mode(u8 *buf, int len)
{
	ilits->proxmity_face = 0;

	if (!ilits->prox_face_mode) {
		ILI_ERR("proximity face mode Error\n");
		return;
	}

	atomic_set(&ilits->tp_sleep, START);

	ili_irq_disable();

	ilits->proxmity_face = buf[1];
	ILI_DBG("TP mode (0x%x)\n", ilits->actual_tp_mode);
	ILI_DBG("proximity %s, cmd : %X\n", ilits->proxmity_face ? "Near" : "Far", buf[1]);
	ILI_DBG("power_status %s\n", ilits->power_status ? "true" : "false");
	if ((ilits->actual_tp_mode == P5_X_FW_AP_MODE) && (ilits->proxmity_face == true) && (ilits->power_status == true)) {

		ilits->tp_suspend = true;
		ilits->power_status = false;

		ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);

		ili_suspend_cmd();

		enable_irq_wake(ilits->irq_num);

		ilits->pll_clk_wakeup = true;
	} else if ((ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) && (ilits->proxmity_face == false) && (ilits->power_status == false)) {

		if (ilits->gesture)
			disable_irq_wake(ilits->irq_num);
		ilits->actual_tp_mode = P5_X_FW_AP_MODE;

		ili_resume_cmd();

		ilits->tp_data_format = DATA_FORMAT_DEMO;
		ilits->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;

		ilits->tp_suspend = false;
		ilits->pll_clk_wakeup = false;
		ilits->power_status = true;
	}
	ili_irq_enable();
	ili_touch_release_all_point();
	atomic_set(&ilits->tp_sleep, END);

}
#endif

void ili_report_gesture_mode(u8 *buf, int len)
{
	int i, lu_x = 0, lu_y = 0, rd_x = 0, rd_y = 0, score = 0;
	u8 ges[P5_X_GESTURE_INFO_LENGTH] = {0};
	struct gesture_coordinate *gc = ilits->gcoord;
	struct input_dev *input = ilits->input;
	bool transfer = ilits->trans_xy;

	for (i = 0; i < len; i++)
		ges[i] = buf[i];

	memset(gc, 0x0, sizeof(struct gesture_coordinate));

	gc->code = ges[1];
	score = ges[36];
	ILI_INFO("gesture code = 0x%x, score = %d\n", gc->code, score);

	/* Parsing gesture coordinate */
	gc->pos_start.x = ((ges[4] & 0xF0) << 4) | ges[5];
	gc->pos_start.y = ((ges[4] & 0x0F) << 8) | ges[6];
	gc->pos_end.x   = ((ges[7] & 0xF0) << 4) | ges[8];
	gc->pos_end.y   = ((ges[7] & 0x0F) << 8) | ges[9];
	gc->pos_1st.x   = ((ges[16] & 0xF0) << 4) | ges[17];
	gc->pos_1st.y   = ((ges[16] & 0x0F) << 8) | ges[18];
	gc->pos_2nd.x   = ((ges[19] & 0xF0) << 4) | ges[20];
	gc->pos_2nd.y   = ((ges[19] & 0x0F) << 8) | ges[21];
	gc->pos_3rd.x   = ((ges[22] & 0xF0) << 4) | ges[23];
	gc->pos_3rd.y   = ((ges[22] & 0x0F) << 8) | ges[24];
	gc->pos_4th.x   = ((ges[25] & 0xF0) << 4) | ges[26];
	gc->pos_4th.y   = ((ges[25] & 0x0F) << 8) | ges[27];

	switch (gc->code) {
	case GESTURE_DOUBLECLICK:
		ILI_INFO("Double Click key event\n");
		input_report_key(input, KEY_GESTURE_POWER, 1);
		input_sync(input);
		input_report_key(input, KEY_GESTURE_POWER, 0);
		input_sync(input);
		gc->type  = GESTURE_DOUBLECLICK;
		gc->clockwise = 1;
		gc->pos_end.x = gc->pos_start.x;
		gc->pos_end.y = gc->pos_start.y;
		break;
	case GESTURE_LEFT:
		gc->type  = GESTURE_LEFT;
		gc->clockwise = 1;
		break;
	case GESTURE_RIGHT:
		gc->type  = GESTURE_RIGHT;
		gc->clockwise = 1;
		break;
	case GESTURE_UP:
		gc->type  = GESTURE_UP;
		gc->clockwise = 1;
		break;
	case GESTURE_DOWN:
		gc->type  = GESTURE_DOWN;
		gc->clockwise = 1;
		break;
	case GESTURE_O:
		gc->type  = GESTURE_O;
		gc->clockwise = (ges[34] > 1) ? 0 : ges[34];

		lu_x = (((ges[28] & 0xF0) << 4) | (ges[29]));
		lu_y = (((ges[28] & 0x0F) << 8) | (ges[30]));
		rd_x = (((ges[31] & 0xF0) << 4) | (ges[32]));
		rd_y = (((ges[31] & 0x0F) << 8) | (ges[33]));

		gc->pos_1st.x = ((rd_x + lu_x) / 2);
		gc->pos_1st.y = lu_y;
		gc->pos_2nd.x = lu_x;
		gc->pos_2nd.y = ((rd_y + lu_y) / 2);
		gc->pos_3rd.x = ((rd_x + lu_x) / 2);
		gc->pos_3rd.y = rd_y;
		gc->pos_4th.x = rd_x;
		gc->pos_4th.y = ((rd_y + lu_y) / 2);
		break;
	case GESTURE_W:
		gc->type  = GESTURE_W;
		gc->clockwise = 1;
		break;
	case GESTURE_M:
		gc->type  = GESTURE_M;
		gc->clockwise = 1;
		break;
	case GESTURE_V:
		gc->type  = GESTURE_V;
		gc->clockwise = 1;
		break;
	case GESTURE_C:
		gc->type  = GESTURE_C;
		gc->clockwise = 1;
		break;
	case GESTURE_E:
		gc->type  = GESTURE_E;
		gc->clockwise = 1;
		break;
	case GESTURE_S:
		gc->type  = GESTURE_S;
		gc->clockwise = 1;
		break;
	case GESTURE_Z:
		gc->type  = GESTURE_Z;
		gc->clockwise = 1;
		break;
	case GESTURE_TWOLINE_DOWN:
		gc->type  = GESTURE_TWOLINE_DOWN;
		gc->clockwise = 1;
		gc->pos_1st.x  = (((ges[10] & 0xF0) << 4) | (ges[11]));
		gc->pos_1st.y  = (((ges[10] & 0x0F) << 8) | (ges[12]));
		gc->pos_2nd.x  = (((ges[13] & 0xF0) << 4) | (ges[14]));
		gc->pos_2nd.y  = (((ges[13] & 0x0F) << 8) | (ges[15]));
		break;
	default:
		ILI_ERR("Unknown gesture code\n");
		break;
	}

	if (!transfer) {
		gc->pos_start.x	= gc->pos_start.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_start.y = gc->pos_start.y * ilits->panel_hei / TPD_HEIGHT;
		gc->pos_end.x   = gc->pos_end.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_end.y   = gc->pos_end.y * ilits->panel_hei / TPD_HEIGHT;
		gc->pos_1st.x   = gc->pos_1st.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_1st.y   = gc->pos_1st.y * ilits->panel_hei / TPD_HEIGHT;
		gc->pos_2nd.x   = gc->pos_2nd.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_2nd.y   = gc->pos_2nd.y * ilits->panel_hei / TPD_HEIGHT;
		gc->pos_3rd.x   = gc->pos_3rd.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_3rd.y   = gc->pos_3rd.y * ilits->panel_hei / TPD_HEIGHT;
		gc->pos_4th.x   = gc->pos_4th.x * ilits->panel_wid / TPD_WIDTH;
		gc->pos_4th.y   = gc->pos_4th.y * ilits->panel_hei / TPD_HEIGHT;
	}

	ILI_INFO("Transfer = %d, Type = %d, clockwise = %d\n", transfer, gc->type, gc->clockwise);
	ILI_INFO("Gesture Points: (%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n",
			gc->pos_start.x, gc->pos_start.y,
			gc->pos_end.x, gc->pos_end.y,
			gc->pos_1st.x, gc->pos_1st.y,
			gc->pos_2nd.x, gc->pos_2nd.y,
			gc->pos_3rd.x, gc->pos_3rd.y,
			gc->pos_4th.x, gc->pos_4th.y);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_i2cuart_mode(u8 *buf, int len)
{
	int type = buf[3] & 0x0F;
	int need_read_len = 0, one_data_bytes = 0;
	int actual_len = len - 5;
	int uart_len;
	u8 *uart_buf = NULL, *total_buf = NULL;

	ILI_DBG("data[3] = %x, type = %x, actual_len = %d\n",
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
	ILI_DBG("need_read_len = %d  one_data_bytes = %d\n", need_read_len, one_data_bytes);

	if (need_read_len <= actual_len) {
		ilitek_tddi_touch_send_debug_data(buf, len);
		goto out;
	}

	uart_len = need_read_len - actual_len;
	ILI_DBG("uart len = %d\n", uart_len);

	uart_buf = kcalloc(uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(uart_buf)) {
		ILI_ERR("Failed to allocate uart_buf memory %ld\n", PTR_ERR(uart_buf));
		goto out;
	}

	if (ilits->wrapper(NULL, 0, uart_buf, uart_len, OFF, OFF) < 0) {
		ILI_ERR("i2cuart read data failed\n");
		goto out;
	}

	total_buf = kcalloc(len + uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(total_buf)) {
		ILI_ERR("Failed to allocate total_buf memory %ld\n", PTR_ERR(total_buf));
		goto out;
	}

	memcpy(total_buf, buf, len);
	memcpy(total_buf + len, uart_buf, uart_len);
	ilitek_tddi_touch_send_debug_data(total_buf, len + uart_len);

out:
	ipio_kfree((void **)&uart_buf);
	ipio_kfree((void **)&total_buf);
	return;
}

#ifdef ROI
int ili_read_knuckle_roi_data(void)
{
	int i = 0;
	int ret = 0;
	struct ts_kit_device_data *ts_dev_data = ilits->ts_dev_data;

	ILI_DBG("roi_switch = %d, roi_support =%d\n",
		ts_dev_data->ts_platform_data->feature_info.roi_info.roi_switch,
		ts_dev_data->ts_platform_data->feature_info.roi_info.roi_supported);

	ILI_INFO("last_fingers = %d, cur_fingers =%d\n",
		ilits->last_touch,
		ilits->finger);

	if (ts_dev_data->ts_platform_data->feature_info.roi_info.roi_switch &&
		ts_dev_data->ts_platform_data->feature_info.roi_info.roi_supported) {
		if (ilits->last_touch != ilits->finger &&
		ilits->finger <= ILITEK_KNUCKLE_ROI_FINGERS) {
			ret = ili_config_knuckle_roi_ctrl(CMD_ROI_DATA);
			if (ret) {
				ILI_ERR("write data failed, ret = %d\n", ret);
				return ret;
			}
			mdelay(1);
			ret = ilits->wrapper(NULL, 0, ilits->knuckle_roi_data, ROI_DATA_READ_LENGTH, OFF, OFF);
			if (ret) {
				ILI_ERR("read data failed, ret = %d\n", ret);
				return ret;
			}

			ILI_DBG("index = %d, fixed = %d, peak_row = %d, peak_colum = %d\n",
				ilits->knuckle_roi_data[0],
				ilits->knuckle_roi_data[1],
				ilits->knuckle_roi_data[2],
				ilits->knuckle_roi_data[3]);
			if (debug_en) {
				ILI_INFO("=================== roi data in bytes ===================\n");
				for (i = 4; i < ROI_DATA_READ_LENGTH; i++) {
				ILI_INFO(KERN_CONT "%3d ", ilits->knuckle_roi_data[i]);
					if ((i - 3) % 14 == 0) { /* 14 * 14  rawdata bytes */
						ILI_INFO("\n");
					}
				}
			}
		}
	}
	return 0;
}
#endif