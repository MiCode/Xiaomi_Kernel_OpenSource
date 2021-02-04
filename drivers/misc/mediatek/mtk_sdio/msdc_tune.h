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

#ifndef _MSDC_TUNE_H_
#define _MSDC_TUNE_H_

#define MSDC_CLKTXDLY                   0
#define MSDC_DDRCKD                     0

#define MSDC0_HS400_CLKTXDLY            0
#define MSDC0_HS400_CMDTXDLY            0xA
#define MSDC0_HS400_DAT0TXDLY           0
#define MSDC0_HS400_DAT1TXDLY           0
#define MSDC0_HS400_DAT2TXDLY           0
#define MSDC0_HS400_DAT3TXDLY           0
#define MSDC0_HS400_DAT4TXDLY           0
#define MSDC0_HS400_DAT5TXDLY           0
#define MSDC0_HS400_DAT6TXDLY           0
#define MSDC0_HS400_DAT7TXDLY           0
#define MSDC0_HS400_TXSKEW              1

#define MSDC0_DDR50_DDRCKD              1

#define MSDC0_CLKTXDLY                  0
#define MSDC0_CMDTXDLY                  0
#define MSDC0_DAT0TXDLY                 0
#define MSDC0_DAT1TXDLY                 0
#define MSDC0_DAT2TXDLY                 0
#define MSDC0_DAT3TXDLY                 0
#define MSDC0_DAT4TXDLY                 0
#define MSDC0_DAT5TXDLY                 0
#define MSDC0_DAT6TXDLY                 0
#define MSDC0_DAT7TXDLY                 0
#define MSDC0_TXSKEW                    0

#define MSDC1_CLK_TX_VALUE              0
#define MSDC1_CLK_SDR104_TX_VALUE       3
#define MSDC2_CLK_TX_VALUE              0

/* Declared in msdc_tune.c */
/* FIX ME: move it to another file */
extern int g_ett_tune;
extern int g_reset_tune;

/* FIX ME: check if it can be removed since it is set but referenced */
extern u32 sdio_tune_flag;

void msdc_init_tune_setting(struct msdc_host *host);
void msdc_ios_tune_setting(struct mmc_host *mmc, struct mmc_ios *ios);
void msdc_init_tune_path(struct msdc_host *host, unsigned char timing);

void msdc_reset_pwr_cycle_counter(struct msdc_host *host);
void msdc_reset_tmo_tune_counter(struct msdc_host *host, unsigned int index);
void msdc_reset_crc_tune_counter(struct msdc_host *host, unsigned int index);
u32 msdc_power_tuning(struct msdc_host *host);
int msdc_tuning_wo_autok(struct msdc_host *host);
int msdc_tune_cmdrsp(struct msdc_host *host);
int emmc_hs400_tune_rw(struct msdc_host *host);
int msdc_tune_read(struct msdc_host *host);
int msdc_tune_write(struct msdc_host *host);
int msdc_crc_tune(struct msdc_host *host, struct mmc_command *cmd,
	struct mmc_data *data, struct mmc_command *stop,
	struct mmc_command *sbc);
int msdc_cmd_timeout_tune(struct msdc_host *host, struct mmc_command *cmd);
int msdc_data_timeout_tune(struct msdc_host *host, struct mmc_data *data);

void emmc_clear_timing(void);
void msdc_save_timing_setting(struct msdc_host *host, int save_mode);
void msdc_restore_timing_setting(struct msdc_host *host);

void msdc_set_bad_card_and_remove(struct msdc_host *host);

/* For KS only */
/*int msdc_setting_parameter(struct msdc_hw *hw, unsigned int *para);*/

#endif /* end of_MSDC_TUNE_H_ */
