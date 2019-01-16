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
 *   AudDrv_Anc.c
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

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>       /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>   /* needed by device_* */
#include <mach/hardware.h>  /* needed by __io_address */
#include <asm/io.h>     /* needed by ioremap * */
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>       /* needed by file_operations* */
#include <linux/slab.h>
#include <mach/sync_write.h>
#include <mach/mt_reg_base.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include "../md32/md32_ipi.h"
#include "../md32/md32_helper.h"
#include "AudDrv_Anc.h"
/*****************************************************************************
*           DEFINE AND CONSTANT
******************************************************************************
*/

#define AUDDRV_ANC_NAME   "MediaTek Audio ANC Driver"
#define AUDDRV_AUTHOR "MediaTek WCX"

/*****************************************************************************
*           V A R I A B L E     D E L A R A T I O N
*******************************************************************************/

static char       auddrv_anc_name[]       = "AudioAnc";


/*****************************************************************************
*                  A N C    S E R V I C E S
******************************************************************************
*/
#if 1
static struct
{
    short RegData[60];
    int swapid;
    bool swapwait;
    bool ipimsgwait;
    bool dumpwait;
    bool debugmode;
    int  dumpaddr;
    int init_coeff_addr;
} ancserv;
#endif

/*****************************************************************************
*                        F U N C T I O N   D E F I N I T I O N
******************************************************************************
*/

/*****************************************************************************
*                  U T I L I T Y    F U N C T I O N S
******************************************************************************
*/
static bool anc_ipimsg_wait()
{
    int timeout = 0;
    while (ancserv.ipimsgwait)
    {
        msleep(ANC_WAITCHECK_INTERVAL_MS);
        if (timeout++ >= ANC_IPIMSG_TIMEOUT)
        {
            printk("[ANC Kernel] Error: IPI MSG timeout\n");
            ancserv.ipimsgwait = false;
            return false;
        }
    }
    printk("[ANC Kernel]IPI MSG -: %x\n", timeout);
    return true;
}

static bool anc_swaptcm_wait()
{
    int timeout = 0;
    while (ancserv.swapwait)
    {
        msleep(ANC_WAITCHECK_INTERVAL_MS);
        if (timeout++ >= ANC_SWAPTCM_TIMEOUT)
        {
            printk("[ANC] Error: Swap TCM from/to ANC timeout\n");
            ancserv.swapid = -1;
            ancserv.swapwait = true;
            return false;
        }
    }
    printk("[ANC] Swap TCM from/to ANC group-: %x\n", timeout);
    return true;
}

/*****************************************************************************
*                  I P I    F U N C T I O N S
******************************************************************************
*/
bool anc_ipi_sendmsg(anc_ipi_msgid_t id, void *buf, unsigned int size, unsigned int type, uint wait)
{
    ipi_status status;
    short ipibuf[24];
    ipibuf[0] = id;
    ipibuf[1] = size;

    if (type == 0)
    {
        memcpy((void *)&ipibuf[2], buf, size);
        status = md32_ipi_send(IPI_ANC, (void *)ipibuf, size + 4, wait);
    }
    else
    {
        memcpy((void *)&ipibuf[2], buf, 4);
        status = md32_ipi_send(IPI_ANC, (void *)ipibuf, 8, wait);
    }

    if (status != DONE)
    {
        return false;
    }
    return true;
}

void anc_ipi_handler(int id, void *data, unsigned int len)
{
    anc_ipi_msg_t *msg_md2ap = (anc_ipi_msg_t *)data;

    switch (msg_md2ap->id)
    {
        case MD32_IPIMSG_ANC_ENABLE_ACK:
        {
            printk("[ANC_Kernel] MD32_IPIMSG_ANC_Enable_ACK\n");
            ancserv.ipimsgwait = false;
            break;
        }
        case MD32_IPIMSG_ANC_DISABLE_ACK:
        {
            printk("[ANC_Kernel] MD32_IPIMSG_ANC_DISABLE_ACK\n");
            ancserv.ipimsgwait = false;
            break;
        }
        case MD32_IPIMSG_ANC_CLOSE_ACK:
        {
            printk("[ANC_Kernel] MD32_IPIMSG_ANC_CLOSE_ACK\n");
            ancserv.ipimsgwait = false;
            break;
        }
        case MD32_IPIMSG_ANC_ENABLE_LOGGING_ACK:
        {
            printk("[ANC_Kernel] MD32_IPIMSG_ANC_ENABLE_LOGGING_ACK\n");
            break;
        }
        case MD32_IPIMSG_ANC_DISABLE_LOGGING_ACK:
        {
            printk("[ANC_Kernel] MD32_IPIMSG_IPIMSG_ANC_DISABLE_LOGGING_ACK\n");
            break;
        }
        case MD32_IPIMSG_ANC_FILTERCOEF_NOTIFY:
        {
            printk("[[ANC_Kernel] MD32_IPIMSG_ANC_FILTERCOEF_NOTIFY\n");
            ancserv.dumpwait = false;
            break;
        }
        case MD32_IPIMSG_ANC_COEFF_ADDR_ACK:
            printk("[ANC_Kernel] MD32_IPIMSG_ANC_COEFF_ADDR_ACK\n");
            break;
        default:
            printk("[ANC_Kernel] UNKNOWN MSG NOTIFY FROM MD32:%x\n", msg_md2ap->id);
            break;
    }
}

/*****************************************************************************
*                  D Y N A M I C   S W A P   F U N C T I O N S
******************************************************************************
*/

static int md32_anc_notify(struct notifier_block *self, unsigned long action, void *dev)
{
    MD32_REQUEST_SWAP *request_swap = (MD32_REQUEST_SWAP *)dev;
    int ret;

    switch (action)
    {
#ifdef DYNAMIC_TCM_SWAP
        case MD32_SELF_TRIGGER_TCM_SWAP:
        {
            ret = md32_tcm_swap(ancserv.swapid);
            if (ret < 0)
            {
                printk("md32_anc_notify swap tcm failed\n");
            }
            else
            {
                ancserv.swapid = -1;
                ancserv.swapwait = false;
            }
            break;
        }
        case APP_TRIGGER_TCM_SWAP_START:
        {
            break;
        }
        case APP_TRIGGER_TCM_SWAP_DONE:
        {
            break;
        }
        case APP_TRIGGER_TCM_SWAP_FAIL:
        {
            break;
        }
#endif
        default:
            printk("[ANC_Kernel] md32 anc get unkown action %d\n", action);
            break;
    }
    return NOTIFY_OK;
}

static struct notifier_block md32_anc_nb =
{
    .notifier_call =        md32_anc_notify,
};


/*****************************************************************************
*                 E X P O R T   F U N C T I O N S
******************************************************************************
*/
void anc_service_Init(void)
{
    printk("[ANC_Kernel] md32_anc_init\n");
    ancserv.swapid          = -1;
    ancserv.swapwait        = false;
    ancserv.ipimsgwait      = false;
    ancserv.dumpwait        = false;
    ancserv.debugmode       = false;
    ancserv.dumpaddr        = -1;
    ancserv.init_coeff_addr = -1;

    //register IPI handler
    md32_ipi_registration(IPI_ANC, anc_ipi_handler, "ANC");
    //Hook to the MD32 chain to get notification
    //   md32_register_notify(&md32_anc_nb);
}

void anc_service_SetParameter(unsigned long arg)
{
    if (copy_from_user((void *)(&ancserv.RegData), (const void __user *)(arg), sizeof(ancserv.RegData)))
    {
        return -EFAULT;
    }
}

int anc_service_getDumpAddr(void)
{
    ancserv.dumpwait = true;
    while (ancserv.dumpwait)
    {
        msleep(ANC_WAITDUMP_INTERVAL_MS);
    }
    return ancserv.dumpaddr;
}


void anc_service_EnableLog(void)
{
    printk("[ANC] Enable Debug Log\n");
    ancserv.debugmode = true;
}

void anc_service_DisableLog(void)
{
    printk("[ANC_Kernel] Disable Debug Log\n");
    ancserv.debugmode = false;
}

bool anc_service_SwapToANC(void)
{
    int ret;
    printk("[ANC] Swap TCM to ANC group+\n");
    ret = true;

    if (GROUP_A != get_current_group())
    {
        ancserv.swapwait = true;
        ancserv.swapid   = GROUP_A;
        //      md32_prepare_swap(GROUP_A);
        ret = anc_swaptcm_wait();
    }

    return ret;
}

bool anc_service_SwapFromANC(void)
{
    bool ret;
    printk("[ANC] Swap TCM from ANC group+\n");
    ret = true;
    if (GROUP_BASIC != get_current_group())
    {
        ancserv.swapwait = true;
        ancserv.swapid   = GROUP_BASIC;
        //      md32_prepare_swap(GROUP_BASIC);
        ret = anc_swaptcm_wait();
    }
    return ret;
}

bool anc_service_Enable(void)
{
    bool ret;
    printk("[ANC_Kernel] ANC Enable+\n");

    if (GROUP_A != get_current_group())
    {
        printk("[ANC_Kernel] ANC Enable NotANC group Fail\n");
        return;
    }
    if (ancserv.init_coeff_addr == -1)
    {
        printk("[ANC_Kernel] Get InitCoeff Addr+\n");
        ancserv.ipimsgwait = true;
        anc_ipi_sendmsg(AP_IPIMSG_ANC_COEFF_ADDR, (void *)0, 0, 0, 0);
        if (!anc_ipimsg_wait())
        {
            return false;
        }
    }
    ancserv.ipimsgwait = true;
    if (ancserv.debugmode)
    {
        anc_ipi_sendmsg(AP_IPIMSG_ANC_ENABLE_LOGGING, (void *)0, 0, 0, 0);
    }
    else
    {
        anc_ipi_sendmsg(AP_IPIMSG_ANC_DISABLE_LOGGING, (void *)0, 0, 0, 0);
    }
    ret = anc_ipimsg_wait();
    if (!ret)
    {
        return false;
    }
    ancserv.ipimsgwait = true;
    anc_ipi_sendmsg(AP_IPIMSG_ANC_ENABLE, (void *)0, 0, 0, 0);
    return anc_ipimsg_wait();
}

bool anc_service_Disable(void)
{
    printk("[ANC_Kernel] ANC Disable+\n");

    if (GROUP_A != get_current_group())
    {
        printk("[ANC_Kernel] ANC Disable NotANC group Fail\n");
        return;
    }
    ancserv.ipimsgwait = true;
    anc_ipi_sendmsg(AP_IPIMSG_ANC_DISABLE, (void *)0, 0, 0, 0);
    return anc_ipimsg_wait();
}

bool anc_service_Close(void)
{
    printk("[ANC_Kernel] ANC Close+\n");

    if (GROUP_A != get_current_group())
    {
        printk("[ANC_Kernel] ANC Close NotANC group Fail\n");
        return;
    }
    ancserv.ipimsgwait = true;
    anc_ipi_sendmsg(AP_IPIMSG_ANC_CLOSE, (void *)0, 0, 0, 0);
    return anc_ipimsg_wait();
}

/*****************************************************************************
 * FILE OPERATION FUNCTION
 *  AudDrv_nxpspk_ioctl
 *
 * DESCRIPTION
 *  IOCTL Msg handle
 *
 *****************************************************************************
 */
static long AudDrv_anc_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    int  ret = 0;

    printk("AudDrv_anc_ioctl cmd = 0x%x arg = %lu\n", cmd, arg);

    switch (cmd)
    {
        case SET_ANC_CONTROL:
        {
            printk("SET_ANC_CONTROL(%d)", arg);
            switch (arg)
            {
                case ANCControlCmd_Init:
                    anc_service_Init();
                    break;
                case ANCControlCmd_SwapToANC:
                    if (!anc_service_SwapToANC());
                    ret = -EFAULT;
                    break;
                case ANCControlCmd_SwapFromANC:
                    if (!anc_service_SwapFromANC());
                    ret = -EFAULT;
                    break;
                case ANCControlCmd_Enable:
                    if (!anc_service_Enable());
                    ret = -EFAULT;
                    break;
                case ANCControlCmd_Disable:
                    if (!anc_service_Disable());
                    ret = -EFAULT;
                    break;
                case ANCControlCmd_Close:
                    if (!anc_service_Close());
                    ret = -EFAULT;
                    break;
                case ANCControlCmd_GetDumpAddr:
                    ret = anc_service_getDumpAddr();
                    break;
                case ANCControlCmd_EnableLog:
                    anc_service_EnableLog();
                    break;
                case ANCControlCmd_DisableLog:
                    anc_service_DisableLog();
                    break;
                default:
                    printk("SET_ANC_CONTROL no such command = %x", arg);
                    break;
            }
            break;
        }
        case SET_ANC_PARAMETER:
        {
            printk("SET_ANC_PARAMETER(%d)", arg);
            anc_service_SetParameter(arg);
            break;
        }
        case GET_ANC_PARAMETER:
        {
            printk("GET_ANC_PARAMETER(%d)", arg);
            break;
        }



        default:
        {
            //printk("AudDrv_nxpspk_ioctl Fail command: %x \n", cmd);
            ret = -1;
            break;
        }
    }
    return ret;
}

static int AudDrv_anc_probe(struct platform_device *dev)
{
    int ret = 0;
    printk("AudDrv_anc_probe \n");

    if (ret < 0)
    {
        printk("AudDrv_anc_probe request_irq MT6595_AP_ANC Fail \n");
    }
    anc_service_Init();



    printk("-AudDrv_anc_probe \n");
    return 0;
}

static int AudDrv_anc_open(struct inode *inode, struct file *fp)
{
    printk("AudDrv_anc_open \n");
    return 0;
}
/**************************************************************************
 * STRUCT
 *  File Operations and misc device
 *
 **************************************************************************/

static struct file_operations AudDrv_anc_fops =
{
    .owner   = THIS_MODULE,
    .open    = AudDrv_anc_open,
    .unlocked_ioctl   = AudDrv_anc_ioctl,
};

static struct miscdevice AudDrv_anc_device =
{
    .minor = MISC_DYNAMIC_MINOR,
    .name = "anc",
    .fops = &AudDrv_anc_fops,
};

/***************************************************************************
 * FUNCTION
 *  AudDrv_anc_mod_init / AudDrv_anc_mod_exit
 *
 * DESCRIPTION
 *  Module init and de-init (only be called when system boot up)
 *
 **************************************************************************/

static struct platform_driver AudDrv_anc =
{
    .probe    = AudDrv_anc_probe,
    .driver   = {
        .name = auddrv_anc_name,
    },
};

static struct platform_device *AudDrv_ANC_dev;

static int AudDrv_anc_mod_init(void)
{
    int ret = 0;
    printk("+AudDrv_anc_mod_init \n");


    printk("platform_device_alloc  \n");
    AudDrv_ANC_dev = platform_device_alloc("AudioMTKANC", -1);
    if (!AudDrv_ANC_dev)
    {
        return -ENOMEM;
    }

    printk("platform_device_add  \n");

    ret = platform_device_add(AudDrv_ANC_dev);
    if (ret != 0)
    {
        platform_device_put(AudDrv_ANC_dev);
        return ret;
    }

    // Register platform DRIVER
    ret = platform_driver_register(&AudDrv_anc);
    if (ret)
    {
        printk("AudDrv Fail:%d - Register DRIVER \n", ret);
        return ret;
    }

    // register MISC device
    if ((ret = misc_register(&AudDrv_anc_device)))
    {
        printk("AudDrv_anc_mod_init misc_register Fail:%d \n", ret);
        return ret;
    }

    printk("-AudDrv_anc_mod_init\n");
    return 0;
}

static void  AudDrv_anc_mod_exit(void)
{
    printk("+AudDrv_anc_mod_exit \n");

    printk("-AudDrv_anc_mod_exit \n");
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(AUDDRV_ANC_NAME);
MODULE_AUTHOR(AUDDRV_AUTHOR);

module_init(AudDrv_anc_mod_init);
module_exit(AudDrv_anc_mod_exit);

//EXPORT_SYMBOL(NXPSpk_read_byte);
//EXPORT_SYMBOL(NXPExt_write_byte);


