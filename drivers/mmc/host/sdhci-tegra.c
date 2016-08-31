/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>

#ifndef CONFIG_ARM64
#include <asm/gpio.h>
#endif
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/reboot.h>
#include <linux/devfreq.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>

#include <linux/platform_data/mmc-sdhci-tegra.h>
#include <mach/pinmux.h>
#include <mach/pm_domains.h>

#include "sdhci-pltfm.h"

#if 0
#define SDHCI_TEGRA_DBG(stuff...)	pr_info(stuff)
#else
#define SDHCI_TEGRA_DBG(stuff...)	do {} while (0)
#endif

#define SDHCI_VNDR_CLK_CTRL				0x100
#define SDHCI_VNDR_CLK_CTRL_SDMMC_CLK			0x1
#define SDHCI_VNDR_CLK_CTRL_PADPIPE_CLKEN_OVERRIDE	0x8
#define SDHCI_VNDR_CLK_CTRL_SPI_MODE_CLKEN_OVERRIDE	0x4
#define SDHCI_VNDR_CLK_CTRL_INPUT_IO_CLK		0x2
#define SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT		16
#define SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT		24
#define SDHCI_VNDR_CLK_CTRL_SDR50_TUNING		0x20
#define SDHCI_VNDR_CLK_CTRL_INTERNAL_CLK		0x2

#define SDHCI_VNDR_MISC_CTRL				0x120
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SDR104_SUPPORT	0x8
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SDR50_SUPPORT	0x10
#define SDHCI_VNDR_MISC_CTRL_ENABLE_DDR50_SUPPORT	0x200
#define SDHCI_VNDR_MISC_CTRL_ENABLE_SD_3_0		0x20
#define SDHCI_VNDR_MISC_CTRL_INFINITE_ERASE_TIMEOUT	0x1
#define SDHCI_VNDR_MISC_CTRL_PIPE_STAGES_MASK		0x180
#define SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT	17

#define SDHCI_VNDR_PRESET_VAL0_0	0x1d4
#define SDCLK_FREQ_SEL_HS_SHIFT		20
#define SDCLK_FREQ_SEL_DEFAULT_SHIFT	10

#define SDHCI_VNDR_PRESET_VAL1_0	0x1d8
#define SDCLK_FREQ_SEL_SDR50_SHIFT	20
#define SDCLK_FREQ_SEL_SDR25_SHIFT	10

#define SDHCI_VNDR_PRESET_VAL2_0	0x1dc
#define SDCLK_FREQ_SEL_DDR50_SHIFT	10

#define SDMMC_SDMEMCOMPPADCTRL	0x1E0
#define SDMMC_SDMEMCOMPPADCTRL_VREF_SEL_MASK	0xF
#define SDMMC_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD_MASK	0x80000000

#define SDMMC_AUTO_CAL_CONFIG	0x1E4
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_START	0x80000000
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE	0x20000000
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT	0x8

#define SDMMC_AUTO_CAL_STATUS	0x1EC
#define SDMMC_AUTO_CAL_STATUS_AUTO_CAL_ACTIVE	0x80000000
#define SDMMC_AUTO_CAL_STATUS_PULLDOWN_OFFSET	24
#define PULLUP_ADJUSTMENT_OFFSET	20

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
/* Enable HS200 mode */
#define NVQUIRK_ENABLE_HS200			BIT(14)
/* Enable Infinite Erase Timeout*/
#define NVQUIRK_INFINITE_ERASE_TIMEOUT		BIT(15)
/* No Calibration for sdmmc4 */
#define NVQUIRK_DISABLE_SDMMC4_CALIB		BIT(16)
/* ENAABLE FEEDBACK IO CLOCK */
#define NVQUIRK_EN_FEEDBACK_CLK			BIT(17)
/* Disable AUTO CMD23 */
#define NVQUIRK_DISABLE_AUTO_CMD23		BIT(18)
/* Shadow write xfer mode reg and write it alongwith CMD register */
#define NVQUIRK_SHADOW_XFER_MODE_REG		BIT(19)
/* update PAD_E_INPUT_OR_E_PWRD bit */
#define NVQUIRK_SET_PAD_E_INPUT_OR_E_PWRD	BIT(20)
/* Shadow write xfer mode reg and write it alongwith CMD register */
#define NVQUIRK_SET_PIPE_STAGES_MASK_0		BIT(21)
#define NVQUIRK_HIGH_FREQ_TAP_PROCEDURE		BIT(22)
/* Disable SDMMC3 external loopback */
#define NVQUIRK_DISABLE_EXTERNAL_LOOPBACK	BIT(23)
#define NVQUIRK_TMP_VAR_1_5_TAP_MARGIN		BIT(24)

/* Common subset of quirks for Tegra3 and later sdmmc controllers */
#define TEGRA_SDHCI_NVQUIRKS	(NVQUIRK_ENABLE_PADPIPE_CLKEN | \
		  NVQUIRK_DISABLE_SPI_MODE_CLKEN | \
		  NVQUIRK_EN_FEEDBACK_CLK | \
		  NVQUIRK_SET_TAP_DELAY | \
		  NVQUIRK_ENABLE_SDR50_TUNING | \
		  NVQUIRK_ENABLE_SDR50 | \
		  NVQUIRK_ENABLE_SDR104 | \
		  NVQUIRK_SHADOW_XFER_MODE_REG | \
		  NVQUIRK_DISABLE_AUTO_CMD23)

#define TEGRA_SDHCI_QUIRKS		(SDHCI_QUIRK_BROKEN_TIMEOUT_VAL | \
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK | \
		  SDHCI_QUIRK_SINGLE_POWER_WRITE | \
		  SDHCI_QUIRK_NO_HISPD_BIT | \
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC | \
		  SDHCI_QUIRK_BROKEN_CARD_DETECTION)

#define TEGRA_SDHCI_QUIRKS2	(SDHCI_QUIRK2_PRESET_VALUE_BROKEN | \
		  SDHCI_QUIRK2_NON_STD_VOLTAGE_SWITCHING | \
		  SDHCI_QUIRK2_NON_STANDARD_TUNING | \
		  SDHCI_QUIRK2_NO_CALC_MAX_DISCARD_TO | \
		  SDHCI_QUIRK2_REG_ACCESS_REQ_HOST_CLK)

#define IS_QUIRKS2_DELAYED_CLK_GATE(host) \
		(host->quirks2 & SDHCI_QUIRK2_DELAYED_CLK_GATE)

/* Interface voltages */
#define SDHOST_1V8_OCR_MASK	0x8
#define SDHOST_HIGH_VOLT_MIN	2700000
#define SDHOST_HIGH_VOLT_MAX	3600000
#define SDHOST_HIGH_VOLT_2V8	2800000
#define SDHOST_LOW_VOLT_MIN	1800000
#define SDHOST_LOW_VOLT_MAX	1800000
#define SDHOST_HIGH_VOLT_3V2	3200000
#define SDHOST_HIGH_VOLT_3V3	3300000

/* Clock related definitions */
#define MAX_DIVISOR_VALUE	128
#define DEFAULT_SDHOST_FREQ	50000000
#define SDMMC_AHB_MAX_FREQ	150000000
#define SDMMC_EMC_MAX_FREQ	150000000
#define SDMMC_EMC_NOM_VOLT_FREQ	900000000

/* Tuning related definitions */
#define MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_8	128
#define MMC_TUNING_BLOCK_SIZE_BUS_WIDTH_4	64
#define MAX_TAP_VALUES	255
#define TUNING_FREQ_COUNT	3
#define TUNING_VOLTAGES_COUNT	3
#define TUNING_RETRIES	1
#define DFS_FREQ_COUNT	2
#define NEG_MAR_CHK_WIN_COUNT	2
/* Tuning core voltage requirements */
#define NOMINAL_VCORE_TUN	BIT(0)
#define BOOT_VCORE_TUN	BIT(1)
#define MIN_OVERRIDE_VCORE_TUN	BIT(2)

/* Tap cmd sysfs commands */
#define TAP_CMD_TRIM_DEFAULT_VOLTAGE	1
#define TAP_CMD_TRIM_HIGH_VOLTAGE	2

/*
 * Defined the chip specific quirks and clock sources. For now, the used clock
 * sources vary only from chip to chip. If the sources allowed varies from
 * platform to platform, then move the clock sources list to platform data.
 * When filling the tuning_freq_list in soc_data, the number of entries should
 * be equal to TUNNG_FREQ_COUNT. Depending on number DFS frequencies supported,
 * set the desired low, high or max frequencies and set the remaining entries
 * as 0s. The number of entries should always be equal to TUNING_FREQ_COUNT
 * inorder to get the right tuning data.
 */
struct sdhci_tegra_soc_data {
	const struct sdhci_pltfm_data *pdata;
	u32 nvquirks;
	const char *parent_clk_list[2];
	unsigned int tuning_freq_list[TUNING_FREQ_COUNT];
	u8 t2t_coeffs_count;
	u8 tap_hole_coeffs_count;
	struct tuning_t2t_coeffs *t2t_coeffs;
	struct tap_hole_coeffs *tap_hole_coeffs;
};


enum tegra_regulator_config_ops {
	CONFIG_REG_EN,
	CONFIG_REG_DIS,
	CONFIG_REG_SET_VOLT,
};

enum tegra_tuning_freq {
	TUNING_LOW_FREQ,
	TUNING_HIGH_FREQ,
	TUNING_MAX_FREQ,
};

struct tuning_t2t_coeffs {
	const char *dev_id;
	int vmax;
	int vmin;
	unsigned int t2t_vnom_slope;
	unsigned int t2t_vnom_int;
	unsigned int t2t_vmax_slope;
	unsigned int t2t_vmax_int;
	unsigned int t2t_vmin_slope;
	unsigned int t2t_vmin_int;
};

#define SET_TUNING_COEFFS(_device_id, _vmax, _vmin, _t2t_vnom_slope,	\
	_t2t_vnom_int, _t2t_vmax_slope, _t2t_vmax_int, _t2t_vmin_slope,	\
	_t2t_vmin_int)	\
	{						\
		.dev_id = _device_id,			\
		.vmax = _vmax,				\
		.vmin = _vmin,				\
		.t2t_vnom_slope = _t2t_vnom_slope,	\
		.t2t_vnom_int = _t2t_vnom_int,		\
		.t2t_vmax_slope = _t2t_vmax_slope,	\
		.t2t_vmax_int = _t2t_vmax_int,		\
		.t2t_vmin_slope = _t2t_vmin_slope,	\
		.t2t_vmin_int = _t2t_vmin_int,		\
	}

struct tuning_t2t_coeffs t12x_tuning_coeffs[] = {
	SET_TUNING_COEFFS("sdhci-tegra.3",	1150,	950,	27,	118295,
		27,	118295,	48,	188148),
	SET_TUNING_COEFFS("sdhci-tegra.2",	1150,	950,	29,	124427,
		29, 124427,	 54,	203707),
	SET_TUNING_COEFFS("sdhci-tegra.0",	1150,	950,	25,	115933,
		25,	115933,	47,	187224),
};

struct tap_hole_coeffs {
	const char *dev_id;
	unsigned int freq_khz;
	unsigned int thole_vnom_slope;
	unsigned int thole_vnom_int;
	unsigned int thole_vmax_slope;
	unsigned int thole_vmax_int;
	unsigned int thole_vmin_slope;
	unsigned int thole_vmin_int;
};

#define SET_TAP_HOLE_COEFFS(_device_id, _freq_khz, _thole_vnom_slope,	\
	_thole_vnom_int, _thole_vmax_slope, _thole_vmax_int,	\
	_thole_vmin_slope, _thole_vmin_int)	\
	{					\
		.dev_id = _device_id,		\
		.freq_khz = _freq_khz,		\
		.thole_vnom_slope = _thole_vnom_slope,	\
		.thole_vnom_int = _thole_vnom_int,	\
		.thole_vmax_slope = _thole_vmax_slope,	\
		.thole_vmax_int = _thole_vmax_int,	\
		.thole_vmin_slope = _thole_vmin_slope,	\
		.thole_vmin_int = _thole_vmin_int,	\
	}

struct tap_hole_coeffs t12x_tap_hole_coeffs[] = {
	SET_TAP_HOLE_COEFFS("sdhci-tegra.3",	200000,	1037,	106934,	1037,
		106934,	558,	74315),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.3",	136000,	1703,	186307,	1703,
		186307,	890,	130617),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.3",	100000,	2452,	275601,	2452,
		275601,	1264,	193957),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.3",	81600,	3090,	351666,	3090,
		351666,	1583,	247913),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.2",	204000,	468,	36031,	468,
		36031,	253,	21264),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.2",	136000,	1146,	117841,	1146,
		117841,	589,	78993),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.2",	100000,	1879,	206195,	1879,
		206195,	953,	141341),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.2",	81600,	2504,	281460,	2504,
		281460,	1262,	194452),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.0",	204000,	874,	85243,	874,
		85243,	449,	57321),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.0",	136000,	1554,	167210,	1554,
		167210,	793,	115672),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.0",	100000,	2290,	255734,	2290,
		255734,	1164,	178691),
	SET_TAP_HOLE_COEFFS("sdhci-tegra.0",	81600,	2916,	331143,	2916,
		331143,	1480,	232373),
};

struct freq_tuning_constraints {
	unsigned int vcore_mask;
};

static struct freq_tuning_constraints tuning_vcore_constraints[3] = {
	[0] = {
		.vcore_mask = BOOT_VCORE_TUN,
	},
	[1] = {
		.vcore_mask = BOOT_VCORE_TUN,
	},
	[2] = {
		.vcore_mask = BOOT_VCORE_TUN,
	},
};

struct tuning_ui {
	int ui;
	bool is_valid_ui;
};

enum tap_win_edge_attr {
	WIN_EDGE_BOUN_START,
	WIN_EDGE_BOUN_END,
	WIN_EDGE_HOLE,
};

struct tap_window_data {
	int win_start;
	int win_end;
	enum tap_win_edge_attr win_start_attr;
	enum tap_win_edge_attr win_end_attr;
	u8 win_size;
	u8 hole_pos;
};

struct tuning_values {
	int t2t_vmax;
	int t2t_vmin;
	int ui;
	int ui_vmin;
	int vmax_thole;
	int vmin_thole;
};
struct tegra_tuning_data {
	unsigned int freq_hz;
	int best_tap_value;
	int nom_best_tap_value;
	struct freq_tuning_constraints constraints;
	struct tap_hole_coeffs *thole_coeffs;
	struct tuning_t2t_coeffs *t2t_coeffs;
	struct tuning_values est_values;
	struct tuning_values calc_values;
	struct tap_window_data *tap_data;
	struct tap_window_data *final_tap_data;
	u8 num_of_valid_tap_wins;
	u8 nr_voltages;
	u8 freq_band;
	bool tuning_done;
	bool is_partial_win_valid;
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

struct tegra_freq_gov_data {
	unsigned int		curr_active_load;
	unsigned int		avg_active_load;
	unsigned int		act_load_high_threshold;
	unsigned int		max_idle_monitor_cycles;
	unsigned int		curr_freq;
	unsigned int		freqs[DFS_FREQ_COUNT];
	unsigned int		freq_switch_count;
	bool			monitor_idle_load;
};

struct sdhci_tegra_sd_stats {
	unsigned int data_crc_count;
	unsigned int cmd_crc_count;
	unsigned int data_to_count;
	unsigned int cmd_to_count;
};

struct sdhci_tegra {
	const struct tegra_sdhci_platform_data *plat;
	const struct sdhci_tegra_soc_data *soc_data;
	bool	clk_enabled;
	/* ensure atomic set clock calls */
	struct mutex		set_clock_mutex;
	struct regulator *vdd_io_reg;
	struct regulator *vdd_slot_reg;
	struct regulator *vcore_reg;
	/* Host controller instance */
	unsigned int instance;
	/* vddio_min */
	unsigned int vddio_min_uv;
	/* vddio_max */
	unsigned int vddio_max_uv;
	/* DDR and low speed modes clock */
	struct clk *ddr_clk;
	/* HS200, SDR104 modes clock */
	struct clk *sdr_clk;
	/* Check if ddr_clk is being used */
	bool is_ddr_clk_set;
	/* max clk supported by the platform */
	unsigned int max_clk_limit;
	/* max ddr clk supported by the platform */
	unsigned int ddr_clk_limit;
	bool card_present;
	bool is_rail_enabled;
	struct clk *emc_clk;
	bool is_sdmmc_emc_clk_on;
	struct clk *sclk;
	bool is_sdmmc_sclk_on;
	struct sdhci_tegra_sd_stats *sd_stat_head;
	struct notifier_block reboot_notify;
	bool is_parent_pllc;
	bool set_1v8_calib_offsets;
	int nominal_vcore_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	/* Tuning related structures and variables */
	/* Tuning opcode to be used */
	unsigned int tuning_opcode;
	/* Tuning packet size */
	unsigned int tuning_bsize;
	/* Num of tuning freqs selected */
	int tuning_freq_count;
	unsigned int tap_cmd;
	/* Tuning status */
	unsigned int tuning_status;
	bool force_retune;
#define TUNING_STATUS_DONE	1
#define TUNING_STATUS_RETUNE	2
	/* Freq tuning information for each sampling clock freq */
	struct tegra_tuning_data tuning_data[DFS_FREQ_COUNT];
	struct tegra_freq_gov_data *gov_data;
	u32 speedo;
};

static struct clk *pll_c;
static struct clk *pll_p;
static unsigned long pll_c_rate;
static unsigned long pll_p_rate;
static bool vcore_overrides_allowed;
static bool maintain_boot_voltage;
static unsigned int boot_volt_req_refcount;
DEFINE_MUTEX(tuning_mutex);

static struct tegra_tuning_data *sdhci_tegra_get_tuning_data(
	struct sdhci_host *sdhci, unsigned int clock);
static unsigned long get_nearest_clock_freq(unsigned long pll_rate,
		unsigned long desired_rate);
static void sdhci_tegra_set_tap_delay(struct sdhci_host *sdhci,
	unsigned int tap_delay);
static int tegra_sdhci_configure_regulators(struct sdhci_tegra *tegra_host,
	u8 option, int min_uV, int max_uV);
static void tegra_sdhci_do_calibration(struct sdhci_host *sdhci,
	unsigned char signal_voltage);

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

static u16 tegra_sdhci_readw(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (unlikely((soc_data->nvquirks & NVQUIRK_FORCE_SDHCI_SPEC_200) &&
			(reg == SDHCI_HOST_VERSION))) {
		return SDHCI_SPEC_200;
	}
	return readw(host->ioaddr + reg);
}

static void tegra_sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	/* Seems like we're getting spurious timeout and crc errors, so
	 * disable signalling of them. In case of real errors software
	 * timers should take care of eventually detecting them.
	 */
	if (unlikely(reg == SDHCI_SIGNAL_ENABLE))
		val &= ~(SDHCI_INT_TIMEOUT|SDHCI_INT_CRC);

	writel(val, host->ioaddr + reg);

	if (unlikely((soc_data->nvquirks & NVQUIRK_ENABLE_BLOCK_GAP_DET) &&
			(reg == SDHCI_INT_ENABLE))) {
		u8 gap_ctrl = readb(host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
		if (val & SDHCI_INT_CARD_INT)
			gap_ctrl |= 0x8;
		else
			gap_ctrl &= ~0x8;
		writeb(gap_ctrl, host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
	}
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

static bool disable_scaling __read_mostly;
module_param(disable_scaling, bool, 0644);

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

	if (disable_scaling)
		return freq;

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
	dev_info(mmc_dev(sdhci->mmc), "DFS supported freqs");
	for (i = 0; i < tegra_host->tuning_freq_count; i++) {
		freq = tegra_host->tuning_data[i].freq_hz;
		/*
		 * Check the nearest possible clock with pll_c and pll_p as
		 * the clock sources. Choose the higher frequency.
		 */
		tegra_host->gov_data->freqs[i] =
			get_nearest_clock_freq(pll_c_rate, freq);
		freq = get_nearest_clock_freq(pll_p_rate, freq);
		if (freq > tegra_host->gov_data->freqs[i])
			tegra_host->gov_data->freqs[i] = freq;
		pr_err("%d,", tegra_host->gov_data->freqs[i]);
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

	return gpio_get_value_cansleep(plat->wp_gpio);
}

static int tegra_sdhci_set_uhs_signaling(struct sdhci_host *host,
		unsigned int uhs)
{
	u16 clk, ctrl_2;
	u32 vndr_ctrl, best_tap_value;
	struct tegra_tuning_data *tuning_data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);

	/* Select Bus Speed Mode for host
	 * For HS200 we need to set UHS_MODE_SEL to SDR104.
	 * It works as SDR 104 in SD 4-bit mode and HS200 in eMMC 8-bit mode.
	 * SDR50 mode timing seems to have issues. Programming SDR104
         * mode for SDR50 mode for reliable transfers over interface.
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
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
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

		/* Set the ddr mode trim delay if required */
		if (plat->ddr_trim_delay != -1) {
			vndr_ctrl = sdhci_readl(host, SDHCI_VNDR_CLK_CTRL);
			vndr_ctrl &= ~(0x1F <<
				SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT);
			vndr_ctrl |= (plat->ddr_trim_delay <<
				SDHCI_VNDR_CLK_CTRL_TRIM_VALUE_SHIFT);
			sdhci_writel(host, vndr_ctrl, SDHCI_VNDR_CLK_CTRL);
		}
	}
	/* Set the best tap value based on timing */
	if (((uhs == MMC_TIMING_MMC_HS200) ||
		(uhs == MMC_TIMING_UHS_SDR104) ||
		(uhs == MMC_TIMING_UHS_SDR50)) &&
		(tegra_host->tuning_status == TUNING_STATUS_DONE)) {
		tuning_data = sdhci_tegra_get_tuning_data(host,
			host->mmc->ios.clock);
		best_tap_value = (tegra_host->tap_cmd ==
			TAP_CMD_TRIM_HIGH_VOLTAGE) ?
			tuning_data->nom_best_tap_value :
			tuning_data->best_tap_value;
	} else {
		best_tap_value = tegra_host->plat->tap_delay;
	}
	vndr_ctrl = sdhci_readl(host, SDHCI_VNDR_CLK_CTRL);
	vndr_ctrl &= ~(0xFF <<
		SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	vndr_ctrl |= (best_tap_value <<
		SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	sdhci_writel(host, vndr_ctrl, SDHCI_VNDR_CLK_CTRL);
	return 0;
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
		if (card_present == 1) {
			sdhci->mmc->rescan_disable = 0;
			mmc_detect_change(sdhci->mmc, 0);
		} else if (card_present == 0) {
			sdhci->mmc->detect_change = 0;
			sdhci->mmc->rescan_disable = 1;
		}
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
	int err;

	plat = pdev->dev.platform_data;

	tegra_host->card_present =
			(gpio_get_value_cansleep(plat->cd_gpio) == 0);

	if (tegra_host->card_present) {
		err = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_EN, 0, 0);
		if (err)
			dev_err(mmc_dev(sdhost->mmc),
				"Failed to enable card regulators %d\n", err);
	} else {
		err = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_DIS, 0 , 0);
		if (err)
			dev_err(mmc_dev(sdhost->mmc),
				"Failed to disable card regulators %d\n", err);
		/*
		 * Set retune request as tuning should be done next time
		 * a card is inserted.
		 */
		tegra_host->tuning_status = TUNING_STATUS_RETUNE;
		tegra_host->force_retune = true;
	}

	tasklet_schedule(&sdhost->card_tasklet);
	return IRQ_HANDLED;
};

static void tegra_sdhci_reset_exit(struct sdhci_host *host, u8 mask)
{
	u32 misc_ctrl;
	u32 vendor_ctrl;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;
	unsigned int best_tap_value;

	if (!(mask & SDHCI_RESET_ALL))
		return;

	if (tegra_host->sd_stat_head != NULL) {
		tegra_host->sd_stat_head->data_crc_count = 0;
		tegra_host->sd_stat_head->cmd_crc_count = 0;
		tegra_host->sd_stat_head->data_to_count = 0;
		tegra_host->sd_stat_head->cmd_to_count = 0;
	}

	if (tegra_host->gov_data != NULL)
		tegra_host->gov_data->freq_switch_count = 0;

	vendor_ctrl = sdhci_readl(host, SDHCI_VNDR_CLK_CTRL);
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
			~SDHCI_VNDR_CLK_CTRL_INPUT_IO_CLK;
	} else {
		vendor_ctrl |= SDHCI_VNDR_CLK_CTRL_INTERNAL_CLK;
	}

	if (soc_data->nvquirks & NVQUIRK_SET_TAP_DELAY) {
		if ((tegra_host->tuning_status == TUNING_STATUS_DONE)
			&& (host->mmc->pm_flags & MMC_PM_KEEP_POWER)) {
			tuning_data = sdhci_tegra_get_tuning_data(host,
				host->mmc->ios.clock);
			best_tap_value = (tegra_host->tap_cmd ==
				TAP_CMD_TRIM_HIGH_VOLTAGE) ?
				tuning_data->nom_best_tap_value :
				tuning_data->best_tap_value;
		} else {
			best_tap_value = tegra_host->plat->tap_delay;
		}
		vendor_ctrl &= ~(0xFF << SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
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
	sdhci_writel(host, vendor_ctrl, SDHCI_VNDR_CLK_CTRL);

	misc_ctrl = sdhci_readl(host, SDHCI_VNDR_MISC_CTRL);
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
	/* Enable DDR mode support only for SDMMC4 */
	if (soc_data->nvquirks & NVQUIRK_ENABLE_DDR50) {
		if (tegra_host->instance == 3) {
			misc_ctrl |=
			SDHCI_VNDR_MISC_CTRL_ENABLE_DDR50_SUPPORT;
		}
	}
	if (soc_data->nvquirks & NVQUIRK_INFINITE_ERASE_TIMEOUT) {
		misc_ctrl |=
		SDHCI_VNDR_MISC_CTRL_INFINITE_ERASE_TIMEOUT;
	}
	if (soc_data->nvquirks & NVQUIRK_SET_PIPE_STAGES_MASK_0)
		misc_ctrl &= ~SDHCI_VNDR_MISC_CTRL_PIPE_STAGES_MASK;

	/* External loopback is valid for sdmmc3 only */
	if ((soc_data->nvquirks & NVQUIRK_DISABLE_EXTERNAL_LOOPBACK) &&
		(tegra_host->instance == 2)) {
		if ((tegra_host->tuning_status == TUNING_STATUS_DONE)
			&& (host->mmc->pm_flags &
			MMC_PM_KEEP_POWER)) {
			misc_ctrl &= ~(1 <<
			SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT);
		} else {
			misc_ctrl |= (1 <<
			SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT);
		}
	}
	sdhci_writel(host, misc_ctrl, SDHCI_VNDR_MISC_CTRL);

	if (soc_data->nvquirks & NVQUIRK_DISABLE_AUTO_CMD23)
		host->flags &= ~SDHCI_AUTO_CMD23;

	/* Mask the support for any UHS modes if specified */
	if (plat->uhs_mask & MMC_UHS_MASK_SDR104)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR104;

	if (plat->uhs_mask & MMC_UHS_MASK_DDR50)
		host->mmc->caps &= ~MMC_CAP_UHS_DDR50;

	if (plat->uhs_mask & MMC_UHS_MASK_SDR50)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR50;

	if (plat->uhs_mask & MMC_UHS_MASK_SDR25)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR25;

	if (plat->uhs_mask & MMC_UHS_MASK_SDR12)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR12;

#ifdef CONFIG_MMC_SDHCI_TEGRA_HS200_DISABLE
	host->mmc->caps2 &= ~MMC_CAP2_HS200;
#else
	if (plat->uhs_mask & MMC_MASK_HS200)
		host->mmc->caps2 &= ~MMC_CAP2_HS200;
#endif
}

static int tegra_sdhci_buswidth(struct sdhci_host *sdhci, int bus_width)
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

#ifdef CONFIG_TEGRA_FPGA_PLATFORM
	return;
#endif
	/*
	 * Currently pll_p and pll_c are used as clock sources for SDMMC. If clk
	 * rate is missing for either of them, then no selection is needed and
	 * the default parent is used.
	 */
	if (!pll_c_rate || !pll_p_rate)
		return ;

	pll_c_freq = get_nearest_clock_freq(pll_c_rate, desired_rate);
	pll_p_freq = get_nearest_clock_freq(pll_p_rate, desired_rate);

	/*
	 * For low freq requests, both the desired rates might be higher than
	 * the requested clock frequency. In such cases, select the parent
	 * with the lower frequency rate.
	 */
	if ((pll_c_freq > desired_rate) && (pll_p_freq > desired_rate)) {
		if (pll_p_freq <= pll_c_freq) {
			desired_rate = pll_p_freq;
			parent_clk = pll_p;
		} else {
			desired_rate = pll_c_freq;
			parent_clk = pll_c;
		}
		rc = clk_set_rate(pltfm_host->clk, desired_rate);
		goto set_clk_parent;
	}

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

set_clk_parent:
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
	unsigned int clk_rate;
#ifdef CONFIG_MMC_FREQ_SCALING
	unsigned int tap_value;
	struct tegra_tuning_data *tuning_data;
#endif

	if (sdhci->mmc->ios.timing == MMC_TIMING_UHS_DDR50) {
		/*
		 * In ddr mode, tegra sdmmc controller clock frequency
		 * should be double the card clock frequency.
		 */
		if (tegra_host->ddr_clk_limit)
			clk_rate = tegra_host->ddr_clk_limit * 2;
		else
			clk_rate = clock * 2;
	} else {
		clk_rate = clock;
	}

	if (tegra_host->max_clk_limit &&
		(clk_rate > tegra_host->max_clk_limit))
		clk_rate = tegra_host->max_clk_limit;

	tegra_sdhci_clock_set_parent(sdhci, clk_rate);
	clk_set_rate(pltfm_host->clk, clk_rate);
	sdhci->max_clk = clk_get_rate(pltfm_host->clk);

	/* FPGA supports 26MHz of clock for SDMMC. */
	if (tegra_platform_is_fpga())
		sdhci->max_clk = 26000000;

#ifdef CONFIG_MMC_FREQ_SCALING
	/* Set the tap delay if tuning is done and dfs is enabled */
	if (sdhci->mmc->df &&
		(tegra_host->tuning_status == TUNING_STATUS_DONE)) {
		tuning_data = sdhci_tegra_get_tuning_data(sdhci, clock);
		tap_value = (tegra_host->tap_cmd == TAP_CMD_TRIM_HIGH_VOLTAGE) ?
			tuning_data->nom_best_tap_value :
			tuning_data->best_tap_value;
		sdhci_tegra_set_tap_delay(sdhci, tap_value);
	}
#endif
}

static void tegra_sdhci_set_clock(struct sdhci_host *sdhci, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct platform_device *pdev = to_platform_device(mmc_dev(sdhci->mmc));
	u8 ctrl;

	mutex_lock(&tegra_host->set_clock_mutex);
	pr_debug("%s %s %u enabled=%u\n", __func__,
		mmc_hostname(sdhci->mmc), clock, tegra_host->clk_enabled);
	if (clock) {
		if (!tegra_host->clk_enabled) {
			pm_runtime_get_sync(&pdev->dev);
			clk_prepare_enable(pltfm_host->clk);
			tegra_host->clk_enabled = true;
			sdhci->is_clk_on = tegra_host->clk_enabled;
			ctrl = sdhci_readb(sdhci, SDHCI_VNDR_CLK_CTRL);
			ctrl |= SDHCI_VNDR_CLK_CTRL_SDMMC_CLK;
			sdhci_writeb(sdhci, ctrl, SDHCI_VNDR_CLK_CTRL);
		}
		tegra_sdhci_set_clk_rate(sdhci, clock);

		if (tegra_host->emc_clk && (!tegra_host->is_sdmmc_emc_clk_on)) {
			clk_prepare_enable(tegra_host->emc_clk);
			tegra_host->is_sdmmc_emc_clk_on = true;
		}
		if (tegra_host->sclk && (!tegra_host->is_sdmmc_sclk_on)) {
			clk_prepare_enable(tegra_host->sclk);
			tegra_host->is_sdmmc_sclk_on = true;
		}
	} else if (!clock && tegra_host->clk_enabled) {
		if (tegra_host->emc_clk && tegra_host->is_sdmmc_emc_clk_on) {
			clk_disable_unprepare(tegra_host->emc_clk);
			tegra_host->is_sdmmc_emc_clk_on = false;
		}
		if (tegra_host->sclk && tegra_host->is_sdmmc_sclk_on) {
			clk_disable_unprepare(tegra_host->sclk);
			tegra_host->is_sdmmc_sclk_on = false;
		}
		ctrl = sdhci_readb(sdhci, SDHCI_VNDR_CLK_CTRL);
		ctrl &= ~SDHCI_VNDR_CLK_CTRL_SDMMC_CLK;
		sdhci_writeb(sdhci, ctrl, SDHCI_VNDR_CLK_CTRL);
		clk_disable_unprepare(pltfm_host->clk);
		tegra_host->clk_enabled = false;
		sdhci->is_clk_on = tegra_host->clk_enabled;
		pm_runtime_put_sync(&pdev->dev);
	}
	mutex_unlock(&tegra_host->set_clock_mutex);
}

static void tegra_sdhci_do_calibration(struct sdhci_host *sdhci,
	unsigned char signal_voltage)
{
	unsigned int val;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	unsigned int timeout = 10;
	unsigned int calib_offsets = 0;

	/* No Calibration for sdmmc4 */
	if (unlikely(soc_data->nvquirks & NVQUIRK_DISABLE_SDMMC4_CALIB) &&
		(tegra_host->instance == 3))
		return;

	if (unlikely(soc_data->nvquirks & NVQUIRK_DISABLE_AUTO_CALIBRATION))
		return;

	val = sdhci_readl(sdhci, SDMMC_SDMEMCOMPPADCTRL);
	val &= ~SDMMC_SDMEMCOMPPADCTRL_VREF_SEL_MASK;
	if (soc_data->nvquirks & NVQUIRK_SET_PAD_E_INPUT_OR_E_PWRD)
		val |= SDMMC_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD_MASK;
	val |= 0x7;
	sdhci_writel(sdhci, val, SDMMC_SDMEMCOMPPADCTRL);

	/* Enable Auto Calibration*/
	val = sdhci_readl(sdhci, SDMMC_AUTO_CAL_CONFIG);
	val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
	val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_START;
	if (unlikely(soc_data->nvquirks & NVQUIRK_SET_CALIBRATION_OFFSETS)) {
		if (signal_voltage == MMC_SIGNAL_VOLTAGE_330)
			calib_offsets = tegra_host->plat->calib_3v3_offsets;
		else if (signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			calib_offsets = tegra_host->plat->calib_1v8_offsets;
		if (calib_offsets) {
			/* Program Auto cal PD offset(bits 8:14) */
			val &= ~(0x7F <<
				SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
			val |= (((calib_offsets >> 8) & 0xFF) <<
				SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
			/* Program Auto cal PU offset(bits 0:6) */
			val &= ~0x7F;
			val |= (calib_offsets & 0xFF);
		}
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

	if (soc_data->nvquirks & NVQUIRK_SET_PAD_E_INPUT_OR_E_PWRD) {
		val = sdhci_readl(sdhci, SDMMC_SDMEMCOMPPADCTRL);
		val &= ~SDMMC_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD_MASK;
		sdhci_writel(sdhci, val, SDMMC_SDMEMCOMPPADCTRL);
	}

	if (unlikely(soc_data->nvquirks & NVQUIRK_SET_DRIVE_STRENGTH)) {
		unsigned int pulldown_code;
		unsigned int pullup_code;
		int pg;
		int err;

		/* Disable Auto calibration */
		val = sdhci_readl(sdhci, SDMMC_AUTO_CAL_CONFIG);
		val &= ~SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
		sdhci_writel(sdhci, val, SDMMC_AUTO_CAL_CONFIG);

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
	u16 ctrl;


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

	/* Set/clear the 1.8V signalling */
	sdhci_writew(sdhci, ctrl, SDHCI_HOST_CONTROL2);

	/* Switch the I/O rail voltage */
	rc = tegra_sdhci_configure_regulators(tegra_host, CONFIG_REG_SET_VOLT,
		min_uV, max_uV);
	if (rc && (signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
		dev_err(mmc_dev(sdhci->mmc),
			"setting 1.8V failed %d. Revert to 3.3V\n", rc);
		rc = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_SET_VOLT, SDHOST_HIGH_VOLT_MIN,
			SDHOST_HIGH_VOLT_MAX);
	}

	return rc;
}

static int tegra_sdhci_configure_regulators(struct sdhci_tegra *tegra_host,
	u8 option, int min_uV, int max_uV)
{
	int rc = 0;

	switch (option) {
	case CONFIG_REG_EN:
		if (!tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_slot_reg)
				rc = regulator_enable(tegra_host->vdd_slot_reg);
			if (tegra_host->vdd_io_reg)
				rc = regulator_enable(tegra_host->vdd_io_reg);
			tegra_host->is_rail_enabled = true;
		}
	break;
	case CONFIG_REG_DIS:
		if (tegra_host->is_rail_enabled) {
			if (tegra_host->vdd_io_reg)
				rc = regulator_disable(tegra_host->vdd_io_reg);
			if (tegra_host->vdd_slot_reg)
				rc = regulator_disable(
					tegra_host->vdd_slot_reg);
			tegra_host->is_rail_enabled = false;
		}
	break;
	case CONFIG_REG_SET_VOLT:
		if (tegra_host->vdd_io_reg)
			rc = regulator_set_voltage(tegra_host->vdd_io_reg,
				min_uV, max_uV);
	break;
	default:
		pr_err("Invalid argument passed to reg config %d\n", option);
	}

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
			dev_err(mmc_dev(sdhci->mmc), "Reset 0x%x never"
				"completed.\n", (int)mask);
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
	if (tap_delay > MAX_TAP_VALUES) {
		dev_err(mmc_dev(sdhci->mmc),
			"Valid tap range (0-255). Setting tap value %d\n",
			tap_delay);
		dump_stack();
		return;
	}

	vendor_ctrl = sdhci_readl(sdhci, SDHCI_VNDR_CLK_CTRL);
	vendor_ctrl &= ~(0xFF << SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	vendor_ctrl |= (tap_delay << SDHCI_VNDR_CLK_CTRL_TAP_VALUE_SHIFT);
	sdhci_writel(sdhci, vendor_ctrl, SDHCI_VNDR_CLK_CTRL);
}

static int sdhci_tegra_sd_error_stats(struct sdhci_host *host, u32 int_status)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct sdhci_tegra_sd_stats *head = tegra_host->sd_stat_head;

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

static struct tegra_tuning_data *sdhci_tegra_get_tuning_data(
	struct sdhci_host *sdhci, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	unsigned int low_freq;
	u8 i = 0;

	if (tegra_host->tuning_freq_count == 1) {
		tuning_data = &tegra_host->tuning_data[0];
		goto out;
	}

	/* Get the lowest supported freq */
	for (i = 0; i < TUNING_FREQ_COUNT; ++i) {
		low_freq = tegra_host->soc_data->tuning_freq_list[i];
		if (low_freq)
			break;
	}

	if (clock <= low_freq)
		tuning_data = &tegra_host->tuning_data[0];
	else
		tuning_data = &tegra_host->tuning_data[1];

out:
	return tuning_data;
}

static void calculate_vmin_values(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data, int vmin, int boot_mv)
{
	struct tuning_values *est_values = &tuning_data->est_values;
	struct tuning_values *calc_values = &tuning_data->calc_values;
	struct tuning_t2t_coeffs *t2t_coeffs = tuning_data->t2t_coeffs;
	struct tap_hole_coeffs *thole_coeffs = tuning_data->thole_coeffs;
	int vmin_slope, vmin_int, temp_calc_vmin;
	int t2t_vmax, t2t_vmin;
	int vmax_thole, vmin_thole;

	/*
	 * If current vmin is equal to vmin or vmax of tuning data, use the
	 * previously calculated estimated T2T values directly. Note that the
	 * estimated T2T_vmax is not at Vmax specified in tuning data. It is
	 * the T2T at the boot or max voltage for the current SKU. Hence,
	 * boot_mv is used in place of t2t_coeffs->vmax.
	 */
	if (vmin == t2t_coeffs->vmin) {
		t2t_vmin = est_values->t2t_vmin;
	} else if (vmin == boot_mv) {
		t2t_vmin = est_values->t2t_vmax;
	} else {
		/*
		 * For any intermediate voltage between boot voltage and vmin
		 * of tuning data, calculate the slope and intercept from the
		 * t2t at boot_mv and vmin and calculate the actual values.
		 */
		t2t_vmax = 1000 / est_values->t2t_vmax;
		t2t_vmin = 1000 / est_values->t2t_vmin;
		vmin_slope = ((t2t_vmax - t2t_vmin) * 1000) /
			(boot_mv - t2t_coeffs->vmin);
		vmin_int = (t2t_vmax * 1000 - (vmin_slope * boot_mv)) / 1000;
		t2t_vmin = (vmin_slope * vmin) / 1000 + vmin_int;
		t2t_vmin = (1000 / t2t_vmin);
	}

	calc_values->t2t_vmin = (t2t_vmin * calc_values->t2t_vmax) /
		est_values->t2t_vmax;

	calc_values->ui_vmin = (1000000 / (tuning_data->freq_hz / 1000000)) /
		calc_values->t2t_vmin;

	/* Calculate the vmin tap hole at vmin of tuning data */
	temp_calc_vmin = (est_values->t2t_vmin * calc_values->t2t_vmax) /
		est_values->t2t_vmax;
	vmin_thole = (thole_coeffs->thole_vmin_int -
		(thole_coeffs->thole_vmin_slope * temp_calc_vmin)) /
		1000;
	vmax_thole = calc_values->vmax_thole;

	if (vmin == t2t_coeffs->vmin) {
		calc_values->vmin_thole = vmin_thole;
	} else if (vmin == boot_mv) {
		calc_values->vmin_thole = vmax_thole;
	} else {
		/*
		 * Interpolate the tap hole for any intermediate voltage.
		 * Calculate the slope and intercept from the available data
		 * and use them to calculate the actual values.
		 */
		vmin_slope = ((vmax_thole - vmin_thole) * 1000) /
			(boot_mv - t2t_coeffs->vmin);
		vmin_int = (vmax_thole * 1000 - (vmin_slope * boot_mv)) / 1000;
		calc_values->vmin_thole = (vmin_slope * vmin) / 1000 + vmin_int;
	}

	/* Adjust the partial win start for Vmin boundary */
	if (tuning_data->is_partial_win_valid)
		tuning_data->final_tap_data[0].win_start =
			(tuning_data->final_tap_data[0].win_start *
			tuning_data->calc_values.t2t_vmax) /
			tuning_data->calc_values.t2t_vmin;

	pr_info("**********Tuning values*********\n");
	pr_info("**estimated values**\n");
	pr_info("T2T_Vmax %d, T2T_Vmin %d, 1'st_hole_Vmax %d, UI_Vmax %d\n",
		est_values->t2t_vmax, est_values->t2t_vmin,
		est_values->vmax_thole, est_values->ui);
	pr_info("**Calculated values**\n");
	pr_info("T2T_Vmax %d, 1'st_hole_Vmax %d, UI_Vmax %d\n",
		calc_values->t2t_vmax, calc_values->vmax_thole,
		calc_values->ui);
	pr_info("T2T_Vmin %d, 1'st_hole_Vmin %d, UI_Vmin %d\n",
		calc_values->t2t_vmin, calc_values->vmin_thole,
		calc_values->ui_vmin);
	pr_info("***********************************\n");
}

static int slide_window_start(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data,
	int tap_value, enum tap_win_edge_attr edge_attr, int tap_hole)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (edge_attr == WIN_EDGE_BOUN_START) {
		if (tap_value < 0)
			tap_value += (1000 / tuning_data->calc_values.t2t_vmin);
		else
			tap_value += (1000 / tuning_data->calc_values.t2t_vmax);
	} else if (edge_attr == WIN_EDGE_HOLE) {
		if (soc_data->nvquirks & NVQUIRK_TMP_VAR_1_5_TAP_MARGIN)
			tap_value += ((7 * tap_hole) / 100) + 2;
		else
			tap_value += ((7 * tap_hole) / 100) +
			(((2 * (450 / tuning_data->calc_values.t2t_vmax))
			+ 1) / 2);
	}

	if (tap_value > MAX_TAP_VALUES)
		tap_value = MAX_TAP_VALUES;

	return tap_value;
}

static int slide_window_end(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data,
	int tap_value, enum tap_win_edge_attr edge_attr, int tap_hole)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (edge_attr == WIN_EDGE_BOUN_END) {
		tap_value = (tap_value * tuning_data->calc_values.t2t_vmax) /
			tuning_data->calc_values.t2t_vmin;
		tap_value -= (1000 / tuning_data->calc_values.t2t_vmin);
	} else if (edge_attr == WIN_EDGE_HOLE) {
		if (tap_hole > 0)
			tap_value = tap_hole;
		if (soc_data->nvquirks & NVQUIRK_TMP_VAR_1_5_TAP_MARGIN)
			tap_value -= ((7 * tap_hole) / 100) + 2;
		else
			tap_value -= ((7 * tap_hole) / 100) +
			(((2 * (450 / tuning_data->calc_values.t2t_vmin))
			+ 1) / 2);
	}

	return tap_value;
}

static int adjust_window_boundaries(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data,
	struct tap_window_data *temp_tap_data)
{
	struct tap_window_data *tap_data;
	int vmin_tap_hole;
	int vmax_tap_hole;
	u8 i = 0;

	for (i = 0; i < tuning_data->num_of_valid_tap_wins; i++) {
		tap_data = &temp_tap_data[i];
		/* Update with next hole if first hole is taken care of */
		if (tap_data->win_start_attr == WIN_EDGE_HOLE)
			vmax_tap_hole = tuning_data->calc_values.vmax_thole +
				(tap_data->hole_pos - 1) *
				tuning_data->calc_values.ui;
		tap_data->win_start = slide_window_start(sdhci, tuning_data,
			tap_data->win_start, tap_data->win_start_attr,
			vmax_tap_hole);

		/* Update with next hole if first hole is taken care of */
		if (tap_data->win_end_attr == WIN_EDGE_HOLE)
			vmin_tap_hole = tuning_data->calc_values.vmin_thole +
				(tap_data->hole_pos - 1) *
				tuning_data->calc_values.ui_vmin;
		tap_data->win_end = slide_window_end(sdhci, tuning_data,
			tap_data->win_end, tap_data->win_end_attr,
			vmin_tap_hole);
	}

	pr_info("***********final tuning windows**********\n");
	for (i = 0; i < tuning_data->num_of_valid_tap_wins; i++) {
		tap_data = &temp_tap_data[i];
		pr_info("win[%d]: %d - %d\n", i, tap_data->win_start,
			tap_data->win_end);
	}
	pr_info("********************************\n");
	return 0;
}

static int find_best_tap_value(struct tegra_tuning_data *tuning_data,
	struct tap_window_data *temp_tap_data, int vmin)
{
	struct tap_window_data *tap_data;
	u8 i = 0, sel_win = 0;
	int pref_win = 0, curr_win_size = 0;
	int best_tap_value = 0;

	for (i = 0; i < tuning_data->num_of_valid_tap_wins; i++) {
		tap_data = &temp_tap_data[i];
		if (!i && tuning_data->is_partial_win_valid) {
			pref_win = tap_data->win_end - tap_data->win_start;
			if ((tap_data->win_end * 2) < pref_win)
				pref_win = tap_data->win_end * 2;
			sel_win = 0;
		} else {
			curr_win_size = tap_data->win_end - tap_data->win_start;
			if ((curr_win_size > 0) && (curr_win_size > pref_win)) {
				pref_win = curr_win_size;
				sel_win = i;
			}
		}
	}

	if (pref_win <= 0) {
		pr_err("No window opening for %d vmin\n", vmin);
		return -1;
	}

	tap_data = &temp_tap_data[sel_win];
	if (!sel_win && tuning_data->is_partial_win_valid) {
		i = sel_win;
		best_tap_value = tap_data->win_end - (pref_win / 2);
		if (best_tap_value < 0)
			best_tap_value = 0;
	} else {
		best_tap_value = tap_data->win_start +
			((tap_data->win_end - tap_data->win_start) *
			tuning_data->calc_values.t2t_vmin) /
			(tuning_data->calc_values.t2t_vmin +
			tuning_data->calc_values.t2t_vmax);
	}

	pr_err("best tap win - (%d-%d), best tap value %d\n",
		tap_data->win_start, tap_data->win_end, best_tap_value);
	return best_tap_value;
}

static int sdhci_tegra_calculate_best_tap(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tap_window_data *temp_tap_data = NULL;
	int vmin, curr_vmin, best_tap_value = 0;
	int err = 0;

	curr_vmin = tegra_dvfs_predict_millivolts(pltfm_host->clk,
		tuning_data->freq_hz);
	vmin = curr_vmin;

	do {
		SDHCI_TEGRA_DBG("%s: checking for win opening with vmin %d\n",
			mmc_hostname(sdhci->mmc), vmin);
		if ((best_tap_value < 0) &&
			(vmin > tegra_host->boot_vcore_mv)) {
			dev_err(mmc_dev(sdhci->mmc),
				"No best tap for any vcore range\n");
			return -EINVAL;
		}

		calculate_vmin_values(sdhci, tuning_data, vmin,
			tegra_host->boot_vcore_mv);

		if (temp_tap_data == NULL) {
			temp_tap_data = kzalloc(sizeof(struct tap_window_data) *
				tuning_data->num_of_valid_tap_wins, GFP_KERNEL);
			if (IS_ERR_OR_NULL(temp_tap_data)) {
				dev_err(mmc_dev(sdhci->mmc),
				"No memory for final tap value calculation\n");
				return -ENOMEM;
			}
		}

		memcpy(temp_tap_data, tuning_data->final_tap_data,
			sizeof(struct tap_window_data) *
			tuning_data->num_of_valid_tap_wins);

		adjust_window_boundaries(sdhci, tuning_data, temp_tap_data);

		best_tap_value = find_best_tap_value(tuning_data,
			temp_tap_data, vmin);

		if (best_tap_value < 0)
			vmin += 50;
	} while (best_tap_value < 0);

	tuning_data->best_tap_value = best_tap_value;
	tuning_data->nom_best_tap_value = best_tap_value;

	/* Set the new vmin if there is any change. */
	if ((tuning_data->best_tap_value >= 0) && (curr_vmin != vmin))
		err = tegra_dvfs_set_fmax_at_vmin(pltfm_host->clk,
			tuning_data->freq_hz, vmin);

	kfree(temp_tap_data);
	return err;
}

static int sdhci_tegra_issue_tuning_cmd(struct sdhci_host *sdhci)
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
		err = sdhci_tegra_issue_tuning_cmd(sdhci);
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

static int calculate_actual_tuning_values(int speedo,
	struct tegra_tuning_data *tuning_data, int voltage_mv)
{
	struct tuning_t2t_coeffs *t2t_coeffs = tuning_data->t2t_coeffs;
	struct tap_hole_coeffs *thole_coeffs = tuning_data->thole_coeffs;
	struct tuning_values *calc_values = &tuning_data->calc_values;
	int slope, inpt;
	int vmax_thole, vmin_thole;

	/* T2T_Vmax = (1000000/freq_MHz)/Calc_UI */
	calc_values->t2t_vmax = (1000000 / (tuning_data->freq_hz / 1000000)) /
		calc_values->ui;

	/*
	 * Interpolate the tap hole.
	 * Vmax_1'st_hole = (Calc_T2T_Vmax*(-thole_slope)+thole_tint.
	 */
	vmax_thole = (thole_coeffs->thole_vmax_int -
		(thole_coeffs->thole_vmax_slope * calc_values->t2t_vmax)) /
		1000;
	vmin_thole = (thole_coeffs->thole_vmin_int -
		(thole_coeffs->thole_vmin_slope * calc_values->t2t_vmax)) /
		1000;
	if (voltage_mv == t2t_coeffs->vmin) {
		calc_values->vmax_thole = vmin_thole;
	} else if (voltage_mv == t2t_coeffs->vmax) {
		calc_values->vmax_thole = vmax_thole;
	} else {
		slope = (vmax_thole - vmin_thole) /
			(t2t_coeffs->vmax - t2t_coeffs->vmin);
		inpt = ((vmax_thole * 1000) - (slope * 1250)) / 1000;
		calc_values->vmax_thole = slope * voltage_mv + inpt;
	}

	return 0;
}

/*
 * All coeffs are filled up in the table after multiplying by 1000. So, all
 * calculations should have a divide by 1000 at the end.
 */
static int calculate_estimated_tuning_values(int speedo,
	struct tegra_tuning_data *tuning_data, int voltage_mv)
{
	struct tuning_t2t_coeffs *t2t_coeffs = tuning_data->t2t_coeffs;
	struct tap_hole_coeffs *thole_coeffs = tuning_data->thole_coeffs;
	struct tuning_values *est_values = &tuning_data->est_values;
	int slope, inpt;
	int vmax_t2t, vmin_t2t;
	int vmax_thole, vmin_thole;

	/* Est_T2T_Vmax = (speedo*(-t2t_slope)+t2t_int */
	vmax_t2t = (t2t_coeffs->t2t_vmax_int - (speedo *
		t2t_coeffs->t2t_vmax_slope)) / 1000;
	vmin_t2t = (t2t_coeffs->t2t_vmin_int - (speedo *
		t2t_coeffs->t2t_vmin_slope)) / 1000;
	est_values->t2t_vmin = vmin_t2t;

	if (voltage_mv == t2t_coeffs->vmin) {
		est_values->t2t_vmax = vmin_t2t;
	} else if (voltage_mv == t2t_coeffs->vmax) {
		est_values->t2t_vmax = vmax_t2t;
	} else {
		vmax_t2t = 1000 / vmax_t2t;
		vmin_t2t = 1000 / vmin_t2t;
		/*
		 * For any intermediate voltage between 0.95V and 1.25V,
		 * calculate the slope and intercept from the T2T and tap hole
		 * values of 0.95V and 1.25V and use them to calculate the
		 * actual values. 1/T2T is a linear function of voltage.
		 */
		slope = ((vmax_t2t - vmin_t2t) * 1000) /
			(t2t_coeffs->vmax - t2t_coeffs->vmin);
		inpt = (vmax_t2t * 1000 - (slope * t2t_coeffs->vmax)) / 1000;
		est_values->t2t_vmax = (slope * voltage_mv) / 1000 + inpt;
		est_values->t2t_vmax = (1000 / est_values->t2t_vmax);
	}

	/* Est_UI  = (1000000/freq_MHz)/Est_T2T_Vmax */
	est_values->ui = (1000000 / (thole_coeffs->freq_khz / 1000)) /
		est_values->t2t_vmax;

	/*
	 * Est_1'st_hole = (Est_T2T_Vmax*(-thole_slope)) + thole_int.
	 */
	vmax_thole = (thole_coeffs->thole_vmax_int -
		(thole_coeffs->thole_vmax_slope * est_values->t2t_vmax)) / 1000;
	vmin_thole = (thole_coeffs->thole_vmin_int -
		(thole_coeffs->thole_vmin_slope * est_values->t2t_vmax)) / 1000;

	if (voltage_mv == t2t_coeffs->vmin) {
		est_values->vmax_thole = vmin_thole;
	} else if (voltage_mv == t2t_coeffs->vmax) {
		est_values->vmax_thole = vmax_thole;
	} else {
		/*
		 * For any intermediate voltage between 0.95V and 1.25V,
		 * calculate the slope and intercept from the t2t and tap hole
		 * values of 0.95V and 1.25V and use them to calculate the
		 * actual values. Tap hole is a linear function of voltage.
		 */
		slope = ((vmax_thole - vmin_thole) * 1000) /
			(t2t_coeffs->vmax - t2t_coeffs->vmin);
		inpt = (vmax_thole * 1000 - (slope * t2t_coeffs->vmax)) / 1000;
		est_values->vmax_thole = (slope * voltage_mv) / 1000 + inpt;
	}
	est_values->vmin_thole = vmin_thole;

	return 0;
}

/*
 * Insert the calculated holes and get the final tap windows
 * with the boundaries and holes set.
 */
static int adjust_holes_in_tap_windows(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data)
{
	struct tap_window_data *tap_data;
	struct tap_window_data *final_tap_data;
	struct tuning_values *calc_values = &tuning_data->calc_values;
	int tap_hole, size = 0;
	u8 i = 0, j = 0, num_of_wins, hole_pos = 0;

	tuning_data->final_tap_data =
		devm_kzalloc(mmc_dev(sdhci->mmc),
			sizeof(struct tap_window_data) * 42, GFP_KERNEL);
	if (IS_ERR_OR_NULL(tuning_data->final_tap_data)) {
		dev_err(mmc_dev(sdhci->mmc), "No mem for final tap wins\n");
		return -ENOMEM;
	}

	num_of_wins = tuning_data->num_of_valid_tap_wins;
	tap_hole = calc_values->vmax_thole;
	hole_pos++;
	do {
		tap_data = &tuning_data->tap_data[i];
		final_tap_data = &tuning_data->final_tap_data[j];
		if (tap_hole < tap_data->win_start) {
			tap_hole += calc_values->ui;
			hole_pos++;
			continue;
		} else if (tap_hole > tap_data->win_end) {
			memcpy(final_tap_data, tap_data,
				sizeof(struct tap_window_data));
			i++;
			j++;
			num_of_wins--;
			continue;
		} else if ((tap_hole >= tap_data->win_start) &&
			(tap_hole <= tap_data->win_end)) {
			size = tap_data->win_end - tap_data->win_start;
			do {
				final_tap_data =
					&tuning_data->final_tap_data[j];
				if (tap_hole == tap_data->win_start) {
					final_tap_data->win_start =
						tap_hole + 1;
					final_tap_data->win_start_attr =
						WIN_EDGE_HOLE;
					final_tap_data->hole_pos = hole_pos;
					tap_hole += calc_values->ui;
					hole_pos++;
				} else {
					final_tap_data->win_start =
						tap_data->win_start;
					final_tap_data->win_start_attr =
						WIN_EDGE_BOUN_START;
				}
				if (tap_hole <= tap_data->win_end) {
					final_tap_data->win_end = tap_hole - 1;
					final_tap_data->win_end_attr =
						WIN_EDGE_HOLE;
					final_tap_data->hole_pos = hole_pos;
					tap_data->win_start = tap_hole;
				} else if (tap_hole > tap_data->win_end) {
					final_tap_data->win_end =
						tap_data->win_end;
					final_tap_data->win_end_attr =
						WIN_EDGE_BOUN_END;
					tap_data->win_start =
						tap_data->win_end;
				}
				size = tap_data->win_end - tap_data->win_start;
				j++;
			} while (size > 0);
			i++;
			num_of_wins--;
		}
	} while (num_of_wins > 0);

	/* Update the num of valid wins count after tap holes insertion */
	tuning_data->num_of_valid_tap_wins = j;

	pr_info("********tuning windows after inserting holes*****\n");
	pr_info("WIN_ATTR legend: 0-BOUN_ST, 1-BOUN_END, 2-HOLE\n");
	for (i = 0; i < tuning_data->num_of_valid_tap_wins; i++) {
		final_tap_data = &tuning_data->final_tap_data[i];
		pr_info("win[%d]:%d(%d) - %d(%d)\n", i,
			final_tap_data->win_start,
			final_tap_data->win_start_attr,
			final_tap_data->win_end, final_tap_data->win_end_attr);
	}
	pr_info("***********************************************\n");

	return 0;
}

/*
 * Insert the boundaries from negative margin calculations into the windows
 * from auto tuning.
 */
static int insert_boundaries_in_tap_windows(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data, u8 boun_end)
{
	struct tap_window_data *tap_data;
	struct tap_window_data *new_tap_data;
	struct tap_window_data *temp_tap_data;
	struct tuning_values *calc_values = &tuning_data->calc_values;
	int curr_boun;
	u8 i = 0, j = 0, num_of_wins;
	bool get_next_boun = false;

	temp_tap_data = devm_kzalloc(mmc_dev(sdhci->mmc),
			sizeof(struct tap_window_data) * 42, GFP_KERNEL);
	if (IS_ERR_OR_NULL(temp_tap_data)) {
		dev_err(mmc_dev(sdhci->mmc), "No mem for final tap wins\n");
		return -ENOMEM;
	}

	num_of_wins = tuning_data->num_of_valid_tap_wins;
	curr_boun = boun_end % calc_values->ui;
	do {
		if (get_next_boun) {
			curr_boun += calc_values->ui;
			/*
			 * If the boun_end exceeds the intial boundary end,
			 * just copy remaining windows and return.
			 */
			if (curr_boun >= boun_end)
				curr_boun += MAX_TAP_VALUES;
		}

		tap_data = &tuning_data->tap_data[i];
		new_tap_data = &temp_tap_data[j];
		if (curr_boun <= tap_data->win_start) {
			get_next_boun = true;
			continue;
		} else if (curr_boun >= tap_data->win_end) {
			memcpy(new_tap_data, tap_data,
				sizeof(struct tap_window_data));
			i++;
			j++;
			num_of_wins--;
			get_next_boun = false;
			continue;
		} else if ((curr_boun >= tap_data->win_start) &&
			(curr_boun <= tap_data->win_end)) {
				new_tap_data->win_start = tap_data->win_start;
				new_tap_data->win_start_attr =
					tap_data->win_start_attr;
				new_tap_data->win_end = curr_boun - 1;
				new_tap_data->win_end_attr =
					tap_data->win_end_attr;
				j++;
				new_tap_data = &temp_tap_data[j];
				new_tap_data->win_start = curr_boun;
				new_tap_data->win_end = curr_boun;
				new_tap_data->win_start_attr =
					WIN_EDGE_BOUN_START;
				new_tap_data->win_end_attr =
					WIN_EDGE_BOUN_END;
				j++;
				new_tap_data = &temp_tap_data[j];
				new_tap_data->win_start = curr_boun + 1;
				new_tap_data->win_start_attr = WIN_EDGE_BOUN_START;
				new_tap_data->win_end = tap_data->win_end;
				new_tap_data->win_end_attr =
					tap_data->win_end_attr;
				i++;
				j++;
				num_of_wins--;
				get_next_boun = true;
		}
	} while (num_of_wins > 0);

	/* Update the num of valid wins count after tap holes insertion */
	tuning_data->num_of_valid_tap_wins = j;

	memcpy(tuning_data->tap_data, temp_tap_data,
		j * sizeof(struct tap_window_data));
	SDHCI_TEGRA_DBG("***tuning windows after inserting boundaries***\n");
	SDHCI_TEGRA_DBG("WIN_ATTR legend: 0-BOUN_ST, 1-BOUN_END, 2-HOLE\n");
	for (i = 0; i < tuning_data->num_of_valid_tap_wins; i++) {
		new_tap_data = &tuning_data->tap_data[i];
		SDHCI_TEGRA_DBG("win[%d]:%d(%d) - %d(%d)\n", i,
			new_tap_data->win_start,
			new_tap_data->win_start_attr,
			new_tap_data->win_end, new_tap_data->win_end_attr);
	}
	SDHCI_TEGRA_DBG("***********************************************\n");

	return 0;
}

/*
 * Scan for all tap values and get all passing tap windows.
 */
static int sdhci_tegra_get_tap_window_data(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tap_window_data *tap_data;
	struct tuning_ui tuning_ui[10];
	int err = 0, partial_win_start = 0, temp_margin = 0;
	unsigned int tap_value, calc_ui = 0;
	u8 prev_boundary_end = 0, num_of_wins = 0;
	u8 num_of_uis = 0, valid_num_uis = 0;
	u8 ref_ui, first_valid_full_win = 0;
	u8 boun_end = 0, next_boun_end = 0;
	u8 j = 0;
	bool valid_ui_found = false;

	/*
	 * Assume there are a max of 10 windows and allocate tap window
	 * structures for the same. If there are more windows, the array
	 * size can be adjusted later using realloc.
	 */
	tuning_data->tap_data = devm_kzalloc(mmc_dev(sdhci->mmc),
		sizeof(struct tap_window_data) * 42, GFP_KERNEL);
	if (IS_ERR_OR_NULL(tuning_data->tap_data)) {
		dev_err(mmc_dev(sdhci->mmc), "No memory for tap data\n");
		return -ENOMEM;
	}

	spin_lock(&sdhci->lock);
	tap_value = 0;
	do {
		tap_data = &tuning_data->tap_data[num_of_wins];
		/* Get the window start */
		tap_value = sdhci_tegra_scan_tap_values(sdhci, tap_value, true);
		tap_data->win_start = min_t(u8, tap_value, MAX_TAP_VALUES);
		tap_value++;
		if (tap_value >= MAX_TAP_VALUES) {
			/* If it's first iteration, then all taps failed */
			if (!num_of_wins) {
				dev_err(mmc_dev(sdhci->mmc),
					"All tap values(0-255) failed\n");
				spin_unlock(&sdhci->lock);
				return -EINVAL;
			} else {
				/* All windows obtained */
				break;
			}
		}

		/* Get the window end */
		tap_value = sdhci_tegra_scan_tap_values(sdhci,
				tap_value, false);
		tap_data->win_end = min_t(u8, (tap_value - 1), MAX_TAP_VALUES);
		tap_data->win_size = tap_data->win_end - tap_data->win_start;
		tap_value++;

		/*
		 * If the size of window is more than 4 taps wide, then it is a
		 * valid window. If tap value 0 has passed, then a partial
		 * window exists. Mark all the window edges as boundary edges.
		 */
		if (tap_data->win_size > 4) {
			if (tap_data->win_start == 0)
				tuning_data->is_partial_win_valid = true;
			tap_data->win_start_attr = WIN_EDGE_BOUN_START;
			tap_data->win_end_attr = WIN_EDGE_BOUN_END;
		} else {
			/* Invalid window as size is less than 5 taps */
			SDHCI_TEGRA_DBG("Invalid tuning win (%d-%d) ignored\n",
				tap_data->win_start, tap_data->win_end);
			continue;
		}

		/* Ignore first and last partial UIs */
		if (tap_data->win_end_attr == WIN_EDGE_BOUN_END) {
				tuning_ui[num_of_uis].ui = tap_data->win_end -
					prev_boundary_end;
				tuning_ui[num_of_uis].is_valid_ui = true;
				num_of_uis++;
			prev_boundary_end = tap_data->win_end;
		}
		num_of_wins++;
	} while (tap_value < MAX_TAP_VALUES);
	spin_unlock(&sdhci->lock);

	tuning_data->num_of_valid_tap_wins = num_of_wins;
	valid_num_uis = num_of_uis;

	/* Print info of all tap windows */
	pr_info("**********Auto tuning windows*************\n");
	pr_info("WIN_ATTR legend: 0-BOUN_ST, 1-BOUN_END, 2-HOLE\n");
	for (j = 0; j < tuning_data->num_of_valid_tap_wins; j++) {
		tap_data = &tuning_data->tap_data[j];
		pr_info("win[%d]: %d(%d) - %d(%d)\n",
			j, tap_data->win_start, tap_data->win_start_attr,
			tap_data->win_end, tap_data->win_end_attr);
	}
	pr_info("***************************************\n");

	/* Mark the first last partial UIs as invalid */
	tuning_ui[0].is_valid_ui = false;
	tuning_ui[num_of_uis - 1].is_valid_ui = false;
	valid_num_uis -= 2;

	/* Discredit all uis at either end with size less than 30% of est ui */
	ref_ui = (30 * tuning_data->est_values.ui) / 100;
	for (j = 0; j < num_of_uis; j++) {
		if (tuning_ui[j].is_valid_ui) {
			tuning_ui[j].is_valid_ui = false;
			valid_num_uis--;
		}
		if (tuning_ui[j].ui > ref_ui)
			break;
	}

	for (j = num_of_uis; j > 0; j--) {
		if (tuning_ui[j - 1].ui < ref_ui) {
			if (tuning_ui[j - 1].is_valid_ui) {
				tuning_ui[j - 1].is_valid_ui = false;
				valid_num_uis--;
			}
		} else
			break;
	}

	/* Calculate 0.75*est_UI */
	ref_ui = (75 * tuning_data->est_values.ui) / 100;

	/*
	 * Check for valid UIs and discredit invalid UIs. A UI is considered
	 * valid if it's greater than (0.75*est_UI). If an invalid UI is found,
	 * also discredit the smaller of the two adjacent windows.
	 */
	for (j = 1; j < (num_of_uis - 1); j++) {
		if (tuning_ui[j].ui > ref_ui && tuning_ui[j].is_valid_ui) {
			tuning_ui[j].is_valid_ui = true;
		} else {
			if (tuning_ui[j].is_valid_ui) {
				tuning_ui[j].is_valid_ui = false;
				valid_num_uis--;
			}
			if (!tuning_ui[j + 1].is_valid_ui ||
				!tuning_ui[j - 1].is_valid_ui) {
				if (tuning_ui[j - 1].is_valid_ui) {
					tuning_ui[j - 1].is_valid_ui = false;
					valid_num_uis--;
				} else if (tuning_ui[j + 1].is_valid_ui) {
					tuning_ui[j + 1].is_valid_ui = false;
					valid_num_uis--;
				}
			} else {

				if (tuning_ui[j - 1].ui > tuning_ui[j + 1].ui)
					tuning_ui[j + 1].is_valid_ui = false;
				else
					tuning_ui[j - 1].is_valid_ui = false;
				valid_num_uis--;
			}
		}
	}

	/* Calculate the cumulative UI if there are valid UIs left */
	if (valid_num_uis) {
		for (j = 0; j < num_of_uis; j++)
			if (tuning_ui[j].is_valid_ui) {
				calc_ui += tuning_ui[j].ui;
				if (!first_valid_full_win)
					first_valid_full_win = j;
			}
	}

	if (calc_ui) {
		tuning_data->calc_values.ui = (calc_ui / valid_num_uis);
		valid_ui_found = true;
	} else {
		tuning_data->calc_values.ui = tuning_data->est_values.ui;
		valid_ui_found = false;
	}

	SDHCI_TEGRA_DBG("****Tuning UIs***********\n");
	for (j = 0; j < num_of_uis; j++)
		SDHCI_TEGRA_DBG("Tuning UI[%d] : %d, Is valid[%d]\n",
			j, tuning_ui[j].ui, tuning_ui[j].is_valid_ui);
	SDHCI_TEGRA_DBG("*************************\n");

	/* Get the calculated tuning values */
	err = calculate_actual_tuning_values(tegra_host->speedo, tuning_data,
		tegra_host->boot_vcore_mv);

	/*
	 * Calculate negative margin if partial win is valid. There are two
	 * cases here.
	 * Case 1: If Avg_UI is found, then keep subtracting avg_ui from start
	 * of first valid full window until a value <=0 is obtained.
	 * Case 2: If Avg_UI is not found, subtract avg_ui from all boundary
	 * starts until a value <=0 is found.
	 */
	if (tuning_data->is_partial_win_valid && (num_of_wins > 1)) {
		if (valid_ui_found) {
			partial_win_start =
			tuning_data->tap_data[first_valid_full_win].win_start;
			boun_end = partial_win_start;
			partial_win_start %= tuning_data->calc_values.ui;
			partial_win_start -= tuning_data->calc_values.ui;
		} else {
			for (j = 0; j < NEG_MAR_CHK_WIN_COUNT; j++) {
				temp_margin =
					tuning_data->tap_data[j + 1].win_start;
				if (!boun_end)
					boun_end = temp_margin;
				else if (!next_boun_end)
					next_boun_end = temp_margin;
				temp_margin %= tuning_data->calc_values.ui;
				temp_margin -= tuning_data->calc_values.ui;
				if (!partial_win_start ||
					(temp_margin > partial_win_start))
					partial_win_start = temp_margin;
			}
		}
		if (partial_win_start <= 0)
			tuning_data->tap_data[0].win_start = partial_win_start;
	}

	if (boun_end)
		insert_boundaries_in_tap_windows(sdhci, tuning_data, boun_end);
	if (next_boun_end)
		insert_boundaries_in_tap_windows(sdhci, tuning_data, next_boun_end);

	/* Insert calculated holes into the windows */
	err = adjust_holes_in_tap_windows(sdhci, tuning_data);

	return err;
}

static void sdhci_tegra_dump_tuning_constraints(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	u8 i;

	SDHCI_TEGRA_DBG("%s: Num of tuning frequencies%d\n",
		mmc_hostname(sdhci->mmc), tegra_host->tuning_freq_count);
	for (i = 0; i < tegra_host->tuning_freq_count; ++i) {
		tuning_data = &tegra_host->tuning_data[i];
		SDHCI_TEGRA_DBG("%s: Tuning freq[%d]: %d, freq band %d\n",
			mmc_hostname(sdhci->mmc), i,
			tuning_data->freq_hz, tuning_data->freq_band);
	}
}

static unsigned int get_tuning_voltage(struct sdhci_tegra *tegra_host, u8 *mask)
{
	u8 i = 0;

	i = ffs(*mask) - 1;
	*mask &= ~(1 << i);
	switch (BIT(i)) {
	case NOMINAL_VCORE_TUN:
		return tegra_host->nominal_vcore_mv;
	case BOOT_VCORE_TUN:
		return tegra_host->boot_vcore_mv;
	case MIN_OVERRIDE_VCORE_TUN:
		return tegra_host->min_vcore_override_mv;
	}

	return tegra_host->boot_vcore_mv;
}

static u8 sdhci_tegra_get_freq_point(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const unsigned int *freq_list;
	u32 curr_clock;
	u8 i;

	curr_clock = sdhci->max_clk;
	freq_list = tegra_host->soc_data->tuning_freq_list;

	for (i = 0; i < TUNING_FREQ_COUNT; ++i)
		if (curr_clock <= freq_list[i])
			return i;

	return TUNING_MAX_FREQ;
}

/*
 * The frequency tuning algorithm tries to calculate the tap-to-tap delay
 * UI and estimate holes using equations and predetermined coefficients from
 * the characterization data. The algorithm will not work without this data.
 */
static int find_tuning_coeffs_data(struct sdhci_host *sdhci,
					bool force_retuning)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	struct tegra_tuning_data *tuning_data;
	struct tuning_t2t_coeffs *t2t_coeffs;
	struct tap_hole_coeffs *thole_coeffs;
	const char *dev_id;
	unsigned int freq_khz;
	u8 i, j;
	bool coeffs_set = false;

	dev_id = dev_name(mmc_dev(sdhci->mmc));
	/* Find the coeffs data for all supported frequencies */
	for (i = 0; i < tegra_host->tuning_freq_count; i++) {
		tuning_data = &tegra_host->tuning_data[i];

		/* Skip if T2T coeffs are already found */
		if (tuning_data->t2t_coeffs == NULL || force_retuning) {
			t2t_coeffs = soc_data->t2t_coeffs;
			for (j = 0; j < soc_data->t2t_coeffs_count; j++) {
				if (!strcmp(dev_id, t2t_coeffs->dev_id)) {
					tuning_data->t2t_coeffs = t2t_coeffs;
					coeffs_set = true;
					dev_info(mmc_dev(sdhci->mmc),
						"Found T2T coeffs data\n");
					break;
				}
				t2t_coeffs++;
			}
			if (!coeffs_set) {
				dev_err(mmc_dev(sdhci->mmc),
					"T2T coeffs data missing\n");
				tuning_data->t2t_coeffs = NULL;
				return -ENODATA;
			}
		}

		coeffs_set = false;
		/* Skip if tap hole coeffs are already found */
		if (tuning_data->thole_coeffs == NULL || force_retuning) {
			thole_coeffs = soc_data->tap_hole_coeffs;
			freq_khz = tuning_data->freq_hz / 1000;
			for (j = 0; j < soc_data->tap_hole_coeffs_count; j++) {
				if (!strcmp(dev_id, thole_coeffs->dev_id) &&
					(freq_khz == thole_coeffs->freq_khz)) {
					tuning_data->thole_coeffs =
						thole_coeffs;
					coeffs_set = true;
					dev_info(mmc_dev(sdhci->mmc),
						"%dMHz tap hole coeffs found\n",
						(freq_khz / 1000));
					break;
				}
				thole_coeffs++;
			}

			if (!coeffs_set) {
				dev_err(mmc_dev(sdhci->mmc),
					"%dMHz Tap hole coeffs data missing\n",
					(freq_khz / 1000));
				tuning_data->thole_coeffs = NULL;
				return -ENODATA;
			}
		}
	}

	return 0;
}

/*
 * Determines the numbers of frequencies required and then fills up the tuning
 * constraints for each of the frequencies. The data of lower frequency is
 * filled first and then the higher frequency data. Max supported frequencies
 * is currently two.
 */
static int setup_freq_constraints(struct sdhci_host *sdhci,
	const unsigned int *freq_list)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	int i, freq_count;
	u8 freq_band;

	if ((sdhci->mmc->ios.timing != MMC_TIMING_UHS_SDR50) &&
		(sdhci->mmc->caps2 & MMC_CAP2_FREQ_SCALING))
		freq_count = DFS_FREQ_COUNT;
	else
		freq_count = 1;

	freq_band = sdhci_tegra_get_freq_point(sdhci);
	/* Fill up the req frequencies */
	switch (freq_count) {
	case 1:
		tuning_data = &tegra_host->tuning_data[0];
		tuning_data->freq_hz = sdhci->max_clk;
		tuning_data->freq_band = freq_band;
		tuning_data->constraints.vcore_mask =
			tuning_vcore_constraints[freq_band].vcore_mask;
		tuning_data->nr_voltages =
			hweight32(tuning_data->constraints.vcore_mask);
	break;
	case 2:
		tuning_data = &tegra_host->tuning_data[1];
		tuning_data->freq_hz = sdhci->max_clk;
		tuning_data->freq_band = freq_band;
		tuning_data->constraints.vcore_mask =
			tuning_vcore_constraints[freq_band].vcore_mask;
		tuning_data->nr_voltages =
			hweight32(tuning_data->constraints.vcore_mask);

		tuning_data = &tegra_host->tuning_data[0];
		for (i = (freq_band - 1); i >= 0; i--) {
			if (!freq_list[i])
				continue;
			tuning_data->freq_hz = freq_list[i];
			tuning_data->freq_band = i;
			tuning_data->nr_voltages = 1;
			tuning_data->constraints.vcore_mask =
				tuning_vcore_constraints[i].vcore_mask;
			tuning_data->nr_voltages =
				hweight32(tuning_data->constraints.vcore_mask);
		}
	break;
	default:
		dev_err(mmc_dev(sdhci->mmc), "Unsupported freq count\n");
		freq_count = -1;
	}

	return freq_count;
}

/*
 * Get the supported frequencies and other tuning related constraints for each
 * frequency. The supported frequencies should be determined from the list of
 * frequencies in the soc data and also consider the platform clock limits as
 * well as any DFS related restrictions.
 */
static int sdhci_tegra_get_tuning_constraints(struct sdhci_host *sdhci,
							bool force_retuning)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const unsigned int *freq_list;
	int err = 0;

	/* A valid freq count means freq constraints are already set up */
	if (!tegra_host->tuning_freq_count || force_retuning) {
		freq_list = tegra_host->soc_data->tuning_freq_list;
		tegra_host->tuning_freq_count =
			setup_freq_constraints(sdhci, freq_list);
		if (tegra_host->tuning_freq_count < 0) {
			dev_err(mmc_dev(sdhci->mmc),
				"Invalid tuning freq count\n");
			return -EINVAL;
		}
	}

	err = find_tuning_coeffs_data(sdhci, force_retuning);
	if (err)
		return err;

	sdhci_tegra_dump_tuning_constraints(sdhci);

	return err;
}

/*
 * During boot, only boot voltage for vcore can be set. Check if the current
 * voltage is allowed to be used. Nominal and min override voltages can be
 * set once boot is done. This will be notified through late subsys init call.
 */
static int sdhci_tegra_set_tuning_voltage(struct sdhci_host *sdhci,
	unsigned int voltage)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	int err = 0;
	bool nom_emc_freq_set = false;

	if (voltage && (voltage != tegra_host->boot_vcore_mv) &&
		!vcore_overrides_allowed) {
		SDHCI_TEGRA_DBG("%s: Override vcore %dmv not allowed\n",
			mmc_hostname(sdhci->mmc), voltage);
		return -EPERM;
	}

	SDHCI_TEGRA_DBG("%s: Setting vcore override %d\n",
		mmc_hostname(sdhci->mmc), voltage);
	/* First clear any previous dvfs override settings */
	err = tegra_dvfs_override_core_voltage(pltfm_host->clk, 0);
	if (!voltage)
		return err;

	/* EMC clock freq boost might be required for nominal core voltage */
	if ((voltage == tegra_host->nominal_vcore_mv) &&
		tegra_host->plat->en_nominal_vcore_tuning &&
		tegra_host->emc_clk) {
		err = clk_set_rate(tegra_host->emc_clk,
			SDMMC_EMC_NOM_VOLT_FREQ);
		if (err)
			dev_err(mmc_dev(sdhci->mmc),
				"Failed to set emc nom clk freq %d\n", err);
		else
			nom_emc_freq_set = true;
	}

	err = tegra_dvfs_override_core_voltage(pltfm_host->clk, voltage);
	if (err)
		dev_err(mmc_dev(sdhci->mmc),
			"failed to set vcore override %dmv\n", voltage);

	/* Revert emc clock to normal freq */
	if (nom_emc_freq_set) {
		err = clk_set_rate(tegra_host->emc_clk, SDMMC_EMC_MAX_FREQ);
		if (err)
			dev_err(mmc_dev(sdhci->mmc),
				"Failed to revert emc nom clk freq %d\n", err);
	}

	return err;
}

static int sdhci_tegra_run_tuning(struct sdhci_host *sdhci,
	struct tegra_tuning_data *tuning_data)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	int err = 0;
	int voltage = 0;
	u8 i, vcore_mask = 0;

	vcore_mask = tuning_data->constraints.vcore_mask;
	for (i = 0; i < tuning_data->nr_voltages; i++) {
		voltage = get_tuning_voltage(tegra_host, &vcore_mask);
		err = sdhci_tegra_set_tuning_voltage(sdhci, voltage);
		if (err) {
			dev_err(mmc_dev(sdhci->mmc),
				"Unable to set override voltage.\n");
			return err;
		}

		/* Get the tuning window info */
		SDHCI_TEGRA_DBG("Getting tuning windows...\n");
		err = sdhci_tegra_get_tap_window_data(sdhci, tuning_data);
		if (err) {
			dev_err(mmc_dev(sdhci->mmc),
				"Failed to get tap win %d\n", err);
			return err;
		}
		SDHCI_TEGRA_DBG("%s: %d tuning window data obtained\n",
			mmc_hostname(sdhci->mmc), tuning_data->freq_hz);
	}
	return err;
}

static int sdhci_tegra_verify_best_tap(struct sdhci_host *sdhci)
{
	struct tegra_tuning_data *tuning_data;
	int err = 0;

	tuning_data = sdhci_tegra_get_tuning_data(sdhci, sdhci->max_clk);
	if ((tuning_data->best_tap_value < 0) ||
		(tuning_data->best_tap_value > MAX_TAP_VALUES)) {
		dev_err(mmc_dev(sdhci->mmc),
			"Trying to verify invalid best tap value\n");
		return -EINVAL;
	} else {
		dev_err(mmc_dev(sdhci->mmc),
			"%s: tuning freq %dhz, best tap %d\n",
			__func__, tuning_data->freq_hz,
			tuning_data->best_tap_value);
	}

	/* Set the best tap value */
	sdhci_tegra_set_tap_delay(sdhci, tuning_data->best_tap_value);

	/* Run tuning after setting the best tap value */
	err = sdhci_tegra_issue_tuning_cmd(sdhci);
	if (err)
		dev_err(mmc_dev(sdhci->mmc),
			"%dMHz best tap value verification failed %d\n",
			tuning_data->freq_hz, err);
	return err;
}

static int sdhci_tegra_execute_tuning(struct sdhci_host *sdhci, u32 opcode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct tegra_tuning_data *tuning_data;
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	int err;
	u16 ctrl_2;
	u32 misc_ctrl;
	u32 ier;
	u8 i, set_retuning = 0;
	bool force_retuning = false;
	bool enable_lb_clk;

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

	SDHCI_TEGRA_DBG("%s: Starting freq tuning\n", mmc_hostname(sdhci->mmc));
	enable_lb_clk = (soc_data->nvquirks &
			NVQUIRK_DISABLE_EXTERNAL_LOOPBACK) &&
			(tegra_host->instance == 2);
	if (enable_lb_clk) {
		misc_ctrl = sdhci_readl(sdhci, SDHCI_VNDR_MISC_CTRL);
		misc_ctrl &= ~(1 <<
			SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT);
		sdhci_writel(sdhci, misc_ctrl, SDHCI_VNDR_MISC_CTRL);
	}
	mutex_lock(&tuning_mutex);

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

	/*
	 * If tuning is already done and retune request is not set, then skip
	 * best tap value calculation and use the old best tap value. If the
	 * previous best tap value verification failed, force retuning.
	 */
	if (tegra_host->tuning_status == TUNING_STATUS_DONE) {
		err = sdhci_tegra_verify_best_tap(sdhci);
		if (err) {
			dev_err(mmc_dev(sdhci->mmc),
				"Prev best tap failed. Re-running tuning\n");
			force_retuning = true;
		} else {
			goto out;
		}
	}

	if (tegra_host->force_retune == true) {
		force_retuning = true;
		tegra_host->force_retune = false;
	}

	tegra_host->tuning_status = 0;
	err = sdhci_tegra_get_tuning_constraints(sdhci, force_retuning);
	if (err) {
		dev_err(mmc_dev(sdhci->mmc),
			"Failed to get tuning constraints\n");
		goto out;
	}

	for (i = 0; i < tegra_host->tuning_freq_count; i++) {
		tuning_data = &tegra_host->tuning_data[i];
		if (tuning_data->tuning_done && !force_retuning)
			continue;

		SDHCI_TEGRA_DBG("%s: Setting tuning freq%d\n",
			mmc_hostname(sdhci->mmc), tuning_data->freq_hz);
		tegra_sdhci_set_clock(sdhci, tuning_data->freq_hz);

		SDHCI_TEGRA_DBG("%s: Calculating estimated tuning values\n",
			mmc_hostname(sdhci->mmc));
		err = calculate_estimated_tuning_values(tegra_host->speedo,
			tuning_data, tegra_host->boot_vcore_mv);
		if (err)
			goto out;

		SDHCI_TEGRA_DBG("Running tuning...\n");
		err = sdhci_tegra_run_tuning(sdhci, tuning_data);
		if (err)
			goto out;

		SDHCI_TEGRA_DBG("calculating best tap value\n");
		err = sdhci_tegra_calculate_best_tap(sdhci, tuning_data);
		if (err)
			goto out;

		err = sdhci_tegra_verify_best_tap(sdhci);
		if (!err && !set_retuning) {
			tuning_data->tuning_done = true;
			tegra_host->tuning_status |= TUNING_STATUS_DONE;
		} else {
			tegra_host->tuning_status |= TUNING_STATUS_RETUNE;
		}
	}
out:
	/* Release any override core voltages set */
	sdhci_tegra_set_tuning_voltage(sdhci, 0);

	/* Enable interrupts. Enable full range for core voltage */
	sdhci_writel(sdhci, ier, SDHCI_INT_ENABLE);
	sdhci_writel(sdhci, ier, SDHCI_SIGNAL_ENABLE);
	mutex_unlock(&tuning_mutex);

	SDHCI_TEGRA_DBG("%s: Freq tuning done\n", mmc_hostname(sdhci->mmc));
	if (enable_lb_clk) {
		misc_ctrl = sdhci_readl(sdhci, SDHCI_VNDR_MISC_CTRL);
		if (err) {
			/* Tuning is failed and card will try to enumerate in
			 * Legacy High Speed mode. So, Enable External Loopback
			 * for SDMMC3.
			 */
			misc_ctrl |= (1 <<
				SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT);
		} else {
			misc_ctrl &= ~(1 <<
				SDHCI_VNDR_MISC_CTRL_EN_EXT_LOOPBACK_SHIFT);
		}
		sdhci_writel(sdhci, misc_ctrl, SDHCI_VNDR_MISC_CTRL);
	}
	return err;
}

static int __init sdhci_tegra_enable_vcore_override_tuning(void)
{
	vcore_overrides_allowed = true;
	maintain_boot_voltage = false;
	return 0;
}
late_initcall(sdhci_tegra_enable_vcore_override_tuning);

static int tegra_sdhci_suspend(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	int err = 0;

	tegra_sdhci_set_clock(sdhci, 0);

	/* Disable the power rails if any */
	if (tegra_host->card_present) {
		err = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_DIS, 0, 0);
		if (err)
			dev_err(mmc_dev(sdhci->mmc),
			"Regulators disable in suspend failed %d\n", err);
	}
	return err;
}

static int tegra_sdhci_resume(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct platform_device *pdev;
	struct tegra_sdhci_platform_data *plat;
	unsigned int signal_voltage = 0;
	int err;

	pdev = to_platform_device(mmc_dev(sdhci->mmc));
	plat = pdev->dev.platform_data;

	if (gpio_is_valid(plat->cd_gpio)) {
		tegra_host->card_present =
			(gpio_get_value_cansleep(plat->cd_gpio) == 0);
	}

	/* Setting the min identification clock of freq 400KHz */
	tegra_sdhci_set_clock(sdhci, 400000);

	/* Enable the power rails if any */
	if (tegra_host->card_present) {
		err = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_EN, 0, 0);
		if (err) {
			dev_err(mmc_dev(sdhci->mmc),
				"Regulators enable in resume failed %d\n", err);
			return err;
		}
		if (tegra_host->vdd_io_reg) {
			if (plat->mmc_data.ocr_mask & SDHOST_1V8_OCR_MASK)
				signal_voltage = MMC_SIGNAL_VOLTAGE_180;
			else
				signal_voltage = MMC_SIGNAL_VOLTAGE_330;
			tegra_sdhci_signal_voltage_switch(sdhci,
				signal_voltage);
		}
	}

	/* Reset the controller and power on if MMC_KEEP_POWER flag is set*/
	if (sdhci->mmc->pm_flags & MMC_PM_KEEP_POWER) {
		tegra_sdhci_reset(sdhci, SDHCI_RESET_ALL);
		sdhci_writeb(sdhci, SDHCI_POWER_ON, SDHCI_POWER_CONTROL);
		sdhci->pwr = 0;

		tegra_sdhci_do_calibration(sdhci, signal_voltage);
	}

	return 0;
}

static void tegra_sdhci_post_resume(struct sdhci_host *sdhci)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;

	/* Turn OFF the clocks if the device is not present */
	if ((!tegra_host->card_present || !sdhci->mmc->card) &&
		tegra_host->clk_enabled)
		tegra_sdhci_set_clock(sdhci, 0);
}

/*
 * For tegra specific tuning, core voltage has to be fixed at different
 * voltages to get the tap values. Fixing the core voltage during tuning for one
 * device might affect transfers of other SDMMC devices. Check if tuning mutex
 * is locked before starting a data transfer. The new tuning procedure might
 * take at max 1.5s for completion for a single run. Taking DFS into count,
 * setting the max timeout for tuning mutex check a 3 secs. Since tuning is
 * run only during boot or the first time device is inserted, there wouldn't
 * be any delays in cmd/xfer execution once devices enumeration is done.
 */
static void tegra_sdhci_get_bus(struct sdhci_host *sdhci)
{
	unsigned int timeout = 300;

	while (mutex_is_locked(&tuning_mutex)) {
		msleep(10);
		--timeout;
		if (!timeout) {
			dev_err(mmc_dev(sdhci->mmc),
				"Tuning mutex locked for long time\n");
			return;
		}
	};
}

/*
 * The host/device can be powered off before the retuning request is handled in
 * case of SDIDO being off if Wifi is turned off, sd card removal etc. In such
 * cases, cancel the pending tuning timer and remove any core voltage
 * constraints that are set earlier.
 */
static void tegra_sdhci_power_off(struct sdhci_host *sdhci, u8 power_mode)
{
	int retuning_req_set = 0;

	retuning_req_set = (timer_pending(&sdhci->tuning_timer) ||
		(sdhci->flags & SDHCI_NEEDS_RETUNING));

	if (retuning_req_set) {
		del_timer_sync(&sdhci->tuning_timer);

		if (boot_volt_req_refcount)
			--boot_volt_req_refcount;

		if (!boot_volt_req_refcount) {
			sdhci_tegra_set_tuning_voltage(sdhci, 0);
			SDHCI_TEGRA_DBG("%s: Release override as host is off\n",
				mmc_hostname(sdhci->mmc));
		}
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
	struct dentry *root = host->debugfs_root;
	struct dentry *dfs_root;
	unsigned saved_line;

	if (!root) {
		root = debugfs_create_dir(dev_name(mmc_dev(host->mmc)), NULL);
		if (IS_ERR_OR_NULL(root)) {
			saved_line = __LINE__;
			goto err_root;
		}
		host->debugfs_root = root;
	}

	dfs_root = debugfs_create_dir("dfs_stats_dir", root);
	if (IS_ERR_OR_NULL(dfs_root)) {
		saved_line = __LINE__;
		goto err_node;
	}

	if (!debugfs_create_file("error_stats", S_IRUSR, root, host,
				&sdhci_host_fops)) {
		saved_line = __LINE__;
		goto err_node;
	}
	if (!debugfs_create_file("dfs_stats", S_IRUSR, dfs_root, host,
				&sdhci_host_dfs_fops)) {
		saved_line = __LINE__;
		goto err_node;
	}
	if (!debugfs_create_file("polling_period", 0644, dfs_root, (void *)host,
				&sdhci_polling_period_fops)) {
		saved_line = __LINE__;
		goto err_node;
	}
	if (!debugfs_create_file("active_load_high_threshold", 0644,
				dfs_root, (void *)host,
				&sdhci_active_load_high_threshold_fops)) {
		saved_line = __LINE__;
		goto err_node;
	}

	if (IS_QUIRKS2_DELAYED_CLK_GATE(host)) {
		host->clk_gate_tmout_ticks = -1;
		if (!debugfs_create_u32("clk_gate_tmout_ticks",
			S_IRUGO | S_IWUSR,
			root, (u32 *)&host->clk_gate_tmout_ticks)) {
			saved_line = __LINE__;
			goto err_node;
		}
	}

	return;

err_node:
	debugfs_remove_recursive(root);
	host->debugfs_root = NULL;
err_root:
	pr_err("%s %s: Failed to initialize debugfs functionality at line=%d\n", __func__,
		mmc_hostname(host->mmc), saved_line);
	return;
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
	u32 present_state;
	u8 timeout;
	bool clk_set_for_tap_prog = false;

	tap_cmd = memparse(p, &p);

	card = host->mmc->card;
	if (!card)
		return -ENODEV;

	/* if not uhs -- no tuning and no tap value to set */
	if (!mmc_sd_card_uhs(card) && !mmc_card_hs200(card))
		return count;

	/* if no change in tap value -- just exit */
	if (tap_cmd == tegra_host->tap_cmd)
		return count;

	if ((tap_cmd != TAP_CMD_TRIM_DEFAULT_VOLTAGE) &&
		(tap_cmd != TAP_CMD_TRIM_HIGH_VOLTAGE)) {
		pr_info("echo 1 > cmd_state  # to set normal voltage\n");
		pr_info("echo 2 > cmd_state  # to set high voltage\n");
		return -EINVAL;
	}

	tegra_host->tap_cmd = tap_cmd;
	tuning_data = sdhci_tegra_get_tuning_data(host, host->max_clk);
	/* Check if host clock is enabled */
	if (!tegra_host->clk_enabled) {
		/* Nothing to do if the host is not powered ON */
		if (host->mmc->ios.power_mode != MMC_POWER_ON)
			return count;
		else {
			tegra_sdhci_set_clock(host, host->mmc->ios.clock);
			clk_set_for_tap_prog = true;
		}
	} else {
		timeout = 10;
		/* Wait for any on-going data transfers */
		present_state = sdhci_readl(host, SDHCI_PRESENT_STATE);
		while (present_state & (SDHCI_DOING_WRITE | SDHCI_DOING_READ)) {
			if (!timeout)
				break;
			timeout--;
			mdelay(1);
			present_state = sdhci_readl(host, SDHCI_PRESENT_STATE);
		};
	}
	spin_lock(&host->lock);
	switch (tap_cmd) {
	case TAP_CMD_TRIM_DEFAULT_VOLTAGE:
		/* set tap value for voltage range 1.1 to 1.25 */
		sdhci_tegra_set_tap_delay(host, tuning_data->best_tap_value);
		break;

	case TAP_CMD_TRIM_HIGH_VOLTAGE:
		/* set tap value for voltage range 1.25 to 1.39 */
		sdhci_tegra_set_tap_delay(host,
			tuning_data->nom_best_tap_value);
		break;
	}
	spin_unlock(&host->lock);
	if (clk_set_for_tap_prog) {
		tegra_sdhci_set_clock(host, 0);
		clk_set_for_tap_prog = false;
	}
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

static int tegra_sdhci_reboot_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct sdhci_tegra *tegra_host =
		container_of(nb, struct sdhci_tegra, reboot_notify);
	int err;

	switch (event) {
	case SYS_RESTART:
	case SYS_POWER_OFF:
		err = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_DIS, 0, 0);
		if (err)
			pr_err("Disable regulator in reboot notify failed %d\n",
				err);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

void tegra_sdhci_ios_config_enter(struct sdhci_host *sdhci, struct mmc_ios *ios)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	struct clk *new_mode_clk;
	bool change_clk = false;

	/*
	 * Tegra sdmmc controllers require clock to be enabled for any register
	 * access. Set the minimum controller clock if no clock is requested.
	 */
	if (!sdhci->clock && !ios->clock) {
		tegra_sdhci_set_clock(sdhci, sdhci->mmc->f_min);
		sdhci->clock = sdhci->mmc->f_min;
	} else if (ios->clock && (ios->clock != sdhci->clock)) {
		tegra_sdhci_set_clock(sdhci, ios->clock);
	}

	/*
	 * Check for DDR50 mode setting and set ddr_clk if not already
	 * done. Return if only one clock option is available.
	 */
	if (!tegra_host->ddr_clk || !tegra_host->sdr_clk) {
		return;
	} else {
		if ((ios->timing == MMC_TIMING_UHS_DDR50) &&
			!tegra_host->is_ddr_clk_set) {
			change_clk = true;
			new_mode_clk = tegra_host->ddr_clk;
		} else if ((ios->timing != MMC_TIMING_UHS_DDR50) &&
			tegra_host->is_ddr_clk_set) {
			change_clk = true;
			new_mode_clk = tegra_host->sdr_clk;
		}

		if (change_clk) {
			tegra_sdhci_set_clock(sdhci, 0);
			pltfm_host->clk = new_mode_clk;
			/* Restore the previous frequency */
			tegra_sdhci_set_clock(sdhci, sdhci->max_clk);
			tegra_host->is_ddr_clk_set =
				!tegra_host->is_ddr_clk_set;
		}
	}
}

void tegra_sdhci_ios_config_exit(struct sdhci_host *sdhci, struct mmc_ios *ios)
{
	/*
	 * Do any required handling for retuning requests before powering off
	 * the host.
	 */
	if (ios->power_mode == MMC_POWER_OFF)
		tegra_sdhci_power_off(sdhci, ios->power_mode);

	/*
	 * In case of power off, turn off controller clock now as all the
	 * required register accesses are already done.
	 */
	if (!ios->clock && !sdhci->mmc->skip_host_clkgate)
		tegra_sdhci_set_clock(sdhci, 0);
}

static const struct sdhci_ops tegra_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.get_cd     = tegra_sdhci_get_cd,
	.read_l     = tegra_sdhci_readl,
	.read_w     = tegra_sdhci_readw,
	.write_l    = tegra_sdhci_writel,
	.write_w    = tegra_sdhci_writew,
	.platform_bus_width = tegra_sdhci_buswidth,
	.set_clock		= tegra_sdhci_set_clock,
	.suspend		= tegra_sdhci_suspend,
	.resume			= tegra_sdhci_resume,
	.platform_resume	= tegra_sdhci_post_resume,
	.platform_reset_exit	= tegra_sdhci_reset_exit,
	.platform_get_bus	= tegra_sdhci_get_bus,
	.platform_ios_config_enter	= tegra_sdhci_ios_config_enter,
	.platform_ios_config_exit	= tegra_sdhci_ios_config_exit,
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

static struct sdhci_pltfm_data sdhci_tegra12_pdata = {
	.quirks = TEGRA_SDHCI_QUIRKS,
	.quirks2 = TEGRA_SDHCI_QUIRKS2 |
		SDHCI_QUIRK2_SUPPORT_64BIT_DMA,
	.ops  = &tegra_sdhci_ops,
};

static struct sdhci_tegra_soc_data soc_data_tegra12 = {
	.pdata = &sdhci_tegra12_pdata,
	.nvquirks = TEGRA_SDHCI_NVQUIRKS |
		    NVQUIRK_SET_TRIM_DELAY |
		    NVQUIRK_ENABLE_DDR50 |
		    NVQUIRK_ENABLE_HS200 |
		    NVQUIRK_INFINITE_ERASE_TIMEOUT |
		    NVQUIRK_SET_PAD_E_INPUT_OR_E_PWRD |
		    NVQUIRK_HIGH_FREQ_TAP_PROCEDURE |
		    NVQUIRK_SET_CALIBRATION_OFFSETS |
		    NVQUIRK_DISABLE_EXTERNAL_LOOPBACK,
	.parent_clk_list = {"pll_p", "pll_c"},
	.tuning_freq_list = {81600000, 136000000, 200000000},
	.t2t_coeffs = t12x_tuning_coeffs,
	.t2t_coeffs_count = 3,
	.tap_hole_coeffs = t12x_tap_hole_coeffs,
	.tap_hole_coeffs_count = 12,
};

static const struct of_device_id sdhci_tegra_dt_match[] = {
	{ .compatible = "nvidia,tegra124-sdhci", .data = &soc_data_tegra12 },
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_dt_ids);

static struct tegra_sdhci_platform_data *sdhci_tegra_dt_parse_pdata(
						struct platform_device *pdev)
{
	int val;
	struct tegra_sdhci_platform_data *plat;
	struct device_node *np = pdev->dev.of_node;
	u32 bus_width;

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

	if (of_property_read_u32(np, "bus-width", &bus_width) == 0 &&
	    bus_width == 8)
		plat->is_8bit = 1;

	of_property_read_u32(np, "tap-delay", &plat->tap_delay);
	of_property_read_u32(np, "trim-delay", &plat->trim_delay);
	of_property_read_u32(np, "ddr-clk-limit", &plat->ddr_clk_limit);
	of_property_read_u32(np, "max-clk-limit", &plat->max_clk_limit);

	of_property_read_u32(np, "uhs_mask", &plat->uhs_mask);

	if (of_find_property(np, "built-in", NULL))
		plat->mmc_data.built_in = 1;

	if (!of_property_read_u32(np, "mmc-ocr-mask", &val)) {
		if (val == 0)
			plat->mmc_data.ocr_mask = MMC_OCR_1V8_MASK;
		else if (val == 1)
			plat->mmc_data.ocr_mask = MMC_OCR_2V8_MASK;
		else if (val == 2)
			plat->mmc_data.ocr_mask = MMC_OCR_3V2_MASK;
		else if (val == 3)
			plat->mmc_data.ocr_mask = MMC_OCR_3V3_MASK;
	}
	return plat;
}

static int sdhci_tegra_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sdhci_tegra_soc_data *soc_data;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct tegra_sdhci_platform_data *plat;
	struct sdhci_tegra *tegra_host;
	unsigned int low_freq;
	int rc;
	u8 i;

	match = of_match_device(sdhci_tegra_dt_match, &pdev->dev);
	if (match) {
		soc_data = match->data;
	} else {
		/* Use id tables and remove the following chip defines */
		soc_data = &soc_data_tegra12;
	}

	host = sdhci_pltfm_init(pdev, soc_data->pdata);

	/* sdio delayed clock gate quirk in sdhci_host used */
	host->quirks2 |= SDHCI_QUIRK2_DELAYED_CLK_GATE;

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
	pdev->dev.platform_data = plat;

	tegra_host->sd_stat_head = devm_kzalloc(&pdev->dev,
		sizeof(struct sdhci_tegra_sd_stats), GFP_KERNEL);
	if (!tegra_host->sd_stat_head) {
		dev_err(mmc_dev(host->mmc), "failed to allocate sd_stat_head\n");
		rc = -ENOMEM;
		goto err_power_req;
	}

	tegra_host->soc_data = soc_data;
	pltfm_host->priv = tegra_host;

	for (i = 0; i < ARRAY_SIZE(soc_data->parent_clk_list); i++) {
		if (!soc_data->parent_clk_list[i])
			continue;
		if (!strcmp(soc_data->parent_clk_list[i], "pll_c")) {
			pll_c = clk_get_sys(NULL, "pll_c");
			if (IS_ERR(pll_c)) {
				rc = PTR_ERR(pll_c);
				dev_err(mmc_dev(host->mmc),
					"clk error in getting pll_c: %d\n", rc);
			}
			pll_c_rate = clk_get_rate(pll_c);
		}

		if (!strcmp(soc_data->parent_clk_list[i], "pll_p")) {
			pll_p = clk_get_sys(NULL, "pll_p");
			if (IS_ERR(pll_p)) {
				rc = PTR_ERR(pll_p);
				dev_err(mmc_dev(host->mmc),
					"clk error in getting pll_p: %d\n", rc);
			}
			pll_p_rate = clk_get_rate(pll_p);
		}
	}

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

		tegra_host->card_present =
			(gpio_get_value_cansleep(plat->cd_gpio) == 0);

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
	} else if (plat->mmc_data.ocr_mask & MMC_OCR_3V2_MASK) {
			tegra_host->vddio_min_uv = SDHOST_HIGH_VOLT_3V2;
			tegra_host->vddio_max_uv = SDHOST_HIGH_VOLT_MAX;
	} else if (plat->mmc_data.ocr_mask & MMC_OCR_3V3_MASK) {
			tegra_host->vddio_min_uv = SDHOST_HIGH_VOLT_3V3;
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
		rc = tegra_sdhci_configure_regulators(tegra_host,
			CONFIG_REG_SET_VOLT,
			tegra_host->vddio_min_uv,
			tegra_host->vddio_max_uv);
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"Init volt(%duV-%duV) setting failed %d\n",
				tegra_host->vddio_min_uv,
				tegra_host->vddio_max_uv, rc);
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
		rc = tegra_sdhci_configure_regulators(tegra_host, CONFIG_REG_EN,
			0, 0);
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"Enable regulators failed in probe %d\n", rc);
			goto err_clk_get;
		}
	}

	tegra_pd_add_device(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/* Get the ddr clock */
	tegra_host->ddr_clk = clk_get(mmc_dev(host->mmc), "ddr");
	if (IS_ERR(tegra_host->ddr_clk)) {
		dev_err(mmc_dev(host->mmc), "ddr clk err\n");
		tegra_host->ddr_clk = NULL;
	}

	/* Get high speed clock */
	tegra_host->sdr_clk = clk_get(mmc_dev(host->mmc), NULL);
	if (IS_ERR(tegra_host->sdr_clk)) {
		dev_err(mmc_dev(host->mmc), "sdr clk err\n");
		tegra_host->sdr_clk = NULL;
		/* If both ddr and sdr clks are missing, then fail probe */
		if (!tegra_host->ddr_clk && !tegra_host->sdr_clk) {
			dev_err(mmc_dev(host->mmc),
				"Failed to get ddr and sdr clks\n");
			rc = -EINVAL;
			goto err_clk_get;
		}
	}

	if (tegra_host->sdr_clk) {
		pltfm_host->clk = tegra_host->sdr_clk;
		tegra_host->is_ddr_clk_set = false;
	} else {
		pltfm_host->clk = tegra_host->ddr_clk;
		tegra_host->is_ddr_clk_set = true;
	}

	if (clk_get_parent(pltfm_host->clk) == pll_c)
		tegra_host->is_parent_pllc = true;

	pm_runtime_get_sync(&pdev->dev);
	rc = clk_prepare_enable(pltfm_host->clk);
	if (rc != 0)
		goto err_clk_put;

	tegra_host->emc_clk = devm_clk_get(mmc_dev(host->mmc), "emc");
	if (IS_ERR_OR_NULL(tegra_host->emc_clk)) {
		dev_err(mmc_dev(host->mmc), "Can't get emc clk\n");
		tegra_host->emc_clk = NULL;
	} else {
		clk_set_rate(tegra_host->emc_clk, SDMMC_EMC_MAX_FREQ);
	}

	tegra_host->sclk = devm_clk_get(mmc_dev(host->mmc), "sclk");
	if (IS_ERR_OR_NULL(tegra_host->sclk)) {
		dev_err(mmc_dev(host->mmc), "Can't get sclk clock\n");
		tegra_host->sclk = NULL;
	} else {
		clk_set_rate(tegra_host->sclk, SDMMC_AHB_MAX_FREQ);
	}
	pltfm_host->priv = tegra_host;
	tegra_host->clk_enabled = true;
	host->is_clk_on = tegra_host->clk_enabled;
	mutex_init(&tegra_host->set_clock_mutex);

	tegra_host->max_clk_limit = plat->max_clk_limit;
	tegra_host->ddr_clk_limit = plat->ddr_clk_limit;
	tegra_host->instance = pdev->id;
	tegra_host->tap_cmd = TAP_CMD_TRIM_DEFAULT_VOLTAGE;
	tegra_host->speedo = plat->cpu_speedo;
	dev_info(mmc_dev(host->mmc), "Speedo value %d\n", tegra_host->speedo);
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

	/* disable access to boot partitions */
	host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (soc_data->nvquirks & NVQUIRK_ENABLE_HS200)
		host->mmc->caps2 |= MMC_CAP2_HS200;
#ifdef CONFIG_TEGRA_FPGA_PLATFORM
	/* Enable HS200 mode */
	host->mmc->caps2 |= MMC_CAP2_HS200;
#else
	host->mmc->caps2 |= MMC_CAP2_CACHE_CTRL;
	host->mmc->caps |= MMC_CAP_CMD23;
	host->mmc->caps2 |= MMC_CAP2_PACKED_CMD;
#endif
#endif

	/*
	 * Enable dyamic frequency scaling support only if the platform clock
	 * limit is higher than the lowest supported frequency by tuning.
	 */
	for (i = 0; i < TUNING_FREQ_COUNT; i++) {
		low_freq = soc_data->tuning_freq_list[i];
		if (low_freq)
			break;
	}
	if (plat->en_freq_scaling && (plat->max_clk_limit > low_freq))
		host->mmc->caps2 |= MMC_CAP2_FREQ_SCALING;

	if (!plat->disable_clock_gate)
		host->mmc->caps2 |= MMC_CAP2_CLOCK_GATING;

	if (plat->nominal_vcore_mv)
		tegra_host->nominal_vcore_mv = plat->nominal_vcore_mv;
	if (plat->min_vcore_override_mv)
		tegra_host->min_vcore_override_mv = plat->min_vcore_override_mv;
	if (plat->boot_vcore_mv)
		tegra_host->boot_vcore_mv = plat->boot_vcore_mv;
	dev_info(mmc_dev(host->mmc),
		"Tuning constraints: nom_mv %d, boot_mv %d, min_or_mv %d\n",
		tegra_host->nominal_vcore_mv, tegra_host->boot_vcore_mv,
		tegra_host->min_vcore_override_mv);

	/*
	 * If nominal voltage is equal to boot voltage, there is no need for
	 * nominal voltage tuning.
	 */
	if (plat->nominal_vcore_mv <= plat->boot_vcore_mv)
		plat->en_nominal_vcore_tuning = false;

	rc = sdhci_add_host(host);
	if (rc)
		goto err_add_host;

	INIT_DELAYED_WORK(&host->delayed_clk_gate_wrk, delayed_clk_gate_cb);

	if (gpio_is_valid(plat->cd_gpio)) {
		rc = request_threaded_irq(gpio_to_irq(plat->cd_gpio), NULL,
			carddetect_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			mmc_hostname(host->mmc), host);
		if (rc) {
			dev_err(mmc_dev(host->mmc), "request irq error\n");
			goto err_cd_irq_req;
		}
		if (!plat->cd_wakeup_incapable) {
			rc = enable_irq_wake(gpio_to_irq(plat->cd_gpio));
			if (rc < 0)
				dev_err(mmc_dev(host->mmc),
					"SD card wake-up event registration "
					"failed with error: %d\n", rc);
		}
	}
	sdhci_tegra_error_stats_debugfs(host);
	device_create_file(&pdev->dev, &dev_attr_cmd_state);

	/* Enable async suspend/resume to reduce LP0 latency */
	device_enable_async_suspend(&pdev->dev);

	if (plat->power_off_rail) {
		tegra_host->reboot_notify.notifier_call =
			tegra_sdhci_reboot_notify;
		register_reboot_notifier(&tegra_host->reboot_notify);
	}
	return 0;

err_cd_irq_req:
	if (gpio_is_valid(plat->cd_gpio))
		gpio_free(plat->cd_gpio);
err_add_host:
	if (tegra_host->is_ddr_clk_set)
		clk_disable_unprepare(tegra_host->ddr_clk);
	else
		clk_disable_unprepare(tegra_host->sdr_clk);
	pm_runtime_put_sync(&pdev->dev);
err_clk_put:
	if (tegra_host->ddr_clk)
		clk_put(tegra_host->ddr_clk);
	if (tegra_host->sdr_clk)
		clk_put(tegra_host->sdr_clk);
err_clk_get:
	if (gpio_is_valid(plat->wp_gpio))
		gpio_free(plat->wp_gpio);
err_wp_req:
	if (gpio_is_valid(plat->cd_gpio))
		free_irq(gpio_to_irq(plat->cd_gpio), host);
err_cd_req:
	if (gpio_is_valid(plat->power_gpio))
		gpio_free(plat->power_gpio);
err_power_req:
err_no_plat:
	sdhci_pltfm_free(pdev);
	return rc;
}

static int sdhci_tegra_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = pltfm_host->priv;
	const struct tegra_sdhci_platform_data *plat = tegra_host->plat;
	int dead = (readl(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);
	int rc = 0;

	sdhci_remove_host(host, dead);

	disable_irq_wake(gpio_to_irq(plat->cd_gpio));

	rc = tegra_sdhci_configure_regulators(tegra_host, CONFIG_REG_DIS, 0, 0);
	if (rc)
		dev_err(mmc_dev(host->mmc),
			"Regulator disable in remove failed %d\n", rc);

	if (tegra_host->vdd_slot_reg)
		regulator_put(tegra_host->vdd_slot_reg);
	if (tegra_host->vdd_io_reg)
		regulator_put(tegra_host->vdd_io_reg);

	if (gpio_is_valid(plat->wp_gpio))
		gpio_free(plat->wp_gpio);

	if (gpio_is_valid(plat->cd_gpio)) {
		free_irq(gpio_to_irq(plat->cd_gpio), host);
		gpio_free(plat->cd_gpio);
	}

	if (gpio_is_valid(plat->power_gpio))
		gpio_free(plat->power_gpio);

	if (tegra_host->clk_enabled) {
		if (tegra_host->is_ddr_clk_set)
			clk_disable_unprepare(tegra_host->ddr_clk);
		else
			clk_disable_unprepare(tegra_host->sdr_clk);
		pm_runtime_put_sync(&pdev->dev);
	}

	if (tegra_host->ddr_clk)
		clk_put(tegra_host->ddr_clk);
	if (tegra_host->sdr_clk)
		clk_put(tegra_host->sdr_clk);

	if (tegra_host->emc_clk && tegra_host->is_sdmmc_emc_clk_on)
		clk_disable_unprepare(tegra_host->emc_clk);
	if (tegra_host->sclk && tegra_host->is_sdmmc_sclk_on)
		clk_disable_unprepare(tegra_host->sclk);
	if (plat->power_off_rail)
		unregister_reboot_notifier(&tegra_host->reboot_notify);

	sdhci_pltfm_free(pdev);

	return rc;
}

static struct platform_driver sdhci_tegra_driver = {
	.driver		= {
		.name	= "sdhci-tegra",
		.owner	= THIS_MODULE,
		.of_match_table = sdhci_tegra_dt_match,
		.pm	= SDHCI_PLTFM_PMOPS,
	},
	.probe		= sdhci_tegra_probe,
	.remove		= sdhci_tegra_remove,
};

module_platform_driver(sdhci_tegra_driver);

MODULE_DESCRIPTION("SDHCI driver for Tegra");
MODULE_AUTHOR("Google, Inc.");
MODULE_LICENSE("GPL v2");
