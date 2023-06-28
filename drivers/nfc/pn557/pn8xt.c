     

/*
 * Copyright (C) 2010 Trusted Logic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/******************************************************************************
 *
 *  The original Work has been changed by NXP.
 *
 *  Copyright 2013-2020 NXP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 ******************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include "../../misc/mediatek/base/power/include/clkbuf_v1/mtk_clkbuf_ctl.h"
//#include "mtk_clkbuf_ctl.h"

#include "nfc.h"
#include "pn8xt.h"

#define SIG_NFC 44
extern bool clk_buf_ctrl(enum clk_buf_id id, bool onoff);

struct pn8xt_dev {
    pn8xt_access_st_t       cur_state;
    pn8xt_pwr_scm_t         pwr_scheme;
    bool                    nfc_ven_enabled;
    bool                    spi_ven_enabled;
    long                    service_pid;
    struct semaphore        ese_access_sema;
    struct semaphore        svdd_onoff_sema;
    struct semaphore        dwp_onoff_sema;
    struct semaphore        dwp_complete_sema;
    unsigned long           dwpLinkUpdateStat;
    unsigned int            secure_timer_cnt;
    struct timer_list       secure_timer;
    struct workqueue_struct *pSecureTimerCbWq;
    struct nfc_dev          *nfc_dev;
    struct work_struct      wq_task;
};

static pn8xt_access_st_t *pn8xt_get_state(struct pn8xt_dev *pn8xt_dev)
{
    return &pn8xt_dev->cur_state;
}
static void pn8xt_update_state(struct pn8xt_dev *pn8xt_dev, pn8xt_access_st_t state, bool set)
{
    if (state) {
        if(set) {
            if(pn8xt_dev->cur_state == ST_IDLE)
                pn8xt_dev->cur_state = ST_INVALID;
            pn8xt_dev->cur_state |= state;
        } else {
            pn8xt_dev->cur_state ^= state;
            if(!pn8xt_dev->cur_state)
                pn8xt_dev->cur_state = ST_IDLE;
        }
    }
}

int get_ese_lock(struct pn8xt_dev *pn8xt_dev, int timeout)
{
    unsigned long tempJ = msecs_to_jiffies(timeout);
    if(down_timeout(&pn8xt_dev->ese_access_sema, tempJ) != 0) {
        printk("get_ese_lock: timeout cur_state = %d\n", pn8xt_dev->cur_state);
        return -EBUSY;
    }
    return 0;
}

static int signal_handler(pn8xt_access_st_t state, long nfc_pid)
{
    int sigret = 0;
    pid_t pid;
    struct siginfo sinfo;
    struct task_struct *task;

    if(nfc_pid <= 0) {
        pr_err("%s: invalid nfc service pid %ld\n", __func__, nfc_pid);
        return 0;
    }
    memset(&sinfo, 0, sizeof(struct siginfo));
    sinfo.si_signo = SIG_NFC;
    sinfo.si_code = SI_QUEUE;
    sinfo.si_int = state;
    pid = nfc_pid;

    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if(task) {
        pr_info("%s: %s\n", __func__, task->comm);
        sigret = force_sig_info(SIG_NFC, &sinfo, task);
        if(sigret < 0) {
            pr_err("%s: send_sig_info failed, sigret %d\n", __func__, sigret);
            return -1;
        }
    } else {
        pr_err("%s: finding task from PID failed\n", __func__);
        return -1;
    }
    pr_debug("%s: return successfully\n", __func__);
    return 0;
}

static int trigger_onoff(struct pn8xt_dev *pn8xt_dev, pn8xt_access_st_t state)
{
    int timeout = 4500; //timeout in ms
    unsigned long timeoutInJiffies = msecs_to_jiffies(timeout);
    struct semaphore *sema;
    pr_debug("%s: nfc service_pid: %ld\n", __func__, pn8xt_dev->service_pid);

    if(pn8xt_dev->service_pid <= 0) {
        pr_err("%s:Invalid nfc service_pid %ld\n", __func__, pn8xt_dev->service_pid);
        return 0;
    }
    if((state & ST_SPI_SVDD_SY_START) || (state & ST_SPI_SVDD_SY_END)) {
        sema = &pn8xt_dev->svdd_onoff_sema;
    } else {
        sema = &pn8xt_dev->dwp_onoff_sema;
    }
    sema_init(sema, 0);
    if (0 == signal_handler(state, pn8xt_dev->service_pid)) {
        pr_debug("%s: Waiting for protection response", __func__);
        if(down_timeout(sema, timeoutInJiffies) != 0) {
            pr_err("%s: protection: Timeout", __func__);
            return -1;
        }
        msleep(10);
        pr_debug("%s: wait protection released", __func__);
    }
    return (pn8xt_dev->dwpLinkUpdateStat == 0x00) ? 0 : -1;
}

/*
 * pn8xt_nfc_pwr() - power control/firmware download
 * @filp:    pointer to the file descriptor
 * @arg:    mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states
 * (arg = 0):FW_DL GPIO = 0, VEN GPIO = 0
 * (arg = 1):FW_DL GPIO = 0, VEN GPIO = 1
 * (arg = 2):FW_DL GPIO = 1, KEEP VEN down/up - firmware download mode
 * Return: 0 on success and error on failure
 */
static long pn8xt_nfc_pwr(struct nfc_dev *nfc_dev, unsigned long arg)
{
    bool clk_sw_ctr_ret = 0;
    struct pn8xt_dev *pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    pn8xt_access_st_t *cur_state = pn8xt_get_state(pn8xt_dev);
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    switch(arg) {
        case 0:
            /* power off */
            pr_debug("%s power off\n", __func__);
            nfc_disable_irq(nfc_dev);
            if (nfc_dev->firm_gpio) {
                if ((*cur_state & (ST_WIRED | ST_SPI | ST_SPI_PRIO))== 0){
                    pn8xt_update_state(pn8xt_dev, ST_IDLE, true);
                }
                gpio_set_value(nfc_dev->firm_gpio, 0);
            }
            pn8xt_dev->nfc_ven_enabled = false;
            /* Don't change Ven state if spi made it high */
            if ((pn8xt_dev->spi_ven_enabled == false && !(pn8xt_dev->secure_timer_cnt))
            || (pn8xt_dev->pwr_scheme == PN80T_EXT_PMU_SCM)) {
                gpio_set_value(nfc_dev->ven_gpio, 0);
            }

            if(strstr(saved_command_line,"androidboot.hw_id=98029_1_90")||strstr(saved_command_line,"androidboot.hw_id=98029_1_10")
               ||strstr(saved_command_line,"androidboot.hw_id=98029_1_11")||strstr(saved_command_line,"androidboot.hw_id=98029_1_12")){
                clk_sw_ctr_ret=clk_buf_ctrl(CLK_BUF_NFC,false);
            }
            break;
        case 1:
            /* power on */
            pr_debug("%s power on\n", __func__);
            nfc_enable_irq(nfc_dev);
            if (nfc_dev->firm_gpio) {
                if ((*cur_state & (ST_WIRED|ST_SPI|ST_SPI_PRIO))== 0){
                    pn8xt_update_state(pn8xt_dev, ST_IDLE, true);
                }
                if(*cur_state & ST_DN){
                    pn8xt_update_state(pn8xt_dev, ST_DN, false);
                }
                gpio_set_value(nfc_dev->firm_gpio, 0);
            }
            pn8xt_dev->nfc_ven_enabled = true;
            if (pn8xt_dev->spi_ven_enabled == false || (pn8xt_dev->pwr_scheme == PN80T_EXT_PMU_SCM)) {
                gpio_set_value(nfc_dev->ven_gpio, 1);
            }

            if(strstr(saved_command_line,"androidboot.hw_id=98029_1_90")||strstr(saved_command_line,"androidboot.hw_id=98029_1_10")
               ||strstr(saved_command_line,"androidboot.hw_id=98029_1_11")||strstr(saved_command_line,"androidboot.hw_id=98029_1_12")){
                clk_sw_ctr_ret=clk_buf_ctrl(CLK_BUF_NFC,true);
            }
            break;
        case 2:
            if(*cur_state & (ST_SPI|ST_SPI_PRIO) && (pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM)) {
                /* NFCC fw/download should not be allowed when SPI is being used*/
                pr_err("%s NFCC should not be allowed to reset/FW download \n", __func__);
                return -EBUSY; /* Device or resource busy */
            }
            pn8xt_dev->nfc_ven_enabled = true;
            if ((pn8xt_dev->spi_ven_enabled == false && !(pn8xt_dev->secure_timer_cnt))
            || (pn8xt_dev->pwr_scheme == PN80T_EXT_PMU_SCM))
            {
                /* power on with firmware download (requires hw reset)
                 */
                pr_debug("%s power on with firmware\n", __func__);
                gpio_set_value(nfc_dev->ven_gpio, 1);
                msleep(10);
                if (nfc_dev->firm_gpio) {
                    pn8xt_update_state(pn8xt_dev, ST_DN, true);
                    gpio_set_value(nfc_dev->firm_gpio, 1);
                }
                msleep(10);
                gpio_set_value(nfc_dev->ven_gpio, 0);
                msleep(10);
                gpio_set_value(nfc_dev->ven_gpio, 1);
                msleep(10);
            }
            break;
        case 3:
            if(*cur_state & (ST_SPI|ST_SPI_PRIO)) {
                return -EPERM; /* Operation not permitted */
            }
            if(*cur_state & ST_WIRED) {
                pn8xt_update_state(pn8xt_dev, ST_WIRED, false);
            }
            break;
        case 4:
            pr_debug("%s FW dwld ioctl called from NFC \n", __func__);
            /*NFC Service called FW dwnld*/
            if (nfc_dev->firm_gpio) {
                pn8xt_update_state(pn8xt_dev, ST_DN, true);
                gpio_set_value(nfc_dev->firm_gpio, 1);
                msleep(10);
            }
            break;
        default:
            pr_err("%s bad arg %lu\n", __func__, arg);
            return -EINVAL;
    };
    return 0;
}

static long pn8xt_ese_pwr(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg)
{
    bool isSignalTriggerReqd = !(arg & 0x10);/*5th bit to/not trigger signal*/
    unsigned long pwrLevel = arg & 0x0F;
    struct pn8xt_dev *pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    pn8xt_access_st_t *cur_state = pn8xt_get_state(pn8xt_dev);
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    switch(pwrLevel) {
        case 0:
            pr_debug("%s: power off ese\n", __func__);
            if(*cur_state & ST_SPI_PRIO){
                pn8xt_update_state(pn8xt_dev, ST_SPI_PRIO, false);
                if (!(*cur_state & ST_JCP_DN)) {
                    if(!(*cur_state & ST_WIRED)) {
                        trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START | ST_SPI_PRIO_END);
                    } else {
                        signal_handler(ST_SPI_PRIO_END, pn8xt_dev->service_pid);
                    }
                } else if (!(*cur_state & ST_WIRED)) {
                    trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START);
                }
                pn8xt_dev->spi_ven_enabled = false;

                if(pn8xt_dev->pwr_scheme == PN80T_EXT_PMU_SCM)
                    break;

                /*if secure timer is running, Delay the SPI by 25ms after sending End of Apdu
                  to enable eSE go into DPD gracefully(20ms after EOS+5ms DPD settlement time)*/
                if(pn8xt_dev->secure_timer_cnt)
                    usleep_range(25000, 30000);

                if (!(*cur_state & ST_WIRED) && !(pn8xt_dev->secure_timer_cnt)) {
                    gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
                    /* Delay (2.5ms) after SVDD_PWR_OFF for the shutdown settlement time */
                    usleep_range(2500, 3000);
                    trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_END);
                }
                if ((pn8xt_dev->nfc_ven_enabled == false) && !(pn8xt_dev->secure_timer_cnt)) {
                     gpio_set_value(nfc_dev->ven_gpio, 0);
                     msleep(10);
                }
            } else if((*cur_state & ST_SPI) || (*cur_state & ST_SPI_FAILED)) {
                if (!(*cur_state & ST_WIRED) &&
                    (pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM) &&
                    !(*cur_state & ST_JCP_DN)) {
                    if (isSignalTriggerReqd && !(*cur_state & ST_JCP_DN)) {
                        if(trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START | ST_SPI_END)) {
                            pr_debug(" %s DWP link activation failed. Returning..", __func__);
                            pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, true);
                            return -1;
                        }
                    }
                    /*if secure timer is running,Delay the SPI close by 25ms after sending End of Apdu
                      to enable eSE go into DPD gracefully(20ms after EOS + 5ms DPD settlement time)*/
                    if(pn8xt_dev->secure_timer_cnt)
                        usleep_range(25000, 30000);

                    if (!(pn8xt_dev->secure_timer_cnt)) {
                        gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
                        /* Delay (2.5ms) after SVDD_PWR_OFF for the shutdown settlement time */
                        usleep_range(2500, 3000);
                        if(*cur_state & ST_SPI_FAILED) {
                            pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, false);
                        }
                        if(*cur_state & ST_SPI) {
                            pn8xt_update_state(pn8xt_dev, ST_SPI, false);
                        }
                        if(isSignalTriggerReqd)
                            trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_END);
                    }
                }
                /*If JCOP3.2 or 3.3 for handling triple mode
                protection signal NFC service */
                else {
                    if (isSignalTriggerReqd) {
                        if (!(*cur_state & ST_JCP_DN)) {
                            if(pn8xt_dev->pwr_scheme == PN80T_LEGACY_PWR_SCM) {
                                if(trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START | ST_SPI_END)) {
                                    pr_debug(" %s DWP link activation failed. Returning..", __func__);
                                    pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, true);
                                    return -1;
                                }
                            } else {
                                signal_handler(ST_SPI_END, pn8xt_dev->service_pid);
                            }
                        } else if (pn8xt_dev->pwr_scheme == PN80T_LEGACY_PWR_SCM) {
                            trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START);
                        }
                    }
                    if(pn8xt_dev->pwr_scheme == PN80T_LEGACY_PWR_SCM) {
                        gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
                        if(*cur_state & ST_SPI_FAILED) {
                            pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, false);
                        }
                        if(*cur_state & ST_SPI) {
                            pn8xt_update_state(pn8xt_dev, ST_SPI, false);
                        }
                        if(isSignalTriggerReqd)
                            trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_END);
                        pr_debug("%s:PN80T legacy ese_pwr_gpio off", __func__);
                    }
                }
                pn8xt_dev->spi_ven_enabled = false;
                if (pn8xt_dev->nfc_ven_enabled == false && (pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM)
                        && !(pn8xt_dev->secure_timer_cnt)) {
                    gpio_set_value(nfc_dev->ven_gpio, 0);
                    msleep(10);
                }
            } else {
                pr_err("%s:failed, cur_state = %x\n", __func__, *cur_state);
                return -EPERM; /* Operation not permitted */
            }
            break;
        case 1:
            pr_err("%s: power on ese\n", __func__);
            if (((*cur_state & (ST_SPI|ST_SPI_PRIO)) == 0) || (*cur_state & ST_SPI_FAILED)) {
                /*To handle triple mode protection signal
                NFC service when SPI session started*/
                if (isSignalTriggerReqd && !(*cur_state & ST_JCP_DN)) {
                    if(trigger_onoff(pn8xt_dev, ST_SPI)) {
                        pr_debug(" %s DWP link activation failed. Returning..", __func__);
                        pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, true);
                        return -1;
                    }
                }
                pn8xt_dev->spi_ven_enabled = true;

                if(pn8xt_dev->pwr_scheme == PN80T_EXT_PMU_SCM)
                    break;
                if (pn8xt_dev->nfc_ven_enabled == false) {
                    /* provide power to NFCC if, NFC service not provided */
                    gpio_set_value(nfc_dev->ven_gpio, 1);
                    msleep(10);
                }
                /* pull the gpio to high once NFCC is power on*/
                gpio_set_value(nfc_dev->ese_pwr_gpio, 1);
                /* Delay (10ms) after SVDD_PWR_ON to allow JCOP to bootup (5ms jcop boot time + 5ms guard time) */
                usleep_range(10000, 12000);
                if(*cur_state & ST_SPI_FAILED) {
                    pn8xt_update_state(pn8xt_dev, ST_SPI_FAILED, false);
                }
                pn8xt_update_state(pn8xt_dev, ST_SPI, true);
                if (pn8xt_dev->service_pid) {
                    up(&pn8xt_dev->dwp_complete_sema);
                }
            } else if ((*cur_state & (ST_SPI|ST_SPI_PRIO))
                 && (gpio_get_value(nfc_dev->ese_pwr_gpio)) && (gpio_get_value(nfc_dev->ven_gpio))) {
                /* Returning success if SET_SPI_PWR called while already SPI is open */
                return 0;
            } else {
                pr_info("%s : PN61_SET_SPI_PWR -  power on ese failed \n", __func__);
                return -EBUSY; /* Device or resource busy */
            }
            break;
        case 2:
            pr_debug("%s: reset\n", __func__);
            if (*cur_state & (ST_IDLE|ST_SPI|ST_SPI_PRIO)) {
                if (pn8xt_dev->spi_ven_enabled == false) {
                    pn8xt_dev->spi_ven_enabled = true;
                    if ((pn8xt_dev->nfc_ven_enabled == false) && (pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM)) {
                        /* provide power to NFCC if, NFC service not provided */
                        gpio_set_value(nfc_dev->ven_gpio, 1);
                        msleep(10);
                    }
                }
                if(pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM  && !(pn8xt_dev->secure_timer_cnt)) {
                    trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_START);
                    gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
                    trigger_onoff(pn8xt_dev, ST_SPI_SVDD_SY_END);
                    msleep(10);
                    if(!gpio_get_value(nfc_dev->ese_pwr_gpio))
                        gpio_set_value(nfc_dev->ese_pwr_gpio, 1);
                    msleep(10);
                }
            } else {
                pr_err("%s : PN61_SET_SPI_PWR - reset  failed \n", __func__);
                return -EBUSY; /* Device or resource busy */
            }
            break;
        case 3:
            pr_debug("%s: Prio Session Start power on ese\n", __func__);
            if ((*cur_state & (ST_SPI | ST_SPI_PRIO)) == 0) {
                pn8xt_update_state(pn8xt_dev, ST_SPI_PRIO, true);
                if (*cur_state & ST_WIRED) {
                    trigger_onoff(pn8xt_dev, ST_SPI_PRIO);
                }
                pn8xt_dev->spi_ven_enabled = true;
                if(pn8xt_dev->pwr_scheme != PN80T_EXT_PMU_SCM) {
                    if (pn8xt_dev->nfc_ven_enabled == false) {
                        /* provide power to NFCC if, NFC service not provided */
                        gpio_set_value(nfc_dev->ven_gpio, 1);
                        msleep(10);
                    }
                    /* pull the gpio to high once NFCC is power on*/
                    gpio_set_value(nfc_dev->ese_pwr_gpio, 1);

                    /* Delay (10ms) after SVDD_PWR_ON to allow JCOP to bootup (5ms jcop boot time + 5ms guard time) */
                    usleep_range(10000, 12000);
                }
            } else {
                pr_err("%s : Prio Session Start power on ese failed \n", __func__);
                return -EBUSY; /* Device or resource busy */
            }
            break;
        case 4:
            pr_debug("%s: Prio Session End called\n", __func__);
            if (*cur_state & ST_SPI_PRIO) {
                pr_info("%s : PN61_SET_SPI_PWR - Prio Session Ending...\n", __func__);
                pn8xt_update_state(pn8xt_dev, ST_SPI_PRIO, false);
                /*after SPI prio timeout, the state is changing from SPI prio to SPI */
                pn8xt_update_state(pn8xt_dev, ST_SPI, true);
                if (*cur_state & ST_WIRED) {
                    signal_handler(ST_SPI_PRIO_END, pn8xt_dev->service_pid);
                }
            } else {
                pr_err("%s : PN61_SET_SPI_PWR -  Prio Session End failed \n", __func__);
                return -EBADRQC; /* Device or resource busy */
            }
            break;
        case 5:
            pr_debug("%s: Up ese_access_sema\n", __func__);
            up(&pn8xt_dev->ese_access_sema);
            break;
        default:
            pr_err("%s bad ese pwr arg %lu\n", __func__, arg);
            return -EBADRQC; /* Invalid request code */
    };
    return 0;
}

static long set_jcop_download_state(struct pn8xt_dev *pn8xt_dev, unsigned long arg)
{
    long ret = 0;
    pn8xt_access_st_t *cur_state = pn8xt_get_state(pn8xt_dev);
    pr_debug("%s::JCOP Dwnld arg = %ld",__func__, arg);
    switch(arg) {
        case JCP_DN_INIT:
            if(pn8xt_dev->service_pid) {
                pr_err("%s:nfc service pid %ld", __func__, pn8xt_dev->service_pid);
                signal_handler((pn8xt_access_st_t)JCP_DN_INIT, pn8xt_dev->service_pid);
            } else {
                if (*cur_state & ST_JCP_DN) {
                    ret = -EINVAL;
                } else {
                    pn8xt_update_state(pn8xt_dev, ST_JCP_DN, true);
                }
            }
            break;
        case JCP_DN_START:
            if (*cur_state & ST_JCP_DN) {
                ret = -EINVAL;
            } else {
                pn8xt_update_state(pn8xt_dev, ST_JCP_DN, true);
            }
            break;
        case JCP_SPI_DN_COMP:
            signal_handler((pn8xt_access_st_t)JCP_DWP_DN_COMP, pn8xt_dev->service_pid);
            pn8xt_update_state(pn8xt_dev, ST_JCP_DN, false);
            break;
        case JCP_DWP_DN_COMP:
            pn8xt_update_state(pn8xt_dev, ST_JCP_DN, false);
            break;
        default:
            pr_err("%s: bad ese pwr arg %lu\n", __func__, arg);
            return -EBADRQC; /* Invalid request code */
    };
    return ret;
}

static int set_wired_access(struct nfc_dev *nfc_dev, unsigned long arg)
{
    struct pn8xt_dev *pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    pn8xt_access_st_t *cur_state = pn8xt_get_state(pn8xt_dev);
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    switch(arg) {
        case 0:
            pr_debug("%s: disabling \n", __func__);
            if (*cur_state & ST_WIRED) {
                pn8xt_update_state(pn8xt_dev, ST_WIRED, false);
            } else {
                pr_err("%s: failed, cur_state = %x\n", __func__, *cur_state);
                return -EPERM; /* Operation not permitted */
            }
            break;
        case 1:
            if (*cur_state)
            {
                pr_debug("%s: enabling\n", __func__);
                pn8xt_update_state(pn8xt_dev, ST_WIRED, true);
                if (*cur_state & ST_SPI_PRIO) {
                    signal_handler(ST_SPI_PRIO, pn8xt_dev->service_pid);
                }
            } else {
                pr_err("%s: enabling failed \n", __func__);
                return -EBUSY; /* Device or resource busy */
            }
            break;
        case 2:
        case 3:
            pr_debug("%s: obsolete arguments for P67\n", __func__);
            break;
        case 4:
            up(&pn8xt_dev->ese_access_sema);
            break;
        case 5:
            gpio_set_value(nfc_dev->ese_pwr_gpio, 1);
            if (gpio_get_value(nfc_dev->ese_pwr_gpio)) {
                pr_info("%s: ese_pwr gpio is enabled\n", __func__);
            }
            break;
        case 6:
            gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
            pr_info("%s: ese_pwr gpio set to low\n", __func__);
            break;
        default:
            pr_err("%s bad arg %lu\n", __func__, arg);
            return -EBADRQC; /* Invalid request code */
    };
    return 0;
}


static void secure_timer_callback(struct timer_list *t)
{
    struct pn8xt_dev *pn8xt_dev = from_timer(pn8xt_dev, t, secure_timer);;
    /* Flush and push the timer callback event to the bottom half(work queue)
    to be executed later, at a safer time */
    flush_workqueue(pn8xt_dev->pSecureTimerCbWq);
    queue_work(pn8xt_dev->pSecureTimerCbWq, &pn8xt_dev->wq_task);
    return;
}

static long start_seccure_timer(struct pn8xt_dev *pn8xt_dev, unsigned long timer_value)
{
    long ret = -EINVAL;
    pr_debug("%s: called\n", __func__);
    /* Delete the timer if timer pending */
    if(timer_pending(&pn8xt_dev->secure_timer) == 1) {
        pr_debug("%s: delete pending timer \n", __func__);
        /* delete timer if already pending */
        del_timer(&pn8xt_dev->secure_timer);
    }
    /* Start the timer if timer value is non-zero */
    if(timer_value) {
        timer_setup(&pn8xt_dev->secure_timer, secure_timer_callback, 0);
        pr_debug("%s:timeout %lums (%lu)\n", __func__, timer_value, jiffies);
        ret = mod_timer(&pn8xt_dev->secure_timer, jiffies + msecs_to_jiffies(timer_value));
        if (ret)
            pr_err("%s:Error in mod_timer\n", __func__);
    }
    return ret;
}

static void secure_timer_workqueue(struct work_struct *wq)
{
    struct pn8xt_dev *pn8xt_dev = container_of(wq, struct pn8xt_dev, wq_task);
    struct nfc_dev *nfc_dev = pn8xt_dev->nfc_dev;
    pn8xt_access_st_t *cur_state = pn8xt_get_state(pn8xt_dev);
    pr_debug("%s:called (%lu).\n", __func__, jiffies);
    /* Locking the critical section: ESE_PWR_OFF to allow eSE to shutdown peacefully :: START */
    get_ese_lock(pn8xt_dev, MAX_ESE_ACCESS_TIME_OUT_MS);
    pn8xt_update_state(pn8xt_dev, ST_SECURE_MODE, false);

    if((*cur_state & (ST_SPI|ST_SPI_PRIO)) == 0) {
        pr_debug("%s: make se_pwer_gpio low, state = %d", __func__, *cur_state);
        gpio_set_value(nfc_dev->ese_pwr_gpio, 0);
        /* Delay (2.5ms) after SVDD_PWR_OFF for the shutdown settlement time */
        usleep_range(2500, 3000);
        if(pn8xt_dev->service_pid == 0x00) {
            gpio_set_value(nfc_dev->ven_gpio, 0);
            pr_debug("%s: make ven_gpio low, state = %d", __func__, *cur_state);
        }
  }
  pn8xt_dev->secure_timer_cnt = 0;
  /* Locking the critical section: ESE_PWR_OFF to allow eSE to shutdown peacefully :: END */
  up(&pn8xt_dev->ese_access_sema);
  return;
}


static long secure_timer_operation(struct pn8xt_dev *pn8xt_dev, unsigned long arg)
{
    long ret = -EINVAL;
    unsigned long timer_value =  arg;

    pr_debug("%s: pwr scheme = %d\n", __func__, pn8xt_dev->pwr_scheme);
    if(pn8xt_dev->pwr_scheme == PN80T_LEGACY_PWR_SCM) {
        ret = start_seccure_timer(pn8xt_dev, timer_value);
        if(!ret) {
            pn8xt_dev->secure_timer_cnt  = 1;
            pn8xt_update_state(pn8xt_dev, ST_SECURE_MODE, true);
        } else {
            pn8xt_dev->secure_timer_cnt  = 0;
            pn8xt_update_state(pn8xt_dev, ST_SECURE_MODE, false);
            pr_debug("%s :timer reset \n", __func__);
        }
    } else {
        pr_info("%s: timer session not applicable\n", __func__);
    }
    return ret;
}

long pn8xt_nfc_ese_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    struct pn8xt_dev *pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    if (cmd == PN8XT_GET_ESE_ACCESS) {
        ret = get_ese_lock(pn8xt_dev, arg);
        return ret;
    }

    switch (cmd) {
        case PN8XT_SET_SPI_PWR:
            ret = pn8xt_ese_pwr(nfc_dev, cmd, arg);
            break;
        case PN8XT_GET_PWR_STATUS:
            put_user(pn8xt_dev->cur_state, (int __user *)arg);
            break;
        case PN8XT_SET_DN_STATUS:
            ret = set_jcop_download_state(pn8xt_dev, arg);
            break;
        case PN8XT_SET_POWER_SCM:
            if(arg == PN80T_LEGACY_PWR_SCM || arg == PN80T_EXT_PMU_SCM)
                pn8xt_dev->pwr_scheme = arg;
            else
                pr_err("%s : The power scheme is invalid,\n", __func__);
            break;
        case PN8XT_SECURE_TIMER_SESSION:
            ret = secure_timer_operation(pn8xt_dev, arg);
            break;
        default:
            pr_err("%s bad ioctl %u\n", __func__, cmd);
            ret = -EINVAL;
    };
    return ret;
}

long pn8xt_nfc_ioctl(struct nfc_dev *nfc_dev, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    struct pn8xt_dev *pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    pr_debug("%s :enter cmd = %u, arg = %ld\n", __func__, cmd, arg);

    switch(cmd) {
        case PN8XT_SET_PWR:
            ret = pn8xt_nfc_pwr(nfc_dev, arg);
            break;
        case PN8XT_SET_WIRED_ACCESS:
            ret = set_wired_access(nfc_dev, arg);
            break;
        case PN8XT_REL_SVDD_WAIT:
            pn8xt_dev->dwpLinkUpdateStat = arg;
            up(&pn8xt_dev->svdd_onoff_sema);
            break;
        case PN8XT_SET_NFC_SERVICE_PID:
            pn8xt_dev->service_pid = arg;
            break;
        case PN8XT_REL_DWP_WAIT:
            pn8xt_dev->dwpLinkUpdateStat = arg;
            up(&pn8xt_dev->dwp_onoff_sema);
            sema_init(&pn8xt_dev->dwp_complete_sema, 0);
            /*release JNI only after all the SPI On related actions are completed*/
            if (down_timeout(&pn8xt_dev->dwp_complete_sema, msecs_to_jiffies(500)) != 0) {
                pr_debug("Dwp On/off release wait protection: Timeout");
            }
            pr_debug("Dwp On/Off release wait protection : released");
            break;
        default:
            ret = pn8xt_nfc_ese_ioctl(nfc_dev, cmd, arg);
            if (ret)
                pr_err("%s bad ioctl %u\n", __func__, cmd);
        break;
    };
    pr_debug("%s :exit cmd = %u, arg = %ld\n", __func__, cmd, arg);
    return ret;
}

#define SECURE_TIMER_WORK_QUEUE "SecTimerCbWq"
int pn8xt_nfc_probe(struct nfc_dev *nfc_dev)
{
    struct pn8xt_dev *pn8xt_dev;
    pn8xt_dev = kzalloc(sizeof(struct pn8xt_dev), GFP_KERNEL);
    if (!pn8xt_dev) {
        pr_err("failed to allocate memory for pn8xt_dev\n");
        return -ENOMEM;
    }
    nfc_dev->pdata_op = pn8xt_dev;
    pn8xt_dev->cur_state = ST_IDLE;
    pn8xt_dev->pwr_scheme = PN80T_LEGACY_PWR_SCM;
    pn8xt_dev->secure_timer_cnt = 0;
    pn8xt_dev->nfc_ven_enabled = false;
    pn8xt_dev->spi_ven_enabled = false;
    /* init mutex and queues */
    sema_init(&pn8xt_dev->ese_access_sema, 1);
    sema_init(&pn8xt_dev->dwp_complete_sema, 0);
    pn8xt_dev->pSecureTimerCbWq = create_workqueue(SECURE_TIMER_WORK_QUEUE);
    INIT_WORK(&pn8xt_dev->wq_task, secure_timer_workqueue);
    pn8xt_dev->nfc_dev = nfc_dev;
    return 0;
}

int pn8xt_nfc_remove(struct nfc_dev *nfc_dev)
{
    struct pn8xt_dev *pn8xt_dev;
    pr_debug("%s: called\n", __func__);
    pn8xt_dev = (struct pn8xt_dev *)nfc_dev->pdata_op;
    if (!pn8xt_dev) {
        pr_err("%s: pn8xt_dev doesn't exist anymore\n", __func__);
        return -ENODEV;
    }
    destroy_workqueue(pn8xt_dev->pSecureTimerCbWq);
    pn8xt_dev->cur_state = ST_INVALID;
    pn8xt_dev->nfc_ven_enabled = false;
    pn8xt_dev->spi_ven_enabled = false;
    pn8xt_dev->nfc_dev = NULL;
    kfree(pn8xt_dev);
    return 0;
}


