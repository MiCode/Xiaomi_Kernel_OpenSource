/******************************************************************************
*[File]             ahb.c
*[Version]          v1.0
*[Revision Date]    2013-01-16
*[Author]
*[Description]
*    The program provides AHB HIF driver
*[Copyright]
*    Copyright (C) 2013 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/



/*
** $Log: ahb.c $
 *
 * 01 16 2013 vend_samp.lin
 * Port sdio.c to ahb.c on MT6572/MT6582
 * 1) Initial version
 *
 * 04 12 2012 terry.wu
 * NULL
 * Add AEE message support
 * 1) Show AEE warning(red screen) if SDIO access error occurs
 *
 * 02 14 2012 cp.wu
 * [WCXRP00000851] [MT6628 Wi-Fi][Driver] Add HIFSYS related definition to driver source tree
 * include correct header file upon setting.
 *
 * 11 10 2011 cp.wu
 * [WCXRP00001098] [MT6620 Wi-Fi][Driver] Replace printk by DBG LOG macros in linux porting layer
 * 1. eliminaite direct calls to printk in porting layer.
 * 2. replaced by DBGLOG, which would be XLOG on ALPS platforms.
 *
 * 09 20 2011 cp.wu
 * [WCXRP00000994] [MT6620 Wi-Fi][Driver] dump message for bus error and reset bus error flag while re-initialized
 * 1. always show error message for SDIO bus errors.
 * 2. reset bus error flag when re-initialization
 *
 * 08 17 2011 cp.wu
 * [WCXRP00000851] [MT6628 Wi-Fi][Driver] Add HIFSYS related definition to driver source tree
 * add MT6628 related definitions for Linux/Android driver.
 *
 * 05 18 2011 cp.wu
 * [WCXRP00000702] [MT5931][Driver] Modify initialization sequence for E1 ASIC
 * add device ID for MT5931.
 *
 * 04 08 2011 pat.lu
 * [WCXRP00000623] [MT6620 Wi-Fi][Driver] use ARCH define to distinguish PC Linux driver
 * Use CONFIG_X86 instead of PC_LINUX_DRIVER_USE option to have proper compile settting for PC Linux driver
 *
 * 03 22 2011 pat.lu
 * [WCXRP00000592] [MT6620 Wi-Fi][Driver] Support PC Linux Environment Driver Build
 * Add a compiler option "PC_LINUX_DRIVER_USE" for building driver in PC Linux environment.
 *
 * 03 18 2011 cp.wu
 * [WCXRP00000559] [MT6620 Wi-Fi][Driver] Combine TX/RX DMA buffers into a single one to reduce physically continuous memory consumption
 * deprecate CFG_HANDLE_IST_IN_SDIO_CALLBACK.
 *
 * 03 15 2011 cp.wu
 * [WCXRP00000559] [MT6620 Wi-Fi][Driver] Combine TX/RX DMA buffers into a single one to reduce physically continuous memory consumption
 * 1. deprecate CFG_HANDLE_IST_IN_SDIO_CALLBACK
 * 2. Use common coalescing buffer for both TX/RX directions
 *
 *
 * 03 07 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Toggle non-standard debug messages to comments.
 *
 * 11 15 2010 jeffrey.chang
 * [WCXRP00000181] [MT6620 Wi-Fi][Driver] fix the driver message "GLUE_FLAG_HALT skip INT" during unloading
 * Fix GLUE_FALG_HALT message which cause driver to hang
 *
 * 11 08 2010 cp.wu
 * [WCXRP00000166] [MT6620 Wi-Fi][Driver] use SDIO CMD52 for enabling/disabling interrupt to reduce transaction period
 * correct typo
 *
 * 11 08 2010 cp.wu
 * [WCXRP00000166] [MT6620 Wi-Fi][Driver] use SDIO CMD52 for enabling/disabling interrupt to reduce transaction period
 * change to use CMD52 for enabling/disabling interrupt to reduce SDIO transaction time
 *
 * 11 01 2010 yarco.yang
 * [WCXRP00000149] [MT6620 WI-Fi][Driver]Fine tune performance on MT6516 platform
 * Add code to run WlanIST in SDIO callback.
 *
 * 10 19 2010 cp.wu
 * [WCXRP00000122] [MT6620 Wi-Fi][Driver] Preparation for YuSu source tree integration
 * remove HIF_SDIO_ONE flags because the settings could be merged for runtime detection instead of compile-time.
 *
 * 10 19 2010 jeffrey.chang
 * [WCXRP00000120] [MT6620 Wi-Fi][Driver] Refine linux kernel module to the license of MTK propietary and enable MTK HIF by default
 * Refine linux kernel module to the license of MTK and enable MTK HIF
 *
 * 08 21 2010 jeffrey.chang
 * NULL
 * 1) add sdio two setting
 * 2) bug fix of sdio glue
 *
 * 08 18 2010 jeffrey.chang
 * NULL
 * support multi-function sdio
 *
 * 08 18 2010 cp.wu
 * NULL
 * #if defined(__X86__) is not working, change to use #ifdef CONFIG_X86.
 *
 * 08 17 2010 cp.wu
 * NULL
 * add ENE SDIO host workaround for x86 linux platform.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 07 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * Fix hotplug bug
 *
 * 03 28 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * clear sdio interrupt
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/interrupt.h>
//#include <linux/kernel.h>
#include <linux/device.h>
//#include <linux/errno.h>
#include <linux/platform_device.h>
//#include <linux/fs.h>
//#include <linux/cdev.h>
//#include <linux/poll.h>

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>
#else

#endif

#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpt.h>

#include "gl_os.h"

#if defined(MT6620)
    #include "mt6620_reg.h"
#elif defined(MT5931)
    #include "mt5931_reg.h"
#elif defined(MT6628)
    #include "mtreg.h"
#endif

//#define MTK_DMA_BUF_MEMCPY_SUP /* no virt_to_phys() use */
//#define HIF_DEBUG_SUP
//#define HIF_DEBUG_SUP_TX

#ifdef HIF_DEBUG_SUP
#define HIF_DBG(msg)   printk msg
#else
#define HIF_DBG(msg)
#endif /* HIF_DEBUG_SUP */

#ifdef HIF_DEBUG_SUP_TX
#define HIF_DBG_TX(msg)   printk msg
#else
#define HIF_DBG_TX(msg)
#endif /* HIF_DEBUG_SUP */


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/


/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/


/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static void
HifAhbDmaEnhanceModeConf(
    IN GLUE_INFO_T                  *GlueInfo,
    IN UINT_32                       BurstLen,
    IN UINT_32                       PortId,
    IN UINT_32                       TransByte
    );

static irqreturn_t
HifAhbISR(
    IN int                          Irq,
    IN void                         *Arg
    );

static int
HifAhbProbe (
    VOID
    );

static int
HifAhbRemove (
    VOID
    );

#if (MTK_WCN_SINGLE_MODULE == 0)
static int
HifAhbBusCntGet(
    VOID
    );

static int
HifAhbBusCntClr(
    VOID
    );

static int HifTxCnt = 0;
#endif /* MTK_WCN_SINGLE_MODULE */

#if (CONF_HIF_DEV_MISC == 1)
static ssize_t 
HifAhbMiscRead(
    IN struct file                  *Filp,
    OUT char __user                 *DstBuf,
    IN size_t                       Size,
    IN loff_t                       *Ppos
    );

static ssize_t 
HifAhbMiscWrite(
    IN struct file                  *Filp,
    IN const char __user            *SrcBuf,
    IN size_t                       Size,
    IN loff_t                       *Ppos
    );

static int
HifAhbMiscIoctl(
    IN struct file                  *Filp,
    IN unsigned int                 Cmd,
    IN unsigned long                arg
    );

static int
HifAhbMiscOpen(
    IN struct inode                 *Inodep,
    IN struct file                  *Filp
    );

static int
HifAhbMiscClose(
    IN struct inode                 *Inodep,
    IN struct file                  *Filp
    );
#else

static int
HifAhbPltmProbe(
    IN struct platform_device       *PDev
    );

static int __exit
HifAhbPltmRemove(
    IN struct platform_device       *PDev
    );

#ifdef CONFIG_PM
static int
HifAhbPltmSuspend(
    IN struct platform_device       *PDev,
    pm_message_t                    Message
    );

static int
HifAhbPltmResume(
    IN struct platform_device       *PDev
    );
#endif /* CONFIG_PM */

#endif /* CONF_HIF_DEV_MISC */

#if (CONF_HIF_LOOPBACK_AUTO == 1) /* only for development test */
static VOID
HifAhbLoopbkAuto(
    IN unsigned long            arg
    );
#endif /* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
static irqreturn_t
HifDmaISR(
    IN int                          Irq,
    IN void                         *Arg
    );
#endif /* CONF_HIF_DMA_INT */



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
extern BOOLEAN fgIsResetting;


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* initialiation function from other module */
static probe_card pfWlanProbe = NULL;

/* release function from other module */
static remove_card pfWlanRemove = NULL;

/* DMA operator */
static GL_HIF_DMA_OPS_T *pfWlanDmaOps = NULL;

static BOOLEAN WlanDmaFatalErr = 0;

#if (CONF_HIF_DEV_MISC == 1)
static const struct file_operations MtkAhbOps = {
	.owner				= THIS_MODULE,
	.read				= HifAhbMiscRead,
	.write				= HifAhbMiscWrite,
	.unlocked_ioctl		= HifAhbMiscIoctl,
	.compat_ioctl		= HifAhbMiscIoctl,
	.open				= HifAhbMiscOpen,
	.release			= HifAhbMiscClose,
};

static struct miscdevice MtkAhbDriver = {
	.minor = MISC_DYNAMIC_MINOR, /* any minor number */
	.name  = HIF_MOD_NAME,
	.fops  = &MtkAhbOps,
};
#else

#ifdef CONFIG_OF
static const struct of_device_id apwifi_of_ids[] = {
	{ .compatible = "mediatek,WIFI", },
	{}
};
#endif

struct platform_driver MtkPltmAhbDriver = {
	.driver = {
		.name = "mt-wifi",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = apwifi_of_ids,
#endif
	},
	.probe = HifAhbPltmProbe,
#ifdef CONFIG_PM
	.suspend = HifAhbPltmSuspend,
	.resume = HifAhbPltmResume,
#else
    .suspend = NULL,
    .resume = NULL,
#endif /* CONFIG_PM */
	.remove = __exit_p ( HifAhbPltmRemove ),
};

static struct platform_device *HifAhbPDev;

#endif /* CONF_HIF_DEV_MISC */




/*******************************************************************************
*                       P U B L I C   F U N C T I O N S
********************************************************************************
*/

#if 0
/*set JTAG Mode for MCU*/
#define JTAG_ADDR1_BASE 0x10208000
#define JTAG_ADDR2_BASE 0x10005300
char *jtag_addr1 = (char *)JTAG_ADDR1_BASE;
char *jtag_addr2 = (char *)JTAG_ADDR2_BASE;

#define JTAG1_REG_WRITE(addr, value)    \
    writel(value, ((volatile UINT_32 *)(jtag_addr1+(addr-JTAG_ADDR1_BASE))))
#define JTAG1_REG_READ(addr)            \
    readl(((volatile UINT_32 *)(jtag_addr1+(addr-JTAG_ADDR1_BASE))))
#define JTAG2_REG_WRITE(addr, value)    \
    writel(value, ((volatile UINT_32 *)(jtag_addr2+(addr-JTAG_ADDR2_BASE))))
#define JTAG2_REG_READ(addr)            \
    readl(((volatile UINT_32 *)(jtag_addr2+(addr-JTAG_ADDR2_BASE))))

static int wmt_set_jtag_for_mcu(void)
{
    int iRet = 0;
	unsigned int tmp = 0;

    jtag_addr1 = ioremap(JTAG_ADDR1_BASE, 0x5000);
    if (jtag_addr1 == 0) {
        printk("remap jtag_addr1 fail!\n");
        return -1;
    }
    printk("jtag_addr1 = 0x%p\n", jtag_addr1);
    jtag_addr2 = ioremap(JTAG_ADDR2_BASE, 0x1000);
    if (jtag_addr2 == 0) {
        printk("remap jtag_addr2 fail!\n");
        return -1;
    }
    printk("jtag_addr2 = 0x%p\n", jtag_addr2);

	/*Enable IES of all pins*/
    JTAG1_REG_WRITE(0x10208004, 0xffffffff);
	JTAG1_REG_WRITE(0x10208014, 0xffffffff);
	JTAG1_REG_WRITE(0x10209004, 0xffffffff);
	JTAG1_REG_WRITE(0x1020A004, 0xffffffff);
	JTAG1_REG_WRITE(0x1020B004, 0xffffffff);

/*JTAG1 mode setting*/
	/*set GPIO pull mode*/
	/*0x1020A040[6:0]=0x7f*/
	tmp = JTAG1_REG_READ(0x1020A040);
	tmp &= 0xffffff80;
	tmp |= 0x7f;
	JTAG1_REG_WRITE(0x1020A040, tmp);

#if 0
	/*0x1020A060[6:0]=0x31*/
	tmp = JTAG1_REG_READ(0x1020A060);
	tmp &= 0xffffff80;
	tmp |= 0x31;
    JTAG1_REG_WRITE(0x1020A060, tmp);
	/*set CONSYS JTAG mode 0x10005300=0x07777777*/
	//DRV_WriteReg32(0x10005300, 0x07777777); /*remove JTAG mode1*/
	JTAG2_REG_WRITE(0x10005300, 0x00000000);
#endif

#if 1 // Chaozhong modified
    /*0x1020A060[6:0]=0x31*/
    tmp = JTAG1_REG_READ(0x1020A060);
    tmp &= 0xffffff80;
    tmp |= 0x31;
    JTAG1_REG_WRITE(0x1020A060, tmp);
    /*set CONSYS JTAG mode 0x10005300=0x07777777*/
    //DRV_WriteReg32(0x10005300, 0x07777777); /*remove JTAG mode1*/
    JTAG2_REG_WRITE(0x1000530C, 0x08888888);// Chaozhong modified
#endif


/*JTAG1 mode setting*/
    
 	/*set GPIO pull mode*/
	/*0x1020A050[6:0]=0x7f*/
	tmp = JTAG1_REG_READ(0x1020A050);
	tmp &= 0xffffff80;
	tmp |= 0x7f;
	JTAG1_REG_WRITE(0x1020A050, tmp);
	/*0x1020A070[6:0]=0x31*/
	tmp = JTAG1_REG_READ(0x1020A070);
	tmp &= 0xffffff80;
	tmp |= 0x31;
	JTAG1_REG_WRITE(0x1020A070, tmp);
	/*set CONSYS JTAG mode
	0x10005310=0x77111111
	0x10005320=0x00077777*/
	JTAG2_REG_WRITE(0x1000531C, 0xff111111);
	JTAG2_REG_WRITE(0x1000532C, 0x000fffff);

#if 0
    JTAG2_REG_WRITE(0x10005410, 0x77700000); /* GPIO141 ~ GPIO143 */
    JTAG2_REG_WRITE(0x10005420, 0x00000077); /* GPIO144 ~ GPIO145 */
    JTAG2_REG_WRITE(0x10005370, 0x70000000); /* GPIO63 */
    JTAG2_REG_WRITE(0x10005380, 0x00000777); /* GPIO64 ~ GPIO66 */
    JTAG2_REG_WRITE(0x100053a0, 0x70000000); /* GPIO87 */
    JTAG2_REG_WRITE(0x100053b0, 0x00000777); /* GPIO88 ~ GPIO90 */
    JTAG2_REG_WRITE(0x100053d0, 0x00777000); /* GPIO107 ~ GPIO109 */
#endif

#if 0
	/*set CONSYS debug flag mode*/
	JTAG2_REG_WRITE(0x1000540C, 0xffffffff);
	JTAG2_REG_WRITE(0x1000541C, 0xffffffff);
	JTAG2_REG_WRITE(0x1000538C, 0x11111fff);
	JTAG2_REG_WRITE(0x100053AC, 0xf1111111);
    JTAG2_REG_WRITE(0x100053BC, 0x00000fff);
    JTAG2_REG_WRITE(0x100053DC, 0xfffff001);
    JTAG2_REG_WRITE(0x100053EC, 0x1111100f);
	JTAG2_REG_WRITE(0x1000533C, 0x00ffffff);
	JTAG2_REG_WRITE(0x1000532C, 0xff000000);
#endif
	
	return iRet;
}


static int wmt_set_jtag_for_gps(void)
{
    int iRet = 0;
	unsigned int tmp = 0;
    /*JTAG1 mode setting*/
	/*set GPIO pull mode*/
	/*0x1020B040[21:16]=0x37, leave bit19 alone*/
	tmp = JTAG1_REG_READ(0x1020B040);
	tmp &= 0xffC8ffff;
	tmp |= (0x37 << 16);
	JTAG1_REG_WRITE(0x1020B040, tmp);
	
	/*0x1020B050[21:16]=0x20, leave bit19 alone*/
	tmp = JTAG1_REG_READ(0x1020B050);
	tmp &= 0xffC8ffff;
	tmp |= (0x20 << 16);
	JTAG1_REG_WRITE(0x1020B050, tmp);

	/*set CONSYS JTAG mode 0x100053B0=0x70777000*/
	/*set CONSYS JTAG mode 0x100053C0=0x00000007*/
	JTAG2_REG_WRITE(0x100053BC, 0xf0fff000);
	JTAG2_REG_WRITE(0x100053CC, 0x0000000F);

    /*CONSYS debug flag mode is already done in MCU jtag mode setting*/
	
    return iRet;
}
#endif /* 0 */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register sdio bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering HIF driver (WLAN_STATUS_SUCCESS = 0)
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
glRegisterBus (
    probe_card pfProbe,
    remove_card pfRemove
    )
{
    WLAN_STATUS Ret;


    ASSERT(pfProbe);
    ASSERT(pfRemove);

    printk("glRegisterBus\n");

//    printk(KERN_INFO "mtk_sdio: MediaTek SDIO WLAN driver\n");
//    printk(KERN_INFO "mtk_sdio: Copyright MediaTek Inc.\n");

    pfWlanProbe = pfProbe; /* wlan card initialization in other modules = wlanProbe() */
    pfWlanRemove = pfRemove;

#if (CONF_HIF_DEV_MISC == 1)
    if ((Ret = misc_register(&MtkAhbDriver)) != 0) {
//		printk(MOD_NAME " unable to register misc device\n");
		return Ret;
	}

    HifAhbProbe();
#else

    Ret = platform_driver_register(&MtkPltmAhbDriver);

#endif /* CONF_HIF_DEV_MISC */

    return Ret;

} /* end of glRegisterBus() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister sdio bus to the os
*
* \param[in] pfRemove   Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glUnregisterBus(
    remove_card pfRemove
    )
{
    ASSERT(pfRemove);

    pfRemove();

#if (CONF_HIF_DEV_MISC == 1)
    HifAhbRemove();

	if ((misc_deregister(&MtkAhbDriver)) != 0) {
		//printk(MOD_NAME " unable to de-register character device\n");
	}
#else

    platform_driver_unregister(&MtkPltmAhbDriver);
#endif /* CONF_HIF_DEV_MISC */

    return;

} /* end of glUnregisterBus() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will inform us whole chip reset start event.
*
* \param[in] GlueInfo   Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glResetHif (
    GLUE_INFO_T *GlueInfo
    )
{
    GL_HIF_INFO_T *HifInfo;


    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;
    if (HifInfo->DmaOps)
        HifInfo->DmaOps->DmaReset(HifInfo);
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function stores hif related info, which is initialized before.
*
* \param[in] GlueInfo Pointer to glue info structure
* \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glSetHifInfo (
    GLUE_INFO_T *GlueInfo,
    ULONG ulCookie
    )
{
    GL_HIF_INFO_T *HifInfo;


    /* Init HIF */
    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;
#if (CONF_HIF_DEV_MISC == 1)
    HifInfo->Dev = MtkAhbDriver.this_device;
#else
    HifInfo->Dev = &HifAhbPDev->dev;
#endif /* CONF_HIF_DEV_MISC */
    SET_NETDEV_DEV(GlueInfo->prDevHandler, HifInfo->Dev);

    HifInfo->HifRegBaseAddr = ioremap(HIF_DRV_BASE, HIF_DRV_LENGTH);
    HifInfo->McuRegBaseAddr = ioremap(CONN_MCU_DRV_BASE, CONN_MCU_REG_LENGTH);
    printk("HifInfo->HifRegBaseAddr=0x%p, HifInfo->McuRegBaseAddr=0x%p \n",
        HifInfo->HifRegBaseAddr, HifInfo->McuRegBaseAddr);

    /* default disable DMA */
    HifInfo->fgDmaEnable = FALSE;
    HifInfo->DmaRegBaseAddr = 0;
    HifInfo->DmaOps = NULL;

    /* read chip ID */
    HifInfo->ChipID = HIF_REG_READL(HifInfo, MCR_WCIR) & 0xFFFF;
    if(HifInfo->ChipID == 0x0321 || HifInfo->ChipID == 0x0335 
        || HifInfo->ChipID == 0x0337)
        HifInfo->ChipID = 0x6735; //Denali ChipID transition
    printk("[WiFi/HIF] ChipID = 0x%x\n", HifInfo->ChipID);


    /* Init DMA */
	WlanDmaFatalErr = 0; /* reset error flag */

#if (CONF_MTK_AHB_DMA == 1)
    spin_lock_init(&HifInfo->DdmaLock);

//    if (HifInfo->ChipID == MTK_CHIP_ID_6572)
//        HifPdmaInit(HifInfo);
//    else if (HifInfo->ChipID == MTK_CHIP_ID_6582)
        HifPdmaInit(HifInfo);

    pfWlanDmaOps = HifInfo->DmaOps;
#endif /* CONF_MTK_AHB_DMA */

    /* Start loopback test after 10 seconds */
#if (CONF_HIF_LOOPBACK_AUTO == 1) /* only for development test */
{
    extern int
        kalDevLoopbkThread(
            IN void *data
            );

    init_timer(&(HifInfo->HifTmrLoopbkFn));
    HifInfo->HifTmrLoopbkFn.function = HifAhbLoopbkAuto;
    HifInfo->HifTmrLoopbkFn.data = (unsigned long) GlueInfo;

    init_waitqueue_head(&HifInfo->HifWaitq);
    HifInfo->HifTaskLoopbkFn = kthread_run(\
                    kalDevLoopbkThread, GlueInfo->prDevHandler, "LoopbkThread");
    HifInfo->HifLoopbkFlg = 0;

    /* Note: in FPGA, clock is not accuracy so 3000 here, not 10000 */
    HifInfo->HifTmrLoopbkFn.expires = jiffies + MSEC_TO_SYSTIME(30000);
    add_timer(&(HifInfo->HifTmrLoopbkFn));

    HIF_DBG(("[WiFi/HIF] Start loopback test after 10 seconds (jiffies = %u)...\n",
            jiffies));
}
#endif /* CONF_HIF_LOOPBACK_AUTO */

#if (CONF_HIF_DMA_INT == 1)
    init_waitqueue_head(&HifInfo->HifDmaWaitq);
    HifInfo->HifDmaWaitFlg = 0;
#endif /* CONF_HIF_DMA_INT */

} /* end of glSetHifInfo() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glClearHifInfo (
    GLUE_INFO_T *GlueInfo
    )
{
	iounmap(GlueInfo->rHifInfo.HifRegBaseAddr);
	iounmap(GlueInfo->rHifInfo.DmaRegBaseAddr);
	iounmap(GlueInfo->rHifInfo.McuRegBaseAddr);
    return;

} /* end of glClearHifInfo() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glGetChipInfo (
    GLUE_INFO_T *GlueInfo,
	UINT_8 *pucChipBuf
    )
{
	GL_HIF_INFO_T *HifInfo;


	HifInfo = &GlueInfo->rHifInfo;
	printk("glGetChipInfo ChipID = 0x%x\n", HifInfo->ChipID);
	if (HifInfo->ChipID == MTK_CHIP_ID_6571)
		kalMemCopy(pucChipBuf, "6571", strlen("6571"));
	else if (HifInfo->ChipID == MTK_CHIP_ID_8127)
		kalMemCopy(pucChipBuf, "8127", strlen("8127"));
    else if (HifInfo->ChipID == MTK_CHIP_ID_6752)
		kalMemCopy(pucChipBuf, "6752", strlen("6752"));
    else if (HifInfo->ChipID == MTK_CHIP_ID_6735)
		kalMemCopy(pucChipBuf, "6752", strlen("6752"));
    else
		kalMemCopy(pucChipBuf, "SOC", strlen("SOC"));
} /* end of glGetChipInfo() */


#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
/*----------------------------------------------------------------------------*/
/*!
* \brief This function to check if we need wakelock under Hotspot mode.
*
* \param[in] GlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
glIsChipNeedWakelock (
    GLUE_INFO_T *GlueInfo
    )
{
	GL_HIF_INFO_T *HifInfo;

	HifInfo = &GlueInfo->rHifInfo;
	if (HifInfo->ChipID == MTK_CHIP_ID_6572 || HifInfo->ChipID == MTK_CHIP_ID_6582)
		return TRUE;
	else
        return FALSE;
} /* end of glIsChipNeedWakelock() */
#endif /* CFG_SPM_WORKAROUND_FOR_HOTSPOT */


/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize bus operation and hif related information, request resources.
*
* \param[out] pvData    A pointer to HIF-specific data type buffer.
*                       For eHPI, pvData is a pointer to UINT_32 type and stores a
*                       mapped base address.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
glBusInit (
    PVOID pvData
    )
{
    return TRUE;

} /* end of glBusInit() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus operation and release resources.
*
* \param[in] pvData A pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glBusRelease (
    PVOID pvData
    )
{
    return;

} /* end of glBusRelease() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Setup bus interrupt operation and interrupt handler for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pfnIsr     A pointer to interrupt handler function.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \retval WLAN_STATUS_SUCCESS   if success
*         NEGATIVE_VALUE   if fail
*/
/*----------------------------------------------------------------------------*/
//#ifdef CONFIG_OF  /*for MT6752*/
//#ifndef MT_WF_HIF_IRQ_ID
//#define MT_WF_HIF_IRQ_ID   270
//#endif
//#endif
#ifdef CONFIG_OF
INT_32
glBusSetIrq (
    PVOID pvData,
    PVOID pfnIsr,
    PVOID pvCookie
    )
{
    struct device_node *node = NULL;
    unsigned int irq_info[3] = {0, 0, 0};
    //unsigned int phy_base;
    unsigned int irq_id = 0;
    unsigned int irq_flags = 0;

    struct net_device *prNetDevice;

    ASSERT(pvData);
    if (!pvData) {
        return -1;
    }
    prNetDevice = (struct net_device *) pvData;

    node = of_find_compatible_node(NULL, NULL, "mediatek,WIFI");
    if(node){
        irq_id = irq_of_parse_and_map(node,0);
        /*fixme, be compitable arch 64bits*/
        //mtk_btif.base = (unsigned long)of_iomap(node, 0);
        //BTIF_INFO_FUNC("get btif irq(%d),register base(0x%lx)\n",
        //            mtk_btif.p_irq->irq_id,mtk_btif.base);
        printk("WIFI-OF: get wifi irq(%d)\n", irq_id);
    }else{
        printk("WIFI-OF: get wifi device node fail\n");
    }

    /* get the interrupt line behaviour */
    if (of_property_read_u32_array(node, "interrupts",
                    irq_info, ARRAY_SIZE(irq_info))){
        printk("WIFI-OF: get interrupt flag from DTS fail\n");
    }else{
        irq_flags = irq_info[2];
        printk("WIFI-OF: get interrupt flag(0x%x)\n", irq_flags);
    }

    /* Register AHB IRQ */
    if (request_irq(irq_id,
                    HifAhbISR,
                    irq_flags, HIF_MOD_NAME, prNetDevice)) {
        printk("WIFI-OF: request irq %d fail!\n", irq_id);
        return -1;
    }

    return 0;
}

VOID
glBusFreeIrq (
    PVOID pvData,
    PVOID pvCookie
    )
{
    struct device_node *node = NULL;
    unsigned int irq_info[3] = {0, 0, 0};
    //unsigned int phy_base;
    unsigned int irq_id = 0;
    unsigned int irq_flags = 0;

    struct net_device *prNetDevice;

    /* Init */
    ASSERT(pvData);
    if (!pvData) {
        //printk(KERN_INFO DRV_NAME"%s null pvData\n", __FUNCTION__);
        return;
    }
    prNetDevice = (struct net_device *) pvData;

    node = of_find_compatible_node(NULL, NULL, "mediatek,WIFI");
    if(node){
        irq_id = irq_of_parse_and_map(node,0);
        /*fixme, be compitable arch 64bits*/
        //mtk_btif.base = (unsigned long)of_iomap(node, 0);
        //BTIF_INFO_FUNC("get btif irq(%d),register base(0x%lx)\n",
        //            mtk_btif.p_irq->irq_id,mtk_btif.base);
        printk("WIFI-OF: get wifi irq(%d)\n", irq_id);
    }else{
        printk("WIFI-OF: get wifi device node fail\n");
    }

    /* get the interrupt line behaviour */
    if (of_property_read_u32_array(node, "interrupts",
                    irq_info, ARRAY_SIZE(irq_info))){
        printk("WIFI-OF: get interrupt flag from DTS fail\n");
    }else{
        irq_flags = irq_info[2];
        printk("WIFI-OF: get interrupt flag(0x%x)\n", irq_flags);
    }

    /* Free the IRQ */
    free_irq(irq_id, prNetDevice);
    return;

}

#else

/* the name is different in 72 and 82 */
#ifndef MT_WF_HIF_IRQ_ID /* for MT6572/82/92 */
#define MT_WF_HIF_IRQ_ID   WF_HIF_IRQ_ID
#endif /* MT_WF_HIF_IRQ_ID */

INT_32
glBusSetIrq (
    PVOID pvData,
    PVOID pfnIsr,
    PVOID pvCookie
    )
{
    int ret = 0;
    struct net_device *prNetDevice;
    GLUE_INFO_T *GlueInfo;
    GL_HIF_INFO_T *HifInfo;


    /* Init */
    ASSERT(pvData);
    if (!pvData) {
        return -1;
    }

    prNetDevice = (struct net_device *) pvData;
    GlueInfo = (GLUE_INFO_T *) pvCookie;
    ASSERT(GlueInfo);
    if (!GlueInfo) {
        printk("GlueInfo == NULL!\n");
        return -1;
    }

    HifInfo = &GlueInfo->rHifInfo;


    /* Register AHB IRQ */
    if (request_irq(MT_WF_HIF_IRQ_ID,
                    HifAhbISR,
                    IRQF_TRIGGER_LOW, HIF_MOD_NAME, prNetDevice)) {
        printk("request irq %d fail!\n", MT_WF_HIF_IRQ_ID);
        return -1;
    }

#if (CONF_HIF_DMA_INT == 1)
    if (request_irq(MT_GDMA2_IRQ_ID,
                    HifDmaISR,
                    IRQF_TRIGGER_LOW, "AHB_DMA", prNetDevice)) {
        printk("request irq %d fail!\n", MT_GDMA2_IRQ_ID);
        free_irq(MT_WF_HIF_IRQ_ID, prNetDevice);
        return -1;
    }
#endif /* CONF_HIF_DMA_INT */

    return ret;

} /* end of glBusSetIrq() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus interrupt operation and disable interrupt handling for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID
glBusFreeIrq (
    PVOID pvData,
    PVOID pvCookie
    )
{
    struct net_device *prNetDevice;
    GLUE_INFO_T *GlueInfo;
    GL_HIF_INFO_T *HifInfo;


    /* Init */
    ASSERT(pvData);
    if (!pvData) {
        //printk(KERN_INFO DRV_NAME"%s null pvData\n", __FUNCTION__);
        return;
    }

    prNetDevice = (struct net_device *) pvData;
    GlueInfo = (GLUE_INFO_T *) pvCookie;
    ASSERT(GlueInfo);
    if (!GlueInfo) {
        //printk(KERN_INFO DRV_NAME"%s no glue info\n", __FUNCTION__);
        return;
    }

    HifInfo = &GlueInfo->rHifInfo;


    /* Free the IRQ */
    free_irq(MT_WF_HIF_IRQ_ID, prNetDevice);
    return;

} /* end of glBusreeIrq() */
#endif


/*----------------------------------------------------------------------------*/
/*!
* \brief Read a 32-bit device register
*
* \param[in] GlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] RegOffset Register offset
* \param[in] pu4Value   Pointer to variable used to store read value
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevRegRead (
    IN  GLUE_INFO_T     *GlueInfo,
    IN  UINT_32         RegOffset,
    OUT UINT_32         *pu4Value
    )
{
    GL_HIF_INFO_T *HifInfo;


    /* sanity check and init */
    ASSERT(GlueInfo);
    ASSERT(pu4Value);
    HifInfo = &GlueInfo->rHifInfo;
    /* use PIO mode to read register */
	if (WlanDmaFatalErr && RegOffset != MCR_WCIR && RegOffset != MCR_WHLPCR)
		return FALSE;
    *pu4Value = HIF_REG_READL(HifInfo, RegOffset);

    if ((RegOffset == MCR_WRDR0) || (RegOffset == MCR_WRDR1))
    {
        HIF_DBG(("[WiFi/HIF] kalDevRegRead from Data Port 0 or 1\n"));
    }

    return TRUE;

} /* end of kalDevRegRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Write a 32-bit device register
*
* \param[in] GlueInfo   Pointer to the GLUE_INFO_T structure.
* \param[in] RegOffset  Register offset
* \param[in] RegValue   RegValue to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevRegWrite (
    IN GLUE_INFO_T      *GlueInfo,
    IN UINT_32          RegOffset,
    IN UINT_32          RegValue
    )
{
    GL_HIF_INFO_T *HifInfo;


    /* sanity check and init */
    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;
	if (WlanDmaFatalErr && RegOffset != MCR_WCIR && RegOffset != MCR_WHLPCR)
		return FALSE;

    /* use PIO mode to write register */
	if (WlanDmaFatalErr && RegOffset != MCR_WCIR && RegOffset != MCR_WHLPCR)
		return FALSE;
    HIF_REG_WRITEL(HifInfo, RegOffset, RegValue);

    if ((RegOffset == MCR_WTDR0) || (RegOffset == MCR_WTDR1))
    {
        HIF_DBG(("[WiFi/HIF] kalDevRegWrite to Data Port 0 or 1\n"));
    }

    return TRUE;

} /* end of kalDevRegWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Read device I/O port
*
* \param[in] GlueInfo   Pointer to the GLUE_INFO_T structure.
* \param[in] Port       I/O port offset
* \param[in] Size       Length to be read
* \param[out] Buf       Pointer to read buffer
* \param[in] MaxBufSize Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
#if (CFG_SUPPORT_HIF_FW_DN_DBG == 1)
UINT_8 ucBackTxBuf[4096];
UINT_32 ucBackLen = 0;
BOOLEAN fgDumpSpec = false;

UINT_8 *kalgetbackBuf(void)
{
	return (UINT_8 *)ucBackTxBuf;
}

UINT_32 kalgetbackLen(void)
{
	return ucBackLen;
}

BOOLEAN kalGetDumpSpec(void)
{
	return fgDumpSpec;
}

#endif

BOOLEAN
kalDevPortRead (
    IN  GLUE_INFO_T     *GlueInfo,
    IN  UINT_16         Port,
    IN  UINT_32         Size,
    OUT PUINT_8         Buf,
    IN  UINT_32         MaxBufSize
    )
{
    GL_HIF_INFO_T *HifInfo;
#if (CONF_MTK_AHB_DMA == 1)
    MTK_WCN_HIF_DMA_CONF DmaConf;
    UINT_32 LoopCnt;
#endif /* CONF_MTK_AHB_DMA */
    UINT_32 IdLoop, MaxLoop;
    UINT_32 *LoopBuf;
    unsigned long PollTimeout;

    /* sanity check */
    if ((WlanDmaFatalErr == 1) || (fgIsResetting == TRUE) ||
		(HifIsFwOwn(GlueInfo->prAdapter) == TRUE))
    {
		DBGLOG(RX, ERROR, ("dma err: %d, Resetting: %d, fw own: %d\n",
			WlanDmaFatalErr, fgIsResetting, HifIsFwOwn(GlueInfo->prAdapter)));
        return FALSE;
    }

    /* Init */
    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;

    ASSERT(Buf);
    ASSERT(Size <= MaxBufSize);

    /* Note: burst length should be equal to the one used in DMA */
    if (Port == MCR_WRDR0)
        HifAhbDmaEnhanceModeConf(GlueInfo, HIF_BURST_4DW, HIF_TARGET_RXD0, Size);
    else if (Port == MCR_WRDR1)
        HifAhbDmaEnhanceModeConf(GlueInfo, HIF_BURST_4DW, HIF_TARGET_RXD1, Size);
    else if (Port == MCR_WHISR)
        HifAhbDmaEnhanceModeConf(GlueInfo, HIF_BURST_4DW, HIF_TARGET_WHISR, Size);
    /* else other non-data port */


    /* Read */
#if (CONF_MTK_AHB_DMA == 1)
    if ((HifInfo->fgDmaEnable == TRUE) &&
        (HifInfo->DmaOps != NULL) &&
        ((Port == MCR_WRDR0) || (Port == MCR_WRDR1)))
    {
        /* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
        VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

        /* config DMA, Port = MCR_WRDR0 or MCR_WRDR1 */
        DmaConf.Count = Size;
        DmaConf.Dir = HIF_DMA_DIR_RX;
        DmaConf.Src = HIF_DRV_BASE + Port; /* must be physical addr */

        /* TODO: use dma_unmap_single(struct device *dev, dma_addr_t handle,
  size_t size, enum dma_data_direction dir) */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
        DmaConf.Dst = kalIOPhyAddrGet(Buf); /* must be physical addr */

        /* TODO: use virt_to_phys() */
        if (DmaConf.Dst == NULL)
        {
            HIF_DBG(("[WiFi/HIF] Use Dma Buffer to RX packet (%d %d)...\n",
                    Size, CFG_RX_MAX_PKT_SIZE));
            ASSERT(Size <= CFG_RX_MAX_PKT_SIZE);

            kalDmaBufGet(&DmaVBuf, &DmaPBuf);
            DmaConf.Dst = (ULONG)DmaPBuf;
        }
#else
        /*
            http://kernelnewbies.org/KernelMemoryAllocation
            Since the cache-coherent mapping may be expensive, also a streaming allocation exists.

            This is a buffer for one-way communication, which means coherency is limited to
            flushing the data from the cache after a write finishes. The buffer has to be
            pre-allocated (e.g. using kmalloc()). DMA for it is set up with dma_map_single().

            When the DMA is finished (e.g. when the device has sent an interrupt signaling end of
            DMA), call dma_unmap_single(). Between map and unmap, the device is in control of the
            buffer: if you write to the device, do it before dma_map_single(), if you read from
            it, do it after dma_unmap_single(). 
        */
        /* DMA_FROM_DEVICE invalidated (without writeback) the cache */
        /* TODO: if dst_off was not cacheline aligned? */
        DmaConf.Dst = dma_map_single(HifInfo->Dev, Buf, Size, DMA_FROM_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

        /* start to read data */
        AP_DMA_HIF_LOCK(HifInfo); /* lock to avoid other codes config GDMA */

        if (pfWlanDmaOps != NULL)
            pfWlanDmaOps->DmaClockCtrl(TRUE);

        HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
        HifInfo->DmaOps->DmaStart(HifInfo);

        /* TODO: use interrupt mode */
        /* TODO: use timeout, not loopcnt */
        /* TODO: use semaphore */
#if (CONF_HIF_DMA_INT == 1)
        RtnVal = wait_event_interruptible_timeout(
                    HifInfo->HifDmaWaitq, (HifInfo->HifDmaWaitFlg != 0), 1000);
        if (RtnVal <= 0)
        {
            while(1)
            printk("fatal error1! reset DMA!\n");
        }
        HifInfo->HifDmaWaitFlg = 0;
#else

        LoopCnt = 0;
        PollTimeout = jiffies + HZ*5;

        do {

            if (time_before(jiffies, PollTimeout))
                ; /* not timeout, continue to poll */
            else
            {

#if (CONF_HIF_CONNSYS_DBG == 0)
                /* TODO: impossible! reset DMA */
                printk("fatal error1! reset DMA!\n");
                break;
#else
                /*
                        never break and just wait for response from HIF
                
                        because when we use ICE on CONNSYS, we will break the CPU and do debug,
                        but maybe AP side continues to send packets to HIF, HIF buffer of CONNSYS
                        will be full and we will do DMA reset here then CONNSYS CPU will also be reset.
                    */
				printk("DMA LoopCnt > 100000... (%lu %lu)\n", jiffies, PollTimeout);
//LabelErr:
#if (CFG_SUPPORT_HIF_FW_DN_DBG == 1)
		{
			fgDumpSpec = TRUE;
			kalSmDump(GlueInfo);
			fgDumpSpec = FALSE;
		}
#endif
				/* HifRegDump(GlueInfo->prAdapter); */
				{
					UINT_32 uwcir = 0;
					UINT_32 uwhlpcr = 0;

					uwcir = (UINT_32)HIF_REG_READL(HifInfo, MCR_WCIR);
					uwhlpcr = (UINT_32)HIF_REG_READL(HifInfo, MCR_WHLPCR);
					printk("addr MCR_WCIR is %u, addr MCR_WHLPCR is %u\n", uwcir, uwhlpcr);
				}

                if (HifInfo->DmaOps->DmaRegDump != NULL)
                    HifInfo->DmaOps->DmaRegDump(HifInfo);

                LoopCnt = 0;
                WlanDmaFatalErr = 1;

#if (CONF_HIF_DMA_DBG == 0)
				{
	                UINT_32 FwCnt;
	                printk("CONNSYS FW CPUINFO:\n");
	                for(FwCnt=0; FwCnt<512; FwCnt++)
	                    printk("0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR)); // CONSYS_REG_READ(CONSYS_CPUPCR_REG)
	                printk("\n\n");
					if (!fgIsResetting)
						glDoCoreDump();
	                return FALSE;
				}
#endif /* CONF_HIF_DMA_DBG */
#endif /* CONF_HIF_CONNSYS_DBG */
            }
        } while(!HifInfo->DmaOps->DmaPollIntr(HifInfo));
#endif /* CONF_HIF_DMA_INT */

        HifInfo->DmaOps->DmaAckIntr(HifInfo);
        HifInfo->DmaOps->DmaStop(HifInfo);

        LoopCnt = 0;
        do {
            if (LoopCnt++ > 100000) {
                /* TODO: impossible! reset DMA */
                printk("fatal error2! reset DMA!\n");
                break;
            }
        } while(HifInfo->DmaOps->DmaPollStart(HifInfo) != 0);

        if (pfWlanDmaOps != NULL)
            pfWlanDmaOps->DmaClockCtrl(FALSE);

        AP_DMA_HIF_UNLOCK(HifInfo);

#ifdef MTK_DMA_BUF_MEMCPY_SUP
        if (DmaVBuf != NULL)
            kalMemCopy(Buf, DmaVBuf, Size);
#else
        dma_unmap_single(HifInfo->Dev, DmaConf.Dst, Size, DMA_FROM_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

        HIF_DBG(("[WiFi/HIF] DMA RX OK!\n"));
    }
    else
#endif /* CONF_MTK_AHB_DMA */
    {

        /* default PIO mode */
        MaxLoop = Size >> 2;
        if (Size & 0x3)
            MaxLoop ++;
        LoopBuf = (UINT_32 *)Buf;

        for(IdLoop=0; IdLoop<MaxLoop; IdLoop++) {

            *LoopBuf = HIF_REG_READL(HifInfo, Port);
            LoopBuf ++;
        }
    }

    return TRUE;

} /* end of kalDevPortRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port
*
* \param[in] GlueInfo   Pointer to the GLUE_INFO_T structure.
* \param[in] Port       I/O port offset
* \param[in] Size       Length to be write
* \param[in] Buf        Pointer to write buffer
* \param[in] MaxBufSize Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevPortWrite (
    IN GLUE_INFO_T      *GlueInfo,
    IN UINT_16          Port,
    IN UINT_32          Size,
    IN PUINT_8          Buf,
    IN UINT_32          MaxBufSize
    )
{
    GL_HIF_INFO_T *HifInfo;
#if (CONF_MTK_AHB_DMA == 1)
    MTK_WCN_HIF_DMA_CONF DmaConf;
    UINT_32 LoopCnt;
#endif /* CONF_MTK_AHB_DMA */
    UINT_32 IdLoop, MaxLoop;
    UINT_32 *LoopBuf;
    unsigned long PollTimeout;
    /* sanity check */
    if ((WlanDmaFatalErr == 1) || (fgIsResetting == TRUE) ||
		(HifIsFwOwn(GlueInfo->prAdapter) == TRUE))
    {
		DBGLOG(TX, ERROR, ("dma err: %d, Resetting: %d, fw own: %d\n",
					WlanDmaFatalErr, fgIsResetting, HifIsFwOwn(GlueInfo->prAdapter)));
        return FALSE;
    }

    /* Init */
    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;

    ASSERT(Buf);
    ASSERT(Size <= MaxBufSize);

    HifTxCnt ++;

    /* Note: burst length should be equal to the one used in DMA */
    if (Port == MCR_WTDR0)
        HifAhbDmaEnhanceModeConf(GlueInfo, HIF_BURST_4DW, HIF_TARGET_TXD0, Size);
    else if (Port == MCR_WTDR1)
        HifAhbDmaEnhanceModeConf(GlueInfo, HIF_BURST_4DW, HIF_TARGET_TXD1, Size);
    /* else other non-data port */

    /* Write */
#if (CONF_MTK_AHB_DMA == 1)
    if ((HifInfo->fgDmaEnable == TRUE) &&
        (HifInfo->DmaOps != NULL) &&
        ((Port == MCR_WTDR0) || (Port == MCR_WTDR1)))
    {
        /* only for data port */
#ifdef MTK_DMA_BUF_MEMCPY_SUP
        VOID *DmaVBuf = NULL, *DmaPBuf = NULL;
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

        /* config GDMA */
        HIF_DBG_TX(("[WiFi/HIF/DMA] Prepare to send data...\n"));
        DmaConf.Count = Size;
        DmaConf.Dir = HIF_DMA_DIR_TX;
        DmaConf.Dst = HIF_DRV_BASE + Port; /* must be physical addr */

#ifdef MTK_DMA_BUF_MEMCPY_SUP
        DmaConf.Src = kalIOPhyAddrGet(Buf); /* must be physical addr */

        /* TODO: use virt_to_phys() */
        if (DmaConf.Src == NULL)
        {
            HIF_DBG_TX(("[WiFi/HIF] Use Dma Buffer to TX packet (%d %d)...\n",
                    Size, CFG_RX_MAX_PKT_SIZE));
            ASSERT(Size <= CFG_RX_MAX_PKT_SIZE);

            kalDmaBufGet(&DmaVBuf, &DmaPBuf);
            DmaConf.Src = (ULONG)DmaPBuf;

            kalMemCopy(DmaVBuf, Buf, Size);
        }
#else

        /*
            http://kernelnewbies.org/KernelMemoryAllocation
            Since the cache-coherent mapping may be expensive, also a streaming allocation exists.

            This is a buffer for one-way communication, which means coherency is limited to
            flushing the data from the cache after a write finishes. The buffer has to be
            pre-allocated (e.g. using kmalloc()). DMA for it is set up with dma_map_single().

            When the DMA is finished (e.g. when the device has sent an interrupt signaling end of
            DMA), call dma_unmap_single(). Between map and unmap, the device is in control of the
            buffer: if you write to the device, do it before dma_map_single(), if you read from
            it, do it after dma_unmap_single(). 
        */
        /* DMA_TO_DEVICE writeback the cache */
        DmaConf.Src = dma_map_single(HifInfo->Dev, Buf, Size, DMA_TO_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */

        /* start to write */
        AP_DMA_HIF_LOCK(HifInfo);

        if (pfWlanDmaOps != NULL)
            pfWlanDmaOps->DmaClockCtrl(TRUE);

        HifInfo->DmaOps->DmaConfig(HifInfo, &DmaConf);
        HifInfo->DmaOps->DmaStart(HifInfo);

        /* TODO: use interrupt mode */
        /* TODO: use timeout, not loopcnt */
        /* TODO: use semaphore */
#if (CONF_HIF_DMA_INT == 1)
        RtnVal = wait_event_interruptible_timeout(
                    HifInfo->HifDmaWaitq, (HifInfo->HifDmaWaitFlg != 0), 1000);
        if (RtnVal <= 0)
        {
//            HifRegDump(GlueInfo->prAdapter);
//            if (HifInfo->DmaOps->DmaRegDump != NULL)
//                HifInfo->DmaOps->DmaRegDump(HifInfo);
//            kalSendAeeWarning(
            while(1)
            printk("fatal error1! reset DMA!\n");
        }
        HifInfo->HifDmaWaitFlg = 0;
#else

        LoopCnt = 0;
        PollTimeout = jiffies + HZ*5;

        do {
            if (time_before(jiffies, PollTimeout))
                continue; /* not timeout, continue to poll */

#if (CONF_HIF_CONNSYS_DBG == 0)
            /* TODO: impossible! reset DMA */
            printk("fatal error1! reset DMA!\n");
            break;
#else

			printk("DMA LoopCnt > 100000... (%lu %lu)\n", jiffies, PollTimeout);
//LabelErr:
#if (CFG_SUPPORT_HIF_FW_DN_DBG == 1)
			fgDumpSpec = TRUE;
			kalSmDump(GlueInfo);
			fgDumpSpec = FALSE;
#endif
			{
				UINT_32 uwcir = 0;
				UINT_32 uwhlpcr = 0;

				uwcir = (UINT_32)HIF_REG_READL(HifInfo, MCR_WCIR);
				uwhlpcr = (UINT_32)HIF_REG_READL(HifInfo, MCR_WHLPCR);
				printk("addr MCR_WCIR is %u, addr MCR_WHLPCR is %u\n", uwcir, uwhlpcr);
			}
            if (HifInfo->DmaOps->DmaRegDump != NULL)
                HifInfo->DmaOps->DmaRegDump(HifInfo);
            LoopCnt = 0;

            WlanDmaFatalErr = 1;

#if (CONF_HIF_DMA_DBG == 0)
			{
                UINT_32 FwCnt;
                printk("CONNSYS FW CPUINFO:\n");
                for(FwCnt=0; FwCnt<512; FwCnt++)
                    printk("0x%08x ", MCU_REG_READL(HifInfo, CONN_MCU_CPUPCR));
                printk("\n\n");
                if (!fgIsResetting)
					glDoCoreDump();
                return FALSE;
			}
#endif /* CONF_HIF_DMA_DBG */
#endif /* CONF_HIF_CONNSYS_DBG */
        } while(!HifInfo->DmaOps->DmaPollIntr(HifInfo));
#endif /* CONF_HIF_DMA_INT */

        HifInfo->DmaOps->DmaAckIntr(HifInfo);
        HifInfo->DmaOps->DmaStop(HifInfo);

        LoopCnt = 0;
        do {
            if (LoopCnt++ > 100000) {
                /* TODO: impossible! reset DMA */
                printk("fatal error2! reset DMA!\n");
                break;
            }
        } while(HifInfo->DmaOps->DmaPollStart(HifInfo) != 0);

        if (pfWlanDmaOps != NULL)
            pfWlanDmaOps->DmaClockCtrl(FALSE);

        AP_DMA_HIF_UNLOCK(HifInfo);

#ifndef MTK_DMA_BUF_MEMCPY_SUP
        dma_unmap_single(HifInfo->Dev, DmaConf.Src, Size, DMA_TO_DEVICE);
#endif /* MTK_DMA_BUF_MEMCPY_SUP */
#if (CFG_SUPPORT_HIF_FW_DN_DBG == 1)
	if (Size <= 4096) {
		memset(ucBackTxBuf, 0, sizeof(ucBackTxBuf));
		memcpy(ucBackTxBuf, Buf, Size);
		ucBackLen = Size;
	}
#endif

        HIF_DBG_TX(("[WiFi/HIF] DMA TX OK!\n"));
    }
    else
#endif /* CONF_MTK_AHB_DMA */
    {
        /* PIO mode */
        MaxLoop = Size >> 2;
        LoopBuf = (UINT_32 *)Buf;

        HIF_DBG_TX(("[WiFi/HIF/PIO] Prepare to send data (%d 0x%p-0x%p)...\n",
                Size, LoopBuf, (((UINT8 *)LoopBuf) + (Size & (~0x03)))));

        if (Size & 0x3)
            MaxLoop ++;

        for(IdLoop=0; IdLoop<MaxLoop; IdLoop++) {

//            HIF_DBG_TX(("0x%08x ", *LoopBuf));

            HIF_REG_WRITEL(HifInfo, Port, *LoopBuf);
            LoopBuf ++;
        }

        /* TODO: check with DE if we "must" pad 0x00? */
        /* write the last 4B with padding 0x00 */
#if 0
        if (Size & 0x3) {
            /* need to use another 4B buffer to write */
            *((UINT_32 *)Last4B) = 0;
            BufLast = Last4B;
            BufSrc = ((UINT8 *)LoopBuf);

            for(IdLoop=0; IdLoop<(Size & 0x3); IdLoop++) {
                *BufLast = *BufSrc;
                BufLast ++;
                BufSrc ++;
            }

            HIF_DBG_TX(("0x%08x ", *((UINT_32 *)Last4B)));
            HIF_REG_WRITEL(HifInfo, Port, *((UINT_32 *)Last4B));
        }
#endif

        HIF_DBG_TX(("\n\n"));
    }

    return TRUE;

} /* end of kalDevPortWrite() */


#if 0 /* only for SDIO HIF */
/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port in byte with CMD52
*
* \param[in] GlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] RegOffset             I/O port offset
* \param[in] RegData             Single byte of data to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
kalDevWriteWithSdioCmd52 (
    IN GLUE_INFO_T      *GlueInfo,
    IN UINT_32          RegOffset,
    IN UINT_8           RegData
    )
{
    GL_HIF_INFO_T *HifInfo;


    printk("kalDevWriteWithSdioCmd52\n");

    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;

    /* Write by using PIO mode */
    HIF_REG_WRITEB(HifInfo, RegOffset, RegData);

    return TRUE;

} /* end of kalDevWriteWithSdioCmd52() */
#endif


/*******************************************************************************
*                       P R I V A T E   F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO interrupt callback function
*
* \param[in] func  pointer to SDIO handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/
extern UINT_32 IsrCnt, IsrPassCnt;
static irqreturn_t
HifAhbISR(
    IN int                          Irq,
    IN void                         *Arg
    )
{
    struct net_device *prNetDevice = (struct net_device *)Arg;
    GLUE_INFO_T *GlueInfo;
    GL_HIF_INFO_T *HifInfo;


    /* Init */
	IsrCnt ++;
    ASSERT(prNetDevice);
    GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
    ASSERT(GlueInfo);

    if (!GlueInfo) {
        //printk(KERN_INFO DRV_NAME"No glue info in HifAhbISR()\n");
        return IRQ_HANDLED;
    }
    HifInfo = &GlueInfo->rHifInfo;

    if (GlueInfo->ulFlag & GLUE_FLAG_HALT) {
        HIF_REG_WRITEL(HifInfo, MCR_WHLPCR, WHLPCR_INT_EN_CLR);
        //printk(KERN_INFO DRV_NAME"GLUE_FLAG_HALT skip INT\n");
        return IRQ_HANDLED;
    }

    HIF_REG_WRITEL(HifInfo, MCR_WHLPCR, WHLPCR_INT_EN_CLR);

#if 0
    wlanISR(GlueInfo->prAdapter, TRUE);

    if (GlueInfo->u4Flag & GLUE_FLAG_HALT) {
        /* Should stop now... skip pending interrupt */
        //printk(KERN_INFO DRV_NAME"ignore pending interrupt\n");
    }
    else {
        wlanIST(GlueInfo->prAdapter);
    }
#endif

    /* lock 100ms to avoid suspend */
    kalHifAhbKalWakeLockTimeout(GlueInfo);

    /* Wake up main thread */
    set_bit(GLUE_FLAG_INT_BIT, &GlueInfo->ulFlag);

    /* when we got sdio interrupt, we wake up the tx servie thread */
    wake_up_interruptible(&GlueInfo->waitq);

	IsrPassCnt ++;
    return IRQ_HANDLED;

}


#if (CONF_HIF_DMA_INT == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO interrupt callback function
*
* \param[in] func  pointer to SDIO handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/

static irqreturn_t
HifDmaISR(
    IN int                          Irq,
    IN void                         *Arg
    )
{
    struct net_device *prNetDevice = (struct net_device *)Arg;
    GLUE_INFO_T *GlueInfo;
    GL_HIF_INFO_T *HifInfo;


    /* Init */
    ASSERT(prNetDevice);
    GlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDevice));
    ASSERT(GlueInfo);

    if (!GlueInfo) {
        //printk(KERN_INFO DRV_NAME"No glue info in HifAhbISR()\n");
        return IRQ_HANDLED;
    }
    HifInfo = &GlueInfo->rHifInfo;

    /* disable interrupt */
    HifInfo->DmaOps->DmaAckIntr(HifInfo);

    /* Wake up main thread */
    set_bit(1, &HifInfo->HifDmaWaitFlg);

    /* when we got sdio interrupt, we wake up the tx servie thread */
    wake_up_interruptible(&HifInfo->HifDmaWaitq);

    return IRQ_HANDLED;

}
#endif /* CONF_HIF_DMA_INT */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO probe function
*
* \param[in] func   pointer to SDIO handle
* \param[in] id     pointer to SDIO device id table
*
* \return void
*/
/*----------------------------------------------------------------------------*/
#include <mach/mt_gpio.h>

static int
HifAhbProbe (
    VOID
    )
{
    int Ret = 0;


    printk(KERN_INFO DRV_NAME "HifAhbProbe()\n");

//    ASSERT(dev);

    /* power on WiFi TX PA 3.3V and HIF GDMA clock */
{
#ifdef CONFIG_MTK_PMIC_MT6397
#ifdef MTK_EXTERNAL_LDO
	/* for 8127 tablet */
	mt_set_gpio_mode(GPIO51, GPIO_MODE_04); 
	mt_set_gpio_dir(GPIO51, GPIO_DIR_OUT); 
	mt_set_gpio_pull_enable(GPIO51, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO51, GPIO_PULL_UP);
#elif defined(MTK_ALPS_BOX_SUPPORT)
	/* for 8127 box */
	mt_set_gpio_mode(GPIO89, GPIO_MODE_04); 
	mt_set_gpio_dir(GPIO89, GPIO_DIR_OUT); 
	mt_set_gpio_pull_enable(GPIO89, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO89, GPIO_PULL_UP);
#else
	hwPowerOn(MT65XX_POWER_LDO_VGP4,VOL_3300,"WLAN");
#endif
#else
#ifdef CONFIG_OF  /*for MT6752*/
    extern INT_32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT_32 enable);
    //hwPowerOn(MT6325_POWER_LDO_VCN33, VOL_3300, "WLAN");
    mtk_wcn_consys_hw_wifi_paldo_ctrl(1); /* switch to HW mode */
#else  /*for MT6572/82/92*/
    extern void upmu_set_vcn33_on_ctrl_wifi(UINT_32 val);
    hwPowerOn(MT6323_POWER_LDO_VCN33_WIFI, VOL_3300, "WLAN");
    upmu_set_vcn33_on_ctrl_wifi(1); /* switch to HW mode */
#endif
#endif

//    enable_clock(MT_CG_APDMA_SW_CG, "WLAN");
//    if (pfWlanDmaOps != NULL)
//        pfWlanDmaOps->DmaClockCtrl(TRUE);
}

#if (CONF_HIF_DEV_MISC == 1)
    if (pfWlanProbe((PVOID)&MtkAhbDriver.this_device) != WLAN_STATUS_SUCCESS)
#else
    if (pfWlanProbe((PVOID)&HifAhbPDev->dev) != WLAN_STATUS_SUCCESS)
#endif /* CONF_HIF_DEV_MISC */
    {
        //printk(KERN_WARNING DRV_NAME"pfWlanProbe fail!call pfWlanRemove()\n");

        pfWlanRemove();
        Ret = -1;
    }

    return Ret;

}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function will do module remove.
*
* \param[in] None
*
* \return The result of remove (WLAN_STATUS_SUCCESS = 0)
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbRemove (
    VOID
    )
{
    printk(KERN_INFO DRV_NAME"HifAhbRemove()\n");

//    ASSERT(dev);
    pfWlanRemove();

{
#ifdef CONFIG_MTK_PMIC_MT6397
#ifdef MTK_EXTERNAL_LDO
	/* for 8127 tablet */
	mt_set_gpio_mode(GPIO51, GPIO_MODE_04); 
	mt_set_gpio_dir(GPIO51, GPIO_DIR_OUT); 
	mt_set_gpio_pull_enable(GPIO51, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO51, GPIO_PULL_DOWN);
#elif defined(MTK_ALPS_BOX_SUPPORT)
	/* for 8127 box */
	mt_set_gpio_mode(GPIO89, GPIO_MODE_04); 
	mt_set_gpio_dir(GPIO89, GPIO_DIR_OUT); 
	mt_set_gpio_pull_enable(GPIO89, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO89, GPIO_PULL_DOWN);
#else
	hwPowerDown(MT65XX_POWER_LDO_VGP4,"WLAN");
#endif
#else
#ifdef CONFIG_OF  /*for MT6752*/
    extern INT_32 mtk_wcn_consys_hw_wifi_paldo_ctrl(UINT_32 enable);
    mtk_wcn_consys_hw_wifi_paldo_ctrl(0); /* switch to SW mode */
    //hwPowerDown(MT6325_POWER_LDO_VCN33, "WLAN");
#else  /*for MT6572/82/92*/
    extern void upmu_set_vcn33_on_ctrl_wifi(UINT_32 val);
    upmu_set_vcn33_on_ctrl_wifi(0); /* switch to SW mode */
    hwPowerDown(MT6323_POWER_LDO_VCN33_WIFI, "WLAN");
#endif
#endif

//    disable_clock(MT_CG_APDMA_SW_CG, "WLAN");
//    if (pfWlanDmaOps != NULL)
//        pfWlanDmaOps->DmaClockCtrl(FALSE);
}

//    printk(KERN_INFO DRV_NAME"HifAhbRemove() done\n");
    return 0;

}


#if (MTK_WCN_SINGLE_MODULE == 0)
/*----------------------------------------------------------------------------*/
/*!
* \brief This function gets the TX count pass through HIF AHB bus.
*
* \param[in] None
*
* \return TX count
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbBusCntGet(
    VOID
    )
{
    return HifTxCnt;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function resets the TX count pass through HIF AHB bus.
*
* \param[in] None
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbBusCntClr(
    VOID
    )
{
    HifTxCnt = 0;
    return 0;
}
#endif /* MTK_WCN_SINGLE_MODULE */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function configs the DMA TX/RX settings before any real TX/RX.
*
* \param[in] GlueInfo       Pointer to the GLUE_INFO_T structure.
* \param[in] BurstLen       0(1DW), 1(4DW), 2(8DW), Others(Reserved)
* \param[in] PortId         0(TXD0), 1(TXD1), 2(RXD0), 3(RXD1), 4(WHISR enhance)
* \param[in] TransByte      Should be 4-byte align.
*
* \return void
*/
/*----------------------------------------------------------------------------*/
static void
HifAhbDmaEnhanceModeConf(
    IN GLUE_INFO_T      *GlueInfo,
    UINT_32              BurstLen,
    UINT_32              PortId,
    UINT_32              TransByte
    )
{
    GL_HIF_INFO_T *HifInfo;
	UINT_32 RegHSTCR;


    ASSERT(GlueInfo);
    HifInfo = &GlueInfo->rHifInfo;

    /*
        20130315
        Problem Description:
            HIF 92B issue, 4B problem between 2 block transmission
            HIF will have problems if the read length between 2 block
            transmission is 4B.
        Solution:
            Access any non-func0 register.
    */
	RegHSTCR = HIF_REG_READL(HifInfo, MCR_WHIER);

	RegHSTCR = HIF_REG_READL(HifInfo, MCR_HSTCR);
	RegHSTCR = \
		((BurstLen << HSTCR_AFF_BURST_LEN_OFFSET ) & HSTCR_AFF_BURST_LEN) | \
		((PortId << HSTCR_TRANS_TARGET_OFFSET ) & HSTCR_TRANS_TARGET) | \
		(((TransByte & 0x3) == 0)?(TransByte & HSTCR_HSIF_TRANS_CNT): \
                                    ((TransByte + 4) & HSTCR_HSIF_TRANS_CNT));
	HIF_REG_WRITEL(HifInfo, MCR_HSTCR, RegHSTCR);
	
//	RegHSTCR = HIF_REG_READL(HifInfo, MCR_HSTCR);
//	HIF_DBG(("[WiFi/HIF] HSTCR(0x%08x)\n", RegHSTCR));

}


VOID
glSetPowerState (
    IN GLUE_INFO_T  *GlueInfo,
    IN UINT_32      ePowerMode
    )
{
    return;
}


#if (CONF_HIF_DEV_MISC == 1)
/* no use */
static ssize_t 
HifAhbMiscRead(
    IN struct file              *Filp,
    OUT char __user             *DstBuf,
    IN size_t                   Size,
    IN loff_t                   *Ppos
    )
{
    return 0;
}


static ssize_t 
HifAhbMiscWrite(
    IN struct file              *Filp,
    IN const char __user        *SrcBuf,
    IN size_t                   Size,
    IN loff_t                   *Ppos
    )
{
    return 0;
}


static int
HifAhbMiscIoctl(
    IN struct file              *Filp,
    IN unsigned int             Cmd,
    IN unsigned long            arg
    )
{
    return 0;
}


static int
HifAhbMiscOpen(
    IN struct inode             *Inodep,
    IN struct file              *Filp
    )
{
    return 0;
}


static int
HifAhbMiscClose(
    IN struct inode             *Inodep,
    IN struct file              *Filp
    )
{
    return 0;
}
#else

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbPltmProbe(
    IN struct platform_device       *PDev
    )
{
    /*
        TODO: reference codes
        	struct resource *regs;

        	struct spi_master *master;
        	struct mt_spi_t *ms;

        	if(!request_mem_region(pdev->resource[0].start, 
        						   pdev->resource[0].end-pdev->resource[0].start + 1, 
        						   pdev->name)){
        		dev_err(&pdev->dev,"request_mem_region busy.\n");
        		return -EBUSY;
        	}
        	
        	regs = platform_get_resource ( pdev,IORESOURCE_MEM, 0 );
        	if(!regs){
        		dev_err(&pdev->dev,"get resource regs NULL.\n");
        		return -ENXIO;
        	}

        	irq = platform_get_irq(pdev,0);
        	if(irq  < 0) {
        		dev_err(&pdev->dev,"platform_get_irq error. get invalid irq\n");
        		return irq;
        	}
    */
    HifAhbPDev = PDev;

    printk("HifAhbPltmProbe\n");

#if (CONF_HIF_PMIC_TEST == 1)
    wmt_set_jtag_for_mcu();
    wmt_set_jtag_for_gps();

{
//    extern INT32 mtk_wcn_consys_hw_reg_ctrl(UINT32 on, UINT32 co_clock_en);
//    mtk_wcn_consys_hw_reg_ctrl(1, 0);
}
#endif /* CONF_HIF_PMIC_TEST */

#if (MTK_WCN_SINGLE_MODULE == 1)
    HifAhbProbe(); /* only for test purpose without WMT module */

#else

    /* register WiFi function to WMT */
    printk("mtk_wcn_wmt_wlan_reg\n");
{
    MTK_WCN_WMT_WLAN_CB_INFO WmtCb;
    WmtCb.wlan_probe_cb = HifAhbProbe;
    WmtCb.wlan_remove_cb = HifAhbRemove;
    WmtCb.wlan_bus_cnt_get_cb = HifAhbBusCntGet;
    WmtCb.wlan_bus_cnt_clr_cb = HifAhbBusCntClr;
    mtk_wcn_wmt_wlan_reg(&WmtCb);
}
#endif /* MTK_WCN_SINGLE_MODULE */
    return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int __exit
HifAhbPltmRemove(
    IN struct platform_device       *PDev
    )
{
#if (MTK_WCN_SINGLE_MODULE == 0)
    mtk_wcn_wmt_wlan_unreg();
#endif /* MTK_WCN_SINGLE_MODULE */

#if (CONF_HIF_PMIC_TEST == 1)
{
//    extern INT32 mtk_wcn_consys_hw_reg_ctrl(UINT32 on, UINT32 co_clock_en);
//    mtk_wcn_consys_hw_reg_ctrl(0, 0);
}
#endif /* CONF_HIF_PMIC_TEST */

    return 0;
}


#ifdef CONFIG_PM
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
* \param[in] Message        
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbPltmSuspend(
    IN struct platform_device       *PDev,
    pm_message_t                    Message
    )
{
    return 0;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is called by OS platform device module.
*
* \param[in] PDev           Pointer to the platform device structure.
*
* \return 0
*/
/*----------------------------------------------------------------------------*/
static int
HifAhbPltmResume(
    IN struct platform_device       *PDev
    )
{
    return 0;
}
#endif /* CONFIG_PM */

#endif /* CONF_HIF_DEV_MISC */



#if (CONF_HIF_LOOPBACK_AUTO == 1)
/*----------------------------------------------------------------------------*/
/*!
* \brief Trigger to do HIF loopback test.
*
* \param[in] arg   Pointer to the GLUE_INFO_T structure.
*
* \retval None
*/
/*----------------------------------------------------------------------------*/
static VOID
HifAhbLoopbkAuto(
    IN unsigned long            arg
    )
{

    P_GLUE_INFO_T GlueInfo = (P_GLUE_INFO_T) arg;
    GL_HIF_INFO_T *HifInfo = &GlueInfo->rHifInfo;


    ASSERT(GlueInfo);

    HIF_DBG(("[WiFi/HIF] Trigger to do loopback test...\n"));

    /* Notify tx thread  for timeout event */
//    set_bit(GLUE_FLAG_HIF_LOOPBK_AUTO_BIT, &GlueInfo->u4Flag);
//    wake_up_interruptible(&GlueInfo->waitq);

    set_bit(GLUE_FLAG_HIF_LOOPBK_AUTO_BIT, &HifInfo->HifLoopbkFlg);
    wake_up_interruptible(&HifInfo->HifWaitq);

    return;
}


#endif /* CONF_HIF_LOOPBACK_AUTO */

VOID glDumpConnSysCpuInfo(P_GLUE_INFO_T prGlueInfo) {
	GL_HIF_INFO_T *prHifInfo = &prGlueInfo->rHifInfo;
	unsigned short j = 0;
	for(; j<512; j++)
       printk("0x%08x \n", MCU_REG_READL(prHifInfo, CONN_MCU_CPUPCR)); // CONSYS_REG_READ(CONSYS_CPUPCR_REG)
}

/* End of ahb.c */
