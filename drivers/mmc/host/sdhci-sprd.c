// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@unisoc.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <trace/hooks/mmc.h>
#include <linux/gpio/consumer.h>
#include <linux/types.h>
#include <linux/irqreturn.h>

#include "sdhci-pltfm.h"
#include "mmc_hsq.h"

#include "mmc_swcq.h"

#include "sdhci-sprd-debugfs.h"
#ifndef CONFIG_SPRD_DEBUG
#include "sdhci-sprd-debugfs.c"
#endif

#include "sdhci-sprd-swcq.h"
#include "sdhci-sprd-swcq.c"

#include "cqhci.h"
#include <trace/hooks/mmc.h>

#include "sdhci-sprd-powp.h"
#include "sdhci-sprd-powp.c"

#include "sdhci-sprd-tuning.h"
#include "sdhci-sprd-tuning.c"

#include "sdhci-sprd-ffu.h"
#include "sdhci-sprd-ffu.c"

#include "sdhci-sprd-health.h"
#include "sdhci-sprd-health.c"

#include "sdhci-sprd-debug.h"

#define CREATE_TRACE_POINTS
#include "trace_mmc_sprd.h"

#define DRIVER_NAME "sprd-sdhci"
#define SDHCI_SPRD_DUMP(f, x...) \
	pr_err("%s: " DRIVER_NAME ": " f, mmc_hostname(host->mmc), ## x)

#define SEND_SD_SWITCH		6
#define SEND_SD_READ_EXTR_SINGLE		48
#define SEND_TUNING_BLOCK		19
#define SEND_TUNING_BLOCK_HS200		21

/* SDHCI_ARGUMENT2 register high 16bit */
#define SDHCI_SPRD_ARG2_STUFF		GENMASK(31, 16)

#define SDHCI_SPRD_REG_32_DLL_CFG	0x200
#define  SDHCI_SPRD_DLL_ALL_CPST_EN	(BIT(18) | BIT(24) | BIT(25) | BIT(26) | BIT(27))
#define  SDHCI_SPRD_DLL_EN		BIT(21)
#define  SDHCI_SPRD_DLL_SEARCH_MODE	BIT(16)
#define  SDHCI_SPRD_DLL_INIT_COUNT	0xc00
#define SDHCI_SPRD_DLL_EN_MASK		(SDHCI_SPRD_DLL_EN | SDHCI_SPRD_DLL_ALL_CPST_EN)
#ifdef CONFIG_MMC_SPRD_SDHCR11P3
#define  SDHCI_SPRD_DLL_PHASE_INTERNAL	0x2
#else
#define SDHCI_SPRD_DLL_PHASE_INTERNAL	0x3
#endif

#define SDHCI_SPRD_REG_32_DLL_DLY	0x204

#define SDHCI_SPRD_REG_32_DLL_DLY_OFFSET	0x208
#define  SDHCI_SPRD_BIT_WR_DLY_INV		BIT(5)
#define  SDHCI_SPRD_BIT_CMD_DLY_INV		BIT(13)
#define  SDHCI_SPRD_BIT_POSRD_DLY_INV		BIT(21)
#define  SDHCI_SPRD_BIT_NEGRD_DLY_INV		BIT(29)

#define SDHCI_SPRD_ADMA_BUF_PROCESS_L	0x220
#define SDHCI_SPRD_ADMA_BUF_PROCESS_H	0x224

#define SDHCI_SPRD_BLK_CNT_BUF	0x228
#define SDHCI_SPRD_BLK_CNT_IO	0x22c

#define SDHCI_SPRD_ADMA_PROCESS_L	0x240
#define SDHCI_SPRD_ADMA_PROCESS_H	0x244

#define SDHCI_SPRD_REG_32_BUSY_POSI		0x250
#define  SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN	BIT(25)
#define  SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN	BIT(24)

#define SPRD_SDHC_REG_EMMC_DEBUG0	0x260
#define SPRD_SDHC_REG_EMMC_DEBUG1	0x264
#define SPRD_SDHC_REG_EMMC_DEBUG2	0x268

#define SDHCI_SPRD_REG_DEBOUNCE		0x28C
#define  SDHCI_SPRD_BIT_DLL_BAK		BIT(0)
#define  SDHCI_SPRD_BIT_DLL_VAL		BIT(1)

#define  SDHCI_SPRD_INT_SIGNAL_MASK	0x1B7F410B

/* SDHCI_HOST_CONTROL2 */
#define  SDHCI_SPRD_CTRL_HS200		0x0005
#define  SDHCI_SPRD_CTRL_HS400		0x0006
#define  SDHCI_SPRD_CTRL_HS400ES	0x0007

/*
 * According to the standard specification, BIT(3) of SDHCI_SOFTWARE_RESET is
 * reserved, and only used on Spreadtrum's design, the hardware cannot work
 * if this bit is cleared.
 * 1 : normal work
 * 0 : hardware reset
 */
#define  SDHCI_HW_RESET_CARD		BIT(3)

#define SDHCI_SPRD_MAX_CUR		1020
#define SDHCI_SPRD_CLK_MAX_DIV		1023

#define SDHCI_SPRD_CLK_DEF_RATE		26000000
#define SDHCI_SPRD_PHY_DLL_CLK		52000000
/*
 * The following register is defined by spreadtrum self.
 * It is not standard register of SDIO
 */
#define SDHCI_SPRD_REG_32_DLL_STS0	0x210
#define SDHCI_SPRD_DLL_LOCKED		BIT(18)

#define SDHCI_SPRD_REG_32_DLL_STS1	0x214
#define SDHCI_SPRD_DLL_WAIT_CNT		0xC0000000


#define SDHCI_SPRD_REG_8_DATWR_DLY	0x204
#define SDHCI_SPRD_REG_8_CMDRD_DLY	0x205
#define SDHCI_SPRD_REG_8_POSRD_DLY	0x206
#define SDHCI_SPRD_REG_8_NEGRD_DLY	0x207


#define SDHCI_SPRD_WR_DLY_MASK		0xff
#define SDHCI_SPRD_CMD_DLY_MASK		(0xff << 8)
#define SDHCI_SPRD_POSRD_DLY_MASK		(0xff << 16)
#define SDHCI_SPRD_NEGRD_DLY_MASK		(0xff << 24)
#define SDHCI_SPRD_DLY_TIMING(wr_dly, cmd_dly, posrd_dly, negrd_dly) \
		((wr_dly) | ((cmd_dly) << 8) | \
		((posrd_dly) << 16) | ((negrd_dly) << 24))

#define SDHCI_INT_CMD_ERR_MASK (SDHCI_INT_TIMEOUT | SDHCI_INT_CRC | \
		SDHCI_INT_END_BIT | SDHCI_INT_INDEX)

#define SDHCI_IP_VER_R10 10
#define SDHCI_IP_VER_R11 11

static void sdhci_sprd_dump_vendor_regs(struct sdhci_host *host);

struct mmc_gpio {
	struct gpio_desc *ro_gpio;
	struct gpio_desc *cd_gpio;
	irqreturn_t (*cd_gpio_isr)(int irq, void *dev_id);
	char *ro_label;
	char *cd_label;
	u32 cd_debounce_delay_ms;
};

struct ranges_t {
	int start;
	int end;
};

struct register_hotplug {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct sdhci_sprd_host {
	struct platform_device *pdev;
	u32 version;
	u32 ip_ver;
	struct clk *clk_sdio;
	struct clk *clk_enable;
	struct clk *clk_2x_enable;
	struct clk *clk_1x_enable;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_uhs;
	struct pinctrl_state *pins_default;
	u32 base_rate;
	int flags; /* backup of host attribute */
	u32 phy_delay[MMC_TIMING_MMC_HS400 + 2];
	u32 dll_cnt;
	u32 mid_dll_cnt;
	struct ranges_t *ranges;
	int detect_gpio;
	bool detect_gpio_polar;
	struct register_hotplug reg_detect_polar;
	struct register_hotplug reg_protect_enable;
	struct register_hotplug reg_debounce_en;
	struct register_hotplug reg_debounce_cn;
	struct register_hotplug reg_rmldo_en;
	unsigned char	power_mode;
	struct mmc_card *card;
	bool vqmmc_enabled;
	bool init_flag;
	bool support_swcq;
	bool support_cqe;
	bool support_ice;
	void __iomem *cqe_mem;	/* SPRD CQE mapped address (if available) */
	void __iomem *ice_mem;	/* SPRD ICE mapped address (if available) */
	struct sprd_host_tuning_info *tuning_info;
	u8 *tuning_data_buf;
	u32 tuning_flag;
	u32 cpst_cmd_dly;
	u32 int_status;
	bool tuning_merged;
	bool cmd_dly_all_pass;
	bool wait_read_idle;
	bool mask_excp;
	u8 min_data_timeout;
#ifdef CONFIG_SPRD_DEBUG
	u64 timestamp[10];
#endif
};

enum sdhci_sprd_tuning_type {
	SDHCI_SPRD_TUNING_DEFAULT,
	SDHCI_SPRD_TUNING_SD_HS,
	SDHCI_SPRD_TUNING_SD_UHS_CMD,
	SDHCI_SPRD_TUNING_SD_UHS_DATA,
};

struct sdhci_sprd_phy_cfg {
	const char *property;
	u32 clk_freq;
};

/* The index of the matrix means the corresponding timing */
static const struct sdhci_sprd_phy_cfg sdhci_sprd_phy_cfgs[] = {
	{ "sprd,phy-delay-legacy", HIGH_SPEED_MAX_DTR},
	{ "sprd,phy-delay-mmc-highspeed", MMC_HIGH_52_MAX_DTR},
	{ "sprd,phy-delay-sd-highspeed", HIGH_SPEED_MAX_DTR},
	{ "sprd,phy-delay-sd-uhs-sdr12", UHS_SDR12_MAX_DTR},
	{ "sprd,phy-delay-sd-uhs-sdr25", UHS_SDR25_MAX_DTR},
	{ "sprd,phy-delay-sd-uhs-sdr50", UHS_SDR50_MAX_DTR},
	{ "sprd,phy-delay-sd-uhs-sdr104", UHS_SDR104_MAX_DTR},
	{ "sprd,phy-delay-sd-uhs-ddr50", UHS_DDR50_MAX_DTR},
	{ "sprd,phy-delay-mmc-ddr52", MMC_HIGH_DDR_MAX_DTR},
	{ "sprd,phy-delay-mmc-hs200", MMC_HS200_MAX_DTR},
	{ "sprd,phy-delay-mmc-hs400", MMC_HS200_MAX_DTR},
	{ "sprd,phy-delay-mmc-hs400es", MMC_HS200_MAX_DTR},
};

#define TO_SPRD_HOST(host) sdhci_pltfm_priv(sdhci_priv(host))

/*
 * convert mask to offset in 32bits,
 * like convert 0x1E to 1, 0x00FF0000 to 16
 */
static int mask_to_offset(u32 mask)
{
	int offset;

	for (offset = 0; offset < 32; offset++)
		if ((mask >> offset) & BIT(0))
			return offset;

	return 0;
}

#include "sdhci-sprd-sp.c"

/* extra process for eMMC in mmc_init_card */
static void sdhci_sprd_extra_proc(void *data, struct mmc_card *card)
{
	int err;

	/* mmc health init */
	err = sprd_mmc_health_init(card);
	if (err)
		pr_err("sprd_mmc_health_init: function error = %d\n", err);

	/* mmc powp handle */
	if (!mmc_check_wp_fn(card->host))
		mmc_set_powp(card);

	/* print mmc device info */
	pr_info("%s: manfid= 0x%06x, name= %s, prv= 0x%x\n",
		mmc_hostname(card->host), card->cid.manfid,
		card->cid.prod_name, card->cid.prv);
	if (card->ext_csd.rev < 7)
		pr_info("%s: fwrev= 0x%x\n", mmc_hostname(card->host), card->cid.fwrev);
	else
		pr_info("%s: fwrev= 0x%*phN\n", mmc_hostname(card->host),
			MMC_FIRMWARE_LEN, card->ext_csd.fwrev);
	pr_info("%s: pre_eol_info=0x%02x, life_time= 0x%02x 0x%02x\n",
		mmc_hostname(card->host), card->ext_csd.pre_eol_info,
		card->ext_csd.device_life_time_est_typ_a,
		card->ext_csd.device_life_time_est_typ_b);

	/* Some mmc need perform some special ops */
	mmc_special_ops_needed(card);
}

static void sdhci_sprd_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (HOST_IS_SD_TYPE(mmc)) {
		sprd_host->card = card;
		sprd_host->init_flag = true;
	}
}

static void sdhci_sprd_init_config(struct sdhci_host *host)
{
	u16 val;

	/* set dll backup mode */
	val = sdhci_readl(host, SDHCI_SPRD_REG_DEBOUNCE);
	val |= SDHCI_SPRD_BIT_DLL_BAK | SDHCI_SPRD_BIT_DLL_VAL;
	sdhci_writel(host, val, SDHCI_SPRD_REG_DEBOUNCE);
}

static inline u32 sdhci_sprd_readl(struct sdhci_host *host, int reg)
{
	u32 sts = 0;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (unlikely(reg == SDHCI_MAX_CURRENT))
		return SDHCI_SPRD_MAX_CUR;

	sts = readl_relaxed(host->ioaddr + reg);

	if (likely(reg == SDHCI_INT_STATUS) && sprd_host->support_cqe &&
	    (readl(sprd_host->cqe_mem + CQHCI_IS) &
	    readl(sprd_host->cqe_mem + CQHCI_ISTE) &
	    readl(sprd_host->cqe_mem + CQHCI_ISGE)))
		sts |= SDHCI_INT_CQE;

	/*
	 * Some emmc need mask exception bit to avoid enterring recovery too many times.
	 * Mask exception bit(BIT(6)) in every R1/R1B if needed, except a QSR Query CMD13.
	 */
	if (sprd_host->mask_excp && likely(reg == SDHCI_RESPONSE) &&
		(sts & R1_EXCEPTION_EVENT) && host->cmd &&
		((host->cmd->flags & MMC_RSP_R1) == MMC_RSP_R1) &&
		!((host->cmd->opcode == MMC_SEND_STATUS) && (host->cmd->arg & BIT(15))))
		sts &= ~R1_EXCEPTION_EVENT;

	return sts;
}

static inline void sdhci_sprd_writel(struct sdhci_host *host, u32 val, int reg)
{
	/* SDHCI_MAX_CURRENT is reserved on Spreadtrum's platform */
	if (unlikely(reg == SDHCI_MAX_CURRENT))
		return;

	if (unlikely(reg == SDHCI_SIGNAL_ENABLE || reg == SDHCI_INT_ENABLE))
		val = val & SDHCI_SPRD_INT_SIGNAL_MASK;

	writel_relaxed(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_writew(struct sdhci_host *host, u16 val, int reg)
{
	u16 mask = 0;

	/* SDHCI_BLOCK_COUNT is Read Only on Spreadtrum's platform */
	if (unlikely(reg == SDHCI_BLOCK_COUNT))
		return;

	if ((HOST_IS_SD_TYPE(host->mmc)) &&
		(reg == SDHCI_COMMAND) &&
		(host->cmd->opcode == SEND_SD_SWITCH) &&
		(host->clock <= 400000) &&
		(host->cmd->arg == 0x80FFFFF1))
		val &= ~(mask | SDHCI_CMD_CRC);
	writew(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_writeb(struct sdhci_host *host, u8 val, int reg)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 status;
	u64 i = 0;

	if (unlikely(reg == SDHCI_TIMEOUT_CONTROL)) {
		/*
		 * Spec indicates the tuning process should be shorter than 150ms for 40
		 * executions of tuning command, and therefore data timeout value should
		 * be shorter than before for single tuning commond in Spreadtrum's platform.
		 */
		if (sprd_host->tuning_flag)
			val = 0x4;
		/*
		 * If a T-card's data response is later than normal card, it may failed in
		 * reading few blocks when set a small data timeout value.
		 * Need increase data timetout value for the kind of T-cards with a minimum
		 * data timeout parameter to ensure stability.
		 */
		else if (val < sprd_host->min_data_timeout)
			val = sprd_host->min_data_timeout;
	}

	if (unlikely(reg == SDHCI_SOFTWARE_RESET)) {
		/*
		 * In the tuning process, we need to wait for the controller to be
		 * idle before doing reset operation, otherwise it will affect the
		 * data transfer. So we can wait for BIT(9) of SDHCI_PRESENT_STATE
		 * register to check it on Spreadtrum's platform.
		 */
		if (sprd_host->tuning_flag &&
		   (sprd_host->int_status & SDHCI_INT_ERROR_MASK)) {
			while (i++ < 20000) {
				status = sdhci_readl(host, SDHCI_PRESENT_STATE);
				if (!(status & SDHCI_DOING_READ) &&
				    ((status & SDHCI_DATA_LVL_MASK) == SDHCI_DATA_LVL_MASK))
					break;
			}
			if (i >= 20000) {
				sprd_host->wait_read_idle = true;
				pr_err("%s: wait Read_Active idle fail, exit reset.\n",
					mmc_hostname(host->mmc));
				return;
			}
		}

		/*
		 * Since BIT(3) of SDHCI_SOFTWARE_RESET is reserved according to the
		 * standard specification, sdhci_reset() write this register directly
		 * without checking other reserved bits, that will clear BIT(3) which
		 * is defined as hardware reset on Spreadtrum's platform and clearing
		 * it by mistake will lead the card not work. So here we need to work
		 * around it.
		 */
		if (readb_relaxed(host->ioaddr + reg) & SDHCI_HW_RESET_CARD)
			val |= SDHCI_HW_RESET_CARD;
	}

	writeb_relaxed(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_sd_clk_off(struct sdhci_host *host)
{
	u16 ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);

	ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, ctrl, SDHCI_CLOCK_CONTROL);
}

static inline void sdhci_sprd_sd_clk_on(struct sdhci_host *host)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	ctrl |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, ctrl, SDHCI_CLOCK_CONTROL);
}

static void sdhci_sprd_enable_clk(struct sdhci_host *host, u16 clk)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	ktime_t timeout;

	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 150 ms */
	timeout = ktime_add_ms(ktime_get(), 150);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
		if (clk & SDHCI_CLOCK_INT_STABLE)
			break;
		if (timedout) {
			pr_err("%s: Internal clock never stabilised.\n",
			       mmc_hostname(host->mmc));
			sdhci_err_stats_inc(host, CTRL_TIMEOUT);
			sdhci_sprd_dump_vendor_regs(host);
			return;
		}
		udelay(10);
	}

	/*
	 * avoid SDHCI_CLOCK_CARD_EN set before VDDSDIO is enabled
	 * and SDHCI_CLOCK_CARD_EN not set in mmc_host_set_uhs_voltage
	 */
	if (HOST_IS_SD_TYPE(host->mmc) && !sprd_host->vqmmc_enabled)
		return;

	sdhci_sprd_sd_clk_on(host);
}

static inline void
sdhci_sprd_set_dll_invert(struct sdhci_host *host, u32 mask, bool en)
{
	u32 dll_dly_offset;

	dll_dly_offset = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_DLY_OFFSET);
	if (en)
		dll_dly_offset |= mask;
	else
		dll_dly_offset &= ~mask;
	sdhci_writel(host, dll_dly_offset, SDHCI_SPRD_REG_32_DLL_DLY_OFFSET);
}

static inline u32 sdhci_sprd_calc_div(u32 base_clk, u32 clk)
{
	u32 div;

	/* select 2x clock source */
	if (base_clk <= clk * 2)
		return 0;

	div = (u32) (base_clk / (clk * 2));

	if ((base_clk / div) > (clk * 2))
		div++;

	if (div % 2)
		div = (div + 1) / 2;
	else
		div = div / 2;

	if (div > SDHCI_SPRD_CLK_MAX_DIV)
		div = SDHCI_SPRD_CLK_MAX_DIV;

	return div;
}

static inline void _sdhci_sprd_set_clock(struct sdhci_host *host,
					unsigned int clk)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 div, val, mask;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	div = sdhci_sprd_calc_div(sprd_host->base_rate, clk);
	div = ((div & 0x300) >> 2) | ((div & 0xFF) << 8);

	sdhci_sprd_enable_clk(host, div);

	val = sdhci_readl(host, SDHCI_SPRD_REG_32_BUSY_POSI);
	mask = SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN | SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN;
	/* Enable CLK_AUTO when the clock is greater than 400K. */
	if (clk > 400000) {
		if (mask != (val & mask)) {
			val |= mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	} else {
		if (val & mask) {
			val &= ~mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	}
}

static void sdhci_sprd_enable_phy_dll(struct sdhci_host *host)
{
	u32 tmp;

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	if ((tmp & SDHCI_SPRD_DLL_EN_MASK) == SDHCI_SPRD_DLL_EN_MASK)
		return;

	tmp &= ~SDHCI_SPRD_DLL_EN_MASK;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);

	tmp |= SDHCI_SPRD_DLL_ALL_CPST_EN | SDHCI_SPRD_DLL_SEARCH_MODE |
		SDHCI_SPRD_DLL_INIT_COUNT | SDHCI_SPRD_DLL_PHASE_INTERNAL;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);

	tmp |= SDHCI_SPRD_DLL_EN;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);

	if (read_poll_timeout(sdhci_readl, tmp, (tmp & SDHCI_SPRD_DLL_LOCKED),
		2000, USEC_PER_SEC, false, host, SDHCI_SPRD_REG_32_DLL_STS0)) {
		pr_err("%s: DLL locked fail!\n", mmc_hostname(host->mmc));
		pr_info("%s: DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 mmc_hostname(host->mmc),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG));
		pr_info("%s: CLK_CTRL : 0x%x, HOST_CTRL2 : 0x%x\n",
			 mmc_hostname(host->mmc),
			 sdhci_readl(host, SDHCI_CLOCK_CONTROL),
			 sdhci_readl(host, SDHCI_AUTO_CMD_STATUS));
		if (!IS_ERR(host->mmc->supply.vmmc))
			pr_info("%s: vmmc voltage is %d uV\n",
				mmc_hostname(host->mmc),
				regulator_get_voltage(host->mmc->supply.vmmc));
		if (!IS_ERR(host->mmc->supply.vqmmc))
			pr_info("%s: vqmmc voltage is %d uV\n",
				mmc_hostname(host->mmc),
				regulator_get_voltage(host->mmc->supply.vqmmc));
	}
}

static void sdhci_sprd_disable_phy_dll(struct sdhci_host *host)
{
	u32 tmp;

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);

	if ((tmp & SDHCI_SPRD_DLL_EN_MASK) == SDHCI_SPRD_DLL_EN_MASK) {
		tmp &= ~SDHCI_SPRD_DLL_EN_MASK;
		sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	}
}

static bool sdhci_sprd_dll_en_allowed(struct sdhci_host *host)
{
	u8 reg = sdhci_readb(host, SDHCI_HOST_CONTROL2) & 0xF;

	if (reg < SDHCI_CTRL_UHS_SDR50 || reg == SDHCI_CTRL_UHS_DDR50)
		return false;

	return true;
}

static bool sdhci_sprd_check_invert(struct sdhci_host *host)
{
	if ((host->mmc->index == 0) && (host->timing == MMC_TIMING_MMC_HS))
		return true;

	return false;
}

static int sdhci_sprd_timing_matching(struct sdhci_host *host, unsigned int clock)
{
	struct mmc_host *mmc = host->mmc;
	int timing = mmc->ios.timing;

	switch (timing) {
	case MMC_TIMING_LEGACY:
		/* consider there is no significant diff in delay when clk below 50MHz */
		if (clock < sdhci_sprd_phy_cfgs[timing].clk_freq)
			return timing;
		break;
	case MMC_TIMING_MMC_HS:
		if ((clock == MMC_HIGH_26_MAX_DTR) || (clock == MMC_HIGH_52_MAX_DTR))
			return timing;
		break;
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR12:
	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_SDR50:
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_MMC_HS400:
		if (clock == sdhci_sprd_phy_cfgs[timing].clk_freq) {
			if (mmc->ios.enhanced_strobe)
				return timing + 1;
			else
				return timing;
		}
		break;
	default:
		pr_warn("%s: unknown timing %d\n", mmc_hostname(mmc), timing);
		break;
	};

	return -EINVAL;
}

static int sdhci_sprd_clock_matching(struct mmc_host *mmc, unsigned int clock)
{
	int timing = mmc->ios.timing;
	/*
	 * consider there is no significant diff in delay when clk below 50MHz
	 * and use the same delay with LEGACY timing
	 */
	if (clock < HIGH_SPEED_MAX_DTR)
		return MMC_TIMING_LEGACY;

	if (HOST_IS_EMMC_TYPE(mmc)) {
		switch (clock) {
		case MMC_HIGH_52_MAX_DTR:
			return MMC_TIMING_MMC_HS;
		case MMC_HS200_MAX_DTR:
			return MMC_TIMING_MMC_HS200;
		};
	} else {
		switch (clock) {
		case HIGH_SPEED_MAX_DTR:
			return MMC_TIMING_SD_HS;
		case UHS_SDR50_MAX_DTR:
			return MMC_TIMING_UHS_SDR50;
		case UHS_SDR104_MAX_DTR:
			return MMC_TIMING_UHS_SDR104;
		};
	}

	pr_warn("%s: uncommon clk frequency %d in timing %d\n",
		mmc_hostname(mmc), clock, timing);

	return -EINVAL;
}

static int sdhci_sprd_timing_check(struct sdhci_host *host, unsigned int clock)
{
	struct mmc_host *mmc = host->mmc;
	int timing;

	/* if clock matched current timing, return current timing */
	timing = sdhci_sprd_timing_matching(host, clock);
	if (timing >= 0)
		return timing;

	/* if not matched above, return corresponding timing to clock (if have) */
	return sdhci_sprd_clock_matching(mmc, clock);
}

static void sdhci_sprd_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	bool en = false;
	u32 *p = sprd_host->phy_delay;
	int timing = -EINVAL;

	if (clock == 0) {
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
	} else if (clock != host->clock) {
		sdhci_sprd_sd_clk_off(host);
		_sdhci_sprd_set_clock(host, clock);

		if (clock <= 400000 || sdhci_sprd_check_invert(host))
			en = true;
		sdhci_sprd_set_dll_invert(host, SDHCI_SPRD_BIT_CMD_DLY_INV |
					  SDHCI_SPRD_BIT_POSRD_DLY_INV, en);
	} else {
		sdhci_sprd_sd_clk_on(host);
	}

	/*
	 * According to the Spreadtrum SD host specification, when we changed
	 * the clock to be more than 52M, we should enable the PHY DLL which
	 * is used to track the clock frequency to make the clock work more
	 * stable. Otherwise deviation may occur of the higher clock.
	 */
	if (clock > SDHCI_SPRD_PHY_DLL_CLK && sdhci_sprd_dll_en_allowed(host))
		sdhci_sprd_enable_phy_dll(host);
	else
		sdhci_sprd_disable_phy_dll(host);

	/* Matching suitable timing delay refer to clock frequency */
	timing = sdhci_sprd_timing_check(host, clock);
	if (timing >= 0)
		sdhci_writel(host, p[timing], SDHCI_SPRD_REG_32_DLL_DLY);

	/*
	 * Print manfid/prod_name and do some special ops for some special t-cards
	 */
	if (sprd_host->init_flag && clock >= HIGH_SPEED_MAX_DTR) {
		struct mmc_card *card = sprd_host->card;

		/* print mmc device info */
		pr_info("%s: manfid= 0x%06x, name= %s\n",
			mmc_hostname(mmc), card->cid.manfid, card->cid.prod_name);

		/* Some mmc need perform some special ops */
		mmc_special_ops_needed(card);

		sprd_host->init_flag = false;
	}
}

static unsigned int sdhci_sprd_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	return clk_round_rate(sprd_host->clk_sdio, ULONG_MAX);
}

static unsigned int sdhci_sprd_get_min_clock(struct sdhci_host *host)
{
	return 100000;
}

static void sdhci_sprd_set_uhs_signaling(struct sdhci_host *host,
					 unsigned int timing)
{
	struct mmc_host *mmc = host->mmc;
	u16 ctrl_2;
	bool en = false;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	switch (timing) {
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_TIMING_UHS_SDR104:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		break;
	case MMC_TIMING_MMC_HS200:
		ctrl_2 |= SDHCI_SPRD_CTRL_HS200;
		break;
	case MMC_TIMING_MMC_HS400:
		if (mmc->ios.enhanced_strobe)
			ctrl_2 |= SDHCI_SPRD_CTRL_HS400ES;
		else
			ctrl_2 |= SDHCI_SPRD_CTRL_HS400;
		break;
	default:
		break;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	if (timing == MMC_TIMING_SD_HS || timing == MMC_TIMING_MMC_HS ||
			timing == MMC_TIMING_LEGACY)
		en = true;

	sdhci_sprd_set_dll_invert(host, SDHCI_SPRD_BIT_CMD_DLY_INV |
		SDHCI_SPRD_BIT_POSRD_DLY_INV, en);
}

static void sdhci_sprd_hw_reset(struct sdhci_host *host)
{
	int val;

	/*
	 * Note: don't use sdhci_writeb() API here since it is redirected to
	 * sdhci_sprd_writeb() in which we have a workaround for
	 * SDHCI_SOFTWARE_RESET which would make bit SDHCI_HW_RESET_CARD can
	 * not be cleared.
	 */
	val = readb_relaxed(host->ioaddr + SDHCI_SOFTWARE_RESET);
	val &= ~SDHCI_HW_RESET_CARD;
	writeb_relaxed(val, host->ioaddr + SDHCI_SOFTWARE_RESET);
	/* wait for 10 us */
	usleep_range_state(10, 20, TASK_UNINTERRUPTIBLE);

	val |= SDHCI_HW_RESET_CARD;
	writeb_relaxed(val, host->ioaddr + SDHCI_SOFTWARE_RESET);
	usleep_range_state(300, 500, TASK_UNINTERRUPTIBLE);

	pr_info("%s: %s end\n", mmc_hostname(host->mmc), __func__);
}

static unsigned int sdhci_sprd_get_max_timeout_count(struct sdhci_host *host)
{
	/* The Spredtrum controller actual maximum timeout count is 1 << 31 */
	return 1 << 31;
}

static unsigned int sdhci_sprd_get_ro(struct sdhci_host *host)
{
	return 0;
}

static void sdhci_sprd_request_done(struct sdhci_host *host,
				    struct mmc_request *mrq)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	trace_mmc_cmd_done(host->mmc, mrq);

	if (HOST_IS_EMMC_TYPE(host->mmc) && sdhci_sprd_should_force_error() &&
		mrq && mrq->cmd && mrq->data) {
		if (((mrq->cmd->opcode == MMC_READ_SINGLE_BLOCK) ||
			(mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK) ||
			(mrq->cmd->opcode == MMC_WRITE_BLOCK) ||
			(mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK) ||
			(mrq->cmd->opcode == MMC_EXECUTE_READ_TASK) ||
			(mrq->cmd->opcode == MMC_EXECUTE_WRITE_TASK))) {
			mrq->data->error = -EILSEQ;
			pr_info("%s: fill error to cmd:%d, mrq %p\n",
				mmc_hostname(host->mmc), mrq->cmd->opcode, mrq);
			sdhci_sprd_force_error(false);
		}
	}

	/* Validate if the request was from software queue firstly. */
	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq) {
		if (mmc_swcq_finalize_request(host->mmc, mrq))
			return;
	} else if (sprd_host->support_cqe) {
		mmc_request_done(host->mmc, mrq);
			return;
	} else {
		if (mmc_hsq_finalize_request(host->mmc, mrq))
			return;
	}

	mmc_request_done(host->mmc, mrq);
}

static void sdhci_sprd_set_dll_value(struct sdhci_host *host, u32 *delay_val, u32 opcode, u32 val,
					enum sdhci_sprd_tuning_type type)
{
	switch (type) {
	case SDHCI_SPRD_TUNING_SD_HS:
		if (opcode == MMC_SET_BLOCKLEN) {
			*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK);
			*delay_val |= ((val << 8) & SDHCI_SPRD_CMD_DLY_MASK);
		} else {
			*delay_val &= ~(SDHCI_SPRD_POSRD_DLY_MASK);
			*delay_val |= ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK);
		}
		break;
	case SDHCI_SPRD_TUNING_SD_UHS_CMD:
		*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK);
		*delay_val |= ((val << 8) & SDHCI_SPRD_CMD_DLY_MASK);
		break;
	case SDHCI_SPRD_TUNING_SD_UHS_DATA:
		*delay_val &= ~(SDHCI_SPRD_POSRD_DLY_MASK);
		*delay_val |= ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK);
		break;
	default:
		*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK | SDHCI_SPRD_POSRD_DLY_MASK);
		*delay_val |= (((val << 8) & SDHCI_SPRD_CMD_DLY_MASK) |
			 ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK));
		break;
	}

	sdhci_writel(host, *delay_val, SDHCI_SPRD_REG_32_DLL_DLY);
	pr_debug("%s: dll_dly 0x%08x\n", mmc_hostname(host->mmc), *delay_val);
}

static int sprd_calc_hs_mode_tuning_range(struct sdhci_sprd_host *host, int *value_t)
{
	int i;
	bool prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	for (i = 0; i < mid_dll_cnt; i++) {
		if ((!prev_vl) && value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt]) {
			ranges[range_count - 1].end = i;
			pr_debug("recalculate tuning ok: %d\n", i);
		} else
			pr_debug("recalculate tuning fail: %d\n", i);

		prev_vl = value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt];
	}

	host->ranges = ranges;

	return range_count;
}

static int sprd_calc_tuning_range(struct sdhci_sprd_host *host, int *value_t)
{
	int i;
	bool prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	/*
	 * first: 0 <= i < mid_dll_cnt
	 * tuning range: (0 ~ mid_dll_cnt) && (dll_cnt ~ dll_cnt + mid_dll_cnt)
	 *
	 * second: mid_dll_cnt <= i < dll_cnt
	 * tuning range: mid_dll_cnt ~ dll_cnt
	 */
	for (i = 0; i < dll_cnt; i++) {
		if (i < mid_dll_cnt)
			value_t[i] &= value_t[i + dll_cnt];

		if ((!prev_vl) && value_t[i]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i]) {
			ranges[range_count - 1].end = i;
			pr_debug("recalculate tuning ok: %d\n", i);
		} else
			pr_debug("recalculate tuning fail: %d\n", i);

		prev_vl = value_t[i];
	}

	return range_count;
}

static void sdhci_sprd_clk_autogate(struct sdhci_host *host, bool enable)
{
	u32 val, mask;

	val = sdhci_readl(host, SDHCI_SPRD_REG_32_BUSY_POSI);
	mask = SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN | SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN;

	if (enable) {
		if (mask != (val & mask)) {
			val |= mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	} else {
		if (val & mask) {
			val &= ~mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	}
}

static int sdhci_sprd_tuning(struct mmc_host *mmc, u32 opcode, enum sdhci_sprd_tuning_type type)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 *p = sprd_host->phy_delay;
	int err = 0;
	int i = 0;
	bool value, continuous;
	int *value_t;
	int length;
	unsigned int range_count = 0;
	int longest_range_len = 0;
	int longest_range = 0;
	int mid_step;
	int final_phase;
	u32 dll_cfg, mid_dll_cnt, dll_cnt, dll_dly;
	bool cfg_use_adma = false;
	bool cfg_use_sdma = false;
	u32 tmp;
	int cmd_error = 0;

	sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	dll_cfg = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);

	if (type == SDHCI_SPRD_TUNING_SD_HS)
		dll_cnt = 128;
	else
		dll_cnt = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0) & 0xff;

	/* dll lock is half mode by default after r11*/
	if ((sprd_host->ip_ver >= SDHCI_IP_VER_R11) || (type == SDHCI_SPRD_TUNING_SD_HS))
		dll_cnt = dll_cnt << 1;

	length = (dll_cnt * 150) / 100;
	pr_info("%s: dll config 0x%08x, dll count %d, tuning length: %d\n",
		mmc_hostname(mmc), dll_cfg, dll_cnt, length);

	sprd_host->ranges = kmalloc_array(length, sizeof(*sprd_host->ranges), GFP_KERNEL);
	if (!sprd_host->ranges)
		return -ENOMEM;
	value_t = kmalloc_array(length, sizeof(*value_t), GFP_KERNEL);
	if (!value_t) {
		kfree(sprd_host->ranges);
		sprd_host->ranges = NULL;
		return -ENOMEM;
	}

	dll_dly = p[mmc->ios.timing];
	if (type == SDHCI_SPRD_TUNING_SD_UHS_DATA) {
		/* if tuning uhs data, cmd delay value must in cpst_en disabled status */
		dll_dly &= ~SDHCI_SPRD_CMD_DLY_MASK;
		dll_dly |= (sprd_host->cpst_cmd_dly << 8);
		sprd_host->cpst_cmd_dly = 0;
	}

	/* config dma mode in tuning. */
	if (!cfg_use_adma && (host->flags & SDHCI_USE_ADMA) && !HOST_IS_EMMC_TYPE(mmc)) {
		cfg_use_adma = true;
		host->flags &= ~SDHCI_USE_ADMA;
		if (host->flags & SDHCI_USE_SDMA)
			cfg_use_sdma = true;
		host->flags |= SDHCI_USE_SDMA;
	}

	sprd_host->wait_read_idle = false;

	do {
		sprd_host->tuning_flag = 1;

		/*
		 * When the wait_read_idle flag is set, we need to waiting fo the HC state
		 * become idle before the next tuning commond is sent. The maximum polling
		 * time can be about 4s, which is recommended in Spreadtrum's platform.
		 */
		if (sprd_host->wait_read_idle) {
			if (read_poll_timeout(sdhci_readl, tmp, (!(tmp & SDHCI_DOING_READ) &&
			    ((tmp & SDHCI_DATA_LVL_MASK) == SDHCI_DATA_LVL_MASK)),
			    USEC_PER_MSEC, 4 * USEC_PER_SEC, false, host, SDHCI_PRESENT_STATE)) {
				pr_err("%s: wait host controller idle timeout, please check\n",
					mmc_hostname(mmc));
				err = -EIO;
				goto out;
			}
			sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
			sprd_host->wait_read_idle = false;
		}

		sprd_host_tuning_info_update_index(host, i);

		sdhci_sprd_set_dll_value(host, &dll_dly, opcode, i, type);

		if (type == SDHCI_SPRD_TUNING_SD_HS) {
			if (opcode == MMC_SET_BLOCKLEN)
				value = !mmc_send_tuning_cmd(mmc);
			else
				value = !mmc_send_tuning_read(mmc, sprd_host->tuning_data_buf);
		} else if (type == SDHCI_SPRD_TUNING_SD_UHS_CMD) {
			value = !mmc_send_tuning_cmd(mmc);
		} else {
			value = !mmc_send_tuning_pattern(mmc, opcode, &cmd_error,
							 sprd_host->tuning_data_buf);
		}

		/*
		 * if data tuning occur cmd error and previous cmd tuning all pass,
		 * it means the current cmd delay is not a good one.
		 * then reset cmd delay to 0
		 */
		if ((type == SDHCI_SPRD_TUNING_SD_UHS_DATA) && cmd_error) {
			pr_info("%s: cmd error %d occurs in data tuning, dly = 0x%x\n",
				mmc_hostname(mmc), cmd_error, dll_dly);
			if (sprd_host->cmd_dly_all_pass == true) {
				dll_dly &= ~SDHCI_SPRD_CMD_DLY_MASK;
				sdhci_sprd_set_dll_value(host, &p[mmc->ios.timing],
					MMC_SET_BLOCKLEN, 0, SDHCI_SPRD_TUNING_SD_UHS_CMD);

				i--;
				cmd_error = 0;
				sprd_host->tuning_flag = 0;
				sprd_host->cmd_dly_all_pass = false;
				continue;
			}
		}

		sprd_host->tuning_flag = 0;

		value_t[i] = value;
		if (value)
			pr_debug("%s tuning ok: %d\n", mmc_hostname(mmc), i);
		else
			pr_debug("%s tuning fail: %d\n", mmc_hostname(mmc), i);
	} while (++i < length);

	mid_dll_cnt = length - dll_cnt;
	sprd_host->dll_cnt = dll_cnt;
	sprd_host->mid_dll_cnt = mid_dll_cnt;

	if (type == SDHCI_SPRD_TUNING_SD_HS)
		range_count = sprd_calc_hs_mode_tuning_range(sprd_host, value_t);
	else
		range_count = sprd_calc_tuning_range(sprd_host, value_t);

	if (range_count == 0) {
		pr_warn("%s: all tuning phases fail!\n", mmc_hostname(mmc));
		sprd_host_tuning_info_dump(host);
		err = -EIO;
		goto out;
	}

	if (type == SDHCI_SPRD_TUNING_SD_HS) {
		if ((range_count > 1) && (sprd_host->ranges[range_count - 1].end == 127)
				&& (sprd_host->ranges[0].start == 0)) {
			sprd_host->ranges[0].start = sprd_host->ranges[range_count - 1].start;
			range_count--;
		}
	} else {
		continuous = value_t[0] && value_t[dll_cnt - 1];
		/* combine the head and tail tuning range if they're continuous */
		if ((range_count > 1) && continuous) {
			sprd_host->ranges[0].start = sprd_host->ranges[range_count - 1].start;
			range_count--;
		}
	}

	for (i = 0; i < range_count; i++) {
		int len = (sprd_host->ranges[i].end - sprd_host->ranges[i].start + 1);

		if (len < 0) {
			if (type == SDHCI_SPRD_TUNING_SD_HS)
				len += mid_dll_cnt;
			else
				len += dll_cnt;
		}

		pr_debug("%s: good tuning phase range %d ~ %d\n",
			 mmc_hostname(mmc), sprd_host->ranges[i].start, sprd_host->ranges[i].end);

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

	}
	pr_info("%s: the best tuning step range %d-%d(the length is %d)\n",
		mmc_hostname(mmc), sprd_host->ranges[longest_range].start,
		sprd_host->ranges[longest_range].end, longest_range_len);

	if ((longest_range_len == dll_cnt) && (type == SDHCI_SPRD_TUNING_SD_UHS_CMD))
		sprd_host->cmd_dly_all_pass = true;
	else
		sprd_host->cmd_dly_all_pass = false;

	/*
	 * If more than half of dll_cnt delay value is not suitable, we would assume that
	 * it is better for this SD card to tune the command and data signal separately.
	 */
	if (HOST_IS_SD_TYPE(mmc) && sprd_host->tuning_merged == true &&
		longest_range_len < dll_cnt / 2) {
		pr_err("%s: the best tuning range too short\n", mmc_hostname(mmc));
		err = -EIO;
		goto out;
	}

	mid_step = sprd_host->ranges[longest_range].start + longest_range_len / 2;
	mid_step %= dll_cnt;

	if (type == SDHCI_SPRD_TUNING_SD_UHS_CMD)
		sprd_host->cpst_cmd_dly = mid_step;

	if (type == SDHCI_SPRD_TUNING_SD_HS) {
		dll_cfg &= ~(0xf << 24);
		sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);
		final_phase = mid_step;
	} else {
		dll_cfg |= 0xf << 24;
		sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);
		final_phase = (mid_step * 256) / dll_cnt;
	}

	if (host->flags & SDHCI_HS400_TUNING) {
		p[MMC_TIMING_MMC_HS400] &= ~SDHCI_SPRD_CMD_DLY_MASK;
		p[MMC_TIMING_MMC_HS400] |= (final_phase << 8);
	}
	sdhci_sprd_set_dll_value(host, &p[mmc->ios.timing], opcode, final_phase, type);

	pr_info("%s: the best step %d, phase 0x%02x, delay value 0x%08x\n",
		mmc_hostname(mmc), mid_step, final_phase, p[mmc->ios.timing]);
	err = 0;

out:
	host->flags &= ~SDHCI_HS400_TUNING;

	if (cfg_use_adma)
		host->flags |= SDHCI_USE_ADMA;
	if (!cfg_use_sdma)
		host->flags &= ~SDHCI_USE_SDMA;

	if (err == -EIO)
		sdhci_writel(host, p[mmc->ios.timing], SDHCI_SPRD_REG_32_DLL_DLY);

	kfree(sprd_host->ranges);
	sprd_host->ranges = NULL;

	kfree(value_t);

	return err;
}

static int sdhci_sprd_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	enum sdhci_sprd_tuning_type type = SDHCI_SPRD_TUNING_DEFAULT;
	bool old_tm = sprd_host->tuning_merged;
	int err = 0;

	if (HOST_IS_EMMC_TYPE(mmc))
		sdhci_sprd_clk_autogate(host, false);
retry_tuning:
	/*
	 * For better compatibility with some SD Cards,
	 * if a sd card failed in tuning CMD and DATA line merged,
	 * it must tuning CMD Line and DATA Line separately afterwards
	 */
	if (HOST_IS_SD_TYPE(mmc)) {
		if (sprd_host->tuning_merged == false &&
			((mmc->ios.timing == MMC_TIMING_UHS_SDR104) ||
			 (mmc->ios.timing == MMC_TIMING_UHS_SDR50))) {
			type = SDHCI_SPRD_TUNING_SD_UHS_CMD;
		} else if (mmc->ios.timing == MMC_TIMING_SD_HS) {
			type = SDHCI_SPRD_TUNING_SD_HS;
			sprd_host->tuning_merged = false;
		}
	}

	err = sdhci_sprd_tuning(mmc, opcode, type);
	if (HOST_IS_SD_TYPE(mmc) && err && sprd_host->tuning_merged) {
		pr_err("%s: cmd and data tuning merged failed, afterwards tuning separately\n",
			mmc_hostname(mmc));
		sprd_host->tuning_merged = false;
		goto retry_tuning;
	} else if (type == SDHCI_SPRD_TUNING_SD_HS) {
		sprd_host->tuning_merged = old_tm;
	}

	if (!err && (type == SDHCI_SPRD_TUNING_SD_UHS_CMD)) {
		type = SDHCI_SPRD_TUNING_SD_UHS_DATA;
		err = sdhci_sprd_tuning(mmc, opcode, type);
	}

	if (HOST_IS_EMMC_TYPE(mmc))
		sdhci_sprd_clk_autogate(host, true);

	return err;
}

static void sdhci_sprd_sd_cmdline_tuning(void *data, struct mmc_card *card, int *err)
{
	if (card->host->ios.timing == MMC_TIMING_SD_HS) {
		*err = sdhci_sprd_execute_tuning(card->host, MMC_SET_BLOCKLEN);
		if (*err)
			pr_err("%s: high-speed cmd tuning failed\n",
				mmc_hostname(card->host));
	}
}

static void sdhci_sprd_sd_dataline_tuning(void *data, struct mmc_card *card, int *err)
{
	if (card->host->ios.timing == MMC_TIMING_SD_HS) {
		*err = sdhci_sprd_execute_tuning(card->host, MMC_SEND_TUNING_BLOCK);
		if (*err)
			pr_err("%s: high-speed data and cmd tuning failed\n",
				mmc_hostname(card->host));
	}
}

static void sdhci_sprd_fast_hotplug_disable(struct sdhci_sprd_host *sprd_host)
{
	regmap_update_bits(sprd_host->reg_protect_enable.regmap,
		sprd_host->reg_protect_enable.reg,
		sprd_host->reg_protect_enable.mask, 0);
}

static void sdhci_sprd_fast_hotplug_enable(struct sdhci_sprd_host *sprd_host)
{
	int debounce_counter = 3;
	u32 reg_value = 0;
	int ret = 0;

	if (sprd_host->reg_rmldo_en.regmap) {
		/* this register do not support update in bits */
		ret = regmap_read(sprd_host->reg_rmldo_en.regmap,
				sprd_host->reg_rmldo_en.reg,
				&reg_value);
		if (ret < 0) {
			pr_err("remap global register failed!\n");
			return;
		}
		reg_value |= sprd_host->reg_rmldo_en.mask;
		ret = regmap_write(sprd_host->reg_rmldo_en.regmap,
				sprd_host->reg_rmldo_en.reg,
				reg_value);
		if (ret < 0) {
			pr_err("remap global register failed!\n");
			return;
		}
	}

	regmap_update_bits(sprd_host->reg_protect_enable.regmap,
		sprd_host->reg_protect_enable.reg,
		sprd_host->reg_protect_enable.mask,
		sprd_host->reg_protect_enable.mask);
	regmap_update_bits(sprd_host->reg_debounce_en.regmap,
		sprd_host->reg_debounce_en.reg,
		sprd_host->reg_debounce_en.mask,
		sprd_host->reg_debounce_en.mask);
	regmap_update_bits(sprd_host->reg_debounce_cn.regmap,
		sprd_host->reg_debounce_cn.reg,
		sprd_host->reg_debounce_cn.mask,
		debounce_counter << mask_to_offset(sprd_host->reg_debounce_cn.mask));
	if (sprd_host->detect_gpio_polar)
		regmap_update_bits(sprd_host->reg_detect_polar.regmap,
			sprd_host->reg_detect_polar.reg,
			sprd_host->reg_detect_polar.mask, 0);
	else
		regmap_update_bits(sprd_host->reg_detect_polar.regmap,
			sprd_host->reg_detect_polar.reg,
			sprd_host->reg_detect_polar.mask,
			sprd_host->reg_detect_polar.mask);
}

static void sdhci_sprd_signal_voltage_on_off(struct sdhci_host *host,
	bool on_off)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	const char *name = mmc_hostname(host->mmc);

	if (IS_ERR(host->mmc->supply.vqmmc))
		return;

	if (on_off) {
		if (!sprd_host->vqmmc_enabled) {
			if (regulator_enable(host->mmc->supply.vqmmc))
				pr_err("%s: signal voltage enable fail!\n", name);
			else
				sprd_host->vqmmc_enabled = true;
		}
	} else {
		if (sprd_host->vqmmc_enabled) {
			if (regulator_disable(host->mmc->supply.vqmmc))
				pr_err("%s: signal voltage disable fail!\n", name);
			else
				sprd_host->vqmmc_enabled = false;
		}
	}
}

static void sdhci_sprd_set_power(struct sdhci_host *host, unsigned char mode,
	unsigned short vdd)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_host *mmc = host->mmc;
	int ret;

	if (sprd_host->power_mode == mode)
		return;

	switch (mode) {
	case MMC_POWER_OFF:
		/*
		 * disable hotplug configuration when sd card power off, in order to
		 * ensure power sequence by using software for the next power on.
		 */
		if (sprd_host->reg_protect_enable.regmap)
			sdhci_sprd_fast_hotplug_disable(sprd_host);

		if (!host->mmc_host_ops.get_cd(host->mmc)) {
			unsigned char old_signal_voltage = mmc->ios.signal_voltage;
			unsigned short old_vdd = mmc->ios.vdd;
			/*
			 * make sure io_voltage will keep 3.3V in next power up while plugin sd,
			 * but will not do this in deepsleep power off
			 */
			mmc->ios.signal_voltage = MMC_SIGNAL_VOLTAGE_330;
			mmc->ios.vdd = 18;
			ret = host->mmc_host_ops.start_signal_voltage_switch(mmc, &mmc->ios);
			if (ret)
				pr_err("%s: signal voltage set to 3.3v fail %d!\n",
				       mmc_hostname(host->mmc), ret);
			mmc->ios.vdd = old_vdd;
			mmc->ios.signal_voltage = old_signal_voltage;

			/* restore tuning cmd and data merged after card removed */
			sprd_host->tuning_merged = true;

			/* restore minimum data timeout parameter */
			sprd_host->min_data_timeout = 0;
		}

		sdhci_sprd_signal_voltage_on_off(host, false);
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(host->mmc, mmc->supply.vmmc, 0);
		break;
	case MMC_POWER_ON:
		sdhci_sprd_signal_voltage_on_off(host, true);

		if (sprd_host->reg_detect_polar.regmap && sprd_host->reg_protect_enable.regmap
			&& sprd_host->reg_detect_polar.regmap
			&& sprd_host->reg_protect_enable.regmap
			&& host->mmc_host_ops.get_cd(host->mmc))
			sdhci_sprd_fast_hotplug_enable(sprd_host);
		break;
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(host->mmc, mmc->supply.vmmc, vdd);
		break;
	}

	sprd_host->power_mode = mode;
}

static void sdhci_sprd_disable_sd_cache(struct sdhci_host *host)
{
	/*
	 * Although some sd cards support cache feature, they have problems when
	 * used in practice, so here we turn off the cache feature by modifying
	 * the data returned by the device.
	 */
	if ((sdhci_readl(host, SDHCI_ARGUMENT) == 0x100001ff) &&
		(SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND)) == SEND_SD_READ_EXTR_SINGLE) &&
		(sdhci_readl(host, SDHCI_SPRD_BLK_CNT_BUF) == 1)) {
		u64 buf_paddr;
		void *buf_vaddr;

		/*
		 * SD_READ_EXTR_SINGLE command's receiving data buffer's address can be
		 * obtained from the ADMA_BUF_PROCESS registers in Spreadtrum's platform,
		 * but it needs to shift 512 bytes ahead of that.
		 */
		buf_paddr = (u64)sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_BUF_PROCESS_H) << 32;
		buf_paddr |= sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_BUF_PROCESS_L);
		buf_paddr -= 0x200;

		buf_vaddr = __va(buf_paddr);
		*(u32 *)(buf_vaddr + 4) &= (u32)0xfffffffe;
	}
}

static void sdhci_sprd_dump_adma_info(struct sdhci_host *host)
{
	u64 desc_ptr;
	unsigned long start = jiffies;
	void *desc = host->adma_table;
	dma_addr_t dma = host->adma_addr;

	if (host->flags & SDHCI_USE_64_BIT_DMA) {
		desc_ptr = (u64)sdhci_sprd_readl(host, SDHCI_ADMA_ADDRESS_HI) << 32;
		desc_ptr |= sdhci_sprd_readl(host, SDHCI_ADMA_ADDRESS);
	} else {
		desc_ptr = sdhci_sprd_readl(host, SDHCI_ADMA_ADDRESS);
	}

	SDHCI_SPRD_DUMP("ADMA ADDRESS: %#08llx, ERROR: 0x%08x\n",
			desc_ptr, sdhci_sprd_readl(host, SDHCI_ADMA_ERROR));

	SDHCI_SPRD_DUMP("ADMA DEBUG0: 0x%08x, DEBUG1: 0x%08x, DEBUG2: 0x%08x\n",
			sdhci_sprd_readl(host, SPRD_SDHC_REG_EMMC_DEBUG0),
			sdhci_sprd_readl(host, SPRD_SDHC_REG_EMMC_DEBUG1),
			sdhci_sprd_readl(host, SPRD_SDHC_REG_EMMC_DEBUG2));

	while (desc_ptr) {
		struct sdhci_adma2_64_desc *dma_desc = desc;

		if (host->flags & SDHCI_USE_64_BIT_DMA)
			SDHCI_SPRD_DUMP("ADMA: %08llx: DMA 0x%08x%08x, LEN 0x%04x, Attr=0x%02x\n",
					 (unsigned long long)dma,
					 le32_to_cpu(dma_desc->addr_hi),
					 le32_to_cpu(dma_desc->addr_lo),
					 le16_to_cpu(dma_desc->len),
					 le16_to_cpu(dma_desc->cmd));
		else
			SDHCI_SPRD_DUMP("ADMA: %08llx: DMA 0x%08x, LEN 0x%04x, Attr=0x%02x\n",
					 (unsigned long long)dma,
					 le32_to_cpu(dma_desc->addr_lo),
					 le16_to_cpu(dma_desc->len),
					 le16_to_cpu(dma_desc->cmd));

		desc += host->desc_sz;
		dma += host->desc_sz;

		if (dma_desc->cmd & cpu_to_le16(ADMA2_END))
			break;

		if (time_after(jiffies, start + HZ)) {
			SDHCI_SPRD_DUMP("ADMA error have no end desc\n");
			break;
		}
	}

	desc_ptr = (u64)sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_PROCESS_H) << 32;
	desc_ptr |= sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_PROCESS_L);
	SDHCI_SPRD_DUMP("ADMA Current Process desc: %#08llx\n", desc_ptr);

	desc_ptr = (u64)sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_BUF_PROCESS_H) << 32;
	desc_ptr |= sdhci_sprd_readl(host, SDHCI_SPRD_ADMA_BUF_PROCESS_L);
	SDHCI_SPRD_DUMP("ADMA Current Buf Process desc: %#08llx\n", desc_ptr);
}

static void sdhci_sprd_dump_vendor_regs(struct sdhci_host *host)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 command, clk_ctrl, host_ctrl2, delay_value, int_sts;
	char sdhci_hostname[64];
	bool flag = true;

	if (sprd_host->tuning_flag)
		return;

	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq)
		host->mmc->cqe_ops->cqe_timeout(host->mmc, host->mmc->ongoing_mrq, &flag);

	command = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
	int_sts = sdhci_readl(host, SDHCI_INT_STATUS);
	clk_ctrl = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	host_ctrl2 = sdhci_readl(host, SDHCI_AUTO_CMD_STATUS);
	delay_value = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_DLY);
	SDHCI_SPRD_DUMP("CMD%d Error 0x%08x 0x%08x 0x%08x 0x%08x\n",
		command, int_sts, clk_ctrl, host_ctrl2, delay_value);

	if (!host->mmc->card)
		return;

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x000: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr, 96, 0);

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x200: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr + 0x200, 48, 0);

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x250: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr + 0x250, 32, 0);

	if (sprd_host->support_cqe) {
		sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
				": sprd-sdhci + 0x300: ");
		print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
				16, 4, host->ioaddr + 0x300, 128, 0);
	}

	if (int_sts & SDHCI_INT_ADMA_ERROR)
		sdhci_sprd_dump_adma_info(host);
}

static u32 sdhci_sprd_cqe_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

#ifndef CONFIG_SPRD_DEBUG
	if (HOST_IS_SD_TYPE(host->mmc))
		mmc_debug_update(host, NULL, intmask);
#endif

	sprd_host->int_status = intmask;

	if (intmask & SDHCI_INT_ERROR_MASK)
		sdhci_sprd_dump_vendor_regs(host);
	else if ((host->mmc->index == 1) && (intmask & SDHCI_INT_DATA_END))
		sdhci_sprd_disable_sd_cache(host);

	if (sprd_host->tuning_flag)
		sprd_host_tuning_info_update_intstatus(host);

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

void sdhci_sprd_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (sprd_host->support_cqe && (host->mmc->caps2 & MMC_CAP2_CQE) && (mask & SDHCI_RESET_ALL)
	    && host->mmc->cqe_private)
		cqhci_deactivate(host->mmc);

	sdhci_reset(host, mask);
}

static struct sdhci_ops sdhci_sprd_ops = {
	.read_l = sdhci_sprd_readl,
	.write_l = sdhci_sprd_writel,
	.write_b = sdhci_sprd_writeb,
	.write_w = sdhci_sprd_writew,
	.set_clock = sdhci_sprd_set_clock,
	.set_power = sdhci_sprd_set_power,
	.get_max_clock = sdhci_sprd_get_max_clock,
	.get_min_clock = sdhci_sprd_get_min_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_sprd_reset,
	.set_uhs_signaling = sdhci_sprd_set_uhs_signaling,
	.hw_reset = sdhci_sprd_hw_reset,
	.get_max_timeout_count = sdhci_sprd_get_max_timeout_count,
	.get_ro = sdhci_sprd_get_ro,
	.request_done = sdhci_sprd_request_done,
	.dump_vendor_regs = sdhci_sprd_dump_vendor_regs,
	.irq = sdhci_sprd_cqe_irq,
};

static void sdhci_sprd_check_auto_cmd23(struct mmc_host *mmc,
					struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	host->flags |= sprd_host->flags & SDHCI_AUTO_CMD23;

	/*
	 * From version 4.10 onward, ARGUMENT2 register is also as 32-bit
	 * block count register which doesn't support stuff bits of
	 * CMD23 argument on Spreadtrum's sd host controller.
	 */
	if (sprd_host->support_swcq) {
		if (host->version >= SDHCI_SPEC_410 &&
		    mrq->sbc && ((mrq->sbc->arg & SDHCI_SPRD_ARG2_STUFF) ||
		    (host->mmc->card && mmc_card_cmdq(host->mmc->card))) &&
		    (host->flags & SDHCI_AUTO_CMD23))
			host->flags &= ~SDHCI_AUTO_CMD23;
	} else {
		if (host->version >= SDHCI_SPEC_410 &&
		    mrq->sbc && (mrq->sbc->arg & SDHCI_SPRD_ARG2_STUFF) &&
		    (host->flags & SDHCI_AUTO_CMD23))
			host->flags &= ~SDHCI_AUTO_CMD23;
	}
}

static void sdhci_sprd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_swcq *swcq = mmc->cqe_private;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

#ifndef CONFIG_SPRD_DEBUG
	if (HOST_IS_SD_TYPE(mmc))
		mmc_debug_update(host, mrq->cmd, 0);
#endif

	sdhci_sprd_check_auto_cmd23(mmc, mrq);
	if (HOST_IS_EMMC_TYPE(mmc) && sprd_host->support_swcq) {
		if (!swcq->need_polling) {
			sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
			if (mrq->cmd)
				dbg_add_host_log(mmc, MMC_SEND_CMD, mrq->cmd->opcode,
					mrq->cmd->arg, mrq);
		} else {
			sdhci_sprd_request_sync(mmc, mrq);
			return;
		}
	}

	sdhci_request(mmc, mrq);
}

static struct mmc_request *get_mrq_in_swcq(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_request *p_mrq = NULL;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_swcq *swcq = mmc->cqe_private;
	int index;

	if (!sprd_host->support_swcq || !swcq->cmdq_mode)
		return p_mrq;

	for (index = 0; index < swcq->cmdq_depth; index++) {
		if (swcq->cmdq_slot[index].ext_mrq == mrq) {
			p_mrq = swcq->cmdq_slot[index].mrq;
			break;
		}
	}

	return p_mrq;
}

static int sdhci_sprd_request_atomic(struct mmc_host *mmc,
				      struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_blk_data *main_md = dev_get_drvdata(&mmc->card->dev);
	struct mmc_request *p_mrq = get_mrq_in_swcq(mmc, mrq);

	sdhci_sprd_check_auto_cmd23(mmc, mrq);

	trace_mmc_cmd_send(mmc, main_md->disk, mrq, p_mrq);

	if (sprd_host->support_swcq) {
		if (HOST_IS_EMMC_TYPE(mmc) && mmc->card && mmc_card_cmdq(mmc->card))
			return sdhci_sprd_request_sync(mmc, mrq);
		return _sdhci_request_atomic(mmc, mrq);
	} else {
		return sdhci_request_atomic(mmc, mrq);
	}
}

static int sdhci_sprd_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	int ret;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			pr_err("%s: Switching signalling voltage failed, ret = %d\n",
				mmc_hostname(mmc), ret);
			return ret;
		}
	}

	if (IS_ERR(sprd_host->pinctrl))
		/*
		 * If pinctrl not defined in dts, still need reset here because
		 * voltage have changed. Otherwise the controller will not work
		 * normally. We had ever encoutered CMD2 timeout before.
		 */
		goto reset;

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_180:
		ret = pinctrl_select_state(sprd_host->pinctrl,
					   sprd_host->pins_uhs);
		if (ret) {
			pr_err("%s: failed to select uhs pin state\n",
			       mmc_hostname(mmc));
			return ret;
		}
		break;

	default:
		fallthrough;
	case MMC_SIGNAL_VOLTAGE_330:
		ret = pinctrl_select_state(sprd_host->pinctrl,
					   sprd_host->pins_default);
		if (ret) {
			pr_err("%s: failed to select default pin state\n",
			       mmc_hostname(mmc));
			return ret;
		}
		break;
	}

	/* Wait for 300 ~ 500 us for pin state stable */
	usleep_range_state(300, 500, TASK_UNINTERRUPTIBLE);

reset:
	sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	return 0;
}

static void sdhci_sprd_hs400_enhanced_strobe(struct mmc_host *mmc,
					     struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	u16 ctrl_2;

	if (!ios->enhanced_strobe)
		return;

	sdhci_sprd_sd_clk_off(host);

	/* Set HS400 enhanced strobe mode */
	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	ctrl_2 |= SDHCI_SPRD_CTRL_HS400ES;
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	sdhci_sprd_sd_clk_on(host);
}

/* Parse delay which configured in dts and store */
static void sdhci_sprd_phy_param_parse(struct sdhci_sprd_host *sprd_host,
				       struct device_node *np)
{
	u32 *p = sprd_host->phy_delay;
	int ret, i;
	u32 val[4];

	for (i = 0; i < ARRAY_SIZE(sdhci_sprd_phy_cfgs); i++) {
		ret = of_property_read_u32_array(np,
				sdhci_sprd_phy_cfgs[i].property, val, 4);
		if (ret)
			continue;

		p[i] = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
	}
}

static const struct sdhci_pltfm_data sdhci_sprd_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_MISSING_CAPS,
	.quirks2 = SDHCI_QUIRK2_BROKEN_HS200 |
		   SDHCI_QUIRK2_USE_32BIT_BLK_CNT |
		   SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.ops = &sdhci_sprd_ops,
};

static void sdhci_sprd_get_fast_hotplug_reg(struct device_node *np,
	struct register_hotplug *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];

	regmap = syscon_regmap_lookup_by_phandle_args(np, name, 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_warn("read sdio fast hotplug %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		goto out;
	} else {
		reg->regmap = regmap;
		reg->reg = syscon_args[0];
		reg->mask = syscon_args[1];
	}

out:
	of_node_put(np);
}

static void sdhci_sprd_get_fast_hotplug_info(struct device_node *np,
	struct sdhci_sprd_host *sprd_host)
{
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_detect_polar,
		"sd-detect-pol-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_protect_enable,
		"sd-hotplug-protect-en-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_debounce_en,
		"sd-hotplug-debounce-en-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_debounce_cn,
		"sd-hotplug-debounce-cn-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_rmldo_en,
		"sd-hotplug-rmldo-en-syscon");
}

static int sdhci_sprd_ice_init(struct sdhci_host *host,
				  struct cqhci_host *cq_host)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct device *dev = mmc_dev(mmc);
	struct resource *res;

	if (!(cqhci_readl(cq_host, CQHCI_CAP) & CQHCI_CAP_CS)) {
		dev_warn(dev, "CQE no supprots ICE\n");
		return 0;
	}

	if (!sprd_host->support_ice)
		goto disable;

	res = platform_get_resource_byname(sprd_host->pdev, IORESOURCE_MEM, "ice");
	if (!res) {
		dev_warn(dev, "ICE registers not found\n");
		goto disable;
	}

	sprd_host->ice_mem = devm_ioremap_resource(dev, res);
	if (IS_ERR(sprd_host->ice_mem))
		return PTR_ERR(sprd_host->ice_mem);

	mmc->caps2 |= MMC_CAP2_CRYPTO;

	return 0;

disable:
	dev_warn(dev, "Disabling inline encryption support\n");

	return 0;
}

static void sdhci_sprd_cqe_write_l(struct cqhci_host *host, u32 val, int reg)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(mmc));

	if (reg >= CQHCI_CCAP) {
		writel(val, sprd_host->ice_mem + reg);
	} else {
		/* Prevent TCL int storms. */
		if ((reg == CQHCI_ISGE) && (val & CQHCI_IS_TCL))
			val &= ~CQHCI_IS_TCL;
		writel(val, host->mmio + reg);
	}

	/* Ensure all writes are done before interrupts are enabled */
	wmb();
}

static u32 sdhci_sprd_cqe_read_l(struct cqhci_host *host, int reg)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(mmc));

	if (reg >= CQHCI_CCAP)
		return readl(sprd_host->ice_mem + reg);

	return readl(host->mmio + reg);
}

static void sdhci_sprd_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u8 ctrl;

	sdhci_cqe_enable(mmc);

	spin_lock_irqsave(&host->lock, flags);

	/* Make sure that controller DMA is SDMA mode when using CQE */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	ctrl &= ~SDHCI_CTRL_DMA_MASK;
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	spin_unlock_irqrestore(&host->lock, flags);
}

static void sdhci_sprd_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 ctrl;

	/*
	 * When CQE is halted, the legacy SDHCI path operates only
	 * on 16-byte descriptors in 64bit mode.
	 */
	if (host->flags & SDHCI_USE_64_BIT_DMA)
		host->desc_sz = 16;

	spin_lock_irqsave(&host->lock, flags);

	/*
	 * During CQE command transfers, command complete bit gets latched.
	 * So s/w should clear command complete interrupt status when CQE is
	 * either halted or disabled. Otherwise unexpected SDCHI legacy
	 * interrupt gets triggered when CQE is halted/disabled.
	 */
	ctrl = sdhci_readl(host, SDHCI_INT_ENABLE);
	ctrl |= SDHCI_INT_RESPONSE;
	sdhci_writel(host,  ctrl, SDHCI_INT_ENABLE);
	sdhci_writel(host, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

	spin_unlock_irqrestore(&host->lock, flags);

	sdhci_cqe_disable(mmc, recovery);
}

static void sdhci_sprd_cqe_dump_vendor_regs(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);

	sdhci_sprd_dump_vendor_regs(host);
}

static void sdhci_sprd_cqe_pre_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static void sdhci_sprd_cqe_post_disable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg &= ~CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static const struct cqhci_host_ops sdhci_sprd_cqhci_ops = {
	.write_l = sdhci_sprd_cqe_write_l,
	.read_l = sdhci_sprd_cqe_read_l,
	.enable = sdhci_sprd_cqe_enable,
	.disable = sdhci_sprd_cqe_disable,
	.dumpregs = sdhci_sprd_cqe_dump_vendor_regs,
	.pre_enable = sdhci_sprd_cqe_pre_enable,
	.post_disable = sdhci_sprd_cqe_post_disable,
};

static int sdhci_sprd_cqe_add_host(struct sdhci_host *host,
				struct platform_device *pdev)
{
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	cq_host = cqhci_pltfm_init(pdev);
	if (IS_ERR(cq_host)) {
		ret = PTR_ERR(cq_host);
		dev_err(&pdev->dev, "cqhci-pltfm init: failed: %d\n", ret);
		goto cleanup;
	}

	sprd_host->cqe_mem = cq_host->mmio;
	host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
	cq_host->ops = &sdhci_sprd_cqhci_ops;
	cq_host->mmc = host->mmc;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;

	ret = sdhci_sprd_ice_init(host, cq_host);
	if (ret) {
		dev_err(&pdev->dev, "%s: ICE init: failed (%d)\n",
				mmc_hostname(host->mmc), ret);
		goto cleanup;
	}

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret) {
		dev_err(&pdev->dev, "%s: CQE init: failed (%d)\n",
				mmc_hostname(host->mmc), ret);
		goto cleanup;
	}

	cq_host->caps |= CQHCI_TASK_DESC_SZ_128;

	/* Enable force hw reset during cqe recovery */
	host->mmc->cqe_recovery_reset_always = true;

	dev_info(&pdev->dev, "%s: CQE init: success\n", mmc_hostname(host->mmc));

cleanup:
	return ret;
}

#if !IS_ENABLED(CONFIG_MMC_CQHCI)
int cqhci_resume(struct mmc_host *mmc)
{
	return -EINVAL;
}

int cqhci_deactivate(struct mmc_host *mmc)
{
	return -EINVAL;
}

int cqhci_init(struct cqhci_host *cq_host, struct mmc_host *mmc,
	      bool dma64)
{
	return -EINVAL;
}

struct cqhci_host *cqhci_pltfm_init(struct platform_device *pdev)
{
	return ERR_PTR(-EINVAL);
}

irqreturn_t cqhci_irq(struct mmc_host *mmc, u32 intmask, int cmd_error,
		      int data_error)
{
	return IRQ_HANDLED;
}
#endif

/*
 * Vendor hook can only be registered once.
 */
static void sdhci_sprd_register_vendor_hook(struct sdhci_host *host)
{
	if (HOST_IS_SD_TYPE(host->mmc)) {
		register_trace_android_rvh_mmc_sd_cmdline_timing(
			sdhci_sprd_sd_cmdline_tuning,
			NULL);
		register_trace_android_rvh_mmc_sd_dataline_timing(
			sdhci_sprd_sd_dataline_tuning,
			NULL);
	}
	if (HOST_IS_EMMC_TYPE(host->mmc)) {
		register_trace_android_rvh_mmc_partition_status(
			sdhci_sprd_extra_proc,
			NULL);
	}
}

static bool queue_flag;
static void mmc_hsq_status(void *data, const struct blk_mq_queue_data *bd, int *ret)
{
	struct request *req = bd->rq;
	struct request_queue *q = req->q;
	struct mmc_queue *mq = q->queuedata;
	struct mmc_card *card = mq->card;
	struct mmc_host *mmc = card->host;

	*ret = 0;

	if (!queue_flag && HOST_IS_EMMC_TYPE(mmc)) {
		blk_queue_flag_set(QUEUE_FLAG_SAME_FORCE, q);
		q->limits.discard_granularity = card->pref_erase << 9;
		q->limits.max_hw_discard_sectors = UINT_MAX;
		q->limits.max_discard_sectors = UINT_MAX;
		queue_flag = true;
	}

	if (HOST_IS_SD_TYPE(mmc)) {
		q->limits.discard_granularity = card->pref_erase << 9;
		q->limits.max_hw_discard_sectors = UINT_MAX;
		q->limits.max_discard_sectors = UINT_MAX;
	}

	req->cmd_flags &= ~REQ_FUA;
}

struct mmc_host *g_mmc_host[3];

static void sdhci_sprd_record_mmc_host(struct mmc_host *mmc)
{
	int index = mmc->index;

	if (mmc->index > 2)
		return;

	g_mmc_host[index] = mmc;
}

static irqreturn_t sdhci_sprd_cd_irqt(int irq, void *dev_id)
{
	struct mmc_host *host = dev_id;
	struct mmc_gpio *ctx = host->slot.handler_priv;
	bool allow = true;

	/* reset sd_hotplug_cnt when card is removed */
	sd_hotplug_cnt = 0;
	/* recover rescan_disable */
	if (anomaly_sd_remove_flag == true && host->rescan_disable == 1) {
		host->rescan_disable = 0;
		anomaly_sd_remove_flag = false;
	}

	trace_android_vh_mmc_gpio_cd_irqt(host, &allow);
	if (!allow)
		return IRQ_HANDLED;

	host->trigger_card_event = true;
	mmc_detect_change(host, msecs_to_jiffies(ctx->cd_debounce_delay_ms));
	return IRQ_HANDLED;
}

/* Register an alternate interrupt service routine for
 * the card-detect GPIO.
 */
static void sprd_mmc_gpio_set_cd_isr(struct sdhci_host *host,
				 irqreturn_t (*isr)(int irq, void *dev_id))
{
	struct mmc_gpio *ctx = host->mmc->slot.handler_priv;

	WARN_ON(ctx->cd_gpio_isr);
	ctx->cd_gpio_isr = isr;
}

static int sdhci_sprd_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	enum of_gpio_flags flags;
	struct sdhci_host *host;
	struct sdhci_sprd_host *sprd_host;
	struct mmc_hsq *hsq;
	struct clk *clk;
	int ret = 0;

	host = sdhci_pltfm_init(pdev, &sdhci_sprd_pdata, sizeof(*sprd_host));
	if (IS_ERR(host))
		return PTR_ERR(host);

	sdhci_sprd_record_mmc_host(host->mmc);

	host->dma_mask = DMA_BIT_MASK(64);
	pdev->dev.dma_mask = &host->dma_mask;
	host->mmc_host_ops.request = sdhci_sprd_request;
	host->mmc_host_ops.hs400_enhanced_strobe =
		sdhci_sprd_hs400_enhanced_strobe;
	host->mmc_host_ops.init_card = sdhci_sprd_init_card;
	/*
	 * We can not use the standard ops to change and detect the voltage
	 * signal for Spreadtrum SD host controller, since our voltage regulator
	 * for I/O is fixed in hardware, that means we do not need control
	 * the standard SD host controller to change the I/O voltage.
	 */
	host->mmc_host_ops.start_signal_voltage_switch =
		sdhci_sprd_voltage_switch;
	host->mmc_host_ops.execute_tuning = sdhci_sprd_execute_tuning;

	host->mmc->caps = MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED |
		MMC_CAP_WAIT_WHILE_BUSY;

	ret = mmc_of_parse(host->mmc);
	if (ret)
		goto pltfm_free;

	if (!mmc_card_is_removable(host->mmc))
		host->mmc_host_ops.request_atomic = sdhci_sprd_request_atomic;
	else
		host->always_defer_done = true;

	sprd_host = TO_SPRD_HOST(host);
	sdhci_sprd_phy_param_parse(sprd_host, pdev->dev.of_node);

	sprd_host->pdev = pdev;

	sprd_host->tuning_info = &sprd_tuning_info[host->mmc->index];

	sprd_host->tuning_data_buf = kzalloc(512, GFP_KERNEL);
	if (!sprd_host->tuning_data_buf) {
		ret = -ENOMEM;
		goto pltfm_free;
	}

	sprd_host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR(sprd_host->pinctrl)) {
		sprd_host->pins_uhs =
			pinctrl_lookup_state(sprd_host->pinctrl, "state_uhs");
		if (IS_ERR(sprd_host->pins_uhs)) {
			ret = PTR_ERR(sprd_host->pins_uhs);
			goto pltfm_free;
		}

		sprd_host->pins_default =
			pinctrl_lookup_state(sprd_host->pinctrl, "default");
		if (IS_ERR(sprd_host->pins_default)) {
			ret = PTR_ERR(sprd_host->pins_default);
			goto pltfm_free;
		}
	}

	sprd_host->detect_gpio = of_get_named_gpio_flags(np, "cd-gpios", 0, &flags);
	if (!gpio_is_valid(sprd_host->detect_gpio))
		sprd_host->detect_gpio = -1;
	else {
		sdhci_sprd_get_fast_hotplug_info(np, sprd_host);
		sprd_host->detect_gpio_polar = flags;
	}

	clk = devm_clk_get(&pdev->dev, "sdio");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto pltfm_free;
	}
	sprd_host->clk_sdio = clk;
	sprd_host->base_rate = clk_get_rate(sprd_host->clk_sdio);
	if (!sprd_host->base_rate) {
		sprd_host->base_rate = SDHCI_SPRD_CLK_DEF_RATE;
		pr_err("%s: base rate is 0, please check clk source\n",
			mmc_hostname(host->mmc));
	}

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[0] = sched_clock();
#endif

	if (of_property_read_bool(np, "supports-swcq")) {
		sprd_host->support_swcq = true;
	} else {
		sprd_host->support_swcq = false;
		if (of_property_read_bool(np, "supports-cqe")) {
			sprd_host->support_cqe = true;
			if (of_property_read_bool(np, "supports-ice"))
				sprd_host->support_ice = true;
			else
				sprd_host->support_ice = false;
		} else {
			sprd_host->support_cqe = false;
			sprd_host->support_ice = false;
		}
	}

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[1] = sched_clock();
#endif

	clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto pltfm_free;
	}
	sprd_host->clk_enable = clk;

	clk = devm_clk_get(&pdev->dev, "1x_enable");
	if (!IS_ERR(clk))
		sprd_host->clk_1x_enable = clk;

	clk = devm_clk_get(&pdev->dev, "2x_enable");
	if (!IS_ERR(clk))
		sprd_host->clk_2x_enable = clk;

	ret = clk_prepare_enable(sprd_host->clk_sdio);
	if (ret)
		goto pltfm_free;

	ret = clk_prepare_enable(sprd_host->clk_enable);
	if (ret)
		goto clk_sdio_disable;

	ret = clk_prepare_enable(sprd_host->clk_1x_enable);
	if (ret)
		goto clk_disable;

	ret = clk_prepare_enable(sprd_host->clk_2x_enable);
	if (ret)
		goto clk_1x_disable;

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[2] = sched_clock();
#endif

	sdhci_sprd_init_config(host);
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
	sprd_host->version = ((host->version & SDHCI_SPEC_VER_MASK) >>
			       SDHCI_SPEC_VER_SHIFT);

	if (of_device_is_compatible(np, "sprd,sdhci-r10"))
		sprd_host->ip_ver = SDHCI_IP_VER_R10;
	else
		sprd_host->ip_ver = SDHCI_IP_VER_R11;

	sprd_host->power_mode = MMC_POWER_UNDEFINED;

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[3] = sched_clock();
#endif

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	sdhci_enable_v4_mode(host);

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[4] = sched_clock();
#endif

	/*
	 * Supply the existing CAPS, but clear the UHS-I modes. This
	 * will allow these modes to be specified only by device
	 * tree properties through mmc_of_parse().
	 */
	host->caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	host->caps1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
	host->caps1 &= ~(SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 |
			 SDHCI_SUPPORT_DDR50);

	if (HOST_IS_SD_TYPE(host->mmc)) {
		ret = mmc_regulator_get_supply(host->mmc);
		if (ret)
			goto pm_runtime_disable;
	}

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[5] = sched_clock();
#endif

	ret = sdhci_setup_host(host);
	if (ret)
		goto pm_runtime_disable;

	host->mmc->ocr_avail = 0x40000;
	host->mmc->ocr_avail_sdio = host->mmc->ocr_avail;
	host->mmc->ocr_avail_sd = host->mmc->ocr_avail;
	host->mmc->ocr_avail_mmc = host->mmc->ocr_avail;

	host->mmc->max_current_330 = SDHCI_SPRD_MAX_CUR;
	host->mmc->max_current_300 = SDHCI_SPRD_MAX_CUR;
	host->mmc->max_current_180 = SDHCI_SPRD_MAX_CUR;

	host->mmc->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;

	sprd_host->flags = host->flags;
	sprd_host->tuning_merged = true;

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[6] = sched_clock();
#endif

	if (sprd_host->support_swcq) {
		ret = mmc_hsq_swcq_init(host, pdev);
		if (ret)
			goto err_cleanup_host;
	} else if (sprd_host->support_cqe) {
		ret = sdhci_sprd_cqe_add_host(host, pdev);
		if (ret)
			goto err_cleanup_host;
	} else {
		if (HOST_IS_EMMC_TYPE(host->mmc)) {
			register_trace_android_vh_mmc_check_status(mmc_hsq_status, NULL);
			queue_flag = false;
		}
		hsq = devm_kzalloc(&pdev->dev, sizeof(*hsq), GFP_KERNEL);
		if (!hsq) {
			ret = -ENOMEM;
			goto err_cleanup_host;
		}

		ret = mmc_hsq_init(hsq, host->mmc);
		if (ret)
			goto err_cleanup_host;
	}


#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[7] = sched_clock();
#endif

	if (HOST_IS_SD_TYPE(host->mmc))
		sprd_mmc_gpio_set_cd_isr(host, sdhci_sprd_cd_irqt);

	ret = __sdhci_add_host(host);
	if (ret)
		goto err_cleanup_host;

	if (sprd_host->support_swcq) {
		ret = sdhci_sprd_irq_request_swcq(host);
		if (ret)
			goto err_cleanup_host;
	}

	if (!mmc_check_wp_fn(host->mmc)) {
		ret = mmc_wp_init(host->mmc);
		if (ret)
			goto err_cleanup_host;
	}

	if (!of_property_read_bool(np, "no-ffu")) {
		ret = mmc_ffu_init(host);
		if (ret)
			goto err_cleanup_host;
	}

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[8] = sched_clock();
#endif

	/* disable polling scan for sdiocard */
	if ((host->mmc->caps2 & MMC_CAP2_NO_SD)
			&& (host->mmc->caps2 & MMC_CAP2_NO_MMC)) {
		host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
	}

	sdhci_sprd_register_vendor_hook(host);

	sdhci_sprd_add_host_debugfs(host);
	sdhci_sprd_debug_init(host);

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

#ifdef CONFIG_SPRD_DEBUG
	sprd_host->timestamp[9] = sched_clock();
#endif

	return 0;

err_cleanup_host:
	sdhci_cleanup_host(host);

pm_runtime_disable:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	clk_disable_unprepare(sprd_host->clk_2x_enable);

clk_1x_disable:
	clk_disable_unprepare(sprd_host->clk_1x_enable);

clk_disable:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_sdio_disable:
	clk_disable_unprepare(sprd_host->clk_sdio);

pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static int sdhci_sprd_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (!mmc_check_wp_fn(host->mmc))
		mmc_wp_remove(host->mmc);

	sdhci_sprd_del_debug_timer_sync(host);

	mmc_ffu_remove(host);
	sdhci_remove_host(host, 0);

	clk_disable_unprepare(sprd_host->clk_sdio);
	clk_disable_unprepare(sprd_host->clk_enable);
	clk_disable_unprepare(sprd_host->clk_2x_enable);
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	sdhci_pltfm_free(pdev);

	return 0;
}

static const struct of_device_id sdhci_sprd_of_match[] = {
	{ .compatible = "sprd,sdhci-r10", },
	{ .compatible = "sprd,sdhci-r11", },
	{ }
};
MODULE_DEVICE_TABLE(of, sdhci_sprd_of_match);

#ifdef CONFIG_PM
static int sdhci_sprd_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq)
		mmc_swcq_suspend(host->mmc);
	else if (sprd_host->support_cqe)
		cqhci_suspend(host->mmc);
	else
		mmc_hsq_suspend(host->mmc);

	sdhci_runtime_suspend_host(host);

	clk_disable_unprepare(sprd_host->clk_sdio);
	clk_disable_unprepare(sprd_host->clk_enable);
	clk_disable_unprepare(sprd_host->clk_2x_enable);
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	return 0;
}

static int sdhci_sprd_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	int ret;

	ret = clk_prepare_enable(sprd_host->clk_1x_enable);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sprd_host->clk_2x_enable);
	if (ret)
		goto clk_1x_disable;

	ret = clk_prepare_enable(sprd_host->clk_enable);
	if (ret)
		goto clk_2x_disable;

	ret = clk_prepare_enable(sprd_host->clk_sdio);
	if (ret)
		goto clk_disable;

	sdhci_runtime_resume_host(host, 1);

	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq)
		mmc_swcq_resume(host->mmc);
	else if (sprd_host->support_cqe)
		cqhci_resume(host->mmc);
	else
		mmc_hsq_resume(host->mmc);

	return 0;

clk_disable:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_2x_disable:
	clk_disable_unprepare(sprd_host->clk_2x_enable);

clk_1x_disable:
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	return ret;
}
#endif

static const struct dev_pm_ops sdhci_sprd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(sdhci_sprd_runtime_suspend,
			   sdhci_sprd_runtime_resume, NULL)
};

static struct platform_driver sdhci_sprd_driver = {
	.probe = sdhci_sprd_probe,
	.remove = sdhci_sprd_remove,
	.driver = {
		.name = "sdhci_sprd_r11",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_sprd_of_match,
		.pm = &sdhci_sprd_pm_ops,
	},
};
module_platform_driver(sdhci_sprd_driver);

MODULE_DESCRIPTION("Spreadtrum sdio host controller r11 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sdhci-sprd-r11");
