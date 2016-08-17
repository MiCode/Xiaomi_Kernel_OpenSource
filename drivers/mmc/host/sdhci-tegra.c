/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <asm/gpio.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/reboot.h>
#include <linux/devfreq.h>

#include <mach/gpio-tegra.h>
#include <mach/sdhci.h>
#include <mach/pinmux.h>
#include <mach/clk.h>

#include "sdhci-pltfm.h"

#define SDHCI_VNDR_CLK_CTRL	0x100
#define SDHCI_VNDR_CLK_CTRL_SDMMC_CLK	0x1
#define SDHCI_VNDR_CLK_CTRL_PADPIPE_CLKEN_OVERRIDE	0x8
#define SDHCI_VNDR_CLK_CTRL_SPI_MODE_CLKEN_OVERRIDE	0x4
#define SDHCI_VNDR_CLK_CNTL_INPUT_IO_CLK		0x2
#define SDHCI_VNDR_CLK_CTRL_BASE_CLK_FREQ_SHIFT	8
#define SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT	16
#define SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT	24
#define SDHCI_VNDR_CLK_CTRL_SDR50_TUNING		0x20

#define SDHCI_VNDR_MISC_CTRL		0x120
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SDR104_SUPPORT	0x8
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SDR50_SUPPORT	0x10
#define SDHCI_VNDR_MISC_CTRL_ENABLE_DDR50_SUPPORT	0x200
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SD_3_0	0x20
#define SDHCI_VNDR_MISC_CTRL_INFINITE_ERASE_TIMEOUT	0x1

#define SDMMC_SDMEMCOMPPADCTRL	0x1E0
#define SDMMC_SDMEMCOMPPADCTRL_VREF_SEL_MASK	0xF

#define SDMMC_AUTO_CAL_CONFIG	0x1E4
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_START	0x80000000
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE	0x20000000
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT	0x8
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET	0x70
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PU_OFFSET	0x62

#define SDMMC_AUTO_CAL_STATUS	0x1EC
#define SDMMC_AUTO_CAL_STATUS_AUTO_CAL_ACTIVE	0x80000000
#define SDMMC_AUTO_CAL_STATUS_PULLDOWN_OFFSET	24
#define PULLUP_ADJUSTMENT_OFFSET	20

#define SDHOST_1V8_OCR_MASK	0x8
#define SDHOST_HIGH_VOLT_MIN	2700000
#define SDHOST_HIGH_VOLT_MAX	3600000
#define SDHOST_HIGH_VOLT_2V8	2800000
#define SDHOST_LOW_VOLT_MIN	1800000
#define SDHOST_LOW_VOLT_MAX	1800000

#define MAX_DIVISOR_VALUE	128
#define DEFAULT_SDHOST_FREQ	50000000

#define MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_8	128
#define MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_4	64
#define MAX_TAP_VALUES	255
#define TUNING_FREQ_COUNT	3
#define TUNING_VOLTAGES_COUNT	2
#define TUNING_RETRIES	1
#define SDMMC_AHB_MAX_FREQ	150000000
#define SDMMC_EMC_MAX_FREQ	150000000

#define TAP_CMD_SUSPEND_CONTROLLER	0
#define TAP_CMD_TRIM_DEFAULT_VOLTAGE	1
#define TAP_CMD_TRIM_HIGH_VOLTAGE	2
#define TAP_CMD_GO_COMMAND		3


static unsigned int uhs_max_freq_MHz[] = {
	[MMC_TIMING_UHS_SDR50] = 100,
	[MMC_TIMING_UHS_SDR104] = 208,
	[MMC_TIMING_MMC_HS200] = 200,
};

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
static void tegra_3x_sdhci_set_card_clock(struct sdhci_host *sdhci, unsigned int clock);
#endif

static unsigned long get_nearest_clock_freq(unsigned long pll_rate,
		unsigned long desired_rate);
static void sdhci_tegra_set_tap_delay(struct sdhci_host *sdhci,
	unsigned int tap_delay);

struct tegra_sdhci_hw_ops {
	/* Set the internal clk and card clk.*/
	void	(*set_card_clock)(struct sdhci_host *sdhci, unsigned int clock);
};

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
static struct tegra_sdhci_hw_ops tegra_2x_sdhci_ops = {
};
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
static struct tegra_sdhci_hw_ops tegra_3x_sdhci_ops = {
	.set_card_clock = tegra_3x_sdhci_set_card_clock,
};
#else
static struct tegra_sdhci_hw_ops tegra_11x_sdhci_ops = {
};
#endif

/* Erratum: Version register is invalid in HW */
#define NVQUIRK_FORCE_SDHCI_SPEC_200		BIT(0)
/* Erratum: Enable block gap interrupt detection */
#define NVQUIRK_ENABLE_BLOCK_GAP_DET		BIT(1)
/* Do not enable auto calibration if the platform doesn't support */
#define NVQUIRK_DISABLE_AUTO_CALIBRATION	BIT(2)
/* Set Calibration Offsets */
#define NVQUIRK_SET_CALIBRATION_OFFSETS		BIT(3)
/* Set Drive Strengths */
#define NVQUIRK_SET_DRIVE_STRENGTH		BIT(4)
/* Enable PADPIPE CLKEN */
#define NVQUIRK_ENABLE_PADPIPE_CLKEN		BIT(5)
/* DISABLE SPI_MODE CLKEN */
#define NVQUIRK_DISABLE_SPI_MODE_CLKEN		BIT(6)
/* Set tap delay */
#define NVQUIRK_SET_TAP_DELAY			BIT(7)
/* Set trim delay */
#define NVQUIRK_SET_TRIM_DELAY			BIT(8)
/* Enable SDHOST v3.0 support */
#define NVQUIRK_ENABLE_SD_3_0			BIT(9)
/* Enable SDR50 mode */
#define NVQUIRK_ENABLE_SDR50			BIT(10)
/* Enable SDR104 mode */
#define NVQUIRK_ENABLE_SDR104			BIT(11)
/*Enable DDR50 mode */
#define NVQUIRK_ENABLE_DDR50			BIT(12)
/* Enable Frequency Tuning for SDR50 mode */
#define NVQUIRK_ENABLE_SDR50_TUNING		BIT(13)
/* Enable Infinite Erase Timeout*/
#define NVQUIRK_INFINITE_ERASE_TIMEOUT		BIT(14)
/* Disable AUTO CMD23 */
#define NVQUIRK_DISABLE_AUTO_CMD23		BIT(15)
/* ENAABLE FEEDBACK IO CLOCK */
#define NVQUIRK_EN_FEEDBACK_CLK			BIT(16)
/* Shadow write xfer mode reg and write it alongwith CMD register */
#define NVQUIRK_SHADOW_XFER_MODE_REG		BIT(17)
/* In SDR50 mode, run the sdmmc controller at freq greater than
 * 104MHz to ensure the core voltage is at 1.2V. If the core voltage
 * is below 1.2V, CRC errors would occur during data transfers
 */
#define NVQUIRK_BROKEN_SDR50_CONTROLLER_CLOCK	BIT(19)

struct sdhci_tegra_soc_data {
	struct sdhci_pltfm_data *pdata;
	u32 nvquirks;
};

struct sdhci_tegra_sd_stats {
	unsigned int data_crc_count;
	unsigned int cmd_crc_count;
	unsigned int data_to_count;
	unsigned int cmd_to_count;
};

#ifdef CONFIG_MMC_FREQ_SCALING
struct freq_gov_params {
	u8	idle_mon_cycles;
	u8	polling_interval_ms;
	u8	active_load_threshold;
};

static struct freq_gov_params gov_params[3] = {
	[MMC_TYPE_MMC] = {
		.idle_mon_cycles = 3,
		.polling_interval_ms = 50,
		.active_load_threshold = 25,
	},
	[MMC_TYPE_SDIO] = {
		.idle_mon_cycles = 3,
		.polling_interval_ms = 50,
		.active_load_threshold = 25,
	},
	[MMC_TYPE_SD] = {
		.idle_mon_cycles = 3,
		.polling_interval_ms = 50,
		.active_load_threshold = 25,
	},
};
#endif

enum tegra_tuning_freq {
	TUNING_LOW_FREQ,
	TUNING_HIGH_FREQ,
	TUNING_HIGH_FREQ_HV,
};

struct freq_tuning_params {
	unsigned int	freq_hz;
	unsigned int	nr_voltages;
	unsigned int	voltages[TUNING_VOLTAGES_COUNT];
};

static struct freq_tuning_params tuning_params[TUNING_FREQ_COUNT] = {
	[TUNING_LOW_FREQ] = {
		.freq_hz = 82000000,
		.nr_voltages = 1,
		.voltages = {1250},
	},
	[TUNING_HIGH_FREQ] = {
		.freq_hz = 156000000,
		.nr_voltages = 2,
		.voltages = {1250, 1100},
	},
	[TUNING_HIGH_FREQ_HV] = {
		.freq_hz = 156000000,
		.nr_voltages = 2,
		.voltages = {1390, 1250},
	},
};

struct tap_window_data {
	unsigned int	partial_win;
	unsigned int	full_win_begin;
	unsigned int	full_win_end;
	unsigned int	tuning_ui;
	unsigned int	sampling_point;
	bool		abandon_partial_win;
	bool		abandon_full_win;
};

struct tegra_tuning_data {
	/* need to store multiple values if DVFS */
	unsigned int		best_tap_value[TUNING_FREQ_COUNT];
	bool			select_partial_win;
	bool			nominal_vcore_tuning_done;
	bool			overide_vcore_tuning_done;
	bool			one_shot_tuning;
	struct tap_window_data	*tap_data[TUNING_VOLTAGES_COUNT];
};

struct tegra_freq_gov_data {
	unsigned int		curr_active_load;
	unsigned int		avg_active_load;
	unsigned int		act_load_high_threshold;
	unsigned int		max_idle_monitor_cycles;
	unsigned int		curr_freq;
	unsigned int		freqs[TUNING_FREQ_COUNT];
	unsigned int		freq_switch_count;
	bool			monitor_idle_load;
};

struct sdhci_tegra {
	const struct tegra_sdhci_platform_data *plat;
	const struct sdhci_tegra_soc_data *soc_data;
	bool	clk_enabled;
	struct regulator *vdd_io_reg;
	struct regulator *vdd_slot_reg;
	struct regulator *vcore_reg;
	/* Pointer to the chip specific HW ops */
	struct tegra_sdhci_hw_ops *hw_ops;
	/* Host controller instance */
	unsigned int instance;
	/* vddio_min */
	unsigned int vddio_min_uv;
	/* vddio_max */
	unsigned int vddio_max_uv;
	/* max clk supported by the platform */
	unsigned int max_clk_limit;
	/* max ddr clk supported by the platform */
	unsigned int ddr_clk_limit;
	/* SD Hot Plug in Suspend State */
	unsigned int sd_detect_in_suspend;
	bool card_present;
	bool is_rail_enabled;
	struct clk *emc_clk;
	bool emc_clk_enabled;
	unsigned int emc_max_clk;
	struct clk *sclk;
	bool is_sdmmc_sclk_on;
	struct sdhci_tegra_sd_stats *sd_stat_head;
	unsigned int nominal_vcore_mv;
	unsigned int min_vcore_override_mv;
	/* Tuning related structures and variables */
	/* Tuning opcode to be used */
	unsigned int tuning_opcode;
	/* Tuning packet size */
	unsigned int tuning_bsize;
	/* Tuning status */
	unsigned int tuning_status;
	unsigned int tap_cmd;
	struct mutex retune_lock;
#define TUNING_STATUS_DONE	1
#define TUNING_STATUS_RETUNE	2
	/* Freq tuning information for each sampling clock freq */
	struct tegra_tuning_data tuning_data[TUNING_FREQ_COUNT];
	bool set_tuning_override;
	bool is_parent_pllc;
	struct notifier_block reboot_notify;
	struct tegra_freq_gov_data *gov_data;
};

static struct clk *pll_c;
static struct clk *pll_p;
static unsigned long pll_c_rate;
static unsigned long pll_p_rate;

static int show_error_stats_dump(struct seq_file *s, void *data)
{
	struct sdhci_host *host = s->private;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct sdhci_tegra_sd_stats *head;

	seq_printf(s, "ErrorStatistics:\n");
	seq_printf(s, "DataCRC\tCmdCRC\tDataTimeout\tCmdTimeout\n");
	head = tegra_host->sd_stat_head;
	if (head != NULL)
		seq_printf(s, "%d\t%d\t%d\t%d\n", head->data_crc_count,
			head->cmd_crc_count, head->data_to_count,
			head->cmd_to_count);
	return 0;
}

static int show_dfs_stats_dump(struct seq_file *s, void *data)
{
	struct sdhci_host *host = s->private;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_freq_gov_data *gov_data = tegra_host->gov_data;

	seq_printf(s, "DFS statistics:\n");

	if (host->mmc->dev_stats != NULL)
		seq_printf(s, "Polling_period: %d\n",
			host->mmc->dev_stats->polling_interval);

	if (gov_data != NULL) {
		seq_printf(s, "cur_active_load: %d\n",
			gov_data->curr_active_load);
		seq_printf(s, "avg_active_load: %d\n",
			gov_data->avg_active_load);
		seq_printf(s, "act_load_high_threshold: %d\n",
			gov_data->act_load_high_threshold);
		seq_printf(s, "freq_switch_count: %d\n",
			gov_data->freq_switch_count);
	}
	return 0;
}

static int sdhci_error_stats_dump(struct inode *inode, struct file *file)
{
	return single_open(file, show_error_stats_dump, inode->i_private);
}

static int sdhci_dfs_stats_dump(struct inode *inode, struct file *file)
{
	return single_open(file, show_dfs_stats_dump, inode->i_private);
}


static const struct file_operations sdhci_host_fops = {
	.open		= sdhci_error_stats_dump,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations sdhci_host_dfs_fops = {
	.open		= sdhci_dfs_stats_dump,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static u32 tegra_sdhci_readl(struct sdhci_host *host, int reg)
{
	u32 val;

	if (unlikely(reg == SDHCI_PRESENT_STATE)) {
		/* Use wp_gpio here instead? */
		val = readl(host->ioaddr + reg);
		return val | SDHCI_WRITE_PROTECT;
	}
	return readl(host->ioaddr + reg);
}


#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static u16 tegra_sdhci_readw(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (unlikely((soc_data->nvquirks & NVQUIRK_FORCE_SDHCI_SPEC_200) &&
			(reg == SDHCI_HOST_VERSION))) {
		return SDHCI_SPEC_200;
	}
}
#endif

static void tegra_sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
#endif

	/* Seems like we're getting spurious timeout and crc errors, so
	 * disable signalling of them. In case of real errors software
	 * timers should take care of eventually detecting them.
	 */
	if (unlikely(reg == SDHCI_SIGNAL_ENABLE))
		val &= ~(SDHCI_INT_TIMEOUT|SDHCI_INT_CRC);

	writel(val, host->ioaddr + reg);

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	if (unlikely((soc_data->nvquirks & NVQUIRK_ENABLE_BLOCK_GAP_DET) &&
			(reg == SDHCI_INT_ENABLE))) {
		u8 gap_ctrl = readb(host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
		if (val & SDHCI_INT_CARD_INT)
			gap_ctrl |= 0x8;
		else
			gap_ctrl &= ~0x8;
		writeb(gap_ctrl, host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
	}
#endif
}

static void tegra_sdhci_writew(struct sdhci_host *host, u16 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (soc_data->nvquirks & NVQUIRK_SHADOW_XFER_MODE_REG) {
		switch (reg) {
		case SDHCI_TRANSFER_MODE:
			/*
			 * Postpone this write, we must do it together with a
			 * command write that is down below.
			 */
			pltfm_host->xfer_mode_shadow = val;
			return;
		case SDHCI_COMMAND:
			writel((val << 16) | pltfm_host->xfer_mode_shadow,
				host->ioaddr + SDHCI_TRANSFER_MODE);
			pltfm_host->xfer_mode_shadow = 0;
			return;
		}
	}

	writew(val, host->ioaddr + reg);
}

#ifdef CONFIG_MMC_FREQ_SCALING
/*
 * Dynamic frequency calculation.
 * The active load for the current period and the average active load
 * are calculated at the end of each polling interval.
 *
 * If the current active load is greater than the threshold load, then the
 * frequency is boosted(156MHz).
 * If the active load is lower than the threshold, then the load is monitored
 * for a max of three cycles before reducing the frequency(82MHz). If the
 * average active load is lower, then the monitoring cycles is reduced.
 *
 * The active load threshold value for both eMMC and SDIO is set to 25 which
 * is found to give the optimal power and performance. The polling interval is
 * set to 50 msec.
 *
 * The polling interval and active load threshold values can be changed by
 * the user through sysfs.
*/
static unsigned long calculate_mmc_target_freq(
	struct tegra_freq_gov_data *gov_data)
{
	unsigned long desired_freq = gov_data->curr_freq;
	unsigned int type = MMC_TYPE_MMC;

	if (gov_data->curr_active_load >= gov_data->act_load_high_threshold) {
		desired_freq = gov_data->freqs[TUNING_HIGH_FREQ];
		gov_data->monitor_idle_load = false;
		gov_data->max_idle_monitor_cycles =
			gov_params[type].idle_mon_cycles;
	} else {
		if (gov_data->monitor_idle_load) {
			if (!gov_data->max_idle_monitor_cycles) {
				desired_freq = gov_data->freqs[TUNING_LOW_FREQ];
				gov_data->max_idle_monitor_cycles =
					gov_params[type].idle_mon_cycles;
			} else {
				gov_data->max_idle_monitor_cycles--;
			}
		} else {
			gov_data->monitor_idle_load = true;
			gov_data->max_idle_monitor_cycles *=
				gov_data->avg_active_load;
			gov_data->max_idle_monitor_cycles /= 100;
		}
	}

	return desired_freq;
}

static unsigned long calculate_sdio_target_freq(
	struct tegra_freq_gov_data *gov_data)
{
	unsigned long desired_freq = gov_data->curr_freq;
	unsigned int type = MMC_TYPE_SDIO;

	if (gov_data->curr_active_load >= gov_data->act_load_high_threshold) {
		desired_freq = gov_data->freqs[TUNING_HIGH_FREQ];
		gov_data->monitor_idle_load = false;
		gov_data->max_idle_monitor_cycles =
			gov_params[type].idle_mon_cycles;
	} else {
		if (gov_data->monitor_idle_load) {
			if (!gov_data->max_idle_monitor_cycles) {
				desired_freq = gov_data->freqs[TUNING_LOW_FREQ];
				gov_data->max_idle_monitor_cycles =
					gov_params[type].idle_mon_cycles;
			} else {
				gov_data->max_idle_monitor_cycles--;
			}
		} else {
			gov_data->monitor_idle_load = true;
			gov_data->max_idle_monitor_cycles *=
				gov_data->avg_active_load;
			gov_data->max_idle_monitor_cycles /= 100;
		}
	}

	return desired_freq;
}

static unsigned long calculate_sd_target_freq(
	struct tegra_freq_gov_data *gov_data)
{
	unsigned long desired_freq = gov_data->curr_freq;
	unsigned int type = MMC_TYPE_SD;

	if (gov_data->curr_active_load >= gov_data->act_load_high_threshold) {
		desired_freq = gov_data->freqs[TUNING_HIGH_FREQ];
		gov_data->monitor_idle_load = false;
		gov_data->max_idle_monitor_cycles =
			gov_params[type].idle_mon_cycles;
	} else {
		if (gov_data->monitor_idle_load) {
			if (!gov_data->max_idle_monitor_cycles) {
				desired_freq = gov_data->freqs[TUNING_LOW_FREQ];
				gov_data->max_idle_monitor_cycles =
					gov_params[type].idle_mon_cycles;
			} else {
				gov_data->max_idle_monitor_cycles--;
			}
		} else {
			gov_data->monitor_idle_load = true;
			gov_data->max_idle_monitor_cycles *=
				gov_data->avg_active_load;
			gov_data->max_idle_monitor_cycles /= 100;
		}
	}

	return desired_freq;
}

static unsigned long sdhci_tegra_get_target_freq(struct sdhci_host *sdhci,
	struct devfreq_dev_status *dfs_stats)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_freq_gov_data *gov_data = tegra_host->gov_data;
	unsigned long freq = sdhci->mmc->actual_clock;

	if (!gov_data) {
		dev_err(mmc_dev(sdhci->mmc),
			"No gov data. Continue using current freq %ld", freq);
		return freq;
	}

	/*
	 * If clock gating is enabled and clock is currently disabled, then
	 * return freq as 0.
	 */
	if (!tegra_host->clk_enabled)
		return 0;

	if (dfs_stats->total_time) {
		gov_data->curr_active_load = (dfs_stats->busy_time * 100) /
			dfs_stats->total_time;
	} else {
		gov_data->curr_active_load = 0;
	}

	gov_data->avg_active_load += gov_data->curr_active_load;
	gov_data->avg_active_load >>= 1;

	if (sdhci->mmc->card) {
		if (sdhci->mmc->card->type == MMC_TYPE_SDIO)
			freq = calculate_sdio_target_freq(gov_data);
		else if (sdhci->mmc->card->type == MMC_TYPE_MMC)
			freq = calculate_mmc_target_freq(gov_data);
		else if (sdhci->mmc->card->type == MMC_TYPE_SD)
			freq = calculate_sd_target_freq(gov_data);
		if (gov_data->curr_freq != freq)
			gov_data->freq_switch_count++;
		gov_data->curr_freq = freq;
	}

	return freq;
}

static int sdhci_tegra_freq_gov_init(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	unsigned int i;
	unsigned int freq;
	unsigned int type;

	if (!((sdhci->mmc->ios.timing == MMC_TIMING_UHS_SDR104) ||
		(sdhci->mmc->ios.timing == MMC_TIMING_MMC_HS200))) {
		dev_info(mmc_dev(sdhci->mmc),
			"DFS not required for current operating mode\n");
		return -EACCES;
	}

	if (!tegra_host->gov_data) {
		tegra_host->gov_data = devm_kzalloc(mmc_dev(sdhci->mmc),
			sizeof(struct tegra_freq_gov_data), GFP_KERNEL);
		if (!tegra_host->gov_data) {
			dev_err(mmc_dev(sdhci->mmc),
				"Failed to allocate memory for dfs data\n");
			return -ENOMEM;
		}
	}

	/* Find the supported frequencies */
	for (i = 0; i < TUNING_FREQ_COUNT; i++) {
		freq = tuning_params[i].freq_hz;
		/*
		 * Check the nearest possible clock with pll_c and pll_p as
		 * the clock sources. Choose the higher frequency.
		 */
		tegra_host->gov_data->freqs[i] =
			get_nearest_clock_freq(pll_c_rate, freq);
		freq = get_nearest_clock_freq(pll_p_rate, freq);
		if (freq > tegra_host->gov_data->freqs[i])
			tegra_host->gov_data->freqs[i] = freq;
	}

	tegra_host->gov_data->monitor_idle_load = false;
	tegra_host->gov_data->curr_freq = sdhci->mmc->actual_clock;
	if (sdhci->mmc->card) {
		type = sdhci->mmc->card->type;
		sdhci->mmc->dev_stats->polling_interval =
			gov_params[type].polling_interval_ms;
		tegra_host->gov_data->act_load_high_threshold =
			gov_params[type].active_load_threshold;
		tegra_host->gov_data->max_idle_monitor_cycles =
			gov_params[type].idle_mon_cycles;
	}

	return 0;
}

#endif

static unsigned int tegra_sdhci_get_cd(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;

	return tegra_host->card_present;
}

static unsigned int tegra_sdhci_get_ro(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;

	if (!gpio_is_valid(plat->wp_gpio))
		return -1;

	return gpio_get_value(plat->wp_gpio);
}


static int tegra_sdhci_set_uhs_signaling(struct sdhci_host *host,
		unsigned int uhs)
{
	u16 clk, ctrl_2;
	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);

	/* Select Bus Speed Mode for host */
	/* For HS200 we need to set UHS_MODE_SEL to SDR104.
	 * It works as SDR 104 in SD 4-bit mode and HS200 in eMMC 8-bit mode.
	 */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	switch (uhs) {
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_TIMING_UHS_SDR104:
	case MMC_TIMING_MMC_HS200:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_UHS_DDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		break;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	if (uhs == MMC_TIMING_UHS_DDR50) {
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
		clk &= ~(0xFF << SDHCI_DIVIDER_SHIFT);
		clk |= 1 << SDHCI_DIVIDER_SHIFT;
		sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	}
	return 0;
}

static void tegra_sdhci_reset_exit(struct sdhci_host *sdhci, u8 mask)
{
	u16 misc_ctrl;
	u32 vendor_ctrl;
	unsigned int best_tap_value;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (mask & SDHCI_RESET_ALL) {
		if (tegra_host->sd_stat_head != NULL) {
			tegra_host->sd_stat_head->data_crc_count = 0;
			tegra_host->sd_stat_head->cmd_crc_count = 0;
			tegra_host->sd_stat_head->data_to_count = 0;
			tegra_host->sd_stat_head->cmd_to_count = 0;
		}

		if (tegra_host->gov_data != NULL)
			tegra_host->gov_data->freq_switch_count = 0;

		vendor_ctrl = sdhci_readl(sdhci, SDHCI_VNDR_CLK_CTRL);
		if (soc_data->nvquirks & NVQUIRK_ENABLE_PADPIPE_CLKEN) {
			vendor_ctrl |=
				SDHCI_VNDR_CLK_CTRL_PADPIPE_CLKEN_OVERRIDE;
		}
		if (soc_data->nvquirks & NVQUIRK_DISABLE_SPI_MODE_CLKEN) {
			vendor_ctrl &=
				~SDHCI_VNDR_CLK_CTRL_SPI_MODE_CLKEN_OVERRIDE;
		}
		if (soc_data->nvquirks & NVQUIRK_EN_FEEDBACK_CLK) {
			vendor_ctrl &=
				~SDHCI_VNDR_CLK_CNTL_INPUT_IO_CLK;
		}

		if (soc_data->nvquirks & NVQUIRK_SET_TAP_DELAY) {
			if ((tegra_host->tuning_status == TUNING_STATUS_DONE)
				&& (sdhci->mmc->pm_flags &
				MMC_PM_KEEP_POWER)) {
				if (sdhci->mmc->ios.clock >
					tuning_params[TUNING_LOW_FREQ].freq_hz) {
					if (tegra_host->tap_cmd == TAP_CMD_TRIM_HIGH_VOLTAGE)
						best_tap_value = tegra_host->
							tuning_data[TUNING_HIGH_FREQ_HV].best_tap_value[TUNING_HIGH_FREQ_HV];
					else
						best_tap_value = tegra_host->
							tuning_data[TUNING_HIGH_FREQ].best_tap_value[TUNING_HIGH_FREQ];
				} else
					best_tap_value = tegra_host->
						tuning_data[TUNING_LOW_FREQ].best_tap_value[TUNING_LOW_FREQ];
			} else {
				best_tap_value = plat->tap_delay;
			}
			vendor_ctrl &= ~(0xFF <<
				SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
			vendor_ctrl |= (best_tap_value <<
				SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
		}

		if (soc_data->nvquirks & NVQUIRK_SET_TRIM_DELAY) {
			vendor_ctrl &= ~(0x1F <<
				SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT);
			vendor_ctrl |= (plat->trim_delay <<
				SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT);
		}
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR50_TUNING)
			vendor_ctrl |= SDHCI_VNDR_CLK_CTRL_SDR50_TUNING;
		sdhci_writel(sdhci, vendor_ctrl, SDHCI_VNDR_CLK_CTRL);

		misc_ctrl = sdhci_readw(sdhci, SDHCI_VNDR_MISC_CTRL);
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SD_3_0)
			misc_ctrl |= SDHCI_VNDR_MISC_CTRL_ENABLE_SD_3_0;
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR104) {
			misc_ctrl |=
			SDHCI_VNDR_MISC_CTRL_ENABLE_SDR104_SUPPORT;
		}
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR50) {
			misc_ctrl |=
			SDHCI_VNDR_MISC_CTRL_ENABLE_SDR50_SUPPORT;
		}
		if (soc_data->nvquirks & NVQUIRK_INFINITE_ERASE_TIMEOUT) {
			misc_ctrl |=
			SDHCI_VNDR_MISC_CTRL_INFINITE_ERASE_TIMEOUT;
		}

		/* Enable DDR mode support only if supported */
		if (plat->uhs_mask & MMC_UHS_MASK_DDR50)
			sdhci->mmc->caps &= ~MMC_CAP_UHS_DDR50;
		else if (soc_data->nvquirks & NVQUIRK_ENABLE_DDR50)
			misc_ctrl |= SDHCI_VNDR_MISC_CTRL_ENABLE_DDR50_SUPPORT;

		sdhci_writew(sdhci, misc_ctrl, SDHCI_VNDR_MISC_CTRL);

		/* Mask Auto CMD23 if CMD23 is enabled */
		if ((sdhci->mmc->caps & MMC_CAP_CMD23) &&
			(soc_data->nvquirks & NVQUIRK_DISABLE_AUTO_CMD23))
			sdhci->flags &= ~SDHCI_AUTO_CMD23;

		/* Mask the support for any UHS modes if specified */
		if (plat->uhs_mask & MMC_UHS_MASK_SDR104)
			sdhci->mmc->caps &= ~MMC_CAP_UHS_SDR104;

		if (plat->uhs_mask & MMC_UHS_MASK_SDR50)
			sdhci->mmc->caps &= ~MMC_CAP_UHS_SDR50;

		if (plat->uhs_mask & MMC_UHS_MASK_SDR25)
			sdhci->mmc->caps &= ~MMC_CAP_UHS_SDR25;

		if (plat->uhs_mask & MMC_UHS_MASK_SDR12)
			sdhci->mmc->caps &= ~MMC_CAP_UHS_SDR12;

		if (plat->uhs_mask & MMC_MASK_HS200)
			sdhci->mmc->caps2 &= ~MMC_CAP2_HS200;
	}
}

static void sdhci_status_notify_cb(int card_present, void *dev_id)
{
	struct sdhci_host *sdhci = (struct sdhci_host *)dev_id;
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhci->mmc));
	struct tegra_sdhci_platform_data *plat;
	unsigned int status, oldstat;

	pr_debug("%s: card_present %d\n", mmc_hostname(sdhci->mmc),
		card_present);

	plat = pdev->dev.platform_data;
	if (!plat->mmc_data.status) {
		mmc_detect_change(sdhci->mmc, 0);
		return;
	}

	status = plat->mmc_data.status(mmc_dev(sdhci->mmc));

	oldstat = plat->mmc_data.card_present;
	plat->mmc_data.card_present = status;
	if (status ^ oldstat) {
		pr_debug("%s: Slot status change detected (%d -> %d)\n",
			mmc_hostname(sdhci->mmc), oldstat, status);
		if (status && !plat->mmc_data.built_in)
			mmc_detect_change(sdhci->mmc, (5 * HZ) / 2);
		else
			mmc_detect_change(sdhci->mmc, 0);
	}
}

static irqreturn_t carddetect_irq(int irq, void *data)
{
	struct sdhci_host *sdhost = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhost);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhost->mmc));
	struct tegra_sdhci_platform_data *plat;

	plat = pdev->dev.platform_data;

	tegra_host->card_present = (gpio_get_value(plat->cd_gpio) == 0);

	if (tegra_host->card_present) {
		if (!tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_slot_reg)
				regulator_enable(tegra_host->vdd_slot_reg);
			if (tegra_host->vdd_io_reg)
				regulator_enable(tegra_host->vdd_io_reg);
			tegra_host->is_rail_enabled = 1;
		}
	} else {
		if (tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_io_reg)
				regulator_disable(tegra_host->vdd_io_reg);
			if (tegra_host->vdd_slot_reg)
				regulator_disable(tegra_host->vdd_slot_reg);
			tegra_host->is_rail_enabled = 0;
		}
		/*
		 * Set retune request as tuning should be done next time
		 * a card is inserted.
		 */
		tegra_host->tuning_status = TUNING_STATUS_RETUNE;
	}

	tasklet_schedule(&sdhost->card_tasklet);
	return IRQ_HANDLED;
};

static int tegra_sdhci_8bit(struct sdhci_host *sdhci, int bus_width)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhci->mmc));
	const struct tegra_sdhci_platform_data *plat;
	u32 ctrl;

	plat = pdev->dev.platform_data;

	ctrl = sdhci_readb(sdhci, SDHCI_HOST_CONTROL);
	if (plat->is_8bit && bus_width == MMC_BUS_WIDTH_8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		ctrl |= SDHCI_CTRL_8BITBUS;
	} else {
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (bus_width == MMC_BUS_WIDTH_4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}
	sdhci_writeb(sdhci, ctrl, SDHCI_HOST_CONTROL);
	return 0;
}

/*
* Calculation of nearest clock frequency for desired rate:
* Get the divisor value, div = p / d_rate
* 1. If it is nearer to ceil(p/d_rate) then increment the div value by 0.5 and
* nearest_rate, i.e. result = p / (div + 0.5) = (p << 1)/((div << 1) + 1).
* 2. If not, result = p / div
* As the nearest clk freq should be <= to desired_rate,
* 3. If result > desired_rate then increment the div by 0.5
* and do, (p << 1)/((div << 1) + 1)
* 4. Else return result
* Here, If condtions 1 & 3 are both satisfied then to keep track of div value,
* defined index variable.
*/
static unsigned long get_nearest_clock_freq(unsigned long pll_rate,
		unsigned long desired_rate)
{
	unsigned long result;
	int div;
	int index = 1;

	div = pll_rate / desired_rate;
	if (div > MAX_DIVISOR_VALUE) {
		div = MAX_DIVISOR_VALUE;
		result = pll_rate / div;
	} else {
		if ((pll_rate % desired_rate) >= (desired_rate / 2))
			result = (pll_rate << 1) / ((div << 1) + index++);
		else
			result = pll_rate / div;

		if (desired_rate < result) {
			/*
			* Trying to get lower clock freq than desired clock,
			* by increasing the divisor value by 0.5
			*/
			result = (pll_rate << 1) / ((div << 1) + index);
		}
	}

	return result;
}

static void tegra_sdhci_clock_set_parent(struct sdhci_host *host,
		unsigned long desired_rate)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct clk *parent_clk;
	unsigned long pll_c_freq;
	unsigned long pll_p_freq;
	int rc;

	pll_c_freq = get_nearest_clock_freq(pll_c_rate, desired_rate);
	pll_p_freq = get_nearest_clock_freq(pll_p_rate, desired_rate);

	if (pll_c_freq > pll_p_freq) {
		if (!tegra_host->is_parent_pllc) {
			parent_clk = pll_c;
			tegra_host->is_parent_pllc = true;
			clk_set_rate(pltfm_host->clk, DEFAULT_SDHOST_FREQ);
		} else
			return;
	} else if (tegra_host->is_parent_pllc) {
		parent_clk = pll_p;
		tegra_host->is_parent_pllc = false;
	} else
		return;

	rc = clk_set_parent(pltfm_host->clk, parent_clk);
	if (rc)
		pr_err("%s: failed to set pll parent clock %d\n",
			mmc_hostname(host->mmc), rc);
}

static void tegra_sdhci_set_clk_rate(struct sdhci_host *sdhci,
	unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	unsigned int clk_rate;
	unsigned int emc_clk;

	if (sdhci->mmc->ios.timing == MMC_TIMING_UHS_DDR50) {
		/*
		 * In ddr mode, tegra sdmmc controller clock frequency
		 * should be double the card clock frequency.
		 */
		if (tegra_host->ddr_clk_limit) {
			clk_rate = tegra_host->ddr_clk_limit * 2;
			if (tegra_host->emc_clk) {
				emc_clk = clk_get_rate(tegra_host->emc_clk);
				if (emc_clk == tegra_host->emc_max_clk)
					clk_rate = clock * 2;
			}
		} else {
			clk_rate = clock * 2;
		}
	} else if ((sdhci->mmc->ios.timing == MMC_TIMING_UHS_SDR50) &&
		(soc_data->nvquirks & NVQUIRK_BROKEN_SDR50_CONTROLLER_CLOCK)) {

		/*
		 * In SDR50 mode, run the sdmmc controller at freq greater
		 * than 104MHz to ensure the core voltage is at 1.2V.
		 * If the core voltage is below 1.2V, CRC errors would
		 * occur during data transfers.
		 */
		clk_rate = clock * 2;
	} else
		clk_rate = clock;

	if (tegra_host->max_clk_limit &&
		(clk_rate > tegra_host->max_clk_limit))
			clk_rate = tegra_host->max_clk_limit;

	tegra_sdhci_clock_set_parent(sdhci, clk_rate);
	clk_set_rate(pltfm_host->clk, clk_rate);
	sdhci->max_clk = clk_get_rate(pltfm_host->clk);
#ifdef CONFIG_TEGRA_FPGA_PLATFORM
	/* FPGA supports 26MHz of clock for SDMMC. */
	sdhci->max_clk = 26000000;
#endif

	if (tegra_host->tuning_status == TUNING_STATUS_RETUNE)
		sdhci->flags |= SDHCI_NEEDS_RETUNING;

#ifdef CONFIG_MMC_FREQ_SCALING
	/* Set the tap delay if tuning is done and dfs is enabled */
	if (sdhci->mmc->df &&
		(tegra_host->tuning_status == TUNING_STATUS_DONE)) {
		int freq_band;

		if (clock > tuning_params[TUNING_LOW_FREQ].freq_hz) {
			if (tegra_host->tap_cmd == TAP_CMD_TRIM_HIGH_VOLTAGE)
				freq_band = TUNING_HIGH_FREQ_HV;
			else
				freq_band = TUNING_HIGH_FREQ;
		} else
			freq_band = TUNING_LOW_FREQ;

		sdhci_tegra_set_tap_delay(sdhci,
			tegra_host->tuning_data[freq_band].best_tap_value[freq_band]);
	}
#endif
}
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static void tegra_3x_sdhci_set_card_clock(struct sdhci_host *sdhci, unsigned int clock)
{
	int div;
	u16 clk;
	unsigned long timeout;
	u8 ctrl;

	if (clock && clock == sdhci->clock)
		return;

	/*
	 * Disable the card clock before disabling the internal
	 * clock to avoid abnormal clock waveforms.
	 */
	clk = sdhci_readw(sdhci, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(sdhci, clk, SDHCI_CLOCK_CONTROL);
	sdhci_writew(sdhci, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		goto out;
	if (sdhci->mmc->ios.timing == MMC_TIMING_UHS_DDR50) {
		div = 1;
		goto set_clk;
	}

	if (sdhci->version >= SDHCI_SPEC_300) {
		/* Version 3.00 divisors must be a multiple of 2. */
		if (sdhci->max_clk <= clock) {
			div = 1;
		} else {
			for (div = 2; div < SDHCI_MAX_DIV_SPEC_300; div += 2) {
				if ((sdhci->max_clk / div) <= clock)
					break;
			}
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
			if ((sdhci->max_clk / div) <= clock)
				break;
		}
	}
	div >>= 1;

	/*
	 * Tegra3 sdmmc controller internal clock will not be stabilized when
	 * we use a clock divider value greater than 4. The WAR is as follows.
	 * - Enable internal clock.
	 * - Wait for 5 usec and do a dummy write.
	 * - Poll for clk stable.
	 */
set_clk:
	clk = (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(sdhci, clk, SDHCI_CLOCK_CONTROL);

	/* Wait for 5 usec */
	udelay(5);

	/* Do a dummy write */
	ctrl = sdhci_readb(sdhci, SDHCI_CAPABILITIES);
	ctrl |= 1;
	sdhci_writeb(sdhci, ctrl, SDHCI_CAPABILITIES);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(sdhci, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			dev_err(mmc_dev(sdhci->mmc), "Internal clock never stabilised\n");
			return;
		}
		timeout--;
		mdelay(1);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(sdhci, clk, SDHCI_CLOCK_CONTROL);
out:
	sdhci->clock = clock;
}
#endif /*	#ifdef CONFIG_ARCH_TEGRA_3x_SOC */

static void tegra_sdhci_set_clock(struct sdhci_host *sdhci, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhci->mmc));
	u8 ctrl;

	pr_debug("%s %s %u enabled=%u\n", __func__,
		mmc_hostname(sdhci->mmc), clock, tegra_host->clk_enabled);

	if (clock) {
		if (!tegra_host->clk_enabled) {
			pm_runtime_get_sync(&pdev->dev);
			clk_prepare_enable(pltfm_host->clk);
			ctrl = sdhci_readb(sdhci, SDHCI_VNDR_CLK_CTRL);
			ctrl |= SDHCI_VNDR_CLK_CTRL_SDMMC_CLK;
			sdhci_writeb(sdhci, ctrl, SDHCI_VNDR_CLK_CTRL);
			tegra_host->clk_enabled = true;
		}
		tegra_sdhci_set_clk_rate(sdhci, clock);
		if (tegra_host->hw_ops->set_card_clock)
			tegra_host->hw_ops->set_card_clock(sdhci, clock);

		if (tegra_host->emc_clk && (!tegra_host->emc_clk_enabled)) {
			clk_prepare_enable(tegra_host->emc_clk);
			tegra_host->emc_clk_enabled = true;
		}

		if (tegra_host->sclk && (!tegra_host->is_sdmmc_sclk_on)) {
			clk_prepare_enable(tegra_host->sclk);
			tegra_host->is_sdmmc_sclk_on = true;
		}
	} else if (!clock && tegra_host->clk_enabled) {
		if (tegra_host->emc_clk && tegra_host->emc_clk_enabled) {
			clk_disable_unprepare(tegra_host->emc_clk);
			tegra_host->emc_clk_enabled = false;
		}

		if (tegra_host->sclk && tegra_host->is_sdmmc_sclk_on) {
			clk_disable_unprepare(tegra_host->sclk);
			tegra_host->is_sdmmc_sclk_on = false;
		}

		if (tegra_host->hw_ops->set_card_clock)
			tegra_host->hw_ops->set_card_clock(sdhci, clock);
		ctrl = sdhci_readb(sdhci, SDHCI_VNDR_CLK_CTRL);
		ctrl &= ~SDHCI_VNDR_CLK_CTRL_SDMMC_CLK;
		sdhci_writeb(sdhci, ctrl, SDHCI_VNDR_CLK_CTRL);
		clk_disable_unprepare(pltfm_host->clk);
		pm_runtime_put_sync(&pdev->dev);
		tegra_host->clk_enabled = false;
	}
}
static void tegra_sdhci_do_calibration(struct sdhci_host *sdhci)
{
	unsigned int val;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	unsigned int timeout = 10;

	/* No Calibration for sdmmc4 -- chip bug */
	if (tegra_host->instance == 3)
		return;

	if (unlikely(soc_data->nvquirks & NVQUIRK_DISABLE_AUTO_CALIBRATION))
		return;

	val = sdhci_readl(sdhci, SDMMC_SDMEMCOMPPADCTRL);
	val &= ~SDMMC_SDMEMCOMPPADCTRL_VREF_SEL_MASK;
	val |= 0x7;
	sdhci_writel(sdhci, val, SDMMC_SDMEMCOMPPADCTRL);

	/* Enable Auto Calibration*/
	val = sdhci_readl(sdhci, SDMMC_AUTO_CAL_CONFIG);
	val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
	val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_START;
	if (unlikely(soc_data->nvquirks & NVQUIRK_SET_CALIBRATION_OFFSETS)) {
		/* Program Auto cal PD offset(bits 8:14) */
		val &= ~(0x7F <<
			SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
		val |= (SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET <<
			SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
		/* Program Auto cal PU offset(bits 0:6) */
		val &= ~0x7F;
		val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PU_OFFSET;
	}
	sdhci_writel(sdhci, val, SDMMC_AUTO_CAL_CONFIG);

	/* Wait until the calibration is done */
	do {
		if (!(sdhci_readl(sdhci, SDMMC_AUTO_CAL_STATUS) &
			SDMMC_AUTO_CAL_STATUS_AUTO_CAL_ACTIVE))
			break;

		mdelay(1);
		timeout--;
	} while (timeout);

	if (!timeout)
		dev_err(mmc_dev(sdhci->mmc), "Auto calibration failed\n");

	/* Disable Auto calibration */
	val = sdhci_readl(sdhci, SDMMC_AUTO_CAL_CONFIG);
	val &= ~SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
	sdhci_writel(sdhci, val, SDMMC_AUTO_CAL_CONFIG);

	if (unlikely(soc_data->nvquirks & NVQUIRK_SET_DRIVE_STRENGTH)) {
		unsigned int pulldown_code;
		unsigned int pullup_code;
		int pg;
		int err;

		pg = tegra_drive_get_pingroup(mmc_dev(sdhci->mmc));
		if (pg != -1) {
			/* Get the pull down codes from auto cal status reg */
			pulldown_code = (
				sdhci_readl(sdhci, SDMMC_AUTO_CAL_STATUS) >>
				SDMMC_AUTO_CAL_STATUS_PULLDOWN_OFFSET);
			/* Set the pull down in the pinmux reg */
			err = tegra_drive_pinmux_set_pull_down(pg,
				pulldown_code);
			if (err)
				dev_err(mmc_dev(sdhci->mmc),
				"Failed to set pulldown codes %d err %d\n",
				pulldown_code, err);

			/* Calculate the pull up codes */
			pullup_code = pulldown_code + PULLUP_ADJUSTMENT_OFFSET;
			if (pullup_code >= TEGRA_MAX_PULL)
				pullup_code = TEGRA_MAX_PULL - 1;
			/* Set the pull up code in the pinmux reg */
			err = tegra_drive_pinmux_set_pull_up(pg, pullup_code);
			if (err)
				dev_err(mmc_dev(sdhci->mmc),
				"Failed to set pullup codes %d err %d\n",
				pullup_code, err);
		}
	}
}

static int tegra_sdhci_signal_voltage_switch(struct sdhci_host *sdhci,
	unsigned int signal_voltage)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	unsigned int min_uV = tegra_host->vddio_min_uv;
	unsigned int max_uV = tegra_host->vddio_max_uv;
	unsigned int rc = 0;
	u16 clk, ctrl;


	ctrl = sdhci_readw(sdhci, SDHCI_HOST_CONTROL2);
	if (signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		ctrl |= SDHCI_CTRL_VDD_180;
		min_uV = SDHOST_LOW_VOLT_MIN;
		max_uV = SDHOST_LOW_VOLT_MAX;
	} else if (signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		if (ctrl & SDHCI_CTRL_VDD_180)
			ctrl &= ~SDHCI_CTRL_VDD_180;
	}

	/* Check if the slot can support the required voltage */
	if (min_uV > tegra_host->vddio_max_uv)
		return 0;

	/* Switch OFF the card clock to prevent glitches on the clock line */
	clk = sdhci_readw(sdhci, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(sdhci, clk, SDHCI_CLOCK_CONTROL);

	/* Set/clear the 1.8V signalling */
	sdhci_writew(sdhci, ctrl, SDHCI_HOST_CONTROL2);

	/* Switch the I/O rail voltage */
	if (tegra_host->vdd_io_reg) {
		rc = regulator_set_voltage(tegra_host->vdd_io_reg,
			min_uV, max_uV);
		if (rc) {
			dev_err(mmc_dev(sdhci->mmc), "switching to 1.8V"
			"failed . Switching back to 3.3V\n");
			rc = regulator_set_voltage(tegra_host->vdd_io_reg,
				SDHOST_HIGH_VOLT_MIN,
				SDHOST_HIGH_VOLT_MAX);
			if (rc)
				dev_err(mmc_dev(sdhci->mmc),
				"switching to 3.3V also failed\n");
		}
	}

	/* Wait for 10 msec for the voltage to be switched */
	mdelay(10);

	/* Enable the card clock */
	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(sdhci, clk, SDHCI_CLOCK_CONTROL);

	/* Wait for 1 msec after enabling clock */
	mdelay(1);

	return rc;
}

static void tegra_sdhci_reset(struct sdhci_host *sdhci, u8 mask)
{
	unsigned long timeout;

	sdhci_writeb(sdhci, mask, SDHCI_SOFTWARE_RESET);

	/* Wait max 100 ms */
	timeout = 100;

	/* hw clears the bit when it's done */
	while (sdhci_readb(sdhci, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			dev_err(mmc_dev(sdhci->mmc), "%s: Reset 0x%x never " \
				"completed.\n", mmc_hostname(sdhci->mmc),
				(int)mask);
			return;
		}
		timeout--;
		mdelay(1);
	}

	tegra_sdhci_reset_exit(sdhci, mask);
}

static void sdhci_tegra_set_tap_delay(struct sdhci_host *sdhci,
	unsigned int tap_delay)
{
	u32 vendor_ctrl;

	/* Max tap delay value is 255 */
	BUG_ON(tap_delay > MAX_TAP_VALUES);

	vendor_ctrl = sdhci_readl(sdhci, SDHCI_VNDR_CLK_CTRL);
	vendor_ctrl &= ~(0xFF << SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	vendor_ctrl |= (tap_delay << SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	sdhci_writel(sdhci, vendor_ctrl, SDHCI_VNDR_CLK_CTRL);
}

static int sdhci_tegra_sd_error_stats(struct sdhci_host *host, u32 int_status)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct sdhci_tegra_sd_stats *head;

	head = tegra_host->sd_stat_head;
	if (int_status & SDHCI_INT_DATA_CRC)
		head->data_crc_count++;
	if (int_status & SDHCI_INT_CRC)
		head->cmd_crc_count++;
	if (int_status & SDHCI_INT_TIMEOUT)
		head->cmd_to_count++;
	if (int_status & SDHCI_INT_DATA_TIMEOUT)
		head->data_to_count++;
	return 0;
}

static void sdhci_tegra_dump_tuning_data(struct sdhci_host *sdhci, u8 freq_band)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	char *freq[TUNING_FREQ_COUNT] = {"Low", "Normal", "High"};

	tuning_data = &tegra_host->tuning_data[freq_band];

	if (tuning_data->tap_data[0]) {
		pr_info("%s: Tuning window data at %d mV, frequency band: %s\n",
			mmc_hostname(sdhci->mmc),
			tuning_params[freq_band].voltages[0],
			freq[freq_band]);
		pr_info("Partial window %d\n",
			tuning_data->tap_data[0]->partial_win);
		pr_info("full window start %d\n",
			tuning_data->tap_data[0]->full_win_begin);
		pr_info("full window end %d\n",
			tuning_data->tap_data[0]->full_win_end);
	}

	if (((freq_band == TUNING_HIGH_FREQ) ||
		(freq_band == TUNING_HIGH_FREQ_HV)) &&
		(tuning_data->tap_data[1])) {
		pr_info("%s: Tuning window data at %d-%d mV\n",
			mmc_hostname(sdhci->mmc),
			tuning_params[freq_band].voltages[0],
			tuning_params[freq_band].voltages[1]);
		pr_info("partial window %d\n",
		    tuning_data->tap_data[1]->partial_win);
		pr_info("full window being %d\n",
			tuning_data->tap_data[1]->full_win_begin);
		pr_info("full window end %d\n",
			tuning_data->tap_data[1]->full_win_end);
	}
	pr_info("%s window chosen\n",
		tuning_data->select_partial_win ?
		"partial" : "full");
	pr_info("Best tap value %d\n",
		tuning_data->best_tap_value[freq_band]);
}

/*
 * Calculation of best tap value for low frequencies(82MHz).
 * X = Partial win, Y = Full win start, Z = Full win end.
 * UI = Z - X.
 * Full Window = Z - Y.
 * Taps margin = mid-point of 1/2*(curr_freq/max_frequency)*UI
 *                    = (1/2)*(1/2)*(82/200)*UI
 *                    = (0.1025)*UI
 * if Partial win<(0.22)*UI
 * best tap = Y+(0.1025*UI)
 * else
 * best tap = (X-(Z-Y))+(0.1025*UI)
 * If best tap<0, best tap = 0
 */
static unsigned int calculate_low_freq_tap_value(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	unsigned int curr_clock;
	unsigned int max_clock;
	int best_tap_value;
	struct tap_window_data *tap_data;
	struct tegra_tuning_data *tuning_data;

	tuning_data = &tegra_host->tuning_data[TUNING_LOW_FREQ];
	tap_data = tuning_data->tap_data[0];

	if (tap_data->abandon_full_win) {
		if (tap_data->abandon_partial_win) {
			return 0;
		} else {
			tuning_data->select_partial_win = true;
			goto calculate_best_tap;
		}
	}

	tap_data->tuning_ui = tap_data->full_win_end - tap_data->partial_win;

	/* Calculate the sampling point */
	curr_clock = sdhci->max_clk / 1000000;
	max_clock = uhs_max_freq_MHz[sdhci->mmc->ios.timing];
	tap_data->sampling_point = ((tap_data->tuning_ui * curr_clock) /
		max_clock);
	tap_data->sampling_point >>= 2;

	/*
	 * Check whether partial window should be used. Use partial window
	 * if partial window > 0.22(UI).
	 */
	if ((!tap_data->abandon_partial_win) &&
		(tap_data->partial_win > ((22 * tap_data->tuning_ui) / 100)))
			tuning_data->select_partial_win = true;

calculate_best_tap:
	if (tuning_data->select_partial_win) {
		best_tap_value = (tap_data->partial_win -
			(tap_data->full_win_end - tap_data->full_win_begin)) +
			tap_data->sampling_point;
		if (best_tap_value < 0)
			best_tap_value = 0;
	} else {
		best_tap_value = tap_data->full_win_begin +
			tap_data->sampling_point;
	}
	return best_tap_value;
}

/*
 * Calculation of best tap value for high frequencies(156MHz).
 * Tap window data at 1.25V core voltage
 * X = Partial win, Y = Full win start, Z = Full win end.
 * Full Window = Z-Y.
 * UI = Z-X.
 * Tap_margin = (0.20375)UI
 *
 * Tap window data at 1.1V core voltage
 * X' = Partial win, Y' = Full win start, Z' = Full win end.
 * UI' = Z'-X'.
 * Full Window' = Z'-Y'.
 * Tap_margin' = (0.20375)UI'
 *
 * Full_window_tap=[(Z'-0.20375UI')+(Y+0.20375UI)]/2
 * Partial_window_tap=[(X'-0.20375UI')+(X-(Z-Y)+0x20375UI)]/2
 * if(Partial_window_tap < 0), Partial_window_tap=0
 *
 * Full_window_quality=[(Z'-0.20375UI')-(Y+0.20375UI)]/2
 * Partial_window_quality=(X'-0.20375UI')-Partial_window_tap
 * if(Full_window_quality>Partial_window_quality) choose full window,
 * else choose partial window.
 * If there is no margin window for both cases,
 * best tap=(Y+Z')/2.
 */
static unsigned int calculate_high_freq_tap_value(struct sdhci_host *sdhci, int freq_band)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	unsigned int curr_clock;
	unsigned int max_clock;
	unsigned int best_tap;
	struct tap_window_data *vmax_tap_data;
	struct tap_window_data *vmid_tap_data;
	struct tegra_tuning_data *tuning_data;
	unsigned int full_win_tap;
	int partial_win_start;
	int partial_win_tap;
	int full_win_quality;
	int partial_win_quality;

	tuning_data = &tegra_host->tuning_data[freq_band];
	vmax_tap_data = tuning_data->tap_data[0];
	vmid_tap_data = tuning_data->tap_data[1];

	/*
	 * If tuning at min override voltage is not done or one shot tuning is
	 * done, set the best tap value as 50% of the full window.
	 */
	if (!tuning_data->overide_vcore_tuning_done ||
		tuning_data->one_shot_tuning) {
		dev_info(mmc_dev(sdhci->mmc),
			"Setting best tap as 50 percent of the full window\n");
		best_tap = (vmax_tap_data->full_win_begin +
			(((vmax_tap_data->full_win_end -
			vmax_tap_data->full_win_begin) * 5) / 10));
		return best_tap;
	}

	curr_clock = sdhci->max_clk / 1000000;
	max_clock = uhs_max_freq_MHz[sdhci->mmc->ios.timing];

	/*
	 * Calculate the tuning_ui and sampling points for tap windows found
	 * at all core voltages.
	 */
	vmax_tap_data->tuning_ui = vmax_tap_data->full_win_end -
		vmax_tap_data->partial_win;
	vmax_tap_data->sampling_point =
		(vmax_tap_data->tuning_ui * curr_clock) / max_clock;
	vmax_tap_data->sampling_point >>= 2;

	vmid_tap_data->tuning_ui = vmid_tap_data->full_win_end -
		vmid_tap_data->partial_win;
	vmid_tap_data->sampling_point =
		(vmid_tap_data->tuning_ui * curr_clock) / max_clock;
	vmid_tap_data->sampling_point >>= 2;

	full_win_tap = ((vmid_tap_data->full_win_end -
		vmid_tap_data->sampling_point) +
		(vmax_tap_data->full_win_begin +
		vmax_tap_data->sampling_point));
	full_win_tap >>= 1;
	full_win_quality = (vmid_tap_data->full_win_end -
		vmid_tap_data->sampling_point) -
		(vmax_tap_data->full_win_begin +
		vmax_tap_data->sampling_point);
	full_win_quality >>= 1;

	partial_win_start = (vmax_tap_data->partial_win -
		(vmax_tap_data->full_win_end -
		vmax_tap_data->full_win_begin));
	partial_win_tap = ((vmid_tap_data->partial_win -
		vmid_tap_data->sampling_point) +
		(partial_win_start + vmax_tap_data->sampling_point));
	partial_win_tap >>= 1;
	if (partial_win_tap < 0)
		partial_win_tap = 0;
	partial_win_quality = (vmid_tap_data->partial_win -
		vmid_tap_data->sampling_point) - partial_win_tap;

	if ((full_win_quality <= 0) && (partial_win_quality)) {
		dev_warn(mmc_dev(sdhci->mmc),
			"No margin window for both windows\n");
		best_tap = vmax_tap_data->full_win_begin +
			vmid_tap_data->full_win_end;
		best_tap >>= 1;
	} else {
		if (full_win_quality > partial_win_quality) {
			best_tap = full_win_tap;
		} else {
			best_tap = partial_win_tap;
			tuning_data->select_partial_win = true;
		}
	}
	return best_tap;
}

static int sdhci_tegra_run_frequency_tuning(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	int err = 0;
	u8 ctrl;
	u32 mask;
	unsigned int timeout = 10;
	int flags;
	u32 intstatus;

	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;
	while (sdhci_readl(sdhci, SDHCI_PRESENT_STATE) & mask) {
		if (timeout == 0) {
			dev_err(mmc_dev(sdhci->mmc), "Controller never"
				"released inhibit bit(s).\n");
			err = -ETIMEDOUT;
			goto out;
		}
		timeout--;
		mdelay(1);
	}

	ctrl = sdhci_readb(sdhci, SDHCI_HOST_CONTROL2);
	ctrl &= ~SDHCI_CTRL_TUNED_CLK;
	sdhci_writeb(sdhci, ctrl, SDHCI_HOST_CONTROL2);

	ctrl = sdhci_readb(sdhci, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_EXEC_TUNING;
	sdhci_writeb(sdhci, ctrl, SDHCI_HOST_CONTROL2);

	/*
	 * In response to CMD19, the card sends 64 bytes of tuning
	 * block to the Host Controller. So we set the block size
	 * to 64 here.
	 * In response to CMD21, the card sends 128 bytes of tuning
	 * block for MMC_BUS_WIDTH_8 and 64 bytes for MMC_BUS_WIDTH_4
	 * to the Host Controller. So we set the block size to 64 here.
	 */
	sdhci_writew(sdhci, SDHCI_MAKE_BLKSZ(7, tegra_host->tuning_bsize),
		SDHCI_BLOCK_SIZE);

	sdhci_writeb(sdhci, 0xE, SDHCI_TIMEOUT_CONTROL);

	sdhci_writew(sdhci, SDHCI_TRNS_READ, SDHCI_TRANSFER_MODE);

	sdhci_writel(sdhci, 0x0, SDHCI_ARGUMENT);

	/* Set the cmd flags */
	flags = SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_DATA;
	/* Issue the command */
	sdhci_writew(sdhci, SDHCI_MAKE_CMD(
		tegra_host->tuning_opcode, flags), SDHCI_COMMAND);

	timeout = 5;
	do {
		timeout--;
		mdelay(1);
		intstatus = sdhci_readl(sdhci, SDHCI_INT_STATUS);
		if (intstatus) {
			sdhci_writel(sdhci, intstatus, SDHCI_INT_STATUS);
			break;
		}
	} while(timeout);

	if ((intstatus & SDHCI_INT_DATA_AVAIL) &&
		!(intstatus & SDHCI_INT_DATA_CRC)) {
		err = 0;
		sdhci->tuning_done = 1;
	} else {
		tegra_sdhci_reset(sdhci, SDHCI_RESET_CMD);
		tegra_sdhci_reset(sdhci, SDHCI_RESET_DATA);
		err = -EIO;
	}

	if (sdhci->tuning_done) {
		sdhci->tuning_done = 0;
		ctrl = sdhci_readb(sdhci, SDHCI_HOST_CONTROL2);
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING) &&
			(ctrl & SDHCI_CTRL_TUNED_CLK))
			err = 0;
		else
			err = -EIO;
	}
	mdelay(1);
out:
	return err;
}

static int sdhci_tegra_scan_tap_values(struct sdhci_host *sdhci,
	unsigned int starting_tap, bool expect_failure)
{
	unsigned int tap_value = starting_tap;
	int err;
	unsigned int retry = TUNING_RETRIES;

	do {
		/* Set the tap delay */
		sdhci_tegra_set_tap_delay(sdhci, tap_value);

		/* Run frequency tuning */
		err = sdhci_tegra_run_frequency_tuning(sdhci);
		if (err && retry) {
			retry--;
			continue;
		} else {
			retry = TUNING_RETRIES;
			if ((expect_failure && !err) ||
				(!expect_failure && err))
				break;
		}
		tap_value++;
	} while (tap_value <= MAX_TAP_VALUES);

	return tap_value;
}

/*
 * While scanning for tap values, first get the partial window followed by the
 * full window. Note that, when scanning for full win start, tuning has to be
 * run until a passing tap value is found. Hence, failure is expected during
 * this process and ignored.
 */
static int sdhci_tegra_get_tap_window_data(struct sdhci_host *sdhci,
	struct tap_window_data *tap_data)
{
	unsigned int tap_value;
	int err = 0;

	if (!tap_data) {
		dev_err(mmc_dev(sdhci->mmc), "Invalid tap data\n");
		return -ENODATA;
	}

	/* Get the partial window data */
	tap_value = 0;
	tap_value = sdhci_tegra_scan_tap_values(sdhci, tap_value, false);
	if (!tap_value) {
		tap_data->abandon_partial_win = true;
		tap_data->partial_win = 0;
	} else if (tap_value > MAX_TAP_VALUES) {
		/*
		 * If tap value is more than 0xFF, we have hit the miracle case
		 * of all tap values passing. Discard full window as passing
		 * window has covered all taps.
		 */
		tap_data->partial_win = MAX_TAP_VALUES;
		tap_data->abandon_full_win = true;
		goto out;
	} else {
		tap_data->partial_win = tap_value - 1;
		if (tap_value == MAX_TAP_VALUES) {
			/* All tap values exhausted. No full window */
			tap_data->abandon_full_win = true;
			goto out;
		}
	}

	/* Get the full window start */
	tap_value++;
	tap_value = sdhci_tegra_scan_tap_values(sdhci, tap_value, true);
	if (tap_value > MAX_TAP_VALUES) {
		/* All tap values exhausted. No full window */
		tap_data->abandon_full_win = true;
		goto out;
	} else {
		tap_data->full_win_begin = tap_value;
		/*
		 * If full win start is 0xFF, then set that as full win end
		 * and exit.
		 */
		if (tap_value == MAX_TAP_VALUES) {
			tap_data->full_win_end = tap_value;
			goto out;
		}
	}

	/* Get the full window end */
	tap_value++;
	tap_value = sdhci_tegra_scan_tap_values(sdhci, tap_value, false);
	tap_data->full_win_end = tap_value - 1;
	if (tap_value > MAX_TAP_VALUES)
		tap_data->full_win_end = MAX_TAP_VALUES;
out:
	/*
	 * Mark tuning as failed if both partial and full windows are
	 * abandoned.
	 */
	if (tap_data->abandon_partial_win && tap_data->abandon_full_win)
		err = -EIO;
	return err;
}

static int sdhci_tegra_execute_tuning(struct sdhci_host *sdhci, u32 opcode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	struct tap_window_data *tap_data;
	int err;
	u16 ctrl_2;
	u32 ier;
	int freq_band;
	unsigned int i = 0;
	unsigned int voltage = 0;
#ifdef CONFIG_MMC_FREQ_SCALING
	bool single_freq_tuning = true;
#endif
	bool vcore_override_failed = false;
	static unsigned int vcore_lvl;

	/* Tuning is valid only in SDR104 and SDR50 modes */
	ctrl_2 = sdhci_readw(sdhci, SDHCI_HOST_CONTROL2);
	if (!(((ctrl_2 & SDHCI_CTRL_UHS_MASK) == SDHCI_CTRL_UHS_SDR104) ||
		(((ctrl_2 & SDHCI_CTRL_UHS_MASK) == SDHCI_CTRL_UHS_SDR50) &&
		(sdhci->flags & SDHCI_SDR50_NEEDS_TUNING))))
			return 0;

	/* Tuning should be done only for MMC_BUS_WIDTH_8 and MMC_BUS_WIDTH_4 */
	if (sdhci->mmc->ios.bus_width == MMC_BUS_WIDTH_8)
		tegra_host->tuning_bsize = MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_8;
	else if (sdhci->mmc->ios.bus_width == MMC_BUS_WIDTH_4)
		tegra_host->tuning_bsize = MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_4;
	else
		return -EINVAL;

	sdhci->flags &= ~SDHCI_NEEDS_RETUNING;

	/* Set the tuning command to be used */
	tegra_host->tuning_opcode = opcode;

	/*
	 * Disable all interrupts signalling.Enable interrupt status
	 * detection for buffer read ready and data crc. We use
	 * polling for tuning as it involves less overhead.
	 */
	ier = sdhci_readl(sdhci, SDHCI_INT_ENABLE);
	sdhci_writel(sdhci, 0, SDHCI_SIGNAL_ENABLE);
	sdhci_writel(sdhci, SDHCI_INT_DATA_AVAIL |
		SDHCI_INT_DATA_CRC, SDHCI_INT_ENABLE);

	if (sdhci->max_clk > tuning_params[TUNING_LOW_FREQ].freq_hz) {
		if (tegra_host->tap_cmd == TAP_CMD_TRIM_HIGH_VOLTAGE)
			freq_band = TUNING_HIGH_FREQ_HV;
		else
			freq_band = TUNING_HIGH_FREQ;
	} else
		freq_band = TUNING_LOW_FREQ;

	tuning_data = &tegra_host->tuning_data[freq_band];

	/*
	 * If tuning is already done and retune request is not set, then skip
	 * best tap value calculation and use the old best tap value.
	 */
	if (tegra_host->tuning_status == TUNING_STATUS_DONE) {
			dev_info(mmc_dev(sdhci->mmc),
			"Tuning already done. Setting tuned tap value %d\n",
			tuning_data->best_tap_value[freq_band]);
			goto set_best_tap;
	}

	/* Remove any previously set override voltages */
	if (tegra_host->set_tuning_override) {
		spin_unlock(&sdhci->lock);
		tegra_dvfs_override_core_voltage(0);
		spin_lock(&sdhci->lock);
		vcore_lvl = 0;
		tegra_host->set_tuning_override = false;
	}

#ifdef CONFIG_MMC_FREQ_SCALING
	for (freq_band = 0; freq_band < TUNING_FREQ_COUNT; freq_band++) {
		if (sdhci->mmc->caps2 & MMC_CAP2_FREQ_SCALING) {
			spin_unlock(&sdhci->lock);
			tegra_sdhci_set_clock(sdhci,
				tuning_params[freq_band].freq_hz);
			spin_lock(&sdhci->lock);
			single_freq_tuning = false;
		} else {
			single_freq_tuning = true;
		}
#endif

	/*
	 * Run tuning and get the passing tap window info for all frequencies
	 * and core voltages required to calculate the final tap value. The
	 * standard driver calls this platform specific tuning callback after
	 * holding a lock. The spinlock needs to be released when calling
	 * non-atomic context functions like regulator calls etc.
	 */
	tuning_data = &tegra_host->tuning_data[freq_band];
	for (i = 0; i < tuning_params[freq_band].nr_voltages; i++) {
		spin_unlock(&sdhci->lock);
		if (!tuning_data->tap_data[i]) {
			tuning_data->tap_data[i] = devm_kzalloc(
				mmc_dev(sdhci->mmc),
				sizeof(struct tap_window_data), GFP_KERNEL);
			if (!tuning_data->tap_data[i]) {
				err = -ENOMEM;
				dev_err(mmc_dev(sdhci->mmc),
					"Insufficient memory for tap window " \
					"info\n");
				spin_lock(&sdhci->lock);
				goto out;
			}
		}
		tap_data = tuning_data->tap_data[i];

		/*
		 * If nominal vcore is not specified, run tuning once and set
		 * the tap value. Tuning might fail but this is a better option
		 * than not trying tuning at all.
		 */
		if (!tegra_host->nominal_vcore_mv) {
			dev_err(mmc_dev(sdhci->mmc),
				"Missing nominal vcore. Tuning might fail\n");
			tuning_data->one_shot_tuning = true;
			goto skip_vcore_override;
		}

		voltage = tuning_params[freq_band].voltages[i];
		if (voltage > tegra_host->nominal_vcore_mv) {
			voltage = tegra_host->nominal_vcore_mv;
			if ((tuning_data->nominal_vcore_tuning_done) &&
			(tuning_params[freq_band].nr_voltages == 1)) {
				spin_lock(&sdhci->lock);
				continue;
			}
		} else if (voltage < tegra_host->min_vcore_override_mv) {
			voltage = tegra_host->min_vcore_override_mv;
			if ((tuning_data->overide_vcore_tuning_done) &&
			(tuning_params[freq_band].nr_voltages == 1)) {
				spin_lock(&sdhci->lock);
				continue;
			}
		}
		if (voltage != vcore_lvl) {
			/* Boost emc clock to 900MHz before setting 1.39V */
			if ((voltage == 1390) && tegra_host->emc_clk) {
				err = clk_prepare_enable(tegra_host->emc_clk);
				if (err)
					dev_err(mmc_dev(sdhci->mmc),
					"1.39V emc freq boost failed %d\n",
					err);
				else
					tegra_host->emc_clk_enabled = true;
			}
			err = tegra_dvfs_override_core_voltage(voltage);
			if (err) {
				vcore_override_failed = true;
				dev_err(mmc_dev(sdhci->mmc),
					"Setting tuning override_mv %d failed %d\n",
					voltage, err);
			} else {
				vcore_lvl = voltage;
			}
		}
		spin_lock(&sdhci->lock);

skip_vcore_override:
		/* Get the tuning window info */
		err = sdhci_tegra_get_tap_window_data(sdhci, tap_data);
		if (err) {
			dev_err(mmc_dev(sdhci->mmc), "No tuning window\n");
			goto out;
		}

		if (tuning_data->one_shot_tuning) {
			spin_lock(&sdhci->lock);
			tuning_data->nominal_vcore_tuning_done = true;
			tuning_data->overide_vcore_tuning_done = true;
			break;
		}

		if (!vcore_override_failed) {
			if (voltage == tegra_host->nominal_vcore_mv)
				tuning_data->nominal_vcore_tuning_done = true;
			if (voltage >= tegra_host->min_vcore_override_mv)
				tuning_data->overide_vcore_tuning_done = true;
		}

		/* Release the override voltage setting */
		spin_unlock(&sdhci->lock);
		err = tegra_dvfs_override_core_voltage(0);
		if (err)
			dev_err(mmc_dev(sdhci->mmc),
				"Clearing tuning override voltage failed %d\n",
					err);
		else
			vcore_lvl = 0;
		if (tegra_host->emc_clk_enabled) {
			clk_disable_unprepare(tegra_host->emc_clk);
			tegra_host->emc_clk_enabled = false;
		}

		spin_lock(&sdhci->lock);
	}
	/* If setting min override voltage failed for the first time, set
	 * nominal core voltage as override until retuning is done.
	 */
	if ((tegra_host->tuning_status != TUNING_STATUS_RETUNE) &&
		tuning_data->nominal_vcore_tuning_done &&
		!tuning_data->overide_vcore_tuning_done)
		tegra_host->set_tuning_override = true;

	/* Calculate best tap for current freq band */
	if (freq_band == TUNING_LOW_FREQ)
		tuning_data->best_tap_value[freq_band] =
			calculate_low_freq_tap_value(sdhci);
	else
		tuning_data->best_tap_value[freq_band] =
			calculate_high_freq_tap_value(sdhci, freq_band);
set_best_tap:

	/* Dump the tap window data */
	sdhci_tegra_set_tap_delay(sdhci,
			tuning_data->best_tap_value[freq_band]);
	sdhci_tegra_dump_tuning_data(sdhci, freq_band);


	/*
	 * Run tuning with the best tap value. If tuning fails, set the status
	 * for retuning next time enumeration is done.
	 */
	err = sdhci_tegra_run_frequency_tuning(sdhci);
	if (err) {
		tuning_data->nominal_vcore_tuning_done = false;
		tuning_data->overide_vcore_tuning_done = false;
		tegra_host->tuning_status = TUNING_STATUS_RETUNE;
	} else {
		if (tuning_data->nominal_vcore_tuning_done &&
			tuning_data->overide_vcore_tuning_done)
			tegra_host->tuning_status = TUNING_STATUS_DONE;
		else
			tegra_host->tuning_status = TUNING_STATUS_RETUNE;
	}
#ifdef CONFIG_MMC_FREQ_SCALING
		if (single_freq_tuning)
			break;
	}
#endif
out:
	/*
	 * Lock down the core voltage if tuning at override voltage failed
	 * for the first time. The override setting will be removed once
	 * retuning is called.
	 */
	if (tegra_host->set_tuning_override) {
		dev_info(mmc_dev(sdhci->mmc),
			"Nominal core voltage being set until retuning\n");
		spin_unlock(&sdhci->lock);
		err = tegra_dvfs_override_core_voltage(
				tegra_host->nominal_vcore_mv);
		if (err)
			dev_err(mmc_dev(sdhci->mmc),
				"Setting tuning override voltage failed %d\n",
					err);
		else
			vcore_lvl = tegra_host->nominal_vcore_mv;
		spin_lock(&sdhci->lock);

		/* Schedule for the retuning */
		mod_timer(&sdhci->tuning_timer, jiffies +
			10 * HZ);
	}

	/* Enable interrupts. Enable full range for core voltage */
	sdhci_writel(sdhci, ier, SDHCI_INT_ENABLE);
	sdhci_writel(sdhci, ier, SDHCI_SIGNAL_ENABLE);
	return err;
}

static int tegra_sdhci_suspend(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;

	tegra_sdhci_set_clock(sdhci, 0);

	/* Disable the power rails if any */
	if (tegra_host->card_present) {
		if (tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_io_reg)
				regulator_disable(tegra_host->vdd_io_reg);
			if (tegra_host->vdd_slot_reg)
				regulator_disable(tegra_host->vdd_slot_reg);
			tegra_host->is_rail_enabled = 0;
		}
	}

	return 0;
}

static int tegra_sdhci_resume(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct platform_device *pdev;
	struct tegra_sdhci_platform_data *plat;

	pdev = to_platform_device(mmc_dev(sdhci->mmc));
	plat = pdev->dev.platform_data;

	if (gpio_is_valid(plat->cd_gpio))
		tegra_host->card_present = (gpio_get_value(plat->cd_gpio) == 0);

	/* Enable the power rails if any */
	if (tegra_host->card_present) {
		if (!tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_slot_reg)
				regulator_enable(tegra_host->vdd_slot_reg);
			if (tegra_host->vdd_io_reg) {
				regulator_enable(tegra_host->vdd_io_reg);
				if (plat->mmc_data.ocr_mask &
							SDHOST_1V8_OCR_MASK)
					tegra_sdhci_signal_voltage_switch(sdhci,
							MMC_SIGNAL_VOLTAGE_180);
				else
					tegra_sdhci_signal_voltage_switch(sdhci,
							MMC_SIGNAL_VOLTAGE_330);
			}
			tegra_host->is_rail_enabled = 1;
		}
	}

	/* Setting the min identification clock of freq 400KHz */
	tegra_sdhci_set_clock(sdhci, 400000);

	/* Reset the controller and power on if MMC_KEEP_POWER flag is set*/
	if (sdhci->mmc->pm_flags & MMC_PM_KEEP_POWER) {
		tegra_sdhci_reset(sdhci, SDHCI_RESET_ALL);
		sdhci_writeb(sdhci, SDHCI_POWER_ON, SDHCI_POWER_CONTROL);
		sdhci->pwr = 0;
	}

	return 0;
}

static void tegra_sdhci_post_resume(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;

	if (tegra_host->card_present) {
		if (tegra_host->sd_detect_in_suspend)
			tasklet_schedule(&sdhci->card_tasklet);
	} else if (tegra_host->clk_enabled) {
		/* Turn OFF the clocks if the card is not present */
		tegra_sdhci_set_clock(sdhci, 0);
	}
}

static int show_polling_period(void *data, u64 *value)
{
	struct sdhci_host *host = (struct sdhci_host *)data;

	if (host->mmc->dev_stats != NULL)
		*value = host->mmc->dev_stats->polling_interval;

	return 0;
}

static int set_polling_period(void *data, u64 value)
{
	struct sdhci_host *host = (struct sdhci_host *)data;

	if (host->mmc->dev_stats != NULL) {
		/* Limiting the maximum polling period to 1 sec */
		if (value > 1000)
			value = 1000;
		host->mmc->dev_stats->polling_interval = value;
	}

	return 0;
}
static int show_active_load_high_threshold(void *data, u64 *value)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_freq_gov_data *gov_data = tegra_host->gov_data;

	if (gov_data != NULL)
		*value = gov_data->act_load_high_threshold;

	return 0;
}

static int set_active_load_high_threshold(void *data, u64 value)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_freq_gov_data *gov_data = tegra_host->gov_data;

	if (gov_data != NULL) {
		/* Maximum threshold load percentage is 100.*/
		if (value > 100)
			value = 100;
		gov_data->act_load_high_threshold = value;
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(sdhci_polling_period_fops, show_polling_period,
		set_polling_period, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(sdhci_active_load_high_threshold_fops,
		show_active_load_high_threshold,
		set_active_load_high_threshold, "%llu\n");

static void sdhci_tegra_error_stats_debugfs(struct sdhci_host *host)
{
	struct dentry *root;
	struct dentry *dfs_root;

	root = debugfs_create_dir(dev_name(mmc_dev(host->mmc)), NULL);
	if (IS_ERR(root))
		/* Don't complain -- debugfs just isn't enabled */
		return;
	if (!root)
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		goto err_root;

	host->debugfs_root = root;

	dfs_root = debugfs_create_dir("dfs_stats_dir", root);
	if (IS_ERR_OR_NULL(dfs_root))
		goto err_node;

	if (!debugfs_create_file("error_stats", S_IRUSR, root, host,
				&sdhci_host_fops))
		goto err_node;
	if (!debugfs_create_file("dfs_stats", S_IRUSR, dfs_root, host,
				&sdhci_host_dfs_fops))
		goto err_node;
	if (!debugfs_create_file("polling_period", 0644, dfs_root, (void *)host,
				&sdhci_polling_period_fops))
		goto err_node;
	if (!debugfs_create_file("active_load_high_threshold", 0644,
				dfs_root, (void *)host,
				&sdhci_active_load_high_threshold_fops))
		goto err_node;
	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	pr_err("%s: Failed to initialize debugfs functionality\n", __func__);
	return;
}

static struct sdhci_ops tegra_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.get_cd     = tegra_sdhci_get_cd,
	.read_l     = tegra_sdhci_readl,
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	.read_w     = tegra_sdhci_readw,
#endif
	.write_l    = tegra_sdhci_writel,
	.write_w    = tegra_sdhci_writew,
	.platform_8bit_width = tegra_sdhci_8bit,
	.set_clock		= tegra_sdhci_set_clock,
	.suspend		= tegra_sdhci_suspend,
	.resume			= tegra_sdhci_resume,
	.platform_resume	= tegra_sdhci_post_resume,
	.platform_reset_exit	= tegra_sdhci_reset_exit,
	.set_uhs_signaling	= tegra_sdhci_set_uhs_signaling,
	.switch_signal_voltage	= tegra_sdhci_signal_voltage_switch,
	.switch_signal_voltage_exit = tegra_sdhci_do_calibration,
	.execute_freq_tuning	= sdhci_tegra_execute_tuning,
	.sd_error_stats		= sdhci_tegra_sd_error_stats,
#ifdef CONFIG_MMC_FREQ_SCALING
	.dfs_gov_init		= sdhci_tegra_freq_gov_init,
	.dfs_gov_get_target_freq	= sdhci_tegra_get_target_freq,
#endif
};

static struct sdhci_pltfm_data sdhci_tegra20_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_NON_STD_VOLTAGE_SWITCHING |
		  SDHCI_QUIRK_NON_STANDARD_TUNING |
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
		  SDHCI_QUIRK_NONSTANDARD_CLOCK |
#endif
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_NO_CALC_MAX_DISCARD_TO,
	.quirks2 = SDHCI_QUIRK2_BROKEN_PRESET_VALUES,
	.ops  = &tegra_sdhci_ops,
};

static struct sdhci_tegra_soc_data soc_data_tegra20 = {
	.pdata = &sdhci_tegra20_pdata,
	.nvquirks = NVQUIRK_FORCE_SDHCI_SPEC_200 |
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
		   NVQUIRK_ENABLE_PADPIPE_CLKEN |
		   NVQUIRK_DISABLE_SPI_MODE_CLKEN |
		   NVQUIRK_EN_FEEDBACK_CLK |
		   NVQUIRK_SET_TAP_DELAY |
		   NVQUIRK_ENABLE_SDR50_TUNING |
		   NVQUIRK_ENABLE_SDR50 |
		   NVQUIRK_ENABLE_SDR104 |
#endif
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
		    NVQUIRK_SET_DRIVE_STRENGTH |
#endif
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		    NVQUIRK_DISABLE_AUTO_CALIBRATION |
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
		    NVQUIRK_SET_CALIBRATION_OFFSETS |
		    NVQUIRK_ENABLE_SD_3_0 |
#else
		    NVQUIRK_SET_TRIM_DELAY |
		    NVQUIRK_ENABLE_DDR50 |
		    NVQUIRK_INFINITE_ERASE_TIMEOUT |
		    NVQUIRK_DISABLE_AUTO_CMD23 |
#endif
		    NVQUIRK_ENABLE_BLOCK_GAP_DET,
};

#ifdef CONFIG_ARCH_TEGRA_3x_SOC
static struct sdhci_pltfm_data sdhci_tegra30_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC,
	.ops  = &tegra_sdhci_ops,
};

static struct sdhci_tegra_soc_data soc_data_tegra30 = {
	.pdata = &sdhci_tegra30_pdata,
};
#endif

static const struct of_device_id sdhci_tegra_dt_match[] __devinitdata = {
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	{ .compatible = "nvidia,tegra30-sdhci", .data = &soc_data_tegra30 },
#endif
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	{ .compatible = "nvidia,tegra20-sdhci", .data = &soc_data_tegra20 },
#endif
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_dt_ids);

static struct tegra_sdhci_platform_data * __devinit sdhci_tegra_dt_parse_pdata(
						struct platform_device *pdev)
{
	struct tegra_sdhci_platform_data *plat;
	struct device_node *np = pdev->dev.of_node;

	if (!np)
		return NULL;

	plat = devm_kzalloc(&pdev->dev, sizeof(*plat), GFP_KERNEL);
	if (!plat) {
		dev_err(&pdev->dev, "Can't allocate platform data\n");
		return NULL;
	}

	plat->cd_gpio = of_get_named_gpio(np, "cd-gpios", 0);
	plat->wp_gpio = of_get_named_gpio(np, "wp-gpios", 0);
	plat->power_gpio = of_get_named_gpio(np, "power-gpios", 0);
	if (of_find_property(np, "support-8bit", NULL))
		plat->is_8bit = 1;

	return plat;
}

static void tegra_sdhci_rail_off(struct sdhci_tegra *tegra_host)
{
	if (tegra_host->is_rail_enabled) {
		if (tegra_host->vdd_slot_reg)
			regulator_disable(tegra_host->vdd_slot_reg);
		if (tegra_host->vdd_io_reg)
			regulator_disable(tegra_host->vdd_io_reg);
		tegra_host->is_rail_enabled = false;
	}
}

static int tegra_sdhci_reboot_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sdhci_tegra *tegra_host =
		container_of(nb, struct sdhci_tegra, reboot_notify);

	switch (event) {
	case SYS_RESTART:
	case SYS_POWER_OFF:
		tegra_sdhci_rail_off(tegra_host);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static ssize_t sdhci_handle_boost_mode_tap(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int tap_cmd;
	struct mmc_card *card;
	char *p = (char *)buf;
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;

	tap_cmd = memparse(p, &p);

	card = host->mmc->card;
	if (!card)
		return count;

	/* if not uhs -- no tuning and no tap value to set */
	if (!mmc_sd_card_uhs(card) && !mmc_card_hs200(card))
		return count;

	/* if no change in tap value -- just exit */
	if (tap_cmd == tegra_host->tap_cmd)
		return count;

	switch (tap_cmd) {
	case TAP_CMD_GO_COMMAND:
	case TAP_CMD_SUSPEND_CONTROLLER:
		return count;

	case TAP_CMD_TRIM_DEFAULT_VOLTAGE:
	case TAP_CMD_TRIM_HIGH_VOLTAGE:
		break;

	default:
		pr_info("\necho 1 > cmd_state  # to set normal voltage\n" \
			  "echo 2 > cmd_state  # to set high voltage\n");
		return count;
	}

	spin_lock(&host->lock);
	disable_irq(host->irq);
	tegra_host->tap_cmd = tap_cmd;
	tuning_data = &tegra_host->tuning_data[tap_cmd];
	switch (tap_cmd) {
	case TAP_CMD_TRIM_DEFAULT_VOLTAGE:
		/* set tap value for voltage range 1.1 to 1.25 */
		sdhci_tegra_set_tap_delay(host,
			tuning_data->best_tap_value[TUNING_HIGH_FREQ]);
		break;

	case TAP_CMD_TRIM_HIGH_VOLTAGE:
		/*  set tap value for voltage range 1.25 to 1.39 volts */
		sdhci_tegra_set_tap_delay(host,
			tuning_data->best_tap_value[TUNING_HIGH_FREQ_HV]);
		break;
	default:
		break;
	}
	spin_unlock(&host->lock);
	enable_irq(host->irq);
	return count;
}

static ssize_t sdhci_show_turbo_mode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;

	return sprintf(buf, "%d\n", tegra_host->tap_cmd);
}

static DEVICE_ATTR(cmd_state, 0644, sdhci_show_turbo_mode,
			sdhci_handle_boost_mode_tap);

static int __devinit sdhci_tegra_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sdhci_tegra_soc_data *soc_data;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct tegra_sdhci_platform_data *plat;
	struct sdhci_tegra *tegra_host;
	int rc;

	match = of_match_device(sdhci_tegra_dt_match, &pdev->dev);
	if (match)
		soc_data = match->data;
	else
		soc_data = &soc_data_tegra20;

	host = sdhci_pltfm_init(pdev, soc_data->pdata);
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);

	plat = pdev->dev.platform_data;

	if (plat == NULL)
		plat = sdhci_tegra_dt_parse_pdata(pdev);

	if (plat == NULL) {
		dev_err(mmc_dev(host->mmc), "missing platform data\n");
		rc = -ENXIO;
		goto err_no_plat;
	}

	tegra_host = devm_kzalloc(&pdev->dev, sizeof(*tegra_host), GFP_KERNEL);
	if (!tegra_host) {
		dev_err(mmc_dev(host->mmc), "failed to allocate tegra_host\n");
		rc = -ENOMEM;
		goto err_no_plat;
	}

	tegra_host->plat = plat;
	tegra_host->sd_stat_head = devm_kzalloc(&pdev->dev, sizeof(
						struct sdhci_tegra_sd_stats),
						GFP_KERNEL);
	if (tegra_host->sd_stat_head == NULL) {
		rc = -ENOMEM;
		goto err_no_plat;
	}

	tegra_host->soc_data = soc_data;

	pltfm_host->priv = tegra_host;

	pll_c = clk_get_sys(NULL, "pll_c");
	if (IS_ERR(pll_c)) {
		rc = PTR_ERR(pll_c);
		dev_err(mmc_dev(host->mmc),
			"clk error in getting pll_c: %d\n", rc);
	}

	pll_p = clk_get_sys(NULL, "pll_p");
	if (IS_ERR(pll_p)) {
		rc = PTR_ERR(pll_p);
		dev_err(mmc_dev(host->mmc),
			"clk error in getting pll_p: %d\n", rc);
	}

	pll_c_rate = clk_get_rate(pll_c);
	pll_p_rate = clk_get_rate(pll_p);

#ifdef CONFIG_MMC_EMBEDDED_SDIO
	if (plat->mmc_data.embedded_sdio)
		mmc_set_embedded_sdio_data(host->mmc,
			&plat->mmc_data.embedded_sdio->cis,
			&plat->mmc_data.embedded_sdio->cccr,
			plat->mmc_data.embedded_sdio->funcs,
			plat->mmc_data.embedded_sdio->num_funcs);
#endif

	if (gpio_is_valid(plat->power_gpio)) {
		rc = gpio_request(plat->power_gpio, "sdhci_power");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate power gpio\n");
			goto err_power_req;
		}
		gpio_direction_output(plat->power_gpio, 1);
	}

	if (gpio_is_valid(plat->cd_gpio)) {
		rc = gpio_request(plat->cd_gpio, "sdhci_cd");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate cd gpio\n");
			goto err_cd_req;
		}
		gpio_direction_input(plat->cd_gpio);

		tegra_host->card_present = (gpio_get_value(plat->cd_gpio) == 0);

		rc = request_threaded_irq(gpio_to_irq(plat->cd_gpio), NULL,
				 carddetect_irq,
				 IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				 mmc_hostname(host->mmc), host);

		if (rc)	{
			dev_err(mmc_dev(host->mmc), "request irq error\n");
			goto err_cd_irq_req;
		}
		rc = enable_irq_wake(gpio_to_irq(plat->cd_gpio));
		if (rc < 0)
			dev_err(mmc_dev(host->mmc),
				"SD card wake-up event registration"
					"failed with eroor: %d\n", rc);

	} else if (plat->mmc_data.register_status_notify) {
		plat->mmc_data.register_status_notify(sdhci_status_notify_cb, host);
	}

	if (plat->mmc_data.status) {
		plat->mmc_data.card_present = plat->mmc_data.status(mmc_dev(host->mmc));
	}

	if (gpio_is_valid(plat->wp_gpio)) {
		rc = gpio_request(plat->wp_gpio, "sdhci_wp");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate wp gpio\n");
			goto err_wp_req;
		}
		gpio_direction_input(plat->wp_gpio);
	}

	/*
	 * If there is no card detect gpio, assume that the
	 * card is always present.
	 */
	if (!gpio_is_valid(plat->cd_gpio))
		tegra_host->card_present = 1;

	if (plat->mmc_data.ocr_mask & SDHOST_1V8_OCR_MASK) {
		tegra_host->vddio_min_uv = SDHOST_LOW_VOLT_MIN;
		tegra_host->vddio_max_uv = SDHOST_LOW_VOLT_MAX;
	} else if (plat->mmc_data.ocr_mask & MMC_OCR_2V8_MASK) {
			tegra_host->vddio_min_uv = SDHOST_HIGH_VOLT_2V8;
			tegra_host->vddio_max_uv = SDHOST_HIGH_VOLT_MAX;
	} else {
		/*
		 * Set the minV and maxV to default
		 * voltage range of 2.7V - 3.6V
		 */
		tegra_host->vddio_min_uv = SDHOST_HIGH_VOLT_MIN;
		tegra_host->vddio_max_uv = SDHOST_HIGH_VOLT_MAX;
	}

	tegra_host->vdd_io_reg = regulator_get(mmc_dev(host->mmc),
							"vddio_sdmmc");
	if (IS_ERR_OR_NULL(tegra_host->vdd_io_reg)) {
		dev_info(mmc_dev(host->mmc), "%s regulator not found: %ld."
			"Assuming vddio_sdmmc is not required.\n",
			"vddio_sdmmc", PTR_ERR(tegra_host->vdd_io_reg));
		tegra_host->vdd_io_reg = NULL;
	} else {
		rc = regulator_set_voltage(tegra_host->vdd_io_reg,
			tegra_host->vddio_min_uv,
			tegra_host->vddio_max_uv);
		if (rc) {
			dev_err(mmc_dev(host->mmc), "%s regulator_set_voltage failed: %d",
				"vddio_sdmmc", rc);
			regulator_put(tegra_host->vdd_io_reg);
			tegra_host->vdd_io_reg = NULL;
		}
	}

	tegra_host->vdd_slot_reg = regulator_get(mmc_dev(host->mmc),
							"vddio_sd_slot");
	if (IS_ERR_OR_NULL(tegra_host->vdd_slot_reg)) {
		dev_info(mmc_dev(host->mmc), "%s regulator not found: %ld."
			" Assuming vddio_sd_slot is not required.\n",
			"vddio_sd_slot", PTR_ERR(tegra_host->vdd_slot_reg));
		tegra_host->vdd_slot_reg = NULL;
	}

	if (tegra_host->card_present) {
		if (tegra_host->vdd_slot_reg)
			regulator_enable(tegra_host->vdd_slot_reg);
		if (tegra_host->vdd_io_reg)
			regulator_enable(tegra_host->vdd_io_reg);
		tegra_host->is_rail_enabled = 1;
	}

	pm_runtime_enable(&pdev->dev);
	pltfm_host->clk = clk_get(mmc_dev(host->mmc), NULL);
	if (IS_ERR(pltfm_host->clk)) {
		dev_err(mmc_dev(host->mmc), "clk err\n");
		rc = PTR_ERR(pltfm_host->clk);
		goto err_clk_get;
	}

	if (clk_get_parent(pltfm_host->clk) == pll_c)
		tegra_host->is_parent_pllc = true;

	pm_runtime_get_sync(&pdev->dev);
	rc = clk_prepare_enable(pltfm_host->clk);
	if (rc != 0)
		goto err_clk_put;

	if (!strcmp(dev_name(mmc_dev(host->mmc)), "sdhci-tegra.0")) {
		tegra_host->emc_clk = clk_get(mmc_dev(host->mmc), "emc");
		if (IS_ERR(tegra_host->emc_clk)) {
			dev_err(mmc_dev(host->mmc), "clk err\n");
			rc = PTR_ERR(tegra_host->emc_clk);
			goto err_clk_put;
		} else
			clk_set_rate(tegra_host->emc_clk, 900000000);
	}

	tegra_host->sclk = devm_clk_get(mmc_dev(host->mmc), "sclk");
	if (IS_ERR_OR_NULL(tegra_host->sclk)) {
		dev_err(mmc_dev(host->mmc), "Can't get sclk clock\n");
		tegra_host->sclk = NULL;
	} else {
		clk_set_rate(tegra_host->sclk, 900000000);
	}

	pltfm_host->priv = tegra_host;
	tegra_host->clk_enabled = true;
	tegra_host->max_clk_limit = plat->max_clk_limit;
	tegra_host->ddr_clk_limit = plat->ddr_clk_limit;
	tegra_host->sd_detect_in_suspend = plat->sd_detect_in_suspend;
	tegra_host->instance = pdev->id;
	tegra_host->tap_cmd = TAP_CMD_TRIM_DEFAULT_VOLTAGE;
	mutex_init(&tegra_host->retune_lock);
	host->mmc->pm_caps |= plat->pm_caps;
	host->mmc->pm_flags |= plat->pm_flags;

	host->mmc->caps |= MMC_CAP_ERASE;
	/* enable 1/8V DDR capable */
	host->mmc->caps |= MMC_CAP_1_8V_DDR;
	if (plat->is_8bit)
		host->mmc->caps |= MMC_CAP_8_BIT_DATA;
	host->mmc->caps |= MMC_CAP_SDIO_IRQ;
	host->mmc->pm_caps |= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY;
	if (plat->mmc_data.built_in) {
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;
	}
	host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

#ifdef CONFIG_MMC_BKOPS
	host->mmc->caps2 |= MMC_CAP2_BKOPS;
#endif
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	tegra_host->hw_ops = &tegra_2x_sdhci_ops;
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
	tegra_host->hw_ops = &tegra_3x_sdhci_ops;
#else
	tegra_host->hw_ops = &tegra_11x_sdhci_ops;
	host->mmc->caps2 |= MMC_CAP2_HS200;
	host->mmc->caps2 |= MMC_CAP2_CACHE_CTRL;
	host->mmc->caps |= MMC_CAP_CMD23;
	host->mmc->caps2 |= MMC_CAP2_PACKED_CMD;
#endif

#ifdef CONFIG_MMC_FREQ_SCALING
	/*
	 * Enable dyamic frequency scaling support only if the platform clock
	 * limit is higher than the lowest supported frequency by tuning.
	 */
	if (plat->en_freq_scaling && (plat->max_clk_limit >
		tuning_params[TUNING_LOW_FREQ].freq_hz))
		host->mmc->caps2 |= MMC_CAP2_FREQ_SCALING;
#endif

	if (plat->nominal_vcore_mv)
		tegra_host->nominal_vcore_mv = plat->nominal_vcore_mv;
	if (plat->min_vcore_override_mv)
		tegra_host->min_vcore_override_mv = plat->min_vcore_override_mv;

	host->edp_support = plat->edp_support ? true : false;
	if (host->edp_support)
		for (rc = 0; rc < SD_EDP_NUM_STATES; rc++)
			host->edp_states[rc] = plat->edp_states[rc];

	pr_err("nominal vcore %d, override %d\n",
		tegra_host->nominal_vcore_mv,
		tegra_host->min_vcore_override_mv);
	rc = sdhci_add_host(host);

	device_create_file(&pdev->dev, &dev_attr_cmd_state);

	sdhci_tegra_error_stats_debugfs(host);
	if (rc)
		goto err_add_host;

	/* Enable async suspend/resume to reduce LP0 latency */
	device_enable_async_suspend(&pdev->dev);

	if (plat->power_off_rail) {
		tegra_host->reboot_notify.notifier_call =
			tegra_sdhci_reboot_notify;
		register_reboot_notifier(&tegra_host->reboot_notify);
	}
	return 0;

err_add_host:
	clk_put(tegra_host->emc_clk);
	clk_disable_unprepare(pltfm_host->clk);
	pm_runtime_put_sync(&pdev->dev);
err_clk_put:
	clk_put(pltfm_host->clk);
err_clk_get:
	if (gpio_is_valid(plat->wp_gpio))
		gpio_free(plat->wp_gpio);
err_wp_req:
	if (gpio_is_valid(plat->cd_gpio))
		free_irq(gpio_to_irq(plat->cd_gpio), host);
err_cd_irq_req:
	if (gpio_is_valid(plat->cd_gpio))
		gpio_free(plat->cd_gpio);
err_cd_req:
	if (gpio_is_valid(plat->power_gpio))
		gpio_free(plat->power_gpio);
err_power_req:
err_no_plat:
	sdhci_pltfm_free(pdev);
	return rc;
}

static int __devexit sdhci_tegra_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);

	sdhci_remove_host(host, dead);

	disable_irq_wake(gpio_to_irq(plat->cd_gpio));

	if (tegra_host->vdd_slot_reg) {
		regulator_disable(tegra_host->vdd_slot_reg);
		regulator_put(tegra_host->vdd_slot_reg);
	}

	if (tegra_host->vdd_io_reg) {
		regulator_disable(tegra_host->vdd_io_reg);
		regulator_put(tegra_host->vdd_io_reg);
	}

	if (gpio_is_valid(plat->wp_gpio))
		gpio_free(plat->wp_gpio);

	if (gpio_is_valid(plat->cd_gpio)) {
		free_irq(gpio_to_irq(plat->cd_gpio), host);
		gpio_free(plat->cd_gpio);
	}

	if (gpio_is_valid(plat->power_gpio))
		gpio_free(plat->power_gpio);

	if (tegra_host->clk_enabled) {
		clk_disable_unprepare(pltfm_host->clk);
		pm_runtime_put_sync(&pdev->dev);
	}
	clk_put(pltfm_host->clk);

	if (tegra_host->emc_clk && tegra_host->emc_clk_enabled)
		clk_disable_unprepare(tegra_host->emc_clk);
	if (tegra_host->sclk && tegra_host->is_sdmmc_sclk_on)
		clk_disable_unprepare(tegra_host->sclk);

	if (plat->power_off_rail)
		unregister_reboot_notifier(&tegra_host->reboot_notify);

	sdhci_pltfm_free(pdev);

	return 0;
}

static struct platform_driver sdhci_tegra_driver = {
	.driver		= {
		.name	= "sdhci-tegra",
		.owner	= THIS_MODULE,
		.of_match_table = sdhci_tegra_dt_match,
		.pm	= SDHCI_PLTFM_PMOPS,
	},
	.probe		= sdhci_tegra_probe,
	.remove		= __devexit_p(sdhci_tegra_remove),
};

module_platform_driver(sdhci_tegra_driver);

MODULE_DESCRIPTION("SDHCI driver for Tegra");
MODULE_AUTHOR("Google, Inc.");
MODULE_LICENSE("GPL v2");
