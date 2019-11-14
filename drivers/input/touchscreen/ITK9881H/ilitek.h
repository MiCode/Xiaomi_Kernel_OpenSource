/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#ifndef __ILITEK_H
#define __ILITEK_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/fs.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/uaccess.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <linux/namei.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/rtc.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/mount.h>
#include <linux/firmware.h>
#include <linux/hqsysfs.h>

#ifdef CONFIG_OF
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#else
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_MTK_SPI
#include "mt_spi.h"
#include "sync_write.h"
#endif

#define DRIVER_VERSION			"2.3.1.0"
#define ILK_VENDOR_INFO                             "[Vendor]Tianma(TP) + Tianma(LCD), [TP-IC]ILITEK9881H, [FW]Ver"

/* Options */
#define TDDI_INTERFACE			BUS_SPI /* BUS_I2C(0x18) or BUS_SPI(0x1C) */
#define VDD_VOLTAGE			1800000
#define VCC_VOLTAGE			1800000
#define SPI_CLK				(10*M)
#define SPI_RETRY			5
#define WQ_ESD_DELAY			4000
#define WQ_BAT_DELAY			2000
#define MT_B_TYPE			ENABLE
#define TDDI_RST_BIND			DISABLE
#define MT_PRESSURE			DISABLE
#define ENABLE_WQ_ESD			ENABLE
#define ENABLE_WQ_BAT			DISABLE
#define ENABLE_GESTURE			ENABLE
#define REGULATOR_POWER			DISABLE
#define TP_SUSPEND_PRIO			ENABLE

/* Plaform compatibility */



/* Path */
#define DEBUG_DATA_FILE_SIZE		(10*K)
#define DEBUG_DATA_FILE_PATH		"/sdcard/ILITEK_log.csv"
#define CSV_LCM_ON_PATH			"/sdcard/Download"
#define CSV_LCM_OFF_PATH		"/sdcard/Download"
#define INI_NAME_PATH			"/vendor/etc/mp.ini"
#define UPDATE_FW_PATH			"/vendor/firmware/ILITEK_FW"
#define POWER_STATUS_PATH		"/sys/class/power_supply/battery/status"
#define DUMP_FLASH_PATH			"/sdcard/flash_dump"
#define DUMP_IRAM_PATH			"/sdcard/iram_dump"

/* Debug messages */
#define DEBUG_NONE	0
#define DEBUG_ALL	1
#define DEBUG_OUTPUT	DEBUG_NONE

#define ipio_info(fmt, arg...)						\
({									\
	pr_info("ILITEK:INFO (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

#define ipio_err(fmt, arg...)						\
({									\
	pr_err("ILITEK:ERR (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

extern bool ipio_debug_level;
extern bool is_lcd_resume;
extern bool is_ilitek_tp;
#define ipio_debug(fmt, arg...)						\
do {									\
	if (ipio_debug_level)						\
	pr_info("ILITEK:DEBUG (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
} while (0)

#define ERR_ALLOC_MEM(X)	((IS_ERR(X) || X == NULL) ? 1 : 0)
#define K			(1024)
#define M			(K * K)
#define ENABLE			1
#define START			1
#define ON			1
#define ILI_WRITE		1
#define ILI_READ		0
#define DISABLE			0
#define END			0
#define OFF			0
#define NONE			-1
#define DO_SPI_RECOVER		-2

enum TP_PLAT_TYPE {
	TP_PLAT_MTK = 0,
	TP_PLAT_QCOM
};

enum TP_RST_METHOD {
	TP_IC_WHOLE_RST = 0,
	TP_IC_CODE_RST,
	TP_HW_RST_ONLY,
};

enum TP_FW_UPGRADE_TYPE {
	UPGRADE_FLASH = 0,
	UPGRADE_IRAM
};

enum TP_FW_UPGRADE_TARGET {
	ILI_FILE = 0,
	HEX_FILE
};

enum TP_FW_OPEN_METHOD {
	REQUEST_FIRMWARE = 0,
	FILP_OPEN
};

enum TP_SLEEP_STATUS {
	TP_SUSPEND = 0,
	TP_DEEP_SLEEP = 1,
	TP_RESUME = 2
};

enum TP_SLEEP_CTRL {
	SLEEP_IN = 0x0,
	SLEEP_OUT = 0x1,
	DEEP_SLEEP_IN = 0x3
};

enum TP_FW_BLOCK_NUM {
	AP = 1,
	DATA = 2,
	TUNING = 3,
	GESTURE = 4,
	MP = 5,
	DDI = 6
};

enum TP_FW_BLOCK_TAG {
	BLOCK_TAG_AE = 0xAE,
	BLOCK_TAG_AF = 0xAF,
	BLOCK_TAG_B0 = 0xB0
};

enum TP_WQ_TYPE {
	WQ_ESD = 0,
	WQ_BAT,
};

enum MPTestItem{
	RAW_DATA_NO_BK,
	SHORT_TEST_ILI9881,
	NOISE_PEAK_TO_PEAK_WITH_PANEL,
	DOZE_PEAK_TO_PEAK,
	OPEN_TEST_C,
};

#define TDDI_I2C_ADDR				0x41
#define TDDI_DEV_ID				"ILITEK_TDDI"

 /* define the width and heigth of a screen. */
#define TOUCH_SCREEN_X_MIN			0
#define TOUCH_SCREEN_Y_MIN			0
#define TOUCH_SCREEN_X_MAX			720
#define TOUCH_SCREEN_Y_MAX			1520
#define MAX_TOUCH_NUM				10

/* define the range on panel */
#define TPD_HEIGHT				2048
#define TPD_WIDTH				2048

/* Firmware upgrade */
#define MAX_HEX_FILE_SIZE			(160*K)
#define MAX_FLASH_FIRMWARE_SIZE			(256*K)
#define MAX_IRAM_FIRMWARE_SIZE			(60*K)
#define ILI_FILE_HEADER				64
#define MAX_AP_FIRMWARE_SIZE			(64*K)
#define MAX_DLM_FIRMWARE_SIZE			(8*K)
#define MAX_MP_FIRMWARE_SIZE			(64*K)
#define MAX_GESTURE_FIRMWARE_SIZE		(8*K)
#define MAX_TUNING_FIRMWARE_SIZE		(4*K)
#define MAX_DDI_FIRMWARE_SIZE			(4*K)
#define DLM_START_ADDRESS			0x20610
#define DLM_HEX_ADDRESS				0x10000
#define MP_HEX_ADDRESS				0x13000
#define RESERVE_BLOCK_START_ADDR		0x1D000
#define RESERVE_BLOCK_END_ADDR			0x1DFFF
#define FW_VER_ADDR				0xFFE0
#define FW_BLOCK_INFO_NUM			7
#define SPI_UPGRADE_LEN				2048
#define SPI_READ_LEN				2048
#define FW_BLOCK_INFO_NUM			7

/* DMA Control Registers */
#define DMA_BASED_ADDR				0x72000
#define DMA48_ADDR				(DMA_BASED_ADDR + 0xC0)
#define DMA48_reg_dma_ch0_busy_flag		DMA48_ADDR
#define DMA48_reserved_0			0xFFFE
#define DMA48_reg_dma_ch0_trigger_sel		(BIT(16)|BIT(17)|BIT(18)|BIT(19))
#define DMA48_reserved_1			(BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define DMA48_reg_dma_ch0_start_set		BIT(24)
#define DMA48_reg_dma_ch0_start_clear		BIT(25)
#define DMA48_reg_dma_ch0_trigger_src_mask	BIT(26)
#define DMA48_reserved_2			BIT(27)

#define DMA49_ADDR				(DMA_BASED_ADDR + 0xC4)
#define DMA49_reg_dma_ch0_src1_addr		DMA49_ADDR
#define DMA49_reserved_0			BIT(20)

#define DMA50_ADDR				(DMA_BASED_ADDR + 0xC8)
#define DMA50_reg_dma_ch0_src1_step_inc		DMA50_ADDR
#define DMA50_reserved_0			(DMA50_ADDR + 0x01)
#define DMA50_reg_dma_ch0_src1_format		(BIT(24)|BIT(25))
#define DMA50_reserved_1			(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30))
#define DMA50_reg_dma_ch0_src1_en		BIT(31)

#define DMA52_ADDR				(DMA_BASED_ADDR + 0xD0)
#define DMA52_reg_dma_ch0_src2_step_inc		DMA52_ADDR
#define DMA52_reserved_0			(DMA52_ADDR + 0x01)
#define DMA52_reg_dma_ch0_src2_format		(BIT(24)|BIT(25))
#define DMA52_reserved_1			(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30))
#define DMA52_reg_dma_ch0_src2_en		BIT(31)

#define DMA53_ADDR				(DMA_BASED_ADDR + 0xD4)
#define DMA53_reg_dma_ch0_dest_addr		DMA53_ADDR
#define DMA53_reserved_0			BIT(20)

#define DMA54_ADDR				(DMA_BASED_ADDR + 0xD8)
#define DMA54_reg_dma_ch0_dest_step_inc		DMA54_ADDR
#define DMA54_reserved_0			(DMA54_ADDR + 0x01)
#define DMA54_reg_dma_ch0_dest_format		(BIT(24)|BIT(25))
#define DMA54_reserved_1			(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30))
#define DMA54_reg_dma_ch0_dest_en		BIT(31)

#define DMA55_ADDR				(DMA_BASED_ADDR + 0xDC)
#define DMA55_reg_dma_ch0_trafer_counts		DMA55_ADDR
#define DMA55_reserved_0			(BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define DMA55_reg_dma_ch0_trafer_mode		(BIT(24)|BIT(25)|BIT(26)|BIT(27))
#define DMA55_reserved_1			(BIT(28)|BIT(29)|BIT(30)|BIT(31))

/* INT Function Registers */
#define INTR_BASED_ADDR				0x48000
#define INTR1_ADDR				(INTR_BASED_ADDR + 0x4)
#define INTR1_reg_uart_tx_int_flag		INTR1_ADDR
#define INTR1_reserved_0			(BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))
#define INTR1_reg_wdt_alarm_int_flag		BIT(8)
#define INTR1_reserved_1			(BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define INTR1_reg_dma_ch1_int_flag		BIT(16)
#define INTR1_reg_dma_ch0_int_flag		BIT(17)
#define INTR1_reg_dma_frame_done_int_flag	BIT(18)
#define INTR1_reg_dma_tdi_done_int_flag		BIT(19)
#define INTR1_reserved_2			(BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define INTR1_reg_flash_error_flag		BIT(24)
#define INTR1_reg_flash_int_flag		BIT(25)
#define INTR1_reserved_3			BIT(26)

#define INTR2_ADDR					(INTR_BASED_ADDR + 0x8)
#define INTR2_td_int_flag_clear				INTR2_ADDR
#define INTR2_td_timeout_int_flag_clear			BIT(1)
#define INTR2_td_debug_frame_done_int_flag_clear	BIT(2)
#define INTR2_td_frame_start_scan_int_flag_clear	BIT(3)
#define INTR2_log_int_flag_clear			BIT(4)
#define INTR2_d2t_crc_err_int_flag_clear		BIT(8)
#define INTR2_d2t_flash_req_int_flag_clear		BIT(9)
#define INTR2_d2t_ddi_int_flag_clear			BIT(10)
#define INTR2_wr_done_int_flag_clear			BIT(16)
#define INTR2_rd_done_int_flag_clear			BIT(17)
#define INTR2_tdi_err_int_flag_clear			BIT(18)
#define INTR2_d2t_slpout_rise_flag_clear		BIT(24)
#define INTR2_d2t_slpout_fall_flag_clear		BIT(25)
#define INTR2_d2t_dstby_flag_clear			BIT(26)
#define INTR2_ddi_pwr_rdy_flag_clear			BIT(27)

#define INTR32_ADDR					(INTR_BASED_ADDR + 0x80)
#define INTR32_reg_ice_sw_int_en			INTR32_ADDR
#define INTR32_reg_ice_apb_conflict_int_en		BIT(1)
#define INTR32_reg_ice_ilm_conflict_int_en		BIT(2)
#define INTR32_reg_ice_dlm_conflict_int_en		BIT(3)
#define INTR32_reserved_0				(BIT(4)|BIT(5)|BIT(6)|BIT(7))
#define INTR32_reg_spi_sr_int_en			BIT(8)
#define INTR32_reg_spi_sp_int_en			BIT(9)
#define INTR32_reg_spi_trx_int_en			BIT(10)
#define INTR32_reg_spi_cmd_int_en			BIT(11)
#define INTR32_reg_spi_rw_int_en			BIT(12)
#define INTR32_reserved_1				(BIT(13)|BIT(14)|BIT(15))
#define INTR32_reg_i2c_start_int_en			BIT(16)
#define INTR32_reg_i2c_addr_match_int_en		BIT(17)
#define INTR32_reg_i2c_cmd_int_en			BIT(18)
#define INTR32_reg_i2c_sr_int_en			BIT(19)
#define INTR32_reg_i2c_trx_int_en			BIT(20)
#define INTR32_reg_i2c_rx_stop_int_en			BIT(21)
#define INTR32_reg_i2c_tx_stop_int_en			BIT(22)
#define INTR32_reserved_2				BIT(23)
#define INTR32_reg_t0_int_en				BIT(24)
#define INTR32_reg_t1_int_en				BIT(25)
#define INTR32_reserved_3				(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))

#define INTR33_ADDR					(INTR_BASED_ADDR + 0x84)
#define INTR33_reg_uart_tx_int_en			INTR33_ADDR
#define INTR33_reserved_0				(BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7))
#define INTR33_reg_wdt_alarm_int_en			BIT(8)
#define INTR33_reserved_1				(BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define INTR33_reg_dma_ch1_int_en			BIT(16)
#define INTR33_reg_dma_ch0_int_en			BIT(17)
#define INTR33_reg_dma_frame_done_int_en		BIT(18)
#define INTR33_reg_dma_tdi_done_int_en			BIT(19)
#define INTR33_reserved_2				(BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define INTR33_reg_flash_error_en			BIT(24)
#define INTR33_reg_flash_int_en				BIT(25)
#define INTR33_reserved_3				(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))

/* Flash */
#define FLASH_BASED_ADDR				0x41000
#define FLASH0_ADDR					(FLASH_BASED_ADDR + 0x0)
#define FLASH0_reg_flash_csb				FLASH0_ADDR
#define FLASH0_reserved_0				(BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define FLASH0_reg_preclk_sel				(BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define FLASH0_reg_rx_dual				BIT(24)
#define FLASH0_reg_tx_dual				BIT(25)
#define FLASH0_reserved_26				(BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))
#define FLASH1_ADDR					(FLASH_BASED_ADDR + 0x4)
#define FLASH1_reg_flash_key1				FLASH1_ADDR
#define FLASH1_reg_flash_key2				(FLASH1_ADDR + 0x01)
#define FLASH1_reg_flash_key3				(FLASH1_ADDR + 0x02)
#define FLASH1_reserved_0				(FLASH1_ADDR + 0x03)
#define FLASH2_ADDR					(FLASH_BASED_ADDR + 0x8)
#define FLASH2_reg_tx_data				FLASH2_ADDR
#define FLASH3_ADDR					(FLASH_BASED_ADDR + 0xC)
#define FLASH3_reg_rcv_cnt				FLASH3_ADDR
#define FLASH4_ADDR					(FLASH_BASED_ADDR + 0x10)
#define FLASH4_reg_rcv_data				FLASH4_ADDR
#define FLASH4_reg_rcv_dly				BIT(8)
#define FLASH4_reg_sutrg_en				BIT(9)
#define FLASH4_reserved_1				(BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15))
#define FLASH4_reg_rcv_data_valid_state			BIT(16)
#define FLASH4_reg_flash_rd_finish_state		BIT(17)
#define FLASH4_reserved_2				(BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23))
#define FLASH4_reg_flash_dma_trigger_en			(BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31))

/* The example for the gesture virtual keys */
#define GESTURE_DOUBLECLICK		0x58
#define GESTURE_UP			0x60
#define GESTURE_DOWN			0x61
#define GESTURE_LEFT			0x62
#define GESTURE_RIGHT			0x63
#define GESTURE_M			0x64
#define GESTURE_W			0x65
#define GESTURE_C			0x66
#define GESTURE_E			0x67
#define GESTURE_V			0x68
#define GESTURE_O			0x69
#define GESTURE_S			0x6A
#define GESTURE_Z			0x6B
#define KEY_GUSTURE_WAKEUP		KEY_WAKEUP
#define KEY_GESTURE_POWER		KEY_POWER
#define KEY_GESTURE_UP			KEY_UP
#define KEY_GESTURE_DOWN		KEY_DOWN
#define KEY_GESTURE_LEFT		KEY_LEFT
#define KEY_GESTURE_RIGHT		KEY_RIGHT
#define KEY_GESTURE_O			KEY_O
#define KEY_GESTURE_E			KEY_E
#define KEY_GESTURE_M			KEY_M
#define KEY_GESTURE_W			KEY_W
#define KEY_GESTURE_S			KEY_S
#define KEY_GESTURE_V			KEY_V
#define KEY_GESTURE_C			KEY_C
#define KEY_GESTURE_Z			KEY_Z
#define KEY_GESTURE_F			KEY_F
#define KEY_PACKET				KEY_E
#define GESTURE_CODE_V_DOWN		0x6C
#define GESTURE_CODE_V_LEFT		0x6D
#define GESTURE_CODE_V_RIGHT		0x6E
#define GESTURE_CODE_TWO_LINE_2_BOTTOM	0x6F
#define GESTURE_F			0x70
#define GESTURE_AT			0x71
#define ESD_GESTURE_PWD			0xF38A94EF
#define SPI_ESD_GESTURE_RUN		0x5B92E7F4
#define I2C_ESD_GESTURE_RUN		0xA67C9DFE
#define SPI_ESD_GESTURE_PWD_ADDR	0x25FF8
#define I2C_ESD_GESTURE_PWD_ADDR	0x40054

/* Protocol */
#define PROTOCOL_VER_500		0x050000
#define PROTOCOL_VER_510		0x050100
#define PROTOCOL_VER_520		0x050200
#define PROTOCOL_VER_530		0x050300
#define PROTOCOL_VER_540		0x050400
#define PROTOCOL_VER_550		0x050500
#define PROTOCOL_VER_560		0x050600
#define P5_X_READ_DATA_CTRL		0xF6
#define P5_X_GET_TP_INFORMATION		0x20
#define P5_X_GET_KEY_INFORMATION	0x27
#define P5_X_GET_PANEL_INFORMATION	0x29
#define P5_X_GET_FW_VERSION		0x21
#define P5_X_GET_PROTOCOL_VERSION	0x22
#define P5_X_GET_CORE_VERSION		0x23
#define P5_X_MODE_CONTROL		0xF0
#define P5_X_SET_CDC_INIT		0xF1
#define P5_X_GET_CDC_DATA		0xF2
#define P5_X_CDC_BUSY_STATE		0xF3
#define P5_X_MP_TEST_MODE_INFO		0xFE
#define P5_X_I2C_UART			0x40
#define P5_X_FW_UNKNOWN_MODE		0xFF
#define P5_X_FW_DEMO_MODE		0x00
#define P5_X_FW_TEST_MODE		0x01
#define P5_X_FW_DEBUG_MODE		0x02
#define P5_X_FW_DEMO_DEBUG_INFO_MODE	0x04
#define P5_X_FW_SOP_FLOW_MODE		0xE0
#define P5_X_FW_ESD_MODE		0xFA
#define P5_X_FW_GESTURE_MODE		0x0F
#define P5_X_FW_GESTURE_NORMAL_MODE	0x01
#define P5_X_FW_GESTURE_INFO_MODE	0x02
#define P5_X_FW_DELTA_DATA_MODE		0x03
#define P5_X_FW_RAW_DATA_MODE		0x08
#define P5_X_DEMO_MODE_PACKET_LENGTH	43
#define P5_X_DEBUG_MODE_PACKET_LENGTH	1280
#define P5_X_TEST_MODE_PACKET_LENGTH	1180
#define P5_X_GESTURE_NORMAL_LENGTH	8
#define P5_X_GESTURE_INFO_LENGTH	170
#define P5_X_DEMO_PACKET_ID		0x5A
#define P5_X_LARGE_AREA_RELEASE_PACKET_ID	0x88
#define P5_X_LARGE_AREA_PRESS_PACKET_ID		0x78
#define P5_X_DEBUG_PACKET_ID		0xA7
#define P5_X_TEST_PACKET_ID		0xF2
#define P5_X_GESTURE_PACKET_ID		0xAA
#define P5_X_I2CUART_PACKET_ID		0x7A
#define P5_X_EDGE_PLAM_CTRL_1		0x01
#define P5_X_EDGE_PLAM_CTRL_2		0x12
#define SPI_WRITE			0x82
#define SPI_READ			0x83
#define SPI_ACK				0xA3
#define TDDI_WDT_ON			0xA5
#define TDDI_WDT_OFF			0x5A

/* Chips */
#define TDDI_PID_ADDR			0x4009C
#define TDDI_OTP_ID_ADDR		0x400A0
#define TDDI_ANA_ID_ADDR		0x400A4
#define TDDI_PC_COUNTER_ADDR		0x44008
#define TDDI_WDT_ADDR			0x5100C
#define TDDI_WDT_ACTIVE_ADDR		0x51018
#define TDDI_CHIP_RESET_ADDR		0x40050
#define ILI9881_CHIP			0x9881
#define ILI9881F_AA			0x98810F00
#define ILI9881H_AD			0x98811103
#define ILI9881H_AE			0x98811104
#define ILI7807_CHIP			0x7807
#define ILI7807G_AA			0x78071000
#define ILI7807G_AB			0x78071001
#define RAWDATA_NO_BK_SHIFT_9881H	8192
#define RAWDATA_NO_BK_SHIFT_9881F	4096
#define INFO_HEX_START_ADDR_64K		0xFFB4

#define	WAKEUP_OFF		0x04
#define	WAKEUP_ON		0x05

#define RESULT_INVALID		0
#define RESULT_PASS			2
#define RESULT_NG			1
#define TEST_SPI			1
#define TEST_SHORT			2
#define TEST_OPEN			3
#define TEST_INVAILD		-1

struct ilitek_tddi_dev {
	struct i2c_client *i2c;
	struct spi_device *spi;
	struct input_dev *input;
	struct device *dev;
	struct wakeup_source *gesture_process_ws;
	struct ilitek_hwif_info *hwif;
	struct ilitek_ic_info *chip;
	struct ilitek_protocol_info *protocol;

	struct regulator *vdd;
	struct regulator *vcc;

#ifdef CONFIG_FB
	struct notifier_block notifier_fb;
#else
	struct early_suspend early_suspend;
#endif

	struct mutex touch_mutex;
	struct mutex debug_mutex;
	struct mutex debug_read_mutex;
	spinlock_t irq_spin;

	/* physical path to the input device in the system hierarchy */
	const char *phys;

	u16 max_x;
	u16 max_y;
	u16 min_x;
	u16 min_y;
	u16 panel_wid;
	u16 panel_hei;
	u8 xch_num;
	u8 ych_num;
	u8 stx;
	u8 srx;
	u8 *fw_dma_buf;

	int actual_tp_mode;

	int irq_num;
	int tp_rst;
	int tp_int;

	int finger;
	int curt_touch[MAX_TOUCH_NUM];
	int prev_touch[MAX_TOUCH_NUM];
	int last_touch;

	int fw_retry;
	int fw_update_stat;
	int fw_open;
	bool wq_esd_ctrl;
	bool wq_bat_ctrl;

	bool netlink;
	bool report;
	bool gesture;
	int gesture_mode;
	bool gesture_debug;

	u16 flash_mid;
	u16 flash_devid;
	int program_page;
	int flash_sector;

	/* Sending report data to users for the debug */
	bool debug_node_open;
	int debug_data_frame;
	wait_queue_head_t inq;
	unsigned char **debug_buf;
	int raw_count;
	int delta_count;
	int bg_count;

	int reset;
	int rst_edge_delay;
	int fw_upgrade_mode;
	bool wtd_ctrl;
	bool do_otp_check;
	bool fw_uart_en;
	bool force_fw_update;
	u8 mp_test_result[5];
	bool irq_after_recovery;

	atomic_t irq_stat;
	atomic_t tp_reset;
	atomic_t ice_stat;
	atomic_t fw_stat;
	atomic_t mp_stat;
	atomic_t tp_sleep;
	atomic_t tp_sw_mode;
	atomic_t mp_int_check;
	atomic_t esd_stat;
	atomic_t input_stat;
	atomic_t first_starting_up_reset_stat;

	int (*write)(void *data, int len);
	int (*read)(void *data, int len);
	int (*spi_write_then_read)(struct spi_device *spi, const void *txbuf,
				unsigned n_tx, void *rxbuf, unsigned n_rx);
	int (*mp_move_code)(void);
	int (*gesture_move_code)(int mode);
	int (*esd_recover)(void);
	void (*spi_speed)(bool enable);
	void (*ges_recover)(void);
};
extern struct ilitek_tddi_dev *idev;

struct ilitek_touch_info {
	u16 id;
	u16 x;
	u16 y;
	u16 pressure;
};

struct ilitek_protocol_info {
	u32 ver;
	int fw_ver_len;
	int pro_ver_len;
	int tp_info_len;
	int key_info_len;
	int panel_info_len;
	int core_ver_len;
	int func_ctrl_len;
	int window_len;
	int cdc_len;
	int mp_info_len;
};

struct ilitek_ic_func_ctrl {
	const char *name;
	u8 cmd[3];
	int len;
};

struct ilitek_ic_info {
	u32 pid_addr;
	u32 wdt_addr;
	u32 pc_counter_addr;
	u32 reset_addr;
	u32 otp_addr;
	u32 ana_addr;
	u32 pid;
	u16 id;
	u16 type_hi;
	u16 type_low;
	u32 otp_id;
	u32 ana_id;
	u32 fw_ver;
	u32 core_ver;
	u32 max_count;
	u32 reset_key;
	u16 wtd_key;
	u8 read_reg_data;
	int no_bk_shift;
	bool spi_speed_ctrl;
	s32 (*open_sp_formula)(int dac, int raw);
	s32 (*open_c_formula)(int dac, int raw, int tvch, int gain);
	void (*hd_dma_check_crc_off)(void);
	u8  info[75];
	bool  info_from_hex;
	u32 info_addr;


};

struct ilitek_hwif_info {
	u8 bus_type;
	u8 plat_type;
	const char *name;
	struct module *owner;
	const struct of_device_id *of_match_table;
	int (*plat_probe)(void);
	int (*plat_remove)(void);
	void *info;
};

/* Prototypes for tddi firmware/flash functions */
extern void ilitek_tddi_ic_check_otp_prog_mode(void);
extern void ilitek_tddi_flash_dma_write(u32 start, u32 end, u32 len);
extern void ilitek_tddi_flash_clear_dma(void);
extern void ilitek_tddi_fw_read_flash_info(bool mode);
extern u32 ilitek_tddi_fw_read_hw_crc(u32 start, u32 end);
extern int ilitek_tddi_fw_read_flash(u32 start, u32 end, u8 *data, int len);
extern int ilitek_tddi_fw_dump_iram_data(u32 start, u32 end);
extern int ilitek_tddi_fw_dump_flash_data(u32 start, u32 end, bool user);
extern int ilitek_tddi_fw_upgrade(int upgrade_type, int file_type, int open_file_method);

/* Prototypes for tddi mp test */
extern int ilitek_tddi_mp_test_main(char *apk, bool lcm_on);

/* Prototypes for tddi core functions */
extern void ilitek_tddi_touch_esd_gesture_flash(void);
extern void ilitek_tddi_touch_esd_gesture_iram(void);
extern int ilitek_tddi_move_gesture_code_flash(int mode);
extern int ilitek_tddi_move_gesture_code_iram(int mode);
extern int ilitek_tddi_move_mp_code_flash(void);
extern int ilitek_tddi_move_mp_code_iram(void);
extern void ilitek_tddi_touch_press(u16 x, u16 y, u16 pressure, u16 id);
extern void ilitek_tddi_touch_release(u16 x, u16 y, u16 id);
extern void ilitek_tddi_touch_release_all_point(void);
extern void ilitek_tddi_report_ap_mode(u8 *buf, int len);
extern void ilitek_tddi_report_debug_mode(u8 *buf, int rlen);
extern void ilitek_tddi_report_gesture_mode(u8 *buf, int rlen);
extern void ilitek_tddi_report_i2cuart_mode(u8 *buf, int rlen);
extern int ilitek_tddi_ic_watch_dog_ctrl(bool write, bool enable);
extern void ilitek_tddi_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data);
extern void ilitek_tddi_ic_get_ddi_reg_onepage(u8 page, u8 reg);
extern void ilitek_tddi_ic_spi_speed_ctrl(bool enable);
extern int ilitek_tddi_ic_whole_reset(void);
extern int ilitek_tddi_ic_code_reset(void);
extern int ilitek_tddi_ic_func_ctrl(const char *name, int ctrl);
extern u32 ilitek_tddi_ic_get_pc_counter(void);
extern int ilitek_tddi_ic_check_int_stat(void);
extern int ilitek_tddi_ic_check_busy(int count, int delay);
extern int ilitek_tddi_ic_get_project_id(u8 *pdata, int size);
extern int ilitek_tddi_ic_get_panel_info(void);
extern int ilitek_tddi_ic_get_spi_panel_info(void);
extern int ilitek_tddi_ic_get_tp_info(void);
extern int ilitek_tddi_ic_get_core_ver(void);
extern int ilitek_tddi_ic_get_protocl_ver(void);
extern int ilitek_tddi_ic_get_fw_ver(void);
extern int ilitek_tddi_ic_get_info(void);
extern int ilitek_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value);
extern int ilitek_ice_mode_write(u32 addr, u32 data, int len);
extern int ilitek_ice_mode_read(u32 addr, u32 *data, int len);
extern int ilitek_ice_mode_ctrl(bool enable, bool mcu);
extern void ilitek_tddi_ic_init(void);
extern int ilitek_tddi_edge_palm_ctrl(u8 type);

/* Prototypes for tddi events */
extern int ilitek_tddi_switch_mode(u8 *data);
extern int ilitek_tddi_fw_upgrade_handler(void *data);
extern int ilitek_tddi_wq_esd_i2c_check(void);
extern int ilitek_tddi_wq_esd_spi_check(void);
extern void ilitek_tddi_gesture_recovery(void);
extern void ilitek_tddi_spi_recovery(void);
extern void ilitek_tddi_wq_ctrl(int type, int ctrl);
extern int ilitek_tddi_mp_test_handler(char *apk, bool lcm_on);
extern void ilitek_tddi_report_handler(void);
extern int ilitek_tddi_sleep_handler(int mode);
extern int ilitek_tddi_reset_ctrl(int mode);
extern int ilitek_tddi_init(void);
extern int ilitek_tddi_dev_init(struct ilitek_hwif_info *hwif);
extern void ilitek_tddi_dev_remove(void);
extern int str2hex(char *str);

/* Prototypes for i2c/spi interface */
extern int ilitek_tddi_interface_dev_init(struct ilitek_hwif_info *hwif);
extern void ilitek_tddi_interface_dev_exit(struct ilitek_hwif_info *hwif);
extern int ilitek_spi_write_then_read_split(struct spi_device *spi,
				const void *txbuf, unsigned n_tx,
				void *rxbuf, unsigned n_rx);
extern int ilitek_spi_write_then_read_direct(struct spi_device *spi,
				const void *txbuf, unsigned n_tx,
				void *rxbuf, unsigned n_rx);

/* Prototypes for platform level */
extern void ilitek_plat_regulator_power_on(bool status);
extern void ilitek_plat_input_register(void);
extern void ilitek_plat_irq_disable(void);
extern void ilitek_plat_irq_enable(void);
extern void ilitek_plat_tp_reset(void);

/* Prototypes for miscs */
extern void ilitek_tddi_node_init(void);
extern void ilitek_dump_data(void *data, int type, int len, int row_len, const char *name);
extern u8 ilitek_calc_packet_checksum(u8 *packet, int len);
extern void netlink_reply_msg(void *raw, int size);
extern int katoi(char *str);
extern void ilitek_call_resume_work(void);

extern void ilitek_tddi_gesture_recovery(void);
extern int ilitek_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value);
extern bool ilitek_gesture_flag;
extern void ilitek_tddi_spi_recovery(void);
extern void ilitek_tddi_fw_print_iram_data(void);

static inline void ipio_kfree(void **mem)
{
	if (*mem != NULL) {
		kfree(*mem);
		*mem = NULL;
	}
}

static inline void ipio_vfree(void **mem)
{
	if (*mem != NULL) {
		vfree(*mem);
		*mem = NULL;
	}
}

static inline void *ipio_memcpy(void *dest, const void *src, int n, int dest_size)
{
	if (n > dest_size)
		 n = dest_size;

	return memcpy(dest, src, n);
}

static inline s32 open_sp_formula_ili9881(int dac, int raw)
{
	return (int)((int)(dac * 2 * 10000 * 161 / 100) - (int)(16384 / 2 - (int)raw) * 20000 * 7 / 16384 * 36 / 10) / 31 / 2;
}

static inline s32 open_sp_formula_ili7807(int dac, int raw)
{
	return (int)((int)(dac * 2 * 10000 * 131 / 100) - (int)(16384 / 2 - (int)raw) * 20000 * 7 / 16384 * 36 / 10) / 31 / 2;
}

static inline s32 open_c_formula(int dac, int raw, int tvch, int gain)
{
	return (int)((int)(dac * 414 * 39 / 2) + (int)(((int)raw - 8192) * 36 * (7 * 100 - 22) * 10 / 16384)) /
						tvch / 100 / gain;
}

static inline void firmware_hd_dma_crc_off_ili9881(void)
{
	/* crc off */
	ilitek_ice_mode_write(0x041016, 0x00, 1);
	/* dma crc */
	ilitek_ice_mode_write(0x041048, 0x00000001, 4);
}

static inline void firmware_hd_dma_crc_off_ili7807(void)
{
	/* crc off */
	ilitek_ice_mode_write(0x041016, 0x00, 1);
	/* dma crc */
	ilitek_ice_mode_write(0x041017, 0x03, 1);
}

#endif /* __ILITEK_H */
