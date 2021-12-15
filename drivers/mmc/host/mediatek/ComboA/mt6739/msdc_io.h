/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MSDC_IO_H_
#define _MSDC_IO_H_

/**************************************************************/
/* Section 1: Device Tree                                     */
/**************************************************************/
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

extern const struct of_device_id msdc_of_ids[];
extern unsigned int cd_gpio;

int msdc_dt_init(struct platform_device *pdev, struct mmc_host *mmc);

/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
int msdc_sd_power_switch(struct msdc_host *host, u32 on);
void msdc_set_host_power_control(struct msdc_host *host);

#if !defined(FPGA_PLATFORM)
void msdc_pmic_force_vcore_pwm(bool enable);
int msdc_oc_check(struct msdc_host *host, u32 en);
int msdc_io_check(struct msdc_host *host);
void msdc_sd_power_off(void);
void msdc_dump_ldo_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
void msdc_HQA_set_voltage(struct msdc_host *host);

#else
#define msdc_power_calibration_init(host)
#define msdc_pmic_force_vcore_pwm(enable)
void msdc_fpga_pwr_init(void);
#define msdc_oc_check(msdc_host, en)    (0)
#define msdc_io_check(msdc_host)        (1)
extern void msdc_fpga_power(struct msdc_host *host, u32 on);
#define msdc_emmc_power                 msdc_fpga_power
#define msdc_sd_power                   msdc_fpga_power
#define msdc_sdio_power                 msdc_fpga_power
#define msdc_sd_power_off()
#define msdc_dump_ldo_sts(buff, size, m, host)

#endif

/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#include <linux/clk.h>

#if defined(FPGA_PLATFORM)
extern  u32 hclks_msdc[];
#define msdc_dump_dvfs_reg(buff, size, m, host)
#define msdc_dump_clock_sts(buff, size, m, host)
#define msdc_get_hclk(host, src)        MSDC_SRC_FPGA
static inline int msdc_clk_enable(struct msdc_host *host) { return 0; }
#define msdc_clk_disable(host)
#define msdc_get_ccf_clk_pointer(pdev, host) (0)
#define msdc_clk_enable_and_stable(host)
#endif

#if !defined(FPGA_PLATFORM)
extern u32 *hclks_msdc_all[];
extern void msdc_dump_dvfs_reg(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
void msdc_dump_clock_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host);
#define msdc_get_hclk(id, src)          hclks_msdc_all[id][src]
int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
	struct msdc_host *host);
void msdc_clk_enable_and_stable(struct msdc_host *host);

#define msdc_clk_enable(host) \
	do { \
		(void)clk_enable(host->clk_ctl); \
		if (host->hclk_ctl) \
			(void)clk_enable(host->hclk_ctl); \
	} while (0)

#define msdc_clk_disable(host) \
	do { \
		clk_disable(host->clk_ctl); \
		if (host->hclk_ctl) \
			clk_disable(host->hclk_ctl); \
	} while (0)

#endif

#define msdc_clk_prepare_enable(host)     msdc_clk_enable(host)
#define msdc_clk_disable_unprepare(host)  msdc_clk_disable(host)

/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
/*******************************************************************************
 *PINMUX and GPIO definition
 ******************************************************************************/
#define MSDC_PIN_PULL_NONE      (0)
#define MSDC_PIN_PULL_DOWN      (1)
#define MSDC_PIN_PULL_UP        (2)
#define MSDC_PIN_KEEP           (3)

#define MSDC_TDRDSEL_SLEEP      (0)
#define MSDC_TDRDSEL_3V         (1)
#define MSDC_TDRDSEL_1V8        (2)
#define MSDC_TDRDSEL_CUST       (3)

#ifndef FPGA_PLATFORM
void msdc_set_driving_by_id(u32 id, struct msdc_hw_driving *driving);
void msdc_get_driving_by_id(u32 id, struct msdc_hw_driving *driving);
void msdc_set_ies_by_id(u32 id, int set_ies);
void msdc_set_sr_by_id(u32 id, int clk, int cmd, int dat, int rst, int ds);
void msdc_set_smt_by_id(u32 id, int set_smt);
void msdc_set_tdsel_by_id(u32 id, u32 flag, u32 value);
void msdc_set_rdsel_by_id(u32 id, u32 flag, u32 value);
void msdc_get_tdsel_by_id(u32 id, u32 *value);
void msdc_get_rdsel_by_id(u32 id, u32 *value);
void msdc_dump_vcore(char **buff, unsigned long *size, struct seq_file *m);
void msdc_dump_padctl_by_id(char **buff, unsigned long *size,
	struct seq_file *m, u32 id);
void msdc_pin_config_by_id(u32 id, u32 mode);
void msdc_set_pin_mode(struct msdc_host *host);

#else
#define msdc_set_driving_by_id(id, driving)
#define msdc_get_driving_by_id(id, driving)
#define msdc_set_ies_by_id(id, set_ies)
#define msdc_set_sr_by_id(id, clk, cmd, dat, rst, ds)
#define msdc_set_smt_by_id(id, set_smt)
#define msdc_set_tdsel_by_id(id, flag, value)
#define msdc_set_rdsel_by_id(id, flag, value)
#define msdc_get_tdsel_by_id(id, value)
#define msdc_get_rdsel_by_id(id, value)
#define msdc_dump_vcore(buff, size, m)
#define msdc_dump_padctl_by_id(buff, size, m, id)
#define msdc_pin_config_by_id(id, mode)
#define msdc_set_pin_mode(host)

#endif

#define msdc_get_driving(host, driving) \
	msdc_get_driving_by_id(host->id, driving)

#define msdc_set_driving(host, driving) \
	msdc_set_driving_by_id(host->id, driving)

#define msdc_set_ies(host, set_ies) \
	msdc_set_ies_by_id(host->id, set_ies)

#define msdc_set_sr(host, clk, cmd, dat, rst, ds) \
	msdc_set_sr_by_id(host->id, clk, cmd, dat, rst, ds)

#define msdc_set_smt(host, set_smt) \
	msdc_set_smt_by_id(host->id, set_smt)

#define msdc_set_tdsel(host, flag, value) \
	msdc_set_tdsel_by_id(host->id, flag, value)

#define msdc_set_rdsel(host, flag, value) \
	msdc_set_rdsel_by_id(host->id, flag, value)

#define msdc_get_tdsel(host, value) \
	msdc_get_tdsel_by_id(host->id, value)

#define msdc_get_rdsel(host, value) \
	msdc_get_rdsel_by_id(host->id, value)

#define msdc_dump_padctl(buff, size, m, host) \
	msdc_dump_padctl_by_id(buff, size, m, host->id)

#define msdc_pin_config(host, mode) \
	msdc_pin_config_by_id(host->id, mode)

#endif /* end of _MSDC_IO_H_ */
