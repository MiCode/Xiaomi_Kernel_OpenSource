// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-dbg.h"
#include "mtk-mmc.h"
#include <linux/clk-provider.h>

/*  For msdc register dump  */
u16 msdc_offsets[] = {
	MSDC_CFG,
	MSDC_IOCON,
	MSDC_PS,
	MSDC_INT,
	MSDC_INTEN,
	MSDC_FIFOCS,
	MSDC_TXDATA,
	MSDC_RXDATA,
	SDC_CFG,
	SDC_CMD,
	SDC_ARG,
	SDC_STS,
	SDC_RESP0,
	SDC_RESP1,
	SDC_RESP2,
	SDC_RESP3,
	SDC_BLK_NUM,
	SDC_ADV_CFG0,
	EMMC_IOCON,
	SDC_ACMD_RESP,
	DMA_SA_H4BIT,
	MSDC_DMA_SA,
	MSDC_DMA_CTRL,
	MSDC_DMA_CFG,
	MSDC_DBG_SEL,
	MSDC_DBG_OUT,
	MSDC_PATCH_BIT,
	MSDC_PATCH_BIT1,
	MSDC_PATCH_BIT2,
	MSDC_PAD_TUNE,
	MSDC_PAD_TUNE0,
	MSDC_PAD_TUNE1,
	MSDC_DAT_RDDLY0,
	MSDC_DAT_RDDLY1,
	MSDC_PAD_CTL0,
	PAD_DS_TUNE,
	PAD_CMD_TUNE,
	EMMC50_PAD_DAT01_TUNE,
	EMMC50_PAD_DAT23_TUNE,
	EMMC50_PAD_DAT45_TUNE,
	EMMC50_PAD_DAT67_TUNE,
	EMMC50_CFG0,
	EMMC50_CFG1,
	EMMC50_CFG3,
	EMMC50_CFG4,
	SDC_FIFO_CFG,

	0xFFFF /*as mark of end */
};

u16 msdc_offsets_top[] = {
	EMMC_TOP_CONTROL,
	EMMC_TOP_CMD,
	EMMC50_PAD_CTL0,
	EMMC50_PAD_DS_TUNE,
	EMMC50_PAD_DAT0_TUNE,
	EMMC50_PAD_DAT1_TUNE,
	EMMC50_PAD_DAT2_TUNE,
	EMMC50_PAD_DAT3_TUNE,
	EMMC50_PAD_DAT4_TUNE,
	EMMC50_PAD_DAT5_TUNE,
	EMMC50_PAD_DAT6_TUNE,
	EMMC50_PAD_DAT7_TUNE,

	0xFFFF /*as mark of end */
};

static void msdc_dump_clock_sts_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	char buffer[512];
	char *buf_ptr = buffer;

	if (host->p_clk)
		buf_ptr += sprintf(buf_ptr,
			"[p_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->p_clk), clk_get_rate(host->p_clk));
	if (host->axi_clk)
		buf_ptr += sprintf(buf_ptr,
			"[axi_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->axi_clk), clk_get_rate(host->axi_clk));
	if (host->ahb_clk)
		buf_ptr += sprintf(buf_ptr,
			"[ahb_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->ahb_clk), clk_get_rate(host->ahb_clk));
	if (host->src_clk)
		buf_ptr += sprintf(buf_ptr,
			"[src_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->src_clk), clk_get_rate(host->src_clk));
	if (host->h_clk)
		buf_ptr += sprintf(buf_ptr,
			"[h_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->h_clk), clk_get_rate(host->h_clk));
	if (host->bus_clk)
		buf_ptr += sprintf(buf_ptr,
			"[bus_clk]enable:%d freq:%d,",
			__clk_is_enabled(host->bus_clk), clk_get_rate(host->bus_clk));
	if (host->src_clk_cg)
		buf_ptr += sprintf(buf_ptr,
			"[src_clk_cg]enable:%d freq:%d\n",
			__clk_is_enabled(host->src_clk_cg), clk_get_rate(host->src_clk_cg));

	*buf_ptr = '\0';
	SPREAD_PRINTF(buff, size, m, "%s", buffer);
}

void msdc_dump_clock_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	msdc_dump_clock_sts_core(buff, size, m, host);
}

void msdc_dump_ldo_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	u32 id = host->id;

	switch (id) {
	/*
	 * PMIC only provide regulator APIs with mutex protection.
	 * Therefore, can not dump msdc ldo status in IRQ context.
	 * Enable dump msdc ldo if you make sure the dump context
	 * is correct.
	 */
	case MSDC_EMMC:
		SPREAD_PRINTF(buff, size, m,
			" VEMC_EN=0x%x, VEMC_VOL=%duV [4b'1011(3V)]\n",
			regulator_is_enabled(host->mmc->supply.vmmc),
			regulator_get_voltage(host->mmc->supply.vmmc));
		break;
	case MSDC_SD:
		SPREAD_PRINTF(buff, size, m,
		" VMCH_EN=0x%x, VMCH_VOL=%duV\n",
			regulator_is_enabled(host->mmc->supply.vmmc),
			regulator_get_voltage(host->mmc->supply.vmmc));
		SPREAD_PRINTF(buff, size, m,
		" VMC_EN=0x%x, VMC_VOL=0%duV\n",
			regulator_is_enabled(host->mmc->supply.vqmmc),
			regulator_get_voltage(host->mmc->supply.vqmmc));
		break;
	default:
		break;
	}
}

void msdc_dump_register_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	u32 id = host->id;
	u32 msg_size = 0;
	u32 val;
	u16 offset, i;
	char buffer[PRINTF_REGISTER_BUFFER_SIZE + 1];
	char *buffer_cur_ptr = buffer;

	memset(buffer, 0, PRINTF_REGISTER_BUFFER_SIZE);
	SPREAD_PRINTF(buff, size, m, "MSDC%d normal register\n", id);
	for (i = 0; msdc_offsets[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets[i];
		val = readl(host->base + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	if (!host->top_base)
		return;

	MSDC_RST_REG_PRINT_BUF(msg_size,
		PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr);

	SPREAD_PRINTF(buff, size, m, "MSDC%d top register\n", id);

	for (i = 0;  msdc_offsets_top[i] != (u16)0xFFFF; i++) {
		offset = msdc_offsets_top[i];
		val = readl(host->top_base + offset);
		MSDC_REG_PRINT(offset, val, ONE_REGISTER_STRING_SIZE, msg_size,
			PRINTF_REGISTER_BUFFER_SIZE, buffer, buffer_cur_ptr, m);
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
		writel(i, host->base + MSDC_DBG_SEL);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE + 1,
			"[%.3hx:%.8x]", i, readl(host->base + MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	writel(0x27, host->base + MSDC_DBG_SEL);
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
		writel(i, host->base + EMMC50_CFG4);
		snprintf(buffer_cur_ptr, ONE_REGISTER_STRING_SIZE + 1,
			"[%.3hx:%.8x]", i, readl(host->base + MSDC_DBG_OUT));
		buffer_cur_ptr += ONE_REGISTER_STRING_SIZE;
	}
	SPREAD_PRINTF(buff, size, m, "%s\n", buffer);

	writel(0, host->base + MSDC_DBG_SEL);
}

void msdc_dump_autok(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	int i, j;
	int bit_pos, byte_pos, start;
	char buf[65];

	SPREAD_PRINTF(buff, size, m, "[AUTOK]VER : 0x%02x%02x%02x%02x\r\n",
		host->autok_res[0][AUTOK_VER3],
		host->autok_res[0][AUTOK_VER2],
		host->autok_res[0][AUTOK_VER1],
		host->autok_res[0][AUTOK_VER0]);

	for (i = AUTOK_VCORE_LEVEL1; i >= AUTOK_VCORE_LEVEL0; i--) {
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

		/* cmd response use ds pin, but window is
		 * different with data pin, because cmd response is SDR.
		 */
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
			"[AUTOK]CMD [EDGE:%d CMD_FIFO_EDGE:%d DLY1:%d DLY2:%d]\r\n",
			host->autok_res[i][0], host->autok_res[i][1],
			host->autok_res[i][5], host->autok_res[i][7]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT [RDAT_EDGE:%d RD_FIFO_EDGE:%d WD_FIFO_EDGE:%d]\r\n",
			host->autok_res[i][2], host->autok_res[i][3],
			host->autok_res[i][4]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DAT [LATCH_CK:%d DLY1:%d DLY2:%d]\r\n",
			host->autok_res[i][13], host->autok_res[i][9],
			host->autok_res[i][11]);
		SPREAD_PRINTF(buff, size, m,
			"[AUTOK]DS  [DLY1:%d DLY2:%d DLY3:%d]\r\n",
			host->autok_res[i][14], host->autok_res[i][16],
			host->autok_res[i][18]);
		SPREAD_PRINTF(buff, size, m, "[AUTOK]DAT [TX SEL:%d]\r\n",
			host->autok_res[i][20]);
	}
}

void msdc_dump_info(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host)
{
	struct mmc_host *mmc;

	if (host == NULL) {
		SPREAD_PRINTF(buff, size, m, "msdc host null\n");
		return;
	}

	mmc = host->mmc;

	if (host->tuning_in_progress == true)
		return;

	msdc_dump_register(buff, size, m, host);

	if (!buff)
		mdelay(10);

	msdc_dump_clock_sts(buff, size, m, host);

	msdc_dump_ldo_sts(buff, size, m, host);

	/* prevent bad sdcard, print too much log */
	if (host->id != MSDC_SD)
		msdc_dump_autok(buff, size, m, host);

	if (!buff)
		mdelay(10);

	msdc_dump_dbg_register(buff, size, m, host);
}
EXPORT_SYMBOL(msdc_dump_info);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SD/MMC Debug Driver");
