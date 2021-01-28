/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/sched.h>
#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/printk.h>
#include <asm/page.h>
#include <linux/gpio.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_lpae.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "mtk_sd.h"
#include <mmc/core/core.h>
#include <mmc/core/card.h>
#include <mmc/core/host.h>
#include <mmc/core/queue.h>
#include <mmc/core/mmc_ops.h>
#include "mmc/host/cqhci-crypto.h"

#ifdef MTK_MSDC_BRINGUP_DEBUG
//#include <mach/mt_pmic_wrap.h>
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
#ifdef CONFIG_MTK_HIBERNATION
#include <mtk_hibernate_dpm.h>
#endif


#ifdef CONFIG_MTK_EMMC_HW_CQ
#include "mmc/host/cmdq_hci.h"
#endif

#include "dbg.h"

#define CAPACITY_2G             (2 * 1024 * 1024 * 1024ULL)

/* FIX ME: Check if its reference in mtk_sd_misc.h can be removed */
u32 g_emmc_mode_switch;

#define MSDC_MAX_FLUSH_COUNT    (3)
#define CACHE_UN_FLUSHED        (0)
#define CACHE_FLUSHED           (1)
#ifdef MTK_MSDC_USE_CACHE
static unsigned int g_cache_status = CACHE_UN_FLUSHED;
static unsigned long long g_flush_data_size;
static unsigned int g_flush_error_count;
static int g_flush_error_happened;
#endif

/* if eMMC cache of specific vendor shall be disabled,
 * fill CID.MID into g_emmc_cache_quirk[]
 * exmple:
 * g_emmc_cache_quirk[0] = CID_MANFID_HYNIX;
 * g_emmc_cache_quirk[1] = CID_MANFID_SAMSUNG;
 */
unsigned char g_emmc_cache_quirk[256];
#define CID_MANFID_SANDISK		0x2
#define CID_MANFID_TOSHIBA		0x11
#define CID_MANFID_MICRON		0x13
#define CID_MANFID_SAMSUNG		0x15
#define CID_MANFID_SANDISK_NEW		0x45
#define CID_MANFID_HYNIX		0x90
#define CID_MANFID_KSI			0x70

static u16 u_sdio_irq_counter;
static u16 u_msdc_irq_counter;

bool emmc_sleep_failed;
static struct workqueue_struct *wq_init;

#define DRV_NAME                "mtk-msdc"

#define MSDC_COOKIE_PIO         (1<<0)
#define MSDC_COOKIE_ASYNC       (1<<1)

#define msdc_use_async(x)       (x & MSDC_COOKIE_ASYNC)
#define msdc_use_async_dma(x)   (msdc_use_async(x) && (!(x & MSDC_COOKIE_PIO)))
#define msdc_use_async_pio(x)   (msdc_use_async(x) && ((x & MSDC_COOKIE_PIO)))

#define MSDC_AUTOSUSPEND_DELAY_MS 10

u8 g_emmc_id;
unsigned int cd_gpio;

int msdc_rsp[] = {
	0,                      /* RESP_NONE */
	1,                      /* RESP_R1 */
	2,                      /* RESP_R2 */
	3,                      /* RESP_R3 */
	4,                      /* RESP_R4 */
	1,                      /* RESP_R5 */
	1,                      /* RESP_R6 */
	1,                      /* RESP_R7 */
	7,                      /* RESP_R1b */
};

#define msdc_init_bd(bd, blkpad, dwpad, dptr, dlen) \
	do { \
		WARN_ON(dlen > 0xFFFFFFUL); \
		((struct bd_t *)bd)->blkpad = blkpad; \
		((struct bd_t *)bd)->dwpad = dwpad; \
		((struct bd_t *)bd)->ptrh4 = upper_32_bits(dptr) & 0xF; \
		((struct bd_t *)bd)->ptr = lower_32_bits(dptr); \
		((struct bd_t *)bd)->buflen = dlen; \
	} while (0)

#ifdef CONFIG_NEED_SG_DMA_LENGTH
#define msdc_sg_len(sg, dma)    ((dma) ? (sg)->dma_length : (sg)->length)
#else
#define msdc_sg_len(sg, dma)    sg_dma_len(sg)
#endif

#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
#define msdc_dma_on()           { msdc_check_fde(mmc, mrq); \
			MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_PIO); }
#define msdc_dma_off()          { MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_PIO); \
			MSDC_WRITE32(MSDC_AES_SEL, 0x0); }
#define MSDC_CHECK_FDE_ERR(mmc, mrq)    msdc_check_fde_err(mmc, mrq)

#elif defined(CONFIG_MTK_HW_FDE) && !defined(CONFIG_MTK_HW_FDE_AES) \
	&& !defined(CONFIG_MMC_CRYPTO)
#define msdc_dma_on()           { msdc_pre_crypto(mmc, mrq); \
			MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_PIO); }
#define msdc_dma_off()          { MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_PIO); \
			msdc_post_crypto(host); }
#else
#define msdc_dma_on()           MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_PIO)
#define msdc_dma_off()          MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_PIO)
#endif

/***************************************************************
 * BEGIN register dump functions
 ***************************************************************/
#define PRINTF_REGISTER_BUFFER_SIZE 512
#define ONE_REGISTER_STRING_SIZE    14

#define MSDC_REG_PRINT(OFFSET, VAL, MSG_SZ, MSG_ACCU_SZ, \
	BUF_SZ, BUF, BUF_CUR, SEQ) \
{ \
	if (SEQ) { \
		seq_printf(SEQ, "R[%x]=0x%.8x\n", OFFSET, VAL); \
		continue; \
	} \
	MSG_ACCU_SZ += MSG_SZ; \
	if (MSG_ACCU_SZ >= BUF_SZ) { \
		pr_info("%s", BUF); \
		memset(BUF, 0, BUF_SZ); \
		MSG_ACCU_SZ = MSG_SZ; \
		BUF_CUR = BUF; \
	} \
	snprintf(BUF_CUR, MSG_SZ+1, "[%.3hx:%.8x]", OFFSET, VAL); \
	BUF_CUR += MSG_SZ; \
}

#define MSDC_RST_REG_PRINT_BUF(MSG_ACCU_SZ, BUF_SZ, BUF, BUF_CUR) \
{ \
	MSG_ACCU_SZ = 0; \
	memset(BUF, 0, BUF_SZ); \
	BUF_CUR = BUF; \
}

void msdc_dump_register_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 id = host->id;
	u32 msg_size = 0;
	u32 val;
	u16 offset, i, j;
	char buffer[PRINTF_REGISTER_BUFFER_SIZE + 1];
	char *buffer_cur_ptr = buffer;

	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	SPREAD_PRINTF(buff, size, m, "MSDC%d normal register\n", id);
	for (i = 0; msdc_offsets[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets[i];
		val = MSDC_READ32(base + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	if (host->dvfs_reg_backup_cnt == 0)
		goto skip_dump_dvfs_reg;

	MSDC_RST_REG_PRINT_BUF(msg_size,
		PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr);

	SPREAD_PRINTF(buff, size, m, "MSDC%d DVFS register\n", id);

	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		for (j = 0; j < host->dvfs_reg_backup_cnt; j++) {
			offset = host->dvfs_reg_offsets[j]
				+ MSDC_DVFS_SET_SIZE * i;
			val = MSDC_READ32(host->base + offset);
			MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE,
				msg_size, PRINTF_REGISTER_BUFFER_SIZE, buffer,
				buffer_cur_ptr, m);
		}
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

skip_dump_dvfs_reg:

	if (!host->base_top)
		goto skip_dump_top_reg;

	MSDC_RST_REG_PRINT_BUF(msg_size,
		PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr);

	SPREAD_PRINTF(buff, size, m, "MSDC%d top register\n", id);

	for (i = 0;  msdc_offsets_top[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets_top[i];
		val = MSDC_READ32(host->base_top + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

skip_dump_top_reg:

	if (host->use_hw_dvfs != 1)
		return;

	MSDC_RST_REG_PRINT_BUF(msg_size,
		PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr);
	SPREAD_PRINTF(buff, size, m, "MSDC%d top DVFS register\n", id);

	for (i = 0; i < AUTOK_VCORE_NUM; i++) {
		for (j = 0; j < host->dvfs_reg_backup_cnt_top; j++) {
			offset = host->dvfs_reg_offsets_top[j]
				+ MSDC_TOP_SET_SIZE * i;
			val = MSDC_READ32(host->base_top + offset);
			MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE,
				msg_size, PRINTF_REGISTER_BUFFER_SIZE, buffer,
				buffer_cur_ptr, m);
		}
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);
}

void msdc_dump_register(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	msdc_dump_register_core(buff, size, m, host);
}

void msdc_dump_dbg_register(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 msg_size = 0;
	u16 i;
	char buffer[PRINTF_REGISTER_BUFFER_SIZE + 1];
	char *buffer_cur_ptr = buffer;

	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	SPREAD_PRINTF(buff, size, m, "MSDC debug register [set:out]\n");
	for (i = 0; i < MSDC_DEBUG_REGISTER_COUNT + 1; i++) {
		msg_size += ONE_REGISTER_STRING_SIZE;
		if (msg_size >= PRINTF_REGISTER_BUFFER_SIZE) {
			SPREAD_PRINTF(buff, size, m, "%s", buffer);
			memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
			msg_size = ONE_REGISTER_STRING_SIZE;
			buffer_cur_ptr = buffer;
		}
		MSDC_WRITE32(MSDC_DBG_SEL, i);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE+1,
			"[%.3hx:%.8x]", i, MSDC_READ32(MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}

	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	MSDC_WRITE32(MSDC_DBG_SEL, 0x27);
	msg_size = 0;
	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	buffer_cur_ptr = buffer;
	SPREAD_PRINTF(buff, size, m, "MSDC debug 0x224 register [set:out]\n");
	for (i = 0; i < 12; i++) {
		msg_size += ONE_REGISTER_STRING_SIZE;
		if (msg_size >= PRINTF_REGISTER_BUFFER_SIZE) {
			SPREAD_PRINTF(buff, size, m, "%s", buffer);
			memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
			msg_size = ONE_REGISTER_STRING_SIZE;
			buffer_cur_ptr = buffer;
		}
		MSDC_WRITE32(EMMC50_CFG4, i);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE+1,
			"[%.3hx:%.8x]", i, MSDC_READ32(MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}

	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	MSDC_WRITE32(MSDC_DBG_SEL, 0);
}

void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	u32 id)
{
	struct msdc_host *host = mtk_msdc_host[id];
	struct mmc_host *mmc;

	if (host == NULL) {
		SPREAD_PRINTF(buff, size, m, "msdc host<%d> null\n", id);
		return;
	}

	mmc = host->mmc;

	if (host->tuning_in_progress == true)
		return;

	/* when detect card, timeout log is not needed */
	if (!sd_register_zone[id]) {
		SPREAD_PRINTF(buff, size, m,
	"msdc host<%d> is timeout when detect, so don't dump register\n", id);
		return;
	}

	host->prev_cmd_cause_dump++;
	if (host->prev_cmd_cause_dump > 1)
		return;

	msdc_dump_vcore(buff, size, m);
	msdc_dump_dvfs_reg(buff, size, m, host);

	msdc_dump_register(buff, size, m, host);
	SPREAD_PRINTF(buff, size, m, "latest_INT_status<0x%.8x>",
		latest_int_status[id]);

	if (!buff)
		mdelay(10);

	msdc_dump_clock_sts(buff, size, m, host);

	msdc_dump_ldo_sts(buff, size, m, host);

	msdc_dump_padctl(buff, size, m, host);

	/* prevent bad sdcard, print too much log */
	if (host->id != 1)
		msdc_dump_autok(buff, size, m, host);

	if (!buff)
		mdelay(10);

	msdc_dump_dbg_register(buff, size, m, host);
	mmc_cmd_dump(NULL, NULL, NULL, host->mmc, 100);
}
EXPORT_SYMBOL(msdc_dump_info);
/***************************************************************
 * END register dump functions
 ***************************************************************/

/*
 * for AHB read / write debug
 * return DMA status.
 */
int msdc_get_dma_status(int host_id)
{
	if (host_id < 0 || host_id >= HOST_MAX_NUM) {
		pr_notice("[%s] failed to get dma status, invalid host_id %d\n",
			__func__, host_id);
	} else if (msdc_latest_transfer_mode[host_id] == MODE_DMA) {
		if (msdc_latest_op[host_id] == OPER_TYPE_READ)
			return 1;       /* DMA read */
		else if (msdc_latest_op[host_id] == OPER_TYPE_WRITE)
			return 2;       /* DMA write */
	} else if (msdc_latest_transfer_mode[host_id] == MODE_PIO) {
		return 0;               /* PIO mode */
	}

	return -1;
}
EXPORT_SYMBOL(msdc_get_dma_status);

void msdc_clr_fifo(unsigned int id)
{
	int retry = 3, cnt = 1000;
	void __iomem *base;

	if (id < 0 || id >= HOST_MAX_NUM)
		return;
	base = mtk_msdc_host[id]->base;

	if (MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS) {
		pr_notice("<<<WARN>>>: msdc%d, clear FIFO when DMA active, MSDC_DMA_CFG=0x%x\n",
			id, MSDC_READ32(MSDC_DMA_CFG));
		//show_stack(current, NULL);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
		msdc_retry((MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS),
			retry, cnt, id);
		if (retry == 0) {
			pr_notice("<<<WARN>>>: msdc%d, faield to stop DMA before clear FIFO, MSDC_DMA_CFG=0x%x\n",
				id, MSDC_READ32(MSDC_DMA_CFG));
			return;
		}
	}

	retry = 3;
	cnt = 1000;
	MSDC_SET_BIT32(MSDC_FIFOCS, MSDC_FIFOCS_CLR);
	msdc_retry(MSDC_READ32(MSDC_FIFOCS) & MSDC_FIFOCS_CLR, retry, cnt, id);
}

int msdc_clk_stable(struct msdc_host *host, u32 mode, u32 div,
	u32 hs400_div_dis)
{
	void __iomem *base = host->base;
	int retry = 0;
	int cnt = 1000;
	int retry_cnt = 1;
	int lock;

	do {
		retry = 3;
		lock = spin_is_locked(&host->lock);
		if (lock)
			spin_unlock(&host->lock);
		clk_disable_unprepare(host->clk_ctl);
		MSDC_SET_FIELD(MSDC_CFG,
			MSDC_CFG_CKMOD_HS400 | MSDC_CFG_CKMOD | MSDC_CFG_CKDIV,
			(hs400_div_dis << 14) | (mode << 12) |
				((div + retry_cnt) % 0xfff));
		(void)clk_prepare_enable(host->clk_ctl);
		if (lock)
			spin_lock(&host->lock);
		msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB), retry,
			cnt, host->id);

		if (retry == 0) {
			pr_info("msdc%d on clock failed ===> retry twice\n",
				host->id);

			msdc_clk_disable(host);
			msdc_clk_enable(host);
			msdc_dump_info(NULL, 0, NULL, host->id);
			host->prev_cmd_cause_dump = 0;
		}
		retry = 3;
		cnt = 1000;
		if (lock)
			spin_unlock(&host->lock);
		clk_disable_unprepare(host->clk_ctl);
		MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
		(void)clk_prepare_enable(host->clk_ctl);
		if (lock)
			spin_lock(&host->lock);
		msdc_retry(!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB), retry,
			cnt, host->id);
		if (retry == 0)
			msdc_dump_info(NULL, 0, NULL, host->id);
		msdc_reset_hw(host->id);
		if (retry_cnt == 2)
			break;
		retry_cnt++;
	} while (!retry);

	return 0;
}

#define msdc_irq_save(val) \
	do { \
		val = MSDC_READ32(MSDC_INTEN); \
		MSDC_CLR_BIT32(MSDC_INTEN, val); \
	} while (0)

#define msdc_irq_restore(val) \
	MSDC_SET_BIT32(MSDC_INTEN, val)

/* set the edge of data sampling */
void msdc_set_smpl(struct msdc_host *host, u32 clock_mode, u8 mode, u8 type,
	u8 *edge)
{
	void __iomem *base = host->base;

	switch (type) {
	case TYPE_CMD_RESP_EDGE:
		if (clock_mode == 3) {
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_PADCMD_LATCHCK, 0);
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CMD_RESP_SEL, 0);
		}

		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, mode);
		} else {
			ERR_MSG("invalid resp parameter: type=%d, mode=%d\n",
				type, mode);
		}
		break;
	case TYPE_WRITE_CRC_EDGE:
		if (clock_mode == 3) {
			/*latch write crc status at DS pin*/
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CRC_STS_SEL, 1);
		} else {
			/*latch write crc status at CLK pin*/
			MSDC_SET_FIELD(EMMC50_CFG0,
				MSDC_EMMC50_CFG_CRC_STS_SEL, 0);
		}
		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			if (clock_mode == 3) {
				MSDC_SET_FIELD(EMMC50_CFG0,
					MSDC_EMMC50_CFG_CRC_STS_EDGE, mode);
			} else {
				MSDC_SET_FIELD(MSDC_PATCH_BIT2,
					MSDC_PB2_CFGCRCSTSEDGE, mode);
			}
		} else {
			ERR_MSG("invalid crc parameter: type=%d, mode=%d\n",
				type, mode);
		}
		break;
	case TYPE_READ_DATA_EDGE:
		if (clock_mode == 3) {
			/* for HS400, start bit is output on both edge */
			MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_START_BIT,
				START_AT_RISING_AND_FALLING);
		} else {
			/* for the other modes, start bit is only output on
			 * rising edge; but DDR50 can try falling edge
			 * if error casued by pad delay
			 */
			MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_START_BIT,
				START_AT_RISING);
		}
		if (mode == MSDC_SMPL_RISING || mode == MSDC_SMPL_FALLING) {
			MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL, 0);
			if ((clock_mode == 2) || (clock_mode == 3))
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, 0);
			else
				MSDC_SET_FIELD(MSDC_PATCH_BIT0,
					MSDC_PB0_RD_DAT_SEL, mode);
		} else {
			ERR_MSG("invalid read parameter: type=%d, mode=%d\n",
				type, mode);
		}
		break;
	default:
		ERR_MSG("invalid parameter: type=%d, mode=%d\n", type, mode);
		break;
	}
}

void msdc_set_smpl_all(struct msdc_host *host, u32 clock_mode)
{
	msdc_set_smpl(host, clock_mode, host->hw->cmd_edge,
		TYPE_CMD_RESP_EDGE, NULL);
	msdc_set_smpl(host, clock_mode, host->hw->rdata_edge,
		TYPE_READ_DATA_EDGE, NULL);
	msdc_set_smpl(host, clock_mode, host->hw->wdata_edge,
		TYPE_WRITE_CRC_EDGE, NULL);
}

/* sd card change voltage wait time =
 * (1/freq) * SDC_VOL_CHG_CNT(default 0x145)
 */
#define msdc_set_vol_change_wait_count(count) \
	MSDC_SET_FIELD(SDC_VOL_CHG, SDC_VOL_CHG_CNT, (count))

void msdc_set_check_endbit(struct msdc_host *host, bool enable)
{
	void __iomem *base = host->base;

	if (enable) {
		MSDC_SET_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_INDEX_CHECK);
		MSDC_SET_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_ENDBIT_CHECK);
	} else {
		MSDC_CLR_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_INDEX_CHECK);
		MSDC_CLR_BIT32(SDC_ADV_CFG0, SDC_ADV_CFG0_ENDBIT_CHECK);
	}
}

/* count of bad sd detecter (or bad sd condition kinds),
 * we can add it here if has other condition
 */
#define BAD_SD_DETECTER_COUNT 1

/* we take it as bad sd when the bad sd condition occurs
 * out of tolerance
 */
static u32 bad_sd_tolerance[BAD_SD_DETECTER_COUNT] = {10};

/* bad sd condition occur times
 */
static u32 bad_sd_detecter[BAD_SD_DETECTER_COUNT] = {0};

/* bad sd condition occur times will reset to zero by self
 * when reach the forget time (when set to 0, means not
 * reset to 0 by self), unit:s
 */
static u32 bad_sd_forget[BAD_SD_DETECTER_COUNT] = {3};

/* the latest occur time of the bad sd condition,
 * unit: clock
 */
static unsigned long bad_sd_timer[BAD_SD_DETECTER_COUNT] = {0};

static void msdc_reset_bad_sd_detecter(struct msdc_host *host)
{
	u32 i = 0;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		return;
	}

	host->block_bad_card = 0;
	for (i = 0; i < BAD_SD_DETECTER_COUNT; i++)
		bad_sd_detecter[i] = 0;
}

static void msdc_detect_bad_sd(struct msdc_host *host, u32 condition)
{
	unsigned long time_current = jiffies;

	if (host == NULL) {
		pr_notice("WARN: host is NULL at %s\n", __func__);
		return;
	}

	if (condition >= BAD_SD_DETECTER_COUNT) {
		pr_notice("msdc1: BAD_SD_DETECTER_COUNT is %d, need check it's definition at %s\n",
			BAD_SD_DETECTER_COUNT, __func__);
		return;
	}

	if (bad_sd_forget[condition]
	&& time_after(time_current,
	(bad_sd_timer[condition] + bad_sd_forget[condition] * HZ)))
		bad_sd_detecter[condition] = 0;
	bad_sd_timer[condition] = time_current;

	if (++(bad_sd_detecter[condition]) >= bad_sd_tolerance[condition])
		msdc_set_bad_card_and_remove(host);
}

static u32 msdc_max_busy_timeout_ms(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u64 timeout;
	u32 mode = 0;

	if (host->sclk == 0) {
		timeout = 0;
	} else {
		timeout = 8192 * (1ULL << 20) * 1000;
		do_div(timeout, host->sclk);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/* DDR mode timeout will be in half */
		if (mode >= 2)
			do_div(timeout, 2);
	}

	N_MSG(OPS, "max timeout: %dms, mode:%d, clk_freq=%dKHz\n",
		(u32)timeout, mode, (host->sclk / 1000));

	return (u32)timeout;
}

static void msdc_set_busy_timeout_ms(struct msdc_host *host, u32 ms)
{
	void __iomem *base = host->base;
	u64 timeout, clk_ns, us;
	u32 mode = 0;

	us = (u64)ms * 1000;

	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000ULL;
		do_div(clk_ns, host->sclk);
		timeout = us * 1000 + clk_ns - 1;
		do_div(timeout, clk_ns);
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (1 << 20) - 1) >> 20;
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout*/
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 8191 ? 8191 : timeout;
	}
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_WRDTOC, (u32)timeout);

	N_MSG(OPS, "Set CMD%d busy tmo: %dms(%d x1M cycles), freq=%dKHz\n",
		host->cmd->opcode,
		(ms > host->max_busy_timeout_ms) ? host->max_busy_timeout_ms :
		ms,
		(u32)timeout + 1, (host->sclk / 1000));
}

static void msdc_set_timeout(struct msdc_host *host, u32 ns, u32 clks)
{
	void __iomem *base = host->base;
	u32 timeout, clk_ns;
	u32 mode = 0;

	host->timeout_ns = ns;
	host->timeout_clks = clks;
	if (host->sclk == 0) {
		timeout = 0;
	} else {
		clk_ns  = 1000000000UL / host->sclk;
		timeout = (ns + clk_ns - 1) / clk_ns + clks;
		/* in 1048576 sclk cycle unit */
		timeout = (timeout + (1 << 20) - 1) >> 20;
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		/*DDR mode will double the clk cycles for data timeout*/
		timeout = mode >= 2 ? timeout * 2 : timeout;
		timeout = timeout > 1 ? timeout - 1 : 0;
		timeout = timeout > 255 ? 255 : timeout;
	}
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, timeout);

	N_MSG(OPS,
"Set read data timeout: %dns %dclks(%d x1M cycles), mode:%d, freq=%dKHz\n",
		ns, clks, timeout + 1, mode, (host->sclk / 1000));
}

/* msdc_eirq_sdio() will be called when EIRQ(for WIFI) */
static void msdc_eirq_sdio(void *data)
{
	struct msdc_host *host = (struct msdc_host *)data;

	N_MSG(INT, "SDIO EINT");
#ifdef SDIO_ERROR_BYPASS
	if (host->sdio_error != -EILSEQ) {
#endif
		mmc_signal_sdio_irq(host->mmc);
#ifdef SDIO_ERROR_BYPASS
	}
#endif
}

void msdc_set_mclk(struct msdc_host *host, unsigned char timing, u32 hz)
{
	void __iomem *base = host->base;
	u32 mode;
	u32 flags;
	u32 div;
	u32 sclk;
	u32 hclk = host->hclk;
	u32 hs400_div_dis = 0; /* FOR MSDC_CFG.HS400CKMOD */

	if (!hz) { /* set mmc system clock to 0*/
		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			host->saved_para.hz = hz;
#ifdef SDIO_ERROR_BYPASS
			host->sdio_error = 0;
#endif
		}
		host->mclk = 0;
		msdc_reset_hw(host->id);
		return;
	}

	msdc_irq_save(flags);

	if (timing == MMC_TIMING_MMC_HS400) {
		mode = 0x3; /* HS400 mode */
		if (hz >= hclk/2) {
			hs400_div_dis = 1;
			div = 0;
			sclk = hclk/2;
		} else {
			hs400_div_dis = 0;
			if (hz >= (hclk >> 2)) {
				div  = 0;         /* mean div = 1/4 */
				sclk = hclk >> 2; /* sclk = clk / 4 */
			} else {
				div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
				sclk = (hclk >> 2) / div;
				div  = (div >> 1);
			}
		}
	} else if ((timing == MMC_TIMING_UHS_DDR50)
		|| (timing == MMC_TIMING_MMC_DDR52)) {
		mode = 0x2; /* ddr mode and use divisor */
		if (hz >= (hclk >> 2)) {
			div  = 0;         /* mean div = 1/4 */
			sclk = hclk >> 2; /* sclk = clk / 4 */
		} else {
			div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
			div  = (div >> 1);
		}
#if !defined(FPGA_PLATFORM)
	} else if (hz >= hclk) {
		mode = 0x1; /* no divisor */
		div  = 0;
		sclk = hclk;
#endif
	} else {
		mode = 0x0; /* use divisor */
		if (hz >= (hclk >> 1)) {
			div  = 0;         /* mean div = 1/2 */
			sclk = hclk >> 1; /* sclk = clk / 2 */
		} else {
			div  = (hclk + ((hz << 2) - 1)) / (hz << 2);
			sclk = (hclk >> 2) / div;
		}
	}

	msdc_clk_stable(host, mode, div, hs400_div_dis);

	host->sclk = sclk;
	host->mclk = hz;
	host->timing = timing;

	/* need because clk changed.*/
	host->max_busy_timeout_ms = msdc_max_busy_timeout_ms(host);
	msdc_set_timeout(host, host->timeout_ns, host->timeout_clks);
	pr_info("msdc%d -> !!! Set<%dKHz> Source<%dKHz> -> sclk<%dKHz> timing<%d> mode<%d> div<%d> hs400_div_dis<%d>\n",
		host->id, hz/1000, hclk/1000, sclk/1000, (int)timing, mode, div,
		hs400_div_dis);

	msdc_irq_restore(flags);
}

void msdc_send_stop(struct msdc_host *host)
{
	struct mmc_command stop = {0};
	struct mmc_request mrq = {0};
	u32 err;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &stop;
	stop.mrq = &mrq;
	stop.data = NULL;
	/* stop busy tmo */
	stop.busy_timeout = 500;

	err = msdc_do_command(host, &stop, CMD_TIMEOUT);
}

int msdc_get_card_status(struct mmc_host *mmc, struct msdc_host *host,
	u32 *status)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err;
	int lock;

	cmd.opcode = MMC_SEND_STATUS;   /* CMD13 */
	cmd.arg = host->app_cmd_arg;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;

	/* some case lock is not held */
	lock = spin_is_locked(&host->lock);
	if (!lock)
		spin_lock(&host->lock);

	err = msdc_do_command(host, &cmd, CMD_CQ_TIMEOUT);

	if (!lock)
		spin_unlock(&host->lock);

	if (status)
		*status = cmd.resp[0];

	return err;
}

static void msdc_pin_reset(struct msdc_host *host, int mode, int force_reset)
{
	struct msdc_hw *hw = (struct msdc_hw *)host->hw;
	void __iomem *base = host->base;

	/* Config reset pin */
	if ((hw->flags & MSDC_RST_PIN_EN) || force_reset) {
		if (mode == MSDC_PIN_PULL_UP)
			MSDC_CLR_BIT32(EMMC_IOCON, EMMC_IOCON_BOOTRST);
		else
			MSDC_SET_BIT32(EMMC_IOCON, EMMC_IOCON_BOOTRST);
	}
}

/* called by ops.set_ios */
static void msdc_set_buswidth(struct msdc_host *host, u32 width)
{
	void __iomem *base = host->base;
	u32 val = MSDC_READ32(SDC_CFG);

	val &= ~SDC_CFG_BUSWIDTH;

	switch (width) {
	default:
	case MMC_BUS_WIDTH_1:
		val |= (MSDC_BUS_1BITS << 16);
		break;
	case MMC_BUS_WIDTH_4:
		val |= (MSDC_BUS_4BITS << 16);
		break;
	case MMC_BUS_WIDTH_8:
		val |= (MSDC_BUS_8BITS << 16);
		break;
	}

	MSDC_WRITE32(SDC_CFG, val);
}

static void msdc_init_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* Power on */
	/* msdc_pin_reset(host, MSDC_PIN_PULL_UP, 0); */

	/* Configure to MMC/SD mode */
	MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC);

	/* Disable HW DVFS */
	if ((host->hw->host_function == MSDC_SDIO)
	&& (host->use_hw_dvfs == 1)) {
	}

	/* Reset */
	msdc_reset_hw(host->id);

	/* Disable card detection */
	MSDC_CLR_BIT32(MSDC_PS, MSDC_PS_CDEN);
	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
		MSDC_CLR_BIT32(MSDC_INTEN, MSDC_INT_CDSC);
		MSDC_CLR_BIT32(SDC_CFG, SDC_CFG_INSWKUP);
	}

	/* Disable and clear all interrupts */
	MSDC_CLR_BIT32(MSDC_INTEN, MSDC_READ32(MSDC_INTEN));
	MSDC_WRITE32(MSDC_INT, MSDC_READ32(MSDC_INT));

	/* reset tuning parameter */
	msdc_init_tune_setting(host);

	/* disable endbit check */
	if (host->id == 1)
		msdc_set_check_endbit(host, 0);

	/* For safety, clear SDC_CFG.SDIO_IDE (INT_DET_EN) & set SDC_CFG.SDIO
	 *  in pre-loader,uboot,kernel drivers.
	 */
	/* Enable SDIO mode. it's must otherwise sdio cmd5 failed */
	MSDC_SET_BIT32(SDC_CFG, SDC_CFG_SDIO);

	if (host->hw->flags & MSDC_SDIO_IRQ) {
		/* enable sdio detection when drivers needs */
		MSDC_SET_BIT32(SDC_CFG, SDC_CFG_SDIOIDE);
	} else {
		MSDC_CLR_BIT32(SDC_CFG, SDC_CFG_SDIOIDE);
	}

	msdc_set_smt(host, 1);

	msdc_set_driving(host, &host->hw->driving);

	msdc_set_pin_mode(host);

	/* msdc_set_ies(host, 1); */

	/* write crc timeout detection */
	MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_DETWR_CRCTMO, 1);

	/* Configure to default data timeout */
	MSDC_SET_FIELD(SDC_CFG, SDC_CFG_DTOC, DEFAULT_DTOC);

	msdc_set_buswidth(host, MMC_BUS_WIDTH_1);

	/* Configure support 64G */
	if (enable_4G())
		MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_SUPPORT64G);
	else
		MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_SUPPORT64G);

	host->need_tune = TUNE_NONE;
	host->tune_smpl_times = 0;
	host->reautok_times = 0;

	/* Set default sample edge, use mode 0 for init */
	msdc_set_smpl_all(host, 0);

#ifdef CONFIG_MTK_EMMC_HW_CQ
	if ((host->hw->host_function == MSDC_EMMC)
		&& (host->mmc->caps2 & MMC_CAP2_CQE)) {
		MSDC_WRITE32(MSDC_INTEN, MSDC_INT_XFER_COMPL
				| MSDC_INT_DATTMO | MSDC_INT_DATCRCERR
				| MSDC_INT_CMDQ | MSDC_INT_CMDTMO
				| MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR);
	}
#endif

	N_MSG(FUC, "init hardware done!");
}

/*
 * card hw reset if error
 * 1. reset pin:    DONW => 2us    => UP  => 200us
 * 2. power:        OFF     => 10us  => ON => 200us
 */
static void msdc_card_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	pr_notice("XXX msdc%d reset card\n", host->id);

	if (mmc->caps & MMC_CAP_HW_RESET) {
		if (host->power_control) {
			host->power_control(host, 0);
			udelay(10);
			host->power_control(host, 1);
		}
		usleep_range(200, 500);

		msdc_pin_reset(host, MSDC_PIN_PULL_DOWN, 1);
		udelay(2);
		msdc_pin_reset(host, MSDC_PIN_PULL_UP, 1);
		usleep_range(200, 500);
	}

	mmc->ios.timing = MMC_TIMING_LEGACY;
	mmc->ios.clock = 260000;
	msdc_ops_set_ios(mmc, &mmc->ios);

	msdc_init_hw(host);

}

static int msdc_prepare_hs400_tuning(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->hs400_mode = true;

	return 0;
}

static void msdc_set_power_mode(struct msdc_host *host, u8 mode)
{
	N_MSG(CFG, "Set power mode(%d)", mode);
	if (host->power_mode == MMC_POWER_OFF && mode != MMC_POWER_OFF) {
		msdc_pin_reset(host, MSDC_PIN_PULL_UP, 0);
		msdc_pin_config(host, MSDC_PIN_PULL_UP);

		if (host->power_control)
			host->power_control(host, 1);

		if (host->id == 1) {
			mdelay(10);
			if (msdc_oc_check(host, 1))
				return;

			(void)msdc_io_check(host);

			msdc_set_check_endbit(host, 1);
		}

	} else if (host->power_mode != MMC_POWER_OFF && mode == MMC_POWER_OFF) {

		if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ)) {
			msdc_pin_config(host, MSDC_PIN_PULL_UP);
		} else {

			if (host->power_control)
				host->power_control(host, 0);

			if (host->hw->host_function == MSDC_SD) {
				/* do not set same as mmc->ios.clock in
				 * sdcard_reset_tuning() or else it will be
				 * set as block_bad_card when power cycle
				 */
				if (host->mclk == HOST_MIN_MCLK) {
					host->block_bad_card = 1;
					pr_notice("[%s]: msdc%d power off at clk %dhz set block_bad_card = %d\n",
						__func__, host->id, host->mclk,
						host->block_bad_card);
				}
			}

			msdc_pin_config(host, MSDC_PIN_PULL_DOWN);
		}
		mdelay(10);
		msdc_pin_reset(host, MSDC_PIN_PULL_DOWN, 0);
	}
	host->power_mode = mode;
}

int msdc_switch_part(struct msdc_host *host, char part_id)
{
	int ret = 0;
	u8 *l_buf = NULL;

	if (!host || !host->mmc || !host->mmc->card)
		return -ENOMEDIUM;

	ret = mmc_get_ext_csd(host->mmc->card, &l_buf);
	if (ret)
		return ret;

	if ((part_id >= 0) && (part_id != (l_buf[EXT_CSD_PART_CONFIG] & 0x7))) {
		l_buf[EXT_CSD_PART_CONFIG] &= ~0x7;
		l_buf[EXT_CSD_PART_CONFIG] |= (part_id & 0x7);
		ret = mmc_switch(host->mmc->card, 0, EXT_CSD_PART_CONFIG,
			l_buf[EXT_CSD_PART_CONFIG], 1000);
	}
	kfree(l_buf);

	return ret;
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
static int check_enable_cqe(void)
{
#if !defined(FPGA_PLATFORM)
	enum boot_mode_t mode;

	mode = get_boot_mode();
	/*
	 * Disable cqe in recovery/power off mode, for bypass flush
	 *
	 * Device will return switch error if flush cache
	 * with cache disabled.
	 */
	if ((mode == RECOVERY_BOOT) ||
		(mode == KERNEL_POWER_OFF_CHARGING_BOOT) ||
		(mode == LOW_POWER_OFF_CHARGING_BOOT))
		return 0;

	return 1;
#else
	return 1;
#endif
}
#endif

static int msdc_cache_onoff(struct mmc_data *data)
{
#if !defined(FPGA_PLATFORM)
	u8 *ptr = (u8 *) sg_virt(data->sg);
#if defined(MTK_MSDC_USE_CACHE)
	int i;
	enum boot_mode_t mode;

	/*
	 * disable cache in recovery and charger modes
	 */
	mode = get_boot_mode();
	if ((mode == RECOVERY_BOOT) ||
		(mode == KERNEL_POWER_OFF_CHARGING_BOOT) ||
		(mode == LOW_POWER_OFF_CHARGING_BOOT)) {
		/* Set cache_size as 0 so that mmc layer won't enable cache */
		*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
		return 0;
	}
	/*
	 * Enable cache by eMMC vendor
	 * disable emmc cache if eMMC vendor is in emmc_cache_quirk[]
	 */

	for (i = 0; g_emmc_cache_quirk[i] != 0 ; i++) {
		if (g_emmc_cache_quirk[i] == g_emmc_id) {
			/* Set cache_size as 0
			 * so that mmc layer won't enable cache
			 */
			*(ptr + 252) = *(ptr + 251) = 0;
			*(ptr + 250) = *(ptr + 249) = 0;
		}
	}
#else
	*(ptr + 252) = *(ptr + 251) = *(ptr + 250) = *(ptr + 249) = 0;
#endif
#endif
	return 0;
}

int msdc_cache_ctrl(struct msdc_host *host, unsigned int enable,
	u32 *status)
{
	struct mmc_command cmd = { 0 };
	struct mmc_request mrq = { 0 };
	u32 err;

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24)
		| (EXT_CSD_CACHE_CTRL << 16) | (!!enable << 8)
		| EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &cmd;
	cmd.mrq = &mrq;
	cmd.data = NULL;
	/* set CMD6 max tmo */
	cmd.busy_timeout = 2550;

	ERR_MSG("do eMMC %s Cache\n", (enable ? "enable" : "disable"));
	err = msdc_do_command(host, &cmd, CMD_TIMEOUT);

	if (status)
		*status = cmd.resp[0];
	if (!err) {
		host->mmc->card->ext_csd.cache_ctrl = !!enable;
		/* FIX ME, check if the next 2 line can be removed */
		host->autocmd |= MSDC_AUTOCMD23;
		N_MSG(CHE,
		"enable AUTO_CMD23 because Cache feature is disabled\n");
	}

	return err;
}

#ifdef MTK_MSDC_USE_CACHE
static void msdc_update_cache_flush_status(struct msdc_host *host,
	struct mmc_request *mrq, struct mmc_data *data,
	u32 l_bypass_flush)
{
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_command *sbc = NULL;

	if (!check_mmc_cache_ctrl(host->mmc->card))
		return;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (check_mmc_cmd47(cmd->opcode))
		sbc = host->mmc->areq_que[(cmd->arg >> 16)
			& 0x1f]->mrq_que->sbc;
	else
#endif
		if (check_mmc_cmd2425(cmd->opcode))
			sbc = mrq->sbc;

	if (sbc) {
		if ((host->error == 0)
		 && ((sbc->arg & (0x1 << 24)) || (sbc->arg & (0x1 << 31)))) {
			/* if reliable write, or force prg write succeed,
			 * do set cache flushed status
			 */
			if (g_cache_status == CACHE_UN_FLUSHED) {
				g_cache_status = CACHE_FLUSHED;
				N_MSG(CHE,
		"reliable/force prg write happened, g_flush_data_size = %lld",
					g_flush_data_size);
				g_flush_data_size = 0;
			}
		} else if (host->error == 0) {
			/* if normal write succee,
			 * do clear the cache flushed status
			 */
			if (g_cache_status == CACHE_FLUSHED) {
				g_cache_status = CACHE_UN_FLUSHED;
				N_MSG(CHE, "normal write happened");
			}
			g_flush_data_size += data->blocks;
		} else if (host->error) {
			g_flush_data_size += data->blocks;
			ERR_MSG("write error happened, g_flush_data_size=%lld",
				g_flush_data_size);
		}
	} else if (l_bypass_flush == 0) {
		if (host->error == 0) {
			/* if flush cache of emmc device successfully,
			 * do set the cache flushed status
			 */
			g_cache_status = CACHE_FLUSHED;
			N_MSG(CHE,
	"flush happened, update g_cache_status = %d, g_flush_data_size = %lld",
				g_cache_status, g_flush_data_size);
			g_flush_data_size = 0;
		} else {
			g_flush_error_happened = 1;
		}
	}
}

void msdc_check_cache_flush_error(struct msdc_host *host,
	struct mmc_command *cmd)
{
	if (g_flush_error_happened &&
	    check_mmc_cache_ctrl(host->mmc->card) &&
	    check_mmc_cache_flush_cmd(cmd)) {
		g_flush_error_count++;
		g_flush_error_happened = 0;
		ERR_MSG(
		"the %d time flush error happened, g_flush_data_size = %lld",
			g_flush_error_count, g_flush_data_size);
		/*
		 * if reinit emmc at resume, cache should not be enabled
		 * because too much flush error, so add cache quirk for
		 * this emmmc.
		 * if awake emmc at resume, cache should not be enabled
		 * because too much flush error, so force set cache_size = 0
		 */
		if (g_flush_error_count >= MSDC_MAX_FLUSH_COUNT) {
			if (!msdc_cache_ctrl(host, 0, NULL)) {
				g_emmc_cache_quirk[0] = g_emmc_id;
				host->mmc->card->ext_csd.cache_size = 0;
			}
			pr_notice("msdc%d:flush cache error count = %d, Disable cache\n",
				host->id, g_flush_error_count);
		}
	}
}
#endif

/*--------------------------------------------------------------------------*/
/* mmc_host_ops members                                                     */
/*--------------------------------------------------------------------------*/
static u32 wints_cmd = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
static u32 wints_acmd = MSDC_INT_ACMDRDY | MSDC_INT_ACMDCRCERR
	| MSDC_INT_ACMDTMO;

static void msdc_set_cmd_intr(struct msdc_host *host,
	bool check_busy, unsigned long timeout_ms)
{
	void __iomem *base = host->base;
	unsigned long flags;

	init_completion(&host->cmd_done);

	if (check_busy) {
		msdc_set_busy_timeout_ms(host, timeout_ms);
		/* set check busy */
		MSDC_SET_BIT32(MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
	}

	host->use_cmd_intr = true; /* set flag */
	host->intsts = 0;

	/* use interrupt way */
	spin_lock_irqsave(&host->reg_lock, flags);
	MSDC_SET_BIT32(MSDC_INTEN, wints_cmd);
	spin_unlock_irqrestore(&host->reg_lock, flags);
}

static unsigned int msdc_wait_cmd_intr(struct msdc_host *host,
	bool check_busy, unsigned long timeout_ms)
{
	void __iomem *base = host->base;
	unsigned int ret = 0;
	unsigned long tmo, flags;
	int lock;

	/* some case lock is not held */
	lock = spin_is_locked(&host->lock);

	if (lock)
		spin_unlock(&host->lock);

	/* wait for msdc_irq */
	tmo = wait_for_completion_timeout(&host->cmd_done,
		msecs_to_jiffies(timeout_ms));

	if (lock)
		spin_lock(&host->lock);

	spin_lock_irqsave(&host->reg_lock, flags);
	MSDC_CLR_BIT32(MSDC_INTEN, wints_cmd);
	spin_unlock_irqrestore(&host->reg_lock, flags);

	host->use_cmd_intr = false; /* clear flag */

	if (check_busy)
		MSDC_CLR_BIT32(MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);

	if (!tmo) {
		host->sw_timeout++;
		ret = -1;
	}

	return ret;
}

static unsigned int msdc_command_start(struct msdc_host   *host,
	struct mmc_command *cmd,
	unsigned long       timeout)
{
	void __iomem *base = host->base;
	u32 opcode = cmd->opcode;
	u32 rawcmd;
	u32 resp;
	unsigned long tmo;
	struct mmc_command *sbc = NULL;
	char *str;
	unsigned long flags;
	bool use_cmd_intr = false;

	if (host->data && host->data->mrq && host->data->mrq->sbc)
		sbc = host->data->mrq->sbc;

	/* Protocol layer does not provide response type, but our hardware needs
	 * to know exact type, not just size!
	 */
	switch (opcode) {
	case MMC_SEND_OP_COND:
	case SD_APP_OP_COND:
		resp = RESP_R3;
		break;
	case MMC_SET_RELATIVE_ADDR:
	/*case SD_SEND_RELATIVE_ADDR:*/
		/* Since SD_SEND_RELATIVE_ADDR=MMC_SET_RELATIVE_ADDR=3,
		 * only one is allowed in switch case.
		 */
		resp = (mmc_cmd_type(cmd) == MMC_CMD_BCR) ? RESP_R6 : RESP_R1;
		break;
	case MMC_FAST_IO:
		resp = RESP_R4;
		break;
	case MMC_GO_IRQ_STATE:
		resp = RESP_R5;
		break;
	case MMC_SELECT_CARD:
		resp = (cmd->arg != 0) ? RESP_R1 : RESP_NONE;
		host->app_cmd_arg = cmd->arg;
		N_MSG(PWR, "msdc%d select card<0x%.8x>", host->id, cmd->arg);
		break;
	case SD_IO_RW_DIRECT:
	case SD_IO_RW_EXTENDED:
		/* SDIO workaround. */
		resp = RESP_R1;
		break;
	case SD_SEND_IF_COND:
		resp = RESP_R1;
		break;
	/* Ignore crc errors when sending status cmd to poll for busy
	 * MMC_RSP_CRC will be set, then mmc_resp_type will return
	 * MMC_RSP_NONE. CMD13 will not receive resp
	 */
	case MMC_SEND_STATUS:
		resp = RESP_R1;
		break;
	default:
		switch (mmc_resp_type(cmd)) {
		case MMC_RSP_R1:
			resp = RESP_R1;
			break;
		case MMC_RSP_R1B:
			resp = RESP_R1B;
			/* use interrupt way for r1b */
			use_cmd_intr = true;
			break;
		case MMC_RSP_R2:
			resp = RESP_R2;
			break;
		case MMC_RSP_R3:
			resp = RESP_R3;
			break;
		case MMC_RSP_NONE:
		default:
			resp = RESP_NONE;
			break;
		}
	}

	cmd->error = 0;
	/* rawcmd :
	 * vol_swt << 30 | auto_cmd << 28 | blklen << 16 | go_irq << 15 |
	 * stop << 14 | rw << 13 | dtype << 11 | rsptyp << 7 | brk << 6 |
	 * opcode
	 */

	rawcmd = opcode | msdc_rsp[resp] << 7 | host->blksz << 16;

	switch (opcode) {
	case MMC_READ_MULTIPLE_BLOCK:
	case MMC_WRITE_MULTIPLE_BLOCK:
		rawcmd |= (2 << 11);
		if (opcode == MMC_WRITE_MULTIPLE_BLOCK)
			rawcmd |= (1 << 13);
		if (host->autocmd & MSDC_AUTOCMD12) {
			rawcmd |= (1 << 28);
			N_MSG(CMD, "AUTOCMD12 is set, addr<0x%x>", cmd->arg);
#ifdef MTK_MSDC_USE_CMD23
		} else if ((host->autocmd & MSDC_AUTOCMD23)) {
			unsigned int reg_blk_num;

			rawcmd |= (1 << 29);
			if (sbc) {
				/* if block number is greater than 0xFFFF,
				 * CMD23 arg will fail to set it.
				 */
				reg_blk_num = MSDC_READ32(SDC_BLK_NUM);
				if (reg_blk_num != (sbc->arg & 0xFFFF))
					pr_notice("msdc%d: acmd23 arg(0x%x) fail to match block num(0x%x), SDC_BLK_NUM(0x%x)\n",
						host->id, sbc->arg,
						cmd->data->blocks, reg_blk_num);
				else
					MSDC_WRITE32(SDC_BLK_NUM, sbc->arg);
				N_MSG(CMD, "AUTOCMD23 addr<0x%x>, arg<0x%x> ",
					cmd->arg, sbc->arg);
			}
#endif /* end of MTK_MSDC_USE_CMD23 */
		}
		break;

	case MMC_READ_SINGLE_BLOCK:
	case MMC_SEND_TUNING_BLOCK:
	case MMC_SEND_TUNING_BLOCK_HS200:
		rawcmd |= (1 << 11);
		break;
	case MMC_WRITE_BLOCK:
		rawcmd |= ((1 << 11) | (1 << 13));
		break;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	case MMC_EXECUTE_READ_TASK:
		rawcmd |= (2 << 11);
		break;
	case MMC_EXECUTE_WRITE_TASK:
		rawcmd |= ((2 << 11) | (1 << 13));
		break;
	case MMC_CMDQ_TASK_MGMT:
		break;
#endif
	case MMC_GEN_CMD:
		if (cmd->data && cmd->data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (cmd->data && cmd->data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);
		break;
	case SD_IO_RW_EXTENDED:
		if (cmd->data && cmd->data->flags & MMC_DATA_WRITE)
			rawcmd |= (1 << 13);
		if (cmd->data && cmd->data->blocks > 1)
			rawcmd |= (2 << 11);
		else
			rawcmd |= (1 << 11);

		if (cmd->data && cmd->data->flags & MMC_DATA_READ) {
			if ((cmd->data->blocks * host->blksz) > 256)
				MSDC_SET_FIELD(EMMC50_CFG0,
					MSDC_EMMC50_CFG_ENDBIT_CNT,
						(host->blksz + 17));
			else
				MSDC_SET_FIELD(EMMC50_CFG0,
					MSDC_EMMC50_CFG_ENDBIT_CNT, 273);
		} else {
			MSDC_SET_FIELD(EMMC50_CFG0,
					MSDC_EMMC50_CFG_ENDBIT_CNT, 273);
		}
		break;
	case SD_IO_RW_DIRECT:
		if (cmd->flags == (unsigned int)-1)
			rawcmd |= (1 << 14);
		break;
	case SD_SWITCH_VOLTAGE:
		rawcmd |= (1 << 30);
		break;
	case SD_APP_SEND_SCR:
	case SD_APP_SEND_NUM_WR_BLKS:
	case MMC_SEND_WRITE_PROT:
	/*case MMC_SEND_WRITE_PROT_TYPE:*/
	case 31:
		rawcmd |= (1 << 11);
		break;
	case SD_SWITCH:
	case SD_APP_SD_STATUS:
	case MMC_SEND_EXT_CSD:
		if (mmc_cmd_type(cmd) == MMC_CMD_ADTC)
			rawcmd |= (1 << 11);
		break;
	case MMC_STOP_TRANSMISSION:
		rawcmd |= (1 << 14);
		rawcmd &= ~(0x0FFF << 16);
		break;
	}

	N_MSG(CMD, "CMD<%d><0x%.8x> Arg<0x%.8x>", opcode, rawcmd, cmd->arg);

	tmo = jiffies + timeout;

	if (opcode == MMC_SEND_STATUS) {
		while (sdc_is_cmd_busy()) {
			if (time_after(jiffies, tmo)) {
				str = "cmd_busy";
				goto err;
			}
		}
	} else {
		while (sdc_is_busy()) {
			if (time_after(jiffies, tmo)) {
				str = "sdc_busy";
				goto err;
			}
		}
	}

	host->cmd = cmd;
	host->cmd_rsp = resp;

	if (use_cmd_intr) {
		host->busy_timeout_ms = cmd->busy_timeout == 0 ?
			host->max_busy_timeout_ms : cmd->busy_timeout;

		/* use interrupt way */
		msdc_set_cmd_intr(host, true, host->busy_timeout_ms);
	} else {
		/* use polling way */
		spin_lock_irqsave(&host->reg_lock, flags);
		MSDC_CLR_BIT32(MSDC_INTEN, (wints_cmd | wints_acmd));
		spin_unlock_irqrestore(&host->reg_lock, flags);
	}

	dbg_add_host_log(host->mmc, 0, cmd->opcode, cmd->arg);
	dbg_add_sd_log(host->mmc, 0, cmd->opcode, cmd->arg);

	sdc_send_cmd(rawcmd, cmd->arg);

	return 0;

err:
	ERR_MSG("XXX %s timeout: before CMD<%d>", str, opcode);
	cmd->error = (unsigned int)-ETIMEDOUT;
	msdc_dump_info(NULL, 0, NULL, host->id);
	msdc_reset_hw(host->id);

	return cmd->error;
}

static u32 msdc_command_resp_polling(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long       timeout)
{
	void __iomem *base = host->base;
	u32 intsts;
	u32 resp, tmo_type;
	unsigned long tmo;
	bool use_cmd_intr;

	u32 cmdsts = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
#ifdef MTK_MSDC_USE_CMD23
	struct mmc_command *sbc = NULL;

	if (host->autocmd & MSDC_AUTOCMD23) {
		if (host->data && host->data->mrq && host->data->mrq->sbc)
			sbc = host->data->mrq->sbc;

		/* autocmd interrupt disabled, used polling way */
		cmdsts |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO;
	}
#endif

	resp = host->cmd_rsp;
	use_cmd_intr = host->use_cmd_intr;

	/* interrupt */
	if (use_cmd_intr) {
		/* Add 3 more sec to prevent race condition */
		if (msdc_wait_cmd_intr(host, true,
			host->busy_timeout_ms + 3000)) {
			pr_notice("[%s]: msdc%d CMD<%d> r1b %dms timeout ARG<0x%.8x>\n",
				__func__, host->id, cmd->opcode,
				host->busy_timeout_ms,
				cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(NULL, 0, NULL, host->id);
			msdc_reset_hw(host->id);
			goto out;
		}

		intsts = host->intsts;
		goto skip_cmd_resp_polling;
	}

	/* polling */
	tmo = jiffies + timeout;
	while (1) {
		intsts = MSDC_READ32(MSDC_INT);
		if ((intsts & cmdsts) != 0) {
			/* clear all int flag */
#ifdef MTK_MSDC_USE_CMD23
			/* need clear autocmd23 command ready interrupt */
			intsts &= (cmdsts | MSDC_INT_ACMDRDY);
#else
			intsts &= cmdsts;
#endif
			MSDC_WRITE32(MSDC_INT, intsts);
			break;
		}

		if (time_after(jiffies, tmo)
			&& ((MSDC_READ32(MSDC_INT) & cmdsts) == 0)) {
			pr_notice("[%s]: msdc%d CMD<%d> polling_for_completion timeout ARG<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(NULL, 0, NULL, host->id);
			msdc_reset_hw(host->id);
			goto out;
		}
	}

skip_cmd_resp_polling:
#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug1(host, cmd, sbc, &intsts);
#endif

	/* command interrupts */
	if  (!(intsts & cmdsts))
		goto out;

#ifdef MTK_MSDC_USE_CMD23
	if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_ACMD19_DONE)) {
#else
	if (intsts & (MSDC_INT_CMDRDY | MSDC_INT_ACMD19_DONE
		| MSDC_INT_ACMDRDY)) {
#endif
		u32 *rsp = NULL;

		rsp = &cmd->resp[0];
		switch (host->cmd_rsp) {
		case RESP_NONE:
			break;
		case RESP_R2:
			*rsp++ = MSDC_READ32(SDC_RESP3);
			*rsp++ = MSDC_READ32(SDC_RESP2);
			*rsp++ = MSDC_READ32(SDC_RESP1);
			*rsp++ = MSDC_READ32(SDC_RESP0);
			break;
		default: /* Response types 1, 3, 4, 5, 6, 7(1b) */
			*rsp = MSDC_READ32(SDC_RESP0);
			if ((cmd->opcode == 13) || (cmd->opcode == 25)) {
				/* Only print msg on this error */
				if (*rsp & R1_WP_VIOLATION) {
					if (cmd->opcode == 25)
						pr_notice(
"[%s]: msdc%d XXX CMD<%d> resp<0x%.8x>, write protection violation addr:0x%x\n",
							__func__, host->id,
							cmd->opcode, *rsp,
							cmd->arg);
					else
						pr_notice(
"[%s]: msdc%d XXX CMD<%d> resp<0x%.8x>, write protection violation\n",
							__func__, host->id,
							cmd->opcode, *rsp);
				}

				if ((*rsp & R1_OUT_OF_RANGE)
				 && (host->hw->host_function != MSDC_SDIO)) {
					pr_notice("[%s]: msdc%d XXX CMD<%d> arg<0x%.8x> resp<0x%.8x>, out of range\n",
						__func__, host->id,
						cmd->opcode, cmd->arg, *rsp);
				}
			}
#ifdef SDCARD_ESD_RECOVERY
			host->cmd13_timeout_cont = 0;
#endif
			break;
		}
		dbg_add_host_log(host->mmc, 1, cmd->opcode, cmd->resp[0]);
		dbg_add_sd_log(host->mmc, 1, cmd->opcode, cmd->resp[0]);
	} else if (intsts & MSDC_INT_RSPCRCERR) {
		cmd->error = (unsigned int)-EILSEQ;
		if ((cmd->opcode != 19) && (cmd->opcode != 21)) {
			pr_notice("[%s]: msdc%d CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
			if (host->hw->host_function == MSDC_SDIO)
				msdc_dump_info(NULL, 0, NULL, host->id);
		}
		if (((mmc_resp_type(cmd) == MMC_RSP_R1B) || (cmd->opcode == 13))
			&& (host->hw->host_function != MSDC_SDIO)) {
			pr_notice("[%s]: msdc%d CMD<%d> ARG<0x%.8X> is R1B, CRC not reset hw\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		} else {
			msdc_reset_hw(host->id);
		}
	} else if (intsts & MSDC_INT_CMDTMO) {
		cmd->error = (unsigned int)-ETIMEDOUT;
		if ((cmd->opcode != 19) && (cmd->opcode != 21))
			pr_notice("[%s]: msdc%d CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		if ((cmd->opcode == 52) && (cmd->arg != 0x00000c00)
			&& (cmd->arg != 0x80000c08)) {
			msdc_dump_info(NULL, 0, NULL, host->id);
			/* Set clock to 50MHz */
			if (host->hw->flags & MSDC_SDIO_DDR208) {
				msdc_clk_stable(host, 3, 1, 0);
				pr_info(
				"%s: SDIO set freq to 50MHz MSDC_CFG:0x%x\n",
					__func__, MSDC_READ32(MSDC_CFG));
			}
		}

		if (use_cmd_intr && (mmc_resp_type(cmd) == MMC_RSP_R1B)) {
			/* tmo_type, 0x1:dat0, 0x2:resp */
			MSDC_GET_FIELD(SDC_STS, SDC_STS_CMD_TMO_TYPE, tmo_type);
			pr_notice("[%s]: msdc%d CMD<%d> Arg<0x%.8x> tmo: %dms (max %dms) type: %s\n",
				__func__, host->id, cmd->opcode, cmd->arg,
			host->busy_timeout_ms,
			host->max_busy_timeout_ms,
			tmo_type == 0x1 ? "dat0" : "resp");
			/* when r1b hw tmo, use cmd13 instead */
			/*
			 * Maybe need fixed in the future to adjust to the
			 * __mmc_switch(), otherwise cmd13 in __mmc_switch()
			 * maybe polling for a long time(even through 10min).
			 */
			if (tmo_type == 0x1)
				/* data0 busy tmo, fall back */
				cmd->error = 0;
			goto out;
		}

		if ((cmd->opcode != 52) && (cmd->opcode != 8)
		 && (cmd->opcode != 55) && (cmd->opcode != 19)
		 && (cmd->opcode != 21) && (cmd->opcode != 1)
		 && (cmd->opcode != 5 ||
		     (host->mmc->card && mmc_card_mmc(host->mmc->card)))
		 && (cmd->opcode != 13 || g_emmc_mode_switch == 0)) {
			msdc_dump_info(NULL, 0, NULL, host->id);
			/* Set clock to 50MHz */
			if (host->hw->flags & MSDC_SDIO_DDR208) {
				msdc_clk_stable(host, 3, 1, 0);
				pr_info(
				"%s: SDIO set freq to 50MHz MSDC_CFG:0x%x\n",
					__func__, MSDC_READ32(MSDC_CFG));
			}
		}
		if (cmd->opcode == MMC_STOP_TRANSMISSION) {
			cmd->error = 0;
			pr_notice("msdc%d: send stop TMO, device status: %x\n",
				host->id, host->device_status);
		}
#ifdef SDCARD_ESD_RECOVERY
		if (cmd->opcode == 13) {
			host->cmd13_timeout_cont++;
			pr_notice("%s: %d: CMD%d cmd13_timeout_cont = %d\n",
				__func__, __LINE__,
				cmd->opcode, host->cmd13_timeout_cont);
		}
#endif
		if ((mmc_resp_type(cmd) == MMC_RSP_R1B) ||
	((cmd->opcode == 13) && (mmc_cmd_type(cmd) != MMC_CMD_ADTC))) {
			pr_notice("[%s]: msdc%d XXX CMD<%d> ARG<0x%.8X> is R1B, TMO not reset hw\n",
				__func__, host->id, cmd->opcode, cmd->arg);
		} else {
			msdc_reset_hw(host->id);
		}
		if ((cmd->opcode == 11) && (host->hw->host_function == MSDC_SD))
			msdc_detect_bad_sd(host, 0);
	}
#ifdef MTK_MSDC_USE_CMD23
	if ((sbc != NULL) && (host->autocmd & MSDC_AUTOCMD23)) {
		if (intsts & MSDC_INT_ACMDRDY) {
			/* do nothing */
		} else if (intsts & MSDC_INT_ACMDCRCERR) {
			pr_notice("[%s]: msdc%d, autocmd23 crc error\n",
				__func__, host->id);
			sbc->error = (unsigned int)-EILSEQ;
			/* record the error info in current cmd struct */
			cmd->error = (unsigned int)-EILSEQ;
			/* host->error |= REQ_CMD23_EIO; */
			msdc_reset_hw(host->id);
		} else if (intsts & MSDC_INT_ACMDTMO) {
			pr_notice("[%s]: msdc%d, autocmd23 tmo error\n",
				__func__, host->id);
			sbc->error = (unsigned int)-ETIMEDOUT;
			/* record the error info in current cmd struct */
			cmd->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(NULL, 0, NULL, host->id);
			/* host->error |= REQ_CMD23_TMO; */
			msdc_reset_hw(host->id);
		}
	}
#endif /* end of MTK_MSDC_USE_CMD23 */

out:
	host->cmd = NULL;

	if (!cmd->data && !cmd->error)
		host->prev_cmd_cause_dump = 0;

	return cmd->error;
}

unsigned int msdc_do_command(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long       timeout)
{
	MVG_EMMC_DECLARE_INT32(delay_ns);
	MVG_EMMC_DECLARE_INT32(delay_us);
	MVG_EMMC_DECLARE_INT32(delay_ms);

	/* FIX ME: check if the next 4 lines can be removed */
	if ((cmd->opcode == MMC_GO_IDLE_STATE) &&
	    (host->hw->host_function == MSDC_SD)) {
		mdelay(10);
	}

	MVG_EMMC_ERASE_MATCH(host, (u64)cmd->arg, delay_ms, delay_us,
		delay_ns, cmd->opcode);

	if (msdc_command_start(host, cmd, timeout))
		goto end;

	MVG_EMMC_ERASE_RESET(delay_ms, delay_us, cmd->opcode);

	if (msdc_command_resp_polling(host, cmd, timeout))
		goto end;

end:
	if (cmd->opcode == MMC_SEND_STATUS && cmd->error == 0)
		host->device_status = cmd->resp[0];

	N_MSG(CMD, "        return<%d> resp<0x%.8x>", cmd->error, cmd->resp[0]);

	return cmd->error;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
#define RESP_TO_INTR_TMO_NS (10 * 1000)
static unsigned int msdc_cmdq_command_start(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long timeout)
{
	void __iomem *base = host->base;
	unsigned long tmo;
	u32 wints_cq_cmd = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR
		| MSDC_INT_CMDTMO;
	unsigned long flags;

	switch (cmd->opcode) {
	case MMC_QUE_TASK_PARAMS:
	case MMC_QUE_TASK_ADDR:
	case MMC_SEND_STATUS:
		break;
	default:
		pr_notice("[%s]: ERROR, only CMD44/CMD45/CMD13 can issue\n",
			__func__);
		break;
	}

	cmd->error = 0;

	N_MSG(CMD, "CMD<%d>        Arg<0x%.8x>", cmd->opcode, cmd->arg);

	tmo = jiffies + timeout;

	spin_unlock(&host->lock);
	while (sdc_is_cmd_busy()) {
		if (time_after(jiffies, tmo) && sdc_is_cmd_busy()) {
			ERR_MSG("[%s]: XXX cmd_busy timeout: before CMD<%d>",
				__func__, cmd->opcode);
			spin_lock(&host->lock);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(NULL, 0, NULL, host->id);
			return cmd->error;
		}
	}
	spin_lock(&host->lock);

	host->cmd = cmd;
	host->cmd_rsp = RESP_R1;

	/* use polling way */
	spin_lock_irqsave(&host->reg_lock, flags);
	MSDC_CLR_BIT32(MSDC_INTEN, wints_cq_cmd);
	spin_unlock_irqrestore(&host->reg_lock, flags);

	dbg_add_host_log(host->mmc, 0, cmd->opcode, cmd->arg);
	sdc_send_cmdq_cmd(cmd->opcode, cmd->arg);

	return 0;
}

static unsigned int msdc_cmdq_command_resp_polling(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long timeout)
{
	void __iomem *base = host->base;
	u32 intsts, resp;
	unsigned long tmo;
	u32 cmdsts = MSDC_INT_CMDRDY | MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO;
	u64 rsp_time, now;

	resp = host->cmd_rsp;

cmdq_resp_intr:
	/* interrupt */
	if (host->use_cmd_intr) {
		if (msdc_wait_cmd_intr(host, true, 3000/* 3s */)) {
			pr_notice("[%s]: msdc%d CMD<%d> %dms timeout ARG<0x%.8x>\n",
				__func__, host->id, cmd->opcode,
				host->busy_timeout_ms,
				cmd->arg);
			cmd->error = (unsigned int)-ETIMEDOUT;
			msdc_dump_info(NULL, 0, NULL, host->id);
			goto out;
		}

		intsts = host->intsts;
		goto skip_cmdq_resp_polling;
	}

	/* polling */
	tmo = jiffies + timeout;
	spin_unlock(&host->lock);
	rsp_time = sched_clock();
	while (1) {
		intsts = MSDC_READ32(MSDC_INT);
		if ((intsts & cmdsts) != 0) {
			/* clear all int flag */
			intsts &= cmdsts;
			MSDC_WRITE32(MSDC_INT, intsts);
			break;
		}

		now = sched_clock();
		if (now - rsp_time > RESP_TO_INTR_TMO_NS) {
			spin_lock(&host->lock);
			/* give up CPU */
			msdc_set_cmd_intr(host, false, 0);
			goto cmdq_resp_intr;
		}

		if (time_after(jiffies, tmo)
			&& ((MSDC_READ32(MSDC_INT) & cmdsts) == 0)) {
			pr_notice("[%s]: msdc%d CMD<%d> polling_for_completion timeout ARG<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			spin_lock(&host->lock);
			cmd->error = (unsigned int)-ETIMEDOUT;
			host->sw_timeout++;
			msdc_dump_info(NULL, 0, NULL, host->id);
			goto out;
		}
	}
	spin_lock(&host->lock);

skip_cmdq_resp_polling:
	/* command interrupts */
	if (intsts & cmdsts) {
		if (intsts & MSDC_INT_CMDRDY) {
			u32 *rsp = NULL;

			rsp = &cmd->resp[0];
			switch (host->cmd_rsp) {
			case RESP_NONE:
				break;
			case RESP_R2:
				*rsp++ = MSDC_READ32(SDC_RESP3);
				*rsp++ = MSDC_READ32(SDC_RESP2);
				*rsp++ = MSDC_READ32(SDC_RESP1);
				*rsp++ = MSDC_READ32(SDC_RESP0);
				break;
			default: /* Response types 1, 3, 4, 5, 6, 7(1b) */
				*rsp = MSDC_READ32(SDC_RESP0);
				break;
			}
			dbg_add_host_log(host->mmc, 1, cmd->opcode,
				cmd->resp[0]);
		} else if (intsts & MSDC_INT_RSPCRCERR) {
			cmd->error = (unsigned int)-EILSEQ;
			pr_notice("[%s]: msdc%d XXX CMD<%d> MSDC_INT_RSPCRCERR Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
		} else if (intsts & MSDC_INT_CMDTMO) {
			cmd->error = (unsigned int)-ETIMEDOUT;
			pr_notice("[%s]: msdc%d XXX CMD<%d> MSDC_INT_CMDTMO Arg<0x%.8x>",
				__func__, host->id, cmd->opcode, cmd->arg);
			msdc_dump_info(NULL, 0, NULL, host->id);
		}
	}
out:
	host->cmd = NULL;
	MSDC_SET_FIELD(EMMC51_CFG0, MSDC_EMMC51_CFG_CMDQEN, (0));

	if (!cmd->data && !cmd->error)
		host->prev_cmd_cause_dump = 0;

	return cmd->error;
}

/* do command queue command - CMD44, CMD45, CMD13(QSR)
 * use another register set
 */
unsigned int msdc_do_cmdq_command(struct msdc_host *host,
	struct mmc_command *cmd,
	unsigned long timeout)
{
	if (msdc_cmdq_command_start(host, cmd, timeout))
		goto end;

	if (msdc_cmdq_command_resp_polling(host, cmd, timeout))
		goto end;
end:
	N_MSG(CMD,
	"		return<%d> resp<0x%.8x>", cmd->error, cmd->resp[0]);
	return cmd->error;
}
#endif

/* The abort condition when PIO read/write
 *  tmo:
 */
static int msdc_pio_abort(struct msdc_host *host, struct mmc_data *data,
	unsigned long tmo)
{
	int  ret = 0;
	void __iomem *base = host->base;

	if (atomic_read(&host->abort))
		ret = 1;

	if (time_after(jiffies, tmo)) {
		data->error = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX PIO Data Timeout: CMD<%d>",
			host->mrq->cmd->opcode);
		msdc_dump_info(NULL, 0, NULL, host->id);
		ret = 1;
	}

	if (ret) {
		msdc_reset_hw(host->id);
		ERR_MSG("msdc pio find abort");
	}

	return ret;
}

/*
 *  Need to add a timeout, or WDT timeout, system reboot.
 */
/* pio mode data read/write */
int msdc_pio_read(struct msdc_host *host, struct mmc_data *data)
{
	struct scatterlist *sg = data->sg;
	void __iomem *base = host->base;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | MSDC_INT_XFER_COMPL;
	u32 ints = 0;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	struct page *hmpage = NULL;
	int i = 0, subpage = 0, totalpages = 0;
	int flag = 0;
	ulong *kaddr = host->pio_kaddr;

	/* default 100ms, *2 for better compatibility */
	if (host->xfer_size < 512)
		tmo = jiffies + 1 + host->timeout_ns / (1000000000UL/HZ) * 2;

	WARN_ON(!kaddr);
	/* MSDC_CLR_BIT32(MSDC_INTEN, wints); */
	while (1) {
		if (!get_xfer_done) {
			ints = MSDC_READ32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			MSDC_WRITE32(MSDC_INT, ints);
		}
		if (ints & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR))
			goto error;
		else if (ints & MSDC_INT_XFER_COMPL)
			get_xfer_done = 1;
		if (get_xfer_done && (num == 0) && (left == 0))
			break;
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);
		flag = 0;

		if  ((ptr != NULL) &&
		     !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
			goto check_fifo1;

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP((left + sg->offset), PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if (subpage != 0 || (sg->offset != 0))
			N_MSG(OPS,
"msdc%d: read size or start not align %x, %x, hmpage %lx,sg offset %x\n",
				host->id, subpage, left, (ulong)hmpage,
				sg->offset);

		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:sg_virt %p", ptr);

		if (flag == 0)
			goto check_fifo1;

		/* High memory and more than 1 va address va
		 * and not continuous
		 */
		/* pr_info("msdc0: kmap not continuous %x %x %x\n",
		 * left,kaddr[i],kaddr[i-1]);
		 */
		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if ((subpage != 0) && (i == (totalpages-1)))
				left = subpage;

check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;

			if ((msdc_rxfifocnt() >= MSDC_FIFO_THD) &&
			    (left >= MSDC_FIFO_THD)) {
				count = MSDC_FIFO_THD >> 2;
				do {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
				} while (--count);
				left -= MSDC_FIFO_THD;
			} else if ((left < MSDC_FIFO_THD) &&
				    msdc_rxfifocnt() >= left) {
				while (left > 3) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read32());
#else
					*ptr++ = msdc_fifo_read32();
#endif
					left -= 4;
				}

				u8ptr = (u8 *) ptr;
				while (left) {
#ifdef MTK_MSDC_DUMP_FIFO
					pr_debug("0x%x ", msdc_fifo_read8());
#else
					*u8ptr++ = msdc_fifo_read8();
#endif
					left--;
				}
			} else {
				ints = MSDC_READ32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints&
				    (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR)) {
					MSDC_WRITE32(MSDC_INT, ints);
					goto error;
				}
			}

			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			/* pr_info("read msdc0:unmap %x\n", hmpage); */
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;
		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		/* pr_info("msdc0 read unmap:\n"); */
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "        PIO Read<%d>bytes", size);

	if (data->error) {
		ERR_MSG("read pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);
		if (host->hw->host_function == MSDC_SDIO)
			msdc_dump_info(NULL, 0, NULL, host->id);
	}

	if (!data->error)
		host->prev_cmd_cause_dump = 0;

	return data->error;

error:
	if (ints & MSDC_INT_DATCRCERR) {
		ERR_MSG(
		"[msdc%d] MSDC_INT_DATCRCERR (0x%x), Left DAT: %d bytes\n",
			host->id, ints, left);
		data->error = (unsigned int)-EILSEQ;
	} else if (ints & MSDC_INT_DATTMO) {
		ERR_MSG("[msdc%d] MSDC_INT_DATTMO (0x%x), Left DAT: %d bytes\n",
			host->id, ints, left);
		msdc_dump_info(NULL, 0, NULL, host->id);
		data->error = (unsigned int)-ETIMEDOUT;
	}
	msdc_reset_hw(host->id);

	goto end;
}

/* please make sure won't using PIO when size >= 512
 * which means, memory card block read/write won't using pio
 * then don't need to handle the CMD12 when data error.
 */
int msdc_pio_write(struct msdc_host *host, struct mmc_data *data)
{
	void __iomem *base = host->base;
	struct scatterlist *sg = data->sg;
	u32 num = data->sg_len;
	u32 *ptr;
	u8 *u8ptr;
	u32 left = 0;
	u32 count, size = 0;
	u32 wints = MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | MSDC_INT_XFER_COMPL;
	bool get_xfer_done = 0;
	unsigned long tmo = jiffies + DAT_TIMEOUT;
	u32 ints = 0;
	struct page *hmpage = NULL;
	int i = 0, totalpages = 0;
	int flag, subpage = 0;
	ulong *kaddr = host->pio_kaddr;

	WARN_ON(!kaddr);
	/* MSDC_CLR_BIT32(MSDC_INTEN, wints); */
	while (1) {
		if (!get_xfer_done) {
			ints = MSDC_READ32(MSDC_INT);
			latest_int_status[host->id] = ints;
			ints &= wints;
			MSDC_WRITE32(MSDC_INT, ints);
		}
		if (ints & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR))
			goto error;
		else if (ints & MSDC_INT_XFER_COMPL)
			get_xfer_done = 1;
		if ((get_xfer_done == 1) && (num == 0) && (left == 0))
			break;
		if (msdc_pio_abort(host, data, tmo))
			goto end;
		if ((num == 0) && (left == 0))
			continue;
		left = msdc_sg_len(sg, host->dma_xfer);
		ptr = sg_virt(sg);

		flag = 0;

		/* High memory must kmap, if already mapped,
		 * only add counter
		 */
		if  ((ptr != NULL) &&
		     !(PageHighMem((struct page *)(sg->page_link & ~0x3))))
			goto check_fifo1;

		hmpage = (struct page *)(sg->page_link & ~0x3);
		totalpages = DIV_ROUND_UP(left + sg->offset, PAGE_SIZE);
		subpage = (left + sg->offset) % PAGE_SIZE;

		if ((subpage != 0) || (sg->offset != 0))
			N_MSG(OPS,
"msdc%d: write size or start not align %x, %x, hmpage %lx,sg offset %x\n",
				host->id, subpage, left, (ulong)hmpage,
				sg->offset);

		/* Kmap all need pages, */
		for (i = 0; i < totalpages; i++) {
			kaddr[i] = (ulong) kmap(hmpage + i);
			if ((i > 0) && ((kaddr[i] - kaddr[i - 1]) != PAGE_SIZE))
				flag = 1;
			if (!kaddr[i])
				ERR_MSG("msdc0:kmap failed %lx\n", kaddr[i]);
		}

		ptr = sg_virt(sg);

		if (ptr == NULL)
			ERR_MSG("msdc0:write sg_virt %p\n", ptr);

		if (flag == 0)
			goto check_fifo1;

		/* High memory and more than 1 va address va
		 * may be not continuous
		 */
		/* pr_info(ERR "msdc0:w kmap not continuous %x %x %x\n",
		 * left, kaddr[i], kaddr[i-1]);
		 */
		for (i = 0; i < totalpages; i++) {
			left = PAGE_SIZE;
			ptr = (u32 *) kaddr[i];

			if (i == 0) {
				left = PAGE_SIZE - sg->offset;
				ptr = (u32 *) (kaddr[i] + sg->offset);
			}
			if (subpage != 0 && (i == (totalpages - 1)))
				left = subpage;

check_fifo1:
			if ((flag == 1) && (left == 0))
				continue;
			else if ((flag == 0) && (left == 0))
				goto check_fifo_end;

			if (left >= MSDC_FIFO_SZ && msdc_txfifocnt() == 0) {
				count = MSDC_FIFO_SZ >> 2;
				do {
					msdc_fifo_write32(*ptr);
					ptr++;
				} while (--count);
				left -= MSDC_FIFO_SZ;
			} else if (left < MSDC_FIFO_SZ &&
				   msdc_txfifocnt() == 0) {
				while (left > 3) {
					msdc_fifo_write32(*ptr);
					ptr++;
					left -= 4;
				}
				u8ptr = (u8 *) ptr;
				while (left) {
					msdc_fifo_write8(*u8ptr);
					u8ptr++;
					left--;
				}
			} else {
				ints = MSDC_READ32(MSDC_INT);
				latest_int_status[host->id] = ints;

				if (ints&
				    (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR)) {
					MSDC_WRITE32(MSDC_INT, ints);
					goto error;
				}
			}

			if (msdc_pio_abort(host, data, tmo))
				goto end;

			goto check_fifo1;
		}

check_fifo_end:
		if (hmpage != NULL) {
			for (i = 0; i < totalpages; i++)
				kunmap(hmpage + i);

			hmpage = NULL;

		}
		size += msdc_sg_len(sg, host->dma_xfer);
		sg = sg_next(sg);
		num--;
	}
 end:
	if (hmpage != NULL) {
		for (i = 0; i < totalpages; i++)
			kunmap(hmpage + i);
		pr_info("msdc0 write unmap 0x%x:\n", left);
	}
	data->bytes_xfered += size;
	N_MSG(FIO, "        PIO Write<%d>bytes", size);

	if (data->error) {
		ERR_MSG("write pio data->error<%d> left<%d> size<%d>",
			data->error, left, size);
		if (host->hw->host_function == MSDC_SDIO)
			msdc_dump_info(NULL, 0, NULL, host->id);
	}

	if (!data->error)
		host->prev_cmd_cause_dump = 0;

	return data->error;

error:
	if (ints & MSDC_INT_DATCRCERR) {
		ERR_MSG(
		"[msdc%d] MSDC_INT_DATCRCERR (0x%x), Left DAT: %d bytes\n",
			host->id, ints, left);
		data->error = (unsigned int)-EILSEQ;
	} else if (ints & MSDC_INT_DATTMO) {
		ERR_MSG("[msdc%d] MSDC_INT_DATTMO (0x%x), Left DAT: %d bytes\n",
			host->id, ints, left);
		msdc_dump_info(NULL, 0, NULL, host->id);
		data->error = (unsigned int)-ETIMEDOUT;
	}
	msdc_reset_hw(host->id);

	goto end;
}

#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
#include "mtk_sd_hw_fde.c"
#endif

#if defined(CONFIG_MTK_HW_FDE) && !defined(CONFIG_MTK_HW_FDE_AES) \
	&& !defined(CONFIG_MMC_CRYPTO)
#include "mtk_hw_fde.c"
#endif

static void msdc_dma_start(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO
		| MSDC_INT_DATCRCERR | MSDC_INT_GPDCSERR | MSDC_INT_BDCSERR;
	unsigned long flags;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_set(&host->mmc->is_data_dma, 1);
#endif

	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO
			| MSDC_INT_ACMDRDY;
	MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_START, 1);

	spin_lock_irqsave(&host->reg_lock, flags);
	MSDC_SET_BIT32(MSDC_INTEN, wints);
	spin_unlock_irqrestore(&host->reg_lock, flags);

	N_MSG(DMA, "DMA start");
	/* Schedule delayed work to check if data0 keeps busy */
	if (host->data) {
		host->data_timeout_ms = DATA_TIMEOUT_MS;
		schedule_delayed_work(&host->data_timeout_work,
			msecs_to_jiffies(host->data_timeout_ms));
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, schedule_delayed_work",
			host->data_timeout_ms);
	}

	host->dma_cnt++;
	host->start_dma_time = sched_clock();
	host->stop_dma_time = 0;
	mb(); /* make sure write committed */
}

static void msdc_dma_stop(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 500;
	int count = 1000;
	u32 wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO | MSDC_INT_DATCRCERR;
	unsigned long flags;

	/* Clear DMA data busy timeout */
	if (host->data) {
		cancel_delayed_work(&host->data_timeout_work);
		N_MSG(DMA, "DMA Data Busy Timeout:%u ms, cancel_delayed_work",
			host->data_timeout_ms);
		host->data_timeout_ms = 0; /* clear timeout */
	}

	/* handle autocmd12 error in msdc_irq */
	if (host->autocmd & MSDC_AUTOCMD12)
		wints |= MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO
			| MSDC_INT_ACMDRDY;
	N_MSG(DMA, "DMA status: 0x%.8x", MSDC_READ32(MSDC_DMA_CFG));

	MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	msdc_retry((MSDC_READ32(MSDC_DMA_CFG) & MSDC_DMA_CFG_STS), retry,
		count, host->id);
	if (retry == 0)
		ERR_MSG("DMA stop retry timeout");

	spin_lock_irqsave(&host->reg_lock, flags);
	MSDC_CLR_BIT32(MSDC_INTEN, wints); /* Not just xfer_comp */
	spin_unlock_irqrestore(&host->reg_lock, flags);

	host->stop_dma_time = sched_clock();
	mb(); /* make sure write committed */

	N_MSG(DMA, "DMA stop, latest_INT_status<0x%.8x>",
		latest_int_status[host->id]);
}

/* calc checksum */
static u8 msdc_dma_calcs(u8 *buf, u32 len)
{
	u32 i, sum = 0;

	for (i = 0; i < len; i++)
		sum += buf[i];
	return 0xFF - (u8) sum;
}

/* gpd bd setup + dma registers */
static int msdc_dma_config(struct msdc_host *host, struct msdc_dma *dma)
{
	void __iomem *base = host->base;
	u32 sglen = dma->sglen;
	u32 j, bdlen;
	dma_addr_t dma_address;
	u32 dma_len;
	u8  blkpad, dwpad, chksum;
	struct scatterlist *sg = dma->sg;
	struct gpd_t *gpd;
	struct bd_t *bd, vbd = {0};

	switch (dma->mode) {
	case MSDC_MODE_DMA_BASIC:
		WARN_ON(dma->sglen != 1);
		dma_address = sg_dma_address(sg);
		dma_len = msdc_sg_len(sg, host->dma_xfer);

		N_MSG(DMA, "BASIC DMA len<%x> dma_address<%llx>",
			dma_len, (u64)dma_address);

		MSDC_SET_FIELD(MSDC_DMA_SA_HIGH, MSDC_DMA_SURR_ADDR_HIGH4BIT,
			upper_32_bits(dma_address) & 0xF);
		MSDC_WRITE32(MSDC_DMA_SA, lower_32_bits(dma_address));

		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_LASTBUF, 1);
		MSDC_WRITE32(MSDC_DMA_LEN, dma_len);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
			dma->burstsz);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 0);
		break;
	case MSDC_MODE_DMA_DESC:
		blkpad = (dma->flags & DMA_FLAG_PAD_BLOCK) ? 1 : 0;
		dwpad  = (dma->flags & DMA_FLAG_PAD_DWORD) ? 1 : 0;
		chksum = (dma->flags & DMA_FLAG_EN_CHKSUM) ? 1 : 0;

		WARN_ON(sglen > MAX_BD_PER_GPD);

		gpd = dma->gpd;
		bd  = dma->bd;
		bdlen = sglen;

		gpd->hwo = 1;   /* hw will clear it */
		gpd->bdp = 1;
		gpd->chksum = 0;        /* need to clear first. */
		if (chksum)
			gpd->chksum = msdc_dma_calcs((u8 *) gpd, 16);

		for (j = 0; j < bdlen; j++) {
#ifdef MSDC_DMA_VIOLATION_DEBUG
			if (g_dma_debug[host->id] &&
			    (msdc_latest_op[host->id] == OPER_TYPE_READ)) {
				pr_debug("[%s] msdc%d read to 0x10000\n",
					__func__, host->id);
				dma_address = 0x10000;
			} else
#endif
				dma_address = sg_dma_address(sg);

			dma_len = msdc_sg_len(sg, host->dma_xfer);

			N_MSG(DMA, "DESC DMA len<%x> dma_address<%llx>",
				dma_len, (u64)dma_address);

			vbd.next = bd[j].next;
			vbd.nexth4 = bd[j].nexth4;

			msdc_init_bd(&vbd, blkpad, dwpad, dma_address,
				dma_len);

			if (j == bdlen - 1)
				vbd.eol = 1;  /* the last bd */
			else
				vbd.eol = 0;

			/* checksume need to clear first */
			vbd.chksum = 0;
			if (chksum)
				vbd.chksum = msdc_dma_calcs((u8 *) (&vbd), 16);

			memcpy(&bd[j], &vbd, sizeof(struct bd_t));

			sg++;
		}
#ifdef MSDC_DMA_VIOLATION_DEBUG
		if (g_dma_debug[host->id] &&
		    (msdc_latest_op[host->id] == OPER_TYPE_READ))
			g_dma_debug[host->id] = 0;
#endif

		dma->used_gpd += 2;
		dma->used_bd += bdlen;

		MSDC_SET_FIELD(MSDC_DMA_CFG, MSDC_DMA_CFG_DECSEN, chksum);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_BRUSTSZ,
			dma->burstsz);
		MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_MODE, 1);

		MSDC_SET_FIELD(MSDC_DMA_SA_HIGH, MSDC_DMA_SURR_ADDR_HIGH4BIT,
			upper_32_bits(dma->gpd_addr) & 0xF);
		MSDC_WRITE32(MSDC_DMA_SA, lower_32_bits(dma->gpd_addr));
		break;

	default:
		break;
	}

	N_MSG(DMA, "DMA_CTRL = 0x%x", MSDC_READ32(MSDC_DMA_CTRL));
	N_MSG(DMA, "DMA_CFG  = 0x%x", MSDC_READ32(MSDC_DMA_CFG));
	N_MSG(DMA, "DMA_SA_HIGH   = 0x%x", MSDC_READ32(MSDC_DMA_SA_HIGH));
	N_MSG(DMA, "DMA_SA   = 0x%x", MSDC_READ32(MSDC_DMA_SA));

	return 0;
}

static void msdc_dma_setup(struct msdc_host *host, struct msdc_dma *dma,
	struct scatterlist *sg, unsigned int sglen)
{
	WARN_ON(sglen > MAX_BD_NUM);     /* not support currently */

	dma->sg = sg;
	dma->flags = DMA_FLAG_EN_CHKSUM;
	/* dma->flags = DMA_FLAG_NONE; */ /* CHECKME */
	dma->sglen = sglen;
	dma->xfersz = host->xfer_size;
	dma->burstsz = MSDC_BRUST_64B;

	if (sglen == 1)
		dma->mode = MSDC_MODE_DMA_BASIC;
	else
		dma->mode = MSDC_MODE_DMA_DESC;

	N_MSG(DMA, "DMA mode<%d> sglen<%d> xfersz<%d>", dma->mode, dma->sglen,
		dma->xfersz);

	msdc_dma_config(host, dma);
}

static void msdc_dma_clear(struct msdc_host *host)
{
	void __iomem *base = host->base;

	host->data = NULL;
	host->mrq = NULL;
	host->dma_xfer = 0;
	msdc_dma_off();
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
	host->blksz = 0;
}

static void msdc_log_data_cmd(struct msdc_host *host, struct mmc_command *cmd,
	struct mmc_data *data)
{
	N_MSG(OPS,
	"CMD<%d> ARG<0x%8x> data<%s %s> blksz<%d> block<%d> error<%d>",
		cmd->opcode, cmd->arg, (host->dma_xfer ? "dma" : "pio"),
		((data->flags & MMC_DATA_READ) ? "read " : "write"),
		data->blksz, data->blocks, data->error);

	if (!(is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))) {
		if (!check_mmc_cmd2425(cmd->opcode) &&
		    !check_mmc_cmd1718(cmd->opcode)) {
			N_MSG(NRW, "CMD<%3d> Resp<0x%8x> size<%d>",
				cmd->opcode, cmd->resp[0],
				data->blksz * data->blocks);
		} else if (cmd->opcode != 13) { /* by pass CMD13 */
			N_MSG(NRW, "CMD<%3d> Resp<%8x %8x %8x %8x>",
				cmd->opcode, cmd->resp[0],
				cmd->resp[1], cmd->resp[2], cmd->resp[3]);
		} else {
			N_MSG(RW, "CMD<%3d> Resp<0x%8x> block<%d>",
				cmd->opcode, cmd->resp[0], data->blocks);
		}
	}
}

#define NON_ASYNC_REQ           0
#define ASYNC_REQ               1

int msdc_if_send_stop(struct msdc_host *host,
	struct mmc_request *mrq, int req_type)
{

	if (!check_mmc_cmd1825(mrq->cmd->opcode))
		return 0;

	if (!mrq->cmd->data || !mrq->cmd->data->stop)
		return 0;

#ifdef MTK_MSDC_USE_CMD23
	if ((host->hw->host_function == MSDC_EMMC) && (req_type != ASYNC_REQ)) {
		/* multi r/w without cmd23 and autocmd12, need manual cmd12 */
		/* if PIO mode and autocmd23 enable, cmd12 need send,
		 * because autocmd23 is disable under PIO
		 */
		if (((mrq->sbc == NULL) &&
		     !(host->autocmd & MSDC_AUTOCMD12))
		 || (!host->dma_xfer && mrq->sbc &&
		     (host->autocmd & MSDC_AUTOCMD23))) {
			if (msdc_do_command(host, mrq->cmd->data->stop,
				CMD_TIMEOUT) != 0)
				return 1;
		}
	} else
#endif
	{
		if ((mrq->cmd->error != 0)
		 || (mrq->cmd->data->error != 0)
		 || !(host->autocmd & MSDC_AUTOCMD12)) {
			if (msdc_do_command(host, mrq->cmd->data->stop,
				CMD_TIMEOUT) != 0)
				return 1;
		}
	}

	return 0;
}

int msdc_rw_cmd_using_sync_dma(struct mmc_host *mmc, struct mmc_command *cmd,
	struct mmc_data *data, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	int dir;
	unsigned long flags;

	msdc_dma_on();  /* enable DMA mode first!! */

	init_completion(&host->xfer_done);

	if (host->hw->host_function != MSDC_SDIO) {
		if (msdc_command_start(host, cmd, CMD_TIMEOUT) != 0)
			return -1;

		dir = data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE;
		(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);

		if (msdc_command_resp_polling(host, cmd, CMD_TIMEOUT) != 0)
			return -2;

		/* start DMA after response, so that DMA need not be stopped
		 * if response error occurs
		 */
		msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);
		msdc_dma_start(host);
	} else {
		dir = data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE;
		(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);

		/* SDIO must disable interrupt and start dma after send cmd.
		 * If host send read cmd but DMA not start ASAP,
		 * The fifo will full and stop clock till DMA start.
		 * And stop clock when DVFS may cause CRC error.
		 */
		local_irq_save(flags);
		if (msdc_command_start(host, cmd, CMD_TIMEOUT) != 0) {
			local_irq_restore(flags);
			return -1;
		}

		if (msdc_command_resp_polling(host, cmd, CMD_TIMEOUT) != 0) {
			local_irq_restore(flags);
			return -2;
		}

		msdc_dma_start(host);
		local_irq_restore(flags);
	}

	spin_unlock(&host->lock);
	if (!wait_for_completion_timeout(&host->xfer_done, DAT_TIMEOUT)) {
		ERR_MSG("XXX CMD<%d> ARG<0x%x> wait xfer_done<%d> timeout!!",
			cmd->opcode, cmd->arg, data->blocks * data->blksz);

		host->sw_timeout++;

		msdc_dump_info(NULL, 0, NULL, host->id);
		data->error = (unsigned int)-ETIMEDOUT;
		msdc_reset(host->id);
	}
	spin_lock(&host->lock);

	msdc_dma_stop(host);

	if ((mrq->data && mrq->data->error)
	 || (mrq->stop && mrq->stop->error && (host->autocmd & MSDC_AUTOCMD12))
	 || (mrq->sbc && mrq->sbc->error && (host->autocmd & MSDC_AUTOCMD23))) {
		msdc_clr_fifo(host->id);
		msdc_clr_int();
	}

	host->dma_xfer = 0;
	msdc_dma_off();
	host->dma.used_bd = 0;
	host->dma.used_gpd = 0;
	dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_set(&host->mmc->is_data_dma, 0);
#endif

	return 0;
}

int msdc_do_request_prepare(struct msdc_host *host, struct mmc_request *mrq)
{
	void __iomem *base = host->base;

	struct mmc_data *data = mrq->cmd->data;
#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
#endif

#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif

	atomic_set(&host->abort, 0);

#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* FIX ME: modify as runtime check of enabling of cmdq */
	/* check msdc work ok: RX/TX fifocnt must be zero after last request
	 * if find abnormal, try to reset msdc first
	 */
	if (msdc_txfifocnt() || msdc_rxfifocnt()) {
		pr_notice("[SD%d] register abnormal, please check! fifo = 0x%x, content = 0x%x\n",
			host->id, MSDC_READ32(MSDC_FIFOCS), msdc_fifo_read32());
		msdc_reset_hw(host->id);
	}
#endif

	WARN_ON(data->blksz > HOST_MAX_BLKSZ);

	data->error = 0;
	msdc_latest_op[host->id] = (data->flags & MMC_DATA_READ)
		? OPER_TYPE_READ : OPER_TYPE_WRITE;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* if CMDQ CMD13 QSR, host->data may be data of mrq - CMD46,47 */
	if (!check_mmc_cmd13_sqs(mrq->cmd))
		host->data = data;
#else
	host->data = data;
#endif

	if (data->flags & MMC_DATA_READ) {
		if ((host->timeout_ns != data->timeout_ns) ||
		    (host->timeout_clks != data->timeout_clks)) {
			msdc_set_timeout(host, data->timeout_ns,
				data->timeout_clks);
		}
	}

	MSDC_WRITE32(SDC_BLK_NUM, data->blocks);

#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc) {
		host->autocmd &= ~MSDC_AUTOCMD12;

#ifdef MTK_MSDC_USE_CACHE
		if (check_mmc_cache_ctrl(host->mmc->card)
		 && (mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
			l_force_prg = !msdc_can_apply_cache(mrq->cmd->arg,
				data->blocks);
			if (l_force_prg && !(mrq->sbc->arg & (0x1 << 31)))
				mrq->sbc->arg |= (1 << 24);
		}
#endif

		if (0 == (host->autocmd & MSDC_AUTOCMD23)) {
			if (msdc_command_start(host, mrq->sbc,
				CMD_TIMEOUT) != 0)
				return -1;

			/* then wait command done */
			if (msdc_command_resp_polling(host, mrq->sbc,
				CMD_TIMEOUT) != 0) {
				return -2;
			}
		}
	} else {
		/* some sd card may not support cmd23,
		 * some emmc card have problem with cmd23,
		 * so use cmd12 here
		 */
		if (host->hw->host_function != MSDC_SDIO) {
			host->autocmd |= MSDC_AUTOCMD12;
			if (0 != (host->autocmd & MSDC_AUTOCMD23)) {
				host->autocmd &= ~MSDC_AUTOCMD23;
				l_card_no_cmd23 = 1;
			}
		}
		/*
		 * check the current region is RPMB or not
		 * storage "host->autocmd when operating RPMB"
		 * mask 'MSDC_AUTOCMD12' when operating RPMB"
		 */
		if (host->hw->host_function == MSDC_EMMC) {
			if (host->mmc->card
			 && (host->mmc->card->ext_csd.part_config &
			     EXT_CSD_PART_CONFIG_ACC_MASK)
			     == EXT_CSD_PART_CONFIG_ACC_RPMB)
				host->autocmd &= ~MSDC_AUTOCMD12;
		}
	}

	return l_card_no_cmd23;
#else
	if ((host->dma_xfer) && (host->hw->host_function != MSDC_SDIO))
		host->autocmd |= MSDC_AUTOCMD12;

	return 0;
#endif

}

static void msdc_if_set_request_err(struct msdc_host *host,
	struct mmc_request *mrq, int async)
{
	void __iomem *base = host->base;

#ifdef MTK_MSDC_USE_CMD23
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-EILSEQ))
		host->error |= REQ_CMD_EIO;
	if (mrq->sbc && (mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
#ifdef CONFIG_MTK_AEE_FEATURE
		/*
		 * aee_kernel_warning_api(__FILE__, __LINE__,
		 *	DB_OPT_NE_JBT_TRACES|DB_OPT_DISPLAY_HANG_DUMP,
		 *	"\n@eMMC FATAL ERROR@\n", "eMMC fatal error ");
		 */
#endif
		host->error |= REQ_CMD_TMO;
	}
#endif

	if (mrq->cmd->error == (unsigned int)-EILSEQ) {
		if (((mrq->cmd->opcode == MMC_SELECT_CARD) ||
		     (mrq->cmd->opcode == MMC_SLEEP_AWAKE))
		 && ((host->hw->host_function == MSDC_EMMC) ||
		     (host->hw->host_function == MSDC_SD))) {
			mrq->cmd->error = 0x0;
		} else {
			host->error |= REQ_CMD_EIO;
		}
	}

	if (mrq->cmd->error == (unsigned int)-ETIMEDOUT) {
		if (mrq->cmd->opcode == MMC_SLEEP_AWAKE) {
			if (mrq->cmd->arg & 0x8000) {
				pr_notice("Sleep_Awake CMD timeout, MSDC_PS %0x\n",
					MSDC_READ32(MSDC_PS));
				mrq->cmd->error = 0x0;
			} else {
				host->error |= REQ_CMD_TMO;
			}
		} else {
			host->error |= REQ_CMD_TMO;
		}
	}

	if (mrq->data && mrq->data->error) {
		if (mrq->data->flags & MMC_DATA_WRITE)
			host->error |= REQ_CRC_STATUS_ERR;
		else
			host->error |= REQ_DAT_ERR;
		/* FIXME: return cmd error for retry if data CRC error */
		if ((!async) && (host->hw->host_function != MSDC_SDIO))
			mrq->cmd->error = (unsigned int)-EILSEQ;
	}

	if (mrq->stop && (mrq->stop->error == (unsigned int)-EILSEQ))
		host->error |= REQ_STOP_EIO;
	if (mrq->stop && (mrq->stop->error == (unsigned int)-ETIMEDOUT))
		host->error |= REQ_STOP_TMO;
}

int msdc_do_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	void __iomem *base = host->base;
	u32 l_autocmd23_is_set = 0;

#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#ifdef MTK_MSDC_USE_CACHE
	u32 l_bypass_flush = 1;
#endif
#endif
	unsigned int left = 0;
	int ret = 0;
	unsigned long pio_tmo;

	#ifndef SDIO_EARLY_SETTING_RESTORE
	if (is_card_sdio(host) || (host->hw->flags & MSDC_SDIO_IRQ))
		msdc_sdio_restore_after_resume(host);
	#endif

	if (host->hw->flags & MSDC_SDIO_IRQ) {
		if ((u_sdio_irq_counter > 0) && ((u_sdio_irq_counter%800) == 0))
			ERR_MSG(
	"sdio_irq=%d, msdc_irq=%d  SDC_CFG=%x MSDC_INTEN=%x MSDC_INT=%x ",
				u_sdio_irq_counter, u_msdc_irq_counter,
				MSDC_READ32(SDC_CFG), MSDC_READ32(MSDC_INTEN),
				MSDC_READ32(MSDC_INT));
	}

	cmd = mrq->cmd;
	data = mrq->cmd->data;

#ifdef MTK_MSDC_USE_CACHE
	if ((host->hw->host_function == MSDC_EMMC)
	 && check_mmc_cache_flush_cmd(cmd)) {
		if (g_cache_status == CACHE_FLUSHED) {
			N_MSG(CHE, "bypass flush command, g_cache_status=%d",
				g_cache_status);
			l_bypass_flush = 1;
			/*
			 * WARNING: Maybe removed in future when use
			 * MMC_CAP_WAIT_WHILE_BUSY;
			 * Workaround: Must return error when dat0 busy (such
			 * as device already having bug), otherwise cmd13 will
			 * polling for MMC_OPS_TIMEOUT_MS(10 mins) in
			 * __mmc_switch().
			 */
			if (sdc_is_busy()) {
				pr_notice("msdc0: sdc_busy before CMD<%d>",
					cmd->opcode);
				cmd->error = (unsigned int)-ETIMEDOUT;
			}
			goto done;
		} else
			l_bypass_flush = 0;
	}
#endif

	if (!data) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (check_mmc_cmd13_sqs(cmd)) {
			if (msdc_do_cmdq_command(host, cmd, CMD_TIMEOUT) != 0)
				goto done_no_data;
		} else {
#endif
			if (msdc_do_command(host, cmd, CMD_TIMEOUT) != 0)
				goto done_no_data;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		}
#endif

		/* Get emmc_id when send ALL_SEND_CID command */
		if ((host->hw->host_function == MSDC_EMMC) &&
			(cmd->opcode == MMC_ALL_SEND_CID))
			g_emmc_id = UNSTUFF_BITS(cmd->resp, 120, 8);

		goto done_no_data;
	}

	host->xfer_size = data->blocks * data->blksz;
	host->blksz = data->blksz;
	if (drv_mode[host->id] == MODE_PIO) {
		host->dma_xfer = 0;
		msdc_latest_transfer_mode[host->id] = MODE_PIO;
	} else if (drv_mode[host->id] == MODE_DMA) {
		host->dma_xfer = 1;
		msdc_latest_transfer_mode[host->id] = MODE_DMA;
	} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
		host->dma_xfer = (host->xfer_size >= dma_size[host->id])
			? 1 : 0;
		msdc_latest_transfer_mode[host->id] =
			host->dma_xfer ? MODE_DMA : MODE_PIO;
	}
	l_card_no_cmd23 = msdc_do_request_prepare(host, mrq);
	if (l_card_no_cmd23 == -1)
		goto done;
	else if (l_card_no_cmd23 == -2)
		goto stop;

	if (host->dma_xfer) {
		ret = msdc_rw_cmd_using_sync_dma(mmc, cmd, data, mrq);
		if (ret == -1)
			goto done;
		else if (ret == -2)
			goto stop;

	} else {
		if (is_card_sdio(host)) {
			msdc_reset_hw(host->id);
			msdc_dma_off();
			data->error = 0;
		}

		host->autocmd &= ~MSDC_AUTOCMD12;
		if (host->autocmd & MSDC_AUTOCMD23) {
			l_autocmd23_is_set = 1;
			host->autocmd &= ~MSDC_AUTOCMD23;
		}

		if (msdc_do_command(host, cmd, CMD_TIMEOUT) != 0)
			goto stop;

		/* Secondly: pio data phase */
		if (data->flags & MMC_DATA_READ) {
#ifdef MTK_MSDC_DUMP_FIFO
			pr_debug("[%s]: start pio read\n", __func__);
#endif
			if (msdc_pio_read(host, data)) {
				msdc_clk_disable(host);
				msdc_clk_enable_and_stable(host);
				goto stop;      /* need cmd12 */
			}
		} else {
#ifdef MTK_MSDC_DUMP_FIFO
			pr_debug("[%s]: start pio write\n", __func__);
#endif
			if (msdc_pio_write(host, data)) {
				msdc_clk_disable(host);
				msdc_clk_enable_and_stable(host);
				goto stop;
			}

			/* For write case: make sure contents in fifo
			 * flushed to device
			 */

			pio_tmo = jiffies + DAT_TIMEOUT;
			while (1) {
				left = msdc_txfifocnt();
				if (left == 0)
					break;

				if (msdc_pio_abort(host, data, pio_tmo))
					break;
			}
		}
	}

stop:
	/* pio mode had disabled autocmd23, restore it for invoking
	 * msdc_if_send_stop()
	 */
	if (l_autocmd23_is_set == 1)
		host->autocmd |= MSDC_AUTOCMD23;

	if (msdc_if_send_stop(host, mrq, NON_ASYNC_REQ))
		goto done;

done:

#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 *  need enable autocmd23 for next request
	 */
	if (l_card_no_cmd23 == 1) {
		host->autocmd |= MSDC_AUTOCMD23;
		host->autocmd &= ~MSDC_AUTOCMD12;
	}
#endif

	if (data) {
		host->data = NULL;

		/* If eMMC we use is in g_emmc_cache_quirk[] or
		 * MTK_MSDC_USE_CACHE is not set. Driver should return
		 * cache_size = 0 in exd_csd to mmc layer
		 * So, mmc_init_card can disable cache
		 */
		if ((cmd->opcode == MMC_SEND_EXT_CSD) &&
			(host->hw->host_function == MSDC_EMMC)) {
			msdc_cache_onoff(data);
		}

		host->blksz = 0;

		if (sd_debug_zone[host->id])
			msdc_log_data_cmd(host, cmd, data);
	}

done_no_data:

	msdc_if_set_request_err(host, mrq, 0);

	/* mmc_blk_err_check will also do legacy request
	 * So, use '|=' instead '=' if command error occur
	 * to avoid clearing data error flag
	 */
	if (host->error & REQ_CMD_EIO)
		host->need_tune |= TUNE_LEGACY_CMD;
	else if (host->error & REQ_DAT_ERR)
		host->need_tune = TUNE_LEGACY_DATA_READ;
	else if (host->error & REQ_CRC_STATUS_ERR)
		host->need_tune = TUNE_LEGACY_DATA_WRITE;
#ifdef SDCARD_ESD_RECOVERY
	else if (host->error & REQ_CMD_TMO)
		host->need_tune = TUNE_LEGACY_CMD_TMO;
#endif

	if (host->error & (REQ_CMD_EIO | REQ_DAT_ERR | REQ_CRC_STATUS_ERR))
		host->err_cmd = mrq->cmd->opcode;
#ifdef SDIO_ERROR_BYPASS
	if (is_card_sdio(host) && !host->error)
		host->sdio_error = 0;
#endif

#ifdef MTK_MSDC_USE_CACHE
	if (data)
		msdc_update_cache_flush_status(host, mrq, data, l_bypass_flush);
#endif

	return host->error;
}

static int msdc_stop_and_wait_busy(struct msdc_host *host)
{
	void __iomem *base = host->base;
	unsigned long polling_tmo = jiffies + POLLING_BUSY;

	msdc_send_stop(host);
	pr_notice("msdc%d, waiting device is not busy\n", host->id);
	while ((MSDC_READ32(MSDC_PS) & 0x10000) != 0x10000) {
		if (time_after(jiffies, polling_tmo) &&
				((MSDC_READ32(MSDC_PS) & 0x10000) != 0x10000)) {
			pr_notice("msdc%d, device stuck in PRG!\n",
				host->id);
			return -1;
		}
	}

	return 0;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static int msdc_do_discard_task_cq(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	u32 task_id;

	task_id = (mrq->sbc->arg >> 16) & 0x1f;
	memset(&mmc->deq_cmd, 0, sizeof(struct mmc_command));
	mmc->deq_cmd.opcode = MMC_CMDQ_TASK_MGMT;
	mmc->deq_cmd.arg = 2 | (task_id << 16);
	mmc->deq_cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1B | MMC_CMD_AC;
	mmc->deq_cmd.data = NULL;

	msdc_do_command(host, &mmc->deq_cmd, CMD_TIMEOUT);

	pr_info("[%s]: msdc%d, discard task id %d, CMD<%d> arg<0x%08x> rsp<0x%08x>",
		__func__, host->id, task_id, mmc->deq_cmd.opcode,
		mmc->deq_cmd.arg, mmc->deq_cmd.resp[0]);

	return mmc->deq_cmd.error;
}

static int msdc_do_request_cq(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
#ifdef MTK_MSDC_USE_CACHE
	u32 l_force_prg = 0;
#endif

	host->error = 0;
	atomic_set(&host->abort, 0);

	cmd  = mrq->sbc;

	mrq->sbc->error = 0;
	mrq->cmd->error = 0;

#ifdef MTK_MSDC_USE_CACHE
	/* check cache enabled, write direction */
	if (check_mmc_cache_ctrl(host->mmc->card)
	 && !(cmd->arg & (0x1 << 30))) {
		l_force_prg = !msdc_can_apply_cache(mrq->cmd->arg,
			cmd->arg & 0xffff);
		/* check not reliable write */
		if (!(cmd->arg & (0x1 << 31)) && l_force_prg)
			cmd->arg |= (1 << 24);
	}
#endif

	(void)msdc_do_cmdq_command(host, cmd, CMD_CQ_TIMEOUT);

	if (cmd->error == (unsigned int)-EILSEQ)
		host->error |= REQ_CMD_EIO;
	else if (cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;

	cmd  = mrq->cmd;

	(void)msdc_do_cmdq_command(host, cmd, CMD_CQ_TIMEOUT);

	if (cmd->error == (unsigned int)-EILSEQ)
		host->error |= REQ_CMD_EIO;
	else if (cmd->error == (unsigned int)-ETIMEDOUT)
		host->error |= REQ_CMD_TMO;

	return host->error;
}

static int msdc_cq_cmd_wait_xfr_done(struct msdc_host *host)
{
	unsigned long polling_tmo = jiffies + 10 * HZ;
	int lock, ret = 0;
	bool timeout = false;

	/* some case lock is not held */
	lock = spin_is_locked(&host->lock);

	if (lock)
		spin_unlock(&host->lock);

	while (atomic_read(&host->mmc->is_data_dma)) {
		msleep(100);
		if (time_after(jiffies, polling_tmo)) {
			if (!timeout && atomic_read(&host->mmc->is_data_dma)) {
				ERR_MSG("xfer tmo, check data timeout");
			cancel_delayed_work_sync(&host->data_timeout_work);
				host->data_timeout_ms = 0;
				schedule_delayed_work(&host->data_timeout_work,
						host->data_timeout_ms);
				timeout = true;
				/* wait 10 second for completion */
				polling_tmo = jiffies + 10 * HZ;
			} else if (timeout) {
				ERR_MSG("wait data complete tmo");
				ret = -1;
				break;
			}
		}
	}

	if (lock)
		spin_lock(&host->lock);

	return ret;
}

static void msdc_cq_need_stop(struct msdc_host *host)
{
	if (atomic_read(&host->cq_error_need_stop)) {
		(void)msdc_stop_and_wait_busy(host);
		atomic_set(&host->cq_error_need_stop, 0);
	}
}

static int tune_cmdq_cmdrsp(struct mmc_host *mmc,
	struct mmc_request *mrq, int *retry)
{
	struct msdc_host *host = mmc_priv(mmc);
	unsigned long polling_status_tmo;
	u32 err = 0, status = 0;

	/* wait for transfer done */
	pr_notice("msdc%d waiting data transfer done1\n", host->id);
	if (msdc_cq_cmd_wait_xfr_done(host)) {
		pr_notice(
			"msdc%d waiting data transfer done1 TMO\n", host->id);
		return -1;
	}

	/* time for wait device to return to trans state
	 * when needed to send CMD48
	 */
	polling_status_tmo = jiffies + 30 * HZ;

	do {
		err = msdc_get_card_status(mmc, host, &status);
		if (err) {
			ERR_MSG("get card status, err = %d", err);

			if (msdc_execute_tuning(mmc, MMC_SEND_STATUS)) {
				ERR_MSG("failed to updata cmd para");
				return 1;
			}

			continue;
		}

		if (status & (1 << 22)) {
			/* illegal command */
			(*retry)--;
			ERR_MSG("status = %x, illegal command, retry = %d",
				status, *retry);
			if ((mrq->cmd->error || mrq->sbc->error) && *retry)
				return 0;
			else
				return 1;
		} else {
			if (R1_CURRENT_STATE(status) != R1_STATE_TRAN) {
				if (time_after(jiffies, polling_status_tmo))
					ERR_MSG("wait xfer state timeout\n");
				else {
					msdc_cq_need_stop(host);
					err = 1;
					continue;
				}
			}
			ERR_MSG("status = %x, discard task, re-send command",
				status);
			err = msdc_do_discard_task_cq(mmc, mrq);
			if (err == (unsigned int)-EIO)
				continue;
			else
				break;
		}
	} while (err);

	if (msdc_execute_tuning(mmc, MMC_SEND_STATUS)) {
		pr_notice("msdc%d autok failed\n", host->id);
		return 1;
	}

	return 0;
}
#endif

/* FIX ME, consider to move it to msdc_tune.c */
int msdc_error_tuning(struct mmc_host *mmc,  struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;
	int autok_err_type = -1;
	unsigned int tune_smpl = 0;
	u32 status;

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc->card && mmc_card_cmdq(mmc->card)) {
		pr_notice("msdc%d waiting data transfer done3\n", host->id);
		if (msdc_cq_cmd_wait_xfr_done(host)) {
			pr_notice("msdc%d waiting data transfer done3 TMO\n",
				host->id);
			return -1;
		}
	}
#endif

	host->tuning_in_progress = true;

	/* autok_err_type is used for switch edge tune and CMDQ tune */
	if (host->need_tune & (TUNE_ASYNC_CMD | TUNE_LEGACY_CMD))
		autok_err_type = CMD_ERROR;
	else if (host->need_tune
			& (TUNE_ASYNC_DATA_READ | TUNE_LEGACY_DATA_READ))
		autok_err_type = DATA_ERROR;
	else if (host->need_tune
			& (TUNE_ASYNC_DATA_WRITE | TUNE_LEGACY_DATA_WRITE))
		autok_err_type = CRC_STATUS_ERROR;
#ifdef SDCARD_ESD_RECOVERY
	else if (host->need_tune & TUNE_LEGACY_CMD_TMO) {
		host->need_tune = TUNE_NONE;
		if ((host->hw->host_function == MSDC_SD)
			&& (mrq->cmd->opcode == MMC_SEND_STATUS)) {
			if (host->cmd13_timeout_cont >= 3) {
				pr_notice(
					"%s: CMD%d tmo cnt = %d,reset sdcard\n",
					__func__, mrq->cmd->opcode,
					host->cmd13_timeout_cont);
				(void)sdcard_hw_reset(mmc);
			}
		}
		goto end;
	}
#endif
	/* 1. mmc_blk_err_check will send CMD13 to check device status
	 * Don't autok/switch edge here, or it will cause CMD19 send when
	 * device is not in transfer status
	 * 2. mmc_blk_err_check will send CMD12 to stop transition if device is
	 * in RCV and DATA status.
	 * Don't autok/switch edge here, or it will cause switch edge failed
	 */
	if (mrq
	 && (mrq->cmd->opcode == MMC_SEND_STATUS ||
	     mrq->cmd->opcode == MMC_STOP_TRANSMISSION)
	 && host->err_cmd != mrq->cmd->opcode) {
		goto end;
	}

	if (mrq)
		pr_info("%s: host->need_tune : 0x%x CMD<%d>\n", __func__,
			host->need_tune, mrq->cmd->opcode);

	status = host->device_status;
	/* clear device status */
	host->device_status = 0x0;
	pr_info("msdc%d saved device status: %x", host->id, status);

	if (host->hw->host_function == MSDC_SDIO) {
		host->need_tune = TUNE_NONE;
		goto end;
	}

	if (host->hw->host_function == MSDC_SD) {
		if (autok_err_type == CRC_STATUS_ERROR) {
			pr_notice("%s: reset sdcard\n",
				__func__);
			(void)sdcard_hw_reset(mmc);
			goto start_tune;
		}
	}

	/* send stop command if device not in transfer state */
	if (R1_CURRENT_STATE(status) != R1_STATE_TRAN &&
		msdc_stop_and_wait_busy(host))
		goto recovery;

start_tune:

	switch (mmc->ios.timing) {
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_UHS_SDR50:
		pr_notice("msdc%d: SD UHS_SDR104/UHS_SDR50 re-autok %d times\n",
			host->id, ++host->reautok_times);
#ifndef SD_RUNTIME_AUTOK_MERGE
		ret = autok_execute_tuning(host, NULL);
#else
		ret = sd_runtime_autok_merge(host);
#endif
		/* ret = sd_execute_dvfs_autok(host, MMC_SEND_TUNING_BLOCK); */
		break;
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_MMC_HS400:
		if (mmc->ios.clock > 52000000) {
			pr_notice("msdc%d: eMMC re-autok %d times\n",
				host->id, ++host->reautok_times);
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
			/* CQ DAT tune in MMC layer, here tune CMD13 CRC */
			if (mmc->card && mmc_card_cmdq(mmc->card))
				emmc_execute_dvfs_autok(host, MMC_SEND_STATUS);
			else
#endif
			{
				if (host->hw->host_function == MSDC_EMMC)
					emmc_execute_dvfs_autok(host,
						MMC_SEND_TUNING_BLOCK_HS200);
				else if (host->hw->host_function == MSDC_SD)
					sd_execute_dvfs_autok(host,
						MMC_SEND_TUNING_BLOCK_HS200);
			}
			break;
		}
		/* fall through */
		/* Other speed mode will tune smpl */
	default:
		tune_smpl = 1;
		pr_notice("msdc%d: tune smpl %d times timing:%d err: %d\n",
			host->id, ++host->tune_smpl_times,
			mmc->ios.timing, autok_err_type);
		autok_low_speed_switch_edge(host, &mmc->ios, autok_err_type);
		break;
	}

	if (ret) {
		/* FIX ME, consider to use msdc_dump_info() to replace all */
		msdc_dump_clock_sts(NULL, 0, NULL, host);
		msdc_dump_ldo_sts(NULL, 0, NULL, host);
		pr_info("msdc%d latest_INT_status<0x%.8x>\n", host->id,
			latest_int_status[host->id]);
		msdc_dump_register(NULL, 0, NULL, host);
		msdc_dump_dbg_register(NULL, 0, NULL, host);
	}

	/* autok failed three times will try reinit tuning */
	if (host->reautok_times >= 4 || host->tune_smpl_times >= 4) {
recovery:
		pr_notice("msdc%d autok error\n", host->id);
		/* eMMC will change to HS200 and lower frequence */
		if (host->hw->host_function == MSDC_EMMC) {
			msdc_dump_register(NULL, 0, NULL, host);
#ifdef MSDC_SWITCH_MODE_WHEN_ERROR
			ret = emmc_reinit_tuning(mmc);
#endif
		}
		/* SDcard will change speed mode and
		 * power reset sdcard
		 */
		if (host->hw->host_function == MSDC_SD)
			ret = sdcard_reset_tuning(mmc);
	} else if (!tune_smpl) {
		pr_info("msdc%d autok pass\n", host->id);
		host->need_tune |= TUNE_AUTOK_PASS;
	}

end:
	host->tuning_in_progress = false;

	return ret;
}

static void msdc_dump_trans_error(struct msdc_host   *host,
	struct mmc_command *cmd,
	struct mmc_data    *data,
	struct mmc_command *stop,
	struct mmc_command *sbc)
{
	if ((cmd->opcode == 52) && (cmd->arg == 0xc00))
		return;
	if ((cmd->opcode == 52) && (cmd->arg == 0x80000c08))
		return;

	if (host->hw->host_function == MSDC_SD) {
		/* SDcard bypass SDIO init command */
		if (cmd->opcode == 5)
			return;
	} else if (host->hw->host_function == MSDC_EMMC) {
		/* eMMC bypss SDIO/SD init command */
		if ((cmd->opcode == 5) || (cmd->opcode == 55))
			return;
	} else {
		if (cmd->opcode == 8)
			return;
	}

	ERR_MSG("XXX CMD<%d><0x%x> Error<%d> Resp<0x%x>", cmd->opcode, cmd->arg,
		cmd->error, cmd->resp[0]);

	if (data) {
		ERR_MSG("XXX DAT block<%d> Error<%d>", data->blocks,
			data->error);
	}
	if (stop) {
		ERR_MSG("XXX STOP<%d> Error<%d> Resp<0x%x>",
			stop->opcode, stop->error, stop->resp[0]);
	}

	if (sbc) {
		ERR_MSG("XXX SBC<%d><0x%x> Error<%d> Resp<0x%x>",
			sbc->opcode, sbc->arg, sbc->error, sbc->resp[0]);
	}

#ifdef SDIO_ERROR_BYPASS

	if (!data)
		return;

	if (is_card_sdio(host) &&
	    (host->sdio_error != -EILSEQ) &&
	    (cmd->opcode == 53) &&
	    (sg_dma_len(data->sg) > 4)) {
		host->sdio_error = -EILSEQ;
		ERR_MSG("XXX SDIO Error ByPass");
	}
#endif
}

static void msdc_pre_req(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	struct mmc_command *cmd = mrq->cmd;

	data = mrq->data;

	if (!data)
		return;

	data->host_cookie = MSDC_COOKIE_ASYNC;
	if (check_mmc_cmd1718(cmd->opcode) ||
	    check_mmc_cmd2425(cmd->opcode)) {
		host->xfer_size = data->blocks * data->blksz;
		if (drv_mode[host->id] == MODE_PIO) {
			data->host_cookie |= MSDC_COOKIE_PIO;
			msdc_latest_transfer_mode[host->id] = MODE_PIO;
		} else if (drv_mode[host->id] == MODE_DMA) {
			msdc_latest_transfer_mode[host->id] = MODE_DMA;
		} else if (drv_mode[host->id] == MODE_SIZE_DEP) {
			if (host->xfer_size < dma_size[host->id]) {
				data->host_cookie |= MSDC_COOKIE_PIO;
				msdc_latest_transfer_mode[host->id] =
					MODE_PIO;
			} else {
				msdc_latest_transfer_mode[host->id] =
					MODE_DMA;
			}
		}
	}
	if (msdc_use_async_dma(data->host_cookie)) {
		(void)dma_map_sg(mmc_dev(mmc), data->sg, data->sg_len,
			((data->flags & MMC_DATA_READ)
				? DMA_FROM_DEVICE : DMA_TO_DEVICE));
	}
}

static void msdc_post_req(struct mmc_host *mmc, struct mmc_request *mrq,
	int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_data *data;
	int dir = DMA_FROM_DEVICE;

	data = mrq->data;
	if (data && (msdc_use_async_dma(data->host_cookie))) {
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		if (!mmc->card || !mmc_card_cmdq(mmc->card))
#endif
			host->xfer_size = data->blocks * data->blksz;
		dir = data->flags & MMC_DATA_READ ?
			DMA_FROM_DEVICE : DMA_TO_DEVICE;
		dma_unmap_sg(mmc_dev(mmc), data->sg, data->sg_len, dir);
		N_MSG(OPS, "CMD<%d> ARG<0x%x> blksz<%d> block<%d> error<%d>",
			mrq->cmd->opcode, mrq->cmd->arg, data->blksz,
			data->blocks, data->error);
	}

	if (data)
		data->host_cookie = 0;

}

static int msdc_do_request_async(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	void __iomem *base = host->base;
	u32 arg;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int task_id;
#endif
#ifdef MTK_MSDC_USE_CMD23
	u32 l_card_no_cmd23 = 0;
#endif

	MVG_EMMC_DECLARE_INT32(delay_ns);
	MVG_EMMC_DECLARE_INT32(delay_us);
	MVG_EMMC_DECLARE_INT32(delay_ms);

	host->error = 0;

	spin_lock(&host->lock);

	cmd = mrq->cmd;
	data = mrq->cmd->data;

	host->mrq = mrq;

	host->xfer_size = data->blocks * data->blksz;
	host->blksz = data->blksz;
	host->dma_xfer = 1;
	l_card_no_cmd23 = msdc_do_request_prepare(host, mrq);
	if (l_card_no_cmd23 == -1)
		goto done;
	else if (l_card_no_cmd23 == -2)
		goto stop;

	msdc_dma_on();          /* enable DMA mode first!! */

	if (msdc_command_start(host, cmd, CMD_TIMEOUT) != 0)
		goto done;

	if (msdc_command_resp_polling(host, cmd, CMD_TIMEOUT) != 0)
		goto stop;

	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request
	 */
	msdc_dma_setup(host, &host->dma, data->sg, data->sg_len);

	msdc_dma_start(host);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (check_mmc_cmd4647(cmd->opcode)) {
		task_id = (cmd->arg >> 16) & 0x1f;
		arg = mmc->areq_que[task_id]->mrq_que->cmd->arg;
	} else
#endif
		arg = cmd->arg;

	MVG_EMMC_WRITE_MATCH(host, (u64)arg, delay_ms, delay_us, delay_ns,
		cmd->opcode, host->xfer_size);

	if (sd_debug_zone[host->id])
		msdc_log_data_cmd(host, cmd, data);

	spin_unlock(&host->lock);

#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request
	 */
	if (l_card_no_cmd23 == 1) {
		host->autocmd |= MSDC_AUTOCMD23;
		host->autocmd &= ~MSDC_AUTOCMD12;
	}
#endif

#ifdef MTK_MSDC_USE_CACHE
	msdc_update_cache_flush_status(host, mrq, data, 1);
#endif

	return 0;


stop:
	if (msdc_if_send_stop(host, mrq, ASYNC_REQ))
		pr_notice("%s send stop error\n", __func__);
done:
#ifdef MTK_MSDC_USE_CMD23
	/* for msdc use cmd23, but card not supported(sbc is NULL),
	 * need enable autocmd23 for next request
	 */
	if (l_card_no_cmd23 == 1) {
		host->autocmd |= MSDC_AUTOCMD23;
		host->autocmd &= ~MSDC_AUTOCMD12;
	}
#endif

	if (sd_debug_zone[host->id])
		msdc_log_data_cmd(host, cmd, data);

	msdc_dma_clear(host);

	msdc_if_set_request_err(host, mrq, 1);

	/* re-autok or try smpl except TMO */
	if (host->error & REQ_CMD_EIO)
		host->need_tune = TUNE_ASYNC_CMD;

#ifdef MTK_MSDC_USE_CACHE
	msdc_update_cache_flush_status(host, mrq, data, 1);
#endif

	if (mrq->done)
		mrq->done(mrq);

	spin_unlock(&host->lock);

	return host->error;
}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
static int msdc_do_cmdq_request_with_retry(struct msdc_host *host,
	struct mmc_request *mrq)
{
	struct mmc_host *mmc;
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
	int ret = 0, retry;

	mmc = host->mmc;
	cmd = mrq->cmd;
	data = mrq->cmd->data;
	if (data)
		stop = data->stop;

	retry = 5;
	while (msdc_do_request_cq(mmc, mrq)) {
		msdc_dump_trans_error(host, cmd, data, stop, mrq->sbc);
		if ((cmd->error == (unsigned int)-EILSEQ) ||
			(cmd->error == (unsigned int)-ETIMEDOUT) ||
			(mrq->sbc->error == (unsigned int)-EILSEQ) ||
			(mrq->sbc->error == (unsigned int)-ETIMEDOUT)) {
			ret = tune_cmdq_cmdrsp(mmc, mrq, &retry);
			if (ret)
				return ret;
		} else {
			ERR_MSG("CMD44 and CMD45 error - error %d %d",
				mrq->sbc->error, cmd->error);
			break;
		}
	}

	return ret;
}
#endif

/* ops.request */
static void msdc_ops_request_legacy(struct mmc_host *mmc,
	struct mmc_request *mrq)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct mmc_command *cmd;
	struct mmc_data *data;
	struct mmc_command *stop = NULL;
#ifdef MTK_MMC_SDIO_DEBUG
	struct timespec sdio_profile_start;
#endif
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	int ret;
#endif

	host->error = 0;

#ifndef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (host->mrq) {
		ERR_MSG("XXX host->mrq<0x%p> cmd<%d>arg<0x%x>", host->mrq,
			host->mrq->cmd->opcode, host->mrq->cmd->arg);
		WARN_ON(1);
	}
#endif

	/* start to process */
	spin_lock(&host->lock);
	cmd = mrq->cmd;
	data = mrq->cmd->data;
	if (data)
		stop = data->stop;

#ifdef MTK_MMC_SDIO_DEBUG
	if (sdio_pro_enable)
		sdio_get_time(mrq, &sdio_profile_start);
#endif

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (check_mmc_cmd44(mrq->sbc)) {
		ret = msdc_do_cmdq_request_with_retry(host, mrq);
		goto cq_req_done;
	}

	/* only CMD0/12/13 can be send when non-empty queue @ CMDQ on */
	if (mmc->card && mmc_card_cmdq(mmc->card)
		&& atomic_read(&mmc->areq_cnt)
		&& !check_mmc_cmd001213(cmd->opcode)
		&& !check_mmc_cmd48(cmd->opcode)) {
		ERR_MSG("[%s][WARNING] CMDQ on, sending CMD%d\n",
			__func__, cmd->opcode);
	}
	if (!check_mmc_cmd13_sqs(mrq->cmd))
#endif
		host->mrq = mrq;

	if (msdc_do_request(host->mmc, mrq)) {
		msdc_dump_trans_error(host, cmd, data, stop, mrq->sbc);
	} else {
		/* mmc_blk_err_check will do legacy request without data */
		if (host->need_tune & TUNE_LEGACY_CMD)
			host->need_tune &= ~TUNE_LEGACY_CMD;
#ifdef SDCARD_ESD_RECOVERY
		if (host->need_tune & TUNE_LEGACY_CMD_TMO)
			host->need_tune &= ~TUNE_LEGACY_CMD_TMO;
#endif
		/* Retry legacy data read pass, clear autok pass flag */
		if ((host->need_tune & TUNE_LEGACY_DATA_READ) &&
			mrq->cmd->data) {
			host->need_tune &= ~TUNE_LEGACY_DATA_READ;
			host->need_tune &= ~TUNE_AUTOK_PASS;
			host->reautok_times = 0;
			host->tune_smpl_times = 0;
		}
		/* Retry legacy data write pass, clear autok pass flag */
		if ((host->need_tune & TUNE_LEGACY_DATA_WRITE) &&
			mrq->cmd->data) {
			host->need_tune &= ~TUNE_LEGACY_DATA_WRITE;
			host->need_tune &= ~TUNE_AUTOK_PASS;
			host->reautok_times = 0;
			host->tune_smpl_times = 0;
		}
		/* legacy command err, tune pass, clear autok pass flag */
		if (host->need_tune == TUNE_AUTOK_PASS) {
			host->reautok_times = 0;
			host->tune_smpl_times = 0;
			host->need_tune = TUNE_NONE;
		}
		if (host->need_tune == TUNE_NONE)
			host->err_cmd = -1;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
cq_req_done:
#endif

#ifdef MTK_MSDC_USE_CACHE
	msdc_check_cache_flush_error(host, cmd);
#endif

	/* ==== when request done, check if app_cmd ==== */
	if (mrq->cmd->opcode == MMC_APP_CMD) {
		host->app_cmd = 1;
		host->app_cmd_arg = mrq->cmd->arg;      /* save the RCA */
	} else {
		host->app_cmd = 0;
		/* host->app_cmd_arg = 0; */
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	/* if not CMDQ CMD44/45 or CMD13, follow original flow to
	 * clear host->mrq if it's CMD44/45 or CMD13 QSR,
	 */
	if (!(check_mmc_cmd13_sqs(mrq->cmd) || check_mmc_cmd44(mrq->sbc)))
#endif
		host->mrq = NULL;

#ifdef MTK_MMC_SDIO_DEBUG
	if (sdio_pro_enable)
		sdio_calc_time(mrq, &sdio_profile_start);
#endif

	spin_unlock(&host->lock);

	mmc_request_done(mmc, mrq);
}

int msdc_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;

	/* SDIO3.0+ need disable retune, othrewise
	 * mmc may tune SDR104 when device in DDR208 mode
	 * and autok error when mode incorrect.
	 */
	if ((host->hw->host_function == MSDC_SDIO) && (mmc->doing_retune)) {
		pr_info("msdc%d: mmc retune do nothing\n", host->id);
		return 0;
	}

	msdc_init_tune_path(host, mmc->ios.timing);
	autok_msdc_tx_setting(host, &mmc->ios);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	if (mmc->card && mmc_card_cmdq(mmc->card)) {
		if (msdc_cq_cmd_wait_xfr_done(host)) {
			pr_notice("msdc%d waiting data transfer done4 TMO\n",
				host->id);
			return -1;
		}
	}
#endif

	host->tuning_in_progress = true;

	if (host->hw->host_function == MSDC_SD)
		ret = sd_execute_dvfs_autok(host, opcode);
	else if (host->hw->host_function == MSDC_EMMC)
		ret = emmc_execute_dvfs_autok(host, opcode);
	else if (host->hw->host_function == MSDC_SDIO)
		sdio_execute_dvfs_autok(host);

	host->tuning_in_progress = false;
	if (ret)
		msdc_dump_info(NULL, 0, NULL, host->id);

	/* return error to reset emmc when timeout occurs during autok */
	return ret;
}

static void msdc_unreq_vcore(struct work_struct *work)
{
}

static void msdc_ops_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int host_cookie = 0;
	struct msdc_host *host = mmc_priv(mmc);
#ifdef CONFIG_MTK_EMMC_HW_CQ
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	bool cq_host_en = false;
#endif

	if ((host->hw->host_function == MSDC_SDIO) &&
	    !(host->trans_lock.active))
		__pm_stay_awake(&host->trans_lock);

	/* SDIO need need lock dvfs */

	if (mrq->data) {
		host_cookie = mrq->data->host_cookie;
#ifdef CONFIG_MTK_EMMC_HW_CQ
		if (cq_host && cq_host->enabled) {
			pr_notice("WARN: data xf with cqhci enabled\n");
			cq_host_en = true;
			mmc->cmdq_ops->disable(mmc, true);
			WARN_ON(1);
		}
#endif
	}

	if (!(host->tuning_in_progress) && host->need_tune &&
		!(host->need_tune & TUNE_AUTOK_PASS))
		msdc_error_tuning(mmc, mrq);

	if (is_card_present(host)
		&& host->power_mode == MMC_POWER_OFF
		&& mrq->cmd->opcode == 7 && mrq->cmd->arg == 0
		&& host->hw->host_function == MSDC_SD) {
		ERR_MSG(
		"cmd<7> arg<0x0> card<1> power<0>, bypass return -ENOMEDIUM");
	} else if (!is_card_present(host) ||
		host->power_mode == MMC_POWER_OFF) {
		ERR_MSG("cmd<%d> arg<0x%x> card<%d> power<%d>",
			mrq->cmd->opcode, mrq->cmd->arg,
			is_card_present(host), host->power_mode);
		mrq->cmd->error = (unsigned int)-ENOMEDIUM;
		if (mrq->done)
			mrq->done(mrq); /* call done directly. */
		goto end;
	}

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_set(&host->cq_error_need_stop, 0);
#endif

	/* Async only support DMA and asyc CMD flow */
	if (msdc_use_async_dma(host_cookie))
		msdc_do_request_async(mmc, mrq);
	else
		msdc_ops_request_legacy(mmc, mrq);

	/* SDIO need check lock dvfs */

	if ((host->hw->host_function == MSDC_SDIO) &&
	    (host->trans_lock.active))
		__pm_relax(&host->trans_lock);
end:
#ifdef CONFIG_MTK_EMMC_HW_CQ
	if (cq_host_en)
		mmc->cmdq_ops->enable(mmc);
#endif
	return;
}

void msdc_sd_clock_run(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* mclk: 0 -> 260000 */
	msdc_set_mclk(host, MMC_TIMING_LEGACY, 260000);

	MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_CKPDN);
	mdelay(1);
	MSDC_CLR_BIT32(MSDC_CFG, MSDC_CFG_CKPDN);

	/* mclk: 260000 -> 0  */
	msdc_set_mclk(host, MMC_TIMING_LEGACY, 0);
}

/* ops.set_ios */
void msdc_ops_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);

	spin_lock(&host->lock);

	/*
	 * Save timing setting if leaving current timing for restore
	 * using.
	 */
	if ((host->hw->host_function == MSDC_EMMC ||
		(mmc->caps2 & MMC_CAP2_NMCARD))
			&& host->timing != ios->timing
			&& ios->timing == MMC_TIMING_LEGACY)
		msdc_save_timing_setting(host);

	if (host->power_mode != ios->power_mode) {
		switch (ios->power_mode) {
		case MMC_POWER_OFF:
			spin_unlock(&host->lock);
			msdc_set_power_mode(host, ios->power_mode);
			spin_lock(&host->lock);
			break;
		case MMC_POWER_UP:
			spin_unlock(&host->lock);
			msdc_init_hw(host);
			msdc_set_power_mode(host, ios->power_mode);
			spin_lock(&host->lock);
			break;
		case MMC_POWER_ON:
			if (host->hw->host_function == MSDC_SD)
				msdc_sd_clock_run(host);
			break;
		default:
			break;
		}
		host->power_mode = ios->power_mode;
	}

	if (host->bus_width != ios->bus_width) {
		msdc_set_buswidth(host, ios->bus_width);
		host->bus_width = ios->bus_width;
	}

	if (host->timing != ios->timing) {
		/* msdc setting TX parameter */
		msdc_ios_tune_setting(host, ios);

		if (ios->timing == MMC_TIMING_MMC_DDR52) {
			msdc_init_tune_setting(host);
			msdc_set_mclk(host, ios->timing, ios->clock);
		}
		host->timing = ios->timing;

		/* For MSDC design, driving shall actually depend on clock freq
		 * instead of timing mode. However, we may not be able to
		 * determine driving between 100MHz and 200MHz, e.g., 150MHz
		 * or 180MHz Therefore we select to change driving when
		 * timing mode changes.
		 */
		if (host->hw->host_function == MSDC_EMMC) {
			if (host->timing == MMC_TIMING_MMC_HS400) {
				host->hw->driving_applied =
					&host->hw->driving_hs400;
			} else if (host->timing == MMC_TIMING_MMC_HS200) {
				host->hw->driving_applied =
					&host->hw->driving_hs200;
			}
			msdc_set_driving(host, host->hw->driving_applied);
		} else if (host->hw->host_function == MSDC_SD) {
			if (host->timing == MMC_TIMING_UHS_SDR104) {
				host->hw->driving_applied =
					&host->hw->driving_sdr104;
			} else if (host->timing == MMC_TIMING_UHS_SDR50) {
				host->hw->driving_applied =
					&host->hw->driving_sdr50;
			} else if (host->timing == MMC_TIMING_UHS_DDR50) {
				host->hw->driving_applied =
					&host->hw->driving_ddr50;
			} else if (host->timing == MMC_TIMING_MMC_HS200) {
				host->hw->driving_applied =
					&host->hw->driving_hs200;
			}
			msdc_set_driving(host, host->hw->driving_applied);
		}
	}

	if (host->mclk != ios->clock) {
		if ((host->mclk > ios->clock)
		 && (ios->clock <= 52000000)
		 && (ios->clock > 0))
			msdc_init_tune_setting(host);

		msdc_set_mclk(host, ios->timing, ios->clock);

		/*
		 * Only restore tune setting on resumming for saving
		 * time.
		 */
		if ((host->hw->host_function == MSDC_EMMC ||
			(mmc->caps2 & MMC_CAP2_NMCARD))
		&& mmc->card && mmc_card_suspended(mmc->card)
		&& ios->timing != MMC_TIMING_LEGACY) {
			msdc_restore_timing_setting(host);
			pr_notice("[AUTOK]eMMC restored timing setting\n");
		} else if (ios->timing == MMC_TIMING_MMC_HS400) {
			msdc_execute_tuning(host->mmc,
				MMC_SEND_TUNING_BLOCK_HS200);
			mmc_retune_disable(host->mmc);
		}
	}

	spin_unlock(&host->lock);
}

/* ops.get_ro */
static int msdc_ops_get_ro(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	unsigned long flags;
	int ro = 0;

	if (host->hw->flags & MSDC_WP_PIN_EN)
		ro = (MSDC_READ32(MSDC_PS) >> 31);

	return ro;
}

/* ops.get_cd */
static int msdc_ops_get_cd(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	/* unsigned long flags; */
	int level = 1;

	/* spin_lock_irqsave(&host->lock, flags); */

	/* for emmc, MSDC_REMOVABLE not set, always return 1 */
	if (mmc->caps & MMC_CAP_NONREMOVABLE) {
		host->card_inserted = 1;
		goto end;
	} else {
#ifdef CONFIG_GPIOLIB
		level = __gpio_get_value(cd_gpio);
#endif
		host->card_inserted = (host->hw->cd_level == level) ? 1 : 0;
	}

	if (host->block_bad_card)
		host->card_inserted = 0;
 end:
	/* enable msdc register dump */
	sd_register_zone[host->id] = 1;
	INIT_MSG(
	"Card insert<%d> Block bad card<%d>, mrq<%p> claimed<%d> pwrcnt<%d> trigger card event<%d>",
		host->card_inserted,
		host->block_bad_card,
		host->mrq, mmc->claimed,
		host->power_cycle_cnt, mmc->trigger_card_event);

	/* spin_unlock_irqrestore(&host->lock, flags); */

	return host->card_inserted;
}

static void msdc_ops_card_event(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	host->power_cycle_cnt = 0;
	host->block_bad_card = 0;
#ifdef SDCARD_ESD_RECOVERY
	host->cmd13_timeout_cont = 0;
#endif
	host->data_timeout_cont = 0;
	msdc_reset_bad_sd_detecter(host);

	host->is_autok_done = 0;
	msdc_ops_get_cd(mmc);
	/* when detect card, timeout log log is not needed */
	sd_register_zone[host->id] = 0;
}

/* ops.enable_sdio_irq */
static void msdc_ops_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	unsigned long flags;

	if (hw->flags & MSDC_EXT_SDIO_IRQ) {    /* yes for sdio */
		if (enable)
			hw->enable_sdio_eirq(); /* combo_sdio_enable_eirq */
		else
			hw->disable_sdio_eirq(); /* combo_sdio_disable_eirq */
	} else if (hw->flags & MSDC_SDIO_IRQ) {
		spin_lock_irqsave(&host->sdio_irq_lock, flags);

		if (enable) {
			while (1) {
				MSDC_SET_BIT32(MSDC_INTEN, MSDC_INT_SDIOIRQ);
				pr_debug("@#0x%08x @e >%d<\n",
					(MSDC_READ32(MSDC_INTEN)),
					host->mmc->sdio_irq_pending);
				if ((MSDC_READ32(MSDC_INTEN) & MSDC_INT_SDIOIRQ)
					== 0) {
					pr_debug(
				"Should never ever get into this >%d<\n",
						host->mmc->sdio_irq_pending);
				} else {
					break;
				}
			}
		} else {
			MSDC_CLR_BIT32(MSDC_INTEN, MSDC_INT_SDIOIRQ);
			pr_debug("@#0x%08x @d\n", (MSDC_READ32(MSDC_INTEN)));
		}

		spin_unlock_irqrestore(&host->sdio_irq_lock, flags);
	}
}

/* FIXME: This function is only used to switch voltage to 1.8V
 * This function can be used in initialization power setting after msdc driver
 * use mmc layer power control instead of MSDC power control
 */
static int msdc_ops_switch_volt(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	unsigned int status = 0;

	if (host->hw->host_function == MSDC_EMMC)
		return 0;
	if (host->hw->host_function == MSDC_SD && !host->power_switch)
		return 0;

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_330:
		/* do nothing and don't print anything to avoid log much */
		return 0;

	case MMC_SIGNAL_VOLTAGE_180:
		/* switch voltage */
		if (host->power_switch)
			host->power_switch(host, 1);
		/* Clock is gated by HW after CMD11,
		 * Must keep clock gate 5ms before switch voltage
		 */
		usleep_range(10000, 10500);
		/* set as 500T -> 1.25ms for 400KHz or 1.9ms for 260KHz */
		msdc_set_vol_change_wait_count(VOL_CHG_CNT_DEFAULT_VAL);
		/* start to provide clock to device */
		MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_BV18SDT);
		/* Delay 1ms wait HW to finish voltage switch */
		usleep_range(1000, 1500);

		while ((status =
			MSDC_READ32(MSDC_CFG)) & MSDC_CFG_BV18SDT)
			;
		if (status & MSDC_CFG_BV18PSS)
			return 0;

		pr_notice(
		"msdc%d: 1.8V regulator output did not became stable\n",
			host->id);
		return -EAGAIN;
	default:
		return 0;
	}
}

static int msdc_card_busy(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	u32 status;

	if (host->block_bad_card)
		return 0;

	status = MSDC_READ32(MSDC_PS);
	if (((status >> 16) & 0x1) != 0x1) {
		if (host->hw->host_function == MSDC_SDIO)
			pr_info("msdc%d: card is busy!\n", host->id);
		return 1;
	}

	return 0;
}

/* Add this function to check if no interrupt back after dma starts.
 * It may occur when write crc received, but busy over host->data_timeout_ms
 */
static void msdc_check_data_timeout(struct work_struct *work)
{
	struct msdc_host *host =
		container_of(work, struct msdc_host, data_timeout_work.work);
	void __iomem *base = host->base;
	struct mmc_data  *data = host->data;
	struct mmc_request *mrq = host->mrq;
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo;
	u32 intsts;
	u32 wints = MSDC_INT_XFER_COMPL | MSDC_INT_DATTMO
		| MSDC_INT_DATCRCERR | MSDC_INT_GPDCSERR | MSDC_INT_BDCSERR;

	if (!data || !mrq || !mmc)
		return;

	pr_info("[%s]: XXX DMA Data Busy Timeout: %u ms, CMD<%d>",
		__func__, host->data_timeout_ms, mrq->cmd->opcode);

	intsts = MSDC_READ32(MSDC_INT);

	msdc_dump_host_state(NULL, NULL, NULL, host);
	msdc_dump_info(NULL, 0, NULL, host->id);

	/* MSDC have received int, but delay by system. Just print warning */
	if (intsts & wints) {
		pr_info(
		"[%s]: Warning msdc%d ints are delayed by system, ints: %x\n",
			__func__, host->id, intsts);
		return;
	}

	if (msdc_use_async_dma(data->host_cookie)) {
		dbg_add_host_log(host->mmc, 3, 0, 0);
		msdc_dma_stop(host);
		msdc_dma_clear(host);
		msdc_reset_hw(host->id);
		if (host->id == 1) {
			pr_info("msdc1 err, reset sdcard\n");
			(void)sdcard_hw_reset(host->mmc);
		}
		tmo = jiffies + POLLING_BUSY;

		/* check card state, try to bring back to trans state */
		spin_lock(&host->lock);
		do {
			/* if anything wrong, let block driver do error
			 * handling.
			 */
			err = msdc_get_card_status(mmc, host, &status);
			if (err) {
				ERR_MSG("CMD13 ERR<%d>", err);
				break;
			}

			state = R1_CURRENT_STATE(status);
			ERR_MSG("check card state<%d>", state);
			if (state == R1_STATE_DATA || state == R1_STATE_RCV) {
				ERR_MSG("state<%d> need cmd12 to stop", state);
				msdc_send_stop(host);
			} else if (state == R1_STATE_PRG) {
				ERR_MSG("state<%d> card is busy", state);
				spin_unlock(&host->lock);
				msleep(100);
				spin_lock(&host->lock);
			}

			if (time_after(jiffies, tmo)
				&& (state != R1_STATE_TRAN)) {
				ERR_MSG("card stuck in %d state", state);
				spin_unlock(&host->lock);
				if (host->hw->host_function == MSDC_SD) {
					ERR_MSG("remove such bad card!");
					msdc_set_bad_card_and_remove(host);
				}
				spin_lock(&host->lock);
				break;
			}
		} while (state != R1_STATE_TRAN);
		spin_unlock(&host->lock);

		data->error = (unsigned int)-ETIMEDOUT;
		host->sw_timeout++;

		if (mrq->done)
			mrq->done(mrq);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		/* clear flag here */
		atomic_set(&host->mmc->is_data_dma, 0);
#endif
		host->error |= REQ_DAT_ERR;
	} else {
		pr_info("[%s]: Warn! should not go here %d\n",
			__func__, __LINE__);
		/* do nothing, since legacy mode or async tuning
		 * have it own timeout.
		 */
		/* complete(&host->xfer_done); */
	}
}

static struct mmc_host_ops mt_msdc_ops = {
	.post_req                      = msdc_post_req,
	.pre_req                       = msdc_pre_req,
	.request                       = msdc_ops_request,
	.set_ios                       = msdc_ops_set_ios,
	.get_ro                        = msdc_ops_get_ro,
	.get_cd                        = msdc_ops_get_cd,
	.card_event                    = msdc_ops_card_event,
	.enable_sdio_irq               = msdc_ops_enable_sdio_irq,
	.start_signal_voltage_switch   = msdc_ops_switch_volt,
	.execute_tuning                = msdc_execute_tuning,
	.hw_reset                      = msdc_card_reset,
	.card_busy                     = msdc_card_busy,
	.prepare_hs400_tuning          = msdc_prepare_hs400_tuning,
	.remove_bad_sdcard	       = msdc_ops_set_bad_card_and_remove,
};

static void msdc_irq_cmd_complete(struct msdc_host *host)
{
	/* command interrupt completion */
	if (host->use_cmd_intr)
		complete(&host->cmd_done);
}

static void msdc_irq_data_complete(struct msdc_host *host,
	struct mmc_data *data, int error)
{
	void __iomem *base = host->base;
	struct mmc_request *mrq;

	if ((msdc_use_async_dma(data->host_cookie)) &&
	    (!host->tuning_in_progress)) {
		msdc_dma_stop(host);
		mrq = host->mrq;
		if (error) {
			dbg_add_host_log(host->mmc, 3, 0, 1);
#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
			if (MSDC_CHECK_FDE_ERR(host->mmc, mrq))
				goto skip_non_FDE_ERROR_HANDLING;
#endif

			msdc_clr_fifo(host->id);
			msdc_reset(host->id);
			msdc_dma_clear(host);

#if defined(CONFIG_MTK_HW_FDE) && defined(CONFIG_MTK_HW_FDE_AES)
skip_non_FDE_ERROR_HANDLING:
#endif

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			/* With CQ enable, is_data_dma shall be cleared at
			 * last setp instead of msdc_dma_stop().
			 * Otherwise, CMD only error handling and msdc_irq()'s
			 * error handling may interfer
			 */
			atomic_set(&host->mmc->is_data_dma, 0);

			/* CQ mode:just set data->error & let mmc layer tune */
			if (host->mmc->card
				&& mmc_card_cmdq(host->mmc->card)) {
				if (mrq->data->flags & MMC_DATA_WRITE)
					atomic_set(&host->cq_error_need_stop,
						1);
				goto skip;
			}
#endif
			if (mrq->data->flags & MMC_DATA_WRITE) {
				host->error |= REQ_CRC_STATUS_ERR;
				host->need_tune = TUNE_ASYNC_DATA_WRITE;
			} else {
				host->error |= REQ_DAT_ERR;
				host->need_tune = TUNE_ASYNC_DATA_READ;
			}
			host->err_cmd = mrq->cmd->opcode;
#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
skip:
#endif
			/* FIXME: return as cmd error for retry
			 * if data CRC error
			 */
			mrq->cmd->error = (unsigned int)-EILSEQ;
		} else {
			dbg_add_host_log(host->mmc, 3, 0, 0);
			msdc_dma_clear(host);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
			atomic_set(&host->cq_error_need_stop, 0);
			atomic_set(&host->mmc->is_data_dma, 0);
#endif

			host->error &= ~REQ_DAT_ERR;
			host->need_tune = TUNE_NONE;
			host->reautok_times = 0;
			host->tune_smpl_times = 0;
			host->err_cmd = -1;
			host->prev_cmd_cause_dump = 0;
		}
		if (mrq->done)
			mrq->done(mrq);
	} else {
		/* Autocmd12 issued but error, data transfer done INT won't set,
		 * so cmplete is need here
		 */
		complete(&host->xfer_done);
	}
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
static irqreturn_t msdc_cmdq_irq(struct msdc_host *host, u32 intsts)
{
	int err = 0;
	void __iomem *base = host->base;

	if (intsts & MSDC_INT_RSPCRCERR) {
		err = (unsigned int)-EILSEQ;
		ERR_MSG("XXX CMD CRC");
	} else if (intsts & MSDC_INT_CMDTMO) {
		err = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX CMD TIMEOUT");
	}

	if (intsts & MSDC_INT_DATCRCERR) {
		err = (unsigned int)-EILSEQ;
		ERR_MSG("XXX DATA CRC");
	} else if (intsts & MSDC_INT_DATTMO) {
		err = (unsigned int)-ETIMEDOUT;
		ERR_MSG("XXX DATA TIMEOUT");
	}

	if (err) {
		ERR_MSG("err = %d, intsts = 0x%x", err, intsts);
		MSDC_WRITE32(MSDC_INT, intsts); /* clear interrupts */
	}

	return cmdq_irq(host->mmc, err);
}
#endif

static irqreturn_t msdc_irq(int irq, void *dev_id)
{
	struct msdc_host *host = (struct msdc_host *)dev_id;
	struct mmc_data *data = host->data;
	struct mmc_command *cmd = host->cmd;
	struct mmc_command *stop = NULL;
	void __iomem *base = host->base;

	u32 acmdsts = MSDC_INT_ACMDCRCERR | MSDC_INT_ACMDTMO |
			MSDC_INT_ACMDRDY | MSDC_INT_ACMD19_DONE;
	u32 datsts = MSDC_INT_DATCRCERR | MSDC_INT_DATTMO;
	u32 gpdsts = MSDC_INT_GPDCSERR | MSDC_INT_BDCSERR;
	u32 intsts, inten;
	u32 cmdsts = MSDC_INT_RSPCRCERR | MSDC_INT_CMDTMO | MSDC_INT_CMDRDY;

	if (host->hw->flags & MSDC_SDIO_IRQ)
		spin_lock(&host->sdio_irq_lock);

	intsts = MSDC_READ32(MSDC_INT);
	host->intsts = intsts; /* save int raw status */

	latest_int_status[host->id] = intsts;
	inten = MSDC_READ32(MSDC_INTEN);
	if (host->hw->flags & MSDC_SDIO_IRQ)
		intsts &= inten;

#ifdef CONFIG_MTK_EMMC_HW_CQ
	if (host->mmc->card
		&& mmc_card_cmdq(host->mmc->card)
		&& (intsts & MSDC_INT_CMDQ)) {
		msdc_cmdq_irq(host, intsts);
		MSDC_WRITE32(MSDC_INT, intsts); /* clear interrupts */

		/* not used, but for coverity */
		if (host->hw->flags & MSDC_SDIO_IRQ)
			spin_unlock(&host->sdio_irq_lock);

		return IRQ_HANDLED;
	}
#endif

	/* CMD TO/CRC/RDY polling status in
	 * msdc_command_resp_polling()
	 * msdc_irq cannot clear here or cause
	 * msdc_command_resp_polling() SW timeout.
	 */
	/* don't check cmd status if int not enabled */
	if (!(inten & cmdsts))
		intsts &= ~cmdsts;

	MSDC_WRITE32(MSDC_INT, intsts); /* clear interrupts */

	/* sdio interrupt */
	if (host->hw->flags & MSDC_SDIO_IRQ) {
		spin_unlock(&host->sdio_irq_lock);

		if (intsts & MSDC_INT_SDIOIRQ)
			mmc_signal_sdio_irq(host->mmc);
	}

	if (intsts & gpdsts) {
		/* GPD or BD checksum verification error occurs.
		 * There shall be HW issue, so BUG_ON here
		 */
		msdc_dump_gpd_bd(host->id);
		msdc_dump_dbg_register(NULL, 0, NULL, host);
		if (host->hw->host_function == MSDC_SD) {
#ifdef CONFIG_GPIOLIB
			if (host->hw->cd_level == __gpio_get_value(cd_gpio))
				WARN_ON(1);
#endif
		} else {
			WARN_ON(1);
		}
	}

	if (intsts & cmdsts) {
		msdc_irq_cmd_complete(host);
		/* return if no other intr event */
		if (!(inten & intsts & ~cmdsts))
			goto out;
	}

	if (data == NULL)
		goto skip_data_interrupts;

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug2(host, stop, &intsts);
#endif

	stop = data->stop;

	if (intsts & MSDC_INT_XFER_COMPL) {
		/* Finished data transfer */
		host->data_timeout_cont = 0;
		data->bytes_xfered = host->dma.xfersz;
		msdc_irq_data_complete(host, data, 0);
		goto out;
	}

	if (intsts & datsts) {
		if (intsts & MSDC_INT_DATTMO) {
			data->error = (unsigned int)-ETIMEDOUT;
			host->data_timeout_cont++;
			ERR_MSG(
			"XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATTMO (cont %d)",
				host->mrq->cmd->opcode,
				host->mrq->cmd->arg, host->data_timeout_cont);
		} else if (intsts & MSDC_INT_DATCRCERR) {
			data->error = (unsigned int)-EILSEQ;
			host->data_timeout_cont = 0;
			ERR_MSG(
	"XXX CMD<%d> Arg<0x%.8x> MSDC_INT_DATCRCERR, SDC_DCRC_STS<0x%x>",
				host->mrq->cmd->opcode, host->mrq->cmd->arg,
				MSDC_READ32(SDC_DCRC_STS));
		}

		/* do basic reset, or stop command will sdc_busy */
		if ((intsts & MSDC_INT_DATTMO)
		 || (host->hw->host_function == MSDC_SDIO))
			msdc_dump_info(NULL, 0, NULL, host->id);

		if (host->dma_xfer)
			msdc_reset(host->id);
		else
			msdc_reset_hw(host->id);

		atomic_set(&host->abort, 1);    /* For PIO mode exit */

		goto tune;

	}

	if ((stop != NULL) &&
	    (host->autocmd & MSDC_AUTOCMD12) &&
	    (intsts & acmdsts)) {
		if (intsts & MSDC_INT_ACMDRDY) {
			u32 *arsp = &stop->resp[0];
			*arsp = MSDC_READ32(SDC_ACMD_RESP);
			goto skip_data_interrupts;
		} else if (intsts & MSDC_INT_ACMDCRCERR) {
			stop->error = (unsigned int)-EILSEQ;
			host->error |= REQ_STOP_EIO;
			ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_ACMDCRCERR",
				stop->opcode, stop->arg);
		} else if (intsts & MSDC_INT_ACMDTMO) {
			stop->error = (unsigned int)-ETIMEDOUT;
			host->error |= REQ_STOP_TMO;
			ERR_MSG("XXX CMD<%d> Arg<0x%.8x> MSDC_INT_ACMDTMO",
				stop->opcode, stop->arg);
		}
		if (host->dma_xfer)
			msdc_reset(host->id);
		else
			msdc_reset_hw(host->id);

		goto tune;
	}

skip_data_interrupts:
	/* command interrupts */
	if ((cmd == NULL) || !(intsts & acmdsts))
		goto skip_cmd_interrupts;

#ifdef MTK_MSDC_ERROR_TUNE_DEBUG
	msdc_error_tune_debug1(host, cmd, NULL, &intsts);
#endif

skip_cmd_interrupts:
	if (host->dma_xfer)
		msdc_irq_data_complete(host, data, 1);

	/*latest_int_status[host->id] = 0;*/
	return IRQ_HANDLED;

tune:   /* DMA DATA transfer crc error */
	/* PIO mode can't do complete, because not init */
	if (host->dma_xfer)
		msdc_irq_data_complete(host, data, 1);

out:
	return IRQ_HANDLED;
}

/* init gpd and bd list in msdc_drv_probe */
static void msdc_init_gpd_bd(struct msdc_host *host, struct msdc_dma *dma)
{
	struct gpd_t *gpd = dma->gpd;
	struct bd_t *bd = dma->bd;
	struct bd_t *ptr, *prev;
	dma_addr_t dma_addr;

	/* we just support one gpd */
	int bdlen = MAX_BD_PER_GPD;

	/* init the 2 gpd */
	memset(gpd, 0, sizeof(struct gpd_t) * 2);
	dma_addr = dma->gpd_addr + sizeof(struct gpd_t);
	gpd->nexth4 = upper_32_bits(dma_addr) & 0xF;
	gpd->next = lower_32_bits(dma_addr);

	gpd->bdp = 1;           /* hwo, cs, bd pointer */
	dma_addr = dma->bd_addr;
	gpd->ptrh4 = upper_32_bits(dma_addr) & 0xF;
	gpd->ptr = lower_32_bits(dma_addr); /* physical address */

	memset(bd, 0, sizeof(struct bd_t) * bdlen);
	ptr = bd + bdlen - 1;
	while (ptr != bd) {
		prev = ptr - 1;
		dma_addr = dma->bd_addr + sizeof(struct bd_t) * (ptr - bd);
		prev->nexth4 = upper_32_bits(dma_addr) & 0xF;
		prev->next = lower_32_bits(dma_addr);
		ptr = prev;
	}
}

#ifdef CONFIG_MTK_HIBERNATION
int msdc_drv_pm_restore_noirq(struct device *device)
{
	struct msdc_host *host = NULL;

	WARN_ON(device == NULL);
	host = dev_get_drvdata(device);
	if (host->hw->host_function == MSDC_SD) {
		if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
#ifdef CONFIG_GPIOLIB
			if ((host->hw->cd_level == __gpio_get_value(cd_gpio))
			 && host->mmc->card) {
				mmc_card_set_removed(host->mmc->card);
				host->card_inserted = 0;
			}
#endif
		}
		host->block_bad_card = 0;
		msdc_reset_bad_sd_detecter(host);
	}

	return 0;
}
#endif

static void msdc_deinit_hw(struct msdc_host *host)
{
	void __iomem *base = host->base;

	/* Disable and clear all interrupts */
	MSDC_CLR_BIT32(MSDC_INTEN, MSDC_READ32(MSDC_INTEN));
	MSDC_WRITE32(MSDC_INT, MSDC_READ32(MSDC_INT));

	/* make sure power down */
	msdc_set_power_mode(host, MMC_POWER_OFF);
}

static void msdc_remove_host(struct msdc_host *host)
{
	if (host->irq >= 0)
		free_irq(host->irq, host);
	dev_set_drvdata(&host->pdev->dev, NULL);
	msdc_deinit_hw(host);
	kfree(host->hw);
	mmc_free_host(host->mmc);
}

static void msdc_add_host(struct work_struct *work)
{
	int ret;
	struct msdc_host *host = NULL;

	host = container_of(work, struct msdc_host, work_init.work);
	if (host && host->mmc) {
		ret = mmc_add_host(host->mmc);
		if (ret)
			msdc_remove_host(host);
	}
}

/* FIX ME */
static void msdc_dvfs_kickoff(struct work_struct *work)
{
}

#ifdef CONFIG_MTK_EMMC_HW_CQ
static void msdc_cqhci_post_cqe_halt(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;

	MSDC_SET_FIELD(MSDC_DMA_CTRL, MSDC_DMA_CTRL_STOP, 1);
	msdc_reset_hw(host->id);
}

void msdc_cqhci_set_busy_timeout(struct mmc_host *mmc, u32 val)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (val == 0)
		val = host->max_busy_timeout_ms;

	msdc_set_busy_timeout_ms(host, val);
}

static void msdc_cqhci_pre_cqe_enable(struct mmc_host *mmc, bool en)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;

	if (en) {
		/* enable busy check */
		MSDC_SET_BIT32(MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
		/* default write data / busy timeout 20 * 1000ms */
		msdc_set_busy_timeout_ms(host, 20 * 1000);
		/* default read data timeout 100ms */
		msdc_set_timeout(host, (100 * 1000 * 1000UL), 0);
	} else {
		/* disable busy check */
		MSDC_CLR_BIT32(MSDC_PATCH_BIT1, MSDC_PB1_BUSY_CHECK_SEL);
		/* switch to PIO mode after cmdq_disable */
		MSDC_SET_BIT32(MSDC_CFG, MSDC_CFG_PIO);
	}
}

static int msdc_cqhci_pre_irq_complete(struct mmc_host *mmc, unsigned int err)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	int ret = 0;
	u32 msdc_int, msdc_int_err = MSDC_INT_DATTMO |
		MSDC_INT_DATCRCERR | MSDC_INT_CMDTMO | MSDC_INT_RSPCRCERR;

	msdc_int = MSDC_READ32(MSDC_INT);
	if (!err && (msdc_int & msdc_int_err)) {
		MSDC_WRITE32(MSDC_INT, msdc_int);
		ret = (unsigned int)-ETIMEDOUT;
		pr_notice("%s %d: msdc_int = 0x%x, err = %d\n",
			__func__, __LINE__, msdc_int, err);
	}

	return ret;
}

static int msdc_cqhci_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct cmdq_host *cq_host = mmc_cmdq_private(mmc);
	int ret = 0;

	pr_notice("%s: re-cal timing\n", __func__);

	if (cq_host && cq_host->enabled)
		pr_notice("WARN: data xf with cqhci enabled\n");

	ret = emmc_execute_dvfs_autok(host,
		MMC_SEND_TUNING_BLOCK_HS200);

	/* clear flag */
	host->need_tune = TUNE_NONE;

	return ret;
}

static const struct cmdq_host_ops msdc_cmdq_ops = {
	.post_cqe_halt = msdc_cqhci_post_cqe_halt,
#if (defined(CONFIG_MTK_HW_FDE))
	.crypto_cfg = msdc_cqhci_crypto_cfg,
#endif
	.set_data_timeout = msdc_cqhci_set_busy_timeout,
	.pre_cqe_enable = msdc_cqhci_pre_cqe_enable,
	.pre_irq_complete = msdc_cqhci_pre_irq_complete,
	.reset = msdc_cqhci_reset,
};
#endif

static int msdc_drv_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc = NULL;
	struct msdc_host *host = NULL;
	struct msdc_hw *hw = NULL;
	void __iomem *base = NULL;
	int ret = 0;

	/* Allocate MMC host for this device */
	mmc = mmc_alloc_host(sizeof(struct msdc_host), &pdev->dev);
	if (!mmc)
		return -ENOMEM;

	ret = msdc_dt_init(pdev, mmc);
	if (ret) {
		mmc_free_host(mmc);
		return ret;
	}

	host = mmc_priv(mmc);
	base = host->base;
	hw = host->hw;

	/* Set host parameters to mmc */
	mmc->ops        = &mt_msdc_ops;
	mmc->ocr_avail  = MSDC_OCR_AVAIL;
	/* set clock when dts is not defined */
	if (!mmc->f_min)
		mmc->f_min = HOST_MIN_MCLK;
	if (!mmc->f_max)
		mmc->f_max = HOST_MAX_MCLK;

	if ((hw->flags & MSDC_SDIO_IRQ) || (hw->flags & MSDC_EXT_SDIO_IRQ))
		mmc->caps |= MMC_CAP_SDIO_IRQ;  /* yes for sdio */
#ifdef MTK_MSDC_USE_CMD23
	if (host->hw->host_function == MSDC_EMMC)
		mmc->caps |= MMC_CAP_CMD23;
#endif
	if (host->hw->host_function == MSDC_SD) {
		mmc->caps |= MMC_CAP_AGGRESSIVE_PM;
#ifdef NMCARD_SUPPORT
		mmc->caps2 |= MMC_CAP2_NMCARD;
#endif
	}
	mmc->caps |= MMC_CAP_ERASE;

#ifdef CONFIG_MTK_EMMC_HW_CQ
	if (check_enable_cqe() && host->hw->host_function == MSDC_EMMC)
		mmc->caps2 |= MMC_CAP2_CQE;
#endif

#ifdef CONFIG_MMC_CRYPTO
	if (host->hw->host_function == MSDC_EMMC) {
		mmc->caps2 |= MMC_CAP2_CRYPTO;
		#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
		/* inline crypto */
		msdc_crypto_init_vops(mmc);
		#endif
	}
#endif

	/* If 0  < mmc->max_busy_timeout < cmd.busy_timeout,
	 * R1B will change to R1, host will not detect DAT0 busy,
	 * next CMD may send to eMMC at busy state.
	 */
	mmc->max_busy_timeout = 0;

	/* MMC core transfer sizes tunable parameters */
	mmc->max_segs = MAX_HW_SGMTS;
	/* mmc->max_phys_segs = MAX_PHY_SGMTS; */
	if (hw->host_function == MSDC_SDIO)
		mmc->max_seg_size  = MAX_SGMT_SZ_SDIO;
	else
		mmc->max_seg_size  = MAX_SGMT_SZ;
	mmc->max_blk_size  = HOST_MAX_BLKSZ;
	mmc->max_req_size  = MAX_REQ_SZ;
	mmc->max_blk_count = MAX_REQ_SZ / 512; /* mmc->max_req_size; */

	host->hclk              = msdc_get_hclk(pdev->id, hw->clk_src);
	host->power_mode        = MMC_POWER_OFF;
	host->power_control     = NULL;
	host->power_switch      = NULL;

	host->dma_mask          = DMA_BIT_MASK(36);
	mmc_dev(mmc)->dma_mask  = &host->dma_mask;

#ifndef FPGA_PLATFORM
	/* FIX ME, consider to move it into msdc_io.c */
	if (msdc_get_ccf_clk_pointer(pdev, host))
#ifndef CONFIG_MTK_MSDC_BRING_UP_BYPASS
		return 1;
#else
		pr_notice("[MSDC]msdc_get_ccf_clk_pointer fail.\n");
#endif
#endif

	msdc_set_host_power_control(host);

#ifdef CONFIG_MTK_EMMC_HW_CQ
	if ((host->hw->host_function == MSDC_EMMC)
		&& (host->mmc->caps2 & MMC_CAP2_CQE)) {
		host->cq_host = cmdq_pltfm_init(pdev);
		host->cq_host->caps |= CMDQ_TASK_DESC_SZ_128;
		host->cq_host->caps |= CMDQ_CAP_CRYPTO_SUPPORT;
		host->cq_host->mmio = base + 0x800;
		cmdq_init(host->cq_host, mmc, true);
		host->cq_host->ops = &msdc_cmdq_ops;
		host->cq_host->mmc->max_segs = 128;
		/* cqhci describes data buffer length by 16bits
		 * 0 means 65536 bytes, so we don't have to -1 here
		 */
		host->cq_host->mmc->max_seg_size = 64 * 1024;
		host->cq_host->mmc->card = host->mmc->card;
	}
#endif


	host->card_inserted =
		host->mmc->caps & MMC_CAP_NONREMOVABLE ? 1 : 0;
	host->timeout_clks = DEFAULT_DTOC * 1048576;

	if (host->hw->host_function == MSDC_EMMC) {
#ifdef MTK_MSDC_USE_CMD23
		host->autocmd &= ~MSDC_AUTOCMD12;
#if (MSDC_USE_AUTO_CMD23 == 1)
		host->autocmd |= MSDC_AUTOCMD23;
#endif
#else
		host->autocmd |= MSDC_AUTOCMD12;
#endif
	} else if (host->hw->host_function == MSDC_SD) {
		host->autocmd |= MSDC_AUTOCMD12;
	} else {
		host->autocmd &= ~MSDC_AUTOCMD12;
	}

	host->mrq = NULL;

	/* using dma_alloc_coherent */
	host->dma.gpd = dma_alloc_coherent(&pdev->dev,
			MAX_GPD_NUM * sizeof(struct gpd_t),
			&host->dma.gpd_addr, GFP_KERNEL);
	if (!host->dma.gpd)
		return -ENOMEM;
	host->dma.bd = dma_alloc_coherent(&pdev->dev,
			MAX_BD_NUM * sizeof(struct bd_t),
			&host->dma.bd_addr, GFP_KERNEL);
	if (!host->dma.bd)
		return -ENOMEM;
	host->pio_kaddr = kmalloc_array(DIV_ROUND_UP(MAX_SGMT_SZ, PAGE_SIZE),
		sizeof(ulong), GFP_KERNEL);
	WARN_ON(!host->pio_kaddr);
	msdc_init_gpd_bd(host, &host->dma);
	mtk_msdc_host[host->id] = host;

	/* for re-autok */
	host->tuning_in_progress = false;
	INIT_DELAYED_WORK(&(host->set_vcore_workq), msdc_unreq_vcore);
	init_completion(&host->autok_done);
	host->need_tune	= TUNE_NONE;
	host->err_cmd = -1;

	if (host->hw->host_function == MSDC_SDIO)
		wakeup_source_init(&host->trans_lock, "MSDC Transfer Lock");

	INIT_DELAYED_WORK(&host->data_timeout_work, msdc_check_data_timeout);
	INIT_DELAYED_WORK(&host->work_init, msdc_add_host);
	INIT_DELAYED_WORK(&host->remove_card, msdc_remove_card);

	spin_lock_init(&host->lock);
	spin_lock_init(&host->reg_lock);
	spin_lock_init(&host->remove_bad_card);
#ifdef CONFIG_MTK_EMMC_HW_CQ
	spin_lock_init(&host->cmd_dump_lock);
#endif
	spin_lock_init(&host->sdio_irq_lock);

#ifdef CONFIG_MTK_EMMC_CQ_SUPPORT
	atomic_set(&host->cq_error_need_stop, 0);
#endif

	msdc_dvfs_reg_backup_init(host);

	ret = request_irq((unsigned int)host->irq, msdc_irq, IRQF_TRIGGER_NONE,
		DRV_NAME, host);
	if (ret) {
		host->irq = -1;
		goto release;
	}

	MVG_EMMC_SETUP(host);

	if (hw->request_sdio_eirq)
		/* set to combo_sdio_request_eirq() for WIFI */
		/* msdc_eirq_sdio() will be called when EIRQ */
		hw->request_sdio_eirq(msdc_eirq_sdio, (void *)host);

	if (host->hw->host_function == MSDC_EMMC)
		mmc->pm_flags |= MMC_PM_KEEP_POWER;

	dev_set_drvdata(&pdev->dev, host);

#ifdef CONFIG_MTK_HIBERNATION
	if (pdev->id == 1)
		register_swsusp_restore_noirq_func(ID_M_MSDC,
			msdc_drv_pm_restore_noirq, &(pdev->dev));
#endif

	/* Use ordered workqueue to reduce msdc moudle init time, and we should
	 * run sd init in a delay time(we use 5s) to ensure assign mmcblk1 to sd
	 */
	if (!queue_delayed_work(wq_init, &host->work_init,
		(host->hw->host_function == MSDC_SD ? HZ * 5 : 0))) {
		pr_notice("msdc%d init work queuing failed\n", host->id);
		WARN_ON(1);
	}

	if (host->hw->host_function == MSDC_SDIO) {
		INIT_DELAYED_WORK(&host->work_sdio, msdc_dvfs_kickoff);

		/* Use ordered workqueue to reduce msdc moudle init time */
		if (!queue_delayed_work(wq_init, &host->work_sdio, 30 * HZ)) {
			pr_notice(
			"msdc%d queue delay work failed WARN_ON,[%s]L:%d\n",
				host->id, __func__, __LINE__);
			WARN_ON(1);
		}
	}

	pm_qos_add_request(&host->msdc_pm_qos_req,
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, MSDC_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(&pdev->dev);

#ifdef MTK_MSDC_BRINGUP_DEBUG
	pr_info("[%s]: msdc%d, mmc->caps=0x%x, mmc->caps2=0x%x\n",
		__func__, host->id, mmc->caps, mmc->caps2);
	msdc_dump_clock_sts(NULL, 0, NULL, host);
#endif

	if (host->hw->host_function == MSDC_EMMC)
		msdc_debug_proc_init_bootdevice();

	return 0;

release:
	msdc_remove_host(host);

	return ret;
}

/* 4 device share one driver, using "drvdata" to show difference */
static int msdc_drv_remove(struct platform_device *pdev)
{
	struct msdc_host *host;
	struct resource *mem;

	host = dev_get_drvdata(&pdev->dev);

	ERR_MSG("msdc_drv_remove");

#ifndef FPGA_PLATFORM
	/* clock unprepare */
	if (host->clk_ctl)
		clk_disable_unprepare(host->clk_ctl);
	if (host->hclk_ctl)
		clk_disable_unprepare(host->hclk_ctl);
#if defined(CONFIG_MTK_HW_FDE) || defined(CONFIG_MMC_CRYPTO)
	if (host->aes_clk_ctl)
		clk_disable_unprepare(host->aes_clk_ctl);
#endif
	if (host->axi_clk_ctl)
		clk_disable_unprepare(host->axi_clk_ctl);
	if (host->ahb2axi_brg_clk_ctl)
		clk_disable_unprepare(host->ahb2axi_brg_clk_ctl);
	if (host->pclk_ctl)
		clk_disable_unprepare(host->pclk_ctl);
#endif
	pm_qos_remove_request(&host->msdc_pm_qos_req);
	pm_runtime_disable(&pdev->dev);
	mmc_remove_host(host->mmc);

	dma_free_coherent(NULL, MAX_GPD_NUM * sizeof(struct gpd_t),
		host->dma.gpd, host->dma.gpd_addr);
	dma_free_coherent(NULL, MAX_BD_NUM * sizeof(struct bd_t),
		host->dma.bd, host->dma.bd_addr);
	kfree(host->pio_kaddr);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (mem)
		release_mem_region(mem->start, mem->end - mem->start + 1);

	msdc_remove_host(host);

	return 0;
}

#ifdef CONFIG_PM
static int msdc_runtime_suspend(struct device *dev)
{
	struct msdc_host *host = dev_get_drvdata(dev);

	clk_disable_unprepare(host->clk_ctl);
	if (host->aes_clk_ctl)
		clk_disable_unprepare(host->aes_clk_ctl);
	if (host->hclk_ctl)
		clk_disable_unprepare(host->hclk_ctl);
	if (host->axi_clk_ctl)
		clk_disable_unprepare(host->axi_clk_ctl);
	if (host->ahb2axi_brg_clk_ctl)
		clk_disable_unprepare(host->ahb2axi_brg_clk_ctl);
	if (host->pclk_ctl)
		clk_disable_unprepare(host->pclk_ctl);

	pm_qos_update_request(&host->msdc_pm_qos_req,
		PM_QOS_DEFAULT_VALUE);

	return 0;
}

static int msdc_runtime_resume(struct device *dev)
{
	struct msdc_host *host = dev_get_drvdata(dev);
	struct arm_smccc_res smccc_res;
	void __iomem *base = host->base;

	pm_qos_update_request(&host->msdc_pm_qos_req, 0);

	if (host->pclk_ctl)
		(void)clk_prepare_enable(host->pclk_ctl);
	if (host->axi_clk_ctl)
		(void)clk_prepare_enable(host->axi_clk_ctl);
	if (host->ahb2axi_brg_clk_ctl)
		(void)clk_prepare_enable(host->ahb2axi_brg_clk_ctl);
	(void)clk_prepare_enable(host->clk_ctl);
	if (host->aes_clk_ctl)
		(void)clk_prepare_enable(host->aes_clk_ctl);
	if (host->hclk_ctl)
		(void)clk_prepare_enable(host->hclk_ctl);

	while (!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB))
		cpu_relax();
	/*
	 * 1: MSDC_AES_CTL_INIT
	 * 4: cap_id, no-meaning
	 * 1: cfg_id, we choose the second cfg group
	 */
	if (host->mmc->caps2 & MMC_CAP2_CRYPTO)
		arm_smccc_smc(MTK_SIP_KERNEL_HW_FDE_MSDC_CTL,
			1, 4, 1, 0, 0, 0, 0, &smccc_res);

	return 0;
}

static int msdc_suspend(struct device *dev)
{
	struct msdc_host *host = dev_get_drvdata(dev);
	int ret = 0;

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: already runtime suspended\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}
	ret = msdc_runtime_suspend(dev);

out:
#if defined(CONFIG_MTK_HW_FDE) \
	&& !defined(CONFIG_MTK_HW_FDE_AES)
	host->is_crypto_init = false;
#endif
	return ret;
}

static int msdc_resume(struct device *dev)
{
	struct msdc_host *host = dev_get_drvdata(dev);
	int ret = 0;

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: runtime suspended, defer system resume\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}

	ret = msdc_runtime_resume(dev);

out:
	return ret;
}

static const struct dev_pm_ops msdc_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(msdc_suspend, msdc_resume)
	SET_RUNTIME_PM_OPS(msdc_runtime_suspend, msdc_runtime_resume,
				NULL)
};
#endif

static struct platform_driver mt_msdc_driver = {
	.probe = msdc_drv_probe,
	.remove = msdc_drv_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msdc_of_ids,
#ifdef CONFIG_PM
		.pm     = &msdc_pmops,
#endif
	},
};

/*--------------------------------------------------------------------------*/
/* module init/exit                                                         */
/*--------------------------------------------------------------------------*/
static int __init mt_msdc_init(void)
{
	int ret;

	/* Alloc init workqueue */
	wq_init = alloc_ordered_workqueue("msdc-init", 0);
	if (!wq_init) {
		pr_notice("msdc create work_queue failed.[%s]:%d",
			__func__, __LINE__);
		return -1;
	}

	ret = platform_driver_register(&mt_msdc_driver);
	if (ret) {
		pr_notice(DRV_NAME ": Can't register driver");
		return ret;
	}

	msdc_debug_proc_init();

	pr_debug(DRV_NAME ": MediaTek MSDC Driver\n");

	return 0;
}

static void __exit mt_msdc_exit(void)
{
	platform_driver_unregister(&mt_msdc_driver);

	if (wq_init) {
		destroy_workqueue(wq_init);
		wq_init = NULL;
	}

#ifdef CONFIG_MTK_HIBERNATION
	unregister_swsusp_restore_noirq_func(ID_M_MSDC);
#endif
}
module_init(mt_msdc_init);
module_exit(mt_msdc_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SD/MMC Card Driver");
