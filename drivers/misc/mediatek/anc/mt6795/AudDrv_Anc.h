/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Anc.h
 *
 * Project:
 * --------
 *   MT6595  Audio Driver ANC control
 *
 * Description:
 * ------------
 *   ANC control
 *
 * Author:
 * -------
 *   Doug Wang (mtk02134)
 *
 *------------------------------------------------------------------------------
 * $Revision$
 * $Modtime:$
 * $Log:$
 *
 *
 *
 *
 *
 *******************************************************************************/

#ifndef _AUDDRV_ANC_H
#define _AUDDRV_ANC_H

/*****************************************************************************
*                          I N C L U D E  F I L E S
******************************************************************************
*/

#include <mach/mt_typedefs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/xlog.h>
#include <mach/irqs.h>
#include <mach/mt_irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/mt_reg_base.h>
#include <asm/div64.h>
#include <linux/aee.h>
#include <linux/xlog.h>
#include <linux/i2c.h>
#include <mach/mt_gpio.h>
#include <mach/mt_boot.h>
#include <cust_eint.h>
#include <cust_gpio_usage.h>
#include <mach/eint.h>
/*****************************************************************************
*                          C O N S T A N T S
******************************************************************************
*/

#define ANC_UT 0

/* MD32/AP IPI Message ID */
#define MD32_IPI_AUDMSG_BASE 0x5F00
#define AP_IPI_AUDMSG_BASE   0x7F00

#define ANC_SWAPTCM_TIMEOUT        50
#define ANC_IPIMSG_TIMEOUT         50
#define ANC_WAITCHECK_INTERVAL_MS  10
#define ANC_WAITDUMP_INTERVAL_MS    5


//below is control message
#define AUD_DRV_ANC_IOC_MAGIC 'C'
//ANC Control
#define SET_ANC_CONTROL          _IOW(AUD_DRV_ANC_IOC_MAGIC, 0x1, int)
#define SET_ANC_PARAMETER        _IOW(AUD_DRV_ANC_IOC_MAGIC, 0x2, int)
#define GET_ANC_PARAMETER        _IOW(AUD_DRV_ANC_IOC_MAGIC, 0x3, int)

/*****************************************************************************
*                         D A T A   T Y P E S
******************************************************************************
*/
enum ANC_Control_Cmd
{
    ANCControlCmd_Init = 0,
    ANCControlCmd_SwapToANC,
    ANCControlCmd_SwapFromANC,
    ANCControlCmd_Enable,
    ANCControlCmd_Disable,
    ANCControlCmd_EnableLog,
    ANCControlCmd_DisableLog,
    ANCControlCmd_Close,
    ANCControlCmd_GetDumpAddr
#if ANC_UT
    ,
    ANCControlCmd_GetStatus,
    ANCControlCmd_GetLogStatus,
    ANCControlCmd_GetCurGroup
#endif
};

typedef enum  anc_ipi_msgid_t
{
    AP_IPIMSG_ANC_ENABLE = AP_IPI_AUDMSG_BASE,
    AP_IPIMSG_ANC_DISABLE,
    AP_IPIMSG_ANC_CLOSE,
    AP_IPIMSG_ANC_ENABLE_LOGGING,
    AP_IPIMSG_ANC_DISABLE_LOGGING,
    AP_IPIMSG_ANC_ANC_FILTERCOEF_NOTIFY_ACK,
    AP_IPIMSG_ANC_COEFF_ADDR,

    MD32_IPIMSG_ANC_ENABLE_ACK = AP_IPI_AUDMSG_BASE,
    MD32_IPIMSG_ANC_DISABLE_ACK,
    MD32_IPIMSG_ANC_CLOSE_ACK,
    MD32_IPIMSG_ANC_ENABLE_LOGGING_ACK,
    MD32_IPIMSG_ANC_DISABLE_LOGGING_ACK,
    MD32_IPIMSG_ANC_FILTERCOEF_NOTIFY,
    MD32_IPIMSG_ANC_COEFF_ADDR_ACK
} anc_ipi_msgid_t;

typedef struct
{
    short id;
    short size;
    short *buf;
} anc_ipi_msg_t;

/*****************************************************************************
*                        F U N C T I O N   D E F I N I T I O N
******************************************************************************
*/

bool anc_service_SwapToANC(void);
bool anc_service_SwapFromANC(void);
void anc_service_Init(void);
bool anc_service_Enable(void);
bool anc_service_Disable(void);
bool anc_service_Close(void);
void anc_service_EnableLog(void);
void anc_service_DisableLog(void);
void anc_service_SetParameter(unsigned long arg);
int anc_service_getDumpAddr(void);

int anc_ut_curgroup();
int anc_ut_curstatus();
int anc_ut_logstatus();
int anc_ut_curreg(int arg);


static long AudDrv_anc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static int AudDrv_anc_probe(struct platform_device *dev);
static int AudDrv_anc_open(struct inode *inode, struct file *fp);
static int AudDrv_anc_mod_init(void);
static void  AudDrv_anc_mod_exit(void);

#endif   // _AUDDRV_ANC_H
