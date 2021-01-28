/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <linux/pm_wakeup.h>
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

#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#endif

#ifdef CONFIG_MTK_SPI
#include <linux/spi/spi.h>
#endif

#include "tpd.h"
#define DRIVER_VERSION			"2.0.10.0.200801"

/* Options */
#define TDDI_INTERFACE			BUS_SPI /* BUS_I2C(0x18) or BUS_SPI(0x1C) */
#define VDD_VOLTAGE			1800000
#define VCC_VOLTAGE			1800000
#define SPI_CLK                         1      /* follow by clk list */
#define SPI_RETRY			5
#define IRQ_GPIO_NUM			66
#define TR_BUF_SIZE			(2*K) /* Buffer size of touch report */
#define TR_BUF_LIST_SIZE		(1*K) /* Buffer size of touch report */
#define SPI_TX_BUF_SIZE  		4096
#define SPI_RX_BUF_SIZE  		4096
#define WQ_ESD_DELAY			4000
#define WQ_BAT_DELAY			2000
#define AP_INT_TIMEOUT			600 /*600ms*/
#define MP_INT_TIMEOUT			5000 /*5s*/
#define MT_B_TYPE			ENABLE
#define TDDI_RST_BIND			DISABLE
#define MT_PRESSURE			DISABLE
#define ENABLE_WQ_ESD			DISABLE
#define ENABLE_WQ_BAT			DISABLE
#define ENABLE_GESTURE			DISABLE
#define REGULATOR_POWER			DISABLE
#define TP_SUSPEND_PRIO			ENABLE
#define RESUME_BY_DDI			DISABLE
#define BOOT_FW_UPDATE			DISABLE
#define PLL_CLK_WAKEUP_TP_RESUME	DISABLE
#define CHARGER_NOTIFIER_CALLBACK	DISABLE
#define ENABLE_EDGE_PALM_PARA		DISABLE

/* SPI Mode */
#define SPI_ICE_MODE			0
#define SPI_SLAVE_MODE			1
#define SPI_MODE			SPI_SLAVE_MODE
#define SPI_MODE_DELAY			mdelay(1)

/*if current interface is spi, must to hostdownload */
#if (TDDI_INTERFACE == BUS_SPI)
#define HOST_DOWN_LOAD			ENABLE
#else
#define HOST_DOWN_LOAD			DISABLE
#endif

/* Plaform compatibility */
#define CONFIG_PLAT_SPRD		DISABLE
#define I2C_DMA_TRANSFER		DISABLE
#define SPI_DMA_TRANSFER_SPLIT		DISABLE
#define SPRD_SYSFS_SUSPEND_RESUME	DISABLE

/* Path */
#define DEBUG_DATA_FILE_SIZE		(10*K)
#define DEBUG_DATA_FILE_PATH		"/sdcard/ILITEK_log.csv"
#define CSV_LCM_ON_PATH			"/sdcard/ilitek_mp_lcm_on_log"
#define CSV_LCM_OFF_PATH		"/sdcard/ilitek_mp_lcm_off_log"
#define POWER_STATUS_PATH		"/sys/class/power_supply/battery/status"
#define DUMP_FLASH_PATH			"/sdcard/flash_dump"
#define DUMP_IRAM_PATH			"/sdcard/iram_dump"

/* Debug messages */
#define DEBUG_NONE	0
#define DEBUG_ALL	1
#define DEBUG_OUTPUT	DEBUG_NONE

#define ipio_info(fmt, arg...)						\
({									\
	pr_info("ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

#define ipio_err(fmt, arg...)						\
({									\
	pr_info("ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
})									\

extern bool ipio_debug_level;
#define ipio_debug(fmt, arg...)						\
do {									\
	if (ipio_debug_level)						\
	pr_info("ILITEK: (%s, %d): " fmt, __func__, __LINE__, ##arg);	\
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
#define DO_I2C_RECOVER		-3
#define SPI_IS_LOCKED		INT_MAX

enum TP_SPI_CLK_LIST {
	TP_SPI_CLK_1M = 1000000,
	TP_SPI_CLK_2M = 2000000,
	TP_SPI_CLK_3M = 3000000,
	TP_SPI_CLK_4M =	4000000,
	TP_SPI_CLK_5M =	5000000,
	TP_SPI_CLK_6M =	6000000,
	TP_SPI_CLK_7M =	7000000,
	TP_SPI_CLK_8M = 8000000,
	TP_SPI_CLK_9M = 9000000,
	TP_SPI_CLK_10M = 10000000,
	TP_SPI_CLK_11M = 11000000,
	TP_SPI_CLK_12M = 12000000,
	TP_SPI_CLK_13M = 13000000,
	TP_SPI_CLK_14M = 14000000,
	TP_SPI_CLK_15M = 15000000,
};

enum READ_WRITE_CASE {
	RW_SYNC = 0,
	R_ONLY,
	W_ONLY
};

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

enum TP_FW_UPGRADE_STATUS {
	FW_STAT_INIT = 0,
	FW_UPDATE_PASS = 100,
	FW_UPDATE_FAIL = -1
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

enum TP_PROXIMITY_STATUS {
	DDI_POWER_OFF = 0,
	DDI_POWER_ON = 1,
	WAKE_UP_GESTURE_RECOVERY = 2,
	WAKE_UP_SWITCH_GESTURE_MODE = 3
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

enum TP_RECORD_DATA {
	DISABLE_RECORD = 0,
	ENABLE_RECORD,
	DATA_RECORD
};

enum TP_DATA_FORMAT {
	DATA_FORMAT_DEMO = 0,
	DATA_FORMAT_DEBUG,
	DATA_FORMAT_DEMO_DEBUG_INFO,
	DATA_FORMAT_GESTURE_SPECIAL_DEMO,
	DATA_FORMAT_GESTURE_INFO,
	DATA_FORMAT_GESTURE_NORMAL,
	DATA_FORMAT_GESTURE_DEMO,
	DATA_FORMAT_GESTURE_DEBUG,
	DATA_FORMAT_DEBUG_LITE_ROI,
	DATA_FORMAT_DEBUG_LITE_WINDOW,
	DATA_FORMAT_DEBUG_LITE_AREA
};

enum NODE_MODE_SWITCH {
	AP_MODE = 0,
	TEST_MODE,
	DEBUG_MODE,
	DEBUG_LITE_ROI,
	DEBUG_LITE_WINDOW,
	DEBUG_LITE_AREA
};

enum TP_MODEL {
	MODEL_DEF = 0,
	MODEL_CSOT,
	MODEL_AUO,
	MODEL_BOE,
	MODEL_INX,
	MODEL_DJ,
	MODEL_TXD,
	MODEL_TM
};

enum TP_ERR_CODE {
	EMP_CMD = 100,
	EMP_PROTOCOL,
	EMP_FILE,
	EMP_INI,
	EMP_TIMING_INFO,
	EMP_INVAL,
	EMP_PARSE,
	EMP_NOMEM,
	EMP_GET_CDC,
	EMP_INT,
	EMP_CHECK_BUY,
	EMP_MODE,
	EMP_FW_PROC,
	EMP_FORMUL_NULL,
	EMP_PARA_NULL,
	EFW_CONVERT_FILE,
	EFW_ICE_MODE,
	EFW_WDT,
	EFW_CRC,
	EFW_REST,
	EFW_ERASE,
	EFW_PROGRAM,
	EFW_INTERFACE,
};

enum TP_IC_TYPE {
	ILI_A = 0x0A,
	ILI_B,
	ILI_C,
	ILI_D,
	ILI_E,
	ILI_F,
	ILI_G,
	ILI_H,
	ILI_I,
	ILI_J,
	ILI_K,
	ILI_L,
	ILI_M,
	ILI_N,
	ILI_O,
	ILI_P,
	ILI_Q,
	ILI_R,
	ILI_S,
	ILI_T,
	ILI_U,
	ILI_V,
	ILI_W,
	ILI_X,
	ILI_Y,
	ILI_Z,
};

typedef enum cmd_types{
    CMD_DISABLE = 0x00,
    CMD_ENABLE = 0x01,
    CMD_STATUS = 0x02,
    CMD_ROI_DATA = 0x03,
}cmd_types;

struct gesture_symbol {
	u8 double_tap                 :1;
	u8 alphabet_line_2_top        :1;
	u8 alphabet_line_2_bottom     :1;
	u8 alphabet_line_2_left       :1;
	u8 alphabet_line_2_right      :1;
	u8 alphabet_m                 :1;
	u8 alphabet_w                 :1;
	u8 alphabet_c                 :1;
	u8 alphabet_E                 :1;
	u8 alphabet_V                 :1;
	u8 alphabet_O                 :1;
	u8 alphabet_S                 :1;
	u8 alphabet_Z                 :1;
	u8 alphabet_V_down            :1;
	u8 alphabet_V_left            :1;
	u8 alphabet_V_right           :1;
	u8 alphabet_two_line_2_bottom :1;
	u8 alphabet_F                 :1;
	u8 alphabet_AT                :1;
	u8 reserve0 		      :5;
};

 struct report_info_block {
	 u8 nReportByPixel	 :1;
	 u8 nIsHostDownload	 :1;
	 u8 nIsSPIICE		 :1;
	 u8 nIsSPISLAVE 	 :1;
	 u8 nIsI2C		 :1;
	 u8 nReserved00 	 :3;
	 u8 nReserved01 	 :8;
	 u8 nReserved02 	 :8;
	 u8 nReserved03 	 :8;
 };

#define TDDI_I2C_ADDR				0x41
#define TDDI_DEV_ID				"ilitek"

 /* define the width and heigth of a screen. */
#define TOUCH_SCREEN_X_MIN			0
#define TOUCH_SCREEN_Y_MIN			0
#define TOUCH_SCREEN_X_MAX			720
#define TOUCH_SCREEN_Y_MAX			1440
#define MAX_TOUCH_NUM				10
#define ILITEK_KNUCKLE_ROI_FINGERS		2

/* define the range on space, don't change */
#define TPD_HEIGHT				2048
#define TPD_WIDTH				2048

/* Firmware upgrade */
#define CORE_VER_1410				0x01040100
#define CORE_VER_1420				0x01040200
#define CORE_VER_1430				0x01040300
#define CORE_VER_1470				0x01040700
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
#define INFO_HEX_ST_ADDR			0x4F
#define INFO_MP_HEX_ADDR			0x1F
#define FW_BLOCK_INFO_NUM			8
#define SPI_UPGRADE_LEN				2048

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

/* Dummy Registers */
#define WDT_DUMMY_BASED_ADDR				0x5101C
#define WDT7_DUMMY0					WDT_DUMMY_BASED_ADDR
#define WDT8_DUMMY1					(WDT_DUMMY_BASED_ADDR + 0x04)
#define WDT9_DUMMY2					(WDT_DUMMY_BASED_ADDR + 0x08)

/* The example for the gesture virtual keys */
#define GESTURE_DOUBLECLICK				0x58
#define GESTURE_UP					0x60
#define GESTURE_DOWN					0x61
#define GESTURE_LEFT					0x62
#define GESTURE_RIGHT					0x63
#define GESTURE_M					0x64
#define GESTURE_W					0x65
#define GESTURE_C					0x66
#define GESTURE_E					0x67
#define GESTURE_V					0x68
#define GESTURE_O					0x69
#define GESTURE_S					0x6A
#define GESTURE_Z					0x6B
#define KEY_GESTURE_POWER				KEY_POWER
#define KEY_GESTURE_UP					KEY_UP
#define KEY_GESTURE_DOWN				KEY_DOWN
#define KEY_GESTURE_LEFT				KEY_LEFT
#define KEY_GESTURE_RIGHT				KEY_RIGHT
#define KEY_GESTURE_O					KEY_O
#define KEY_GESTURE_E					KEY_E
#define KEY_GESTURE_M					KEY_M
#define KEY_GESTURE_W					KEY_W
#define KEY_GESTURE_S					KEY_S
#define KEY_GESTURE_V					KEY_V
#define KEY_GESTURE_C					KEY_C
#define KEY_GESTURE_Z					KEY_Z
#define KEY_GESTURE_F					KEY_F
#define GESTURE_V_DOWN					0x6C
#define GESTURE_V_LEFT					0x6D
#define GESTURE_V_RIGHT  				0x6E
#define GESTURE_TWOLINE_DOWN				0x6F
#define GESTURE_F					0x70
#define GESTURE_AT					0x71
#define ESD_GESTURE_PWD					0xF38A94EF
#define SPI_ESD_GESTURE_RUN				0x5B92E7F4
#define I2C_ESD_GESTURE_RUN				0xA67C9DFE
#define SPI_ESD_GESTURE_PWD_ADDR			0x25FF8
#define I2C_ESD_GESTURE_PWD_ADDR			0x40054

/* FW data format */
#define DATA_FORMAT_DEMO_CMD				0x00
#define DATA_FORMAT_DEBUG_CMD				0x02
#define DATA_FORMAT_DEMO_DEBUG_INFO_CMD 		0x04
#define DATA_FORMAT_GESTURE_NORMAL_CMD 			0x01
#define DATA_FORMAT_GESTURE_INFO_CMD			0x02
#define DATA_FORMAT_DEBUG_LITE_CMD			0x05
#define DATA_FORMAT_DEBUG_LITE_ROI_CMD			0x01
#define DATA_FORMAT_DEBUG_LITE_WINDOW_CMD		0x02
#define DATA_FORMAT_DEBUG_LITE_AREA_CMD			0x03
#define P5_X_DEMO_MODE_PACKET_LEN			43
#define P5_X_INFO_HEADER_LENGTH				3
#define P5_X_INFO_CHECKSUM_LENGTH			1
#define P5_X_DEMO_DEBUG_INFO_ID0_LENGTH			14
#define P5_X_DEBUG_MODE_PACKET_LENGTH			1280
#define P5_X_TEST_MODE_PACKET_LENGTH			1180
#define P5_X_GESTURE_NORMAL_LENGTH			8
#define P5_X_GESTURE_INFO_LENGTH			170
#define P5_X_DEBUG_LITE_LENGTH				300
#define P5_X_CORE_VER_THREE_LENGTH			5
#define P5_X_CORE_VER_FOUR_LENGTH			6
#define P5_X_EDGE_PALM_PARA_LENGTH			31

#define DOUBLE_TAP                                   	( ON )//BIT0
#define ALPHABET_LINE_2_TOP                          	( ON )//BIT1
#define ALPHABET_LINE_2_BOTTOM                   	( ON )//BIT2
#define ALPHABET_LINE_2_LEFT                         	( ON )//BIT3
#define ALPHABET_LINE_2_RIGHT                        	( ON )//BIT4
#define ALPHABET_M					( ON )//BIT5
#define ALPHABET_W					( ON )//BIT6
#define ALPHABET_C					( ON )//BIT7
#define ALPHABET_E					( ON )//BIT8
#define ALPHABET_V					( ON )//BIT9
#define ALPHABET_O					( ON )//BIT10
#define ALPHABET_S					( ON )//BIT11
#define ALPHABET_Z					( ON )//BIT12
#define ALPHABET_V_DOWN					( ON )//BIT13
#define ALPHABET_V_LEFT					( ON )//BIT14
#define ALPHABET_V_RIGHT				( ON )//BIT15
#define ALPHABET_TWO_LINE_2_BOTTOM			( ON )//BIT16
#define ALPHABET_F					( ON )//BIT17
#define ALPHABET_AT					( ON )//BIT18

/* Protocol */
#define PROTOCOL_VER_500				0x050000
#define PROTOCOL_VER_510				0x050100
#define PROTOCOL_VER_520				0x050200
#define PROTOCOL_VER_530				0x050300
#define PROTOCOL_VER_540				0x050400
#define PROTOCOL_VER_550				0x050500
#define PROTOCOL_VER_560				0x050600
#define PROTOCOL_VER_570				0x050700
#define P5_X_READ_DATA_CTRL				0xF6
#define P5_X_GET_TP_INFORMATION				0x20
#define P5_X_GET_KEY_INFORMATION			0x27
#define P5_X_GET_PANEL_INFORMATION			0x29
#define P5_X_GET_FW_VERSION				0x21
#define P5_X_GET_PROTOCOL_VERSION			0x22
#define P5_X_GET_CORE_VERSION				0x23
#define P5_X_GET_CORE_VERSION_NEW			0x24
#define P5_X_MODE_CONTROL				0xF0
#define P5_X_SET_CDC_INIT				0xF1
#define P5_X_GET_CDC_DATA				0xF2
#define P5_X_CDC_BUSY_STATE				0xF3
#define P5_X_MP_MODE_CHECK				0xFD
#define P5_X_MP_TEST_MODE_INFO				0xFE
#define P5_X_I2C_UART					0x40
#define P5_X_FW_UNKNOWN_MODE				0xFF
#define P5_X_FW_AP_MODE					0x00
#define P5_X_FW_TEST_MODE				0x01
#define P5_X_FW_GESTURE_MODE				0x0F
#define P5_X_FW_DELTA_DATA_MODE				0x03
#define P5_X_FW_RAW_DATA_MODE				0x08
#define P5_X_DEMO_PACKET_ID				0x5A
#define P5_X_DEBUG_PACKET_ID				0xA7
#define P5_X_TEST_PACKET_ID				0xF2
#define P5_X_GESTURE_PACKET_ID				0xAA
#define P5_X_GESTURE_FAIL_ID				0xAE
#define P5_X_I2CUART_PACKET_ID				0x7A
#define P5_X_DEBUG_LITE_PACKET_ID			0x9A
#define P5_X_INFO_HEADER_PACKET_ID			0xB7
#define P5_X_DEMO_DEBUG_INFO_PACKET_ID			0x5C
#define P5_X_EDGE_PLAM_CTRL_1				0x01
#define P5_X_EDGE_PLAM_CTRL_2				0x12
#define P5_X_EDGE_PALM_TUNING_PARA			0x1E
#define SPI_WRITE					0x82
#define SPI_READ					0x83
#define SPI_ACK						0xA3
#define TDDI_WDT_ON					0xA5
#define TDDI_WDT_OFF					0x5A

/* Chips */
#define TDDI_PID_ADDR					0x4009C
#define TDDI_OTP_ID_ADDR				0x400A0
#define TDDI_ANA_ID_ADDR				0x400A4
#define TDDI_PC_COUNTER_ADDR				0x44008
#define TDDI_PC_LATCH_ADDR				0x51010
#define TDDI_WDT_ADDR					0x5100C
#define TDDI_WDT_ACTIVE_ADDR				0x51018
#define TDDI_CHIP_RESET_ADDR				0x40050
#define ILI9881_CHIP					0x9881
#define ILI9881F_AA					0x98810F00
#define ILI9881H_AD					0x98811103
#define ILI9881H_AE					0x98811104
#define ILI7807_CHIP					0x7807
#define ILI7807G_AA					0x78071000
#define ILI7807G_AB					0x78071001
#define ILI7807G_AH					0x78071007
#define ILI7807R_AA					0x78071B00
#define ILI9882_CHIP					0x9882
#define ILI7807R					0x78071B
#define ILI7807G					0x780710
#define ILI9881H					0x988111
#define RAWDATA_NO_BK_SHIFT_9881H			8192
#define RAWDATA_NO_BK_SHIFT_9881F			4096

/* Edge Palm */
#define ZONE_A_W					6
#define ZONE_B_W					32
#define ZONE_A_ROTATION_W				6
#define ZONE_B_ROTATION_W				32
#define ZONE_C_WIDTH					80
#define ZONE_C_HEIGHT					320
#define ZONE_C_HEIGHT_LITTLE				100

struct ilitek_tddi_dev {
	struct i2c_client *i2c;
	struct spi_device *spi;
	struct input_dev *input;
	struct device *dev;
	struct wakeup_source *ws;

	struct ilitek_hwif_info *hwif;
	struct ilitek_ic_info *chip;
	struct ilitek_protocol_info *protocol;
	struct gesture_coordinate *gcoord;
	struct regulator *vdd;
	struct regulator *vcc;

#ifdef CONFIG_FB
	struct notifier_block notifier_fb;
#else
	struct early_suspend early_suspend;
#endif
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
/* add_for_charger_start */
	struct notifier_block notifier_charger;
	struct workqueue_struct *charger_notify_wq;
	struct work_struct	update_charger;
	int usb_plug_status;
/*  add_for_charger_end  */
#endif
#endif
#ifdef ROI
	struct ts_kit_device_data *ts_dev_data;
#endif
	struct mutex touch_mutex;
	struct mutex debug_mutex;
	struct mutex debug_read_mutex;
	spinlock_t irq_spin;

	struct completion pm_completion;
	bool pm_suspend;
	/* physical path to the input device in the system hierarchy */
	const char *phys;

	bool boot;
	u32 fw_pc;
	u32 fw_latch;

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
	u8 *update_buf;
	u8 *tr_buf;
	u8 *spi_tx;
	u8 *spi_rx;
	u8 spi_mode;
#ifdef ROI
	u8 knuckle_roi_data[ROI_DATA_READ_LENGTH];
#endif
	struct firmware tp_fw;

	int actual_tp_mode;
	int tp_data_mode;
	int tp_data_format;
	int tp_data_len;

	int irq_num;
	int irq_gpio;
	int irq_tirgger_type;
	int tp_rst;
	int tp_int;
	int wait_int_timeout;

	int finger;
	int curt_touch[MAX_TOUCH_NUM];
	int prev_touch[MAX_TOUCH_NUM];
	int last_touch;

	int fw_retry;
	int fw_update_stat;
	int fw_open;
	u8  fw_info[75];
	u8  fw_mp_ver[4];
	u8  edge_palm_para[P5_X_EDGE_PALM_PARA_LENGTH];
	bool wq_ctrl;
	bool wq_esd_ctrl;
	bool wq_bat_ctrl;

	bool netlink;
	bool report;
	bool gesture;
	bool mp_retry;
	bool knuckle;
	int gesture_mode;
	int gesture_demo_ctrl;
	struct gesture_symbol ges_sym;
	struct report_info_block rib;

	u16 flash_mid;
	u16 flash_devid;
	int program_page;
	int flash_sector;

	/* Sending report data to users for the debug */
	bool dnp; //debug node open
	int dbf; //debug data frame
	int odi; //out data index
	wait_queue_head_t inq;
	struct debug_buf_list *dbl;
	int raw_count;
	int delta_count;
	int bg_count;

	int reset;
	int rst_edge_delay;
	int fw_upgrade_mode;
	int mp_ret_len;
	bool wtd_ctrl;
	bool do_otp_check;
	bool fw_uart_en;
	bool force_fw_update;
	bool irq_after_recovery;
	bool ddi_rest_done;
	bool tp_suspend;
	bool info_from_hex;
	bool prox_near;
	bool gesture_load_code;
	bool fix_ice;
	bool trans_xy;
	bool ss_ctrl;
	bool node_update;
	bool pll_clk_wakeup;

	/* module info */
	int tp_module;
	int md_fw_ili_size;
	char *md_name;
	char *md_fw_filp_path;
	char *md_fw_rq_path;
	char *md_ini_path;
	char *md_ini_rq_path;
	u8 *md_fw_ili;

	atomic_t irq_stat;
	atomic_t tp_reset;
	atomic_t ice_stat;
	atomic_t fw_stat;
	atomic_t mp_stat;
	atomic_t tp_sleep;
	atomic_t tp_sw_mode;
	atomic_t cmd_int_check;
	atomic_t esd_stat;

	/* Event for driver test */
	struct completion esd_done;

	int (*write)(void *data, int len);
	int (*read)(void *data, int len);
	int (*spi_write_then_read)(struct spi_device *spi, const void *txbuf,
				unsigned n_tx, void *rxbuf, unsigned n_rx);
	int (*spi_ack)(void);
	int (*mp_move_code)(void);
	int (*gesture_move_code)(int mode);
	int (*esd_recover)(void);
	void (*spi_speed)(bool enable);
	int (*ges_recover)(void);
	void (*demo_debug_info[5])(u8 *, size_t);
	int (*wrapper)(u8* wdata, u32 wlen, u8* rdata, u32 rlen, bool slave_int_check, bool int_check);
	int (*pll_clk_wakeup_func)(u8 cmd);
	int (*detect_int_stat)(void);
};
extern struct ilitek_tddi_dev *idev;

struct debug_buf_list {
	bool mark;
	unsigned char *data;
};

struct ilitek_touch_info {
	u16 id;
	u16 x;
	u16 y;
	u16 pressure;
};

struct gesture_coordinate {
	u16 code;
	u8 clockwise;
	int type;
	struct ilitek_touch_info pos_start;
	struct ilitek_touch_info pos_end;
	struct ilitek_touch_info pos_1st;
	struct ilitek_touch_info pos_2nd;
	struct ilitek_touch_info pos_3rd;
	struct ilitek_touch_info pos_4th;
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
	u8 cmd[6];
	int len;
	u8 def_cmd;
	u8 rec_state; //0:disable, 1: enable, 2: ignore record
	u8 rec_cmd;
};

struct ilitek_ic_info {
	u8 type;
	u8 ver;
	u16 id;
	u16 wtd_key;
	u32 pid;
	u32 pid_addr;
	u32 wdt_addr;
	u32 pc_counter_addr;
	u32 pc_latch_addr;
	u32 reset_addr;
	u32 otp_addr;
	u32 ana_addr;
	u32 otp_id;
	u32 ana_id;
	u32 fw_ver;
	u32 fw_mp_ver;
	u32 core_ver;
	u32 max_count;
	u32 reset_key;
	int no_bk_shift;
	bool spi_speed_ctrl;
	void (*hd_dma_check_crc_off)(void);
};

struct ilitek_hwif_info {
	u8 bus_type;
	u8 plat_type;
	const char *name;
	struct module *owner;
	const struct of_device_id *of_match_table;
	const struct dev_pm_ops *pm;
	int (*plat_probe)(void);
	int (*plat_remove)(void);
	void *info;
};

/* Prototypes for tddi firmware/flash functions */
extern void ilitek_tddi_ic_check_otp_prog_mode(bool mcu);
extern void ilitek_tddi_flash_dma_write(u32 start, u32 end, u32 len);
extern void ilitek_tddi_flash_clear_dma(void);
extern void ilitek_tddi_fw_read_flash_info(bool mcu);
extern int ilitek_tddi_fw_read_hw_crc(u32 start, u32 end, u32 *flash_crc);
extern int ilitek_tddi_fw_read_flash(u32 start, u32 end, u8 *data, int len);
extern int ilitek_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu);
extern int ilitek_tddi_fw_dump_flash_data(u32 start, u32 end, bool user, bool mcu);
extern int ilitek_tddi_fw_upgrade(int op);

/* Prototypes for tddi mp test */
extern int ilitek_tddi_mp_test_main(char *apk, bool lcm_on);

/* Prototypes for tddi core functions */
extern int ilitek_tddi_touch_esd_gesture_flash(void);
extern int ilitek_tddi_touch_esd_gesture_iram(void);
extern int ilitek_tddi_move_gesture_code_flash(int mode);
extern int ilitek_tddi_move_gesture_code_iram(int mode);
extern void ilitek_set_gesture_symbol(void);
extern int ilitek_tddi_move_mp_code_flash(void);
extern int ilitek_tddi_move_mp_code_iram(void);
extern void ilitek_tddi_touch_press(u16 x, u16 y, u16 pressure, u16 id);
extern void ilitek_tddi_touch_release(u16 x, u16 y, u16 id);
extern void ilitek_tddi_touch_release_all_point(void);
extern void ilitek_tddi_report_ap_mode(u8 *buf, int len);
extern void ilitek_tddi_report_debug_mode(u8 *buf, int rlen);
extern void ilitek_tddi_report_debug_lite_mode(u8 *buf, int len);
extern void ilitek_tddi_report_gesture_mode(u8 *buf, int rlen);
extern void ilitek_tddi_report_i2cuart_mode(u8 *buf, int rlen);
extern int ilitek_tddi_ic_watch_dog_ctrl(bool write, bool enable);
extern void ilitek_tddi_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data, bool mcu);
extern void ilitek_tddi_ic_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data, bool mcu);
extern void ilitek_tddi_ic_spi_speed_ctrl(bool enable);
extern int ilitek_tddi_ic_whole_reset(bool mcu);
extern int ilitek_tddi_ic_code_reset(bool mcu);
extern void ilitek_tddi_ic_edge_palm_para_init(void);
extern void ilitek_tddi_ic_send_edge_palm_para(void);
extern int ilitek_tddi_ic_func_ctrl(const char *name, int ctrl, u8 *rxbuf, u32 rlen);
extern void ilitek_tddi_ic_func_ctrl_reset(void);
extern void ilitek_tddi_ic_get_pc_counter(int stat);
extern int ilitek_tddi_ic_check_int_stat(void);
extern int ilitek_tddi_ic_check_busy(int count, int delay);
extern int ilitek_tddi_ic_get_project_id(u8 *pdata, int size);
extern int ilitek_tddi_ic_get_panel_info(void);
extern int ilitek_tddi_ic_get_tp_info(void);
extern int ilitek_tddi_ic_get_core_ver(void);
extern int ilitek_tddi_ic_get_protocl_ver(void);
extern int ilitek_tddi_ic_get_fw_ver(void);
extern int ilitek_tddi_ic_get_info(void);
extern int ilitek_tddi_ic_dummy_check(bool mcu);
extern void ilitek_tddi_ic_setting(void);
extern int ilitek_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool slave_int_check, bool int_check);
extern int ilitek_spi_slave_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool slave_int_check, bool int_check);
extern int ilitek_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value);
extern int ilitek_ice_mode_write(u32 addr, u32 data, int len);
extern int ilitek_ice_mode_read(u32 addr, u32 *data, int len);
extern int ilitek_ice_mode_ctrl(bool enable, bool mcu);
extern void ilitek_tddi_ic_init(void);
extern void ilitek_tddi_fw_uart_ctrl(u8 ctrl);
#ifdef ROI
extern int ilitek_read_knuckle_roi_data(void);
extern int ilitek_config_get_knuckle_roi_status(void);
extern int ilitek_config_knuckle_roi_ctrl(cmd_types cmd);
#endif
/* Prototypes for tddi events */
#if RESUME_BY_DDI
extern void ilitek_resume_by_ddi(void);
#endif
extern int ilitek_tddi_proximity_far(int mode);
extern int ilitek_tddi_proximity_near(int mode);
extern int ilitek_tddi_switch_tp_mode(u8 data);
extern int ilitek_set_tp_data_len(int format, bool send, u8* data);
extern int ilitek_tddi_fw_upgrade_handler(void *data);
extern int ilitek_tddi_wq_esd_i2c_check(void);
extern int ilitek_tddi_wq_esd_spi_check(void);
extern int ilitek_tddi_gesture_recovery(void);
extern void ilitek_tddi_spi_recovery(void);
extern void ilitek_tddi_wq_ctrl(int type, int ctrl);
extern int ilitek_tddi_mp_test_handler(char *apk, bool lcm_on);
extern int ilitek_tddi_report_handler(void);
extern int ilitek_tddi_sleep_handler(int mode);
extern int ilitek_tddi_reset_ctrl(int mode);
extern int ilitek_tddi_init(void);
extern int ilitek_tddi_dev_init(struct ilitek_hwif_info *hwif);
extern void ilitek_tddi_dev_remove(void);

/* Prototypes for i2c/spi interface */
extern int core_spi_setup(int num);
extern int ilitek_tddi_interface_dev_init(struct ilitek_hwif_info *hwif);
extern void ilitek_tddi_interface_dev_exit(struct ilitek_tddi_dev *idev);
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
extern void ilitek_plat_irq_unregister(void);
extern int ilitek_plat_irq_register(int type);

/* Prototypes for miscs */
extern void ilitek_tddi_node_init(void);
extern void ilitek_dump_data(void *data, int type, int len, int row_len, const char *name);
extern u8 ilitek_calc_packet_checksum(u8 *packet, int len);
extern void netlink_reply_msg(void *raw, int size);
extern int katoi(char *str);
extern int str2hex(char *str);

/* Prototypes for additional functionalities */
extern void gesture_fail_reason(bool enable);
extern int get_tp_recore_ctrl(int data);
extern int get_tp_recore_data(bool mcu);
extern void demo_debug_info_mode(u8 *buf, size_t rlen);
extern void demo_debug_info_id0(u8 *buf, size_t len);

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

static inline int ipio_strcmp(const char *s1, const char *s2)
{
	return (strlen(s1) != strlen(s2)) ? -1 : strncmp(s1, s2, strlen(s1));
}

static inline void *ipio_memcpy(void *dest, const void *src, int n, int dest_size)
{
	if (n > dest_size)
		 n = dest_size;

	return memcpy(dest, src, n);
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
