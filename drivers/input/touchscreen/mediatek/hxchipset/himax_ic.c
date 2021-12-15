/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "himax_ic.h"

static int iref_number = 11;
static bool iref_found;

#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
uint8_t test_counter;
uint8_t TEST_DATA_TIMES = 3;
#endif

/* 1uA */
static unsigned char E_IrefTable_1[16][2] = {
	{0x20, 0x0F}, {0x20, 0x1F}, {0x20, 0x2F}, {0x20, 0x3F},
	{0x20, 0x4F}, {0x20, 0x5F}, {0x20, 0x6F}, {0x20, 0x7F},
	{0x20, 0x8F}, {0x20, 0x9F}, {0x20, 0xAF}, {0x20, 0xBF},
	{0x20, 0xCF}, {0x20, 0xDF}, {0x20, 0xEF}, {0x20, 0xFF} };

/* 2uA */
static unsigned char E_IrefTable_2[16][2] = {
	{0xA0, 0x0E}, {0xA0, 0x1E}, {0xA0, 0x2E}, {0xA0, 0x3E},
	{0xA0, 0x4E}, {0xA0, 0x5E}, {0xA0, 0x6E}, {0xA0, 0x7E},
	{0xA0, 0x8E}, {0xA0, 0x9E}, {0xA0, 0xAE}, {0xA0, 0xBE},
	{0xA0, 0xCE}, {0xA0, 0xDE}, {0xA0, 0xEE}, {0xA0, 0xFE} };

/* 3uA */
static unsigned char E_IrefTable_3[16][2] = {
	{0x20, 0x0E}, {0x20, 0x1E}, {0x20, 0x2E}, {0x20, 0x3E},
	{0x20, 0x4E}, {0x20, 0x5E}, {0x20, 0x6E}, {0x20, 0x7E},
	{0x20, 0x8E}, {0x20, 0x9E}, {0x20, 0xAE}, {0x20, 0xBE},
	{0x20, 0xCE}, {0x20, 0xDE}, {0x20, 0xEE}, {0x20, 0xFE} };

/* 4uA */
static unsigned char E_IrefTable_4[16][2] = {
	{0xA0, 0x0D}, {0xA0, 0x1D}, {0xA0, 0x2D}, {0xA0, 0x3D},
	{0xA0, 0x4D}, {0xA0, 0x5D}, {0xA0, 0x6D}, {0xA0, 0x7D},
	{0xA0, 0x8D}, {0xA0, 0x9D}, {0xA0, 0xAD}, {0xA0, 0xBD},
	{0xA0, 0xCD}, {0xA0, 0xDD}, {0xA0, 0xED}, {0xA0, 0xFD} };

/* 5uA */
static unsigned char E_IrefTable_5[16][2] = {
	{0x20, 0x0D}, {0x20, 0x1D}, {0x20, 0x2D}, {0x20, 0x3D},
	{0x20, 0x4D}, {0x20, 0x5D}, {0x20, 0x6D}, {0x20, 0x7D},
	{0x20, 0x8D}, {0x20, 0x9D}, {0x20, 0xAD}, {0x20, 0xBD},
	{0x20, 0xCD}, {0x20, 0xDD}, {0x20, 0xED}, {0x20, 0xFD} };

/* 6uA */
static unsigned char E_IrefTable_6[16][2] = {
	{0xA0, 0x0C}, {0xA0, 0x1C}, {0xA0, 0x2C}, {0xA0, 0x3C},
	{0xA0, 0x4C}, {0xA0, 0x5C}, {0xA0, 0x6C}, {0xA0, 0x7C},
	{0xA0, 0x8C}, {0xA0, 0x9C}, {0xA0, 0xAC}, {0xA0, 0xBC},
	{0xA0, 0xCC}, {0xA0, 0xDC}, {0xA0, 0xEC}, {0xA0, 0xFC} };

/* 7uA */
static unsigned char E_IrefTable_7[16][2] = {
	{0x20, 0x0C}, {0x20, 0x1C}, {0x20, 0x2C}, {0x20, 0x3C},
	{0x20, 0x4C}, {0x20, 0x5C}, {0x20, 0x6C}, {0x20, 0x7C},
	{0x20, 0x8C}, {0x20, 0x9C}, {0x20, 0xAC}, {0x20, 0xBC},
	{0x20, 0xCC}, {0x20, 0xDC}, {0x20, 0xEC}, {0x20, 0xFC} };

#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
static int Selftest_flag;
uint16_t *mutual_bank;
uint16_t *self_bank;

bool raw_data_chk_arr[20] = {false};
#endif

#ifdef HX_TP_PROC_2T2R
bool Is_2T2R;
int HX_2T2R_Addr = 0x96;       /* Need to check with project FW eng. */
int HX_2T2R_en_setting = 0x02; /* Need to check with project FW eng. */
#endif

#if defined(HX_ESD_RECOVERY)
u8 ESD_R36_FAIL;

#endif

uint8_t IC_STATUS_CHECK = 0xAA;
int himax_touch_data_size = 128;

int himax_get_touch_data_size(void)
{
	return himax_touch_data_size;
}

#ifdef HX_RST_PIN_FUNC

void himax_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);

	himax_rst_gpio_set(private_ts->rst_gpio, 0);
	msleep(20);
	himax_rst_gpio_set(private_ts->rst_gpio, 1);
	msleep(20);
}

void himax_reload_config(void)
{
	himax_loadSensorConfig(private_ts->client, private_ts->pdata);
	himax_power_on_init(private_ts->client);
	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);
	;
	calculate_point_number();
}

void himax_irq_switch(int switch_on)
{
	int ret = 0;

	if (switch_on) {

		if (private_ts->use_irq)
			himax_int_enable(private_ts->client->irq, switch_on);
		else
			hrtimer_start(&private_ts->timer, ktime_set(1, 0),
				      HRTIMER_MODE_REL);
	} else {
		if (private_ts->use_irq)
			himax_int_enable(private_ts->client->irq, switch_on);
		else {
			hrtimer_cancel(&private_ts->timer);
			ret = cancel_work_sync(&private_ts->work);
		}
	}
}

void himax_ic_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;

	HX_HW_RESET_ACTIVATE = 1;

	I("%s,status: loadconfig=%d,int_off=%d\n", __func__, loadconfig,
	  int_off);

	if (ts->rst_gpio >= 0) {
		if (int_off)
			himax_irq_switch(0);

		himax_pin_reset();
		if (loadconfig)
			himax_reload_config();

		if (int_off)
			himax_irq_switch(1);
	}
}
#endif

#if defined(HX_ESD_RECOVERY)
int g_zero_event_count;

int himax_ic_esd_recovery(int hx_esd_event, int hx_zero_event, int length)
{
	int shaking_ret = 0;

	/* hand shaking status: 0:Running, 1:Stop, 2:I2C Fail */
	shaking_ret = himax_hand_shaking(private_ts->client);
	if (shaking_ret == 2) {
		I("[HIMAX TP MSG]: I2C Fail.\n");
		goto err_workqueue_out;
	}
	if (hx_esd_event == length) {
		goto checksum_fail;
	} else if (shaking_ret == 1 && hx_zero_event == length) {
		I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
		goto checksum_fail;
	} else
		goto workqueue_out;

checksum_fail:
	return CHECKSUM_FAIL;
err_workqueue_out:
	return ERR_WORK_OUT;
workqueue_out:
	return WORK_OUT;
}

void himax_esd_ic_reset(void)
{
	uint8_t read_R36[2] = {0};

	while (ESD_R36_FAIL <= 3) {
		HX_ESD_RESET_ACTIVATE = 1;
#ifdef HX_RST_PIN_FUNC
		himax_pin_reset();
#endif
		if (himax_loadSensorConfig(private_ts->client,
					   private_ts->pdata) < 0) {
			ESD_R36_FAIL++;
			continue;
		}
		if (i2c_himax_read(private_ts->client, 0x36, read_R36, 2, 10) <
		    0) {
			ESD_R36_FAIL++;
			continue;
		}
		if (read_R36[0] != 0x0F || read_R36[1] != 0x53) {
			E("%s R36 Fail : R36[0]=%d,R36[1]=%d,R36 Counter=%d\n",
			  __func__, read_R36[0], read_R36[1], ESD_R36_FAIL);
			ESD_R36_FAIL++;
		} else {
			himax_power_on_init(private_ts->client);
			if (himax_report_data_init())
				E("%s: allocate data fail\n", __func__);
			;
			calculate_point_number();
			break;
		}
	}
	/* reset status */
	ESD_R36_FAIL = 0;
}
#endif

int himax_hand_shaking(
	struct i2c_client *client) /* 0:Running, 1:Stop, 2:I2C Fail */
{
	int ret, result;
	uint8_t hw_reset_check[1];
	uint8_t hw_reset_check_2[1];
	uint8_t buf0[2];

	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));
	memset(hw_reset_check_2, 0x00, sizeof(hw_reset_check_2));

	buf0[0] = 0xF2;
	if (IC_STATUS_CHECK == 0xAA) {
		buf0[1] = 0xAA;
		IC_STATUS_CHECK = 0x55;
	} else {
		buf0[1] = 0x55;
		IC_STATUS_CHECK = 0xAA;
	}

	ret = i2c_himax_master_write(client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0xF2 failed line: %d\n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	msleep(50);

	buf0[0] = 0xF2;
	buf0[1] = 0x00;
	ret = i2c_himax_master_write(client, buf0, 2, DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:write 0x92 failed line: %d\n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}
	msleep(20);

	ret = i2c_himax_read(client, 0x90, hw_reset_check, 1,
			     DEFAULT_RETRY_CNT);
	if (ret < 0) {
		E("[Himax]:i2c_himax_read 0x90 failed line: %d\n", __LINE__);
		goto work_func_send_i2c_msg_fail;
	}

	if (hw_reset_check[0] != IC_STATUS_CHECK) {
		msleep(20);
		ret = i2c_himax_read(client, 0x90, hw_reset_check_2, 1,
				     DEFAULT_RETRY_CNT);
		if (ret < 0) {
			E("[Himax]:i2c_himax_read 0x90 failed line: %d\n",
			  __LINE__);
			goto work_func_send_i2c_msg_fail;
		}

		if (hw_reset_check[0] == hw_reset_check_2[0])
			result = 1;
		else
			result = 0;

	} else {
		result = 0;
	}

	return result;

work_func_send_i2c_msg_fail:
	return 2;
}

void himax_idle_mode(struct i2c_client *client, int disable)
{
}

int himax_determin_diag_rawdata(int diag_command)
{
	return (diag_command / 10 > 0) ? 0 : diag_command % 10;
}

int himax_determin_diag_storage(int diag_command)
{
	return 0;
}

int himax_switch_mode(struct i2c_client *client, int mode)
{
	return 1;
}

void himax_return_event_stack(struct i2c_client *client)
{
}

void himax_diag_register_set(struct i2c_client *client, uint8_t diag_command)
{
	uint8_t command_F1h[2] = {0xF1, 0x00};

	/* diag_command = diag_command - '0'; */
	if (diag_command > 8) {
		E("[Himax]Diag command error!diag_command=0x%x\n",
		  diag_command);
		return;
	}
	command_F1h[1] = diag_command;

	i2c_himax_write(client, command_F1h[0], &command_F1h[1], 1,
			DEFAULT_RETRY_CNT);
}

void himax_flash_dump_func(struct i2c_client *client,
			   uint8_t local_flash_command, int Flash_Size,
			   uint8_t *flash_buffer)
{
	int i = 0, j = 0, k = 0, l = 0;
	int buffer_ptr = 0;
	uint8_t x81_command[2] = {HX_CMD_TSSLPOUT, 0x00};
	uint8_t x82_command[2] = {HX_CMD_TSSOFF, 0x00};
	uint8_t x43_command[4] = {HX_CMD_FLASH_ENABLE, 0x00, 0x00, 0x00};
	uint8_t x44_command[4] = {HX_CMD_FLASH_SET_ADDRESS, 0x00, 0x00, 0x00};
	uint8_t x59_tmp[4] = {0, 0, 0, 0};
	uint8_t page_tmp[128];

	himax_int_enable(client->irq, 0);

#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
	cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
#endif

	setFlashDumpGoing(true);

	local_flash_command = getFlashCommand();
#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(false, false);
#endif

	if (i2c_himax_master_write(client, x81_command, 1, 3) <
	    0) { /* sleep out */

		E("%s i2c write 81 fail.\n", __func__);
		goto Flash_Dump_i2c_transfer_error;
	}
	msleep(120);
	if (i2c_himax_master_write(client, x82_command, 1, 3) < 0) {
		E("%s i2c write 82 fail.\n", __func__);
		goto Flash_Dump_i2c_transfer_error;
	}
	msleep(100);

	x43_command[1] = 0x01;
	if (i2c_himax_write(client, x43_command[0], &x43_command[1], 1,
			    DEFAULT_RETRY_CNT) < 0) {
		goto Flash_Dump_i2c_transfer_error;
	}
	msleep(100);

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 32; j++) {
			/* I(" Step 2 i=%d , j=%d %s\n",i,j,__func__); */
			/* read page start */
			for (k = 0; k < 128; k++)
				page_tmp[k] = 0x00;

			for (k = 0; k < 32; k++) {
				x44_command[1] = k;
				x44_command[2] = j;
				x44_command[3] = i;
				if (i2c_himax_write(client, x44_command[0],
						    &x44_command[1], 3,
						    DEFAULT_RETRY_CNT) < 0) {
					E("%s i2c write 44 fail.\n", __func__);
					goto Flash_Dump_i2c_transfer_error;
				}
				if (i2c_himax_write_command(client, 0x46,
							    DEFAULT_RETRY_CNT) <
				    0) {
					E("%s i2c write 46 fail.\n", __func__);
					goto Flash_Dump_i2c_transfer_error;
				}
				/* msleep(20); */
				if (i2c_himax_read(client, 0x59, x59_tmp, 4,
						   DEFAULT_RETRY_CNT) < 0) {
					E("%s i2c write 59 fail.\n", __func__);
					goto Flash_Dump_i2c_transfer_error;
				}
				/* msleep(20); */
				for (l = 0; l < 4; l++)
					page_tmp[k * 4 + l] = x59_tmp[l];

				/* msleep(20); */
			}
			/* read page end */

			for (k = 0; k < 128; k++)
				flash_buffer[buffer_ptr++] = page_tmp[k];

			setFlashDumpProgress(i * 32 + j);
		}
	}

	I("Complete~~~~~~~~~~~~~~~~~~~~~~~\n");
	if (local_flash_command == 0x01) {
		I(" buffer_ptr = %d\n", buffer_ptr);

		for (i = 0; i < buffer_ptr; i++) {
			I("%2.2X ", flash_buffer[i]);
			if ((i % 16) == 15)
				I("\n");
		}
		I("End~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	} else if (local_flash_command == 2) {
		struct file *fn;

		fn = filp_open(FLASH_DUMP_FILE, O_CREAT | O_WRONLY, 0);
		if (!IS_ERR(fn)) {
			fn->f_op->write(fn, flash_buffer,
					buffer_ptr * sizeof(uint8_t),
					&fn->f_pos);
			filp_close(fn, NULL);
		}
	}

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(true, false);
#endif

	himax_int_enable(client->irq, 1);
#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(private_ts->himax_chip_monitor_wq,
			   &private_ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMES * HZ);
#endif
	setFlashDumpGoing(false);

	setFlashDumpComplete(1);
	setSysOperation(0);
	return;

Flash_Dump_i2c_transfer_error:

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(true, false);
#endif

	himax_int_enable(client->irq, 1);
#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(private_ts->himax_chip_monitor_wq,
			   &private_ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMES * HZ);
#endif
	setFlashDumpGoing(false);
	setFlashDumpComplete(0);
	setFlashDumpFail(1);
	setSysOperation(0);
}

#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
void himax_get_raw_data(uint8_t diag_command, uint16_t mutual_num,
			uint16_t self_num)
{
	uint8_t command_F1h_bank[2] = {0xF1, diag_command};
	int i = 0;

	I("Start get raw data\n");

	memset(mutual_bank, 0xFF, mutual_num * sizeof(uint16_t));
	memset(self_bank, 0xFF, self_num * sizeof(uint16_t));
	Selftest_flag = 1;
	I(" mutual_num = %d, self_num = %d\n", mutual_num, self_num);
	I(" Selftest_flag = %d\n", Selftest_flag);

	i2c_himax_write(private_ts->client, command_F1h_bank[0],
			&command_F1h_bank[1], 1, DEFAULT_RETRY_CNT);
	msleep(100);

	himax_int_enable(private_ts->client->irq, 1);
	for (i = 0; i < 100; i++) { /* check for max 1000 times */

		I(" conuter = %d ,Selftest_flag = %d\n", i, Selftest_flag);
		if (Selftest_flag == 0)
			break;
		msleep(20);
	}

	himax_int_enable(private_ts->client->irq, 0);
	I(" Write diag cmd = %d\n", command_F1h_bank[1]);
}
#endif
#ifdef HX_TP_SELF_TEST_DRIVER
static uint8_t Self_Test_Bank(uint8_t *RB1H)
{
	uint8_t i;
	uint8_t x_channel = 0;
	uint8_t y_channel = 0;
	uint16_t mutual_num = 0;
	uint16_t self_num = 0;
	uint32_t bank_sum, m;
	uint8_t bank_avg;
	uint8_t bank_ulmt, bank_dlmt;
	uint8_t bank_min, bank_max;
	uint8_t slf_tx_fail_cnt, slf_rx_fail_cnt;
	uint8_t mut_fail_cnt;
	int fail_flag;
	uint8_t bank_val_tmp;

	uint8_t set_bnk_ulmt;
	uint8_t set_bnk_dlmt;
	uint8_t set_avg_bnk_ulmt;
	uint8_t set_avg_bnk_dlmt;
	uint8_t set_slf_bnk_ulmt;
	uint8_t set_slf_bnk_dlmt;

	I("Start self_test\n");
	x_channel = ic_data->HX_RX_NUM;
	y_channel = ic_data->HX_TX_NUM;
	mutual_num = x_channel * y_channel;
	self_num = x_channel + y_channel; /* don't add KEY_COUNT */

	mutual_bank = kcalloc(mutual_num, sizeof(uint16_t), GFP_KERNEL);
	self_bank = kcalloc(self_num, sizeof(uint16_t), GFP_KERNEL);
	himax_get_raw_data(0x03, mutual_num, self_num); /* get bank data */
	if (RB1H[0] == 0x80) {
		I(" Enter Test flow\n");
		set_bnk_ulmt = RB1H[2];
		set_bnk_dlmt = RB1H[3];
		set_avg_bnk_ulmt = RB1H[4];
		set_avg_bnk_dlmt = RB1H[5];
		set_slf_bnk_ulmt =
			RB1H[6]; /* Increase @ 2012/05/24 for weak open/short */
		set_slf_bnk_dlmt = RB1H[7];

		fail_flag = 0;
		bank_sum = 0;
		bank_avg = 0;
		mut_fail_cnt = 0;

		/* Calculate Bank Average */
		for (m = 0; m < mutual_num; m++)
			bank_sum += mutual_bank[m];

		I(" bank_sum = %d\n", bank_sum);
		bank_avg = (bank_sum / mutual_num);
		I(" bank_avg = %d\n", bank_avg);
		/* ======Condition 1======Check average bank with absolute value
		 */
		if ((bank_avg > set_avg_bnk_ulmt) ||
		    (bank_avg < set_avg_bnk_dlmt))
			fail_flag = 1;
		I(" fail_flag = %d\n", fail_flag);
		if (fail_flag) {
			RB1H[0] = 0xF1; /* Fail ID for Condition 1 */
			RB1H[1] = bank_avg;
			RB1H[2] = set_avg_bnk_ulmt;
			RB1H[3] = set_avg_bnk_dlmt;
			RB1H[4] = 0xFF;
			for (i = 0; i < 8; i++)
				I(" RB1H[%d] = %X\n", i, RB1H[i]);

		} else {
/* ======Condition 2======Check every block's bank with average value */
#if 1 /* def SLF_TEST_BANK_ABS_LMT */
			bank_ulmt = set_bnk_ulmt;
			bank_dlmt = set_bnk_dlmt;
#else
			if ((bank_avg + set_bnk_ulmt) > 245)
				bank_ulmt = 245;
			else
				bank_ulmt = bank_avg + set_bnk_ulmt;

			if (bank_avg > set_bnk_dlmt) {
				bank_dlmt = bank_avg - set_bnk_dlmt;
				if (bank_dlmt < 10)
					bank_dlmt = 10;
			} else
				bank_dlmt = 10;
#endif

			bank_min = 0xFF;
			bank_max = 0x00;
			I(" bank_ulmt = %d, bank_dlmt = %d\n", bank_ulmt,
			  bank_dlmt);
			for (m = 0; m < mutual_num; m++) {
				bank_val_tmp = mutual_bank[m];
				if ((bank_val_tmp > bank_ulmt) ||
				    (bank_val_tmp < bank_dlmt)) {
					fail_flag = 1;
					mut_fail_cnt++;
				}

				/* Bank information record */
				if (bank_val_tmp > bank_max)
					bank_max = bank_val_tmp;
				else if (bank_val_tmp < bank_min)
					bank_min = bank_val_tmp;
			}
			I(" fail_flag = %d, mut_fail_cnt = %d\n", fail_flag,
			  mut_fail_cnt);
			if (fail_flag) {
				RB1H[0] = 0xF2; /* Fail ID for Condition 2 */
				RB1H[1] = mut_fail_cnt;
				RB1H[2] = bank_avg;
				RB1H[3] = bank_max;
				RB1H[4] = bank_min;
				RB1H[5] = bank_ulmt;
				RB1H[6] = bank_dlmt;
				RB1H[7] = 0xFF;
				for (i = 0; i < 8; i++)
					I(" RB1H[%d] = %X\n", i, RB1H[i]);

				for (m = 0; m < mutual_num; m++) {
					I(" mutual_bank[%d] = %X\n", m,
					  mutual_bank[m]);
				}
				for (m = 0; m < self_num; m++) {
					I(" self_bank[%d] = %X\n", m,
					  self_bank[m]);
				}
			} else {
				/* ======Condition 3======Check every self */
				/* channel bank */
				slf_rx_fail_cnt = 0x00; /* Check SELF RX BANK */
				slf_tx_fail_cnt = 0x00; /* Check SELF TX BANK */
				for (i = 0; i < (x_channel + y_channel); i++) {
					bank_val_tmp = self_bank[i];
					if ((i < x_channel) &&
			(bank_val_tmp > set_slf_bnk_ulmt) ||
			(bank_val_tmp < set_slf_bnk_dlmt)) {
						fail_flag = 1;
						slf_rx_fail_cnt++;
						}
					if ((i >= x_channel) &&
			(bank_val_tmp > set_slf_bnk_ulmt) ||
			(bank_val_tmp < set_slf_bnk_dlmt)) {
						fail_flag = 1;
						slf_tx_fail_cnt++;
					}
				}
				I(" rx_fail_cnt = %d, tx_fail_cnt = %d\n",
				  slf_rx_fail_cnt, slf_tx_fail_cnt);
				if (fail_flag) {
					RB1H
						[0] = 0xF3;
			/* Fail ID for Condition 3 */
					RB1H[1] = slf_rx_fail_cnt;
					RB1H[2] = slf_tx_fail_cnt;
					RB1H[3] = set_slf_bnk_ulmt;
					RB1H[4] = set_slf_bnk_dlmt;
					RB1H[5] = 0xFF;
					for (i = 0; i < 8; i++) {
						I(" RB1H[%d] = %X\n", i,
						  RB1H[i]);
					}
					for (m = 0; m < mutual_num; m++) {
						I(" mutual_bank[%d] = %X\n", m,
						  mutual_bank[m]);
					}
					for (m = 0; m < self_num; m++) {
						I(" self_bank[%d] = %X\n", m,
						  self_bank[m]);
					}
				} else {
					RB1H[0] = 0xAA; /* //PASS ID */
					RB1H[1] = bank_avg;
					RB1H[2] = bank_max;
					RB1H[3] = bank_min;
				}
			}
		}
	}
	kfree(mutual_bank);
	kfree(self_bank);
	return RB1H[0];
}
#endif

int himax_chip_self_test(struct i2c_client *client)
{
	uint8_t cmdbuf[11];
	uint8_t valuebuf[16];
	int pf_value = 0x00;
	int retry_times = 3;
	int retry_readB1 = 10;
#ifdef HX_TP_SELF_TEST_DRIVER
	uint8_t RB1H[8];
#else
	int i = 0;
#endif

test_retry:

	memset(cmdbuf, 0x00, sizeof(cmdbuf));
	memset(valuebuf, 0x00, sizeof(valuebuf));
#ifdef HX_TP_SELF_TEST_DRIVER
	memset(RB1H, 0x00, sizeof(RB1H));

	Selftest_flag = 1;
	g_diag_command = 0x03;
	himax_int_enable(client->irq, 1);
	/* Get Criteria */
	i2c_himax_read(client, 0xB1, RB1H, 8, DEFAULT_RETRY_CNT);
	msleep(20);
	I("[Himax]: self-test RB1H[0]=%x\n", RB1H[0]);
	RB1H[0] = RB1H[0] | 0x80;
	I("[Himax]: disable reK RB1H[0]=%x\n", RB1H[0]);
	i2c_himax_write(client, 0xB1, &RB1H[0], 1, DEFAULT_RETRY_CNT);
	msleep(20);
#else
	cmdbuf[0] = 0x06;
	i2c_himax_write(client, 0xF1, &cmdbuf[0], 1, DEFAULT_RETRY_CNT);
	msleep(120);
#endif
	i2c_himax_write(client, HX_CMD_TSSON, &cmdbuf[0], 0, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_write(client, HX_CMD_TSSLPOUT, &cmdbuf[0], 0,
			DEFAULT_RETRY_CNT);
#ifdef HX_TP_SELF_TEST_DRIVER
	msleep(120);
	valuebuf[0] = Self_Test_Bank(&RB1H[0]);
#else
	memset(valuebuf, 0x00, sizeof(valuebuf));
	msleep(2000);
#endif
	while (valuebuf[0] == 0x00 && retry_readB1 > 0) {
		i2c_himax_read(client, 0xB1, valuebuf, 2, DEFAULT_RETRY_CNT);
		msleep(100);
		retry_readB1--;
	}

	i2c_himax_write(client, HX_CMD_TSSOFF, &cmdbuf[0], 0,
			DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_write(client, HX_CMD_TSSLPIN, &cmdbuf[0], 0,
			DEFAULT_RETRY_CNT);
	msleep(120);

#ifdef HX_TP_SELF_TEST_DRIVER
	himax_int_enable(client->irq, 0);
	g_diag_command = 0x00;
	Selftest_flag = 0;
	RB1H[0] = 0x00;
	I("[Himax]: enable reK RB1H[0]=%x\n", RB1H[0]);
	i2c_himax_write(client, 0xB1, &RB1H[0], 1, DEFAULT_RETRY_CNT);
#else
	cmdbuf[0] = 0x00;
	i2c_himax_write(client, 0xF1, &cmdbuf[0], 1, DEFAULT_RETRY_CNT);
	msleep(120);

	i2c_himax_read(client, 0xB1, valuebuf, 8, DEFAULT_RETRY_CNT);
	msleep(20);

	for (i = 0; i < 8; i++) {
		I("[Himax]: After slf test 0xB1 buff_back[%d] = 0x%x\n", i,
		  valuebuf[i]);
	}

	msleep(30);
#endif
	if (valuebuf[0] == 0xAA) {
		I("[Himax]: self-test pass\n");
		pf_value = 0x1;
	} else {
		E("[Himax]: self-test fail : 0x%x\n", valuebuf[0]);
		if (retry_times > 0) {
			retry_times--;
			goto test_retry;
		}
		pf_value = 0x0;
	}

	return pf_value;
}

void himax_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable,
			   bool suspended)
{
	uint8_t buf[4];

	i2c_himax_read(client, 0x8F, buf, 1, DEFAULT_RETRY_CNT);

	if (HSEN_enable == 1 && !suspended)
		buf[0] |= 0x40;
	else
		buf[0] &= 0xBF;

	if (i2c_himax_write(client, 0x8F, buf, 1, DEFAULT_RETRY_CNT) < 0)
		E("%s i2c write fail.\n", __func__);
}

int himax_palm_detect(uint8_t *buf)
{
	int loop_i = 0;
	int base = 0;
	int x = 0;
	int y = 0;
	int w = 0;

	loop_i = 0;
	base = loop_i * 4;
	x = buf[base] << 8 | buf[base + 1];
	y = (buf[base + 2] << 8 | buf[base + 3]);
	w = buf[(private_ts->nFinger_support * 4) + loop_i];
	I(" %s HX_PALM_REPORT_loopi=%d,base=%x,X=%x,Y=%x,W=%x\n", __func__,
	  loop_i, base, x, y, w);

	if ((!atomic_read(&private_ts->suspend_mode)) && (x == 0xFA5A) &&
	    (y == 0xFA5A) && (w == 0x00)) {
		return NO_ERR;
	} else {
		return GESTURE_DETECT_FAIL;
	}
}

void himax_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable,
			   bool suspended)
{
	uint8_t buf[4];

	i2c_himax_read(client, 0x8F, buf, 1, DEFAULT_RETRY_CNT);

	if (SMWP_enable == 1 && suspended)
		buf[0] |= 0x20;
	else
		buf[0] &= 0xDF;

	if (i2c_himax_write(client, 0x8F, buf, 1, DEFAULT_RETRY_CNT) < 0)
		E("%s i2c write fail.\n", __func__);
}

void himax_usb_detect_set(struct i2c_client *client, uint8_t *cable_config)
{
	uint8_t tmp_data[4];
	uint8_t retry_cnt = 0;

	do {
		i2c_himax_master_write(client, cable_config, 2,
				       DEFAULT_RETRY_CNT);

		i2c_himax_read(client, cable_config[0], tmp_data, 1,
			       DEFAULT_RETRY_CNT);
		/* I("%s: tmp_data[0]=%d, retry_cnt=%d\n", __func__, */
		/* tmp_data[0],retry_cnt); */
		retry_cnt++;
	} while (tmp_data[0] != cable_config[1] &&
		 retry_cnt < HIMAX_REG_RETRY_TIMES);
}

void himax_register_read(struct i2c_client *client, uint8_t *read_addr,
			 int read_length, uint8_t *read_data, bool cfg_flag)
{
	uint8_t outData[4];

	if (cfg_flag) {
		outData[0] = 0x14;
		i2c_himax_write(client, 0x8C, &outData[0], 1,
				DEFAULT_RETRY_CNT);

		msleep(20);

		outData[0] = 0x00;
		outData[1] = read_addr[0];
		i2c_himax_write(client, 0x8B, &outData[0], 2,
				DEFAULT_RETRY_CNT);
		msleep(20);

		i2c_himax_read(client, 0x5A, read_data, read_length,
			       DEFAULT_RETRY_CNT);
		msleep(20);

		outData[0] = 0x00;
		i2c_himax_write(client, 0x8C, &outData[0], 1,
				DEFAULT_RETRY_CNT);
	} else {
		i2c_himax_read(client, read_addr[0], read_data, read_length,
			       DEFAULT_RETRY_CNT);
	}
}

void himax_register_write(struct i2c_client *client, uint8_t *write_addr,
			  int write_length, uint8_t *write_data, bool cfg_flag)
{
	uint8_t outData[4];

	if (cfg_flag) {
		outData[0] = 0x14;
		i2c_himax_write(client, 0x8C, &outData[0], 1,
				DEFAULT_RETRY_CNT);
		msleep(20);

		outData[0] = 0x00;
		outData[1] = write_addr[0];
		i2c_himax_write(client, 0x8B, &outData[0], 2,
				DEFAULT_RETRY_CNT);
		msleep(20);

		i2c_himax_write(client, 0x40, &write_data[0], write_length,
				DEFAULT_RETRY_CNT);
		msleep(20);

		outData[0] = 0x00;
		i2c_himax_write(client, 0x8C, &outData[0], 1,
				DEFAULT_RETRY_CNT);

		I("CMD: FE(%x), %x, %d\n", write_addr[0], write_data[0],
		  write_length);
	} else {
		i2c_himax_write(client, write_addr[0], &write_data[0],
				write_length, DEFAULT_RETRY_CNT);
	}
}

void himax_interface_on(struct i2c_client *client)
{
}

bool wait_wip(struct i2c_client *client, int Timing)
{
	return true;
}

void himax_sense_off(struct i2c_client *client)
{
	i2c_himax_write_command(client, HX_CMD_TSSOFF, DEFAULT_RETRY_CNT);
	msleep(50);

	i2c_himax_write_command(client, HX_CMD_TSSLPIN, DEFAULT_RETRY_CNT);
	msleep(50);
}

void himax_sense_on(struct i2c_client *client, uint8_t FlashMode)
{
	i2c_himax_write_command(client, HX_CMD_TSSON, DEFAULT_RETRY_CNT);
	msleep(30);

	i2c_himax_write_command(client, HX_CMD_TSSLPOUT, DEFAULT_RETRY_CNT);
	msleep(50);
}

static int himax_ManualMode(struct i2c_client *client, int enter)
{
	uint8_t cmd[2];

	cmd[0] = enter;
	if (i2c_himax_write(client, HX_CMD_MANUALMODE, &cmd[0], 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	return 0;
}

static int himax_FlashMode(struct i2c_client *client, int enter)
{
	uint8_t cmd[2];

	cmd[0] = enter;
	if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	return 0;
}

static int himax_lock_flash(struct i2c_client *client, int enable)
{
	uint8_t cmd[5];

	if (i2c_himax_write(client, 0xAA, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	/* lock sequence start */
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x06;
	if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	cmd[0] = 0x03;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(client, HX_CMD_FLASH_SET_ADDRESS, &cmd[0], 3, 3) <
	    0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (enable != 0) {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x70;
		cmd[3] = 0x03;
	} else {
		cmd[0] = 0x63;
		cmd[1] = 0x02;
		cmd[2] = 0x30;
		cmd[3] = 0x00;
	}

	if (i2c_himax_write(client, HX_CMD_FLASH_WRITE_REGISTER, &cmd[0], 4,
			    3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (i2c_himax_write_command(client, HX_CMD_4A, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	msleep(50);

	if (i2c_himax_write(client, 0xA9, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	return 0;
	/* lock sequence stop */
}

static void himax_changeIref(struct i2c_client *client, int selected_iref)
{

	unsigned char temp_iref[16][2] = {
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00},
		{0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00}, {0x00, 0x00} };
	uint8_t cmd[10];
	int i = 0;
	int j = 0;

	I("%s: start to check iref,iref number = %d\n", __func__,
	  selected_iref);

	if (i2c_himax_write(client, 0xAA, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 2; j++) {
			if (selected_iref == 1)
				temp_iref[i][j] = E_IrefTable_1[i][j];
			else if (selected_iref == 2)
				temp_iref[i][j] = E_IrefTable_2[i][j];
			else if (selected_iref == 3)
				temp_iref[i][j] = E_IrefTable_3[i][j];
			else if (selected_iref == 4)
				temp_iref[i][j] = E_IrefTable_4[i][j];
			else if (selected_iref == 5)
				temp_iref[i][j] = E_IrefTable_5[i][j];
			else if (selected_iref == 6)
				temp_iref[i][j] = E_IrefTable_6[i][j];
			else if (selected_iref == 7)
				temp_iref[i][j] = E_IrefTable_7[i][j];
		}
	}

	if (!iref_found) {
		/* Read Iref */
		/* Register 0x43 */
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x0A;
		if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3,
				    3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/* Register 0x44 */
		cmd[0] = 0x00;
		cmd[1] = 0x00;
		cmd[2] = 0x00;
		if (i2c_himax_write(client, HX_CMD_FLASH_SET_ADDRESS, &cmd[0],
				    3, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/* Register 0x46 */
		if (i2c_himax_write(client, 0x46, &cmd[0], 0, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/* Register 0x59 */
		if (i2c_himax_read(client, 0x59, cmd, 4, 3) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return;
		}

		/* find iref group , default is iref 3 */
		for (i = 0; i < 16; i++) {
			if ((cmd[0] == temp_iref[i][0]) &&
			    (cmd[1] == temp_iref[i][1])) {
				iref_number = i;
				iref_found = true;
				break;
			}
		}

		if (!iref_found) {
			E("%s: Can't find iref number!\n", __func__);
			return;
		}
		I("%s: iref_number=%d, cmd[0]=0x%x, cmd[1]=0x%x\n", __func__,
		  iref_number, cmd[0], cmd[1]);
	}

	msleep(20);

	/* iref write */
	/* Register 0x43 */
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x06;
	if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x44 */
	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(client, HX_CMD_FLASH_SET_ADDRESS, &cmd[0], 3, 3) <
	    0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x45 */
	cmd[0] = temp_iref[iref_number][0];
	cmd[1] = temp_iref[iref_number][1];
	cmd[2] = 0x17;
	cmd[3] = 0x28;

	if (i2c_himax_write(client, HX_CMD_FLASH_WRITE_REGISTER, &cmd[0], 4,
			    3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x4A */
	if (i2c_himax_write(client, HX_CMD_4A, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Read SFR to check the result */
	/* Register 0x43 */
	cmd[0] = 0x01;
	cmd[1] = 0x00;
	cmd[2] = 0x0A;
	if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x44 */
	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = 0x00;
	if (i2c_himax_write(client, HX_CMD_FLASH_SET_ADDRESS, &cmd[0], 3, 3) <
	    0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x46 */
	if (i2c_himax_write(client, 0x46, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	/* Register 0x59 */
	if (i2c_himax_read(client, 0x59, cmd, 4, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}

	I("%s:cmd[0]=%d,cmd[1]=%d,temp_iref_1=%d,temp_iref_2=%d\n", __func__,
	  cmd[0], cmd[1], temp_iref[iref_number][0], temp_iref[iref_number][1]);

	if (cmd[0] != temp_iref[iref_number][0] ||
	    cmd[1] != temp_iref[iref_number][1]) {
		E("%s: IREF Read Back is not match.\n", __func__);
		E("%s: Iref [0]=%d,[1]=%d\n", __func__, cmd[0], cmd[1]);
	} else {
		I("%s: IREF Pass", __func__);
	}

	if (i2c_himax_write(client, 0xA9, &cmd[0], 0, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
}

bool himax_calculateChecksum(struct i2c_client *client, bool change_iref)
{

	int iref_flag = 0;
	uint8_t cmd[10];

	memset(cmd, 0x00, sizeof(cmd));

	/* Sleep out */
	if (i2c_himax_write(client, HX_CMD_TSSLPOUT, &cmd[0], 0,
			    DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	msleep(120);

	while (true) {

		if (change_iref) {
			if (iref_flag == 0)
				himax_changeIref(client, 2); /* iref 2 */
			else if (iref_flag == 1)
				himax_changeIref(client, 5); /* iref 5 */
			else if (iref_flag == 2)
				himax_changeIref(client, 1); /* iref 1 */
			else
				goto CHECK_FAIL;

			iref_flag++;
		}

		cmd[0] = 0x00;
		cmd[1] = 0x04;
		cmd[2] = 0x0A;
		cmd[3] = 0x02;

		if (i2c_himax_write(client, 0xED, &cmd[0], 4,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		/* Enable Flash */
		cmd[0] = 0x01;
		cmd[1] = 0x00;
		cmd[2] = 0x02;

		if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}
		cmd[0] = 0x05;
		if (i2c_himax_write(client, 0xD2, &cmd[0], 1,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		cmd[0] = 0x01;
		if (i2c_himax_write(client, 0x53, &cmd[0], 1,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		msleep(200);

		if (i2c_himax_read(client, 0xAD, cmd, 4, DEFAULT_RETRY_CNT) <
		    0) {
			E("%s: i2c access fail!\n", __func__);
			return -1;
		}

		I("%s 0xAD[0,1,2,3] = %d,%d,%d,%d\n", __func__, cmd[0], cmd[1],
		  cmd[2], cmd[3]);

		if (cmd[0] == 0 && cmd[1] == 0 && cmd[2] == 0 && cmd[3] == 0) {
			himax_FlashMode(client, 0);
			goto CHECK_PASS;
		} else {
			himax_FlashMode(client, 0);
			goto CHECK_FAIL;
		}

CHECK_PASS:
		if (change_iref) {
			if (iref_flag < 3)
				continue;
			else
				return 0;

		} else {
			return 0;
		}

CHECK_FAIL:
		return 1;
	}
	return 1;
}

bool himax_flash_lastdata_check(struct i2c_client *client)
{
	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs_32k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref)
{
	unsigned char *ImageBuffer = fw;
	int fullFileLength = len;
	int i;
	uint8_t cmd[5], last_byte, prePage;
	int FileLength = 0;
	uint8_t checksumResult = 0;

	I("Enter %s", __func__);
	if (len != 0x8000) { /* 32k */

		E("%s: The file size is not 32K bytes, len = %d\n", __func__,
		  fullFileLength);
		return false;
	}

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(false, false);
#endif

	FileLength = fullFileLength;

	if (i2c_himax_write(client, HX_CMD_TSSLPOUT, &cmd[0], 0,
			    DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	msleep(120);

	himax_lock_flash(client, 0);

	cmd[0] = 0x05;
	cmd[1] = 0x00;
	cmd[2] = 0x02;
	if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 3,
			    DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}

	if (i2c_himax_write(client, 0x4F, &cmd[0], 0, DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return 0;
	}
	msleep(50);

	himax_ManualMode(client, 1);
	himax_FlashMode(client, 1);

	FileLength = (FileLength + 3) / 4;
	for (i = 0, prePage = 0; i < FileLength; i++) {
		last_byte = 0;
		cmd[0] = i & 0x1F;
		if (cmd[0] == 0x1F || i == FileLength - 1)
			last_byte = 1;

		cmd[1] = (i >> 5) & 0x1F;
		cmd[2] = (i >> 10) & 0x1F;
		if (i2c_himax_write(client, HX_CMD_FLASH_SET_ADDRESS, &cmd[0],
				    3, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		if (prePage != cmd[1] || i == 0) {
			prePage = cmd[1];
			cmd[0] = 0x01;
			cmd[1] = 0x09; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x0D; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x09; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}
		}

		memcpy(&cmd[0], &ImageBuffer[4 * i], 4);
		if (i2c_himax_write(client, HX_CMD_FLASH_WRITE_REGISTER,
				    &cmd[0], 4, DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		cmd[0] = 0x01;
		cmd[1] = 0x0D; /* cmd[2] = 0x02; */
		if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 2,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		cmd[0] = 0x01;
		cmd[1] = 0x09; /* cmd[2] = 0x02; */
		if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE, &cmd[0], 2,
				    DEFAULT_RETRY_CNT) < 0) {
			E("%s: i2c access fail!\n", __func__);
			return 0;
		}

		if (last_byte == 1) {
			cmd[0] = 0x01;
			cmd[1] = 0x01; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x05; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x01; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			cmd[0] = 0x01;
			cmd[1] = 0x00; /* cmd[2] = 0x02; */
			if (i2c_himax_write(client, HX_CMD_FLASH_ENABLE,
					    &cmd[0], 2,
					    DEFAULT_RETRY_CNT) < 0) {
				E("%s: i2c access fail!\n", __func__);
				return 0;
			}

			msleep(20);
			if (i == (FileLength - 1)) {
				himax_FlashMode(client, 0);
				himax_ManualMode(client, 0);
				checksumResult = himax_calculateChecksum(
					client, change_iref); /*  */
				/* himax_ManualMode(client,0); */
				himax_lock_flash(client, 1);

				if (!checksumResult) { /* Success */

					return 1;
				}

				E("%s: checksumResult fail!\n", __func__);
				return 0;
			}
		}
	}
	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref)
{

	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client,
					unsigned char *fw, int len,
					bool change_iref)
{

	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client,
					 unsigned char *fw, int len,
					 bool change_iref)
{

	return 0;
}

int fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client,
					 unsigned char *fw, int len,
					 bool change_iref)
{

	return 0;
}

void himax_touch_information(struct i2c_client *client)
{
#ifndef HX_FIX_TOUCH_INFO
	char data[12] = {0};

	I("%s:IC_TYPE =%d\n", __func__, IC_TYPE);

	if (IC_TYPE == HX_85XX_ES_SERIES_PWON) {
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x70;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(20);
		i2c_himax_read(client, 0x5A, data, 12, DEFAULT_RETRY_CNT);
		ic_data->HX_RX_NUM = data[0];		    /* FE(70) */
		ic_data->HX_TX_NUM = data[1];		    /* FE(71) */
		ic_data->HX_MAX_PT = (data[2] & 0xF0) >> 4; /* FE(72) */
#ifdef HX_EN_SEL_BUTTON
		ic_data->HX_BT_NUM = (data[2] & 0x0F); /* FE(72) */
#endif
		if ((data[4] & 0x04) == 0x04) { /* FE(74) */

			ic_data->HX_XY_REVERSE = true;
			ic_data->HX_Y_RES =
				data[6] * 256 + data[7]; /* FE(76),FE(77) */
			ic_data->HX_X_RES =
				data[8] * 256 + data[9]; /* FE(78),FE(79) */
		} else {
			ic_data->HX_XY_REVERSE = false;
			ic_data->HX_X_RES =
				data[6] * 256 + data[7]; /* FE(76),FE(77) */
			ic_data->HX_Y_RES =
				data[8] * 256 + data[9]; /* FE(78),FE(79) */
		}
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
#ifdef HX_EN_MUT_BUTTON
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x64;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(20);
		i2c_himax_read(client, 0x5A, data, 4, DEFAULT_RETRY_CNT);
		ic_data->HX_BT_NUM = (data[0] & 0x03);
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
#endif
#ifdef HX_TP_PROC_2T2R
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);

		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = HX_2T2R_Addr;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(20);

		i2c_himax_read(client, 0x5A, data, 10, DEFAULT_RETRY_CNT);

		ic_data->HX_RX_NUM_2 = data[0];
		ic_data->HX_TX_NUM_2 = data[1];

		I("%s:Touch Panel Type=%d\n", __func__, data[2]);
		if ((data[2] & 0x02) ==
		    HX_2T2R_en_setting) /* 2T2R type panel */
			Is_2T2R = true;
		else
			Is_2T2R = false;

		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
#endif
		data[0] = 0x8C;
		data[1] = 0x14;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
		data[0] = 0x8B;
		data[1] = 0x00;
		data[2] = 0x02;
		i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
		msleep(20);
		i2c_himax_read(client, 0x5A, data, 10, DEFAULT_RETRY_CNT);
		if ((data[1] & 0x01) == 1) { /* FE(02) */

			ic_data->HX_INT_IS_EDGE = true;
		} else {
			ic_data->HX_INT_IS_EDGE = false;
		}
		data[0] = 0x8C;
		data[1] = 0x00;
		i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
		msleep(20);
		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d\n", __func__,
		  ic_data->HX_RX_NUM, ic_data->HX_TX_NUM, ic_data->HX_MAX_PT);

		if (i2c_himax_read(client, HX_VER_FW_CFG, data, 1, 3) < 0)
			E("%s: i2c access fail!\n", __func__);

		ic_data->vendor_config_ver = data[0];
		I("config_ver=%x.\n", ic_data->vendor_config_ver);

	} else {
		ic_data->HX_RX_NUM = 0;
		ic_data->HX_TX_NUM = 0;
		ic_data->HX_BT_NUM = 0;
		ic_data->HX_X_RES = 0;
		ic_data->HX_Y_RES = 0;
		ic_data->HX_MAX_PT = 0;
		ic_data->HX_XY_REVERSE = false;
		ic_data->HX_INT_IS_EDGE = false;
	}
#else
	ic_data->HX_RX_NUM = FIX_HX_RX_NUM;
	ic_data->HX_TX_NUM = FIX_HX_TX_NUM;
	ic_data->HX_BT_NUM = FIX_HX_BT_NUM;
	ic_data->HX_X_RES = FIX_HX_X_RES;
	ic_data->HX_Y_RES = FIX_HX_Y_RES;
	ic_data->HX_MAX_PT = FIX_HX_MAX_PT;
	ic_data->HX_XY_REVERSE = FIX_HX_XY_REVERSE;
	ic_data->HX_INT_IS_EDGE = FIX_HX_INT_IS_EDGE;
#ifdef HX_TP_PROC_2T2R
	Is_2T2R = true;
	ic_data->HX_RX_NUM_2 = FIX_HX_RX_NUM_2;
	ic_data->HX_TX_NUM_2 = FIX_HX_TX_NUM_2;
#endif
	I("%s:RX_NUM =%d,TX_NUM =%d,MAX_PT=%d,X_RES =%d,Y_RES =%d,INT_IS=%d\n",
	  __func__, ic_data->HX_RX_NUM, ic_data->HX_TX_NUM, ic_data->HX_MAX_PT,
	  ic_data->HX_X_RES, ic_data->HX_Y_RES, ic_data->HX_INT_IS_EDGE);
#endif
}

static int himax_read_Sensor_ID(struct i2c_client *client)
{
	uint8_t val_high[1], val_low[1], ID0 = 0, ID1 = 0;
	char data[3];
	const int normalRetry = 10;
	int sensor_id;

	data[0] = 0x56;
	data[1] = 0x02;
	data[2] = 0x02; /*ID pin PULL High*/
	i2c_himax_master_write(client, &data[0], 3, normalRetry);
	msleep(20);

	/* read id pin high */
	i2c_himax_read(client, 0x57, val_high, 1, normalRetry);

	data[0] = 0x56;
	data[1] = 0x01;
	data[2] = 0x01; /*ID pin PULL Low*/
	i2c_himax_master_write(client, &data[0], 3, normalRetry);
	msleep(20);

	/* read id pin low */
	i2c_himax_read(client, 0x57, val_low, 1, normalRetry);

	if ((val_high[0] & 0x01) == 0)
		ID0 = 0x02; /*GND*/
	else if ((val_low[0] & 0x01) == 0)
		ID0 = 0x01; /*Floating*/
	else
		ID0 = 0x04; /*VCC*/

	if ((val_high[0] & 0x02) == 0)
		ID1 = 0x02; /*GND*/
	else if ((val_low[0] & 0x02) == 0)
		ID1 = 0x01; /*Floating*/
	else
		ID1 = 0x04; /*VCC*/
	if ((ID0 == 0x04) && (ID1 != 0x04)) {
		data[0] = 0x56;
		data[1] = 0x02;
		data[2] = 0x01; /*ID pin PULL High,Low*/
		i2c_himax_master_write(client, &data[0], 3, normalRetry);
		msleep(20);

	} else if ((ID0 != 0x04) && (ID1 == 0x04)) {
		data[0] = 0x56;
		data[1] = 0x01;
		data[2] = 0x02; /*ID pin PULL Low,High*/
		i2c_himax_master_write(client, &data[0], 3, normalRetry);
		msleep(20);

	} else if ((ID0 == 0x04) && (ID1 == 0x04)) {
		data[0] = 0x56;
		data[1] = 0x02;
		data[2] = 0x02; /*ID pin PULL High,High*/
		i2c_himax_master_write(client, &data[0], 3, normalRetry);
		msleep(20);
	}
	sensor_id = (ID1 << 4) | ID0;

	data[0] = 0xE4;
	data[1] = sensor_id;
	i2c_himax_master_write(client, &data[0], 2,
			       normalRetry); /*Write to MCU*/
	msleep(20);

	return sensor_id;
}

int himax_read_i2c_status(struct i2c_client *client)
{
	return i2c_error_count; /*  */
}

int himax_read_ic_trigger_type(struct i2c_client *client)
{
	char data[12] = {0};
	int trigger_type = false;

	himax_sense_off(client);
	data[0] = 0x8C;
	data[1] = 0x14;
	i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
	msleep(20);
	data[0] = 0x8B;
	data[1] = 0x00;
	data[2] = 0x02;
	i2c_himax_master_write(client, &data[0], 3, DEFAULT_RETRY_CNT);
	msleep(20);
	i2c_himax_read(client, 0x5A, data, 10, DEFAULT_RETRY_CNT);
	if ((data[1] & 0x01) == 1) { /* FE(02) */

		trigger_type = true;
	} else {
		trigger_type = false;
	}
	data[0] = 0x8C;
	data[1] = 0x00;
	i2c_himax_master_write(client, &data[0], 2, DEFAULT_RETRY_CNT);
	himax_sense_on(client, 0x01);

	return trigger_type;
}

void himax_read_FW_ver(struct i2c_client *client)
{
	uint8_t data[64];

	/* read fw version */
	if (i2c_himax_read(client, HX_VER_FW_MAJ, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	ic_data->vendor_fw_ver = data[0] << 8;

	if (i2c_himax_read(client, HX_VER_FW_MIN, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	ic_data->vendor_fw_ver = data[0] | ic_data->vendor_fw_ver;

	/* read config version */
	if (i2c_himax_read(client, HX_VER_FW_CFG, data, 1, 3) < 0) {
		E("%s: i2c access fail!\n", __func__);
		return;
	}
	ic_data->vendor_config_ver = data[0];
	/* read sensor ID */
	ic_data->vendor_sensor_id = himax_read_Sensor_ID(client);

	I("sensor_id=%x.\n", ic_data->vendor_sensor_id);
	I("fw_ver=%x.\n", ic_data->vendor_fw_ver);
	I("config_ver=%x.\n", ic_data->vendor_config_ver);

	ic_data->vendor_panel_ver = -1;

	ic_data->vendor_cid_maj_ver = -1;
	ic_data->vendor_cid_min_ver = -1;
}

bool himax_ic_package_check(struct i2c_client *client)
{
	uint8_t cmd[3];

	memset(cmd, 0x00, sizeof(cmd));

	msleep(50);
	if (i2c_himax_read(client, 0xD1, cmd, 3, DEFAULT_RETRY_CNT) < 0)
		return false;

	if (cmd[0] == 0x05 && cmd[1] == 0x85 &&
	    (cmd[2] == 0x25 || cmd[2] == 0x26 || cmd[2] == 0x27 ||
	     cmd[2] == 0x28)) {
		IC_TYPE = HX_85XX_ES_SERIES_PWON;
		IC_CHECKSUM = HX_TP_BIN_CHECKSUM_CRC;
		/* Himax: Set FW and CFG Flash Address */
		FW_VER_MAJ_FLASH_ADDR = 133; /* 0x0085 */
		FW_VER_MAJ_FLASH_LENG = 1;
		;
		FW_VER_MIN_FLASH_ADDR = 134; /* 0x0086 */
		FW_VER_MIN_FLASH_LENG = 1;
		FW_CFG_VER_FLASH_ADDR = 132; /* 0x0084 */
#ifdef HX_AUTO_UPDATE_FW
		g_i_FW_VER = i_CTPM_FW[FW_VER_MAJ_FLASH_ADDR] << 8 |
			     i_CTPM_FW[FW_VER_MIN_FLASH_ADDR];
		g_i_CFG_VER = i_CTPM_FW[FW_CFG_VER_FLASH_ADDR];
		g_i_CID_MAJ = -1;
		g_i_CID_MIN = -1;
#endif
		I("Himax IC package 852x ES\n");
	} else {
		E("Himax IC package incorrect!!PKG[0]=%x,PKG[1]=%x,PKG[2]=%x\n",
		  cmd[0], cmd[1], cmd[2]);
		return false;
	}
	return true;
}

void himax_power_on_init(struct i2c_client *client)
{
	I("%s:\n", __func__);
	himax_sense_on(client, 0x01);
	himax_sense_off(client);
	himax_touch_information(client);
	himax_sense_on(client, 0x01);
}

void himax_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data)
{
}

void himax_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte)
{
}

/* ts_work */
int cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max)
{
	int RawDataLen;

	if (raw_cnt_rmd != 0x00)
		RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 3) * 4) - 1;
	else
		RawDataLen = 128 - ((HX_MAX_PT + raw_cnt_max + 2) * 4) - 1;

	return RawDataLen;
}

bool himax_read_event_stack(struct i2c_client *client, uint8_t *buf, int length)
{
	uint8_t cmd[4];

	if (length > 56)
		length = 128;
	/* ===================== */
	/* Read event stack */
	/* ===================== */

	cmd[0] = 0x31;
	if (i2c_himax_read(client, 0x86, buf, length, DEFAULT_RETRY_CNT) < 0) {
		E("%s: i2c access fail!\n", __func__);
		goto err_workqueue_out;
	}

	return 1;

err_workqueue_out:
	return 0;
}

/* return checksum result */
bool diag_check_sum(struct himax_report_data *hx_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0; i < (hx_touch_data->touch_all_size -
					    hx_touch_data->touch_info_size);
	     i++) {
		check_sum_cal += hx_touch_data->hx_rawdata_buf[i];
	}
	if (check_sum_cal % 0x100 != 0)
		return 0;

	return 1;
}

void diag_parse_raw_data(struct himax_report_data *hx_touch_data, int mul_num,
			 int self_num, uint8_t diag_cmd, int32_t *mutual_data,
			 int32_t *self_data)
{
	int index = 0;
	int temp1, temp2, i;
#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	int cnt = 0;
	uint8_t command_F1h_bank[2] = {0xF1, 0x00};
#endif

	/* Himax: Check Raw-Data Header */
	if (hx_touch_data->hx_rawdata_buf[0] ==
		    hx_touch_data->hx_rawdata_buf[1] &&
	    hx_touch_data->hx_rawdata_buf[1] ==
		    hx_touch_data->hx_rawdata_buf[2] &&
	    hx_touch_data->hx_rawdata_buf[2] ==
		    hx_touch_data->hx_rawdata_buf[3] &&
	    hx_touch_data->hx_rawdata_buf[0] > 0) {
		index = (hx_touch_data->hx_rawdata_buf[0] - 1) *
			hx_touch_data->rawdata_size;
		/* I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", */
		/* index, buf[56], buf[57], buf[58], buf[59], mul_num, */
		/* self_num); */
		for (i = 0; i < hx_touch_data->rawdata_size; i++) {
			temp1 = index + i;

			if (temp1 < mul_num) {
				/* mutual */
				mutual_data[index + i] =
					hx_touch_data->hx_rawdata_buf
						[i + 4]; /* 4: RawData Header */
#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
				if (Selftest_flag == 1) {
					/* mutual_bank[index + i] = */
					/* hx_touch_data->hx_rawdata_buf[i + 4];
					 */
					raw_data_chk_arr
						[hx_touch_data
							 ->hx_rawdata_buf[0] -
						 1] = true;
				}
#endif
			} else {
				/* self */
				temp1 = i + index;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i + index - mul_num] =
					hx_touch_data->hx_rawdata_buf
						[i + 4]; /* 4: RawData Header */
#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
				if (Selftest_flag == 1) {
					/* self_bank[i+index-mul_num] = */
					/* hx_touch_data->hx_rawdata_buf[i + 4];
					 */
					raw_data_chk_arr
						[hx_touch_data
							 ->hx_rawdata_buf[0] -
						 1] = true;
				}
#endif
			}
		}
#if defined(HX_TP_SELF_TEST_DRIVER) ||                                         \
	defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
		if (Selftest_flag == 1) {
			cnt = 0;
			for (i = 0; i < hx_touch_data->rawdata_frame_size;
			     i++) {
				if (raw_data_chk_arr[i] == true)
					cnt++;
			}
			if (cnt == hx_touch_data->rawdata_frame_size) {
				I("test_counter = %d\n", test_counter);
				test_counter++;
			}
			if (test_counter == TEST_DATA_TIMES) {
				memcpy(mutual_bank, mutual_data,
				       mul_num * sizeof(uint16_t));
				memcpy(self_bank, self_data,
				       self_num * sizeof(uint16_t));
				for (i = 0;
				     i < hx_touch_data->rawdata_frame_size;
				     i++) {
					raw_data_chk_arr[i] = false;
				}
				test_counter = 0;
				Selftest_flag = 0;
				g_diag_command = 0;
				command_F1h_bank[1] = 0x00;
				i2c_himax_write(private_ts->client,
						command_F1h_bank[0],
						&command_F1h_bank[1], 1,
						DEFAULT_RETRY_CNT);
				msleep(20);
			}
		}
#endif
	}
}

uint8_t himax_read_DD_status(uint8_t *cmd_set, uint8_t *tmp_data)
{
	return -1;
}

int himax_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr)
{
	return -1;
}

#ifdef HX_TP_PROC_GUEST_INFO
#define HX_GUEST_INFO_SIZE 10
#define HX_GUEST_INFO_LEN_SIZE 4
int g_guest_info_ongoing; /* 0 stop //1 ongoing */
char g_guest_str[10][128];

int himax_guest_info_get_status(void)
{
	return g_guest_info_ongoing;
}
void himax_guest_info_set_status(int setting)
{
	g_guest_info_ongoing = setting;
}

void himax_guest_info_read(uint32_t start_addr, uint8_t *flash_tmp_buffer)
{
}

int himax_read_project_id(void)
{
	int custom_info_temp = 0;
	char *temp_str = "DO NOT support this function!\n";

	himax_guest_info_set_status(1);
	for (custom_info_temp = 0; custom_info_temp < HX_GUEST_INFO_SIZE;
	     custom_info_temp++) {
		memcpy(&g_guest_str[custom_info_temp], temp_str, 30);
	}
	himax_guest_info_set_status(0);
	return NO_ERR;
}
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
void himax_resend_cmd_func(bool suspended)
{
	struct himax_ts_data *ts;

	ts = private_ts;

	if (!suspended) { /* if entering resume need to sense off first*/

		himax_int_enable(ts->client->irq, 0);
#ifdef HX_RESUME_HW_RESET
		himax_ic_reset(false, false);
#else
		himax_sense_off(ts->client);
#endif
	}

#ifdef HX_SMART_WAKEUP
	himax_set_SMWP_enable(ts->client, ts->SMWP_enable, suspended);
#endif
#ifdef HX_HIGH_SENSE
	himax_set_HSEN_enable(ts->client, ts->HSEN_enable, suspended);
#endif
#ifdef HX_USB_DETECT_GLOBAL
	himax_cable_detect_func(true);
#endif
}

void himax_rst_cmd_recovery_func(bool suspended)
{
}
#endif

void himax_resume_ic_action(struct i2c_client *client)
{
	i2c_himax_write_command(client, HX_CMD_TSSON, DEFAULT_RETRY_CNT);
	msleep(30);

	i2c_himax_write_command(client, HX_CMD_TSSLPOUT, DEFAULT_RETRY_CNT);
}

void himax_suspend_ic_action(struct i2c_client *client)
{
	i2c_himax_write_command(client, HX_CMD_TSSOFF, DEFAULT_RETRY_CNT);
	msleep(40);

	i2c_himax_write_command(client, HX_CMD_TSSLPIN, DEFAULT_RETRY_CNT);
}
