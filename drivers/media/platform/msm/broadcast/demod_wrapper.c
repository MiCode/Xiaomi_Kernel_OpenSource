/**
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>
#include <linux/bug.h>

#include "linux/demod_wrapper.h"


/* Base addrs */
#define DEMOD_WRAPPER_BASE       0x000D0000
#define BCSS_CC_BASE             0x00144000

/* Registers shifts */
/* RXFE_ATVRXFE_C2 */
#define CFG_RXFE_LUT_ENTRY0_SHIFT 0
#define CFG_RXFE_LUT_ENTRY1_SHIFT 4
#define CFG_RXFE_LUT_ENTRY2_SHIFT 8
#define CFG_RXFE_LUT_ENTRY3_SHIFT 12
#define CFG_RXFE_LUT_ENTRY4_SHIFT 16
#define CFG_RXFE_LUT_ENTRY5_SHIFT 20
#define CFG_RXFE_LUT_ENTRY6_SHIFT 24
#define CFG_RXFE_LUT_ENTRY7_SHIFT 28
/* RXFE_ATVRXFE_C3 */
#define CFG_RXFE_LUT_ENTRY8_SHIFT 0
#define CFG_RXFE_LUT_ENTRY9_SHIFT 4
#define CFG_RXFE_LUT_ENTRY10_SHIFT 8
#define CFG_RXFE_LUT_ENTRY11_SHIFT 12
#define CFG_RXFE_LUT_ENTRY12_SHIFT 16
#define CFG_RXFE_LUT_ENTRY13_SHIFT 20
#define CFG_RXFE_LUT_ENTRY14_SHIFT 24
#define CFG_RXFE_LUT_ENTRY15_SHIFT 28
/* PDM + ADC shifts*/
#define PDM_EN_SHIFT 31
#define PDM_SOURCE_SHIFT 0
#define ADC_MUX_SEL_SHIFT 0

/* Registers addresses */
/* General */
#define BCDEM_REVISION		(DEMOD_WRAPPER_BASE + 0x00000000)
#define BCDEM_CTRL		(DEMOD_WRAPPER_BASE + 0x00000004)
#define ADC_MUX_SEL		(DEMOD_WRAPPER_BASE + 0x00000008)
#define BCDEM_CGC		(DEMOD_WRAPPER_BASE + 0x0000000C)
#define BCDEM_ADC_POWER_DOWN	(DEMOD_WRAPPER_BASE + 0x00000014)
#define UCCP_METX_BOOT		(DEMOD_WRAPPER_BASE + 0x00000010)
#define ATV_POST_PROCESS_CVBS_SIF (DEMOD_WRAPPER_BASE + 0x00000030)
/* RXFE */
#define RXFE_IMGRXFE_C1          (DEMOD_WRAPPER_BASE + 0x00000034)
#define RXFE_IMGRXFE_C2          (DEMOD_WRAPPER_BASE + 0x00000038)
#define RXFE_IMGRXFE_C3          (DEMOD_WRAPPER_BASE + 0x0000003C)
#define RXFE_IMGRXFE_C4          (DEMOD_WRAPPER_BASE + 0x00000040)
#define RXFE_IMGRXFE_C10_COEFF(n) (DEMOD_WRAPPER_BASE + 0x00000058 + 0x4 * (n))
#define RXFE_IMGRXFE_C11         (DEMOD_WRAPPER_BASE + 0x00000070)
#define RXFE_IMGRXFE_C12_COEFF(n) (DEMOD_WRAPPER_BASE + 0x00000074 + 0x4 * (n))
#define RXFE_ATVRXFE_C1          (DEMOD_WRAPPER_BASE + 0x00000098)
#define RXFE_ATVRXFE_C2          (DEMOD_WRAPPER_BASE + 0x0000009C)
#define RXFE_ATVRXFE_C3          (DEMOD_WRAPPER_BASE + 0x000000A0)
#define RXFE_ATVRXFE_C4          (DEMOD_WRAPPER_BASE + 0x000000A4)
#define RXFE_ATVRXFE_C5          (DEMOD_WRAPPER_BASE + 0x000000A8)
#define RXFE_ATVRXFE_C6          (DEMOD_WRAPPER_BASE + 0x000000AC)
#define RXFE_ATVRXFE_C7          (DEMOD_WRAPPER_BASE + 0x000000B0)
#define RXFE_ATVRXFE_C8          (DEMOD_WRAPPER_BASE + 0x000000B4)
#define RXFE_ATVRXFE_C9_COEFF(n) (DEMOD_WRAPPER_BASE + 0x000000B8 + 0x4 * (n))
#define RXFE_ATVRXFE_C10_COEFF(n) (DEMOD_WRAPPER_BASE + 0x000000CC + 0x4 * (n))
#define RXFE_ATVRXFE_C11_COEFF(n) (DEMOD_WRAPPER_BASE + 0x000000E4 + 0x4 * (n))
#define RXFE_ATVRXFE_C12         (DEMOD_WRAPPER_BASE + 0x00000110)
#define RXFE_ATVRXFE_C13         (DEMOD_WRAPPER_BASE + 0x00000114)
#define RXFE_ATVRXFE_C13A        (DEMOD_WRAPPER_BASE + 0x00000118)
#define RXFE_ATVRXFE_C14         (DEMOD_WRAPPER_BASE + 0x0000011C)
#define RXFE_IMGSIF_C1           (DEMOD_WRAPPER_BASE + 0x00000120)
#define RXFE_IMGSIF_C2_COEFF(n)  (DEMOD_WRAPPER_BASE + 0x00000124 + 0x4 * (n))
#define RXFE_IMGSIF_C3           (DEMOD_WRAPPER_BASE + 0x0000014C)
/* ADC */
#define ADC_BBR0_TEST1           (DEMOD_WRAPPER_BASE + 0x00000200)
#define ADC_BBR0_TEST2           (DEMOD_WRAPPER_BASE + 0x00000204)
#define ADC_BBR0_TEST3           (DEMOD_WRAPPER_BASE + 0x00000208)
#define ADC_BBR0_CONFIG          (DEMOD_WRAPPER_BASE + 0x0000020C)
#define ADC_BBR0_MISC            (DEMOD_WRAPPER_BASE + 0x00000210)
#define ADC_BBR1_TEST1           (DEMOD_WRAPPER_BASE + 0x00000214)
#define ADC_BBR1_TEST2           (DEMOD_WRAPPER_BASE + 0x00000218)
#define ADC_BBR1_TEST3           (DEMOD_WRAPPER_BASE + 0x0000021C)
#define ADC_BBR1_CONFIG          (DEMOD_WRAPPER_BASE + 0x00000220)
#define ADC_BBR1_MISC            (DEMOD_WRAPPER_BASE + 0x00000224)
#define ADC_BBR2_TEST1           (DEMOD_WRAPPER_BASE + 0x00000228)
#define ADC_BBR2_TEST2           (DEMOD_WRAPPER_BASE + 0x0000022C)
#define ADC_BBR2_TEST3           (DEMOD_WRAPPER_BASE + 0x00000230)
#define ADC_BBR2_CONFIG          (DEMOD_WRAPPER_BASE + 0x00000234)
#define ADC_BBR2_MISC            (DEMOD_WRAPPER_BASE + 0x00000238)
/* PDM */
#define PDM0_CONTROL             (DEMOD_WRAPPER_BASE + 0x00000300)
#define PDM0_GAIN_CTRL_IN        (DEMOD_WRAPPER_BASE + 0x00000304)
#define PDM0_GAIN_CTRL_OUT       (DEMOD_WRAPPER_BASE + 0x00000308)
#define PDM1_CONTROL             (DEMOD_WRAPPER_BASE + 0x00000310)
#define PDM1_GAIN_CTRL_IN        (DEMOD_WRAPPER_BASE + 0x00000314)
#define PDM1_GAIN_CTRL_OUT       (DEMOD_WRAPPER_BASE + 0x00000318)
/* PM */
#define PM_CTRL                  (DEMOD_WRAPPER_BASE + 0x00000500)
#define PM_THRESHOLD1            (DEMOD_WRAPPER_BASE + 0x00000504)
#define PM_THRESHOLD2            (DEMOD_WRAPPER_BASE + 0x00000508)
#define PM_THRESHOLD_VLD_ACK     (DEMOD_WRAPPER_BASE + 0x0000050C)
#define PM_RO_THRESHOLD_ACK      (DEMOD_WRAPPER_BASE + 0x00000510)
#define PM_COUNTERS_LCH_VLD      (DEMOD_WRAPPER_BASE + 0x00000514)
#define PM_RO_COUNTERS_VALID     (DEMOD_WRAPPER_BASE + 0x00000518)
#define PM_RO_COUNT1_I           (DEMOD_WRAPPER_BASE + 0x0000051C)
#define PM_RO_COUNT1_Q           (DEMOD_WRAPPER_BASE + 0x00000520)
#define PM_RO_COUNT2_I           (DEMOD_WRAPPER_BASE + 0x00000524)
#define PM_RO_COUNT2_Q           (DEMOD_WRAPPER_BASE + 0x00000528)
#define PM_LOOP_CNTR             (DEMOD_WRAPPER_BASE + 0x0000052C)
#define PM_PARAMS_THRESHOLD      (DEMOD_WRAPPER_BASE + 0x00000530)
#define PM_PARAMS_VLD_ACK        (DEMOD_WRAPPER_BASE + 0x00000534)
#define PM_RO_PARAMS_ACK         (DEMOD_WRAPPER_BASE + 0x00000538)
#define PM_S2_LCH_VLD            (DEMOD_WRAPPER_BASE + 0x0000053C)
#define PM_RO_S2_VALID           (DEMOD_WRAPPER_BASE + 0x00000540)
#define PM_RO_THRSHLD_CNTR       (DEMOD_WRAPPER_BASE + 0x00000544)
#define PM_S3_LCH_VLD            (DEMOD_WRAPPER_BASE + 0x00000548)
#define PM_RO_S3_VALID           (DEMOD_WRAPPER_BASE + 0x0000054C)
#define PM_RO_POWER              (DEMOD_WRAPPER_BASE + 0x00000550)
#define PM_SSID                  (DEMOD_WRAPPER_BASE + 0x00000554)
#define PM_SID                   (DEMOD_WRAPPER_BASE + 0x00000558)
/* TS bridge */
#define BCDEM_REGS_TS_CFG        (DEMOD_WRAPPER_BASE + 0x00000800)
/* VBIF */
#define VBIF_FUNC_PRIO_CFG       (DEMOD_WRAPPER_BASE + 0x00000900)
#define VBIF_TEST_PRIO_CFG       (DEMOD_WRAPPER_BASE + 0x00000950)
/* TEST */
#define TEST_L1L2_TRUNC_MUX      (DEMOD_WRAPPER_BASE + 0x00000400)
#define TEST_SIF_CVBS            (DEMOD_WRAPPER_BASE + 0x00000404)
#define TEST_INJECTION           (DEMOD_WRAPPER_BASE + 0x00000408)
#define TEST_GRAM                (DEMOD_WRAPPER_BASE + 0x0000040C)
#define TEST_VBIF_MODE           (DEMOD_WRAPPER_BASE + 0x00000410)
#define TEST_VBIF_ERR            (DEMOD_WRAPPER_BASE + 0x00000414)
#define TEST_VBIF_ADDR           (DEMOD_WRAPPER_BASE + 0x00000418)
#define TEST_VBIF_DDR_SIZE	 (DEMOD_WRAPPER_BASE + 0x0000041C)
#define TEST_VBIF_SEGMENT_MODE   (DEMOD_WRAPPER_BASE + 0x00000420)
#define TEST_VBIF_FILL_LEVEL     (DEMOD_WRAPPER_BASE + 0x00000424)
#define TEST_NIDAQ_DIR           (DEMOD_WRAPPER_BASE + 0x00000428)
#define TEST_NIDAQ_PREAMB        (DEMOD_WRAPPER_BASE + 0x0000042C)
#define TEST_QDSS                (DEMOD_WRAPPER_BASE + 0x00000430)
#define TEST_ENABLES             (DEMOD_WRAPPER_BASE + 0x00000434)
#define TEST_G_ENABLE            (DEMOD_WRAPPER_BASE + 0x00000438)
#define TEST_DAQSS_SEQ           (DEMOD_WRAPPER_BASE + 0x0000043C)
#define TEST_DAQSS_CNT           (DEMOD_WRAPPER_BASE + 0x00000440)
#define TEST_DAQSS_DIS           (DEMOD_WRAPPER_BASE + 0x00000444)
#define REGS_DEBUG               (DEMOD_WRAPPER_BASE + 0x00000650)
/* IRQS */
#define GLOBAL_IRQ_STATUS        (DEMOD_WRAPPER_BASE + 0x00000700)
#define GLOBAL_IRQ_CLEAR         (DEMOD_WRAPPER_BASE + 0x00000704)
#define GLOBAL_IRQ_EN            (DEMOD_WRAPPER_BASE + 0x00000708)

/* RXFE_ATVRXFE_C1 */
#define CFG_ATVRXFE_ORDER (1<<1)
/* RXFE_ATVRXFE_C2 */
#define CFG_ATVRXFE_LUT_ENTRY0_SHIFT 0
#define CFG_ATVRXFE_LUT_ENTRY1_SHIFT 4
#define CFG_ATVRXFE_LUT_ENTRY2_SHIFT 8
#define CFG_ATVRXFE_LUT_ENTRY3_SHIFT 12
#define CFG_ATVRXFE_LUT_ENTRY4_SHIFT 16
#define CFG_ATVRXFE_LUT_ENTRY5_SHIFT 20
#define CFG_ATVRXFE_LUT_ENTRY6_SHIFT 24
#define CFG_ATVRXFE_LUT_ENTRY7_SHIFT 28
/* RXFE_ATVRXFE_C3 */
#define CFG_ATVRXFE_LUT_ENTRY8_SHIFT 0
#define CFG_ATVRXFE_LUT_ENTRY9_SHIFT 4
#define CFG_ATVRXFE_LUT_ENTRY10_SHIFT 8
#define CFG_ATVRXFE_LUT_ENTRY11_SHIFT 12
#define CFG_ATVRXFE_LUT_ENTRY12_SHIFT 16
#define CFG_ATVRXFE_LUT_ENTRY13_SHIFT 20
#define CFG_ATVRXFE_LUT_ENTRY14_SHIFT 24
#define CFG_ATVRXFE_LUT_ENTRY15_SHIFT 28
/* RXFE_ATVRXFE_C4 */
#define CFG_ATVRXFE_PRE_FILTER_OUT_SEL (1<<8)
/* RXFE_ATVRXFE_C5 */
#define CFG_ATVRXFE_RESAMPLER_BYPASS (1<<0)
/* RXFE_ATVRXFE_C6 */
#define CFG_ATVRXFE_RESAMPLER_T2_TO_T1M1_MASK ((1<<30)-1)
/* RXFE_ATVRXFE_C7 */
#define CFG_ATVRXFE_RESAMPLER_T1_TO_T2_MASK (0x7F)
#define CFG_ATVRXFE_RESAMPLER_OUT_GAIN_MASK (0x7F<<8)
#define CFG_ATVRXFE_RESAMPLER_OUT_GAIN_SHIFT 8
#define CFG_ATVRXFE_RESAMPLER_OUT_RND_MASK   (0x7<<16)
#define CFG_ATVRXFE_RESAMPLER_OUT_RND_SHIFT  16
#define CFG_ATVRXFE_DECIM_SHIFT 20
#define CFG_ATVRXFE_DECIM_MASK (3<<20)
/* RXFE_ATVRXFE_C8 */
#define CFG_ATVRXFE_FIR3_RND_BITS (1<<0)
/* RXFE_ATVRXFE_C13 */
#define CFG_ATVRXFE_FIR_EQ_VAL_A_MASK (0x1FFF)
#define CFG_ATVRXFE_FIR_EQ_VAL_A_SHIFT (0)
#define CFG_ATVRXFE_FIR_EQ_VAL_B_MASK (0x1FFF<<16)
#define CFG_ATVRXFE_FIR_EQ_VAL_B_SHIFT (16)
/* RXFE_ATVRXFE_C13A */
#define CFG_ATVRXFE_FIR_EQ_VAL_C_MASK (0x1FFF)
/* RXFE_ATVRXFE_C14 */
#define CFG_ATVRXFE_IIR_EQ_BYPASS      (1<<0)
#define CFG_ATVRXFE_IIR_EQ_VAL_A_MASK (0xFFF<<4)
#define CFG_ATVRXFE_IIR_EQ_VAL_A_SHIFT 4
#define CFG_ATVRXFE_IIR_EQ_VAL_B_MASK (0xFFF<<16)
#define CFG_ATVRXFE_IIR_EQ_VAL_B_SHIFT 16

/* For power meter polling */
#define MAX_POLLING 10

/* Structs */

/**
 * struct debugfs_entry - entry in debugfs array
 *
 * @name:		Name of register.
 * @offset:		Offset of register in demod wrapper.
 * @mode:		Acess mode of debugfs register.
 */
struct debugfs_entry {
	const char *name;
	int offset;
	mode_t mode;
};

/**
 * struct demod_wrapper_device - holds all data of demod wrapper device.
 *
 * @demod_wrapper_dev: dev_t variable needed for creating user space device.
 * @c_dev: cdev struct needed for creating user space device.
 * @demod_wrapper_class: class struct needed for creating user space device.
 * @dev: holding the created device.
 * @path_control: holds info about activated paths.
 * @fd_control: holds file descriptors that activated paths.
 * @num_of_activated_paths:
 * @fd_mutex: mutex restricting access to module functions (open,close,ioctl)
 * from diffrent file descriptors.
 * @ts_bridge_init: flag indicating if ts bridge data was initialized.
 * @num_of_fds: number of performed demod_wrapper_open.
 * @base: base address of demod wrapper block casted to unsigned int.
 * @base_mem: base address of demod wrapper block.
 * @debugfs_dir: the directory of debugfs.
 * @gdsc: the power regulator.
 * @reg_l27: l27 regulator.
 * @pdev: holds platform device.
 * @wakeup_source: used to prevent from the system to suspend.
 * @adc_01_clk_src: clock to set rate.
 * @adc_2_clk_src: clock to set rate.
 * @bcc_adc_0_in_clk: clock to enable/disable.
 * @bcc_adc_1_in_clk: clock to enable/disable.
 * @bcc_adc_2_in_clk: clock to enable/disable.
 * @bcc_adc_0_out_clk: out clock of adc0.
 * @bcc_adc_1_out_clk: out clock of adc1.
 * @bcc_adc_2_out_clk: out clock of adc2.
 * @dem_rxfe_clk_src: mux to img-rxfe.
 * @atv_rxfe_clk_src: mux to atv-rxfe.
 * @bcc_dem_rxfe_i_clk: clock to enable/disable.
 * @bcc_dem_rxfe_div3_mux_div4_i_clk: mux to img-rxfe.
 * @bcc_dem_rxfe_if_clk_src: mux to img-rxfe.
 * @bcc_dem_rxfe_div3_i_clk: clock for img-rxfe.
 * @bcc_dem_rxfe_div4_i_clk: clock for img-rxfe.
 * @bcc_ts_out_clk_src: clock to set rate.
 * @bcc_ts_out_clk: clock to enable/disable.
 * @bcc_atv_rxfe_clk: clock to enable/disable.
 * @bcc_atv_rxfe_resamp_clk_src: clock to enable/disable.
 * @bcc_atv_rxfe_resamp_clk: mux to atv-rxfe.
 * @bcc_atv_rxfe_x1_resamp_clk: mux to atv-rxfe.
 * @bcc_atv_rxfe_div8_clk: cock for atv-rxfe.
 * @albacore_sif_clk: mux to albacore-sif.
 * @bcc_atv_x1_clk: clock for atv-rxfe.
 * @bcc_forza_sync_x5_clk: enable/disable to forza sync.
 * @atv_x5_clk: enable/disable forza clock.
 * @bcc_dem_ahb_clk: enable/disable for adcs.
 */
struct demod_wrapper_device {
	/* for user space device */
	dev_t demod_wrapper_dev;
	struct cdev c_dev;
	struct class *demod_wrapper_class;
	struct device *dev;
	/* paths maintenance */
	bool path_control[DEMOD_WRAPPER_NUM_OF_PATHS];
	struct file *fd_control[DEMOD_WRAPPER_NUM_OF_PATHS];
	int num_of_activated_paths;
	/* ts-bridge maintenance */
	bool ts_bridge_init;
	/* general */
	struct mutex fd_mutex;
	int num_of_fds;
	unsigned int base;
	void __iomem *base_mem;
	struct dentry *debugfs_dir;
	struct regulator *gdsc;
	struct regulator *reg_l27;
	struct platform_device *pdev;
	struct wakeup_source wakeup_src;
	/* clocks */
	struct clk *adc_01_clk_src; /* set rate adc0 and adc1 */
	struct clk *adc_2_clk_src; /* set rate adc2 */
	struct clk *bcc_adc_0_in_clk; /* enable/disable adc0 */
	struct clk *bcc_adc_1_in_clk; /* enable/disable adc1 */
	struct clk *bcc_adc_2_in_clk; /* enable/disable adc2 */
	struct clk *bcc_adc_0_out_clk; /* for adc mux set parent */
	struct clk *bcc_adc_1_out_clk; /* for adc mux set parent */
	struct clk *bcc_adc_2_out_clk; /* for adc mux set parent */
	struct clk *dem_rxfe_clk_src; /* mux to choose dtv_sat ot dtv_t_c */
	struct clk *atv_rxfe_clk_src; /* mux to choose ext_atv or forza*/
	struct clk *bcc_dem_rxfe_i_clk; /* enable-disable make sure that */
					/*I don't need to set rate */
	struct clk *bcc_dem_rxfe_div3_mux_div4_i_clk; /* mux to choose if we */
						/* div 3 or div4 what goes to */
						/* img_rxfe.*/
	struct clk *bcc_dem_rxfe_if_clk_src; /* mux*/
	struct clk *bcc_dem_rxfe_div3_i_clk; /* for set parent */
	struct clk *bcc_dem_rxfe_div4_i_clk; /* for set parent */
	struct clk *bcc_ts_out_clk_src; /* set rate */
	struct clk *bcc_ts_out_clk; /* enable-disable */
	struct clk *bcc_atv_rxfe_clk; /* enable-disable should also set rate? */
	struct clk *bcc_atv_rxfe_resamp_clk_src; /* set_rate enable-disable */
	struct clk *bcc_atv_rxfe_resamp_clk; /* mux */
	struct clk *bcc_atv_rxfe_x1_resamp_clk; /* mux */
	struct clk *bcc_atv_rxfe_div8_clk; /* atv-rxfe */
	struct clk *albacore_sif_clk; /* mux if clock to albacore */
					/* goes from forza or atv*/
	struct clk *bcc_atv_x1_clk; /* atv rxfe */
	struct clk *bcc_forza_sync_x5_clk; /* enable-disable forza sync*/
	struct clk *atv_x5_clk; /* forza */
	struct clk *bcc_dem_ahb_clk; /* for adc */
};

/**
 * struct demod_wrapper_clocks_info - names of the clocks for passing to
 * clk_get function.
 *
 * @adc_01_clk_src: clock to set rate.
 * @adc_2_clk_src: clock to set rate.
 * @bcc_adc_0_in_clk: clock to enable/disable.
 * @bcc_adc_1_in_clk: clock to enable/disable.
 * @bcc_adc_2_in_clk: clock to enable/disable.
 * @bcc_adc_0_out_clk: out clock of adc0.
 * @bcc_adc_1_out_clk: out clock of adc1.
 * @bcc_adc_2_out_clk: out clock of adc2.
 * @dem_rxfe_clk_src: mux to img-rxfe.
 * @atv_rxfe_clk_src: mux to atv-rxfe.
 * @bcc_dem_rxfe_i_clk: clock to enable/disable.
 * @bcc_dem_rxfe_div3_mux_div4_i_clk: mux to img-rxfe.
 * @bcc_dem_rxfe_if_clk_src: mux to img-rxfe.
 * @bcc_dem_rxfe_div3_i_clk: clock for img-rxfe.
 * @bcc_dem_rxfe_div4_i_clk: clock for img-rxfe.
 * @bcc_ts_out_clk_src: clock to set rate.
 * @bcc_ts_out_clk: clock to enable/disable.
 * @bcc_atv_rxfe_clk: clock to enable/disable.
 * @bcc_atv_rxfe_resamp_clk_src: clock to enable/disable.
 * @bcc_atv_rxfe_resamp_clk: mux to atv-rxfe.
 * @bcc_atv_rxfe_x1_resamp_clk: mux to atv-rxfe.
 * @bcc_atv_rxfe_div8_clk: cock for atv-rxfe.
 * @albacore_sif_clk: mux to albacore-sif.
 * @bcc_atv_x1_clk: clock for atv-rxfe.
 * @bcc_forza_sync_x5_clk: enable/disable to forza sync.
 * @atv_x5_clk: enable/disable forza clock.
 * @bcc_dem_ahb_clk: enable/disable for adcs.
 */
struct demod_wrapper_clocks_info {
	const char *adc_01_clk_src;
	const char *adc_2_clk_src;
	const char *bcc_adc_0_in_clk;
	const char *bcc_adc_1_in_clk;
	const char *bcc_adc_2_in_clk;
	const char *bcc_adc_0_out_clk;
	const char *bcc_adc_1_out_clk;
	const char *bcc_adc_2_out_clk;
	const char *dem_rxfe_clk_src;
	const char *atv_rxfe_clk_src;
	const char *bcc_dem_rxfe_i_clk;
	const char *bcc_dem_rxfe_div3_mux_div4_i_clk;
	const char *bcc_dem_rxfe_if_clk_src;
	const char *bcc_dem_rxfe_div3_i_clk;
	const char *bcc_dem_rxfe_div4_i_clk;
	const char *bcc_ts_out_clk_src;
	const char *bcc_ts_out_clk;
	const char *bcc_atv_rxfe_clk;
	const char *bcc_atv_rxfe_resamp_clk_src;
	const char *bcc_atv_rxfe_resamp_clk;
	const char *bcc_atv_rxfe_x1_resamp_clk;
	const char *bcc_atv_rxfe_div8_clk;
	const char *albacore_sif_clk;
	const char *bcc_atv_x1_clk;
	const char *bcc_forza_sync_x5_clk;
	const char *atv_x5_clk;
	const char *bcc_dem_ahb_clk;
};

static struct demod_wrapper_device *device;
static struct demod_wrapper_clocks_info clocks_info = {
	.adc_01_clk_src = "adc_01_clk_src",
	.adc_2_clk_src = "adc_2_clk_src",
	.bcc_adc_0_in_clk = "bcc_adc_0_in_clk",
	.bcc_adc_1_in_clk = "bcc_adc_1_in_clk",
	.bcc_adc_2_in_clk = "bcc_adc_2_in_clk",
	.bcc_adc_0_out_clk = "bcc_adc_0_out_clk",
	.bcc_adc_1_out_clk = "bcc_adc_1_out_clk",
	.bcc_adc_2_out_clk = "bcc_adc_2_out_clk",
	.dem_rxfe_clk_src = "dem_rxfe_clk_src",
	.atv_rxfe_clk_src = "atv_rxfe_clk_src",
	.bcc_dem_rxfe_i_clk = "bcc_dem_rxfe_i_clk",
	.bcc_dem_rxfe_div3_mux_div4_i_clk = "bcc_dem_rxfe_div3_mux_div4_i_clk",
	.bcc_dem_rxfe_if_clk_src = "bcc_dem_rxfe_if_clk_src",
	.bcc_dem_rxfe_div3_i_clk = "bcc_dem_rxfe_div3_i_clk",
	.bcc_dem_rxfe_div4_i_clk = "bcc_dem_rxfe_div4_i_clk",
	.bcc_ts_out_clk_src = "bcc_ts_out_clk_src",
	.bcc_ts_out_clk = "bcc_ts_out_clk",
	.bcc_atv_rxfe_clk = "bcc_atv_rxfe_clk",
	.bcc_atv_rxfe_resamp_clk_src = "bcc_atv_rxfe_resamp_clk_src",
	.bcc_atv_rxfe_resamp_clk = "bcc_atv_rxfe_resamp_clk",
	.bcc_atv_rxfe_x1_resamp_clk = "bcc_atv_rxfe_x1_resamp_clk",
	.bcc_atv_rxfe_div8_clk = "bcc_atv_rxfe_div8_clk",
	.albacore_sif_clk = "albacore_sif_clk",
	.bcc_atv_x1_clk = "bcc_atv_x1_clk",
	.bcc_forza_sync_x5_clk = "bcc_forza_sync_x5_clk",
	.atv_x5_clk = "atv_x5_clk",
	.bcc_dem_ahb_clk = "bcc_dem_ahb_clk"
};

static struct debugfs_entry debugfs_demod_wrapper_regs[] = {
	{"bcdem_revision", BCDEM_REVISION, S_IRUGO | S_IWUSR},
	{"bcdem_ctrl", BCDEM_CTRL, S_IRUGO | S_IWUSR},
	{"adc_mux_sel", ADC_MUX_SEL, S_IRUGO | S_IWUSR},
	{"bcdem_cgc", BCDEM_CGC, S_IRUGO | S_IWUSR},
	{"bcdem_adc_power_down", BCDEM_ADC_POWER_DOWN, S_IRUGO | S_IWUSR},
	{"uccp_metx_boot", UCCP_METX_BOOT, S_IRUGO | S_IWUSR},
	{"atv_post_process_cvbs_sif", ATV_POST_PROCESS_CVBS_SIF,
							S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c1", RXFE_IMGRXFE_C1, S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c2", RXFE_IMGRXFE_C2, S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c3", RXFE_IMGRXFE_C3, S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c4", RXFE_IMGRXFE_C4, S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff0", RXFE_IMGRXFE_C10_COEFF(0),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff1", RXFE_IMGRXFE_C10_COEFF(1),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff2", RXFE_IMGRXFE_C10_COEFF(2),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff3", RXFE_IMGRXFE_C10_COEFF(3),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff4", RXFE_IMGRXFE_C10_COEFF(4),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c10_coeff5", RXFE_IMGRXFE_C10_COEFF(5),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c11", RXFE_IMGRXFE_C11, S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff0", RXFE_IMGRXFE_C12_COEFF(0),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff1", RXFE_IMGRXFE_C12_COEFF(1),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff2", RXFE_IMGRXFE_C12_COEFF(2),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff3", RXFE_IMGRXFE_C12_COEFF(3),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff4", RXFE_IMGRXFE_C12_COEFF(4),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff5", RXFE_IMGRXFE_C12_COEFF(5),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff6", RXFE_IMGRXFE_C12_COEFF(6),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff7", RXFE_IMGRXFE_C12_COEFF(7),
						S_IRUGO | S_IWUSR},
	{"rxfe_imgrxfe_c12_coeff8", RXFE_IMGRXFE_C12_COEFF(8),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c1", RXFE_ATVRXFE_C1, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c2", RXFE_ATVRXFE_C2, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c3", RXFE_ATVRXFE_C3, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c4", RXFE_ATVRXFE_C4, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c5", RXFE_ATVRXFE_C5, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c6", RXFE_ATVRXFE_C6, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c7", RXFE_ATVRXFE_C7, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c8", RXFE_ATVRXFE_C1, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c9_coeff0", RXFE_ATVRXFE_C9_COEFF(0),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c9_coeff1", RXFE_ATVRXFE_C9_COEFF(1),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c9_coeff2", RXFE_ATVRXFE_C9_COEFF(2),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c9_coeff3", RXFE_ATVRXFE_C9_COEFF(3),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c9_coeff4", RXFE_ATVRXFE_C9_COEFF(4),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff0", RXFE_ATVRXFE_C10_COEFF(0),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff1", RXFE_ATVRXFE_C10_COEFF(1),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff2", RXFE_ATVRXFE_C10_COEFF(2),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff3", RXFE_ATVRXFE_C10_COEFF(3),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff4", RXFE_ATVRXFE_C10_COEFF(4),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c10_coeff5", RXFE_ATVRXFE_C10_COEFF(5),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff0", RXFE_ATVRXFE_C11_COEFF(0),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff1", RXFE_ATVRXFE_C11_COEFF(1),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff2", RXFE_ATVRXFE_C11_COEFF(2),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff3", RXFE_ATVRXFE_C11_COEFF(3),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff4", RXFE_ATVRXFE_C11_COEFF(4),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff5", RXFE_ATVRXFE_C11_COEFF(5),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff6", RXFE_ATVRXFE_C11_COEFF(6),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff7", RXFE_ATVRXFE_C11_COEFF(7),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff8", RXFE_ATVRXFE_C11_COEFF(8),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff9", RXFE_ATVRXFE_C11_COEFF(9),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c11_coeff10", RXFE_ATVRXFE_C11_COEFF(10),
						S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c12", RXFE_ATVRXFE_C12, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c13", RXFE_ATVRXFE_C13, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c13a", RXFE_ATVRXFE_C13A, S_IRUGO | S_IWUSR},
	{"rxfe_atvrxfe_c14", RXFE_ATVRXFE_C14, S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c1", RXFE_IMGSIF_C1, S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff0", RXFE_IMGSIF_C2_COEFF(0), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff1", RXFE_IMGSIF_C2_COEFF(1), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff2", RXFE_IMGSIF_C2_COEFF(2), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff3", RXFE_IMGSIF_C2_COEFF(3), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff4", RXFE_IMGSIF_C2_COEFF(4), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff5", RXFE_IMGSIF_C2_COEFF(5), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff6", RXFE_IMGSIF_C2_COEFF(6), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff7", RXFE_IMGSIF_C2_COEFF(7), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff8", RXFE_IMGSIF_C2_COEFF(8), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c2_coeff9", RXFE_IMGSIF_C2_COEFF(9), S_IRUGO | S_IWUSR},
	{"rxfe_imgsif_c3", RXFE_IMGSIF_C3, S_IRUGO | S_IWUSR},
	{"adc_bbr0_test1", ADC_BBR0_TEST1, S_IRUGO | S_IWUSR},
	{"adc_bbr0_test2", ADC_BBR0_TEST2, S_IRUGO | S_IWUSR},
	{"adc_bbr0_test3", ADC_BBR0_TEST3, S_IRUGO | S_IWUSR},
	{"adc_bbr0_config", ADC_BBR0_CONFIG, S_IRUGO | S_IWUSR},
	{"adc_bbr0_misc", ADC_BBR0_MISC, S_IRUGO | S_IWUSR},
	{"adc_bbr1_test1", ADC_BBR1_TEST1, S_IRUGO | S_IWUSR},
	{"adc_bbr1_test2", ADC_BBR1_TEST2, S_IRUGO | S_IWUSR},
	{"adc_bbr1_test3", ADC_BBR1_TEST3, S_IRUGO | S_IWUSR},
	{"adc_bbr1_config", ADC_BBR1_CONFIG, S_IRUGO | S_IWUSR},
	{"adc_bbr1_misc", ADC_BBR1_MISC, S_IRUGO | S_IWUSR},
	{"adc_bbr2_test1", ADC_BBR2_TEST1, S_IRUGO | S_IWUSR},
	{"adc_bbr2_test2", ADC_BBR2_TEST2, S_IRUGO | S_IWUSR},
	{"adc_bbr2_test3", ADC_BBR2_TEST3, S_IRUGO | S_IWUSR},
	{"adc_bbr2_config", ADC_BBR2_CONFIG, S_IRUGO | S_IWUSR},
	{"adc_bbr2_misc", ADC_BBR2_MISC, S_IRUGO | S_IWUSR},
	{"bcdem_pdm0_control", PDM0_CONTROL, S_IRUGO | S_IWUSR},
	{"bcdem_pdm0_gain_ctrl_in", PDM0_GAIN_CTRL_IN, S_IRUGO | S_IWUSR},
	{"bcdem_pdm0_gain_ctrl_out", PDM0_GAIN_CTRL_OUT, S_IRUGO | S_IWUSR},
	{"bcdem_pdm1_control", PDM1_CONTROL, S_IRUGO | S_IWUSR},
	{"bcdem_pdm1_gain_ctrl_in", PDM1_GAIN_CTRL_IN, S_IRUGO | S_IWUSR},
	{"bcdem_pdm1_gain_ctrl_out", PDM0_GAIN_CTRL_OUT, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_l1l2_trunc_mux", TEST_L1L2_TRUNC_MUX,
							S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_sif_cvbs", TEST_SIF_CVBS, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_injection", TEST_INJECTION, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_gram", TEST_GRAM, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_mode", TEST_VBIF_MODE, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_err", TEST_VBIF_ERR, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_addr",	TEST_VBIF_ADDR, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_ddr_size", TEST_VBIF_DDR_SIZE,
							S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_segment_mode", TEST_VBIF_SEGMENT_MODE,
							S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_vbif_fill_level", TEST_VBIF_FILL_LEVEL,
							S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_nidaq_dir", TEST_NIDAQ_DIR, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_nidaq_preamb", TEST_NIDAQ_PREAMB,
							S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_qdss", TEST_QDSS, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_enables", TEST_ENABLES, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_g_enable", TEST_G_ENABLE, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_daqss_seq", TEST_DAQSS_SEQ, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_daqss_cnt", TEST_DAQSS_CNT, S_IRUGO | S_IWUSR},
	{"bcdem_demod_test_daqss_dis", TEST_DAQSS_DIS, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ctrl", PM_CTRL, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_threshold1", PM_THRESHOLD1, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_threshold2", PM_THRESHOLD2, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_threshold_vld_ack", PM_THRESHOLD_VLD_ACK,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_threshold_ack", PM_RO_THRESHOLD_ACK,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_counters_lch_vld", PM_COUNTERS_LCH_VLD,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_counters_valid", PM_RO_COUNTERS_VALID,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_count1_i", PM_RO_COUNT1_I, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_count1_a", PM_RO_COUNT1_Q, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_count2_i", PM_RO_COUNT2_I, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_count2_q", PM_RO_COUNT2_Q, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_loop_cntr", PM_LOOP_CNTR, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_params_threshold", PM_PARAMS_THRESHOLD,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_params_vld_ack", PM_PARAMS_VLD_ACK, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_params_ack", PM_RO_PARAMS_ACK, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_s2_lch_vld", PM_S2_LCH_VLD, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_s2_valid", PM_RO_S2_VALID, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_to_thrshld_cntr", PM_RO_THRSHLD_CNTR,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_s3_lch_vld", PM_S3_LCH_VLD, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_s3_valid", PM_RO_S3_VALID, S_IRUGO | S_IWUSR},
	{"bcdem_regs_pm_ro_power", PM_RO_POWER, S_IRUGO | S_IWUSR},
	{"bcdem_regs_debug", REGS_DEBUG, S_IRUGO | S_IWUSR},
	{"bcdem_global_irq_status", GLOBAL_IRQ_STATUS, S_IRUGO | S_IWUSR},
	{"bcdem_global_irq_clear", GLOBAL_IRQ_CLEAR, S_IRUGO | S_IWUSR},
	{"bcdem_global_irq_en",	GLOBAL_IRQ_EN, S_IRUGO | S_IWUSR},
	{"bcdem_regs_ts_cfg", BCDEM_REGS_TS_CFG, S_IRUGO | S_IWUSR},
	{"bcdem_regs_vbif_func_priority_cfg", VBIF_FUNC_PRIO_CFG,
							S_IRUGO | S_IWUSR},
	{"bcdem_regs_vbif_test_priority_cfg", VBIF_TEST_PRIO_CFG,
							S_IRUGO | S_IWUSR}
};

/**
 * init_data_structs() - initialize data of device struct
 */
static void init_data_structs(void)
{
	int i;

	/* init mutex */
	mutex_init(&(device->fd_mutex));
	/* init path_control */
	for (i = 0; i < DEMOD_WRAPPER_NUM_OF_PATHS; i++) {
		device->path_control[i] = false;
		device->fd_control[i] = NULL;
	}
	/* Variables initialization */
	device->num_of_activated_paths = 0;
	device->num_of_fds = 0;
	device->ts_bridge_init = false;
}

/* Functions for Debufgs */

static int debugfs_iomem_x32_set(void *data, u64 val)
{
	int rc = 0;
	int *offset = (int *)data;

	rc = mutex_lock_interruptible(&(device->fd_mutex));
	if (rc != 0) {
		dev_err(device->dev, "mutex_lock_interruptible on fd_mutex returned with error\n");
		goto end;
	}
	if (device->num_of_fds == 0) {
		dev_err(device->dev, "%s: device not opened\n", __func__);
		rc = -EPERM;
		goto mutex_unlock;
	}

	writel_relaxed(val, device->base + *offset);
	wmb();
mutex_unlock:
	mutex_unlock(&(device->fd_mutex));
end:
	return rc;
}

static int debugfs_iomem_x32_get(void *data, u64 *val)
{
	int rc = 0;
	int *offset = (int *)data;

	rc = mutex_lock_interruptible(&(device->fd_mutex));
	if (rc != 0) {
		dev_err(device->dev, "mutex_lock_interruptible on fd_mutex returned with error\n");
		goto end;
	}
	if (device->num_of_fds == 0) {
		dev_err(device->dev, "%s: device not opened\n", __func__);
		rc = -EPERM;
		goto mutex_unlock;
	}

	*val = readl_relaxed(device->base + *offset);
mutex_unlock:
	mutex_unlock(&(device->fd_mutex));
end:
	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_iomem_x32, debugfs_iomem_x32_get,
			debugfs_iomem_x32_set, "0x%08llx");

static void demod_wrapper_debugfs_init(void)
{
	int i;

	device->debugfs_dir = debugfs_create_dir(
	      "demod_wrapper", NULL);
	if (device->debugfs_dir) {
		for (i = 0; i < ARRAY_SIZE(debugfs_demod_wrapper_regs); i++) {
			debugfs_create_file(
				debugfs_demod_wrapper_regs[i].name,
				debugfs_demod_wrapper_regs[i].mode,
				device->debugfs_dir,
				&(debugfs_demod_wrapper_regs[i].offset),
				&fops_iomem_x32);
		}
	} else {
		dev_err(device->dev, "debugfs_create_dir returned with error\n");
	}
}

static void demod_wrapper_debugfs_exit(void)
{
	debugfs_remove_recursive(device->debugfs_dir);
	device->debugfs_dir = NULL;
}

/**
 * activate_forza_atv_path() - Activates Forza ATV path in demod wrapper.
 *
 * @pdm: indicates the pdm number to work with.
 *
 * Return 0 on success, error value otherwise.
 */
static int activate_forza_atv_path(enum demod_wrapper_pdm_num pdm)
{
	unsigned int base = device->base;
	unsigned int adc_reg_val = 0x00000010;
	unsigned int pdm_addrs;
	int rc = 0;

	dev_dbg(device->dev, "%s\n", __func__);

	/* clocks muxes */
	rc = clk_set_parent(device->atv_rxfe_clk_src,
		device->bcc_adc_1_out_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for atv_rxfe_clk_src returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_atv_rxfe_x1_resamp_clk,
		device->bcc_atv_rxfe_div8_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_atv_rxfe_x1_resamp_clk returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_atv_rxfe_resamp_clk,
		device->bcc_atv_rxfe_resamp_clk_src);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_atv_rxfe_resamp_clk returned with error");
		goto end;
	}
	rc = clk_set_parent(device->albacore_sif_clk,
		device->bcc_atv_x1_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for albacore_sif_mux returned with error");
		goto end;
	}

	/* adc mux */
	writel_relaxed(adc_reg_val, base + ADC_MUX_SEL);
	/* atv_rxfe init */
	writel_relaxed(0, base + RXFE_ATVRXFE_C1);
	writel_relaxed(
			(8 << CFG_ATVRXFE_LUT_ENTRY0_SHIFT) |
			(9 << CFG_ATVRXFE_LUT_ENTRY1_SHIFT) |
			(11 << CFG_ATVRXFE_LUT_ENTRY2_SHIFT) |
			(10 << CFG_ATVRXFE_LUT_ENTRY3_SHIFT) |
			(15 << CFG_ATVRXFE_LUT_ENTRY4_SHIFT) |
			(14 << CFG_ATVRXFE_LUT_ENTRY5_SHIFT) |
			(12 << CFG_ATVRXFE_LUT_ENTRY6_SHIFT) |
			(13UL << CFG_ATVRXFE_LUT_ENTRY7_SHIFT),
			base + RXFE_ATVRXFE_C2);

	writel_relaxed(
			(7 << CFG_ATVRXFE_LUT_ENTRY8_SHIFT) |
			(6 << CFG_ATVRXFE_LUT_ENTRY9_SHIFT) |
			(4 << CFG_ATVRXFE_LUT_ENTRY10_SHIFT) |
			(5 << CFG_ATVRXFE_LUT_ENTRY11_SHIFT) |
			(0 << CFG_ATVRXFE_LUT_ENTRY12_SHIFT) |
			(1 << CFG_ATVRXFE_LUT_ENTRY13_SHIFT) |
			(3 << CFG_ATVRXFE_LUT_ENTRY14_SHIFT) |
			(2 << CFG_ATVRXFE_LUT_ENTRY15_SHIFT),
			base + RXFE_ATVRXFE_C3);

	writel_relaxed(7, base + PM_LOOP_CNTR);
	writel_relaxed(100, base + PM_PARAMS_THRESHOLD);

	writel_relaxed(CFG_ATVRXFE_PRE_FILTER_OUT_SEL, base + RXFE_ATVRXFE_C4);
	writel_relaxed(0, base + RXFE_ATVRXFE_C5);
	writel_relaxed(603979776 & CFG_ATVRXFE_RESAMPLER_T2_TO_T1M1_MASK,
	base + RXFE_ATVRXFE_C6);

	writel_relaxed(
	(82 & CFG_ATVRXFE_RESAMPLER_T1_TO_T2_MASK) |
	((20 << CFG_ATVRXFE_RESAMPLER_OUT_GAIN_SHIFT) &
	CFG_ATVRXFE_RESAMPLER_OUT_GAIN_MASK) |
	((2 << CFG_ATVRXFE_RESAMPLER_OUT_RND_SHIFT) &
	CFG_ATVRXFE_RESAMPLER_OUT_RND_MASK) |
	((3 << CFG_ATVRXFE_DECIM_SHIFT) & CFG_ATVRXFE_DECIM_MASK),
	base + RXFE_ATVRXFE_C7);

	writel_relaxed(CFG_ATVRXFE_FIR3_RND_BITS, base + RXFE_ATVRXFE_C8);

	writel_relaxed(-6, base + RXFE_ATVRXFE_C9_COEFF(0));
	writel_relaxed(52, base + RXFE_ATVRXFE_C9_COEFF(1));
	writel_relaxed(-244, base + RXFE_ATVRXFE_C9_COEFF(2));
	writel_relaxed(1186, base + RXFE_ATVRXFE_C9_COEFF(3));
	writel_relaxed(1976, base + RXFE_ATVRXFE_C9_COEFF(4));

	writel_relaxed(3, base + RXFE_ATVRXFE_C10_COEFF(0));
	writel_relaxed(-22, base + RXFE_ATVRXFE_C10_COEFF(1));
	writel_relaxed(92, base + RXFE_ATVRXFE_C10_COEFF(2));
	writel_relaxed(-299, base + RXFE_ATVRXFE_C10_COEFF(3));
	writel_relaxed(1234, base + RXFE_ATVRXFE_C10_COEFF(4));
	writel_relaxed(2016, base + RXFE_ATVRXFE_C10_COEFF(5));

	writel_relaxed(-2, base + RXFE_ATVRXFE_C11_COEFF(0));
	writel_relaxed(10, base + RXFE_ATVRXFE_C11_COEFF(1));
	writel_relaxed(-28, base + RXFE_ATVRXFE_C11_COEFF(2));
	writel_relaxed(66, base + RXFE_ATVRXFE_C11_COEFF(3));
	writel_relaxed(-135, base + RXFE_ATVRXFE_C11_COEFF(4));
	writel_relaxed(253, base + RXFE_ATVRXFE_C11_COEFF(5));
	writel_relaxed(-451, base + RXFE_ATVRXFE_C11_COEFF(6));
	writel_relaxed(800, base + RXFE_ATVRXFE_C11_COEFF(7));
	writel_relaxed(-1556, base + RXFE_ATVRXFE_C11_COEFF(8));
	writel_relaxed(5036, base + RXFE_ATVRXFE_C11_COEFF(9));
	writel_relaxed(7986, base + RXFE_ATVRXFE_C11_COEFF(10));

	writel_relaxed(0, base + RXFE_ATVRXFE_C12);

	writel_relaxed(
	((0x001F << CFG_ATVRXFE_FIR_EQ_VAL_A_SHIFT) &
	CFG_ATVRXFE_FIR_EQ_VAL_A_MASK) |
	((0x1DE8 << CFG_ATVRXFE_FIR_EQ_VAL_B_SHIFT) &
	CFG_ATVRXFE_FIR_EQ_VAL_B_MASK),
	base + RXFE_ATVRXFE_C13);

	writel_relaxed(0x09F9 & CFG_ATVRXFE_FIR_EQ_VAL_C_MASK,
			base + RXFE_ATVRXFE_C13A);

	writel_relaxed(
		CFG_ATVRXFE_IIR_EQ_BYPASS |
		((0x0B3 << CFG_ATVRXFE_IIR_EQ_VAL_A_SHIFT) &
		CFG_ATVRXFE_IIR_EQ_VAL_A_MASK) |
		((0x180 << CFG_ATVRXFE_IIR_EQ_VAL_B_SHIFT) &
		CFG_ATVRXFE_IIR_EQ_VAL_B_MASK),
		base + RXFE_ATVRXFE_C14);

	/* sif+cvbs mux */
	writel_relaxed(0x00001100, base + RXFE_IMGSIF_C3);

	/* pdm */
	switch (pdm) {
	case DEMOD_WRAPPER_PDM0:
		pdm_addrs = PDM0_CONTROL;
		break;
	case DEMOD_WRAPPER_PDM1:
		pdm_addrs = PDM1_CONTROL;
		break;
	default:
		dev_dbg(device->dev,
			"Illegal value in switch case of ioctl commands , shouldn’t get here\n");
		rc = -EINVAL;
		goto end;
	}
	writel_relaxed(0x80000003, base + pdm_addrs);
end:
	return rc;
}

/**
 * activate_dtv_s_path() - Activate DTV SAT path in demod wrapper.
 *
 * @baud_rate:	Baud rate mode
 *
 * Return 0 on success, error value otherwise.
 */
static int activate_dtv_s_path(
	enum demod_wrapper_baud_rate_mode baud_rate,
	enum demod_wrapper_pdm_num pdm)
{
	unsigned int base = device->base;
	unsigned int adc_reg_val = 0x00000000;
	unsigned int pdm_addrs;
	int rc = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	/* mutual part */
	/* clock muxes */
	rc = clk_set_parent(device->dem_rxfe_clk_src,
		device->bcc_adc_0_out_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for dem_rxfe_clk_src returned with error");
		goto end;
	}
	/* adc mux */
	if (device->path_control[DEMOD_WRAPPER_EXT_ATV])
		adc_reg_val = 0x00000020;
	writel_relaxed(adc_reg_val, base + ADC_MUX_SEL);

	writel_relaxed(0, base + RXFE_IMGRXFE_C1);
	/* cfg_imgrxfe_lut_entry0-7 */
	writel_relaxed(
		(8 << CFG_RXFE_LUT_ENTRY0_SHIFT) |
		(9 << CFG_RXFE_LUT_ENTRY1_SHIFT) |
		(11 << CFG_RXFE_LUT_ENTRY2_SHIFT) |
		(10 << CFG_RXFE_LUT_ENTRY3_SHIFT) |
		(15 << CFG_RXFE_LUT_ENTRY4_SHIFT) |
		(14 << CFG_RXFE_LUT_ENTRY5_SHIFT) |
		(12 << CFG_RXFE_LUT_ENTRY6_SHIFT) |
		(13UL << CFG_RXFE_LUT_ENTRY7_SHIFT),
		base + RXFE_IMGRXFE_C2);
	/* cfg_imgrxfe_lut_entry8-15 */
	writel_relaxed(
		(7 << CFG_RXFE_LUT_ENTRY8_SHIFT) |
		(6 << CFG_RXFE_LUT_ENTRY9_SHIFT) |
		(4 << CFG_RXFE_LUT_ENTRY10_SHIFT) |
		(5 << CFG_RXFE_LUT_ENTRY11_SHIFT) |
		(0 << CFG_RXFE_LUT_ENTRY12_SHIFT) |
		(1 << CFG_RXFE_LUT_ENTRY13_SHIFT) |
		(3 << CFG_RXFE_LUT_ENTRY14_SHIFT) |
		(2UL << CFG_RXFE_LUT_ENTRY15_SHIFT),
		base + RXFE_IMGRXFE_C3);

	switch (baud_rate) {
	case DEMOD_WRAPPER_WIDE:
		/* clocks mux */
		rc = clk_set_parent(device->bcc_dem_rxfe_div3_mux_div4_i_clk,
		device->bcc_dem_rxfe_div3_i_clk);
		if (rc) {
			dev_err(device->dev, "clk_set_parent for bcc_dem_rxfe_div3_mux_div4_i_clk returned with error");
			goto end;
		}
		/* img_rxfe */
		/* cfg_imgrxfe_decim */
		writel_relaxed(2, base + RXFE_IMGRXFE_C4);

		/* cfg_imgrxfe_pm_threshold1 */
		writel_relaxed(30, base + PM_THRESHOLD1);
		/* cfg_imgrxfe_pm_threshold2 */
		writel_relaxed(40, base + PM_THRESHOLD2);

		/* cfg_imgrxfe_fir1_coeff0-5 */
		writel_relaxed(8, base + RXFE_IMGRXFE_C10_COEFF(0));
		writel_relaxed(36, base + RXFE_IMGRXFE_C10_COEFF(1));
		writel_relaxed(70, base + RXFE_IMGRXFE_C10_COEFF(2));
		writel_relaxed(0, base + RXFE_IMGRXFE_C10_COEFF(3));
		writel_relaxed(0, base + RXFE_IMGRXFE_C10_COEFF(4));
		writel_relaxed(0, base + RXFE_IMGRXFE_C10_COEFF(5));

		/* cfg_imgrxfe_fir1_rnd_bits */
		writel_relaxed(1, base + RXFE_IMGRXFE_C11);

		/* cfg_imgrxfe_fir2_coeff0-8 */
		writel_relaxed(64, base + RXFE_IMGRXFE_C12_COEFF(0));
		writel_relaxed(-30, base + RXFE_IMGRXFE_C12_COEFF(1));
		writel_relaxed(-185, base + RXFE_IMGRXFE_C12_COEFF(2));
		writel_relaxed(-258, base + RXFE_IMGRXFE_C12_COEFF(3));
		writel_relaxed(-66, base + RXFE_IMGRXFE_C12_COEFF(4));
		writel_relaxed(454, base + RXFE_IMGRXFE_C12_COEFF(5));
		writel_relaxed(1124, base + RXFE_IMGRXFE_C12_COEFF(6));
		writel_relaxed(1598, base + RXFE_IMGRXFE_C12_COEFF(7));
		writel_relaxed(0, base + RXFE_IMGRXFE_C12_COEFF(8));
		break;

	case DEMOD_WRAPPER_MEDIUM:
		/* clocks mux */
		rc = clk_set_parent(device->bcc_dem_rxfe_div3_mux_div4_i_clk,
		device->bcc_dem_rxfe_div4_i_clk);
		if (rc) {
			dev_err(device->dev, "clk_set_parent for dem_rxfe_div3_mux_div4_i_clk returned with error");
			goto end;
		}
		/* img_rxfe */
		/* cfg_imgrxfe_decim */
		writel_relaxed(3, base + RXFE_IMGRXFE_C4);

		/* cfg_imgrxfe_pm_threshold1 */
		writel_relaxed(30, base + PM_THRESHOLD1);
		/* cfg_imgrxfe_pm_threshold2 */
		writel_relaxed(40, base + PM_THRESHOLD2);

		/* cfg_imgrxfe_fir1_coeff0-5 */
		writel_relaxed(14, base + RXFE_IMGRXFE_C10_COEFF(0));
		writel_relaxed(-52, base + RXFE_IMGRXFE_C10_COEFF(1));
		writel_relaxed(140, base + RXFE_IMGRXFE_C10_COEFF(2));
		writel_relaxed(-346, base + RXFE_IMGRXFE_C10_COEFF(3));
		writel_relaxed(1250, base + RXFE_IMGRXFE_C10_COEFF(4));
		writel_relaxed(2009, base + RXFE_IMGRXFE_C10_COEFF(5));

		/* cfg_imgrxfe_fir1_rnd_bits */
		writel_relaxed(1, base + RXFE_IMGRXFE_C11);

		/* cfg_imgrxfe_fir2_coeff0-8 */
		writel_relaxed(-3, base + RXFE_IMGRXFE_C12_COEFF(0));
		writel_relaxed(10, base + RXFE_IMGRXFE_C12_COEFF(1));
		writel_relaxed(-25, base + RXFE_IMGRXFE_C12_COEFF(2));
		writel_relaxed(53, base + RXFE_IMGRXFE_C12_COEFF(3));
		writel_relaxed(-102, base + RXFE_IMGRXFE_C12_COEFF(4));
		writel_relaxed(190, base + RXFE_IMGRXFE_C12_COEFF(5));
		writel_relaxed(-380, base + RXFE_IMGRXFE_C12_COEFF(6));
		writel_relaxed(1248, base + RXFE_IMGRXFE_C12_COEFF(7));
		writel_relaxed(1982, base + RXFE_IMGRXFE_C12_COEFF(8));
		break;
	case DEMOD_WRAPPER_NARROW:
		/* clocks mux */
		rc = clk_set_parent(device->bcc_dem_rxfe_div3_mux_div4_i_clk,
		device->bcc_dem_rxfe_div4_i_clk);
		if (rc) {
			dev_err(device->dev, "clk_set_parent for bcc_dem_rxfe_div3_mux_div4_i_clk returned with error");
			goto end;
		}
		/* img_rxfe */
		/* cfg_imgrxfe_decim */
		writel_relaxed(3, base + RXFE_IMGRXFE_C4);

		/* cfg_imgrxfe_pm_threshold1 */
		writel_relaxed(30, base + PM_THRESHOLD1);
		/* cfg_imgrxfe_pm_threshold2 */
		writel_relaxed(40, base + PM_THRESHOLD2);

		/* cfg_imgrxfe_fir1_coeff0-5 */
		writel_relaxed(14, base + RXFE_IMGRXFE_C10_COEFF(0));
		writel_relaxed(-52, base + RXFE_IMGRXFE_C10_COEFF(1));
		writel_relaxed(140, base + RXFE_IMGRXFE_C10_COEFF(2));
		writel_relaxed(-346, base + RXFE_IMGRXFE_C10_COEFF(3));
		writel_relaxed(1250, base + RXFE_IMGRXFE_C10_COEFF(4));
		writel_relaxed(2009, base + RXFE_IMGRXFE_C10_COEFF(5));

		/* cfg_imgrxfe_fir1_rnd_bits */
		writel_relaxed(1, base + RXFE_IMGRXFE_C11);

		/* cfg_imgrxfe_fir2_coeff0-8 */
		writel_relaxed(-3, base + RXFE_IMGRXFE_C12_COEFF(0));
		writel_relaxed(10, base + RXFE_IMGRXFE_C12_COEFF(1));
		writel_relaxed(-25, base + RXFE_IMGRXFE_C12_COEFF(2));
		writel_relaxed(53, base + RXFE_IMGRXFE_C12_COEFF(3));
		writel_relaxed(-102, base + RXFE_IMGRXFE_C12_COEFF(4));
		writel_relaxed(190, base + RXFE_IMGRXFE_C12_COEFF(5));
		writel_relaxed(-380, base + RXFE_IMGRXFE_C12_COEFF(6));
		writel_relaxed(1248, base + RXFE_IMGRXFE_C12_COEFF(7));
		writel_relaxed(1982, base + RXFE_IMGRXFE_C12_COEFF(8));
		break;
	default:
		BUG_ON(true);
		rc = -EINVAL;
		goto end;
	}

	/* pdm */
	switch (pdm) {
	case DEMOD_WRAPPER_PDM0:
		pdm_addrs = PDM0_CONTROL;
		break;
	case DEMOD_WRAPPER_PDM1:
		pdm_addrs = PDM1_CONTROL;
		break;
	default:
		BUG_ON(true);
		rc = -EINVAL;
		goto end;
	}
	writel_relaxed(0x80000002, base + pdm_addrs);
end:
	return rc;
}

/**
 * activate_dtv_t_c_path() - Activates DTV Cable or Terrestrial path
 * in demod wrapper.
 *
 * @pdm: Indicated the pdm number to work with.
 *
 * Return 0 on success, error value otherwise.
 */
static int activate_dtv_t_c_path(enum demod_wrapper_pdm_num pdm)
{
	unsigned int base = device->base;
	unsigned int adc_reg_val = 0x00000001;
	unsigned int pdm_addrs;
	unsigned int rc = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	/* clocks muxes */
	rc = clk_set_parent(device->dem_rxfe_clk_src,
		device->bcc_adc_1_out_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for dem_rxfe_clk_src returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_dem_rxfe_if_clk_src,
		device->bcc_dem_rxfe_div4_i_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_dem_rxfe_if_clk_src returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_dem_rxfe_div3_mux_div4_i_clk,
		device->bcc_dem_rxfe_div4_i_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_dem_rxfe_div3_mux_div4_i_clk returned with error");
		goto end;
	}

	/* adc mux */
	if (device->path_control[DEMOD_WRAPPER_EXT_ATV])
		adc_reg_val = 0x00000021;
	dev_info(device->dev, "First write from %x\n", base + ADC_MUX_SEL);
	writel_relaxed(adc_reg_val, base + ADC_MUX_SEL);
	dev_info(device->dev, "After first write\n");

	/* img_rxfe */
	writel_relaxed(0, base + RXFE_IMGRXFE_C1); /* CFG_IMGRXFE_ORDER */
	/* cfg_imgrxfe_lut_entry0-7 */
	writel_relaxed(
		  (8 << CFG_RXFE_LUT_ENTRY0_SHIFT) |
		  (9 << CFG_RXFE_LUT_ENTRY1_SHIFT) |
		  (11 << CFG_RXFE_LUT_ENTRY2_SHIFT) |
		  (10 << CFG_RXFE_LUT_ENTRY3_SHIFT) |
		  (15 << CFG_RXFE_LUT_ENTRY4_SHIFT) |
		  (14 << CFG_RXFE_LUT_ENTRY5_SHIFT) |
		  (12 << CFG_RXFE_LUT_ENTRY6_SHIFT) |
		  (13UL << CFG_RXFE_LUT_ENTRY7_SHIFT),
		  base + RXFE_IMGRXFE_C2);

	writel_relaxed(/* cfg_imgrxfe_lut_entry8-15 */
		  (7 << CFG_RXFE_LUT_ENTRY8_SHIFT) |
		  (6 << CFG_RXFE_LUT_ENTRY9_SHIFT) |
		  (4 << CFG_RXFE_LUT_ENTRY10_SHIFT) |
		  (5 << CFG_RXFE_LUT_ENTRY11_SHIFT) |
		  (0 << CFG_RXFE_LUT_ENTRY12_SHIFT) |
		  (1 << CFG_RXFE_LUT_ENTRY13_SHIFT) |
		  (3 << CFG_RXFE_LUT_ENTRY14_SHIFT) |
		  (2UL << CFG_RXFE_LUT_ENTRY15_SHIFT),
		  base + RXFE_IMGRXFE_C3);
	/* cfg_imgrxfe_decim */
	writel_relaxed(3, base + RXFE_IMGRXFE_C4);

	/* cfg_imgrxfe_pm_threshold1 */
	writel_relaxed(20, base + PM_THRESHOLD1);
	/* cfg_imgrxfe_pm_threshold2 */
	writel_relaxed(33, base + PM_THRESHOLD2);

	/* cfg_imgrxfe_fir1_coeff0-5 */
	writel_relaxed(3, base + RXFE_IMGRXFE_C10_COEFF(0));
	writel_relaxed(-22, base + RXFE_IMGRXFE_C10_COEFF(1));
	writel_relaxed(89, base + RXFE_IMGRXFE_C10_COEFF(2));
	writel_relaxed(-281, base + RXFE_IMGRXFE_C10_COEFF(3));
	writel_relaxed(1142, base + RXFE_IMGRXFE_C10_COEFF(4));
	writel_relaxed(1862, base + RXFE_IMGRXFE_C10_COEFF(5));

	/* cfg_imgrxfe_fir1_rnd_bits */
	writel_relaxed(0, base + RXFE_IMGRXFE_C11);

	/* cfg_imgrxfe_fir2_coeff0-8 */
	writel_relaxed(0, base + RXFE_IMGRXFE_C12_COEFF(0));
	writel_relaxed(1, base + RXFE_IMGRXFE_C12_COEFF(1));
	writel_relaxed(-5, base + RXFE_IMGRXFE_C12_COEFF(2));
	writel_relaxed(18, base + RXFE_IMGRXFE_C12_COEFF(3));
	writel_relaxed(-53, base + RXFE_IMGRXFE_C12_COEFF(4));
	writel_relaxed(135, base + RXFE_IMGRXFE_C12_COEFF(5));
	writel_relaxed(-332, base + RXFE_IMGRXFE_C12_COEFF(6));
	writel_relaxed(1206, base + RXFE_IMGRXFE_C12_COEFF(7));
	writel_relaxed(1940, base + RXFE_IMGRXFE_C12_COEFF(8));

	/* pdm */
	switch (pdm) {
	case DEMOD_WRAPPER_PDM0:
		pdm_addrs = PDM0_CONTROL;
		break;
	case DEMOD_WRAPPER_PDM1:
		pdm_addrs = PDM1_CONTROL;
		break;
	default:
		BUG_ON(true);
		rc = -EINVAL;
		goto end;
	}
	writel_relaxed(0x80000002, base + pdm_addrs);
end:
	return rc;
}

/**
 * activate_ext_atv_path() - Activates the external ATV path in
 * demod wrapper.
 *
 * Return 0 on success, error value otherwise.
 */
static int activate_ext_atv_path(void)
{
	unsigned int base = device->base;
	unsigned int adc_reg_val = 0x00000020;
	int rc = 0;

	/* clocks muxes */
	rc = clk_set_parent(device->atv_rxfe_clk_src,
		device->bcc_adc_2_out_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for atv_rxfe_clk_src returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_atv_rxfe_x1_resamp_clk,
		device->bcc_atv_rxfe_div8_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_atv_rxfe_x1_resamp_clk returned with error");
		goto end;
	}
	rc = clk_set_parent(device->bcc_atv_rxfe_resamp_clk,
		device->bcc_atv_rxfe_resamp_clk_src);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for bcc_atv_rxfe_resamp_clk returned with error");
		goto end;
	}
	rc = clk_set_parent(device->albacore_sif_clk,
		device->bcc_atv_rxfe_x1_resamp_clk);
	if (rc) {
		dev_err(device->dev, "clk_set_parent for albacore_sif_clk returned with error");
		goto end;
	}
	dev_dbg(device->dev, "%s\n", __func__);
	/* adc mux */
	if (device->path_control[DEMOD_WRAPPER_DTV_T_C])
		adc_reg_val = 0x00000021;
	writel_relaxed(adc_reg_val, base + ADC_MUX_SEL);

	/* atv_rxfe init */
	writel_relaxed(0, base + RXFE_ATVRXFE_C1);
	writel_relaxed(
		(8 << CFG_ATVRXFE_LUT_ENTRY0_SHIFT) |
		(9 << CFG_ATVRXFE_LUT_ENTRY1_SHIFT) |
		(11 << CFG_ATVRXFE_LUT_ENTRY2_SHIFT) |
		(10 << CFG_ATVRXFE_LUT_ENTRY3_SHIFT) |
		(15 << CFG_ATVRXFE_LUT_ENTRY4_SHIFT) |
		(14 << CFG_ATVRXFE_LUT_ENTRY5_SHIFT) |
		(12 << CFG_ATVRXFE_LUT_ENTRY6_SHIFT) |
		(13UL << CFG_ATVRXFE_LUT_ENTRY7_SHIFT),
		base + RXFE_ATVRXFE_C2);

	writel_relaxed(
		(7 << CFG_ATVRXFE_LUT_ENTRY8_SHIFT) |
		(6 << CFG_ATVRXFE_LUT_ENTRY9_SHIFT) |
		(4 << CFG_ATVRXFE_LUT_ENTRY10_SHIFT) |
		(5 << CFG_ATVRXFE_LUT_ENTRY11_SHIFT) |
		(0 << CFG_ATVRXFE_LUT_ENTRY12_SHIFT) |
		(1 << CFG_ATVRXFE_LUT_ENTRY13_SHIFT) |
		(3 << CFG_ATVRXFE_LUT_ENTRY14_SHIFT) |
		(2 << CFG_ATVRXFE_LUT_ENTRY15_SHIFT),
		base + RXFE_ATVRXFE_C3);

	writel_relaxed(7, base + PM_LOOP_CNTR);
	writel_relaxed(100, base + PM_PARAMS_THRESHOLD);

	writel_relaxed(CFG_ATVRXFE_PRE_FILTER_OUT_SEL, base + RXFE_ATVRXFE_C4);
	writel_relaxed(0, base + RXFE_ATVRXFE_C5);
	writel_relaxed(603979776 & CFG_ATVRXFE_RESAMPLER_T2_TO_T1M1_MASK,
	base + RXFE_ATVRXFE_C6);

	writel_relaxed(
		(82 & CFG_ATVRXFE_RESAMPLER_T1_TO_T2_MASK) |
		((20 << CFG_ATVRXFE_RESAMPLER_OUT_GAIN_SHIFT) &
		CFG_ATVRXFE_RESAMPLER_OUT_GAIN_MASK) |
		((2 << CFG_ATVRXFE_RESAMPLER_OUT_RND_SHIFT) &
		CFG_ATVRXFE_RESAMPLER_OUT_RND_MASK) |
		((3 << CFG_ATVRXFE_DECIM_SHIFT) & CFG_ATVRXFE_DECIM_MASK),
		base + RXFE_ATVRXFE_C7);

	writel_relaxed(CFG_ATVRXFE_FIR3_RND_BITS, base + RXFE_ATVRXFE_C8);

	writel_relaxed(-6, base + RXFE_ATVRXFE_C9_COEFF(0));
	writel_relaxed(52, base + RXFE_ATVRXFE_C9_COEFF(1));
	writel_relaxed(-244, base + RXFE_ATVRXFE_C9_COEFF(2));
	writel_relaxed(1186, base + RXFE_ATVRXFE_C9_COEFF(3));
	writel_relaxed(1976, base + RXFE_ATVRXFE_C9_COEFF(4));

	writel_relaxed(3, base + RXFE_ATVRXFE_C10_COEFF(0));
	writel_relaxed(-22, base + RXFE_ATVRXFE_C10_COEFF(1));
	writel_relaxed(92, base + RXFE_ATVRXFE_C10_COEFF(2));
	writel_relaxed(-299, base + RXFE_ATVRXFE_C10_COEFF(3));
	writel_relaxed(1234, base + RXFE_ATVRXFE_C10_COEFF(4));
	writel_relaxed(2016, base + RXFE_ATVRXFE_C10_COEFF(5));

	writel_relaxed(-2, base + RXFE_ATVRXFE_C11_COEFF(0));
	writel_relaxed(10, base + RXFE_ATVRXFE_C11_COEFF(1));
	writel_relaxed(-28, base + RXFE_ATVRXFE_C11_COEFF(2));
	writel_relaxed(66, base + RXFE_ATVRXFE_C11_COEFF(3));
	writel_relaxed(-135, base + RXFE_ATVRXFE_C11_COEFF(4));
	writel_relaxed(253, base + RXFE_ATVRXFE_C11_COEFF(5));
	writel_relaxed(-451, base + RXFE_ATVRXFE_C11_COEFF(6));
	writel_relaxed(800, base + RXFE_ATVRXFE_C11_COEFF(7));
	writel_relaxed(-1556, base + RXFE_ATVRXFE_C11_COEFF(8));
	writel_relaxed(5036, base + RXFE_ATVRXFE_C11_COEFF(9));
	writel_relaxed(7986, base + RXFE_ATVRXFE_C11_COEFF(10));

	writel_relaxed(0, base + RXFE_ATVRXFE_C12);

	writel_relaxed(
	((0x001F << CFG_ATVRXFE_FIR_EQ_VAL_A_SHIFT) &
	CFG_ATVRXFE_FIR_EQ_VAL_A_MASK) |
	((0x1DE8 << CFG_ATVRXFE_FIR_EQ_VAL_B_SHIFT) &
	CFG_ATVRXFE_FIR_EQ_VAL_B_MASK),
	base + RXFE_ATVRXFE_C13);

	writel_relaxed(0x09F9 & CFG_ATVRXFE_FIR_EQ_VAL_C_MASK, base +
			RXFE_ATVRXFE_C13A);

	writel_relaxed(
	CFG_ATVRXFE_IIR_EQ_BYPASS |
	((0x0B3 << CFG_ATVRXFE_IIR_EQ_VAL_A_SHIFT) &
	CFG_ATVRXFE_IIR_EQ_VAL_A_MASK) |
	((0x180 << CFG_ATVRXFE_IIR_EQ_VAL_B_SHIFT) &
	CFG_ATVRXFE_IIR_EQ_VAL_B_MASK),
	base + RXFE_ATVRXFE_C14);
	/* sif mux */
	writel_relaxed(0x00002000, base + RXFE_IMGSIF_C3);
end:
	return rc;
}

/**
 * release_path_data() - Release a path logically in data structs.
 *
 * @type: the path type to release.
 */
static void release_path_data(enum demod_wrapper_path_type type)
{
	device->path_control[type] = false;
	device->num_of_activated_paths--;
	device->fd_control[type] = NULL;
}

/**
 * release_path() - releases a path in demod wrapper
 *
 * @type:	The type of the path to release.
 * @file:	The file descriptor that requested the path release.
 * @fd_check: A flag indicates whether to preform a file descriptor check.
 *
 * Return 0 on success, error value otherwise.
 */
static int release_path(enum demod_wrapper_path_type type,
						struct file *file,
						bool fd_check)
{
	int res = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	if (device->path_control[type] == false) {
		dev_err(device->dev, "path is not set - nothing to release\n");
		goto end;
	}
	if (fd_check && (file != device->fd_control[type])) {
		dev_err(device->dev, "release_path request was invoked by diffrent source than the set_path\n");
		res = -EPERM;
		goto end;
	}
	release_path_data(type);
end:
	return res;
}

/**
 * set_path() - Sets path of the requested type to be active in
 * demod wrapper.
 *
 * @type: The type of the path to set.
 * @pdm: The pdm number to be used with this path.
 * @power: The power mode.
 * @file: the file descriptor that originated the request.
 *
 * Return 0 on success, error value otherwise.
 */
static int set_path(enum demod_wrapper_path_type type,
	enum demod_wrapper_pdm_num pdm, enum demod_wrapper_power_mode power,
	struct file *file)
{
	int res = 0;

	bool set_control_data = false;
	dev_info(device->dev, "%s\n", __func__);
	if (device->path_control[type] == true)
		goto end;
	switch (type) {
	case DEMOD_WRAPPER_FORZA_ATV:
		if (device->num_of_activated_paths != 0) {
			dev_dbg(device->dev, "the activation of forza_atv path is overriding\n");
			if (device->path_control[DEMOD_WRAPPER_DTV_S])
				release_path_data(DEMOD_WRAPPER_DTV_S);
			if (device->path_control[DEMOD_WRAPPER_DTV_T_C])
				release_path_data(DEMOD_WRAPPER_DTV_T_C);
			if (device->path_control[DEMOD_WRAPPER_EXT_ATV])
				release_path_data(DEMOD_WRAPPER_EXT_ATV);
		}
		set_control_data = true;
		res = activate_forza_atv_path(pdm);
		if (res) {
			dev_err(device->dev, "activating forza_atv_path failed\n");
			goto end;
		}
		dev_dbg(device->dev, "forza_atv path is activated\n");
		break;

	case DEMOD_WRAPPER_DTV_S:
		dev_err(device->dev, "dtv_s request should be preformed using set_path_img_sat\n");
		res = -EINVAL;
		break;

	case DEMOD_WRAPPER_DTV_T_C:
		if ((device->num_of_activated_paths > 0) &&
			((device->path_control[DEMOD_WRAPPER_DTV_S] == true) ||
			(device->
			path_control[DEMOD_WRAPPER_FORZA_ATV] == true))) {
			dev_dbg(device->dev, "the activation of dtv_t_c path is overriding\n");
			if (device->path_control[DEMOD_WRAPPER_DTV_S])
				release_path_data(DEMOD_WRAPPER_DTV_S);
			if (device->path_control[DEMOD_WRAPPER_FORZA_ATV])
				release_path_data(DEMOD_WRAPPER_FORZA_ATV);
		}
		set_control_data = true;
		res = activate_dtv_t_c_path(pdm);
		if (res) {
			dev_err(device->dev, "activating dtv_t_c_path failed\n");
			goto end;
		}
		dev_dbg(device->dev, "dtv_t_c path is activated\n");
		break;

	case DEMOD_WRAPPER_EXT_ATV:
		if ((device->num_of_activated_paths > 0) &&
			(device->
			path_control[DEMOD_WRAPPER_FORZA_ATV] == true)) {
			dev_dbg(device->dev, "the activation of ext_atv path is overriding\n");
			if (device->path_control[DEMOD_WRAPPER_FORZA_ATV])
				release_path_data(DEMOD_WRAPPER_FORZA_ATV);
		}
		set_control_data = true;
		res = activate_ext_atv_path();
		if (res) {
			dev_err(device->dev, "activating ext_atv_path failed\n");
			goto end;
		}
		dev_dbg(device->dev, "ext_atv path is activated\n");
		break;

	default:
		BUG_ON(true);
		break;
	}
	if (set_control_data) {
		device->path_control[type] = true;
		device->num_of_activated_paths++;
		device->fd_control[type] = file;
	}

end:
	return res;
}

/**
 * set_path_dtv_sat() - Sets the path in the demod wrapper to be
 * DTV Sattalite.
 *
 * @pdm: The pdm to be used with this path type.
 * @power: The power mode.
 * @baude: The baud rate mode.
 *
 * Return 0 on success, error value otherwise.
 */
static int set_path_dtv_sat(enum demod_wrapper_pdm_num pdm,
	enum demod_wrapper_power_mode  power,
	enum demod_wrapper_baud_rate_mode baude,
	struct file *file)
{
	int res = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	if (device->path_control[DEMOD_WRAPPER_DTV_S] == true)
		goto end;
	if ((device->num_of_activated_paths > 0) &&
		((device->path_control[DEMOD_WRAPPER_DTV_T_C] == true) ||
		(device->path_control[DEMOD_WRAPPER_FORZA_ATV] == true)))
		dev_dbg(device->dev, "the activation of dtv_s path is overriding\n");
		device->path_control[DEMOD_WRAPPER_DTV_S] = true;
		device->num_of_activated_paths++;
		device->fd_control[DEMOD_WRAPPER_DTV_S] =  file;
		res = activate_dtv_s_path(baude, pdm);
		if (res) {
			dev_err(device->dev, "activating dtv_sat_path failed\n");
			goto end;
		}
		dev_dbg(device->dev, "dtv_s path is activated\n");
end:
	return res;
}

/**
 * init_ts_bridge() - Initializes the ts bridge.
 *
 * @out_config:	Configures the output of the ts-bridge.
 *
 * Return 0 on success, error value otherwise.
 */
static int init_ts_bridge(enum demod_wrapper_ts_bridge out_config)
{
	int res = 0;
	unsigned int base = device->base;
	dev_dbg(device->dev, "%s\n", __func__);
	if (out_config == DEMOD_WRAPPER_TS_PARALLEL) {
		writel_relaxed(0x00000011, base + BCDEM_REGS_TS_CFG);
		device->ts_bridge_init = true;
	} else if (out_config == DEMOD_WRAPPER_TS_SERIAL) {
		writel_relaxed(0x00000001, base + BCDEM_REGS_TS_CFG);
		device->ts_bridge_init = true;
	} else {
		res = -EINVAL;
	}
	return res;
}

/**
 * enable_ts_bridge() - Enables the ts-bridge.
 *
 */
static void enable_ts_bridge(void)
{
	dev_dbg(device->dev, "%s\n", __func__);
	/* clock set */
	if (device->bcc_ts_out_clk) {
		if (clk_prepare_enable(device->bcc_ts_out_clk) != 0)
			pr_err("%s: Can't start bcc_ts_out_clk\n", __func__);
	}
	if (!device->ts_bridge_init)
		/* setting default value to in parallel- out parallel */
		init_ts_bridge(DEMOD_WRAPPER_TS_PARALLEL);
	/* TODO PinCntrl */
}

/**
 * disable_ts_bridge() - Disables the ts-bridge.
 *
 */
static void disable_ts_bridge(void)
{
	dev_info(device->dev, "%s\n", __func__);
	/* clock down */
	if (device->bcc_ts_out_clk)
		clk_disable_unprepare(device->bcc_ts_out_clk);
}

/**
 * pm_set_params() - Set loop_counter and threshold params to power meter.
 *
 * @loop_cntr: the power meter parameter loop_cntr that will be set to
 * PM_LOOP_CNTR register.
 * @threshold: the power meter parameter threshold that will be set to
 * PM_PARAMS_THRESHOLD register.
 *
 * Return 0 on success, error value otherwise.
 */
static int pm_set_params(unsigned int loop_cntr, unsigned int threshold)
{
	int polling_counter = MAX_POLLING;
	int res = -EFAULT;
	unsigned int ro_ack_reg;
	unsigned int base = device->base;

	dev_dbg(device->dev, "%s\n", __func__);
	writel_relaxed(loop_cntr, base + PM_LOOP_CNTR);
	writel_relaxed(threshold, base + PM_PARAMS_THRESHOLD);
	writel_relaxed(0x00000001, base + PM_PARAMS_VLD_ACK);
	writel_relaxed(0x00000000, base + PM_PARAMS_VLD_ACK);
	while (polling_counter != 0) {
		ro_ack_reg = readl_relaxed(base + PM_RO_PARAMS_ACK);
		if ((ro_ack_reg & 0x80000000) == 0x80000000) {
			/*TODO consider make use 32 bit spesific write */
			writel_relaxed(0x80000000, base + PM_PARAMS_VLD_ACK);
			writel_relaxed(0x00000000, base + PM_PARAMS_VLD_ACK);
			res = 0;
			break;
		}

		/* Not the first iteration of polling */
		if (polling_counter != MAX_POLLING)
			udelay(10);
		polling_counter--;
	}
	return res;
}

/**
 * pm_get_thrshld_cntr() - Gets threshold counter param from power meter.
 *
 * @thrshld_cntr: the value of power meter's threshold counter. Is read from
 * PM_RO_THRSHLD_CNTR register.
 *
 * Return 0 on success, error value otherwise.
 */
static int pm_get_thrshld_cntr(unsigned int *thrshld_cntr)
{
	int polling_counter = MAX_POLLING;
	int res = -EFAULT;
	unsigned int s2_valid_reg;
	unsigned int base = device->base;

	writel_relaxed(0x00000001, base + PM_S2_LCH_VLD);
	writel_relaxed(0x00000000, base + PM_S2_LCH_VLD);
	while (polling_counter != 0) {
		s2_valid_reg = readl_relaxed(base + PM_RO_S2_VALID);
		if ((s2_valid_reg & 0x80000000) == 0x80000000) {
			*thrshld_cntr = readl_relaxed(
					base + PM_RO_THRSHLD_CNTR);
			writel_relaxed(0x80000000, base + PM_S2_LCH_VLD);
			writel_relaxed(0x00000000, base + PM_S2_LCH_VLD);
			res = 0;
			break;
		}
		/* Not the first iteration of polling */
		if (polling_counter != MAX_POLLING)
			udelay(10);
		polling_counter--;
	}
	return res;
}

/**
 * pm_get_power() - Get power param from power meter.
 *
 * @power: the value of power meter's power. Is read from
 * PM_RO_POWER register.
 *
 * Return 0 on success, error value otherwise.
 */
static int pm_get_power(unsigned int *power)
{
	int polling_counter = MAX_POLLING;
	int res = -EFAULT;
	unsigned int s3_valid_reg;
	unsigned int base = device->base;

	writel_relaxed(0x00000001, base + PM_S3_LCH_VLD);
	writel_relaxed(0x00000000, base + PM_S3_LCH_VLD);
	while (polling_counter != 0) {
		s3_valid_reg = readl_relaxed(base + PM_RO_S3_VALID);
		if ((s3_valid_reg & 0x80000000) == 0x80000000) {
			*power = readl_relaxed(base + PM_RO_POWER);
			writel_relaxed(0x80000000, base + PM_S3_LCH_VLD);
			writel_relaxed(0x00000000, base + PM_S3_LCH_VLD);
			res = 0;
			break;
		}
		/* Not the first iteration of polling */
		if (polling_counter != MAX_POLLING)
			udelay(10);
		polling_counter--;
	}
	return res;
}

/**
 * demod_clock_start() - Enable the required demod wrapper clocks
 *
 * Return 0 on success, error value otherwise.
 */
static int demod_wrapper_clock_start(void)
{
	int bcc_atv_rxfe_clk = 0;
	int bcc_dem_rxfe_i_clk = 0;
	int bcc_atv_rxfe_resamp_clk_src = 0;
	int bcc_forza_sync_x5_clk = 0;
	int atv_x5_clk = 0;

	if (device == NULL) {
		pr_err("%s: Can't start clocks, invalid device\n", __func__);
		return -EINVAL;
	}

	if (device->bcc_atv_rxfe_clk) {
		if (clk_prepare_enable(device->bcc_atv_rxfe_clk) != 0) {
			pr_err("%s: Can't start bcc_atv_rxfe_clk\n", __func__);
			goto err_clocks;
		}
		bcc_atv_rxfe_clk = 1;
	}

	if (device->bcc_dem_rxfe_i_clk) {
		if (clk_prepare_enable(device->bcc_dem_rxfe_i_clk) != 0) {
			pr_err("%s: Can't start cc_dem_rxfe_i_clk\n", __func__);
			goto err_clocks;
		}
		bcc_dem_rxfe_i_clk = 1;
	}

	if (device->bcc_atv_rxfe_resamp_clk_src) {
		if (clk_prepare_enable(
		device->bcc_atv_rxfe_resamp_clk_src) != 0) {
			pr_err("%s: Can't start bcc_atv_rxfe_resamp_clk_src\n",
				__func__);
			goto err_clocks;
		}
		bcc_atv_rxfe_resamp_clk_src = 1;
	}

	if (device->bcc_forza_sync_x5_clk) {
		if (clk_prepare_enable(device->bcc_forza_sync_x5_clk) != 0) {
			pr_err("%s: Can't start bcc_forza_sync_x5_clk\n",
				__func__);
			goto err_clocks;
		}
		bcc_forza_sync_x5_clk = 1;
	}

	if (device->atv_x5_clk) {
		if (clk_prepare_enable(device->atv_x5_clk) != 0) {
			pr_err("%s: Can't start atv_x5_clk\n",
				__func__);
			goto err_clocks;
		}
		atv_x5_clk = 1;
	}
	return 0;

err_clocks:

	if (bcc_atv_rxfe_clk)
		clk_disable_unprepare(device->bcc_atv_rxfe_clk);

	if (bcc_dem_rxfe_i_clk)
		clk_disable_unprepare(device->bcc_dem_rxfe_i_clk);

	if (bcc_atv_rxfe_resamp_clk_src)
		clk_disable_unprepare(device->bcc_atv_rxfe_resamp_clk_src);

	if (bcc_forza_sync_x5_clk)
		clk_disable_unprepare(device->bcc_forza_sync_x5_clk);

	if (atv_x5_clk)
		clk_disable_unprepare(device->atv_x5_clk);

	return -EBUSY;
}

/**
 * demod_wrapper_clock_stop() - Disable demod wrapper clocks
 *
 */
static void demod_wrapper_clock_stop(void)
{
	if (device == NULL) {
		pr_err("%s: Can't stop clocks, invalid device\n", __func__);
		return;
	}

	if (device->bcc_adc_0_in_clk)
		clk_disable_unprepare(device->bcc_adc_0_in_clk);

	if (device->bcc_adc_1_in_clk)
		clk_disable_unprepare(device->bcc_adc_1_in_clk);

	if (device->bcc_adc_2_in_clk)
		clk_disable_unprepare(device->bcc_adc_2_in_clk);

	if (device->bcc_atv_rxfe_clk)
		clk_disable_unprepare(device->bcc_atv_rxfe_clk);

	if (device->bcc_dem_rxfe_i_clk)
		clk_disable_unprepare(device->bcc_dem_rxfe_i_clk);

	if (device->bcc_ts_out_clk)
		clk_disable_unprepare(device->bcc_ts_out_clk);

	if (device->bcc_atv_rxfe_resamp_clk_src)
		clk_disable_unprepare(device->bcc_atv_rxfe_resamp_clk_src);

	if (device->bcc_forza_sync_x5_clk)
		clk_disable_unprepare(device->bcc_forza_sync_x5_clk);

	if (device->atv_x5_clk)
		clk_disable_unprepare(device->atv_x5_clk);

	if (device->bcc_dem_ahb_clk)
		clk_disable_unprepare(device->bcc_dem_ahb_clk);
}

static void config_adcs(void)
{
	unsigned int base = device->base;
	dev_dbg(device->dev, "%s\n", __func__);
	writel_relaxed(0x98804100, base + ADC_BBR0_CONFIG); /* SAT 4 */
	writel_relaxed(0x80804c00, base + ADC_BBR1_CONFIG); /* non SAT 1*/
	writel_relaxed(0x80804c00, base + ADC_BBR2_CONFIG); /* non SAT 1*/
	writel_relaxed(0x00000001, base + ADC_BBR0_MISC);
	writel_relaxed(0x00000001, base + ADC_BBR1_MISC);
	writel_relaxed(0x00000001, base + ADC_BBR2_MISC);
	udelay(10); /* adc requires a delay*/
	writel_relaxed(0x00000011, base + ADC_BBR0_MISC);
	writel_relaxed(0x00000011, base + ADC_BBR1_MISC);
	writel_relaxed(0x00000011, base + ADC_BBR2_MISC);
}

static void adcs_clock_stop(void)
{
	if (device->bcc_adc_0_in_clk)
		clk_disable_unprepare(device->bcc_adc_0_in_clk);

	if (device->bcc_adc_1_in_clk)
		clk_disable_unprepare(device->bcc_adc_1_in_clk);

	if (device->bcc_adc_2_in_clk)
		clk_disable_unprepare(device->bcc_adc_2_in_clk);
}

static int adcs_clock_start(void)
{
	int bcc_adc_0_in_clk = 0;
	int bcc_adc_1_in_clk = 0;
	int bcc_adc_2_in_clk = 0;

	if (device == NULL) {
		pr_err("%s: Can't start clocks, invalid device\n", __func__);
		return -EINVAL;
	}

	if (device->bcc_adc_0_in_clk) {
		if (clk_prepare_enable(device->bcc_adc_0_in_clk) != 0) {
			pr_err("%s: Can't start bcc_adc_0_in_clk\n", __func__);
			goto err_clocks;
		}
		bcc_adc_0_in_clk = 1;
	}

	if (device->bcc_adc_1_in_clk) {
		if (clk_prepare_enable(device->bcc_adc_1_in_clk) != 0) {
			pr_err("%s: Can't start bcc_adc_1_in_clk\n", __func__);
			goto err_clocks;
		}
		bcc_adc_1_in_clk = 1;
	}

	if (device->bcc_adc_2_in_clk) {
		if (clk_prepare_enable(device->bcc_adc_2_in_clk) != 0) {
			pr_err("%s: Can't start bcc_adc_2_in_clk\n", __func__);
			goto err_clocks;
		}
		bcc_adc_2_in_clk = 1;
	}


	return 0;

err_clocks:
	if (bcc_adc_0_in_clk)
		clk_disable_unprepare(device->bcc_adc_0_in_clk);

	if (bcc_adc_1_in_clk)
		clk_disable_unprepare(device->bcc_adc_1_in_clk);

	if (bcc_adc_2_in_clk)
		clk_disable_unprepare(device->bcc_adc_2_in_clk);

	return -EBUSY;
}

static int start_dem_ahb_clk(void)
{
	int bcc_dem_ahb_clk = 0;

	if (device == NULL) {
		pr_err("%s: Can't start clocks, invalid device\n", __func__);
		return -EINVAL;
	}

	if (device->bcc_dem_ahb_clk) {
		if (clk_prepare_enable(device->bcc_dem_ahb_clk) != 0) {
			pr_err("%s: Can't start bcc_dem_ahb_clk\n", __func__);
			goto err_clocks;
		}
		bcc_dem_ahb_clk = 1;
	}

	return 0;

err_clocks:
	if (bcc_dem_ahb_clk)
		clk_disable_unprepare(device->bcc_dem_ahb_clk);

	return -EBUSY;
}

static void stop_dem_ahb_clk(void)
{
	if (device->bcc_dem_ahb_clk)
		clk_disable_unprepare(device->bcc_dem_ahb_clk);
}

/* Demod wrapper open */
static int demod_wrapper_open(struct inode *i, struct file *f)
{
	int rc = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	rc = mutex_lock_interruptible(&(device->fd_mutex));
	if (rc) {
		dev_err(device->dev, "mutex_lock_interruptible returned with error\n");
		goto end;
	}
	/* Operations that should be done upon first open */
	if (device->num_of_fds == 0) {
		/* Enable l27 regulator */
		dev_info(device->dev, "enabling l27 regulator\n");
		rc = regulator_enable(device->reg_l27);
		if (rc) {
			dev_info(device->dev, "regulator l27 enable failed\n");
			goto mutex_unlock;
		}

		/* Enable power regulator */
		rc = regulator_enable(device->gdsc);
		if (rc) {
			dev_info(device->dev, "regulator gdsc enable failed\n");
			goto l27_regulator_disbale;
		}

		/* enable dem_ahb_clock */
		dev_info(device->dev, "starting dem_ahb_clk\n");
		rc = start_dem_ahb_clk();
		if (rc) {
			dev_info(device->dev, "starting bcc_dem_ahb_clk failed\n");
			goto rd;
		}

		dev_info(device->dev, "configuring the adcs\n");
		config_adcs();
		/* set rate on adc */
		dev_info(device->dev, "starting adc clock\n");
		rc = adcs_clock_start();
		if (rc) {
			dev_info(device->dev, "starting adcs_clock failed\n");
			goto stop_dem_ahb;
		}

		/* Start HW clocks before accessing registers */
		rc = demod_wrapper_clock_start();
		if (rc) {
			dev_info(device->dev, "starting demod_wrapper_clocks_failed");
			goto stop_adcs;
		}

		/* Wakeup */
		__pm_stay_awake(&device->wakeup_src);
	}
	device->num_of_fds++;
	goto mutex_unlock;

stop_adcs:
	adcs_clock_stop();
stop_dem_ahb:
	stop_dem_ahb_clk();
rd:
	regulator_disable(device->gdsc);
l27_regulator_disbale:
	regulator_disable(device->reg_l27);
mutex_unlock:
	mutex_unlock(&(device->fd_mutex));

end:
	return rc;
}

/* Demod wrapper close */
static int demod_wrapper_close(struct inode *i, struct file *f)
{
	int j;
	int res = 0;

	dev_dbg(device->dev, "%s\n", __func__);
	mutex_lock(&(device->fd_mutex));
	if (device->num_of_fds == 1) {
		/* release all paths */
		for (j = 0; j < DEMOD_WRAPPER_NUM_OF_PATHS; j++) {
			if (device->path_control[j]) {
				if (release_path(
					(enum demod_wrapper_path_type)j,
					NULL, false) != 0) {
					res = -1;
					dev_err(device->dev, "release_path returned with error\n");
				}
				dev_dbg(device->dev, "path was released");
			}
		}

		/* stop clocks includes adc clocks stop and dem_ahb clock stop*/
		demod_wrapper_clock_stop();

		/* stop regulator */
		if (regulator_disable(device->gdsc))
			dev_err(device->dev,
			"%s: Error disabling power regulator\n", __func__);

		/* stop l27 regulator*/
		if (regulator_disable(device->reg_l27))
			dev_err(device->dev,
			"%s: Error disabling l27 regulator\n", __func__);
	}

	device->num_of_fds--;
	mutex_unlock(&(device->fd_mutex));
	return res;
}

/* Demod wrapper ioctl */
static long demod_wrapper_ioctl(struct file *file, unsigned cmd,
	unsigned long arg)
{
	int res = 0;
	struct demod_wrapper_set_path_args sp_args;
	struct demod_wrapper_set_path_dtv_sat_args sp_sat_args;
	struct demod_wrapper_release_path_args r_args;
	struct demod_wrapper_init_ts_bridge_args ts_args;
	struct demod_wrapper_pm_set_params_args pm_set_args;
	struct demod_wrapper_pm_get_thr_cntr_args pm_cntr_args;
	struct demod_wrapper_pm_get_power_args pm_power_args;

	dev_dbg(device->dev, "%s\n", __func__);
	res = mutex_lock_interruptible(&(device->fd_mutex));
	if (res != 0) {
		dev_err(device->dev, "mutex_lock fd_mutex returned with error\n");
		goto end;
	}
	switch (cmd) {
	case DEMOD_WRAPPER_SET_PATH:
		if (copy_from_user(&sp_args,
			(struct demod_wrapper_set_path_args *)arg,
			sizeof(struct demod_wrapper_set_path_args))) {
			dev_err(device->dev, "copying set_path arguments from user failed\n");
			res = -EACCES;
			break;
		}
		res = set_path(sp_args.type, sp_args.pdm,
				sp_args.power, file);
		break;
	case DEMOD_WRAPPER_SET_PATH_DTV_SAT:
		if (copy_from_user(&sp_sat_args,
			(struct demod_wrapper_set_path_dtv_sat_args *)arg,
			sizeof(struct demod_wrapper_set_path_dtv_sat_args))) {
			dev_err(device->dev, "copying set_path_dtv_sat arguments from user failed\n");
			res = -EACCES;
			break;
		}
		res = set_path_dtv_sat(sp_sat_args.pdm,
			sp_sat_args.power,
			sp_sat_args.br_mode, file);
		break;
	case DEMOD_WRAPPER_RELEASE_PATH:
		if (copy_from_user(&r_args,
			(struct demod_wrapper_release_path_args *)arg,
			sizeof(struct demod_wrapper_release_path_args))) {
			dev_err(device->dev, "copying release_path arguments from user failed\n");
			res = -EACCES;
			break;
		}
		res = release_path(r_args.type, file, true);
		break;
	case DEMOD_WRAPPER_TS_BRIDGE_INIT:
		if (copy_from_user(&ts_args,
			(struct demod_wrapper_init_ts_bridge_args *)arg,
			sizeof(struct demod_wrapper_init_ts_bridge_args))) {
			dev_err(device->dev, "copying ts_bridge_init arguments from user failed\n");
			res = -EACCES;
			break;
		}
		res = init_ts_bridge(ts_args.out);
		break;
	case DEMOD_WRAPPER_TS_BRIDGE_ENABLE:
		enable_ts_bridge();
		break;
	case DEMOD_WRAPPER_TS_BRIDGE_DISABLE:
		disable_ts_bridge();
		break;
	case DEMOD_WRAPPER_PM_SET_PARAMS:
		if (copy_from_user(&pm_set_args,
			(struct demod_wrapper_pm_set_params_args *) arg,
			sizeof(struct demod_wrapper_pm_set_params_args))) {
			dev_err(device->dev, "copying pm_set_params arguments from user failed\n");
			res = -EACCES;
			break;
		}
		pm_set_params(pm_set_args.pm_loop_cntr,
				pm_set_args.pm_params_threshold);
		break;
	case DEMOD_WRAPPER_PM_GET_THRSHLD_CNTR:
		res = pm_get_thrshld_cntr(&pm_cntr_args.pm_thrshld_cntr);
		if (res == 0) {
			if (copy_to_user(
				(struct demod_wrapper_pm_get_thrs_cntr_args *)
				arg,
				&pm_cntr_args,
				sizeof(
				struct
				demod_wrapper_pm_get_thr_cntr_args))) {
				dev_err(device->dev,
				"copying pm_get_thrshld_cntr to user failed\n");
				res = -EACCES;
				break;
			}
		}
		break;
	case DEMOD_WRAPPER_PM_GET_POWER:
		res = pm_get_power(&pm_power_args.pm_power);
		if (res == 0) {
			if (copy_to_user(
				(struct demod_wrapper_pm_get_power_args *) arg,
				&pm_power_args,
				sizeof(struct
				demod_wrapper_pm_get_power_args))) {
				dev_err(device->dev,
				"copying pm_get_power args to user failed\n");
				res = -EACCES;
				break;
			}
		}
		break;

	default:
		BUG_ON(true);
		res = -ENOTTY;
	}

	mutex_unlock(&(device->fd_mutex));
end:
	return res;
}

static const struct file_operations demod_wrapper_fops = {
	.owner = THIS_MODULE,
	.open = demod_wrapper_open,
	.release = demod_wrapper_close,
	.unlocked_ioctl = demod_wrapper_ioctl
};

/**
 * demod_wrapper_clocks_put() - Put clocks and disable regulator.
 *
 */
static void demod_wrapper_clocks_put(void)
{
	pr_debug("%s", __func__);

	if (device->adc_01_clk_src)
		clk_put(device->adc_01_clk_src);

	if (device->adc_2_clk_src)
		clk_put(device->adc_2_clk_src);

	if (device->bcc_adc_0_in_clk)
		clk_put(device->bcc_adc_0_in_clk);

	if (device->bcc_adc_1_in_clk)
		clk_put(device->bcc_adc_1_in_clk);

	if (device->bcc_adc_2_in_clk)
		clk_put(device->bcc_adc_2_in_clk);

	if (device->bcc_adc_0_out_clk)
		clk_put(device->bcc_adc_0_out_clk);

	if (device->bcc_adc_1_out_clk)
		clk_put(device->bcc_adc_1_out_clk);

	if (device->bcc_adc_2_out_clk)
		clk_put(device->bcc_adc_2_out_clk);

	if (device->dem_rxfe_clk_src)
		clk_put(device->dem_rxfe_clk_src);

	if (device->atv_rxfe_clk_src)
		clk_put(device->atv_rxfe_clk_src);

	if (device->bcc_dem_rxfe_i_clk)
		clk_put(device->bcc_dem_rxfe_i_clk);

	if (device->bcc_dem_rxfe_div3_mux_div4_i_clk)
		clk_put(device->bcc_dem_rxfe_div3_mux_div4_i_clk);

	if (device->bcc_dem_rxfe_if_clk_src)
		clk_put(device->bcc_dem_rxfe_if_clk_src);

	if (device->bcc_dem_rxfe_div3_i_clk)
		clk_put(device->bcc_dem_rxfe_div3_i_clk);

	if (device->bcc_dem_rxfe_div4_i_clk)
		clk_put(device->bcc_dem_rxfe_div4_i_clk);

	if (device->bcc_ts_out_clk_src)
		clk_put(device->bcc_ts_out_clk_src);

	if (device->bcc_ts_out_clk)
		clk_put(device->bcc_ts_out_clk);

	if (device->bcc_atv_rxfe_clk)
		clk_put(device->bcc_atv_rxfe_clk);

	if (device->bcc_atv_rxfe_resamp_clk_src)
		clk_put(device->bcc_atv_rxfe_resamp_clk_src);

	if (device->bcc_atv_rxfe_resamp_clk)
		clk_put(device->bcc_atv_rxfe_resamp_clk);

	if (device->bcc_atv_rxfe_x1_resamp_clk)
		clk_put(device->bcc_atv_rxfe_x1_resamp_clk);

	if (device->bcc_atv_rxfe_div8_clk)
		clk_put(device->bcc_atv_rxfe_div8_clk);

	if (device->albacore_sif_clk)
		clk_put(device->albacore_sif_clk);

	if (device->bcc_atv_x1_clk)
		clk_put(device->bcc_atv_x1_clk);

	if (device->bcc_forza_sync_x5_clk)
		clk_put(device->bcc_forza_sync_x5_clk);

	if (device->atv_x5_clk)
		clk_put(device->atv_x5_clk);

	device->adc_01_clk_src = NULL;
	device->adc_2_clk_src = NULL;
	device->bcc_adc_0_in_clk = NULL;
	device->bcc_adc_1_in_clk = NULL;
	device->bcc_adc_2_in_clk = NULL;
	device->bcc_adc_0_out_clk = NULL;
	device->bcc_adc_1_out_clk = NULL;
	device->bcc_adc_2_out_clk = NULL;
	device->dem_rxfe_clk_src = NULL;
	device->atv_rxfe_clk_src = NULL;
	device->bcc_dem_rxfe_i_clk = NULL;
	device->bcc_dem_rxfe_div3_mux_div4_i_clk = NULL;
	device->bcc_dem_rxfe_if_clk_src = NULL;
	device->bcc_dem_rxfe_div3_i_clk = NULL;
	device->bcc_dem_rxfe_div4_i_clk = NULL;
	device->bcc_ts_out_clk_src = NULL;
	device->bcc_ts_out_clk = NULL;
	device->bcc_atv_rxfe_clk = NULL;
	device->bcc_atv_rxfe_resamp_clk_src = NULL;
	device->bcc_atv_rxfe_resamp_clk = NULL;
	device->bcc_atv_rxfe_x1_resamp_clk = NULL;
	device->bcc_atv_rxfe_div8_clk = NULL;
	device->albacore_sif_clk = NULL;
	device->bcc_atv_x1_clk = NULL;
	device->bcc_forza_sync_x5_clk = NULL;
	device->atv_x5_clk = NULL;
}

/**
 * demod_wrapper_clocks_setup() - Get clocks and set their rate, enable regulator.
 *
 * @pdev:	Platform device, containing platform information.
 *
 * Return 0 on success, error value otherwise.
 */
static int demod_wrapper_clocks_setup(struct platform_device *pdev)
{
	int ret = 0;
	unsigned long rate_in_hz = 0;
	struct clk *atv_x5_clk_src = NULL;

	/* Get power regulator (GDSC) */
	device->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(device->gdsc)) {
		pr_err("%s: Failed to get vdd power regulator\n", __func__);
		ret = PTR_ERR(device->gdsc);
		device->gdsc = NULL;
		return ret;
	}
	/* Get l27 power regulator needed for adc */
	device->reg_l27 = devm_regulator_get(&pdev->dev, "adc");
	if (IS_ERR(device->reg_l27)) {
		pr_err("%s: Failed to get l27 regulator\n", __func__);
		ret = PTR_ERR(device->reg_l27);
		device->reg_l27 = NULL;
		return ret;
	}
	device->adc_01_clk_src = NULL;
	device->adc_2_clk_src = NULL;
	device->bcc_adc_0_in_clk = NULL;
	device->bcc_adc_1_in_clk = NULL;
	device->bcc_adc_2_in_clk = NULL;
	device->bcc_adc_0_out_clk = NULL;
	device->bcc_adc_1_out_clk = NULL;
	device->bcc_adc_2_out_clk = NULL;
	device->dem_rxfe_clk_src = NULL;
	device->atv_rxfe_clk_src = NULL;
	device->bcc_dem_rxfe_i_clk = NULL;
	device->bcc_dem_rxfe_div3_mux_div4_i_clk = NULL;
	device->bcc_dem_rxfe_if_clk_src = NULL;
	device->bcc_dem_rxfe_div3_i_clk = NULL;
	device->bcc_dem_rxfe_div4_i_clk = NULL;
	device->bcc_ts_out_clk_src = NULL;
	device->bcc_ts_out_clk = NULL;
	device->bcc_atv_rxfe_clk = NULL;
	device->bcc_atv_rxfe_resamp_clk_src = NULL;
	device->bcc_atv_rxfe_resamp_clk = NULL;
	device->bcc_atv_rxfe_x1_resamp_clk = NULL;
	device->bcc_atv_rxfe_div8_clk = NULL;
	device->albacore_sif_clk = NULL;
	device->bcc_atv_x1_clk = NULL;
	device->bcc_forza_sync_x5_clk = NULL;
	device->atv_x5_clk = NULL;

	device->adc_01_clk_src =
		clk_get(&pdev->dev, clocks_info.adc_01_clk_src);
	if (IS_ERR(device->adc_01_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.adc_01_clk_src);
		ret = PTR_ERR(device->adc_01_clk_src);
		device->adc_01_clk_src = NULL;
		goto err_clocks;
	}

	device->adc_2_clk_src =
		clk_get(&pdev->dev, clocks_info.adc_2_clk_src);
	if (IS_ERR(device->adc_2_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.adc_2_clk_src);
		ret = PTR_ERR(device->adc_2_clk_src);
		device->adc_2_clk_src = NULL;
		goto err_clocks;
	}

	device->bcc_adc_0_in_clk =
		clk_get(&pdev->dev, clocks_info.bcc_adc_0_in_clk);
	if (IS_ERR(device->bcc_adc_0_in_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_0_in_clk);
		ret = PTR_ERR(device->bcc_adc_0_in_clk);
		device->bcc_adc_0_in_clk = NULL;
		goto err_clocks;
	}

	device->bcc_adc_1_in_clk =
		clk_get(&pdev->dev, clocks_info.bcc_adc_1_in_clk);
	if (IS_ERR(device->bcc_adc_1_in_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_1_in_clk);
		ret = PTR_ERR(device->bcc_adc_1_in_clk);
		device->bcc_adc_1_in_clk = NULL;
		goto err_clocks;
	}

	device->bcc_adc_2_in_clk = clk_get(&pdev->dev,
		clocks_info.bcc_adc_2_in_clk);
	if (IS_ERR(device->bcc_adc_2_in_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_2_in_clk);
		ret = PTR_ERR(device->bcc_adc_2_in_clk);
		device->bcc_adc_2_in_clk = NULL;
		goto err_clocks;
	}

	device->bcc_adc_0_out_clk = clk_get(&pdev->dev,
		clocks_info.bcc_adc_0_out_clk);
	if (IS_ERR(device->bcc_adc_0_out_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_0_out_clk);
		ret = PTR_ERR(device->bcc_adc_0_out_clk);
		device->bcc_adc_0_out_clk = NULL;
		goto err_clocks;
	}

	device->bcc_adc_1_out_clk = clk_get(&pdev->dev,
		clocks_info.bcc_adc_1_out_clk);
	if (IS_ERR(device->bcc_adc_1_out_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_1_out_clk);
		ret = PTR_ERR(device->bcc_adc_1_out_clk);
		device->bcc_adc_1_out_clk = NULL;
		goto err_clocks;
	}

	device->bcc_adc_2_out_clk = clk_get(&pdev->dev,
		clocks_info.bcc_adc_2_out_clk);
	if (IS_ERR(device->bcc_adc_2_out_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_adc_2_out_clk);
		ret = PTR_ERR(device->bcc_adc_2_out_clk);
		device->bcc_adc_2_out_clk = NULL;
		goto err_clocks;
	}

	device->dem_rxfe_clk_src = clk_get(&pdev->dev,
		clocks_info.dem_rxfe_clk_src);
	if (IS_ERR(device->dem_rxfe_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.dem_rxfe_clk_src);
		ret = PTR_ERR(device->dem_rxfe_clk_src);
		device->dem_rxfe_clk_src = NULL;
		goto err_clocks;
	}

	device->atv_rxfe_clk_src = clk_get(&pdev->dev,
		clocks_info.atv_rxfe_clk_src);
	if (IS_ERR(device->atv_rxfe_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.atv_rxfe_clk_src);
		ret = PTR_ERR(device->atv_rxfe_clk_src);
		device->atv_rxfe_clk_src = NULL;
		goto err_clocks;
	}

	device->bcc_dem_rxfe_i_clk = clk_get(&pdev->dev,
		clocks_info.bcc_dem_rxfe_i_clk);
	if (IS_ERR(device->bcc_dem_rxfe_i_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_dem_rxfe_i_clk);
		ret = PTR_ERR(device->bcc_dem_rxfe_i_clk);
		device->bcc_dem_rxfe_i_clk = NULL;
		goto err_clocks;
	}

	device->bcc_dem_rxfe_div3_mux_div4_i_clk = clk_get(&pdev->dev,
		clocks_info.bcc_dem_rxfe_div3_mux_div4_i_clk);
	if (IS_ERR(device->bcc_dem_rxfe_div3_mux_div4_i_clk)) {
		pr_err("%s: Failed to get %s", __func__,
			clocks_info.bcc_dem_rxfe_div3_mux_div4_i_clk);
		ret = PTR_ERR(device->bcc_dem_rxfe_div3_mux_div4_i_clk);
		device->bcc_dem_rxfe_div3_mux_div4_i_clk = NULL;
		goto err_clocks;
	}

	device->bcc_dem_rxfe_if_clk_src = clk_get(&pdev->dev,
		clocks_info.bcc_dem_rxfe_if_clk_src);
	if (IS_ERR(device->bcc_dem_rxfe_if_clk_src)) {
		pr_err("%s: Failed to get %s", __func__,
			clocks_info.bcc_dem_rxfe_if_clk_src);
		ret = PTR_ERR(device->bcc_dem_rxfe_if_clk_src);
		device->bcc_dem_rxfe_if_clk_src = NULL;
		goto err_clocks;
	}

	device->bcc_dem_rxfe_div3_i_clk = clk_get(&pdev->dev,
		clocks_info.bcc_dem_rxfe_div3_i_clk);
	if (IS_ERR(device->bcc_dem_rxfe_div3_i_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_dem_rxfe_div3_i_clk);
		ret = PTR_ERR(device->bcc_dem_rxfe_div3_i_clk);
		device->bcc_dem_rxfe_div3_i_clk = NULL;
		goto err_clocks;
	}

	device->bcc_dem_rxfe_div4_i_clk = clk_get(&pdev->dev,
		clocks_info.bcc_dem_rxfe_div4_i_clk);
	if (IS_ERR(device->bcc_dem_rxfe_div4_i_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_dem_rxfe_div4_i_clk);
		ret = PTR_ERR(device->bcc_dem_rxfe_div4_i_clk);
		device->bcc_dem_rxfe_div4_i_clk = NULL;
		goto err_clocks;
	}

	device->bcc_ts_out_clk_src = clk_get(&pdev->dev,
		clocks_info.bcc_ts_out_clk_src);
	if (IS_ERR(device->bcc_ts_out_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_ts_out_clk_src);
		ret = PTR_ERR(device->bcc_ts_out_clk_src);
		device->bcc_ts_out_clk_src = NULL;
		goto err_clocks;
	}

	device->bcc_ts_out_clk = clk_get(&pdev->dev,
		clocks_info.bcc_ts_out_clk);
	if (IS_ERR(device->bcc_ts_out_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_ts_out_clk);
		ret = PTR_ERR(device->bcc_ts_out_clk);
		device->bcc_ts_out_clk = NULL;
		goto err_clocks;
	}

	device->bcc_atv_rxfe_clk = clk_get(&pdev->dev,
		clocks_info.bcc_atv_rxfe_clk);
	if (IS_ERR(device->bcc_atv_rxfe_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_rxfe_clk);
		ret = PTR_ERR(device->bcc_atv_rxfe_clk);
		device->bcc_atv_rxfe_clk = NULL;
		goto err_clocks;
	}

	device->bcc_atv_rxfe_resamp_clk_src = clk_get(&pdev->dev,
		clocks_info.bcc_atv_rxfe_resamp_clk_src);
	if (IS_ERR(device->bcc_atv_rxfe_resamp_clk_src)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_rxfe_resamp_clk_src);
		ret = PTR_ERR(device->bcc_atv_rxfe_resamp_clk_src);
		device->bcc_atv_rxfe_resamp_clk_src = NULL;
		goto err_clocks;
	}

	device->bcc_atv_rxfe_resamp_clk = clk_get(&pdev->dev,
		clocks_info.bcc_atv_rxfe_resamp_clk);
	if (IS_ERR(device->bcc_atv_rxfe_resamp_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_rxfe_resamp_clk);
		ret = PTR_ERR(device->bcc_atv_rxfe_resamp_clk);
		device->bcc_atv_rxfe_resamp_clk = NULL;
		goto err_clocks;
	}

	device->bcc_atv_rxfe_x1_resamp_clk = clk_get(&pdev->dev,
		clocks_info.bcc_atv_rxfe_x1_resamp_clk);
	if (IS_ERR(device->bcc_atv_rxfe_x1_resamp_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_rxfe_x1_resamp_clk);
		ret = PTR_ERR(device->bcc_atv_rxfe_x1_resamp_clk);
		device->bcc_atv_rxfe_x1_resamp_clk = NULL;
		goto err_clocks;
	}

	device->bcc_atv_rxfe_div8_clk = clk_get(&pdev->dev,
		clocks_info.bcc_atv_rxfe_div8_clk);
	if (IS_ERR(device->bcc_atv_rxfe_div8_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_rxfe_div8_clk);
		ret = PTR_ERR(device->bcc_atv_rxfe_div8_clk);
		device->bcc_atv_rxfe_div8_clk = NULL;
		goto err_clocks;
	}

	device->albacore_sif_clk = clk_get(&pdev->dev,
		clocks_info.albacore_sif_clk);
	if (IS_ERR(device->albacore_sif_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.albacore_sif_clk);
		ret = PTR_ERR(device->albacore_sif_clk);
		device->albacore_sif_clk = NULL;
		goto err_clocks;
	}

	device->bcc_atv_x1_clk = clk_get(&pdev->dev,
		clocks_info.bcc_atv_x1_clk);
	if (IS_ERR(device->bcc_atv_x1_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_atv_x1_clk);
		ret = PTR_ERR(device->bcc_atv_x1_clk);
		device->bcc_atv_x1_clk  = NULL;
		goto err_clocks;
	}

	device->bcc_forza_sync_x5_clk = clk_get(&pdev->dev,
		clocks_info.bcc_forza_sync_x5_clk);
	if (IS_ERR(device->bcc_forza_sync_x5_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_forza_sync_x5_clk);
		ret = PTR_ERR(device->bcc_forza_sync_x5_clk);
		device->bcc_forza_sync_x5_clk = NULL;
		goto err_clocks;
	}

	device->atv_x5_clk = clk_get(&pdev->dev,
		clocks_info.atv_x5_clk);
	if (IS_ERR(device->atv_x5_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.atv_x5_clk);
		ret = PTR_ERR(device->atv_x5_clk);
		device->atv_x5_clk = NULL;
		goto err_clocks;
	}

	device->bcc_dem_ahb_clk = clk_get(&pdev->dev,
		clocks_info.bcc_dem_ahb_clk);
	if (IS_ERR(device->bcc_dem_ahb_clk)) {
		pr_err("%s: Failed to get %s",
			__func__, clocks_info.bcc_dem_ahb_clk);
		ret = PTR_ERR(device->bcc_dem_ahb_clk);
		device->bcc_dem_ahb_clk = NULL;
		goto err_clocks;
	}

	/* Set relevant clock rates */
	rate_in_hz = clk_round_rate(device->adc_01_clk_src, 1);
	/* the default rate is div3 rate and we need div1 */
	rate_in_hz = rate_in_hz*3;
	if (clk_set_rate(device->adc_01_clk_src, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to %s\n", __func__,
			rate_in_hz, clocks_info.adc_01_clk_src);
		goto err_clocks;
	}

	rate_in_hz = clk_round_rate(device->adc_2_clk_src, 1);
	/* the default rate is div3 rate and we need div1 */
	rate_in_hz = rate_in_hz*3;
	if (clk_set_rate(device->adc_2_clk_src, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to %s\n", __func__,
			rate_in_hz, clocks_info.adc_2_clk_src);
		goto err_clocks;
	}

	rate_in_hz = clk_round_rate(device->bcc_ts_out_clk_src, 1);
	if (clk_set_rate(device->bcc_ts_out_clk_src, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to %s\n", __func__,
			rate_in_hz, clocks_info.bcc_ts_out_clk_src);
		goto err_clocks;
	}

	rate_in_hz = clk_round_rate(device->bcc_atv_rxfe_resamp_clk_src, 1);
	if (clk_set_rate(device->bcc_atv_rxfe_resamp_clk_src, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to %s\n", __func__,
			rate_in_hz, clocks_info.bcc_atv_rxfe_resamp_clk_src);
		goto err_clocks;
	}

	atv_x5_clk_src = clk_get_parent(device->atv_x5_clk);
	rate_in_hz = 122880000;
	if (clk_set_rate(atv_x5_clk_src, rate_in_hz)) {
		pr_err("%s: Failed to set rate %lu to atv_x5_clk_src\n",
			__func__, rate_in_hz);
		goto err_clocks;
	}
	return 0;

err_clocks:
	demod_wrapper_clocks_put();
	return ret;
}

/**
 * demod_wrapper_map_io_memory() - Map memory resources to kernel space.
 *
 * @pdev:	Platform device, containing platform information.
 *
 * Return 0 on success, error value otherwise.
 */
static int demod_wrapper_map_io_memory(struct platform_device *pdev)
{
	struct resource *mem_demw;

	/* Get memory resources */
	mem_demw = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "demod-wrapper-base");
	if (!mem_demw) {
		pr_err("%s: Missing demod-wrapper-base resource"
			, __func__);
		return -ENXIO;
	}
	/* Map memory physical addresses to kernel space */
	device->base_mem = ioremap(mem_demw->start,
		resource_size(mem_demw));
	device->base = (unsigned int)(device->base_mem);
	if (!device->base) {
		pr_err("%s: ioremap failed", __func__);
		return -ENXIO;
	}
	return 0;
}

/**
 * demod_wrapper_init() - Initializes demos wrapper
 * structs.
 */
static void demod_wrapper_init(void)
{
	init_data_structs();
	demod_wrapper_debugfs_init();
}

/**
 * demod_wrapper_uninit() - Destroy device data
 *
 */
static void demod_wrapper_uninit(void)
{
	mutex_destroy(&(device->fd_mutex));
	demod_wrapper_debugfs_exit();
}

/**
 * demod_wrapper_destroy_device() - Destroy device
 *
 */
static void demod_wrapper_destroy_device(void)
{
	cdev_del(&(device->c_dev));
	device_destroy(device->demod_wrapper_class, device->demod_wrapper_dev);
	class_destroy(device->demod_wrapper_class);
	unregister_chrdev_region(device->demod_wrapper_dev, 1);
}

/**
 * demod_wrapper_create_device() - Create user space device.
 *
 */
static int demod_wrapper_create_device(void)
{
	int rc = 0;
	pr_debug("demod_wrapper init module");
	rc = alloc_chrdev_region(&(device->demod_wrapper_dev), 0, 1,
				"demod_wrapper");
	if (rc) {
		pr_err("demod_wrapper alloc_chrdev_region failed: %d\n", rc);
		goto err_devrgn;
	}

	device->demod_wrapper_class = class_create(THIS_MODULE,
						"demod_wrapper");
	if (IS_ERR(device->demod_wrapper_class)) {
		rc = PTR_ERR(device->demod_wrapper_class);
		pr_err("demod_wrapper class_create returned with error: %d\n",
			rc);
		goto err_class;
	}

	device->dev = device_create(device->demod_wrapper_class, NULL,
			 device->demod_wrapper_dev, NULL, "demod_wrapper");
	if (IS_ERR(device->dev)) {
		rc = PTR_ERR(device->dev);
		pr_err("demod_wrapper device_create returned with error: %d\n",
			rc);
		goto err_device;
	}
	cdev_init(&(device->c_dev), &demod_wrapper_fops);
	rc = cdev_add(&(device->c_dev), device->demod_wrapper_dev, 1);
	if (rc) {
		pr_err("demod_wrapper cdev_add returned with error: %d\n", rc);
		goto err_cdev;
	}
	return 0;

err_cdev:
	device_destroy(device->demod_wrapper_class, device->demod_wrapper_dev);
err_device:
	class_destroy(device->demod_wrapper_class);
err_class:
	unregister_chrdev_region(device->demod_wrapper_dev, 1);
err_devrgn:
	return rc;
}

/* Device driver function */
static int demod_wrapper_probe(struct platform_device *pdev)
{
	int rc = 0;

	pr_debug("Demod wrapper: %s started\n", __func__);

	if (pdev->dev.of_node) {
		/* Get power regulator */
		if (!of_get_property(pdev->dev.of_node, "vdd-supply", NULL)) {
			pr_err("%s: Could not find vdd-supply property\n",
				__func__);
			rc = -EINVAL;
			goto err_end;
		}
	} else {
		pr_err("%s: Can't read data from device tree\n", __func__);
		rc = -EINVAL;
		goto err_end;
	}

	/* allocate the device */
	device = devm_kzalloc(&pdev->dev,
				sizeof(struct demod_wrapper_device),
				GFP_KERNEL);
	if (!device) {
		pr_err("%s: Failed to allocate memory for device\n", __func__);
		rc = -ENOMEM;
		goto err_end;
	}

	platform_set_drvdata(pdev, device);
	device->pdev = pdev;

	rc = demod_wrapper_clocks_setup(pdev);
	if (rc)
		goto err_end;

	rc = demod_wrapper_map_io_memory(pdev);
	if (rc)
		goto err_map_io_memory;

	wakeup_source_init(&device->wakeup_src, dev_name(&pdev->dev));

	rc = demod_wrapper_create_device();
	if (rc)
		goto err_create_device;

	demod_wrapper_init();
	pr_debug("Demod wrapper: %s ended successfully\n", __func__);
	return rc;

err_create_device:
	wakeup_source_trash(&device->wakeup_src);
	iounmap(device->base_mem);
err_map_io_memory:
	demod_wrapper_clocks_put();

err_end:
	return rc;
}

/* Device driver remove function */
static int demod_wrapper_remove(struct platform_device *pdev)
{
	pr_debug("Demod wrapper remove\n");

	demod_wrapper_uninit();

	demod_wrapper_destroy_device();

	wakeup_source_trash(&device->wakeup_src);

	iounmap(device->base_mem);

	demod_wrapper_clocks_put();

	return 0;
}

/* Power Management */
static int demod_wrapper_runtime_suspend(struct device *dev)
{
	return 0;
}

static int demod_wrapper_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops demod_wrapper_dev_pm_ops = {
	.runtime_suspend = demod_wrapper_runtime_suspend,
	.runtime_resume = demod_wrapper_runtime_resume,
};

/* Platform driver information */
static struct of_device_id msm_demod_wrapper_match_table[] = {
	{.compatible = "qcom,msm-demod-wrapper"},
	{}
};
static struct platform_driver demod_wrapper_driver = {
	.probe          = demod_wrapper_probe,
	.remove         = demod_wrapper_remove,
	.driver         = {
		.name   = "demod_wrapper",
		.pm     = &demod_wrapper_dev_pm_ops,
		.of_match_table = msm_demod_wrapper_match_table,
	},
};

static int __init demod_wrapper_mod_init(void) /* Constructor */
{
	int rc;

	rc = platform_driver_register(&demod_wrapper_driver);
	if (rc)
		pr_err("%s: platform_driver_register failed: %d\n",
			__func__, rc);

	return rc;
}

static void __exit demod_wrapper_mod_exit(void) /* Destructor */
{
	platform_driver_unregister(&demod_wrapper_driver);
}

module_init(demod_wrapper_mod_init);
module_exit(demod_wrapper_mod_exit);

MODULE_DESCRIPTION("Demod Wrapper (demod_wrapper) Device Driver");
MODULE_LICENSE("GPL v2");
