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

#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/delay.h>
#include <linux/fs.h>

#include "autok_dvfs.h"

#define SDIO_AUTOK_HIGH_RES_PATH    "/data/sdio_autok_high"  /* E1 only use high */
#define SDIO_AUTOK_LOW_RES_PATH     "/data/sdio_autok_low"   /* E2 use low and high with DFFS */

#define SDIO_AUTOK_DIFF_MARGIN      3

u8 sdio_autok_res[2][TUNING_PARAM_COUNT];
u8 emmc_autok_res[2][TUNING_PARAM_COUNT];
u8 sd_autok_res[2][TUNING_PARAM_COUNT];
int sdio_ver;

static struct file *msdc_file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);

	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}

	return filp;
}

static int msdc_file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);

	return ret;
}

static int msdc_file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);

	return ret;
}

int sdio_autok_res_apply(struct msdc_host *host, int vcore)
{
	struct file *filp = NULL;
	size_t size;
	u8 *res;
	int ret = -1;
	int i;

	if (vcore <= AUTOK_VCORE_LOW) {
		res = sdio_autok_res[AUTOK_VCORE_LOW];
		filp = msdc_file_open(SDIO_AUTOK_LOW_RES_PATH, O_RDONLY, 0644);
	} else {
		res = sdio_autok_res[AUTOK_VCORE_HIGH];
		filp = msdc_file_open(SDIO_AUTOK_HIGH_RES_PATH, O_RDONLY, 0644);
	}

	if (filp == NULL) {
		pr_err("autok result open fail\n");
		return ret;
	}

	size = msdc_file_read(filp, 0, res, TUNING_PARAM_COUNT);
	if (size == TUNING_PARAM_COUNT) {
		autok_tuning_parameter_init(host, res);

		for (i = 1; i < TUNING_PARAM_COUNT; i++)
			pr_err("autok result exist!, result[%d] = %d\n", i, res[i]);
		ret = 0;
	}

	filp_close(filp, NULL);

	return ret;
}

int sdio_autok_res_save(struct msdc_host *host, int vcore, u8 *res)
{
	struct file *filp = NULL;
	size_t size;
	int ret = -1;

	if (res == NULL)
		return ret;

	if (vcore <= AUTOK_VCORE_LOW) {
		memcpy((void *)sdio_autok_res[AUTOK_VCORE_LOW], (const void *)res, TUNING_PARAM_COUNT);
		filp = msdc_file_open(SDIO_AUTOK_LOW_RES_PATH, O_CREAT | O_WRONLY, 0644);
	} else {
		memcpy((void *)sdio_autok_res[AUTOK_VCORE_HIGH], (const void *)res, TUNING_PARAM_COUNT);
		filp = msdc_file_open(SDIO_AUTOK_HIGH_RES_PATH, O_CREAT | O_WRONLY, 0644);
	}

	if (filp == NULL) {
		pr_err("autok result open fail\n");
		return ret;
	}

	size = msdc_file_write(filp, 0, res, TUNING_PARAM_COUNT);
	if (size == TUNING_PARAM_COUNT)
		ret = 0;
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);

	return ret;
}

int autok_res_check(u8 *res_h, u8 *res_l)
{
	int ret = 0;
	int i;

	for (i = 0; i < TUNING_PARAM_COUNT; i++) {
		if ((i == CMD_RD_D_DLY1) || (i == DAT_RD_D_DLY1)) {
			if ((res_h[i] > res_l[i]) && (res_h[i] - res_l[i] > SDIO_AUTOK_DIFF_MARGIN))
				ret = -1;
			if ((res_l[i] > res_h[i]) && (res_l[i] - res_h[i] > SDIO_AUTOK_DIFF_MARGIN))
				ret = -1;
		} else if ((i == CMD_RD_D_DLY1_SEL) || (i == DAT_RD_D_DLY1_SEL)) {
			/* this is cover by previous check,
			 * just by pass if 0 and 1 in cmd/dat delay
			 */
		} else {
			if (res_h[i] != res_l[i])
				ret = -1;
		}
	}
	pr_err("autok_res_check %d!\n", ret);

	return ret;
}

int sdio_version(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 div = 0;

	if ((host->id == 2) && (sdio_ver == 0)) {
		/* enable dvfs feature */
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, 1);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_DVFS_EN, div);

		if (div == 0)
			sdio_ver = 1;
		else
			sdio_ver = 2;

		pr_err("SDIO E%d!\n", sdio_ver);
	}

	return sdio_ver;
}

void sdio_unreq_vcore(struct work_struct *work)
{
#if 0
	if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_UNREQ) == 0)
		pr_debug("unrequest vcore pass\n");
	else
		pr_err("unrequest vcore fail\n");
#endif
}


void sdio_set_vcore_performance(struct msdc_host *host, u32 enable)
{
#if 0
	if (enable) {
		if (cancel_delayed_work_sync(&(host->set_vcore_workq)) == 0) {
			/* true if dwork was pending, false otherwise */
			pr_debug("** cancel @ FALSE\n");
			if (vcorefs_request_dvfs_opp(KIR_SDIO, OPPI_PERF) == 0) {
				pr_debug("msdc%d -> request vcore pass\n",
					host->id);
			} else {
				pr_err("msdc%d -> request vcore fail\n",
					host->id);
			}
		} else {
			pr_debug("** cancel @ TRUE\n");
		}
	} else {
		schedule_delayed_work(&(host->set_vcore_workq), SDIO_DVFS_TIMEOUT);
	}
#endif
}

void sdio_set_vcorefs_sram(int vcore, int done, struct msdc_host *host)
{
#if 0
	void __iomem *base = host->base;

	if (vcore >= AUTOK_VCORE_HIGH) {
		vcorefs_set_sram_data(1, MSDC_READ32(MSDC_IOCON));
		vcorefs_set_sram_data(2, MSDC_READ32(MSDC_PATCH_BIT0));
		vcorefs_set_sram_data(3, MSDC_READ32(MSDC_PATCH_BIT1));
		vcorefs_set_sram_data(4, MSDC_READ32(MSDC_PATCH_BIT2));
		vcorefs_set_sram_data(5, MSDC_READ32(MSDC_PAD_TUNE0));
		vcorefs_set_sram_data(6, MSDC_READ32(MSDC_PAD_TUNE1));
		vcorefs_set_sram_data(7, MSDC_READ32(MSDC_DAT_RDDLY0));
		vcorefs_set_sram_data(8, MSDC_READ32(MSDC_DAT_RDDLY1));
		vcorefs_set_sram_data(9, MSDC_READ32(MSDC_DAT_RDDLY2));
		vcorefs_set_sram_data(10, MSDC_READ32(MSDC_DAT_RDDLY3));
	} else {
		vcorefs_set_sram_data(11, MSDC_READ32(MSDC_IOCON));
		vcorefs_set_sram_data(12, MSDC_READ32(MSDC_PATCH_BIT0));
		vcorefs_set_sram_data(13, MSDC_READ32(MSDC_PATCH_BIT1));
		vcorefs_set_sram_data(14, MSDC_READ32(MSDC_PATCH_BIT2));
		vcorefs_set_sram_data(15, MSDC_READ32(MSDC_PAD_TUNE0));
		vcorefs_set_sram_data(16, MSDC_READ32(MSDC_PAD_TUNE1));
		vcorefs_set_sram_data(17, MSDC_READ32(MSDC_DAT_RDDLY0));
		vcorefs_set_sram_data(18, MSDC_READ32(MSDC_DAT_RDDLY1));
		vcorefs_set_sram_data(19, MSDC_READ32(MSDC_DAT_RDDLY2));
		vcorefs_set_sram_data(20, MSDC_READ32(MSDC_DAT_RDDLY3));
	}

	/* SPM see 0x0x55AA55AA then SDIO para apply for HPM/LPM transisiton */
	if (done)
		vcorefs_set_sram_data(0, 0x55AA55AA);
#endif
}

/* For backward compatible, remove later */
int wait_sdio_autok_ready(void *data)
{
	return 0;
}
EXPORT_SYMBOL(wait_sdio_autok_ready);


void sdio_autok_wait_dvfs_ready(void)
{
#if 0
	int dvfs;

	dvfs = is_vcorefs_can_work();

	/* DVFS not ready, just wait */
	while (dvfs == 0) {
		pr_err("DVFS not ready\n");
		msleep(100);
		dvfs = is_vcorefs_can_work();
	}

	if (dvfs == -1)
		pr_err("DVFS feature not enable\n");

	if (dvfs == 1)
		pr_err("DVFS ready\n");
#endif
}

int emmc_autok(void)
{
	struct msdc_host *host = mtk_msdc_host[0];
	struct mmc_host *mmc = host->mmc;

	if (mmc == NULL) {
		pr_err("eMMC device not ready\n");
		return -1;
	}

	pr_err("emmc autok\n");

#if 0 /* Wait Light confirm */
	mmc_claim_host(mmc);

	/* Performance mode, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_EMMC, OPPI_PERF) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		pr_err("[AUTOK]eMMC HS200 Tune\r\n");
		hs200_execute_tuning(host, emmc_autok_res[AUTOK_VCORE_HIGH]);
	} else if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		pr_err("[AUTOK]eMMC HS400 Tune\r\n");
		hs400_execute_tuning(host, emmc_autok_res[AUTOK_VCORE_HIGH]);
	}

	/* Low power mode, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_EMMC, OPPI_LOW_PWR) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");

	if (mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		pr_err("[AUTOK]eMMC HS200 Tune\r\n");
		hs200_execute_tuning(host, emmc_autok_res[AUTOK_VCORE_LOW]);
	} else if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		pr_err("[AUTOK]eMMC HS400 Tune\r\n");
		hs400_execute_tuning(host, emmc_autok_res[AUTOK_VCORE_LOW]);
	}

	/* Un-request, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_EMMC, OPPI_UNREQ) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_UNREQ fail!\n");

	mmc_release_host(mmc);
#endif

	return 0;
}
EXPORT_SYMBOL(emmc_autok);

int sd_autok(void)
{
	struct msdc_host *host = mtk_msdc_host[1];
	struct mmc_host *mmc = host->mmc;

	if (mmc == NULL) {
		pr_err("SD card not ready\n");
		return -1;
	}

	pr_err("sd autok\n");

#if 0 /* Wait Cool confirm */
	mmc_claim_host(mmc);

	/* Performance mode, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_SD, OPPI_PERF) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
	autok_execute_tuning(host, sd_autok_res[AUTOK_VCORE_HIGH]);

	/* Low power mode, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_SD, OPPI_LOW_PWR) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_PERF fail!\n");
	autok_execute_tuning(host, sd_autok_res[AUTOK_VCORE_LOW]);

	/* Un-request, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_SD, OPPI_UNREQ) != 0)
		pr_err("vcorefs_request_dvfs_opp@OPPI_UNREQ fail!\n");

	mmc_release_host(mmc);
#endif

	return 0;
}
EXPORT_SYMBOL(sd_autok);

int sdio_autok(void)
{
	struct msdc_host *host = mtk_msdc_host[2];

	if ((host == NULL) || (host->hw == NULL))
		return -1;

	if (host->hw->host_function != MSDC_SDIO)
		return -1;

	pr_err("sdio autok\n");

	/* DVFS need wait device ready and excute autok here */
	if (!wait_for_completion_timeout(&host->autok_done, 10 * HZ)) {
		pr_err("SDIO wait device autok ready timeout");
		return -1;
	}

	pr_err("sdio autok done!");

	return 0;
}
EXPORT_SYMBOL(sdio_autok);



