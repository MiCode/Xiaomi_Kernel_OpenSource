/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/delay.h>
#include <linux/fs.h>

#include "autok_dvfs.h"
#include "mtk_sd.h"
#include "dbg.h"
#include <mmc/core/sdio_ops.h>
#include <mmc/core/core.h>

static char const * const sdio_autok_res_path[] = {
	"/data/sdio_autok_0", "/data/sdio_autok_1",
	"/data/sdio_autok_2", "/data/sdio_autok_3",
};

/* After merge still have over 10 OOOOOOOOOOO window */
#define AUTOK_MERGE_MIN_WIN			10

static struct file *msdc_file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
#ifdef SDIO_HQA
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
#endif

	return filp;
}

static int msdc_file_read(struct file *file, unsigned long long offset,
	unsigned char *data, unsigned int size)
{
	int ret = 0;
#ifdef SDIO_HQA
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);
#endif

	return ret;
}

static int msdc_file_write(struct file *file, unsigned long long offset,
	unsigned char *data, unsigned int size)
{
	int ret = 0;
#ifdef SDIO_HQA
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
#endif

	return ret;
}

int sdio_autok_res_exist(struct msdc_host *host)
{
#ifdef SDIO_HQA
	struct file *filp = NULL;
	int i;

	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		filp = msdc_file_open(sdio_autok_res_path[i], O_RDONLY, 0644);
		if (filp == NULL) {
			pr_notice("autok result not exist\n");
			return 0;
		}
		filp_close(filp, NULL);
	}

	return 1;
#else
	return 0;
#endif
}

int sdio_autok_res_apply(struct msdc_host *host, int vcore)
{
	struct file *filp = NULL;
	size_t size;
	u8 *res;
	int ret = -1;
	int i;

	if (vcore < AUTOK_VCORE_LEVEL0 ||  vcore >= AUTOK_VCORE_NUM)
		vcore = AUTOK_VCORE_LEVEL0;

	res = host->autok_res[vcore];

	filp = msdc_file_open(sdio_autok_res_path[vcore], O_RDONLY, 0644);
	if (filp == NULL) {
		pr_notice("autok result open fail\n");
		return ret;
	}

	size = msdc_file_read(filp, 0, res, TUNING_PARA_SCAN_COUNT);
	if (size == TUNING_PARA_SCAN_COUNT) {
		autok_tuning_parameter_init(host, res);

		for (i = 1; i < TUNING_PARA_SCAN_COUNT; i++)
			pr_info("autok result exist!, result[%d] = %d\n",
				i, res[i]);
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

	if (vcore < AUTOK_VCORE_LEVEL0 ||  vcore >= AUTOK_VCORE_NUM)
		vcore = AUTOK_VCORE_LEVEL0;

	filp = msdc_file_open(sdio_autok_res_path[vcore], O_CREAT | O_WRONLY,
		0644);
	if (filp == NULL) {
		pr_notice("autok result open fail\n");
		return ret;
	}

	size = msdc_file_write(filp, 0, res, TUNING_PARA_SCAN_COUNT);
	if (size == TUNING_PARA_SCAN_COUNT)
		ret = 0;
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);

	return ret;
}

static u32 sdio_reg_backup[AUTOK_VCORE_NUM * BACKUP_REG_COUNT_SDIO];
static u32 emmc_reg_backup[AUTOK_VCORE_NUM * BACKUP_REG_COUNT_EMMC];
/* static u32 sd_reg_backup[AUTOK_VCORE_NUM][BACKUP_REG_COUNT]; */

u16 sdio_reg_backup_offsets_src[] = {
	OFFSET_MSDC_IOCON,
	OFFSET_MSDC_PATCH_BIT0,
	OFFSET_MSDC_PATCH_BIT1,
	OFFSET_MSDC_PATCH_BIT2,
	OFFSET_MSDC_PAD_TUNE0,
	OFFSET_MSDC_PAD_TUNE1,
	OFFSET_EMMC50_PAD_DS_TUNE,
	OFFSET_EMMC50_PAD_CMD_TUNE,
	OFFSET_EMMC50_PAD_DAT01_TUNE,
	OFFSET_EMMC50_PAD_DAT23_TUNE,
	OFFSET_EMMC50_PAD_DAT45_TUNE,
	OFFSET_EMMC50_PAD_DAT67_TUNE,
	OFFSET_EMMC50_CFG0,
	OFFSET_EMMC50_CFG1
};

u16 sdio_dvfs_reg_backup_offsets[] = {
	OFFSET_MSDC_IOCON_1,
	OFFSET_MSDC_PATCH_BIT0_1,
	OFFSET_MSDC_PATCH_BIT1_1,
	OFFSET_MSDC_PATCH_BIT2_1,
	OFFSET_MSDC_PAD_TUNE0_1,
	OFFSET_MSDC_PAD_TUNE1_1,
	OFFSET_EMMC50_PAD_DS_TUNE_1,
	OFFSET_EMMC50_PAD_CMD_TUNE_1,
	OFFSET_EMMC50_PAD_DAT01_TUNE_1,
	OFFSET_EMMC50_PAD_DAT23_TUNE_1,
	OFFSET_EMMC50_PAD_DAT45_TUNE_1,
	OFFSET_EMMC50_PAD_DAT67_TUNE_1,
	OFFSET_EMMC50_CFG0_1,
	OFFSET_EMMC50_CFG1_1
};

u16 emmc_reg_backup_offsets_src[] = {
	OFFSET_MSDC_IOCON,
	OFFSET_MSDC_PATCH_BIT0,
	OFFSET_MSDC_PATCH_BIT1,
	OFFSET_MSDC_PATCH_BIT2,
	OFFSET_EMMC50_CFG0,
};

u16 emmc_dvfs_reg_backup_offsets[] = {
	OFFSET_MSDC_IOCON_1,
	OFFSET_MSDC_PATCH_BIT0_1,
	OFFSET_MSDC_PATCH_BIT1_1,
	OFFSET_MSDC_PATCH_BIT2_1,
	OFFSET_EMMC50_CFG0_1
};

u16 emmc_dvfs_reg_backup_offsets_top[] = {
	OFFSET_EMMC_TOP_CONTROL + MSDC_TOP_SET_SIZE,
	OFFSET_EMMC_TOP_CMD + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_CTL0 + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DS_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT0_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT1_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT2_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT3_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT4_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT5_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT6_TUNE + MSDC_TOP_SET_SIZE,
	OFFSET_TOP_EMMC50_PAD_DAT7_TUNE + MSDC_TOP_SET_SIZE
};

void msdc_dvfs_reg_restore(struct msdc_host *host)
{
#if defined(VCOREFS_READY)
	void __iomem *base = host->base;
	int i, j;
	u32 *reg_backup_ptr;

	if (!host->dvfs_reg_backup)
		return;

	reg_backup_ptr = host->dvfs_reg_backup;
	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		for (j = 0; j < host->dvfs_reg_backup_cnt; j++) {
			MSDC_WRITE32(
				host->base + MSDC_DVFS_SET_SIZE * i
				+ host->dvfs_reg_offsets[j],
				*reg_backup_ptr);
			reg_backup_ptr++;
		}
		if (!host->base_top)
			continue;
		for (j = 0; j < host->dvfs_reg_backup_cnt_top; j++) {
			MSDC_WRITE32(
				host->base_top + MSDC_TOP_SET_SIZE * i
				+ host->dvfs_reg_offsets_top[j],
				*reg_backup_ptr);
			reg_backup_ptr++;
		}
	}

	/* Enable HW DVFS */
	MSDC_WRITE32(MSDC_CFG,
		MSDC_READ32(MSDC_CFG) | (MSDC_CFG_DVFS_EN | MSDC_CFG_DVFS_HW));
#endif
}

static void msdc_dvfs_reg_backup(struct msdc_host *host)
{
	int i, j;
	u32 *reg_backup_ptr;

	if (!host->dvfs_reg_backup)
		return;

	reg_backup_ptr = host->dvfs_reg_backup;
	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		for (j = 0; j < host->dvfs_reg_backup_cnt; j++) {
			*reg_backup_ptr = MSDC_READ32(
				host->base + MSDC_DVFS_SET_SIZE * i
				+ host->dvfs_reg_offsets[j]);
			reg_backup_ptr++;
		}
		if (!host->base_top)
			continue;
		for (j = 0; j < host->dvfs_reg_backup_cnt_top; j++) {
			*reg_backup_ptr = MSDC_READ32(
				host->base_top + MSDC_TOP_SET_SIZE * i
				+ host->dvfs_reg_offsets_top[j]);
			reg_backup_ptr++;
		}
	}
}

static void msdc_set_hw_dvfs(int vcore, struct msdc_host *host)
{
	void __iomem *addr, *addr_src;
	int i;

	vcore = AUTOK_VCORE_NUM - 1 - vcore;

	addr = host->base + MSDC_DVFS_SET_SIZE * vcore;
	addr_src = host->base;
	for (i = 0; i < host->dvfs_reg_backup_cnt; i++) {
		MSDC_WRITE32(addr + host->dvfs_reg_offsets[i],
			MSDC_READ32(addr_src + host->dvfs_reg_offsets_src[i]));
	}

	if (!host->base_top)
		return;
	addr = host->base_top + MSDC_TOP_SET_SIZE * vcore;
	addr_src = host->base_top - MSDC_TOP_SET_SIZE;
	for (i = 0; i < host->dvfs_reg_backup_cnt_top; i++) {
		MSDC_WRITE32(addr + host->dvfs_reg_offsets_top[i],
			MSDC_READ32(addr_src + host->dvfs_reg_offsets_top[i]));
	}
}

void sdio_autok_wait_dvfs_ready(void)
{
	int dvfs;

	dvfs = is_vcorefs_can_work();

	/* DVFS not ready, just wait */
	while (dvfs == 0) {
		pr_info("DVFS not ready\n");
		msleep(100);
		dvfs = is_vcorefs_can_work();
	}

	if (dvfs == -1)
		pr_info("DVFS feature not enable\n");

	if (dvfs == 1)
		pr_info("DVFS ready\n");
}

int sd_execute_dvfs_autok(struct msdc_host *host, u32 opcode)
{
	int ret = 0;
	u8 *res;

	/* For SD, HW_DVFS is always not feasible because:
	 * 1. SD insertion can be performed at anytime
	 * 2. Lock vcore at low vcore is not allowed after booting
	 * Therefore, SD autok is performed on one vcore, current vcore.
	 * We always store autok result at host->autok_res[0].
	 */
	res = host->autok_res[0];
	if (host->mmc->ios.timing == MMC_TIMING_UHS_SDR104 ||
	    host->mmc->ios.timing == MMC_TIMING_UHS_SDR50) {
		if (host->is_autok_done == 0) {
			pr_info("[AUTOK]SDcard autok\n");
			ret = autok_execute_tuning(host, res);
			host->is_autok_done = 1;
		} else {
			autok_init_sdr104(host);
			autok_tuning_parameter_init(host, res);
		}
	}

	/* Enable this line if SD use HW DVFS */
	/* msdc_set_hw_dvfs(vcore, host); */
	/* msdc_dump_register_core(host, */

	return ret;
}

void msdc_dvfs_reg_backup_init(struct msdc_host *host)
{
	if (host->hw->host_function == MSDC_EMMC && host->use_hw_dvfs) {
		host->dvfs_reg_backup = emmc_reg_backup;
		host->dvfs_reg_offsets = emmc_dvfs_reg_backup_offsets;
		host->dvfs_reg_offsets_src = emmc_reg_backup_offsets_src;
		if (host->base_top) {
			host->dvfs_reg_offsets_top =
				emmc_dvfs_reg_backup_offsets_top;
			host->dvfs_reg_backup_cnt_top =
				BACKUP_REG_COUNT_EMMC_TOP;
		}
		host->dvfs_reg_backup_cnt = BACKUP_REG_COUNT_EMMC_INTERNAL;
	} else if (host->hw->host_function == MSDC_SDIO && host->use_hw_dvfs) {
		host->dvfs_reg_backup = sdio_reg_backup;
		host->dvfs_reg_offsets = sdio_dvfs_reg_backup_offsets;
		host->dvfs_reg_offsets_src = sdio_reg_backup_offsets_src;
		host->dvfs_reg_backup_cnt = BACKUP_REG_COUNT_SDIO;
	}
}

/* Table for usage of use_hw_dvfs and lock_vcore:
 * use_hw_dvfs  lock_vcore  Usage
 * ===========  ==========  ==========================================
 *      1           X       Use HW_DVFS
 *      0           0       Don't use HW_DVFS and
 *                           1. Let CRC error.
 *                           2. Autok window of all vcores can overlap properly
 *      0           1       Don't use HW_DVFS and lock vcore
 */

int emmc_execute_dvfs_autok(struct msdc_host *host, u32 opcode)
{
	int ret = 0;
	int vcore = 0;
#if defined(VCOREFS_READY)
	int vcore_dvfs_work;
#endif
	u8 *res;

#if defined(VCOREFS_READY)
	if (host->use_hw_dvfs == 0) {
		vcore = AUTOK_VCORE_MERGE;
	} else {
		vcore_dvfs_work = is_vcorefs_can_work();
		if (vcore_dvfs_work == -1) {
			vcore = 0;
			pr_info("DVFS feature not enabled\n");
		} else if (vcore_dvfs_work == 0) {
			vcore = vcorefs_get_hw_opp();
			pr_info("DVFS not ready\n");
		} else if (vcore_dvfs_work == 1) {
			vcore = vcorefs_get_hw_opp();
			pr_info("DVFS ready\n");
		} else {
			vcore = 0;
			pr_notice("is_vcorefs_can_work() return invalid value\n");
		}
		if (vcore >= AUTOK_VCORE_NUM)
			vcore = AUTOK_VCORE_NUM - 1;
	}
#endif

	res = host->autok_res[vcore];

	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS200) {
		#ifdef MSDC_HQA
		msdc_HQA_set_voltage(host);
		#endif

		if (opcode == MMC_SEND_STATUS) {
			pr_info("[AUTOK]eMMC HS200 Tune CMD only\n");
			ret = hs200_execute_tuning_cmd(host, res);
		} else {
			pr_info("[AUTOK]eMMC HS200 Tune\n");
			ret = hs200_execute_tuning(host, res);
		}
		if (host->hs400_mode == false) {
			host->is_autok_done = 1;
			complete(&host->autok_done);
		}
	} else if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		#ifdef MSDC_HQA
		msdc_HQA_set_voltage(host);
		#endif

		if (opcode == MMC_SEND_STATUS) {
			pr_info("[AUTOK]eMMC HS400 Tune CMD only\n");
			ret = hs400_execute_tuning_cmd(host, res);
		} else {
			pr_info("[AUTOK]eMMC HS400 Tune\n");
			ret = hs400_execute_tuning(host, res);
		}
		host->is_autok_done = 1;
		complete(&host->autok_done);
	}

	if (host->use_hw_dvfs == 1)
		msdc_set_hw_dvfs(vcore, host);
	/* msdc_dump_register_core(host, NULL); */

	return ret;
}

void sdio_execute_dvfs_autok_mode(struct msdc_host *host, bool ddr208)
{
#if !defined(FPGA_PLATFORM)
	void __iomem *base = host->base;
	int sdio_res_exist = 0;
	int vcore;
	int i, vcore_step1 = -1, vcore_step2 = 0;
	int merge_result, merge_mode, merge_window;
	int (*autok_init)(struct msdc_host *host);
	int (*autok_execute)(struct msdc_host *host, u8 *res);

	if (ddr208) {
		autok_init = autok_init_ddr208;
		autok_execute = autok_sdio30_plus_tuning;
		merge_mode = MERGE_DDR208;
		pr_info("[AUTOK]SDIO DDR208 Tune\n");
	} else {
		autok_init = autok_init_sdr104;
		autok_execute = autok_execute_tuning;
		merge_mode = MERGE_HS200_SDR104;
		pr_info("[AUTOK]SDIO SDR104 Tune\n");
	}

	if (host->is_autok_done) {
		autok_init(host);

		/* Check which vcore setting to apply */
		if (host->use_hw_dvfs == 0) {
			if (host->lock_vcore == 0)
				vcore = AUTOK_VCORE_MERGE;
			else
				vcore = AUTOK_VCORE_LEVEL0;
		} else {
			/* Force use_hw_dvfs as 1 to shut-up annoying
			 * "maybe-uninitialized" error
			 */
			host->use_hw_dvfs = 1;
			vcore = msdc_vcorefs_get_hw_opp(host);
		}

		autok_tuning_parameter_init(host, host->autok_res[vcore]);

		pr_info("[AUTOK]Apply first tune para (vcore = %d) %s HW DVFS\n",
			vcore, (host->use_hw_dvfs ? "with" : "without"));
		if (host->use_hw_dvfs == 1)
			msdc_dvfs_reg_restore(host);

		return;
	}
	/* HQA need read autok setting from file */
	sdio_res_exist = sdio_autok_res_exist(host);

	/* Wait DFVS ready for excute autok here */
	sdio_autok_wait_dvfs_ready();

	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, i) != 0)
			pr_notice("vcorefs_request_dvfs_opp@LEVEL%d fail!\n", i);
#ifdef POWER_READY
		pmic_read_interface(REG_VCORE_VOSEL, &vcore_step2,
			MASK_VCORE_VOSEL, SHIFT_VCORE_VOSEL);
#else
		vcore_step2 = 0;
#endif /* POWER_READY */
		if (vcore_step2 == vcore_step1) {
			pr_info("skip duplicated vcore autok\n");
			if (i >= 1)
				memcpy(host->autok_res[i], host->autok_res[i-1],
					TUNING_PARA_SCAN_COUNT);
		} else {
			#ifdef SDIO_HQA
			msdc_HQA_set_voltage(host);
			#endif

			if (sdio_res_exist)
				sdio_autok_res_apply(host, i);
			else
				autok_execute(host, host->autok_res[i]);
		}
		vcore_step1 = vcore_step2;

		if (host->use_hw_dvfs == 1)
			msdc_set_hw_dvfs(i, host);
	}

	if (host->use_hw_dvfs == 1)
		msdc_dvfs_reg_backup(host);

	merge_result = autok_vcore_merge_sel(host, merge_mode);
	for (i = CMD_MAX_WIN; i <= H_CLK_TX_MAX_WIN; i++) {
		merge_window = host->autok_res[AUTOK_VCORE_MERGE][i];
		if (merge_window < AUTOK_MERGE_MIN_WIN)
			merge_result = -1;
		if (merge_window != 0xFF)
			pr_info("[AUTOK]merge_window = %d\n", merge_window);
	}

	if (merge_result == 0) {
		host->lock_vcore = 0;
		host->use_hw_dvfs = 0;
		autok_tuning_parameter_init(host, host->autok_res[AUTOK_VCORE_MERGE]);
		pr_info("[AUTOK]No need change para when dvfs\n");
	} else if (host->use_hw_dvfs == 0) {
		autok_tuning_parameter_init(host, host->autok_res[AUTOK_VCORE_LEVEL3]);
		host->lock_vcore = 1;
		pr_info("[AUTOK]Need lock vcore for SDIO access\n");
	} else {
		/* host->use_hw_dvfs == 1 and
		 * autok window of all vcores cannot overlap properly
		 */
		pr_info("[AUTOK]Need change para when dvfs\n");

		msdc_dvfs_reg_backup(host);

		/* Enable HW DVFS, but setting used now is at register offset <=0x104.
		 * Setting at register offset >=0x300 will effect after SPM handshakes
		 * with MSDC.
		 */
		MSDC_WRITE32(MSDC_CFG,
			MSDC_READ32(MSDC_CFG) | (MSDC_CFG_DVFS_EN | MSDC_CFG_DVFS_HW));
	}

	/* Un-request, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_SDIO, OPP_UNREQ) != 0)
		pr_notice("vcorefs_request_dvfs_opp@OPP_UNREQ fail!\n");

	/* Tell DVFS can start now because AUTOK done */
	spm_msdc_dvfs_setting(KIR_AUTOK_SDIO, 1);

	host->is_autok_done = 1;
	complete(&host->autok_done);
#endif
}

static int msdc_io_rw_direct_host(struct mmc_host *host, int write, unsigned fn,
	unsigned addr, u8 in, u8 *out)
{
	struct mmc_command cmd = {0};
	int err;

	if (!host || fn > 7)
		return -EINVAL;

	/* sanity check */
	if (addr & ~0x1FFFF)
		return -EINVAL;

	cmd.opcode = SD_IO_RW_DIRECT;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= fn << 28;
	cmd.arg |= (write && out) ? 0x08000000 : 0x00000000;
	cmd.arg |= addr << 9;
	cmd.arg |= in;
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_AC;

	err = mmc_wait_for_cmd(host, &cmd, 0);
	if (err)
		return err;

	if (mmc_host_is_spi(host)) {
		/* host driver already reported errors */
	} else {
		if (cmd.resp[0] & R5_ERROR)
			return -EIO;
		if (cmd.resp[0] & R5_FUNCTION_NUMBER)
			return -EINVAL;
		if (cmd.resp[0] & R5_OUT_OF_RANGE)
			return -ERANGE;
	}

	if (out) {
		if (mmc_host_is_spi(host))
			*out = (cmd.resp[0] >> 8) & 0xFF;
		else
			*out = cmd.resp[0] & 0xFF;
	}

	return 0;
}

/* #define DEVICE_RX_READ_DEBUG */
void sdio_plus_set_device_rx(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	void __iomem *base = host->base;
	u32 msdc_cfg;
	int retry = 3, cnt = 1000;
#ifdef DEVICE_RX_READ_DEBUG
	unsigned char data;
#endif
	int ret = 0;

	msdc_cfg = MSDC_READ32(MSDC_CFG);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD_HS400, 0);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, 0);
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, 5);
	msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB),
		retry, cnt, host->id);

#ifdef DEVICE_RX_READ_DEBUG
	pr_info("%s +++++++++++++++++++++++++=\n", __func__);
	ret = msdc_io_rw_direct_host(mmc, 0, 0, 0x2, 0, &data);
	pr_info("0x2 data: %x , ret: %x\n", data, ret);
#endif
	ret = msdc_io_rw_direct_host(mmc, 1, 0, 0x2, 0x2, 0);

#ifdef DEVICE_RX_READ_DEBUG
	ret = msdc_io_rw_direct_host(mmc, 0, 0, 0x2, 0, &data);
	pr_info("0x2 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x11C, 0, &data);
	pr_info("0x11C data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x124, 0, &data);
	pr_info("0x124 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x125, 0, &data);
	pr_info("0x125 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x126, 0, &data);
	pr_info("0x126 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x127, 0, &data);
	pr_info("0x127 data: %x , ret: %x\n", data, ret);
#endif

	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x11C, 0x90, 0);

#if 0 /* Device data RX window cover by host data TX */
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x124, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x125, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x126, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x127, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x128, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x129, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x12A, 0x87, 0);
	ret = msdc_io_rw_direct_host(mmc, 1, 1, 0x12B, 0x87, 0);
#endif

#ifdef DEVICE_RX_READ_DEBUG
	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x11C, 0, &data);
	pr_info("0x11C data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x124, 0, &data);
	pr_info("0x124 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x125, 0, &data);
	pr_info("0x125 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x126, 0, &data);
	pr_info("0x126 data: %x , ret: %x\n", data, ret);

	ret = msdc_io_rw_direct_host(mmc, 0, 1, 0x127, 0, &data);
	pr_info("0x127 data: %x , ret: %x\n", data, ret);
#endif

	MSDC_WRITE32(MSDC_CFG, msdc_cfg);
	msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB), retry, cnt, host->id);
}

#define SDIO_CCCR_MTK_DDR208       0xF2
#define SDIO_MTK_DDR208            0x3
#define SDIO_MTK_DDR208_SUPPORT    0x2
int sdio_plus_set_device_ddr208(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	static u8 autok_res104[TUNING_PARA_SCAN_COUNT];
	unsigned char data;
	int err = 0;

	if (host->is_autok_done) {
		autok_init_sdr104(host);
		autok_tuning_parameter_init(host, autok_res104);
	} else {
		autok_execute_tuning(host, autok_res104);
	}

	/* Read SDIO Device CCCR[0x00F2]
	 * Bit[1] Always 1, Support DDR208 Mode.
	 * Bit[0]
	 *        1:Enable DDR208.
	 *        0:Disable DDR208.
	 */
	err = msdc_io_rw_direct_host(mmc, 0, 0, SDIO_CCCR_MTK_DDR208, 0, &data);

	/* Re-autok sdr104 if default setting fail */
	if (err) {
		autok_execute_tuning(host, autok_res104);
		err = msdc_io_rw_direct_host(mmc, 0, 0, SDIO_CCCR_MTK_DDR208, 0, &data);
	}

	if (err) {
		pr_notice("Read SDIO_CCCR_MTK_DDR208 fail\n");
		goto end;
	}
	if ((data & SDIO_MTK_DDR208_SUPPORT) == 0) {
		pr_notice("Device not support SDIO_MTK_DDR208\n");
		err = -1;
		goto end;
	}

	/* Switch to DDR208 Flow :
	 * 1. First switch to DDR50 mode;
	 * 2. Then Host CMD52 Write 0x03/0x01 to CCCR[0x00F2]
	 */
	err = msdc_io_rw_direct_host(mmc, 0, 0, SDIO_CCCR_SPEED, 0, &data);
	if (err) {
		pr_notice("Read SDIO_CCCR_SPEED fail\n");
		goto end;
	}

	data = (data & (~SDIO_SPEED_BSS_MASK)) | SDIO_SPEED_DDR50;
	err = msdc_io_rw_direct_host(mmc, 1, 0, SDIO_CCCR_SPEED, data, NULL);
	if (err) {
		pr_notice("Set SDIO_CCCR_SPEED to DDR fail\n");
		goto end;
	}

	err = msdc_io_rw_direct_host(mmc, 1, 0, SDIO_CCCR_MTK_DDR208,
		SDIO_MTK_DDR208, NULL);
	if (err) {
		pr_notice("Set SDIO_MTK_DDR208 fail\n");
		goto end;
	}

end:

	return err;
}

void sdio_execute_dvfs_autok(struct msdc_host *host)
{
	/* Set device timming for latch data */
	if (host->hw->flags & MSDC_SDIO_DDR208)
		sdio_plus_set_device_rx(host);

	/* Not support DDR208 and only Autok SDR104 */
	if (!(host->hw->flags & MSDC_SDIO_DDR208)) {
		sdio_execute_dvfs_autok_mode(host, 0);
		return;
	}

	if (sdio_plus_set_device_ddr208(host))
		return;

	/* Set HS400 clock mode and DIV = 0 */
	msdc_clk_stable(host, 3, 0, 1);

	/* Find DDR208 timing */
	sdio_execute_dvfs_autok_mode(host, 1);
}

/* Priority: Window Merger > HW DVFS > Lock Vcore > Let it error */
/* invoked by SPM */
int emmc_autok(void)
{
#if !defined(FPGA_PLATFORM) && defined(VCOREFS_READY)
	struct msdc_host *host = mtk_msdc_host[0];
	void __iomem *base;
	int merge_result, merge_mode, merge_window;
	int i, vcore_step1 = -1, vcore_step2 = 0;

	if (!host || !host->mmc) {
		pr_notice("eMMC device not ready\n");
		return -1;
	}

	if (!(host->mmc->caps2 & MMC_CAP2_HS400_1_8V)
	 && !(host->mmc->caps2 & MMC_CAP2_HS200_1_8V_SDR))
		return 0;

	/* Wait completion of AUTOK triggered by eMMC initialization */
	if (!wait_for_completion_timeout(&host->autok_done, 10 * HZ)) {
		pr_notice("eMMC 1st autok not done\n");
		return -1;
	}

	pr_info("emmc autok\n");

	base = host->base;

	mmc_claim_host(host->mmc);

	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		if (vcorefs_request_dvfs_opp(KIR_AUTOK_EMMC, i) != 0)
			pr_notice("vcorefs_request_dvfs_opp@LEVEL%d fail!\n", i);
		pmic_read_interface(REG_VCORE_VOSEL, &vcore_step2,
			MASK_VCORE_VOSEL, SHIFT_VCORE_VOSEL);
		if (vcore_step2 == vcore_step1) {
			pr_info("skip duplicated vcore autok\n");
			if (i >= 1)
				memcpy(host->autok_res[i], host->autok_res[i-1],
					TUNING_PARA_SCAN_COUNT);
		} else {
			emmc_execute_dvfs_autok(host, MMC_SEND_TUNING_BLOCK_HS200);
			if (host->use_hw_dvfs == 0)
				memcpy(host->autok_res[i], host->autok_res[AUTOK_VCORE_MERGE],
					TUNING_PARA_SCAN_COUNT);
		}
		vcore_step1 = vcore_step2;
	}

	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
		merge_mode = MERGE_HS400;
	else
		merge_mode = MERGE_HS200_SDR104;

	merge_result = autok_vcore_merge_sel(host, merge_mode);
	for (i = CMD_MAX_WIN; i <= H_CLK_TX_MAX_WIN; i++) {
		merge_window = host->autok_res[AUTOK_VCORE_MERGE][i];
		if (merge_window < AUTOK_MERGE_MIN_WIN)
			merge_result = -1;
		if (merge_window != 0xFF)
			pr_info("[AUTOK]merge_value = %d\n", merge_window);
	}

	if (merge_result == 0) {
		autok_tuning_parameter_init(host, host->autok_res[AUTOK_VCORE_MERGE]);
		pr_info("[AUTOK]No need change para when dvfs\n");
	} else if (host->use_hw_dvfs == 1) {
		/* Not supported on MT6739 */
	#if 0
	} else if (host->use_hw_dvfs == 0) {
		autok_tuning_parameter_init(host, host->autok_res[AUTOK_VCORE_LEVEL0]);
		pr_info("[AUTOK]Need lock vcore\n");
		host->lock_vcore = 1;
	#endif
	}

	/* Un-request, return 0 pass */
	if (vcorefs_request_dvfs_opp(KIR_AUTOK_EMMC, OPP_UNREQ) != 0)
		pr_notice("vcorefs_request_dvfs_opp@OPP_UNREQ fail!\n");

	/* spm_msdc_dvfs_setting(KIR_AUTOK_EMMC, 1); */

	mmc_release_host(host->mmc);
#endif

	return 0;
}
EXPORT_SYMBOL(emmc_autok);

/* FIX ME: Since card can be insert at any time but this is invoked only once
 * when DVFS ready, this function is not suitable for card insert after boot
 */
int sd_autok(void)
{
	struct msdc_host *host = mtk_msdc_host[1];

	if (!host || !host->mmc) {
		pr_info("SD card not ready\n");
		return -1;
	}

	pr_info("sd autok\n");

	return 0;
}
EXPORT_SYMBOL(sd_autok);

int sdio_autok(void)
{
#if defined(CONFIG_MTK_COMBO_COMM) && !defined(MSDC_HQA)
	struct msdc_host *host = mtk_msdc_host[2];
	int timeout = 0;

	while (!host || !host->hw) {
		pr_info("SDIO host not ready\n");
		msleep(1000);
		timeout++;
		if (timeout == 20) {
			pr_notice("SDIO host not exist\n");
			return -1;
		}
	}

	if (host->hw->host_function != MSDC_SDIO) {
		pr_notice("SDIO device not in this host\n");
		return -1;
	}

	if (host->use_hw_dvfs == 0) {
		/* HW DVFS not support or disabled by device tree */
		spm_msdc_dvfs_setting(KIR_AUTOK_SDIO, 1);
		return 0;
	}

	pr_info("sdio autok\n");

#else
	spm_msdc_dvfs_setting(KIR_AUTOK_SDIO, 1);

#endif

	return 0;
}
EXPORT_SYMBOL(sdio_autok);

void msdc_dump_autok(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
#ifdef MSDC_BRING_UP
	int i, j;
	int bit_pos, byte_pos, start;
	char buf[65];

	SPREAD_PRINTF(buff, size, m, "[AUTOK]VER : 0x%02x%02x%02x%02x\r\n",
		host->autok_res[0][AUTOK_VER3],
		host->autok_res[0][AUTOK_VER2],
		host->autok_res[0][AUTOK_VER1],
		host->autok_res[0][AUTOK_VER0]);

	for (i = 0; i <= AUTOK_VCORE_NUM; i++) {
		start = CMD_SCAN_R0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD Rising \t: %s\r\n", buf);

		start = CMD_SCAN_F0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD Falling \t: %s\r\n", buf);

		start = DAT_SCAN_R0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT Rising \t: %s\r\n", buf);

		start = DAT_SCAN_F0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT Falling \t: %s\r\n", buf);

		start = DS_CMD_SCAN_0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS CMD Window \t: %s\r\n", buf);

		start = DS_DAT_SCAN_0;
		for (j = 0; j < 64; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS DAT Window \t: %s\r\n", buf);

		start = D_DATA_SCAN_0;
		for (j = 0; j < 32; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]Device Data RX \t: %s\r\n", buf);

		start = H_DATA_SCAN_0;
		for (j = 0; j < 32; j++) {
			bit_pos = j % 8;
			byte_pos = j / 8 + start;
			if (host->autok_res[i][byte_pos] & (1 << bit_pos))
				buf[j] = 'X';
			else
				buf[j] = 'O';
		}
		buf[j] = '\0';
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]Host   Data TX \t: %s\r\n", buf);

		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\n"
			"[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\n"
			"[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\n"
			"[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\n"
			"[AUTOK]DAT [TX SEL:%d]\n",
			host->autok_res[i][0], host->autok_res[i][1],
			host->autok_res[i][5], host->autok_res[i][7],
			host->autok_res[i][2], host->autok_res[i][3],
			host->autok_res[i][4],
			host->autok_res[i][13], host->autok_res[i][9],
			host->autok_res[i][11],
			host->autok_res[i][14], host->autok_res[i][16],
			host->autok_res[i][18],
			host->autok_res[i][20]);
	}

	for (i = CMD_MAX_WIN; i <= H_CLK_TX_MAX_WIN; i++)
		SPREAD_PRINTF(buff, size, m, "[AUTOK]Merge Window \t: %d\r\n",
			host->autok_res[AUTOK_VCORE_MERGE][i]);
#endif
}

int msdc_vcorefs_get_hw_opp(struct msdc_host *host)
{
	int vcore;

	if (host->hw->host_function == MSDC_SD) {
		vcore = AUTOK_VCORE_LEVEL0;
	} else if (host->hw->host_function == MSDC_EMMC
		|| host->use_hw_dvfs == 0) {
		if (host->lock_vcore == 0)
			vcore = AUTOK_VCORE_MERGE;
		else
			vcore = AUTOK_VCORE_LEVEL0;
	} else {
		/* not supported */
		pr_notice("SDIO use_hw_dvfs wrongly set\n");
		host->use_hw_dvfs = 0;
		host->lock_vcore = 1;
		vcore = AUTOK_VCORE_LEVEL0;
	}

	return vcore;
}

