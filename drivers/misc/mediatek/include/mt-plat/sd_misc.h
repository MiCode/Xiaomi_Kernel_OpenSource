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

#ifndef SD_MISC_H
#define SD_MISC_H

#ifdef __KERNEL__
#include <linux/bitops.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#endif

#include <mt_sd.h>

#ifndef FPGA_PLATFORM
extern void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18);
#endif
/* void msdc_dump_padctl(struct msdc_host *host); */
extern u32 msdc_host_mode[HOST_MAX_NUM];
extern u32 msdc_host_mode2[HOST_MAX_NUM];

extern int mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value,
	unsigned int timeout_ms);
extern int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd);
extern struct msdc_host *mtk_msdc_host[];
extern struct msdc_host *msdc_get_host(int host_function, bool boot, bool secondary);
extern int msdc_reinit(struct msdc_host *host);
#if 0
extern int msdc_get_reserve(void);
extern u32 msdc_get_capacity(int get_emmc_total);
extern struct gendisk *mmc_get_disk(struct mmc_card *card);
extern void msdc_get_driving(struct msdc_host *host, struct msdc_ioctl *msdc_ctl);
#endif

#ifdef CONFIG_MTK_EMMC_SUPPORT
/* extern struct excel_info PartInfoEmmc[PART_NUM]; */

extern u32 g_emmc_mode_switch;
#endif

struct msdc_ioctl {
	int opcode;
	int host_num;
	int iswrite;
	int trans_type;
	unsigned int total_size;
	unsigned int address;
	unsigned int *buffer;
	int cmd_pu_driving;
	int cmd_pd_driving;
	int dat_pu_driving;
	int dat_pd_driving;
	int clk_pu_driving;
	int clk_pd_driving;
	int ds_pu_driving;
	int ds_pd_driving;
	int rst_pu_driving;
	int rst_pd_driving;
	int clock_freq;
	int partition;
	int hopping_bit;
	int hopping_time;
	int result;
	int sd30_mode;
	int sd30_max_current;
	int sd30_drive;
	int sd30_power_control;
};

extern  int simple_sd_ioctl_rw(struct msdc_ioctl *msdc_ctl);
/**************for msdc_ssc***********************/
#define AUDPLL_CTL_REG12                (0xF0007070)
#define AUDPLL_CTL_REG01                (0xF00071E0)
#define AUDPLL_CTL_REG02                (0xF100000C)

#define AUDPLL_TSEL_MASK                (1792) /* MASK = 00000111 00000000 */
#define AUDPLL_TSEL_RESULT1             (0)    /* REG = 00000000 00000000
						  30.5us */
#define AUDPLL_TSEL_RESULT2             (256)  /* REG = 00000001 00000000
						  61.0us */
#define AUDPLL_TSEL_RESULT3             (512)  /* REG = 00000010 00000000
						  122.1us */
#define AUDPLL_TSEL_RESULT4             (768)  /* REG = 00000011 00000000
						  244.1us */
#define AUDPLL_TSEL_RESULT5             (1024) /* REG = 00000100 00000000
						  448.3us */

#define AUDPLL_BSEL_MASK                (7)  /* MASK = 00000000 00000111 */
#define AUDPLL_BSEL_RESULT0             (0)  /* REG = 00000000 00000000
						REG init val */
#define AUDPLL_BSEL_RESULT1             (1)  /* REG = 00000000 00000001
						2.26MHz */
#define AUDPLL_BSEL_RESULT2             (2)  /* REG = 00000000 00000010
						4.52MHz */
#define AUDPLL_BSEL_RESULT3             (4)  /* REG = 00000000 00000100
						9.04MHz */

#define SET_HOP_BIT_NONE                (0)
#define SET_HOP_BIT1                    (1)
#define SET_HOP_BIT2                    (2)
#define SET_HOP_BIT3                    (3)

#define SET_HOP_TIME0                   (0)
#define SET_HOP_TIME1                   (1)
#define SET_HOP_TIME2                   (2)
#define SET_HOP_TIME3                   (3)
#define SET_HOP_TIME4                   (4)


/**************for msdc_ssc***********************/

#define MSDC_DRIVING_SETTING            (0)
#define MSDC_CLOCK_FREQUENCY            (1)
#define MSDC_SINGLE_READ_WRITE          (2)
#define MSDC_MULTIPLE_READ_WRITE        (3)
#define MSDC_GET_CID                    (4)
#define MSDC_GET_CSD                    (5)
#define MSDC_GET_EXCSD                  (6)
#define MSDC_ERASE_PARTITION            (7)
#define MSDC_HOPPING_SETTING            (8)

#define MSDC_REINIT_SDCARD              _IOW('r', 9, int)

#define MSDC_SD30_MODE_SWITCH           (10)
#define MSDC_GET_BOOTPART               (11)
#define MSDC_SET_BOOTPART               (12)
#define MSDC_GET_PARTSIZE               (13)

#define MSDC_CD_PIN_EN_SDCARD           _IOW('r', 14, int)
#define MSDC_SD_POWER_OFF               (15)
#define MSDC_SD_POWER_ON                (16)

#define MSDC_ERASE_SELECTED_AREA        (0x20)
#define MSDC_CARD_DUNM_FUNC             (0xff)

typedef enum {
	USER_PARTITION = 0,
	BOOT_PARTITION_1,
	BOOT_PARTITION_2,
	RPMB_PARTITION,
	GP_PARTITION_1,
	GP_PARTITION_2,
	GP_PARTITION_3,
	GP_PARTITION_4,
} PARTITON_ACCESS_T;

typedef enum {
	SDHC_HIGHSPEED = 0, /* 0 Host supports HS mode */
	UHS_SDR12,          /* 1 Host supports UHS SDR12 mode */
	UHS_SDR25,          /* 2 Host supports UHS SDR25 mode */
	UHS_SDR50,          /* 3 Host supports UHS SDR50 mode */
	UHS_SDR104,         /* 4 Host supports UHS SDR104 mode */
	UHS_DDR50,          /* 5 Host supports UHS DDR50 mode */
	EMMC_HS400,         /* 6 Host supports EMMC HS400 mode */
	CAPS_SPEED_NULL,
} SD3_MODE;

typedef enum {
	DRIVER_TYPE_A = 0,  /* 0 Host supports Driver Type A */
	DRIVER_TYPE_B,      /* 1 Host supports Driver Type B */
	DRIVER_TYPE_C,      /* 2 Host supports Driver Type C */
	DRIVER_TYPE_D,      /* 3 Host supports Driver Type D */
	CAPS_DRIVE_NULL
} SD3_DRIVE;

typedef enum {
	MAX_CURRENT_200 = 0,/* 0 Host max current limit is 200mA */
	MAX_CURRENT_400,    /* 1 Host max current limit is 400mA */
	MAX_CURRENT_600,    /* 2 Host max current limit is 600mA */
	MAX_CURRENT_800,    /* 3 Host max current limit is 800mA */
} SD3_MAX_CURRENT;

typedef enum {
	SDXC_NO_POWER_CONTROL = 0, /* 0  Host not supports >150mA current
				      at 3.3V /3.0V/1.8V */
	SDXC_POWER_CONTROL, /* 1 Host supports >150mA current
			       at 3.3V /3.0V/1.8V */
} SD3_POWER_CONTROL;

typedef enum {
	DUMP_INTO_BOOT_CARD_IPANIC = 0,
	DUMP_INTO_BOOT_CARD_KDUMP = 1,
	DUMP_INTO_EXTERN_CARD = 2,
} DUMP_STORAGE_TYPE;

typedef enum {
	EMMC_CARD_BOOT = 0,
	SD_CARD_BOOT,
	EMMC_CARD,
	SD_CARD,
} STORAGE_TPYE;

#define EXT_CSD_BOOT_SIZE_MULT          (226) /* R */
#define EXT_CSD_HC_WP_GPR_SIZE          (221) /* RO */
#define EXT_CSD_RPMB_SIZE_MULT          (168) /* R */
#define EXT_CSD_GP1_SIZE_MULT           (143) /* R/W 3 bytes */
#define EXT_CSD_GP2_SIZE_MULT           (146) /* R/W 3 bytes */
#define EXT_CSD_GP3_SIZE_MULT           (149) /* R/W 3 bytes */
#define EXT_CSD_GP4_SIZE_MULT           (152) /* R/W 3 bytes */
#define EXT_CSD_PART_CFG                (179) /* R/W/E & R/W/E_P */
#define EXT_CSD_CACHE_FLUSH             (32)
#define CAPACITY_2G                     (2 * 1024 * 1024 * 1024ULL)

typedef enum {
	EMMC_BOOT_NO_EN = 0,
	EMMC_BOOT1_EN,
	EMMC_BOOT2_EN,
	EMMC_BOOT_USER = 7,
	EMMC_BOOT_END
} BOOT_PARTITION_EN;

#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
typedef enum {
	EMMC_PART_UNKNOWN = 0,
	EMMC_PART_BOOT1,
	EMMC_PART_BOOT2,
	EMMC_PART_RPMB,
	EMMC_PART_GP1,
	EMMC_PART_GP2,
	EMMC_PART_GP3,
	EMMC_PART_GP4,
	EMMC_PART_USER,
	EMMC_PART_END,
} Region;
#endif

typedef enum {
	CARD_INFO = 0,
	DISK_INFO,
	EMMC_USER_CAPACITY,
	EMMC_CAPACITY,
	EMMC_RESERVE,
} GET_STORAGE_INFO;

struct storage_info {
	struct mmc_card *card;
	struct gendisk *disk;
	unsigned long long emmc_user_capacity;
	unsigned long long emmc_capacity;
	int emmc_reserve;
};

int msdc_get_info(STORAGE_TPYE storage_type, GET_STORAGE_INFO info_type,
	struct storage_info *info);

#endif              /* end of SD_MISC_H */
