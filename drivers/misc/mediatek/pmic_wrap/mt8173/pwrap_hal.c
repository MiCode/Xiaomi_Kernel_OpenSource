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

/******************************************************************************
 * pwrap_hal.c - Linux pmic_wrapper Driver,hardware_dependent driver
 *
 *
 * DESCRIPTION:
 *     This file provid the other drivers PMIC wrapper relative functions
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
#include <linux/clk.h>
#endif
#include <mt_pmic_wrap.h>
#include "pwrap_hal.h"
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/module.h>

#define PMIC_WRAP_DEVICE "pmic_wrap"
/* #define PMIC_WRAP_NO_PMIC */

/*-----start-- global variable-------------------------------------------------*/
#ifdef CONFIG_OF
void __iomem *pwrap_base;
static void __iomem *topckgen_base;
void __iomem *infracfg_ao_base;
u32 pwrap_irq;
#endif
#ifdef CONFIG_OF
#define CKSYS_BASE	(topckgen_base)
#endif

#define CLK_CFG_4_CLR                       (CKSYS_BASE+0x088)	/* 8173 */
#define CLK_CFG_5_CLR                       (CKSYS_BASE+0x098)	/* 8173 */
#define CLK_SPI_CK_26M	0x7

static struct mt_pmic_wrap_driver *mt_wrp;

static spinlock_t wrp_lock = __SPIN_LOCK_UNLOCKED(lock);
/* spinlock_t    wrp_lock = __SPIN_LOCK_UNLOCKED(lock); */
/* ----------interral API ------------------------ */
static s32 _pwrap_init_dio(u32 dio_en);
static s32 _pwrap_init_cipher(void);
static s32 _pwrap_init_reg_clock(u32 regck_sel);

static s32 _pwrap_wacs2_nochk(u32 write, u32 adr, u32 wdata, u32 *rdata);
static s32 pwrap_write_nochk(u32 adr, u32 wdata);
static s32 pwrap_read_nochk(u32 adr, u32 *rdata);
struct pmic_wrapper {
	struct device *dev;
	struct regmap *regmap;
	struct clk *clk_spi;
	struct clk *clk_wrap;
};
#ifdef CONFIG_OF
static int pwrap_of_iomap(void);
static void pwrap_of_iounmap(void);
#endif

#ifdef PMIC_WRAP_NO_PMIC
/*-pwrap debug--------------------------------------------------------------------------*/
static inline void pwrap_dump_all_register(void)
{

}

/********************************************************************************************/
/* extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP */
/********************************************************************************************/
/* -------------------------------------------------------- */
/* Function : pwrap_wacs2_hal() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 pwrap_wacs2_hal(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	PWRAPERR("there is no PMIC real chip,PMIC_WRAP do Nothing\n");
	return 0;
}

/*
 *pmic_wrap init,init wrap interface
 *
 */
s32 pwrap_init(void)
{
	return 0;
}

/* EXPORT_SYMBOL(pwrap_init); */
/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned long flags = 0;

	PWRAPFUC();
	PWRAPREG("dump pwrap register\n");
	spin_lock_irqsave(&wrp_lock, flags);
	/* *----------------------------------------------------------------------- */
	pwrap_dump_all_register();
	/* raise the priority of WACS2 for AP */
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, 1 << 3);

	/* *----------------------------------------------------------------------- */
	/* clear interrupt flag */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);
	PWRAPREG("INT flag 0x%x\n", WRAP_RD32(PMIC_WRAP_INT_EN));
	/* BUG_ON(1); */
	spin_unlock_irqrestore(&wrp_lock, flags);
	return IRQ_HANDLED;
}

static u32 pwrap_read_test(void)
{
	return 0;
}

static u32 pwrap_write_test(void)
{
	return 0;
}

static void pwrap_ut(u32 ut_test)
{
	switch (ut_test) {
	case 1:
		/* pwrap_wacs2_para_test(); */
		pwrap_write_test();
		break;
	case 2:
		/* pwrap_wacs2_para_test(); */
		pwrap_read_test();
		break;

	default:
		PWRAPREG("default test.\n");
		break;
	}
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_show_hal(char *buf)
{
	PWRAPFUC();
	return snprintf(buf, PAGE_SIZE, "%s\n", "no implement");
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_store_hal(const char *buf, size_t count)
{
	u32 reg_value = 0;
	u32 reg_addr = 0;
	u32 return_value = 0;
	u32 ut_test = 0;

	if (!strncmp(buf, "-h", 2)) {
		PWRAPREG("PWRAP debug: [-dump_reg][-trace_wacs2][-init]");
		PWRAPREG("[-rdap][-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	}
	/* ------------------pwrap debug------------------------- */
	else if (!strncmp(buf, "-dump_reg", 9)) {
		pwrap_dump_all_register();
	} else if (!strncmp(buf, "-trace_wacs2", 12)) {
		/* pwrap_trace_wacs2(); */
	} else if (!strncmp(buf, "-init", 5)) {
		return_value = pwrap_init();
		if (return_value == 0)
			PWRAPREG("pwrap_init pass,return_value=%d\n", return_value);
		else
			PWRAPREG("pwrap_init fail,return_value=%d\n", return_value);
	} else if (!strncmp(buf, "-rdap", 5) && (1 == sscanf(buf + 5, "%x", &reg_addr))) {
		/* pwrap_read_reg_on_ap(reg_addr); */
	} else if (!strncmp(buf, "-wrap", 5)
		   && (2 == sscanf(buf + 5, "%x %x", &reg_addr, &reg_value))) {
		/* pwrap_write_reg_on_ap(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf + 7, "%x", &reg_addr))) {
		/* pwrap_read_reg_on_pmic(reg_addr); */
	} else if (!strncmp(buf, "-wrpmic", 7)
		   && (2 == sscanf(buf + 7, "%x %x", &reg_addr, &reg_value))) {
		/* pwrap_write_reg_on_pmic(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-readtest", 9)) {
		pwrap_read_test();
	} else if (!strncmp(buf, "-writetest", 10)) {
		pwrap_write_test();
	}
	/* ----------------pwrap UT-------------------- */
	else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf + 3, "%d", &ut_test))) {
		pwrap_ut(ut_test);
	} else {
		PWRAPREG("wrong parameter\n");
	}
	return count;
}

/*---------------------------------------------------------------------------*/
#else
/*-pwrap debug--------------------------------------------------------------------------*/
static inline void pwrap_dump_ap_register(void)
{
	u32 i = 0;

	PWRAPREG("dump pwrap register, base=0x%p\n", PMIC_WRAP_BASE);
	PWRAPREG("address     :   3 2 1 0    7 6 5 4    B A 9 8    F E D C\n");
	for (i = 0; i <= 0x150; i += 16) {
		PWRAPREG("offset 0x%.3x:0x%.8x 0x%.8x 0x%.8x 0x%.8x\n", i,
			 WRAP_RD32(PMIC_WRAP_BASE + i + 0),
			 WRAP_RD32(PMIC_WRAP_BASE + i + 4),
			 WRAP_RD32(PMIC_WRAP_BASE + i + 8), WRAP_RD32(PMIC_WRAP_BASE + i + 12));
	}
}

static inline void pwrap_dump_pmic_register(void)
{
	/* u32 i=0; */
	/* u32 reg_addr=0; */
	/* u32 reg_value=0; */
	/*  */
	/* PWRAPREG("dump dewrap register\n"); */
	/* for(i=0;i<=14;i++) */
	/* { */
	/* reg_addr=(DEW_BASE+i*4); */
	/* reg_value=pwrap_read_nochk(reg_addr,&reg_value); */
	/* PWRAPREG("0x%x=0x%x\n",reg_addr,reg_value); */
	/* } */
}

static inline void pwrap_dump_all_register(void)
{
	pwrap_dump_ap_register();
	pwrap_dump_pmic_register();
}

static void __pwrap_soft_reset(void)
{
	PWRAPLOG("start reset wrapper\n");
	PWRAP_SOFT_RESET;
	PWRAPLOG("the reset register =%x\n", WRAP_RD32(INFRA_GLOBALCON_RST0));
	PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =0x%x,it should be equal to 0xc\n",
		 WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN));
	/* clear reset bit */
	PWRAP_CLEAR_SOFT_RESET_BIT;
}


/******************************************************************************
  wrapper timeout
 ******************************************************************************/
#define PWRAP_TIMEOUT
#ifdef PWRAP_TIMEOUT
static u64 _pwrap_get_current_time(void)
{
	return sched_clock();	/* /TODO: fix me */
}

/* u64 elapse_time=0; */

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 timeout_time_ns)
{
	u64 cur_time = 0;
	u64 elapse_time = 0;

	/* get current tick */
	cur_time = _pwrap_get_current_time();	/* ns */

	/* avoid timer over flow exiting in FPGA env */
	if (cur_time < start_time_ns) {
		PWRAPERR("@@@@Timer overflow! start%lld cur timer%lld\n", start_time_ns, cur_time);
		start_time_ns = cur_time;
		timeout_time_ns = 255 * 1000;	/* 255us */
		PWRAPERR("@@@@reset timer! start%lld setting%lld\n", start_time_ns,
			 timeout_time_ns);
	}

	elapse_time = cur_time - start_time_ns;

	/* check if timeout */
	if (timeout_time_ns <= elapse_time) {
		/* timeout */
		PWRAPERR("@@@@Timeout: elapse time%lld,start%lld setting timer%lld\n",
			 elapse_time, start_time_ns, timeout_time_ns);
		return true;
	}
	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return time_us * 1000;
}

#else
static u64 _pwrap_get_current_time(void)
{
	return 0;
}

static bool _pwrap_timeout_ns(u64 start_time_ns, u64 elapse_time)
{				/* ,u64 timeout_ns) */
	return false;
}

static u64 _pwrap_time2ns(u64 time_us)
{
	return 0;
}

#endif
/* ##################################################################### */
/* define macro and inline function (for do while loop) */
/* ##################################################################### */
typedef u32(*loop_condition_fp) (u32);	/* define a function pointer */

static inline u32 wait_for_fsm_idle(u32 x)
{
	return (GET_WACS0_FSM(x) != WACS_FSM_IDLE);
}

static inline u32 wait_for_fsm_vldclr(u32 x)
{
	return (GET_WACS0_FSM(x) != WACS_FSM_WFVLDCLR);
}

static inline u32 wait_for_sync(u32 x)
{
	return (GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}

static inline u32 wait_for_idle_and_sync(u32 x)
{
	return ((GET_WACS2_FSM(x) != WACS_FSM_IDLE) || (GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE));
}

static inline u32 wait_for_wrap_idle(u32 x)
{
	return ((GET_WRAP_FSM(x) != 0x0) || (GET_WRAP_CH_DLE_RESTCNT(x) != 0x0));
}

static inline u32 wait_for_wrap_state_idle(u32 x)
{
	return (GET_WRAP_AG_DLE_RESTCNT(x) != 0);
}

static inline u32 wait_for_man_idle_and_noreq(u32 x)
{
	return ((GET_MAN_REQ(x) != MAN_FSM_NO_REQ) || (GET_MAN_FSM(x) != MAN_FSM_IDLE));
}

static inline u32 wait_for_man_vldclr(u32 x)
{
	return (GET_MAN_FSM(x) != MAN_FSM_WFVLDCLR);
}

static inline u32 wait_for_cipher_ready(u32 x)
{
	return (x != 3);
}

static inline u32 wait_for_stdupd_idle(u32 x)
{
	return (GET_STAUPD_FSM(x) != 0x0);
}

static inline u32 wait_for_state_ready_init(loop_condition_fp fp, u32 timeout_us,
					    void *wacs_register, u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata = 0x0;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_ready_init timeout when waiting for idle\n");
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline u32 wait_for_state_idle_init(loop_condition_fp fp, u32 timeout_us,
					   void *wacs_register, void *wacs_vldclr_register,
					   u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle_init timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			/* BUG_ON(1); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

static inline u32 wait_for_state_idle(loop_condition_fp fp, u32 timeout_us, void *wacs_register,
				      void *wacs_vldclr_register, u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait_for_state_idle timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			/* BUG_ON(1); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
		/* if last read command timeout,clear vldclr bit */
		/* read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle */
		switch (GET_WACS0_FSM(reg_rdata)) {
		case WACS_FSM_WFVLDCLR:
			WRAP_WR32(wacs_vldclr_register, 1);
			PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
			break;
		case WACS_FSM_WFDLE:
			PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
			break;
		case WACS_FSM_REQ:
			PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
			break;
		default:
			break;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

/********************************************************************************************/
/* extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP */
/********************************************************************************************/
static inline u32 wait_for_state_ready(loop_condition_fp fp, u32 timeout_us, void *wacs_register,
				       u32 *read_reg)
{

	u64 start_time_ns = 0, timeout_ns = 0;
	u32 reg_rdata;

	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do {
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			/* pwrap_trace_wacs2(); */
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);

		if (GET_INIT_DONE0(reg_rdata) != WACS_INIT_DONE) {
			PWRAPERR("initialization isn't finished\n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while (fp(reg_rdata));	/* IDLE State */
	if (read_reg)
		*read_reg = reg_rdata;
	return 0;
}

/*
 * extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP (72~92 no need this)
 */
u32 mt_pmic_wrap_eint_status(void)
{
	return 0;
}

void mt_pmic_wrap_eint_clr(int offset)
{

}

/* -------------------------------------------------------- */
/* Function : pwrap_wacs2_hal() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 pwrap_wacs2_hal(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	/* u64 wrap_access_time=0x0; */
	u32 reg_rdata = 0;
	u32 wacs_write = 0;
	u32 wacs_adr = 0;
	u32 wacs_cmd = 0;
	u32 return_value = 0;
	unsigned long flags = 0;
	/* PWRAPFUC(); */
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
	    wait_for_state_idle(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
				PMIC_WRAP_WACS2_VLDCLR, 0);
	if (return_value != 0) {
		PWRAPERR("wait_for_fsm_idle fail,return_value=%d\n", return_value);
		goto FAIL;
	}
	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);
	if (write == 0) {
		if (NULL == rdata) {
			PWRAPERR("rdata is a NULL pointer\n");
			return_value = E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value =
		    wait_for_state_ready(wait_for_fsm_vldclr, TIMEOUT_READ, PMIC_WRAP_WACS2_RDATA,
					 &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("wait_for_fsm_vldclr fail,return_value=%d\n", return_value);
			return_value += 1;	/* E_PWR_NOT_INIT_DONE_READ or E_PWR_WAIT_IDLE_TIMEOUT_READ */
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}
	/* spin_unlock_irqrestore(&wrp_lock,flags); */
 FAIL:
	spin_unlock_irqrestore(&wrp_lock, flags);
	if (return_value != 0) {
		PWRAPERR("pwrap_wacs2_hal fail,return_value=%d\n", return_value);
		PWRAPERR("timeout:BUG_ON here\n");
		/* BUG_ON(1); */
	}
	/* wrap_access_time=sched_clock(); */
	/* pwrap_trace(wrap_access_time,return_value,write, adr, wdata,(u32)rdata); */
	return return_value;
}

/* ****************************************************************************** */
/* --internal API for pwrap_init------------------------------------------------- */
/* ****************************************************************************** */
/* -------------------------------------------------------- */
/* Function : _pwrap_wacs2_nochk() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */

/* static s32 pwrap_read_nochk( u32  adr, u32 *rdata ) */
static s32 pwrap_read_nochk(u32 adr, u32 *rdata)
{
	return _pwrap_wacs2_nochk(0, adr, 0, rdata);
}

static s32 pwrap_write_nochk(u32 adr, u32 wdata)
{
	return _pwrap_wacs2_nochk(1, adr, wdata, 0);
}

static s32 _pwrap_wacs2_nochk(u32 write, u32 adr, u32 wdata, u32 *rdata)
{
	u32 reg_rdata = 0x0;
	u32 wacs_write = 0x0;
	u32 wacs_adr = 0x0;
	u32 wacs_cmd = 0x0;
	u32 return_value = 0x0;
	/* PWRAPFUC(); */
	/* Check argument validation */
	if ((write & ~(0x1)) != 0)
		return E_PWR_INVALID_RW;
	if ((adr & ~(0xffff)) != 0)
		return E_PWR_INVALID_ADDR;
	if ((wdata & ~(0xffff)) != 0)
		return E_PWR_INVALID_WDAT;

	/* Check IDLE */
	return_value =
	    wait_for_state_ready_init(wait_for_fsm_idle, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA,
				      0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_wacs2_nochk write command fail,return_value=%x\n", return_value);
		return return_value;
	}

	wacs_write = write << 31;
	wacs_adr = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD, wacs_cmd);

	if (write == 0) {
		if (NULL == rdata)
			return E_PWR_INVALID_ARG;
		/* wait for read data ready */
		return_value =
		    wait_for_state_ready_init(wait_for_fsm_vldclr, TIMEOUT_WAIT_IDLE,
					      PMIC_WRAP_WACS2_RDATA, &reg_rdata);
		if (return_value != 0) {
			PWRAPERR("_pwrap_wacs2_nochk read fail,return_value=%x\n", return_value);
			return return_value;
		}
		*rdata = GET_WACS0_RDATA(reg_rdata);
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR, 1);
	}
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_dio() */
/* Description :call it in pwrap_init,mustn't check init done */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_dio(u32 dio_en)
{
	u32 arb_en_backup = 0x0;
	u32 rdata = 0x0;
	u32 return_value = 0;

	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */
	pwrap_write_nochk(DEW_DIO_EN, dio_en);

	/* Check IDLE & INIT_DONE in advance */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_init_dio fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_DIO_EN, dio_en);
	/* Read Test */
	pwrap_read_nochk(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0x5aa5\n",
			 dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	}
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_cipher() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_cipher(void)
{
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	u32 return_value = 0;
	u64 start_time_ns = 0, timeout_ns = 0;
	/* PWRAPFUC(); */
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS0 */

	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST, 0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL, 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL, 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN, 1);

	/* Config CIPHER @ PMIC */
	pwrap_write_nochk(DEW_CIPHER_SWRST, 0x1);
	pwrap_write_nochk(DEW_CIPHER_SWRST, 0x0);
	pwrap_write_nochk(DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(DEW_CIPHER_IV_SEL, 0x2);
	pwrap_write_nochk(DEW_CIPHER_LOAD, 0x1);
	pwrap_write_nochk(DEW_CIPHER_START, 0x1);

	/* wait for cipher data ready@AP */
	return_value =
	    wait_for_state_ready_init(wait_for_cipher_ready, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_CIPHER_RDY, 0);
	if (return_value != 0) {
		PWRAPERR("wait for cipher data ready@AP fail,return_value=%x\n", return_value);
		return return_value;
	}
	/* wait for cipher data ready@PMIC */
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(TIMEOUT_WAIT_IDLE);
	do {
		pwrap_read_nochk(DEW_CIPHER_RDY, &rdata);
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns)) {
			PWRAPERR("wait for cipher data ready:timeout when waiting for idle\n");
#ifdef PWRAP_EARLY_PORTING
			/* no return statement, Has same issue on MT6589/MT8135 FPGA(early-porting) */
#else
			/* return E_PWR_WAIT_IDLE_TIMEOUT; */
#endif
		}
	} while (rdata != 0x1);	/* cipher_ready */

	pwrap_write_nochk(DEW_CIPHER_MODE, 0x1);

	/* wait for cipher mode idle */
	return_value =
	    wait_for_state_ready_init(wait_for_idle_and_sync, TIMEOUT_WAIT_IDLE,
				      PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("wait for cipher mode idle fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE, 1);

	/* Read Test */
	pwrap_read_nochk(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	}


	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_init_sistrobe() */
/* Description : Initialize SI_CK_CON and SIDLY */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */
static s32 _pwrap_init_sistrobe(void)
{
	u32 arb_en_backup = 0;
	u32 rdata = 0;
	s32 ind = 0;
	u32 tmp1 = 0;
	u32 tmp2 = 0;
	u32 result_faulty = 0;
	u32 result = 0;
	u32 leading_one, tailing_one;

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* only WACS2 */

	/* --------------------------------------------------------------------- */
	/* Scan all possible input strobe by READ_TEST */
	/* --------------------------------------------------------------------- */
	/* 24 sampling clock edge */
	for (ind = 0; ind < 24; ind++) {	/* 24 sampling clock edge */
		WRAP_WR32(PMIC_WRAP_SI_CK_CON, (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY, 0x3 - (ind & 0x3));
		_pwrap_wacs2_nochk(0, DEW_READ_TEST, 0, &rdata);
		if (rdata == DEFAULT_VALUE_READ_TEST) {
			PWRAPLOG("_pwrap_init_sistrobe [Read Test] pass,index=%d rdata=%x\n", ind,
				 rdata);
			result |= (0x1 << ind);
		} else
			PWRAPLOG("_pwrap_init_sistrobe [Read Test] fail,index=%d,rdata=%x\n", ind,
				 rdata);

	}
	/* --------------------------------------------------------------------- */
	/* Locate the leading one and trailing one of PMIC 1/2 */
	/* --------------------------------------------------------------------- */
	for (ind = 23; ind >= 0; ind--) {
		if (result & (0x1 << ind))
			break;
	}
	leading_one = ind;

	for (ind = 0; ind < 24; ind++) {
		if (result & (0x1 << ind))
			break;
	}
	tailing_one = ind;


	/* --------------------------------------------------------------------- */
	/* Check the continuity of pass range */
	/* --------------------------------------------------------------------- */
	tmp1 = (0x1 << (leading_one + 1)) - 1;
	tmp2 = (0x1 << tailing_one) - 1;
	if ((tmp1 - tmp2) != result) {
		/*TERR = "[DrvPWRAP_InitSiStrobe] Fail, tmp1:%d, tmp2:%d", tmp1, tmp2 */
		PWRAPERR("_pwrap_init_sistrobe Fail,tmp1=%x,tmp2=%x\n", tmp1, tmp2);
		result_faulty = 0x1;
	}


	/* --------------------------------------------------------------------- */
	/* Config SICK and SIDLY to the middle point of pass range */
	/* --------------------------------------------------------------------- */
	ind = (leading_one + tailing_one) / 2;
	WRAP_WR32(PMIC_WRAP_SI_CK_CON, (ind >> 2) & 0x7);
	WRAP_WR32(PMIC_WRAP_SIDLY, 0x3 - (ind & 0x3));

	/* --------------------------------------------------------------------- */
	/* Restore */
	/* --------------------------------------------------------------------- */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, arb_en_backup);
	if (result_faulty != 0) {
		/*TERR = "[DrvPWRAP_InitSiStrobe] Fail, result = %x", result */
		PWRAPERR("_pwrap_init_sistrobe Fail,result=%x\n", result);
		return result_faulty;
	}

	return 0;
}

/* -------------------------------------------------------- */
/* Function : _pwrap_reset_spislv() */
/* Description : */
/* Parameter : */
/* Return : */
/* -------------------------------------------------------- */

u32 _pwrap_reset_spislv(void)
{
	u32 ret = 0;
	u32 return_value = 0;
	/* PWRAPFUC(); */
	/* This driver does not using _pwrap_switch_mux */
	/* because the remaining requests are expected to fail anyway */

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, DISABLE_ALL);
	WRAP_WR32(PMIC_WRAP_WRAP_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, MANUAL_MODE);
	WRAP_WR32(PMIC_WRAP_MAN_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_DIO_EN, DISABLE);

	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSL << 8));	/* 0x2100 */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));	/* 0x2800//to reset counter */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_CSH << 8));	/* 0x2000 */
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD, (OP_WR << 13) | (OP_OUTS << 8));

	return_value =
	    wait_for_state_ready_init(wait_for_sync, TIMEOUT_WAIT_IDLE, PMIC_WRAP_WACS2_RDATA, 0);
	if (return_value != 0) {
		PWRAPERR("_pwrap_reset_spislv fail,return_value=%x\n", return_value);
		ret = E_PWR_TIMEOUT;
		goto timeout;
	}

	WRAP_WR32(PMIC_WRAP_MAN_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);

 timeout:
	WRAP_WR32(PMIC_WRAP_MAN_EN, DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL, WRAPPER_MODE);
	return ret;
}

static s32 _pwrap_init_reg_clock(u32 regck_sel)
{
	u32 wdata = 0;
	u32 rdata = 0;

	/* Set reg clk freq */
	pwrap_read_nochk(PMIC_TOP_CKCON2, &rdata);
	if (regck_sel == 1)
		wdata = (rdata & (~(0x3 << 10))) | (0x1 << 10);
	else
		wdata = rdata & (~(0x3 << 10));

	pwrap_write_nochk(PMIC_TOP_CKCON2, wdata);
	pwrap_read_nochk(PMIC_TOP_CKCON2, &rdata);
	if (rdata != wdata) {
		PWRAPERR("_pwrap_init_reg_clock,PMIC_TOP_CKCON2 Write [15]=1 Fail, rdata=%x\n",
			 rdata);
		return E_PWR_WRITE_TEST_FAIL;
	}
	/* Config SPI Waveform according to reg clk */
	if (regck_sel == 1) { /* 12Mhz */
	   WRAP_WR32(PMIC_WRAP_RDDMY		   , 0xc);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE    , 0x0);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_READ	   , 0x4);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_START    , 0x0);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_END	   , 0x4);
	} else if (regck_sel == 2) { /* 24Mhz */
	   WRAP_WR32(PMIC_WRAP_RDDMY		   , 0xc);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE    , 0x0);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_READ	   , 0x4);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_START    , 0x2);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_END	   , 0x2);
	} else { /*Safe mode*/
	   WRAP_WR32(PMIC_WRAP_RDDMY		   , 0xf);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE    , 0xf);
	   WRAP_WR32(PMIC_WRAP_CSHEXT_READ	   , 0xf);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_START    , 0xf);
	   WRAP_WR32(PMIC_WRAP_CSLEXT_END	   , 0xf);
	}

	return 0;
}


/*
 *pmic_wrap init,init wrap interface
 *
 */
s32 pwrap_init(void)
{
	s32 sub_return = 0;
	s32 sub_return1 = 0;
	u32 rdata = 0x0;

	PWRAPFUC();

#ifdef CONFIG_OF
	sub_return = pwrap_of_iomap();
	if (sub_return)
		return sub_return;
#endif
	/* ############################### */
	/* toggle PMIC_WRAP and pwrap_spictl reset */
	/* ############################### */
	__pwrap_soft_reset();

	/* ############################### */
	/* Set SPI_CK_freq = 26MHz */
	/* ############################### */
	WRAP_WR32(CLK_CFG_5_CLR, CLK_SPI_CK_26M);

	/* ############################### */
	/* Enable DCM */
	/* ############################### */
	/* WRAP_WR32(PMIC_WRAP_DCM_EN , ENABLE); */
	WRAP_WR32(PMIC_WRAP_DCM_EN, 3);	/* enable CRC DCM and Pwrap DCM */
	WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD, DISABLE);	/* no debounce */

	/* ############################### */
	/* Reset SPISLV */
	/* ############################### */
	sub_return = _pwrap_reset_spislv();
	if (sub_return != 0) {
		PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_RESET_SPI;
	}
	/* ############################### */
	/* Enable WACS2 */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_WRAP_EN, ENABLE);	/* enable wrap */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, WACS2);	/* Only WACS2 */
	WRAP_WR32(PMIC_WRAP_WACS2_EN, ENABLE);

	/* ############################### */
	/* Input data calibration flow; */
	/* ############################### */
	sub_return = _pwrap_init_sistrobe();
	if (sub_return != 0) {
		PWRAPERR("error,DrvPWRAP_InitSiStrobe fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_SIDLY;
	}
	/* ############################### */
	/* SPI Waveform Configuration */
	/* 0:safe mode, 1:12MHz, 2:24MHz */
	/*############################### */
	sub_return = _pwrap_init_reg_clock(2);
	if (sub_return != 0) {
		PWRAPERR("error,_pwrap_init_reg_clock fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_REG_CLOCK;
	}
	/* ############################### */
	/* Enable PMIC */
	/* (May not be necessary, depending on S/W partition) */
	/* ############################### */
	pwrap_read_nochk(PMIC_WRP_CKPDN, &rdata);
	sub_return = pwrap_write_nochk(PMIC_WRP_CKPDN, rdata & 0x50);
	sub_return1 = pwrap_write_nochk(PMIC_WRP_RST_CON, 0);	/* clear dewrap reset bit */
	if ((sub_return != 0) || (sub_return1 != 0)) {
		PWRAPERR("Enable PMIC fail, sub_return=%x sub_return1=%x\n", sub_return,
			 sub_return1);
		return E_PWR_INIT_ENABLE_PMIC;
	}

	/* ############################### */
	/* Enable DIO mode */
	/* ############################### */
	/* PMIC2 dual io not ready */
	sub_return = _pwrap_init_dio(3);
	if (sub_return != 0) {
		PWRAPERR("_pwrap_init_dio test error,error code=%x, sub_return=%x\n", 0x11,
			 sub_return);
		return E_PWR_INIT_DIO;
	}
	/* ############################### */
	/* Enable Encryption */
	/* ############################### */
	sub_return = _pwrap_init_cipher();
	if (sub_return != 0) {
		PWRAPERR("Enable Encryption fail, return=%x\n", sub_return);
		return E_PWR_INIT_CIPHER;
	}
	/* ############################### */
	/* Write test using WACS2 */
	/* ############################### */
	/* check Wtiet test default value */

	sub_return = pwrap_write_nochk(DEW_WRITE_TEST, WRITE_TEST_VALUE);
	sub_return1 = pwrap_read_nochk(DEW_WRITE_TEST, &rdata);
	if ((rdata != WRITE_TEST_VALUE) || (sub_return != 0) || (sub_return1 != 0)) {
		PWRAPERR
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}

	/* ############################### */
	/* Signature Checking - Using CRC */
	/* should be the last to modify WRITE_TEST */
	/* ############################### */
	sub_return = pwrap_write_nochk(DEW_CRC_EN, 0x1);
	if (sub_return != 0) {
		PWRAPERR("enable CRC fail,sub_return=%x\n", sub_return);
		return E_PWR_INIT_ENABLE_CRC;
	}
	WRAP_WR32(PMIC_WRAP_CRC_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x0);
	WRAP_WR32(PMIC_WRAP_SIG_ADR, DEW_CRC_VAL);

	/* ############################### */
	/* PMIC_WRAP enables */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN, 0x3f);
	WRAP_WR32(PMIC_WRAP_WACS0_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_WACS1_EN, ENABLE);
	WRAP_WR32(PMIC_WRAP_WACS2_EN, 0x1);	/* already enabled */
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x5);	/* 0x1:20us,for concurrence test,MP:0x5;  //100us */
	WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN, 0xf1);
	WRAP_WR32(PMIC_WRAP_WDT_UNIT, 0xf);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN, 0xffffffff);
	WRAP_WR32(PMIC_WRAP_TIMER_EN, 0x1);
	WRAP_WR32(PMIC_WRAP_INT_EN, 0x7ffffff9);

	/* ############################### */
	/* Initialization Done */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_INIT_DONE2, ENABLE);

	/* ############################### */
	/* TBD: Should be configured by MD MCU */
	/* ############################### */
	WRAP_WR32(PMIC_WRAP_INIT_DONE0, ENABLE);
	WRAP_WR32(PMIC_WRAP_INIT_DONE1, ENABLE);

#ifdef CONFIG_OF
/*dvt verify must mark this*/
	pwrap_of_iounmap();
#endif

	return 0;
}

/* EXPORT_SYMBOL(pwrap_init); */
/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned long flags = 0;

	PWRAPFUC();
	PWRAPREG("dump pwrap register\n");
	spin_lock_irqsave(&wrp_lock, flags);
	/* *----------------------------------------------------------------------- */

	pwrap_dump_all_register();
	/* raise the priority of WACS2 for AP */
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO, 1 << 3);


	/* *----------------------------------------------------------------------- */
	/* clear interrupt flag */
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);
	PWRAPREG("INT flag 0x%x\n", WRAP_RD32(PMIC_WRAP_INT_EN));
	/* BUG_ON(1); */
	spin_unlock_irqrestore(&wrp_lock, flags);
	return IRQ_HANDLED;
}

static u32 pwrap_read_test(void)
{
	u32 rdata = 0;
	u32 return_value = 0;
	/* Read Test */
	return_value = pwrap_read(DEW_READ_TEST, &rdata);
	if (rdata != DEFAULT_VALUE_READ_TEST) {
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,
			 return_value);
		return E_PWR_READ_TEST_FAIL;
	}

	PWRAPREG("Read Test pass,return_value=%d\n", return_value);
	return 0;
}

static u32 pwrap_write_test(void)
{
	u32 rdata = 0;
	u32 sub_return = 0;
	u32 sub_return1 = 0;
	/* ############################### */
	/* Write test using WACS2 */
	/* ############################### */
	sub_return = pwrap_write(DEW_WRITE_TEST, WRITE_TEST_VALUE);
	PWRAPREG("after pwrap_write_nochk\n");
	sub_return1 = pwrap_read(DEW_WRITE_TEST, &rdata);
	if ((rdata != WRITE_TEST_VALUE) || (sub_return != 0) || (sub_return1 != 0)) {
		PWRAPREG
		    ("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n",
		     rdata, sub_return, sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}

	PWRAPREG("write Test pass\n");
	return 0;
}

static void pwrap_ut(u32 ut_test)
{
	switch (ut_test) {
	case 1:
		/* pwrap_wacs2_para_test(); */
		pwrap_write_test();
		break;
	case 2:
		/* pwrap_wacs2_para_test(); */
		pwrap_read_test();
		break;

	default:
		PWRAPREG("default test.\n");
		break;
	}
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_show_hal(char *buf)
{
	PWRAPFUC();
	return snprintf(buf, PAGE_SIZE, "%s\n", "no implement");
}

/*---------------------------------------------------------------------------*/
static s32 mt_pwrap_store_hal(const char *buf, size_t count)
{
	u32 reg_value = 0;
	u32 reg_addr = 0;
	u32 return_value = 0;
	u32 ut_test = 0;

	if (!strncmp(buf, "-h", 2)) {
		PWRAPREG("PWRAP debug: [-dump_reg][-trace_wacs2][-init]");
		PWRAPREG("[-rdap][-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	}
	/* -------------------pwrap debug----------------------- */
	else if (!strncmp(buf, "-dump_reg", 9)) {
		pwrap_dump_all_register();
	} else if (!strncmp(buf, "-trace_wacs2", 12)) {
		/* pwrap_trace_wacs2(); */
	} else if (!strncmp(buf, "-init", 5)) {
		return_value = pwrap_init();
		if (return_value == 0)
			PWRAPREG("pwrap_init pass,return_value=%d\n", return_value);
		else
			PWRAPREG("pwrap_init fail,return_value=%d\n", return_value);
	} else if (!strncmp(buf, "-rdap", 5) && (1 == sscanf(buf + 5, "%x", &reg_addr))) {
		/* pwrap_read_reg_on_ap(reg_addr); */
	} else if (!strncmp(buf, "-wrap", 5)
		   && (2 == sscanf(buf + 5, "%x %x", &reg_addr, &reg_value))) {
		/* pwrap_write_reg_on_ap(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf + 7, "%x", &reg_addr))) {
		/* pwrap_read_reg_on_pmic(reg_addr); */
	} else if (!strncmp(buf, "-wrpmic", 7)
		   && (2 == sscanf(buf + 7, "%x %x", &reg_addr, &reg_value))) {
		/* pwrap_write_reg_on_pmic(reg_addr,reg_value); */
	} else if (!strncmp(buf, "-readtest", 9)) {
		pwrap_read_test();
	} else if (!strncmp(buf, "-writetest", 10)) {
		pwrap_write_test();
	}
	/* -------------------pwrap UT---------------------- */
	else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf + 3, "%d", &ut_test))) {
		pwrap_ut(ut_test);
	} else {
		PWRAPREG("wrong parameter\n");
	}
	return count;
}

/*---------------------------------------------------------------------------*/
#endif				/* endif PMIC_WRAP_NO_PMIC */

#ifdef CONFIG_OF
static int pwrap_of_iomap(void)
{
	/*
	 * Map the address of the following register base:
	 * INFRACFG_AO, TOPCKGEN, SCP_CLK_CTRL, SCP_PMICWP2P
	 */

	struct device_node *infracfg_ao_node;
	struct device_node *topckgen_node;

	infracfg_ao_node = of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
	if (!infracfg_ao_node) {
		pr_debug("get INFRACFG_AO failed\n");
		return -ENODEV;
	}

	infracfg_ao_base = of_iomap(infracfg_ao_node, 0);
	if (!infracfg_ao_base) {
		pr_debug("INFRACFG_AO iomap failed\n");
		return -ENOMEM;
	}

	topckgen_node = of_find_compatible_node(NULL, NULL, "mediatek,TOPCKGEN");
	if (!topckgen_node) {
		pr_debug("get TOPCKGEN failed\n");
		return -ENODEV;
	}

	topckgen_base = of_iomap(topckgen_node, 0);
	if (!topckgen_base) {
		pr_debug("TOPCKGEN iomap failed\n");
		return -ENOMEM;
	}


	pr_warn("INFRACFG_AO reg: 0x%p\n", infracfg_ao_base);
	pr_warn("TOPCKGEN reg: 0x%p\n", topckgen_base);

	return 0;
}

static void pwrap_of_iounmap(void)
{
	iounmap(infracfg_ao_base);
	iounmap(topckgen_base);
}
#endif

static int is_pwrap_init_done(void)
{
	int ret = 0;

	ret = WRAP_RD32(PMIC_WRAP_INIT_DONE2);
	if (ret != 0)
		return 0;

	ret = pwrap_init();
	if (ret != 0) {
		PWRAPERR("init error (%d)\n", ret);
		pwrap_dump_all_register();
		return ret;
	}
	PWRAPLOG("init successfully done (%d)\n\n", ret);
	return ret;
}

static int pwrap_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	return pwrap_read(reg, val);
}

static int pwrap_reg_write(void *context, unsigned int reg, unsigned int val)
{
	return pwrap_write(reg, val);
}

const struct regmap_config pwrap_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.reg_read = pwrap_reg_read,
	.reg_write = pwrap_reg_write
};

static struct of_device_id of_pwrap_match_tbl[] = {
	{ .compatible = "mediatek,mt8173-pwrap" },
	{}
};
MODULE_DEVICE_TABLE(of, of_pwrap_match_tbl);

static int pwrap_probe(struct platform_device *pdev)
{
	s32 ret = 0;

#ifdef CONFIG_OF
	struct pmic_wrapper *wrp;
	struct device_node *pwrap_node = pdev->dev.of_node;
	const struct of_device_id *of_id =
		of_match_device(of_pwrap_match_tbl, &pdev->dev);
	struct resource *res;

	if (!of_id) {
		/* We do not expect this to happen */
		dev_err(&pdev->dev, "%s: Unable to match device\n", __func__);
		return -ENODEV;
	}

	wrp = devm_kzalloc(&pdev->dev, sizeof(*wrp), GFP_KERNEL);
	if (!wrp)
		return -ENOMEM;

	wrp->dev = &pdev->dev;

	platform_set_drvdata(pdev, wrp);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pwrap-base");
	if (!res) {
		pr_debug("could not get pwrap-base resource\n");
		return -ENODEV;
	}

	pwrap_base = devm_ioremap(wrp->dev, res->start, resource_size(res));
	if (IS_ERR(pwrap_base)) {
		pr_debug("PWRAP iomap failed\n");
		return PTR_ERR(pwrap_base);
	}

	wrp->regmap = devm_regmap_init(wrp->dev, NULL, wrp, &pwrap_regmap_config);
	if (IS_ERR(wrp->regmap))
		return PTR_ERR(wrp->regmap);

	wrp->clk_spi = devm_clk_get(wrp->dev, "spi");
	if (IS_ERR(wrp->clk_spi)) {
		dev_dbg(wrp->dev, "failed to get clock: %ld\n", PTR_ERR(wrp->clk_spi));
		return PTR_ERR(wrp->clk_spi);
	}

	wrp->clk_wrap = devm_clk_get(wrp->dev, "wrap");
	if (IS_ERR(wrp->clk_wrap)) {
		dev_dbg(wrp->dev, "failed to get clock: %ld\n", PTR_ERR(wrp->clk_wrap));
		return PTR_ERR(wrp->clk_wrap);
	}

	ret = clk_prepare_enable(wrp->clk_spi);
	if (ret)
		return ret;

	ret = clk_prepare_enable(wrp->clk_wrap);
	if (ret)
		goto err_out;

	pwrap_irq = platform_get_irq(pdev, 0);

	ret = of_platform_populate(pwrap_node, NULL, NULL, wrp->dev);
	if (ret) {
		dev_dbg(wrp->dev, "failed to create child devices at %s\n",
				pwrap_node->full_name);
		return ret;
	}

	pr_debug("PWRAP reg: 0x%p,  irq: %d\n", pwrap_base, pwrap_irq);
#endif

	mt_wrp = get_mt_pmic_wrap_drv();
	mt_wrp->store_hal = mt_pwrap_store_hal;
	mt_wrp->show_hal = mt_pwrap_show_hal;
	mt_wrp->wacs2_hal = pwrap_wacs2_hal;

	if (is_pwrap_init_done() == 0) {
		ret = request_irq(MT_PMIC_WRAP_IRQ_ID, mt_pmic_wrap_irq, IRQF_TRIGGER_HIGH, PMIC_WRAP_DEVICE, 0);
		if (ret) {
			PWRAPERR("register IRQ failed (%d)\n", ret);
			return ret;
		}
	} else{
		PWRAPERR("not init (%d)\n", ret);
	}

	return 0;

#ifdef CONFIG_OF
err_out:
	clk_disable_unprepare(wrp->clk_spi);
	return ret;
#endif
}

static struct platform_driver pwrap_drv = {
	.driver = {
		.name = "mediatek,mt8173-pwrap",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_pwrap_match_tbl),
	},
	.probe = pwrap_probe,
};

static int __init pwrap_hal_init(void)
{
	pr_warn("pwrap_hal_init\n");
	return platform_driver_register(&pwrap_drv);
}
postcore_initcall(pwrap_hal_init);
