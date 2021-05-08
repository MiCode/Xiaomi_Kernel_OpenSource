/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/******************************************************************************
 * MTK PMIC Wrapper Driver
 *
 * Copyright 2016 MediaTek Co.,Ltd.
 *
 * DESCRIPTION:
 *     This file provides API for other drivers to access PMIC registers
 *
 ******************************************************************************/

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/io.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mach/mtk_pmic_wrap.h>
#include "pwrap_hal.h"
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ccci_common.h>
#include <linux/ratelimit.h>
#undef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include "sspm_ipi.h"
#endif
#define PMIC_WRAP_DEVICE "pmic_wrap"

/************* marco    ******************************************************/
#if (PMIC_WRAP_PRELOADER)
#elif (PMIC_WRAP_LK)
#elif (PMIC_WRAP_KERNEL)
#ifdef CONFIG_OF
void __iomem *pwrap_base;
#ifndef PMIC_WRAP_NO_PMIC
static void __iomem *topckgen_base;
static void __iomem *infracfg_ao_base;
#endif /* end of #ifndef PMIC_WRAP_NO_PMIC */
#endif
#ifndef PMIC_WRAP_NO_PMIC
static struct mt_pmic_wrap_driver *mt_wrp;

static spinlock_t   wrp_lock = __SPIN_LOCK_UNLOCKED(lock);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#define WRITE_CMD   1
#define READ_CMD     0
#define WRITE_PMIC   1
#define WRITE_PMIC_WRAP   0

static unsigned int pwrap_recv_data[4] = {0};
static signed int pwrap_wacs2_ipi(unsigned int  adr, unsigned int rdata,
					  unsigned int flag);
static int pwrap_ipi_register(void);
#endif

#ifdef CONFIG_OF
static int pwrap_of_iomap(void);
static void pwrap_of_iounmap(void);
#endif
#endif /* end of #ifndef PMIC_WRAP_NO_PMIC */
#elif (PMIC_WRAP_SCP)
#elif (PMIC_WRAP_CTP)
#else
### Compile error, check SW ENV define
#endif

#ifdef PMIC_WRAP_NO_PMIC
#if !(PMIC_WRAP_KERNEL)
signed int pwrap_wacs2(unsigned int write, unsigned int adr, unsigned int wdata,
			    unsigned int *rdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

signed int pwrap_read(unsigned int adr, unsigned int *rdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

signed int pwrap_write(unsigned int adr, unsigned int wdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}
#endif
signed int pwrap_wacs2_read(unsigned int  adr, unsigned int *rdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

/* Provide PMIC write API */
signed int pwrap_wacs2_write(unsigned int  adr, unsigned int  wdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

signed int pwrap_read_nochk(unsigned int adr, unsigned int *rdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

signed int pwrap_write_nochk(unsigned int adr, unsigned int wdata)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

/*
 *pmic_wrap init,init wrap interface
 *
 */
static int __init pwrap_hal_init(void)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}
signed int pwrap_init(void)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

signed int pwrap_init_preloader(void)
{
	pr_info("[PMIC_WRAP] No PMIC real chip, PMIC_WRAP do Nothing.\n");
	return 0;
}

#else /* #ifdef PMIC_WRAP_NO_PMIC */
/*********************start ---internal API***********************************/
static int _pwrap_timeout_ns(unsigned long long start_time_ns,
				      unsigned long long timeout_time_ns);
static unsigned long long _pwrap_get_current_time(void);
static unsigned long long _pwrap_time2ns(unsigned long long time_us);
static signed int _pwrap_reset_spislv(void);
static signed int _pwrap_init_dio(unsigned int dio_en);
/* static signed int _pwrap_init_cipher(void); */
static signed int _pwrap_init_reg_clock(unsigned int regck_sel);
static void _pwrap_enable(void);
static void _pwrap_starve_set(void);
static signed int _pwrap_wacs2_nochk(unsigned int write, unsigned int adr,
				      unsigned int wdata, unsigned int *rdata);
static signed int pwrap_wacs2_hal(unsigned int write, unsigned int adr,
				      unsigned int wdata, unsigned int *rdata);
/*********************test API************************************************/
static inline void pwrap_dump_ap_register(void);
static unsigned int pwrap_write_test(void);
static unsigned int pwrap_read_test(void);
/************* end--internal API**********************************************/
/*********************** external API for pmic_wrap user *********************/
signed int pwrap_wacs2_read(unsigned int  adr, unsigned int *rdata)
{
	pwrap_wacs2_hal(0, adr, 0, rdata);
	return 0;
}

/* Provide PMIC write API */
signed int pwrap_wacs2_write(unsigned int  adr, unsigned int  wdata)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	unsigned int flag;

	flag = WRITE_CMD | (1 << WRITE_PMIC);
	pwrap_wacs2_ipi(adr, wdata, flag);
#else
	pwrap_wacs2_hal(1, adr, wdata, 0);
#endif
	return 0;
}

signed int pwrap_wacs2_audio_read(unsigned int  adr, unsigned int *rdata)
{
	pwrap_wacs2_hal(0, adr, 0, rdata);
	return 0;
}

signed int pwrap_wacs2_audio_write(unsigned int  adr, unsigned int  wdata)
{
	pwrap_wacs2_hal(1, adr, wdata, 0);
	return 0;
}
/******************************************************************************
 *wrapper timeout
 *****************************************************************************/
/*use the same API name with kernel driver
 *however,the timeout API in uboot use tick instead of ns
 */
#ifdef PWRAP_TIMEOUT
static unsigned long long _pwrap_get_current_time(void)
{
	return sched_clock();
}

static int _pwrap_timeout_ns(unsigned long long start_time_ns,
			     unsigned long long timeout_time_ns)
{
	unsigned long long cur_time = 0;
	unsigned long long elapse_time = 0;

	/* get current tick */
	cur_time = _pwrap_get_current_time();	/* ns */

	/* avoid timer over flow exiting in FPGA env */
	if (cur_time < start_time_ns) {
		pr_notice("@@@@Timer overflow! start%lld cur timer%lld\n",
			 start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = TIMEOUT_WAIT_IDLE * 1000;	/* 10000us */
		pr_notice("@@@@reset timer! start%lld setting%lld\n",
			 start_time_ns, timeout_time_ns);
	}

	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		pr_notice("@@@@Timeout: elapse time: %lld\n", elapse_time);
		pr_notice("@@@@Timeout: start time: %lld\n", start_time_ns);
		pr_notice("@@@@Timeout: set timeout: %lld\n", timeout_time_ns);
		pwrap_dump_ap_register();
		aee_kernel_warning("WRAPPER:ERR DUMP", "WRAP");
		return 1;
	}
	return 0;
}

static unsigned long long _pwrap_time2ns(unsigned long long time_us)
{
	return time_us * 1000;
}

#else
static unsigned long long _pwrap_get_current_time(void)
{
	return 0;
}
static int _pwrap_timeout_ns(unsigned long long start_time_ns,
				      unsigned long long elapse_time)
{
	return 0;
}

static unsigned long long _pwrap_time2ns(unsigned long long time_us)
{
	return 0;
}

#endif

/* ##################################################################### */
/* define macro and inline function (for do while loop) */
/* ##################################################################### */
/* define a function pointer */
typedef unsigned int(*loop_condition_fp) (unsigned int);

static inline unsigned int wait_for_fsm_idle(unsigned int x)
{
	return GET_WACS2_FSM(x) != WACS_FSM_IDLE;
}

static inline unsigned int wait_for_fsm_vldclr(unsigned int x)
{
	return GET_WACS2_FSM(x) != WACS_FSM_WFVLDCLR;
}

static inline unsigned int wait_for_sync(unsigned int x)
{
	return GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE;
}

static inline unsigned int wait_for_idle_and_sync(unsigned int x)
{
	return (GET_WACS2_FSM(x) != WACS_FSM_IDLE) ||
		(GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE);
}

static inline unsigned int wait_for_wrap_idle(unsigned int x)
{
	return (GET_WRAP_FSM(x) != 0x0) ||
		(GET_WRAP_CH_DLE_RESTCNT(x) != 0x0);
}

static inline unsigned int wait_for_wrap_state_idle(unsigned int x)
{
	return GET_WRAP_AG_DLE_RESTCNT(x) != 0;
}

static inline unsigned int wait_for_man_idle_and_noreq(unsigned int x)
{
	return (GET_MAN_REQ(x) != MAN_FSM_NO_REQ) ||
		(GET_MAN_FSM(x) != MAN_FSM_IDLE);
}

static inline unsigned int wait_for_man_vldclr(unsigned int x)
{
	return GET_MAN_FSM(x) != MAN_FSM_WFVLDCLR;
}

static inline unsigned int wait_for_cipher_ready(unsigned int x)
{
	return x != 3;
}

static inline unsigned int wait_for_stdupd_idle(unsigned int x)
{
	return GET_STAUPD_FSM(x) != 0x0;
}

/**************used at _pwrap_wacs2_nochk*************************************/
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
static inline unsigned int wait_for_state_ready_init(loop_condition_fp fp,
	unsigned int timeout_us, void *wacs_register, unsigned int *read_reg)
#else
static inline unsigned int wait_for_state_ready_init(
	loop_condition_fp fp, unsigned int timeout_us,
	unsigned int *wacs_register, unsigned int *read_reg)
#endif
{
	unsigned long long start_time_ns = 0, timeout_ns = 0;
	unsigned int reg_rdata = 0x0;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			pr_err("ready_init timeout\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));

	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
static inline unsigned int wait_for_state_idle(loop_condition_fp fp,
			unsigned int timeout_us, void *wacs_register,
			void *wacs_vldclr_register, unsigned int *read_reg)
#else
static inline unsigned int wait_for_state_idle(loop_condition_fp fp,
		unsigned int timeout_us, unsigned int *wacs_register,
		unsigned int *wacs_vldclr_register, unsigned int *read_reg)
#endif
{
	unsigned long long start_time_ns = 0, timeout_ns = 0;
	unsigned int reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			pr_err("state_idle timeout\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if (GET_WACS2_INIT_DONE2(reg_rdata) != WACS_INIT_DONE) {
			pr_err("init isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
		switch (GET_WACS2_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			pr_notice("WACS_FSM = VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			pr_notice("WACS_FSM = WFDLE\n");
			break;
		case WACS_FSM_REQ:
			pr_notice("WACS_FSM = REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));
	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

/**************used at pwrap_wacs2********************************************/
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
static inline unsigned int wait_for_state_ready(loop_condition_fp fp,
	unsigned int timeout_us, void *wacs_register, unsigned int *read_reg)
#else
static inline unsigned int wait_for_state_ready(
	loop_condition_fp fp, unsigned int timeout_us,
	unsigned int *wacs_register, unsigned int *read_reg)
#endif
{
	unsigned long long start_time_ns = 0, timeout_ns = 0;
	unsigned int reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);

	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			pr_err("state_ready timeout\n");
			pwrap_dump_ap_register();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if (GET_WACS2_INIT_DONE2(reg_rdata) != WACS_INIT_DONE) {
			pr_err("init isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while (fp(reg_rdata));
	if (read_reg)
		*read_reg = reg_rdata;

	return 0;
}

/******************************************************
 * Function : pwrap_wacs2_hal()
 * Description :
 * Parameter :
 * Return :
 ******************************************************/
static signed int pwrap_wacs2_hal(unsigned int write, unsigned int adr,
				       unsigned int wdata, unsigned int *rdata)
{
	unsigned int reg_rdata = 0;
	unsigned int wacs_write = 0;
	unsigned int wacs_adr = 0;
	unsigned int wacs_cmd = 0;
	unsigned int return_value = 0;
	unsigned long flags = 0;

	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&wrp_lock, flags);

	/* Check IDLE & INIT_DONE in advance */
	return_value =
		wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE,
			PMIC_WRAP_WACS2_RDATA, PMIC_WRAP_WACS2_VLDCLR, 0);
	if (return_value != 0) {
		pr_err("fsm_idle fail, ret=%d\n", return_value);
		goto FAIL;
	}
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);
	if (write == 0) {
		if (rdata == NULL) {
			pr_err("rdata NULL\n");
			return_value = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value =
		    wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ,
					PMIC_WRAP_WACS2_RDATA, &reg_rdata);
		if (return_value != 0) {
			pr_err("fsm_vldclr fail, ret=%d\n", return_value);
			return_value += 1;
			goto FAIL;
		}
		*rdata = GET_WACS2_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}

FAIL:
	spin_unlock_irqrestore(&wrp_lock, flags);
	if (return_value != 0) {
		pr_notice("pwrap_wacs2_hal fail, ret=%d\n", return_value);
		pr_notice("BUG_ON\n");
	}

	return return_value;
}


/*********************internal API for pwrap_init***************************/

/**********************************
 * Function : _pwrap_wacs2_nochk()
 * Description :
 * Parameter :
 * Return :
 ***********************************/
signed int pwrap_read_nochk(unsigned int adr, unsigned int *rdata)
{
	return _pwrap_wacs2_nochk(0, adr, 0, rdata);
}

signed int pwrap_write_nochk(unsigned int adr, unsigned int wdata)
{
	return _pwrap_wacs2_nochk(1, adr, wdata, 0);
}

static signed int _pwrap_wacs2_nochk(unsigned int write, unsigned int adr,
				       unsigned int wdata, unsigned int *rdata)
{
	unsigned int reg_rdata = 0x0;
	unsigned int wacs_write = 0x0;
	unsigned int wacs_adr = 0x0;
	unsigned int wacs_cmd = 0x0;
	unsigned int return_value = 0x0;

	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	/* Check IDLE */
	return_value =
	    wait_for_state_ready_init(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		pr_err("write fail, ret=%x\n", return_value);
		return return_value;
	}

	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);

	if (write == 0) {
		if (rdata == NULL) {
			pr_notice("rdata NULL\n");
			return_value = E_PWR_INVALID_ARG;
			return return_value;
		}
		return_value = wait_for_state_ready_init(wait_for_fsm_vldclr,
			      TIMEOUT_READ, PMIC_WRAP_WACS2_RDATA, &reg_rdata);
		if (return_value != 0) {
			pr_err("fsm_vldclr fail,ret=%d\n", return_value);
			return_value += 1;
			return return_value;
		}
		*rdata = GET_WACS2_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}

	return 0;
}

static void __pwrap_soft_reset(void)
{
	pr_info("start reset wrapper\n");
	WRAP_WR32(INFRA_GLOBALCON_RST2_SET, 0x1);
	WRAP_WR32(INFRA_GLOBALCON_RST2_CLR, 0x1);
}

static void __pwrap_spi_clk_set(void)
{
	pr_info("pwrap_spictl reset ok\n");

	/* sys_ck cg enable, turn off clock */
	WRAP_WR32(MODULE_SW_CG_0_SET, 0x0000000f);
	/* turn off clock */
	WRAP_WR32(MODULE_SW_CG_2_SET, 0x00000100);

	/* Select ULPOSC/16 Clock as SYS, TMR and SPI Clocks */
	/* in SODI-3.0 and Suspend Modes */
#if !defined(CONFIG_MTK_FPGA)
	WRAP_WR32(CLK_CFG_7_CLR, 0x00000097);
	WRAP_WR32(CLK_CFG_7_SET, 0x00000003);
	WRAP_WR32(CLK_CFG_UPDATE, (0x1 << 28));
#endif

	/* Disable Clock Source Control By SPM */
	pr_info("=====PMICW_CLOCK_CTRL===== (Write before): %x\n",
		 WRAP_RD32(PMICW_CLOCK_CTRL));
	WRAP_WR32(PMICW_CLOCK_CTRL,
		  (WRAP_RD32(PMICW_CLOCK_CTRL) & ~(0x1 << 2)));
	pr_info("=====PMICW_CLOCK_CTRL===== (Write after): %x\n",
		 WRAP_RD32(PMICW_CLOCK_CTRL));

	/* toggle PMIC_WRAP and pwrap_spictl reset */
	__pwrap_soft_reset();

	/*sys_ck cg enable, turn on clock*/
	WRAP_WR32(MODULE_SW_CG_0_CLR, 0x0000000f);
	/* turn on clock*/
	WRAP_WR32(MODULE_SW_CG_2_CLR, 0x00000100);
	pr_info("spi clk set ....\n");
}

/************************************************
 * Function : _pwrap_init_dio()
 * Description :call it in pwrap_init,mustn't check init done
 * Parameter :
 * Return :
 ************************************************/
static signed int _pwrap_init_dio(unsigned int dio_en)
{
	unsigned int rdata = 0x0;

	WRAP_WR32(PMIC_WRAP_HPRIO_ARB_EN, 0x4); /* ONLY WACS2 */

	/* wait for WRAP_FSM idle */
	do {
		rdata = WRAP_RD32(PMIC_WRAP_WRAP_STA);
	} while ((GET_WRAP_FSM(rdata) != 0x0) ||
		 (GET_WRAP_CH_DLE_RESTCNT(rdata) != 0x0));

	pwrap_write_nochk(PMIC_DEW_DIO_EN_ADDR, dio_en);
#ifdef DUAL_PMICS
	pwrap_write_nochk(EXT_DEW_DIO_EN, dio_en);
#endif

	do {
		rdata = WRAP_RD32(PMIC_WRAP_WACS2_RDATA);
	} while ((GET_WACS2_FSM(rdata) != WACS_FSM_IDLE) ||
		 (GET_SYNC_IDLE2(rdata) != WACS_SYNC_IDLE));

#ifndef DUAL_PMICS
	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);
#else
	WRAP_WR32(PMIC_WRAP_DIO_EN, 0x2 | dio_en);
#endif

	return 0;
}

static void _pwrap_InitStaUpd(void)
{

#ifndef DUAL_PMICS
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xf5);
#else
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xff);
#endif

#ifdef PMIC_WRAP_CRC_SUPPORT
	/* CRC */
#ifndef DUAL_PMICS
	pwrap_write_nochk(PMIC_DEW_CRC_EN_ADDR, 0x1);
	WRAP_WR32(PMIC_WRAP_CRC_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, PMIC_DEW_CRC_VAL_ADDR);
#else
	pwrap_write_nochk(PMIC_DEW_CRC_EN_ADDR, 0x1);
	pwrap_write_nochk(EXT_DEW_CRC_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_CRC_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, (PMIC_EXT_DEW_CRC_VAL_ADDR << 16 |
				      PMIC_DEW_CRC_VAL_ADDR));
#endif
#else
	/* Signature */
#ifndef DUAL_PMICS
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, PMIC_DEW_CRC_VAL_ADDR);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, 0x83);
#else
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x3);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, (PMIC_EXT_DEW_CRC_VAL_ADDR << 16) |
				      PMIC_DEW_CRC_VAL_ADDR);
	WRAP_WR32(PMIC_WRAP_SIG_VALUE, (0x83 << 16) | 0x83);
#endif
#endif /* end of crc */

	WRAP_WR32(PMIC_WRAP_EINT_STA0_ADR, PMIC_CPU_INT_STA_ADDR);
#ifdef DUAL_PMICS
	WRAP_WR32(PMIC_WRAP_EINT_STA1_ADR, EXT_INT_STA);
#endif

	/* MD ADC Interface */
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_LATEST_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_WP_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_0_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_1_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_2_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_3_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_4_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_5_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_6_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_7_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_8_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_9_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_10_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_11_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_12_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_13_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_14_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_15_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_16_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_17_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_18_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_19_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_20_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_21_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_22_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_23_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_24_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_25_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_26_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_27_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_28_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_29_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_30_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);
	WRAP_WR32(PMIC_WRAP_MD_AUXADC_RDATA_31_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_MDRT_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_MDRT_ADDR);

	WRAP_WR32(PMIC_WRAP_INT_GPS_AUXADC_CMD_ADDR,
		  (PMIC_AUXADC_RQST_DCXO_BY_GPS_ADDR << 16) +
		   PMIC_AUXADC_RQST_CH7_ADDR);
	WRAP_WR32(PMIC_WRAP_INT_GPS_AUXADC_CMD, (0x0400 << 16) + 0x0080);
	WRAP_WR32(PMIC_WRAP_INT_GPS_AUXADC_RDATA_ADDR,
		  (PMIC_AUXADC_ADC_OUT_DCXO_BY_GPS_ADDR << 16) +
		   PMIC_AUXADC_ADC_OUT_CH7_BY_AP_ADDR);

	WRAP_WR32(PMIC_WRAP_EXT_GPS_AUXADC_RDATA_ADDR,
		  PMIC_AUXADC_ADC_OUT_MDRT_ADDR);

}

static void _pwrap_starve_set(void)
{
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, 0xf);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_0, 0x400);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_1, 0x402);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_2, 0x402);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_3, 0x40e);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_4, 0x402);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_5, 0x427);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_6, 0x427);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_7, 0x4a4);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_8, 0x413);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_9, 0x417);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_10, 0x417);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_11, 0x47b);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_12, 0x47b);
	WRAP_WR32(PMIC_WRAP_STARV_COUNTER_13, 0x45b);
}

static void _pwrap_enable(void)
{
#if (MTK_PLATFORM_MT6357)
	WRAP_WR32(PMIC_WRAP_HPRIO_ARB_EN, 0x3fd35);
#endif
	WRAP_WR32(PMIC_WRAP_WACS0_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_WACS_P2P_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_WACS_MD32_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_STAUPD_CTRL, 0x5); /* 100us */
	WRAP_WR32(PMIC_WRAP_WDT_UNIT, 0xf);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN_0, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN_1, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_TIMER_CTRL, 0x3);

#if defined(CONFIG_MACH_MT6761)
	WRAP_WR32(PMIC_WRAP_INT0_EN, 0x00000007);
	/* disable Matching interrupt for bit 13 */
	WRAP_WR32(PMIC_WRAP_INT1_EN, 0xffffd800);
#elif defined(CONFIG_MACH_MT6765)
	WRAP_WR32(PMIC_WRAP_INT0_EN, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_INT1_EN, 0xffffffff);
#endif
}

/************************************************
 * Function : _pwrap_init_sistrobe()
 * scription : Initialize SI_CK_CON and SIDLY
 * Parameter :
 * Return :
 ************************************************/
static signed int _pwrap_init_sistrobe(int dual_si_sample_settings)
{
	unsigned int rdata;
	int si_en_sel, si_ck_sel, si_dly, si_sample_ctrl, clk_edge_no, i;
	int found, result_faulty = 0;
	int test_data[30] = {0x6996, 0x9669, 0x6996, 0x9669, 0x6996, 0x9669,
			     0x6996, 0x9669, 0x6996, 0x9669, 0x5AA5, 0xA55A,
			     0x5AA5, 0xA55A, 0x5AA5, 0xA55A, 0x5AA5, 0xA55A,
			     0x5AA5, 0xA55A, 0x1B27, 0x1B27, 0x1B27, 0x1B27,
			     0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27, 0x1B27
			    };

	/* TINFO = "[InitSiStrobe] SI Strobe Calibration For PMIC 0" */
	/* TINFO = "[InitSiStrobe] Scan For First Valid Sampling Clock Edge" */
	found = 0;
	for (si_en_sel = 0; si_en_sel < 8; si_en_sel++) {
		for (si_ck_sel = 0; si_ck_sel < 2; si_ck_sel++) {
			si_sample_ctrl = (si_en_sel << 6) | (si_ck_sel << 5);
			WRAP_WR32(PMIC_WRAP_SI_SAMPLE_CTRL, si_sample_ctrl);

			pwrap_read_nochk(PMIC_DEW_READ_TEST_ADDR, &rdata);
			if (rdata == DEFAULT_VALUE_READ_TEST) {
				pr_info("[InitSiStrobe] ");
				pr_info("First Valid Sampling Clock Found\n");
				pr_info("si_en_sel = %x\n", si_en_sel);
				pr_info("si_ck_sel = %x\n", si_ck_sel);
				pr_info("si_sample_ctrl=%x\n", si_sample_ctrl);
				pr_info("rdata = %x\n", rdata);
				found = 1;
				break;
			}
			pr_info("si_en_sel = %x\n", si_en_sel);
			pr_info("si_ck_sel = %x\n", si_ck_sel);
			pr_info("si_sample_ctrl = %x\n", si_sample_ctrl);
			pr_info("rdata = %x\n", rdata);
		}
		if (found == 1)
			break;
	}
	if (found == 0) {
		result_faulty |= 0x1;
		pr_notice("result_faulty = %d\n", result_faulty);
	}
	if ((si_en_sel == 7) && (si_ck_sel == 1)) {
		result_faulty |= 0x2;
		pr_notice("result_faulty_2 = %d\n", result_faulty);
	}
	/* TINFO = "[InitSiStrobe] Search For The Data Boundary" */
	for (si_dly = 0; si_dly < 10; si_dly++) {
		pwrap_write_nochk(PMIC_RG_SPI_DLY_SEL_ADDR, si_dly);

		found = 0;
#ifndef SPEED_UP_PWRAP_INIT
		for (i = 0; i < 30; i++)
#else
		for (i = 0; i < 1; i++)
#endif
		{
			pwrap_write_nochk(PMIC_DEW_WRITE_TEST_ADDR,
					  test_data[i]);
			pwrap_read_nochk(PMIC_DEW_WRITE_TEST_ADDR, &rdata);
			if ((rdata & 0x7fff) != (test_data[i] & 0x7fff)) {
				pr_notice("[InitSiStrobe] ");
				pr_notice("Data Boundary Is Found !!!\n");
				pr_notice("si_dly = %x\n", si_dly);
				pr_notice("rdata = %x\n", rdata);
				found = 1;
				break;
			}
		}
		if (found == 1)
			break;
		pr_notice("si_dly = %x, *RG_SPI_CON2 = %x, rdata = %x\n",
				 si_dly, si_dly, rdata);
	}

	/* TINFO = "[InitSiStrobe] Change Sampling Clock Edge To Next One." */
	si_sample_ctrl = si_sample_ctrl + 0x20;
	WRAP_WR32(PMIC_WRAP_SI_SAMPLE_CTRL, si_sample_ctrl);
	if (si_dly == 10) {
		pr_info("SI Strobe Calibration For PMIC 0 Done\n");
		pr_info("si_sample_ctrl = %x\n", si_sample_ctrl);
		pr_info("si_dly = %x\n", si_dly);
		si_dly--;
	}
	pr_info("SI Strobe Calibration For PMIC 0 Done, (%x, %x)\n",
		 si_sample_ctrl, si_dly);

#if PMIC_WRAP_ULPOSC_CAL
	/* SI Strobe Calibration For ULPOSC Clock */
	/* TINFO = "[InitSiStrobe] SI Strobe Calibration For ULPOSC Clock" */
	si_en_sel = (WRAP_RD32(PMIC_WRAP_SI_SAMPLE_CTRL) << 23) >> 29;
	si_ck_sel = (WRAP_RD32(PMIC_WRAP_SI_SAMPLE_CTRL) << 26) >> 31;
	clk_edge_no = (((si_en_sel * 2 + si_ck_sel) * 100) *
		      CLK_26M_PRD / CLK_ULPOSC_PRD + 50) / 100;
	/* TINFO = "[InitSiStrobe] Sampling Clock Edge Is Chosen" */
	si_en_sel = clk_edge_no / 2;
	si_ck_sel = clk_edge_no % 2;
	si_sample_ctrl = (1 << 19) | (si_en_sel << 6) | (si_ck_sel << 5);
	WRAP_WR32(PMIC_WRAP_SI_SAMPLE_CTRL_ULPOSC, si_sample_ctrl);
	/* TINFO = "[InitSiStrobe] SI Strobe Calibration Done" */
#endif /* end of #if PMIC_WRAP_ULPOSC_CAL */

	if (result_faulty != 0)
		return result_faulty;

	/* Read Test */
	pwrap_read_nochk(PMIC_DEW_READ_TEST_ADDR, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		pr_notice("_pwrap_init_sistrobe Read Test Failed\n");
		pr_notice("rdata = %x, exp = 0x5aa5\n", rdata);
		return 0x10;
	}
	pr_info("_pwrap_init_sistrobe Read Test ok\n");

	return 0;
}

static int __pwrap_InitSPISLV(void)
{
	/* turn on IO filter function */
	pwrap_write_nochk(PMIC_RG_SRCLKEN_IN0_FILTER_EN_ADDR, 0xf0);
	/* turn on IO SMT function to improve noise immunity */
	pwrap_write_nochk(PMIC_RG_SMT_SPI_CLK_ADDR, 0xf);
	/* turn off IO pull function for power saving */
	pwrap_write_nochk(PMIC_GPIO_PULLEN0_CLR_ADDR, 0xf0);
	/* turn off IO pull function for power saving */
	pwrap_write_nochk(PMIC_RG_SLP_RW_EN_ADDR, 0x1);
	/* set IO driving strength to 4 mA */
	pwrap_write_nochk(PMIC_RG_OCTL_SPI_CLK_ADDR, 0x8888);
#ifdef DUAL_PMICS
	/* turn on IO filter function */
	pwrap_write_nochk(EXT_FILTER_CON0, 0xf);
	/* turn on IO SMT function to improve noise immunity */
	pwrap_write_nochk(EXT_SMT_CON1, 0xf);
	/* turn off IO pull function for power saving */
	pwrap_write_nochk(EXT_RG_SPI_CON, 0x1);
	/* set IO driving strength to 4 mA */
	pwrap_write_nochk(EXT_DRV_CON1, 0x8888);
#endif

	return 0;
}

/******************************************************
 * Function : _pwrap_reset_spislv()
 * Description :
 * Parameter :
 * Return :
 ******************************************************/
static signed int _pwrap_reset_spislv(void)
{
	unsigned int ret = 0;
	unsigned int return_value = 0;

	WRAP_WR32(PMIC_WRAP_HPRIO_ARB_EN, DISABLE_ALL);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0x0);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, MANUAL_MODE);
	WRAP_WR32(PMIC_WRAP_MAN_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_DIO_EN, 0x0);

	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSL << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSH << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	return_value = wait_for_state_ready_init(wait_for_sync,
			TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);

	if (return_value != 0) {
		pr_info("reset_spislv fail,ret=%x\n", return_value);
		ret = E_PWR_TIMEOUT;
		goto timeout;
	}

	WRAP_WR32(PMIC_WRAP_MAN_EN, 0x0);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);

timeout:
	WRAP_WR32(PMIC_WRAP_MAN_EN, 0x0);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);
	return ret;
}

static signed int _pwrap_init_reg_clock(unsigned int regck_sel)
{
	unsigned int rdata;

	WRAP_WR32(PMIC_WRAP_EXT_CK_WRITE, 0x1);
#ifndef SLV_CLK_1M
#ifndef DUAL_PMICS
	/* Set Read Dummy Cycle Number (Slave Clock is 18MHz) */
	_pwrap_wacs2_nochk(1, PMIC_DEW_RDDMY_NO_ADDR, 0x8, &rdata);
	WRAP_WR32(PMIC_WRAP_RDDMY, 0x8);
	pr_info("NO_SLV_CLK_1M Set Read Dummy Cycle\n");
#else
	_pwrap_wacs2_nochk(1, PMIC_DEW_RDDMY_NO_ADDR, 0x8, &rdata);
	_pwrap_wacs2_nochk(1, EXT_DEW_RDDMY_NO, 0x8, &rdata);
	WRAP_WR32(PMIC_WRAP_RDDMY, 0x0808);
	pr_info("NO_SLV_CLK_1M Set Read Dummy Cycle dual_pmics\n");
#endif
#else
#ifndef DUAL_PMICS
	/* Set Read Dummy Cycle Number (Slave Clock is 1MHz) */
	_pwrap_wacs2_nochk(1, PMIC_DEW_RDDMY_NO_ADDR, 0x68, &rdata);
	WRAP_WR32(PMIC_WRAP_RDDMY, 0x68);
	pr_info("SLV_CLK_1M Set Read Dummy Cycle\n");
#else
	_pwrap_wacs2_nochk(1, PMIC_DEW_RDDMY_NO_ADDR, 0x68, &rdata);
	_pwrap_wacs2_nochk(1, EXT_DEW_RDDMY_NO, 0x68, &rdata);
	WRAP_WR32(PMIC_WRAP_RDDMY, 0x6868);
	pr_info("SLV_CLK_1M Set Read Dummy Cycle dual_pmics\n");
#endif
#endif

	/* Config SPI Waveform according to reg clk */
	if (regck_sel == 1) { /* Slave Clock is 18MHz */
		/* wait data written into register => 4T_PMIC:
		 * CSHEXT_WRITE_START+EXT_CK+CSHEXT_WRITE_END+CSLEXT_START
		 */
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x0);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_WRITE, 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_READ, 0x0200);
	} else { /*Safe Mode*/
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE, 0x0f0f);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ, 0x0f0f);
		WRAP_WR32(PMIC_WRAP_CSLEXT_WRITE, 0x0f0f);
		WRAP_WR32(PMIC_WRAP_CSLEXT_READ, 0x0f0f);
	}

	return 0;
}

static int _pwrap_wacs2_write_test(int pmic_no)
{
	unsigned int rdata;

	if (pmic_no == 0) {
		pwrap_write_nochk(PMIC_DEW_WRITE_TEST_ADDR, 0xa55a);
		pwrap_read_nochk(PMIC_DEW_WRITE_TEST_ADDR, &rdata);
		if (rdata != 0xa55a) {
			pr_notice("Error: w_rdata = %x, exp = 0xa55a\n", rdata);
			return E_PWR_WRITE_TEST_FAIL;
		}
	}

#ifdef DUAL_PMICS
	if (pmic_no == 1) {
		pwrap_write_nochk(EXT_DEW_WRITE_TEST, 0xa55a);
		pwrap_read_nochk(EXT_DEW_WRITE_TEST, &rdata);
		if (rdata != 0xa55a) {
			pr_notice("Error: ext_w_rdata=%x, exp=0xa55a\n", rdata);
			return E_PWR_WRITE_TEST_FAIL;
		}
	}
#endif

	return 0;
}

static unsigned int pwrap_read_test(void)
{
	unsigned int rdata = 0;
	unsigned int return_value = 0;
	/* Read Test */
	return_value = pwrap_wacs2_read(PMIC_DEW_READ_TEST_ADDR, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		pr_notice("Error: r_rdata = 0x%x, exp = 0x5aa5\n", rdata);
		pr_notice("Error: ret = 0x%x\n", return_value);
		return E_PWR_READ_TEST_FAIL;
	}
	pr_info("Read Test pass,return_value=%d\n", return_value);

	return 0;
}
static unsigned int pwrap_write_test(void)
{
	unsigned int rdata = 0;
	unsigned int sub_return = 0;
	unsigned int sub_return1 = 0;

	/* Write test using WACS2 */
	pr_info("start pwrap_write\n");
	sub_return = pwrap_wacs2_write(PMIC_DEW_WRITE_TEST_ADDR,
				       DEFAULT_VALUE_READ_TEST);
	pr_info("after pwrap_write\n");
	sub_return1 = pwrap_wacs2_read(PMIC_DEW_WRITE_TEST_ADDR, &rdata);
	if ((rdata != DEFAULT_VALUE_READ_TEST) ||
	    (sub_return != 0) || (sub_return1 != 0)) {
		pr_notice("Error: w_rdata = 0x%x, exp = 0xa55a\n", rdata);
		pr_notice("Error: sub_return = 0x%x\n", sub_return);
		pr_notice("Error: sub_return1 = 0x%x\n", sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
	pr_info("write Test pass\n");

	return 0;
}
static void pwrap_ut(unsigned int ut_test)
{
	unsigned int sub_return = 0;

	switch (ut_test) {
	case 1:
		pwrap_write_test();
		break;
	case 2:
		pwrap_read_test();
		break;
	case 3:
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		pwrap_wacs2_ipi(0x10010000 + 0xD8, 0xffffffff,
				(WRITE_CMD | WRITE_PMIC_WRAP));
		break;
#endif
	case 4:
		sub_return =
			pwrap_write_nochk(PMIC_DEW_WRITE_TEST_ADDR, 0x1234);
		sub_return =
			pwrap_write_nochk(PMIC_DEW_WRITE_TEST_ADDR, 0x4321);
		sub_return =
			pwrap_write_nochk(PMIC_DEW_WRITE_TEST_ADDR, 0xF0F0);
		break;
	default:
		pr_info("default test.\n");
		break;
	}
}

signed int pwrap_init(void)
{
	signed int sub_return = 0;

	pr_notice(PWRAPTAG "cpuid=%d,%s\n", raw_smp_processor_id(), __func__);

#ifdef CONFIG_OF
	sub_return = pwrap_of_iomap();
	if (sub_return)
		return sub_return;
#endif

	pr_info("pwrap_init start!!!!!!!!!!!!!\n");

	/* Set SoC SPI IO Driving Strength to 4 mA */
#if defined(CONFIG_MACH_MT6761)
	WRAP_WR32(IOCFG_LM_DRV_CFG1_CLR, 0x7 | (0x7 << 3));
	WRAP_WR32(IOCFG_LM_DRV_CFG1_SET, 0x1 | (0x1 << 3));
#elif defined(CONFIG_MACH_MT6765)
	WRAP_WR32(IOCFG_LM_DRV_CFG1_CLR, 0x3f);
	WRAP_WR32(IOCFG_LM_DRV_CFG1_SET, 0x9);
#endif

	/* Set SPI MO and MI IOs to Pull Down */
#if defined(CONFIG_MACH_MT6761)
	WRAP_WR32(IOCFG_LM_PU_CFG0_CLR, (0x1 << 12) | (0x1 << 13));
	WRAP_WR32(IOCFG_LM_PD_CFG0_SET, (0x1 << 12) | (0x1 << 13));
#elif defined(CONFIG_MACH_MT6765)
	WRAP_WR32(IOCFG_LM_PU_CFG0_CLR, (0x1 << 12));
	WRAP_WR32(IOCFG_LM_PD_CFG0_SET, (0x1 << 12));
#endif

	__pwrap_spi_clk_set();

	pr_info("__pwrap_spi_clk_set ok\n");

	/* Enable DCM */
	pr_info("Not need to enable DCM\n");

	/* Reset SPISLV */
	sub_return = _pwrap_reset_spislv();
	if (sub_return != 0) {
		pr_notice("reset_spislv fail,ret=%x\n", sub_return);
		return E_PWR_INIT_RESET_SPI;
	}
	pr_info("Reset SPISLV ok\n");

	/* Enable WRAP */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, 0x1);
	pr_info("Enable WRAP ok\n");

	/* Enable WACS2 */
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_HPRIO_ARB_EN, 0x4); /* ONLY WACS2 */

	pr_info("Enable WACS2 ok\n");

	/* SPI Waveform Configuration. 0:safe mode, 1:18MHz */
	sub_return = _pwrap_init_reg_clock(1);
	if (sub_return != 0) {
		pr_notice("init_reg_clock fail,ret=%x\n", sub_return);
		return E_PWR_INIT_REG_CLOCK;
	}
	pr_info("_pwrap_init_reg_clock ok\n");

	/* SPI Slave Configuration */
	sub_return = __pwrap_InitSPISLV();
	if (sub_return != 0) {
		pr_notice("InitSPISLV Failed, ret = %x", sub_return);
		return -1;
	}

	/* Enable DIO mode */
	sub_return = _pwrap_init_dio(1);
	if (sub_return != 0) {
		pr_notice("dio test error,err=%x, ret=%x\n", 0x11, sub_return);
		return E_PWR_INIT_DIO;
	}
	pr_info("_pwrap_init_dio ok\n");

	/* Input data calibration flow; */
	sub_return = _pwrap_init_sistrobe(0);
	if (sub_return != 0) {
		pr_notice("InitSiStrobe fail,ret=%x\n", sub_return);
		return E_PWR_INIT_SIDLY;
	}
	pr_info("_pwrap_init_sistrobe ok\n");

#if 0
	/* Enable Encryption */
	sub_return = _pwrap_init_cipher();
	if (sub_return != 0) {
		pr_notice("Encryption fail, ret=%x\n", sub_return);
		return E_PWR_INIT_CIPHER;
	}
	pr_info("_pwrap_init_cipher ok\n");
#endif

	/*  Write test using WACS2. check Write test default value */
	sub_return = _pwrap_wacs2_write_test(0);
	if (sub_return != 0) {
		pr_notice("write test 0 fail\n");
		return E_PWR_INIT_WRITE_TEST;
	}
	pr_info("_pwrap_wacs2_write_test ok\n");

#ifdef DUAL_PMICS
	sub_return = _pwrap_wacs2_write_test(1);
	if (sub_return != 0) {
		pr_notice("write test 1 fail\n");
		return E_PWR_INIT_WRITE_TEST;
	}
	pr_info("_pwrap_wacs2_write_test dual ok\n");
#endif

	/* Status update function initialization
	 * 1. Signature Checking using CRC (CRC 0 only)
	 * 2. EINT update
	 * 3. Read back Auxadc thermal data for GPS
	 */
	_pwrap_InitStaUpd();
	pr_info("_pwrap_InitStaUpd ok\n");

	/* PMIC_WRAP starvation setting */
	_pwrap_starve_set();
	pr_info("_pwrap_starve_set ok\n");

	/* PMIC_WRAP enables */
	_pwrap_enable();
	pr_info("_pwrap_enable ok\n");

	/* Initialization Done */
	WRAP_WR32(PMIC_WRAP_INIT_DONE0, 0x1);
	WRAP_WR32(PMIC_WRAP_INIT_DONE2, 0x1);
	WRAP_WR32(PMIC_WRAP_INIT_DONE_P2P, 0x1);
	WRAP_WR32(PMIC_WRAP_INIT_DONE_MD32, 0x1);

	pwrap_ut(1);
	pwrap_ut(2);

	pr_info("pwrap_init Done!!!!!!!!!\n");

#ifdef CONFIG_OF
	pwrap_of_iounmap();
#endif

/* for simulation runtime ipi test */
#if 0
	mdelay(20000);
	for (i = 0; i < 5; i++) {
		pwrap_ut(1);
		pwrap_ut(2);
		pwrap_ut(3);
	}
#endif
	return 0;
}

/*-------------------pwrap debug---------------------*/
static inline void pwrap_dump_ap_register(void)
{
	unsigned int i = 0, offset = 0;
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
	unsigned int *reg_addr;
#else
	unsigned int reg_addr;
#endif
	unsigned int reg_value = 0;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	pr_notice("dump reg\n");
	if (__ratelimit(&ratelimit)) {
		for (i = 0; i <= PMIC_WRAP_REG_RANGE; i++) {
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
			reg_addr = (unsigned int *) (PMIC_WRAP_BASE + i * 4);
			reg_value = WRAP_RD32(((unsigned int *)
					      (PMIC_WRAP_BASE + i * 4)));
			pr_notice("addr:0x%p = 0x%x\n", reg_addr, reg_value);
#else
			reg_addr = (PMIC_WRAP_BASE + i * 4);
			reg_value = WRAP_RD32(reg_addr);
			pr_notice("addr:0x%x = 0x%x\n", reg_addr, reg_value);
#endif
		}
	}

	for (i = 0; i <= 14; i++) {
		offset = 0xc00 + i * 4;
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
		reg_addr = (unsigned int *) (PMIC_WRAP_BASE + offset);
		reg_value = WRAP_RD32(reg_addr);
		pr_notice("addr:0x%p = 0x%x\n", reg_addr, reg_value);
#else
		reg_addr = (PMIC_WRAP_BASE + offset);
		reg_value = WRAP_RD32(reg_addr);
		pr_notice("addr:0x%x = 0x%x\n", reg_addr, reg_value);
#endif
	}

	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x0);
	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x8); /* clear log */

#ifdef PMIC_WRAP_MATCH_SUPPORT
	/* Matching mode and Stop recording after interrupt trigger */
	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x5); /* reenable */
#else
	/* Matching mode and Continue recording after interrupt trigger */
	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x1); /* reenable */
#endif
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x1);

}

static inline void pwrap_dump_pmic_register(void)
{
	unsigned int i = 0, reg_addr = 0, reg_value = 0, ret = 0;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	pr_info("dump PMIC register\n");
	if (__ratelimit(&ratelimit)) {
		for (i = 0; i <= 4; i++) {
			reg_addr = (PMIC_HWCID_ADDR + i * 2);
			ret = pwrap_read_nochk(reg_addr, &reg_value);
			pr_info("[REG]0x%x=0x%x\n", reg_addr, reg_value);
		}
		for (i = 0; i <= 14; i++) {
			reg_addr = (PMIC_RG_SLP_RW_EN_ADDR + i * 2);
			ret = pwrap_read_nochk(reg_addr, &reg_value);
			pr_info("[REG]0x%x=0x%x\n", reg_addr, reg_value);
		}
	}
}

static void pwrap_logging_at_isr(void)
{
	unsigned int i = 0, offset = 0;
#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
	unsigned int *reg_addr;
#else
	unsigned int reg_addr;
#endif
	unsigned int val = 0;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 5);

	if (__ratelimit(&ratelimit)) {
	pr_info("INT0 flag 0x%x\n", WRAP_RD32(PMIC_WRAP_INT0_FLG));
	pr_info("INT1 flag 0x%x\n", WRAP_RD32(PMIC_WRAP_INT1_FLG));
	pr_info("DCXO_CONN_ADR0=0x%x\n", WRAP_RD32(PMIC_WRAP_DCXO_CONN_ADR0));
	pr_info("DCXO_CONN_ADR1=0x%x\n", WRAP_RD32(PMIC_WRAP_DCXO_CONN_ADR1));
	pr_info("MONITOR_CTRL_0=0x%x\n", WRAP_RD32(PMIC_WRAP_MONITOR_CTRL_0));
	pr_info("CH_SEQ_0=0x%x\n", WRAP_RD32(PMIC_WRAP_CHANNEL_SEQUENCE_0));
	pr_info("CH_SEQ_1=0x%x\n", WRAP_RD32(PMIC_WRAP_CHANNEL_SEQUENCE_1));
	pr_info("CH_SEQ_2=0x%x\n", WRAP_RD32(PMIC_WRAP_CHANNEL_SEQUENCE_2));
	pr_info("CH_SEQ_3=0x%x\n", WRAP_RD32(PMIC_WRAP_CHANNEL_SEQUENCE_3));
	pr_info("CMD_SEQ_0=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_0));
	pr_info("CMD_SEQ_1=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_1));
	pr_info("CMD_SEQ_2=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_2));
	pr_info("CMD_SEQ_3=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_3));
	pr_info("CMD_SEQ_4=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_4));
	pr_info("CMD_SEQ_5=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_5));
	pr_info("CMD_SEQ_6=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_6));
	pr_info("CMD_SEQ_7=0x%x\n", WRAP_RD32(PMIC_WRAP_CMD_SEQUENCE_7));
	pr_info("WDATA_SEQ_0=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_0));
	pr_info("WDATA_SEQ_1=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_1));
	pr_info("WDATA_SEQ_2=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_2));
	pr_info("WDATA_SEQ_3=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_3));
	pr_info("WDATA_SEQ_4=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_4));
	pr_info("WDATA_SEQ_5=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_5));
	pr_info("WDATA_SEQ_6=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_6));
	pr_info("WDATA_SEQ_7=0x%x\n", WRAP_RD32(PMIC_WRAP_WDATA_SEQUENCE_7));

	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x8); /* clear log */

#ifdef PMIC_WRAP_MATCH_SUPPORT
	/* Matching mode and Stop recording after interrupt trigger */
	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x5); /* reenable */
#else
	/* Matching mode and Continue recording after interrupt trigger */
	WRAP_WR32(PMIC_WRAP_MONITOR_CTRL_0, 0x1); /* reenable */
#endif

	for (i = 0; i <= 14; i++) {
		offset = 0xc00 + i * 4;

#if (PMIC_WRAP_KERNEL) || (PMIC_WRAP_CTP)
		reg_addr = (unsigned int *) (PMIC_WRAP_BASE + offset);
		val = WRAP_RD32(reg_addr);
		pr_info("addr:0x%p = 0x%x\n", reg_addr, val);
#else
		reg_addr = (PMIC_WRAP_BASE + offset);
		val = WRAP_RD32(reg_addr);
		pr_info("addr:0x%x = 0x%x\n", reg_addr, val);
#endif
	}
	}
}

static void pwrap_reenable_pmic_logging(void)
{
	unsigned int rdata = 0, sub_return = 0;

	/* Read Last three command */
	pwrap_read_nochk(PMIC_RECORD_CMD0_ADDR, &rdata);
	pr_info("RECORD_CMD0:  0x%x (Last 1st cmd addr)\n", (rdata & 0x3fff));
	pwrap_read_nochk(PMIC_RECORD_WDATA0_ADDR, &rdata);
	pr_info("RECORD_WDATA0:0x%x (Last 1st cmd wdata)\n", rdata);
	pwrap_read_nochk(PMIC_RECORD_CMD1_ADDR, &rdata);
	pr_info("RECORD_CMD1:  0x%x (Last 2nd cmd addr)\n", (rdata & 0x3fff));
	pwrap_read_nochk(PMIC_RECORD_WDATA1_ADDR, &rdata);
	pr_info("RECORD_WDATA1:0x%x (Last 2nd cmd wdata)\n", rdata);
	pwrap_read_nochk(PMIC_RECORD_CMD2_ADDR, &rdata);
	pr_info("RECORD_CMD2:  0x%x (Last 3rd cmd addr)\n", (rdata & 0x3fff));
	pwrap_read_nochk(PMIC_RECORD_WDATA2_ADDR, &rdata);
	pr_info("RECORD_WDATA2:0x%x (Last 3rd cmd wdata)\n", rdata);

	/* Enable Command Recording */
	sub_return = pwrap_write_nochk(PMIC_RG_SPI_RSV_ADDR, 0x3);
	if (sub_return != 0)
		pr_notice("enable spi debug fail, ret=%x\n", sub_return);
	pr_info("enable spi debug ok\n");

	/* Clear Last three record command */
	sub_return = pwrap_write_nochk(PMIC_RG_SPI_RECORD_CLR_ADDR, 0x1);
	if (sub_return != 0)
		pr_notice("clear record command fail, ret=%x\n", sub_return);
	sub_return = pwrap_write_nochk(PMIC_RG_SPI_RECORD_CLR_ADDR, 0x0);
	if (sub_return != 0)
		pr_notice("clear record command fail, ret=%x\n", sub_return);
	pr_info("clear record command ok\n\r");

}

void pwrap_dump_and_recovery(void)
{
	pr_info("pwrap_dump_and_recovery start!!!!!!!!!!!!!\n");
	pwrap_dump_ap_register();
	pwrap_dump_pmic_register();
	pr_info("pwrap_dump_and_recovery end!!!!!!!!!!!!!\n");
}

void pwrap_dump_all_register(void)
{
	unsigned int tsx_0 = 0, tsx_1 = 0, dcxo_0 = 0, dcxo_1 = 0;

	/* add tsx/dcxo temperture log support */
	tsx_0 = WRAP_RD32(PMIC_WRAP_MD_ADCINF_0_STA_0);
	pr_notice("tsx dump reg_addr:0x1000d288 = 0x%x\n", tsx_0);
	tsx_1 = WRAP_RD32(PMIC_WRAP_MD_ADCINF_0_STA_1);
	pr_notice("tsx dump reg_addr:0x1000d28C = 0x%x\n", tsx_1);
	dcxo_0 = WRAP_RD32(PMIC_WRAP_MD_ADCINF_1_STA_0);
	pr_notice("tsx dump reg_addr:0x1000d290 = 0x%x\n", dcxo_0);
	dcxo_1 = WRAP_RD32(PMIC_WRAP_MD_ADCINF_1_STA_1);
	pr_notice("tsx dump reg_addr:0x1000d294 = 0x%x\n", dcxo_1);
}

static int is_pwrap_init_done(void)
{
	int ret = 0;

	ret = WRAP_RD32(PMIC_WRAP_INIT_DONE2);
	pr_info("is_pwrap_init_done %d\n", ret);
	if ((ret & 0x1) == 1)
		return 0;

	ret = pwrap_init();
	if (ret != 0) {
		pr_err("init error (%d)\n", ret);
		pwrap_dump_all_register();
		return ret;
	}
	pr_info("init successfully done (%d)\n\n", ret);
	return ret;
}

/*---------------------------------------------------------------------------*/

#ifdef CONFIG_OF
static int pwrap_of_iomap(void)
{
	/*
	 * Map the address of the following register base:
	 * INFRACFG_AO, TOPCKGEN, SCP_CLK_CTRL, SCP_PMICWP2P
	 */

	struct device_node *infracfg_ao_node;
	struct device_node *topckgen_node;

	infracfg_ao_node = of_find_compatible_node(NULL, NULL,
						   "mediatek,infracfg_ao");
	if (!infracfg_ao_node) {
		pr_err("get INFRACFG_AO failed\n");
		return -ENODEV;
	}

	infracfg_ao_base = of_iomap(infracfg_ao_node, 0);
	if (!infracfg_ao_base) {
		pr_err("INFRACFG_AO iomap failed\n");
		return -ENOMEM;
	}

	topckgen_node = of_find_compatible_node(NULL, NULL,
						"mediatek,topckgen");
	if (!topckgen_node) {
		pr_err("get TOPCKGEN failed\n");
		return -ENODEV;
	}

	topckgen_base = of_iomap(topckgen_node, 0);
	if (!topckgen_base) {
		pr_err("TOPCKGEN iomap failed\n");
		return -ENOMEM;
	}
	return 0;
}

static void pwrap_of_iounmap(void)
{
	iounmap(topckgen_base);
}
#endif




#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
/* pmic wrap used 4*4Byte IPI data buf,
 * buf0 - adr,
 * buf1 - bit0: 0 read,  1 write; bit1: 0 pmic_wrap, 1 pmic.
 * buf3 - rdata, buf4 reserved
 */
static signed int pwrap_wacs2_ipi(unsigned int  adr, unsigned int wdata,
					 unsigned int flag)
{
	int ipi_data_ret = 0, err;
	unsigned int ipi_buf[32];

	/* mutex_lock(&pwrap_lock); */
	ipi_buf[0] = adr;
	ipi_buf[1] = flag;
	ipi_buf[2] = wdata;

	err = sspm_ipi_send_sync_new(IPI_ID_PMIC_WRAP, IPI_OPT_POLLING,
				     (void *)ipi_buf, 3, &ipi_data_ret, 1);
	if (err != 0)
		pr_err("ipi_write error: %d\n", err);
	else
		pr_info("ipi_write success: %x\n", ipi_data_ret);

	/* mutex_unlock(&pwrap_lock); */
	return 0;
}

static int pwrap_ipi_register(void)
{
	int ret;
	int retry = 0;
	ipi_action_t pwrap_isr;

	pwrap_isr.data = (void *)pwrap_recv_data;

	do {
		retry++;
		ret = sspm_ipi_recv_registration(IPI_ID_PMIC_WRAP, &pwrap_isr);
	} while ((ret != 0) && (retry < 10));
	if (retry >= 10)
		pr_err("pwrap_ipi_register fail\n");
	return 0;
}
#endif

#define WK_MONITOR_VCORE_HWCFG		1
/* Interrupt handler function */
static int g_wrap_wdt_irq_count;
static int g_case_flag;
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned int int0_flg = 0, int1_flg = 0, ret = 0;
	unsigned char str[50] = "";
#if WK_MONITOR_VCORE_HWCFG
	unsigned int rdata = 0;
#endif
	int0_flg = WRAP_RD32(PMIC_WRAP_INT0_FLG);
	int1_flg = WRAP_RD32(PMIC_WRAP_INT1_FLG);

	if ((int0_flg & 0xffffffff) != 0) {
		pr_notice("[PWRAP] INT0 error = 0x%x\n", int0_flg);
		pwrap_dump_all_register();
		WRAP_WR32(PMIC_WRAP_INT0_CLR, 0xffffffff);
#if 0
		/* trigger MD ASSERT when CRC fail*/
		if ((int0_flg & 0x02) == 0x02) {
			exec_ccci_kern_func_by_md_id(MD_SYS1,
				ID_FORCE_MD_ASSERT, NULL, 0);
		}
#endif
	}

#if WK_MONITOR_VCORE_HWCFG
	if ((int1_flg & 0x2000) == 0x2000) {
		pr_notice("[PWRAP] Monitor catch a target transaction\n");
		pr_notice("[PWRAP]PMIC_WRAP_INT1_FLG:0x%x(before)\n", int1_flg);
		pwrap_logging_at_isr();
		pwrap_reenable_pmic_logging();
		pwrap_dump_ap_register();
		WRAP_WR32(PMIC_WRAP_INT1_CLR, 0x2000);
		pr_notice("[PWRAP]PMIC_WRAP_INT1_FLG:0x%x(after)\n", int1_flg);

		pwrap_read_nochk(PMIC_RG_BUCK_VCORE_HW0_OP_CFG_ADDR, &rdata);
		pr_notice("[PWRAP]BUCK_VCORE_HW0_OP_CFG=0x%x(before)\n", rdata);
		pwrap_write_nochk(PMIC_RG_BUCK_VCORE_HW0_OP_CFG_ADDR
				  , rdata | 0x2);
		pwrap_read_nochk(PMIC_RG_BUCK_VCORE_HW0_OP_CFG_ADDR, &rdata);
		pr_notice("[PWRAP]BUCK_VCORE_HW0_OP_CFG=0x%x(after)\n", rdata);
	}
#endif

	if ((int1_flg & 0xffffffff) != 0) {
		pr_notice("[PWRAP] INT1 error = 0x%x\n", int1_flg);
		pwrap_dump_all_register();
		WRAP_WR32(PMIC_WRAP_INT1_CLR, 0xffffffff);
	}

	if ((int0_flg & 0x01) == 0x01) {
		g_wrap_wdt_irq_count++;
		g_case_flag = 0;
		pr_notice("g_wrap_wdt_irq_count = %d\n", g_wrap_wdt_irq_count);

	} else if ((int0_flg & 0x02) == 0x02) {
		snprintf(str, 50, "[PWRAP] CRC = 0x%x",
			WRAP_RD32(PMIC_WRAP_SIG_ERRVAL));
		aee_kernel_warning(str, str);
		pwrap_logging_at_isr();
		pwrap_reenable_pmic_logging();
		WRAP_WR32(PMIC_WRAP_INT0_EN, 0xffffffff);

		/* Clear spislv CRC sta */
		ret = pwrap_write_nochk(PMIC_DEW_CRC_SWRST_ADDR, 0x1);
		if (ret != 0)
			pr_notice("clear crc fail, ret=%x\n", ret);
		ret = pwrap_write_nochk(PMIC_DEW_CRC_SWRST_ADDR, 0x0);
		if (ret != 0)
			pr_notice("clear crc fail, ret=%x\n", ret);
		ret = pwrap_write_nochk(PMIC_DEW_CRC_EN_ADDR, 0x0);
		if (ret != 0)
			pr_notice("enable crc fail, ret=%x\n", ret);
		WRAP_WR32(PMIC_WRAP_CRC_EN, 0x0);
		WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xf5);
	} else {
		g_case_flag = 1;
	}

	if (g_wrap_wdt_irq_count == 10 || g_case_flag == 1)
		WARN_ON(1);

	return IRQ_HANDLED;

}

static void pwrap_int_test(void)
{
	unsigned int rdata1 = 0;
	unsigned int rdata2 = 0;

	while (1) {
		rdata1 = WRAP_RD32(PMIC_WRAP_EINT_STA);
		pwrap_read(PMIC_CPU_INT_STA_ADDR, &rdata2);
		pr_info("Pwrap INT status check\n");
		pr_info("PMIC_WRAP_EINT_STA=0x%x\n", rdata1);
		pr_info("INT_STA[0x01B4]=0x%x\n", rdata2);
		msleep(500);
	}
}

/*---------------------------------------------------------------------------*/
static signed int mt_pwrap_show_hal(char *buf)
{
	pr_notice(PWRAPTAG "cpuid=%d,%s\n", raw_smp_processor_id(), __func__);
	return snprintf(buf, PAGE_SIZE, "%s\n", "no implement");
}

/*---------------------------------------------------------------------------*/
static signed int mt_pwrap_store_hal(const char *buf, size_t count)
{
	unsigned int reg_value = 0;
	unsigned int reg_addr = 0;
	unsigned int return_value = 0;
	unsigned int ut_test = 0;

	if (!strncmp(buf, "-h", 2)) {
		pr_info("PWRAP debug:[-dump_reg][-trace_wacs2][-init][-rdap]");
		pr_info("[-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		pr_info("PWRAP UT: [1][2]\n");
	} else if (!strncmp(buf, "-dump_reg", 9)) {
		pwrap_dump_all_register();
	} else if (!strncmp(buf, "-trace_wacs2", 12)) {
		/* pwrap_trace_wacs2(); */
	} else if (!strncmp(buf, "-init", 5)) {
		return_value = pwrap_init();
		if (return_value == 0)
			pr_info("pwrap_init pass,return_value=%d\n",
					return_value);
		else
			pr_info("pwrap_init fail,return_value=%d\n",
					return_value);
	} else if (!strncmp(buf, "-rdap", 5) &&
				(sscanf(buf + 5, "%x", &reg_addr) == 1)) {
		/* pwrap_read_reg_on_ap(reg_addr); */
	} else if (!strncmp(buf, "-wrap", 5)
		   && (sscanf(buf + 5, "%x %x", &reg_addr, &reg_value) == 2)) {
		/* pwrap_write_reg_on_ap(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-rdpmic", 7) &&
				(sscanf(buf + 7, "%x", &reg_addr) == 1)) {
		/* pwrap_read_reg_on_pmic(reg_addr); */
	} else if (!strncmp(buf, "-wrpmic", 7)
		   && (sscanf(buf + 7, "%x %x", &reg_addr, &reg_value) == 2)) {
		/* pwrap_write_reg_on_pmic(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-readtest", 9)) {
		pwrap_read_test();
	} else if (!strncmp(buf, "-writetest", 10)) {
		pwrap_write_test();
	} else if (!strncmp(buf, "-int", 4)) {
		pwrap_int_test();
	} else if (!strncmp(buf, "-ut", 3) &&
				(sscanf(buf + 3, "%d", &ut_test) == 1)) {
		pwrap_ut(ut_test);
	} else {
		pr_info("wrong parameter\n");
	}
	return count;
}

static int __init pwrap_hal_init(void)
{
	signed int ret = 0;
#ifdef CONFIG_OF
	unsigned int pwrap_irq;
	struct device_node *pwrap_node;

	pr_info("mt_pwrap_init++++\n");
	pwrap_node = of_find_compatible_node(NULL, NULL, "mediatek,pwrap");
	if (!pwrap_node) {
		pr_err("PWRAP get node failed\n");
		return -ENODEV;
	}

	pwrap_base = of_iomap(pwrap_node, 0);
	if (!pwrap_base) {
		pr_err("PWRAP iomap failed\n");
		return -ENOMEM;
	}

	pwrap_irq = irq_of_parse_and_map(pwrap_node, 0);
	if (!pwrap_irq) {
		pr_err("PWRAP get irq fail\n");
		return -ENODEV;
	}
	pr_info("PWRAP reg: 0x%p,  irq: %d\n", pwrap_base, pwrap_irq);
#endif
	mt_wrp = get_mt_pmic_wrap_drv();
	mt_wrp->store_hal = mt_pwrap_store_hal;
	mt_wrp->show_hal = mt_pwrap_show_hal;
	mt_wrp->wacs2_hal = pwrap_wacs2_hal;

	pwrap_of_iomap();
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		pwrap_ipi_register();
#endif

	if (is_pwrap_init_done() == 0) {
#ifdef PMIC_WRAP_NO_PMIC
#else
		ret =
			request_irq(MT_PMIC_WRAP_IRQ_ID, mt_pmic_wrap_irq,
					IRQF_TRIGGER_HIGH, PMIC_WRAP_DEVICE, 0);
#endif
		if (ret) {
			pr_err("register IRQ failed (%d)\n", ret);
			return ret;
		}
	} else {
		pr_err("not init (%d)\n", ret);
	}

	pr_info("mt_pwrap_init----\n");
	return ret;
}

/******************************************************************************/
/* extern API for PMIC driver, INT related control (for PMIC chip to AP) */
/******************************************************************************/
#endif /* PMIC_WRAP_NO_PMIC */
unsigned int mt_pmic_wrap_eint_status(void)
{
	return WRAP_RD32(PMIC_WRAP_EINT_STA);
}

void mt_pmic_wrap_eint_clr(int offset)
{
	if ((offset < 0) || (offset > 3))
		pr_err("clear EINT flag error, only 0-3 bit\n");
	else
		WRAP_WR32(PMIC_WRAP_EINT_CLR, (1 << offset));
}

postcore_initcall(pwrap_hal_init);
