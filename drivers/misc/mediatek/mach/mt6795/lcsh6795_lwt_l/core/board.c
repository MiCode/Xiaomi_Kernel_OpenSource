/* system header files */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/mtd/nand.h>

#include <asm/irq.h>
#include <asm/io.h>
//#include <asm/mach-types.h>
//#include <asm/mach/arch.h>
//#include <asm/mach/irq.h>
//#include <asm/mach/map.h>
//#include <asm/mach/time.h>
//#include <asm/setup.h>

#include <mach/system.h>
#include <mach/board.h>
#include <mach/hardware.h>
#include <mach/mt_gpio.h>
#include <mach/mt_bt.h>
#include <mach/eint.h>
#include <mach/mtk_rtc.h>
#include <mach/mt_typedefs.h>
// Fix-me: marked for early porting
#include <cust_gpio_usage.h>
#include <cust_eint.h>

#define CONFIG_EINT_DEVICE_TREE 0

#if CONFIG_EINT_DEVICE_TREE
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>

static atomic_t wifi_irq_flag;
static unsigned int wifi_irq = 0;
#endif

#include "board-custom.h"
#if defined(CONFIG_MTK_COMBO) || defined(CONFIG_MTK_COMBO_MODULE)
#include <mach/mtk_wcn_cmb_stub.h>
#endif

#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
static sdio_irq_handler_t mtk_wcn_cmb_sdio_eirq_handler = NULL;
int mtk_wcn_sdio_irq_flag_set (int falg);
static atomic_t sdio_irq_enable_flag;
static pm_callback_t mtk_wcn_cmb_sdio_pm_cb = NULL;
static void *mtk_wcn_cmb_sdio_pm_data = NULL;
static void *mtk_wcn_cmb_sdio_eirq_data = NULL;

const static u32 mtk_wcn_cmb_sdio_eint_pin = GPIO_WIFI_EINT_PIN;
const static u32 mtk_wcn_cmb_sdio_eint_num = CUST_EINT_WIFI_NUM;
const static u32 mtk_wcn_cmb_sdio_eint_m_eint = GPIO_WIFI_EINT_PIN_M_EINT;
const static u32 mtk_wcn_cmb_sdio_eint_m_gpio = GPIO_WIFI_EINT_PIN_M_GPIO;
/*
index: port number of combo chip (1:SDIO1, 2:SDIO2, no SDIO0)
value: slot power status of  (0:off, 1:on, 0xFF:invalid)
*/
    #if (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
        static unsigned char combo_port_pwr_map[4] = {0x0, 0xFF, 0xFF, 0xFF};
    #elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
        static unsigned char combo_port_pwr_map[4] = {0xFF, 0x0, 0xFF, 0xFF};
    #elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 2)
        static unsigned char combo_port_pwr_map[4] = {0xFF, 0xFF, 0x0, 0xFF};
    #elif (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
        static unsigned char combo_port_pwr_map[4] = {0xFF, 0xFF, 0xFF, 0x0};
    #else
        #error "unsupported CONFIG_MTK_WCN_CMB_SDIO_SLOT" CONFIG_MTK_WCN_CMB_SDIO_SLOT
    #endif
#else
	/*standalone chip's structure should be add here*/
#endif
/*=======================================================================*/
/* Board Specific Devices Power Management                               */
/*=======================================================================*/
extern kal_bool pmic_chrdet_status(void);

void mt_power_off(void)
{
	int count = 0;
	printk("mt_power_off\n");

	/* pull PWRBB low */
	rtc_bbpu_power_down();

	while (1) {
#if defined(CONFIG_POWER_EXT)
		//EVB
		printk("EVB without charger\n");
#else	
		//Phone
		mdelay(100);	
		printk("Phone with charger\n");
		if (pmic_chrdet_status() == KAL_TRUE || count > 10)
			arch_reset(0, "charger");

		count++;
#endif
    }
}

/*=======================================================================*/
/* Board Specific Devices                                                */
/*=======================================================================*/
/*GPS driver*/
/*FIXME: remove mt3326 notation */
struct mt3326_gps_hardware mt3326_gps_hw = {
    .ext_power_on =  NULL,
    .ext_power_off = NULL,
};


#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
static void mtk_wcn_cmb_sdio_enable_eirq(void)
{
#if CONFIG_EINT_DEVICE_TREE
	if(!atomic_read(&wifi_irq_flag))
	{
		printk(KERN_DEBUG "wifi eint has been enabled\n");;
	}
	else
	{
		enable_irq(wifi_irq);
		atomic_dec(&wifi_irq_flag);
		//printk(KERN_DEBUG " enable WIFI EINT irq %d !!\n",wifi_irq);
	}
#else
    mt_eint_unmask(mtk_wcn_cmb_sdio_eint_num);/* CUST_EINT_WIFI_NUM */
#endif
}


static void mtk_wcn_cmb_sdio_disable_eirq(void)
{
#if CONFIG_EINT_DEVICE_TREE
	if(atomic_read(&wifi_irq_flag))
	{
		printk(KERN_DEBUG "wifi eint has been disabled!\n");;
	}
	else
	{
		disable_irq_nosync(wifi_irq);
		atomic_inc(&wifi_irq_flag);
		//printk(KERN_DEBUG "disable WIFI EINT irq %d !!\n",wifi_irq);
	}
#else
    mt_eint_mask(mtk_wcn_cmb_sdio_eint_num); /* CUST_EINT_WIFI_NUM */
#endif
}

#if CONFIG_EINT_DEVICE_TREE
irqreturn_t mtk_wcn_cmb_sdio_eirq_handler_stub(int irq,void *data)
{
    if ((NULL != mtk_wcn_cmb_sdio_eirq_handler) && (0 != atomic_read(&sdio_irq_enable_flag))) {
        mtk_wcn_cmb_sdio_eirq_handler(mtk_wcn_cmb_sdio_eirq_data);
    }
	return IRQ_HANDLED;
}
#else
static void mtk_wcn_cmb_sdio_eirq_handler_stub(void)
{
    if ((NULL != mtk_wcn_cmb_sdio_eirq_handler) && (0 != atomic_read(&sdio_irq_enable_flag))) {
        mtk_wcn_cmb_sdio_eirq_handler(mtk_wcn_cmb_sdio_eirq_data);
    }
}
#endif

static void mtk_wcn_cmb_sdio_request_eirq(sdio_irq_handler_t irq_handler, void *data)
{
#if	CONFIG_EINT_DEVICE_TREE
	struct device_node *node;
	u32 ints[2] = {0,0};
	int ret = -EINVAL;
#endif
    printk( KERN_INFO "enter %s\n", __func__);
    mtk_wcn_sdio_irq_flag_set (0);
    mtk_wcn_cmb_sdio_eirq_data    = data;
    mtk_wcn_cmb_sdio_eirq_handler = irq_handler;
    #if 1
#if	CONFIG_EINT_DEVICE_TREE
	node = of_find_compatible_node(NULL, NULL, "mediatek, WIFI-eint");
	if(node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		mt_gpio_set_debounce(ints[0], ints[1]);
		wifi_irq = irq_of_parse_and_map(node, 0);
		ret = request_irq(wifi_irq, mtk_wcn_cmb_sdio_eirq_handler_stub, IRQF_TRIGGER_NONE,"WIFI-eint", NULL);
		printk(KERN_DEBUG "WIFI EINT irq %d !!\n",wifi_irq);
		atomic_set(&wifi_irq_flag, 0);/*default: irq enable*/

		if(ret)
			printk(KERN_ERR "WIFI EINT IRQ LINE NOT AVAILABLE!!\n");
		else {
			mtk_wcn_cmb_sdio_disable_eirq();/*not ,chip state is power off*/
		}
	}
	else
		printk(KERN_ERR "[%s] can't find wifi eint compatible node\n",__func__);
#else
    mt_eint_registration(mtk_wcn_cmb_sdio_eint_num,
            		CUST_EINT_WIFI_TYPE,
                mtk_wcn_cmb_sdio_eirq_handler_stub,
                0);
#endif
    #else
	{
		int i_ret = 0;
		i_ret = request_irq(mtk_wcn_cmb_sdio_eint_num,
				(irq_handler_t)mtk_wcn_cmb_sdio_eirq_handler_stub,
				IRQF_TRIGGER_LOW,
				"SDIO_EXT_IRQ",
				NULL);
		if (i_ret)
		    printk( KERN_ERR "request_irq for SDIO ext IRQ failed, i_ret(%d)\n", i_ret);
		else
		    printk( KERN_ERR "request_irq for SDIO ext IRQ succeed, i_ret(%d)\n", i_ret);
	}
    #endif
    
#if	CONFIG_EINT_DEVICE_TREE
#else
    mt_eint_mask(mtk_wcn_cmb_sdio_eint_num);/*CUST_EINT_WIFI_NUM */
#endif
	printk(KERN_INFO "exit %s\n", __func__);

}

static void mtk_wcn_cmb_sdio_register_pm(pm_callback_t pm_cb, void *data)
{
    printk( KERN_INFO "mtk_wcn_cmb_sdio_register_pm (0x%p, 0x%p)\n", pm_cb, data);
    /* register pm change callback */
    mtk_wcn_cmb_sdio_pm_cb = pm_cb;
    mtk_wcn_cmb_sdio_pm_data = data;
}

static void mtk_wcn_cmb_sdio_on (int sdio_port_num) {
    pm_message_t state = { .event = PM_EVENT_USER_RESUME };

    printk(KERN_INFO "mtk_wcn_cmb_sdio_on (%d) \n", sdio_port_num);

    /* 1. disable sdio eirq */
    mtk_wcn_cmb_sdio_disable_eirq();
    mt_set_gpio_pull_enable(mtk_wcn_cmb_sdio_eint_pin, GPIO_PULL_DISABLE); /* GPIO_WIFI_EINT_PIN */
    mt_set_gpio_mode(mtk_wcn_cmb_sdio_eint_pin, mtk_wcn_cmb_sdio_eint_m_eint); /* EINT mode */

    /* 2. call sd callback */
    if (mtk_wcn_cmb_sdio_pm_cb) {
        //printk(KERN_INFO "mtk_wcn_cmb_sdio_pm_cb(PM_EVENT_USER_RESUME, 0x%p, 0x%p) \n", mtk_wcn_cmb_sdio_pm_cb, mtk_wcn_cmb_sdio_pm_data);
        mtk_wcn_cmb_sdio_pm_cb(state, mtk_wcn_cmb_sdio_pm_data);
    }
    else {
        printk(KERN_WARNING "mtk_wcn_cmb_sdio_on no sd callback!!\n");
    }
}

static void mtk_wcn_cmb_sdio_off (int sdio_port_num) {
    pm_message_t state = { .event = PM_EVENT_USER_SUSPEND };

    printk(KERN_INFO "mtk_wcn_cmb_sdio_off (%d) \n", sdio_port_num);

    /* 1. call sd callback */
    if (mtk_wcn_cmb_sdio_pm_cb) {
        //printk(KERN_INFO "mtk_wcn_cmb_sdio_off(PM_EVENT_USER_SUSPEND, 0x%p, 0x%p) \n", mtk_wcn_cmb_sdio_pm_cb, mtk_wcn_cmb_sdio_pm_data);
        mtk_wcn_cmb_sdio_pm_cb(state, mtk_wcn_cmb_sdio_pm_data);
    }
    else {
        printk(KERN_WARNING "mtk_wcn_cmb_sdio_off no sd callback!!\n");
    }

    /* 2. disable sdio eirq */
    mtk_wcn_cmb_sdio_disable_eirq();
    /*printk(KERN_INFO "[mt6620] set WIFI_EINT input pull down\n");*/
    mt_set_gpio_mode(mtk_wcn_cmb_sdio_eint_pin, mtk_wcn_cmb_sdio_eint_m_gpio); /* GPIO mode */
    mt_set_gpio_dir(mtk_wcn_cmb_sdio_eint_pin, GPIO_DIR_IN);
    mt_set_gpio_pull_select(mtk_wcn_cmb_sdio_eint_pin, GPIO_PULL_UP);
    mt_set_gpio_pull_enable(mtk_wcn_cmb_sdio_eint_pin, GPIO_PULL_ENABLE);
}
int board_sdio_ctrl (unsigned int sdio_port_num, unsigned int on) {
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT)
			sdio_port_num = CONFIG_MTK_WCN_CMB_SDIO_SLOT;
			printk(KERN_WARNING "mt_combo_sdio_ctrl: force set sdio port to (%d)\n", sdio_port_num);
#endif
    if ((sdio_port_num >= 4) || (combo_port_pwr_map[sdio_port_num] == 0xFF) ) {
        /* invalid sdio port number or slot mapping */
        printk(KERN_WARNING "mt_mtk_wcn_cmb_sdio_ctrl invalid port(%d, %d)\n", sdio_port_num, combo_port_pwr_map[sdio_port_num]);
        return -1;
    }
    /*printk(KERN_INFO "mt_mtk_wcn_cmb_sdio_ctrl (%d, %d)\n", sdio_port_num, on);*/

    if (!combo_port_pwr_map[sdio_port_num] && on) {
    	#if 1
    	  printk(KERN_WARNING  "board_sdio_ctrl force off before on\n");
        mtk_wcn_cmb_sdio_off(sdio_port_num);
      #else
      	printk(KERN_WARNING  "skip sdio off before on\n");
     	#endif
        combo_port_pwr_map[sdio_port_num] = 0;
        /* off -> on */
        mtk_wcn_cmb_sdio_on(sdio_port_num);
        combo_port_pwr_map[sdio_port_num] = 1;
    }
    else if (combo_port_pwr_map[sdio_port_num] && !on) {
        /* on -> off */
        mtk_wcn_cmb_sdio_off(sdio_port_num);
        combo_port_pwr_map[sdio_port_num] = 0;
    }
    else {
        return -2;
    }
    return 0;
}
EXPORT_SYMBOL(board_sdio_ctrl);

int mtk_wcn_sdio_irq_flag_set (int flag)
{

    if (0 != flag)
    {
        atomic_set(&sdio_irq_enable_flag, 1);
    }
    else
    {
        atomic_set(&sdio_irq_enable_flag, 0);
    }
    printk(KERN_INFO  "sdio_irq_enable_flag:%d\n", atomic_read(&sdio_irq_enable_flag));

    return atomic_read(&sdio_irq_enable_flag);
}
EXPORT_SYMBOL(mtk_wcn_sdio_irq_flag_set);

#endif /* end of defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) */
/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

#if defined(CONFIG_WLAN)

int mt_wifi_resume(pm_message_t state)
{
    int evt = state.event;

    if (evt != PM_EVENT_USER_RESUME && evt != PM_EVENT_RESUME) {
        return -1;
    }

    /*printk(KERN_INFO "[WIFI] %s Resume\n", evt == PM_EVENT_RESUME ? "PM":"USR");*/
    return 0;
}

int mt_wifi_suspend(pm_message_t state)
{
    int evt = state.event;


    if (evt != PM_EVENT_USER_SUSPEND && evt != PM_EVENT_SUSPEND) {
        return -1;
    }

    return 0;
}

void mt_wifi_power_on(void)
{
    pm_message_t state = { .event = PM_EVENT_USER_RESUME };

    (void)mt_wifi_resume(state);
}
EXPORT_SYMBOL(mt_wifi_power_on);

void mt_wifi_power_off(void)
{
    pm_message_t state = { .event = PM_EVENT_USER_SUSPEND };

    (void)mt_wifi_suspend(state);
}
EXPORT_SYMBOL(mt_wifi_power_off);

#endif /* end of defined(CONFIG_WLAN) */

/* Board Specific Devices                                                */
/*=======================================================================*/

/*=======================================================================*/
/* Board Specific Devices Init                                           */
/*=======================================================================*/

/*=======================================================================*/
/* Board Devices Capability                                              */
/*=======================================================================*/
#define MSDC_SDIO_FLAG    (MSDC_EXT_SDIO_IRQ | MSDC_HIGHSPEED | MSDC_UHS1)

#if defined(CFG_DEV_MSDC0)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 0)
    struct msdc_hw msdc0_hw = {
        .clk_src        = MSDC_CLKSRC_200MHZ,
        .cmd_edge       = MSDC_SMPL_FALLING,
        .rdata_edge 	  = MSDC_SMPL_FALLING,
        .wdata_edge 	  = MSDC_SMPL_FALLING,
        .clk_drv        = 2,
        .cmd_drv        = 2,
        .dat_drv        = 2,
        .rst_drv        = 2,
        .ds_drv         = 2,
        .data_pins      = 8,
        .data_offset    = 0,
        .flags          = MSDC_SDIO_FLAG,
        .dat0rddly      = 0,
        .dat1rddly      = 0,
        .dat2rddly      = 0,
        .dat3rddly      = 0,
        .dat4rddly      = 0,
        .dat5rddly      = 0,
        .dat6rddly      = 0,
        .dat7rddly      = 0,
        .datwrddly      = 0,
        .cmdrrddly      = 0,
        .cmdrddly       = 0,
        .host_function	= MSDC_SDIO,
        .boot           = MSDC_BOOT_EN,
        .request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
        .enable_sdio_eirq  = mtk_wcn_cmb_sdio_enable_eirq,
        .disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
        .register_pm       = mtk_wcn_cmb_sdio_register_pm,
    };
#elif defined(CONFIG_MTK_EMMC_SUPPORT)
    struct msdc_ett_settings msdc0_ett_settings[] = {
        // common ett settings 
        { MSDC_HS200_MODE, 0xb0,  (0x7 << 7), 0x4 }, //PATCH_BIT0[INT_DAT_LATCH_CK_SEL]
        { MSDC_HS200_MODE, 0xb0,  (0x1f<<10), 0x0 }, //PATCH_BIT0[CKGEN_MSDC_DLY_SEL]
        { MSDC_HS400_MODE, 0xb0,  (0x7 << 7), 0x14 }, //PATCH_BIT0[INT_DAT_LATCH_CK_SEL]
        { MSDC_HS400_MODE, 0xb0,  (0x1f<<10), 0x0 }, //PATCH_BIT0[CKGEN_MSDC_DLY_SEL]
        { MSDC_HS400_MODE, 0x188, (0x1f<< 2), 0xb }, //EMMC50_PAD_DS_TUNE[DSDLY1]
        { MSDC_HS400_MODE, 0x188, (0x1f<<12), 0x1a }, //EMMC50_PAD_DS_TUNE[DSDLY3]

        // command & resp ett settings
        { MSDC_HS200_MODE, 0xb4,  (0x7 << 3), 0x0 }, //PATCH_BIT1[CMD_RSP_TA_CNTR]
        { MSDC_HS200_MODE, 0x4,   (0x1 << 1), 0x2 }, //MSDC_IOCON[R_SMPL]
        { MSDC_HS200_MODE, 0xec,  (0x1f<<16), 0x0 }, //PAD_TUNE[CMD_RXDLY]
        { MSDC_HS200_MODE, 0xec,  (0x1f<<22), 0x1 }, //PAD_TUNE[CMD_RESP_RXDLY]

        { MSDC_HS400_MODE, 0xb4,  (0x7 << 3), 0x0 }, //PATCH_BIT1[CMD_RSP_TA_CNTR]
        { MSDC_HS400_MODE, 0x4,   (0x1 << 1), 0x11 }, //MSDC_IOCON[R_SMPL]
        { MSDC_HS400_MODE, 0xec,  (0x1f<<16), 0x0 }, //PAD_TUNE[CMD_RXDLY]
        { MSDC_HS400_MODE, 0xec,  (0x1f<<22), 0x1 }, //PAD_TUNE[CMD_RESP_RXDLY]

        // write ett settings
        { MSDC_HS200_MODE, 0xb4,  (0x7 << 0), 0x0 }, //PATCH_BIT1[WRDAT_CRCS_TA_CNTR]
        { MSDC_HS200_MODE, 0xec,  (0x1f<< 0), 0x1 }, //PAD_TUNE[DAT_WR_RXDLY]
        { MSDC_HS200_MODE, 0x4,   (0x1 <<10), 0x2 }, //MSDC_IOCON[WD0_SMPL]
        { MSDC_HS200_MODE, 0xf0,  (0x1f<<24), 0x0 }, //DAT_RD_DLY0[DAT0_RD_DLY]

        // read ett settings
        { MSDC_HS200_MODE, 0xec,  (0x1f<< 8), 0x0 }, //PAD_TUNE[DAT_RD_RXDLY]
        { MSDC_HS200_MODE, 0x4,   (0x1 << 2), 0x0 }, //MSDC_IOCON[R_D_SMPL]
    };
    struct msdc_hw msdc0_hw = {
        .clk_src        = MSDC_CLKSRC_200MHZ,
        .cmd_edge       = MSDC_SMPL_FALLING,
        .rdata_edge 	  = MSDC_SMPL_FALLING,
        .wdata_edge 	  = MSDC_SMPL_FALLING,
        .clk_drv        = 2,
        .cmd_drv        = 2,
        .dat_drv        = 2,
        .rst_drv        = 2,
        .ds_drv         = 2,
        .data_pins      = 8,
        .data_offset    = 0,
#ifndef CONFIG_MTK_EMMC_CACHE
        .flags          = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR | MSDC_HS400,
#else
        .flags          = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_CACHE | MSDC_UHS1 |MSDC_DDR | MSDC_HS400, 
#endif
        .dat0rddly      = 0,
        .dat1rddly      = 0,
        .dat2rddly      = 0,
        .dat3rddly      = 0,
        .dat4rddly      = 0,
        .dat5rddly      = 0,
        .dat6rddly      = 0,
        .dat7rddly      = 0,
        .datwrddly      = 0,
        .cmdrrddly      = 0,
        .cmdrddly       = 0,
        .ett_count      = 20, //should be same with ett_settings array size
        .ett_settings   = (struct msdc_ett_settings *)msdc0_ett_settings,
        .host_function	= MSDC_EMMC,
        .boot           = MSDC_BOOT_EN,
    };
#else
	struct msdc_hw msdc0_hw = {
        .clk_src              = MSDC_CLKSRC_200MHZ, //MSDC50_CLKSRC_200MHZ,
        .cmd_edge             = MSDC_SMPL_FALLING,
        .rdata_edge           = MSDC_SMPL_FALLING,
        .wdata_edge           = MSDC_SMPL_FALLING,
        .clk_drv              = 1,
        .cmd_drv              = 1,
        .dat_drv              = 1,
        .clk_drv_sd_18        = 1,         /* sdr104 mode */
        .cmd_drv_sd_18        = 1,
        .dat_drv_sd_18        = 1,
        .clk_drv_sd_18_sdr50	= 1,         /* sdr50 mode */
        .cmd_drv_sd_18_sdr50	= 1,
        .dat_drv_sd_18_sdr50	= 1,
        .clk_drv_sd_18_ddr50	= 1,         /* ddr50 mode */
        .cmd_drv_sd_18_ddr50	= 1,
        .dat_drv_sd_18_ddr50	= 1,
        .data_pins            = 4,
        .data_offset          = 0,
      //#ifdef CUST_EINT_MSDC1_INS_NUM
      #if 0
        .flags                = MSDC_SYS_SUSPEND | MSDC_CD_PIN_EN | MSDC_REMOVABLE | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR,    
      #else
        .flags                = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR,
      #endif
        .dat0rddly            = 0,
        .dat1rddly            = 0,
        .dat2rddly            = 0,
        .dat3rddly            = 0,
        .dat4rddly            = 0,
        .dat5rddly            = 0,
        .dat6rddly            = 0,
        .dat7rddly            = 0,
        .datwrddly            = 0,
        .cmdrrddly            = 0,
        .cmdrddly             = 0,
        .ett_count            = 0, //should be same with ett_settings array size
        .host_function        = MSDC_SD,
        .boot	                = 0,
        .cd_level             = MSDC_CD_HIGH,
    };
#endif
#endif

#if defined(CFG_DEV_MSDC1)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 1)
    struct msdc_hw msdc1_hw = {
        .clk_src              = MSDC_CLKSRC_200MHZ,
        .cmd_edge             = MSDC_SMPL_FALLING,
        .rdata_edge           = MSDC_SMPL_FALLING,
        .wdata_edge           = MSDC_SMPL_FALLING,
        .clk_drv              = 1,
        .cmd_drv              = 1,
        .dat_drv              = 1,
        .clk_drv_sd_18        = 2,         /* sdr104 mode */
        .cmd_drv_sd_18        = 2,
        .dat_drv_sd_18        = 2,
        .clk_drv_sd_18_sdr50	= 2,         /* sdr50 mode */
        .cmd_drv_sd_18_sdr50	= 2,
        .dat_drv_sd_18_sdr50	= 2,
        .clk_drv_sd_18_ddr50	= 2,         /* ddr50 mode */
        .cmd_drv_sd_18_ddr50	= 2,
        .dat_drv_sd_18_ddr50	= 2,
        .data_pins            = 4,
        .data_offset          = 0,
        .flags                = MSDC_SDIO_FLAG,
        .dat0rddly            = 0,
        .dat1rddly            = 0,
        .dat2rddly            = 0,
        .dat3rddly            = 0,
        .dat4rddly            = 0,
        .dat5rddly            = 0,
        .dat6rddly            = 0,
        .dat7rddly            = 0,
        .datwrddly            = 0,
        .cmdrrddly            = 0,
        .cmdrddly             = 0,
        .host_function        = MSDC_SDIO,
        .boot	                = 0,
        .cd_level             = MSDC_CD_HIGH,
        .request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
        .enable_sdio_eirq  = mtk_wcn_cmb_sdio_enable_eirq,
        .disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
        .register_pm       = mtk_wcn_cmb_sdio_register_pm,      
    };
#else
    struct msdc_hw msdc1_hw = {
        .clk_src              = MSDC_CLKSRC_200MHZ,
        .cmd_edge             = MSDC_SMPL_FALLING,
        .rdata_edge           = MSDC_SMPL_FALLING,
        .wdata_edge           = MSDC_SMPL_FALLING,
        .clk_drv              = 2,
        .cmd_drv              = 2,
        .dat_drv              = 2,
        .clk_drv_sd_18        = 3,         /* sdr104 mode */
        .cmd_drv_sd_18        = 2,
        .dat_drv_sd_18        = 2,
        .clk_drv_sd_18_sdr50	= 2,         /* sdr50 mode */
        .cmd_drv_sd_18_sdr50	= 2,
        .dat_drv_sd_18_sdr50	= 2,
        .clk_drv_sd_18_ddr50	= 2,         /* ddr50 mode */
        .cmd_drv_sd_18_ddr50	= 2,
        .dat_drv_sd_18_ddr50	= 2,
        .data_pins            = 4,
        .data_offset          = 0,
#ifdef CUST_EINT_MSDC1_INS_NUM
        //changed by zhoulingyun
        //.flags                = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR,
         .flags                = MSDC_SYS_SUSPEND ,
#else
       // .flags                = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR,   
        .flags                = MSDC_SYS_SUSPEND ,
#endif
        .dat0rddly            = 0,
        .dat1rddly            = 0,
        .dat2rddly            = 0,
        .dat3rddly            = 0,
        .dat4rddly            = 0,
        .dat5rddly            = 0,
        .dat6rddly            = 0,
        .dat7rddly            = 0,
        .datwrddly            = 0,
        .cmdrrddly            = 0,
        .cmdrddly             = 0,
        .ett_count            = 0, //should be same with ett_settings array size
        .host_function        = MSDC_SD,
        .boot	                = 0,
        .cd_level             = MSDC_CD_HIGH,
    };
#endif
#endif

#if defined(CFG_DEV_MSDC2)
#if defined(CONFIG_MTK_COMBO_SDIO_SLOT) && (CONFIG_MTK_COMBO_SDIO_SLOT == 2)
    /* MSDC2 settings for MT66xx combo connectivity chip */
    struct msdc_hw msdc2_hw = {	    
        .clk_src        = MSDC_CLKSRC_200MHZ,
        .cmd_edge       = MSDC_SMPL_FALLING,
        .rdata_edge     = MSDC_SMPL_FALLING,
        .wdata_edge     = MSDC_SMPL_FALLING,
        .clk_drv        = 1,
        .cmd_drv        = 1,
        .dat_drv        = 1,
        .data_pins      = 4,
        .data_offset    = 0,
        //MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume, does not support hot plug
        .flags          = MSDC_SDIO_FLAG,//MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,
        .dat0rddly      = 0,
        .dat1rddly      = 0,
        .dat2rddly      = 0,
        .dat3rddly      = 0,
        .dat4rddly      = 0,
        .dat5rddly      = 0,
        .dat6rddly      = 0,
        .dat7rddly      = 0,
        .datwrddly      = 0,
        .cmdrrddly      = 0,
        .cmdrddly       = 0,
        .host_function	= MSDC_SDIO,
        .boot           = 0,
        .request_sdio_eirq = combo_sdio_request_eirq,
        .enable_sdio_eirq  = combo_sdio_enable_eirq,
        .disable_sdio_eirq = combo_sdio_disable_eirq,
        .register_pm       = combo_sdio_register_pm,
    };
#else
    struct msdc_hw msdc2_hw = {
        .clk_src              = MSDC_CLKSRC_200MHZ,
        .cmd_edge             = MSDC_SMPL_FALLING,
        .rdata_edge           = MSDC_SMPL_FALLING,
        .wdata_edge           = MSDC_SMPL_FALLING,
        .clk_drv              = 2,
        .cmd_drv              = 2,
        .dat_drv              = 2,
        .clk_drv_sd_18        = 3,         /* sdr104 mode */
        .cmd_drv_sd_18        = 2,
        .dat_drv_sd_18        = 2,
        .clk_drv_sd_18_sdr50	= 2,         /* sdr50 mode */
        .cmd_drv_sd_18_sdr50	= 2,
        .dat_drv_sd_18_sdr50	= 2,
        .clk_drv_sd_18_ddr50	= 2,         /* ddr50 mode */
        .cmd_drv_sd_18_ddr50	= 2,
        .dat_drv_sd_18_ddr50	= 2,
        .data_pins            = 4,
        .data_offset          = 0,
        .flags                = MSDC_SYS_SUSPEND | MSDC_HIGHSPEED | MSDC_UHS1 |MSDC_DDR,  
        .dat0rddly	          = 0,
        .dat1rddly	          = 0,
        .dat2rddly	          = 0,
        .dat3rddly	          = 0,
        .dat4rddly	          = 0,
        .dat5rddly	          = 0,
        .dat6rddly	          = 0,
        .dat7rddly	          = 0,
        .datwrddly	          = 0,
        .cmdrrddly	          = 0,
        .cmdrddly             = 0,
        .ett_count            = 0, //should be same with ett_settings array size
        .host_function        = MSDC_SD,
        .boot	                = 0,
        .cd_level             = MSDC_CD_HIGH,
};
#endif
#endif

#if defined(CFG_DEV_MSDC3)
#if defined(CONFIG_MTK_WCN_CMB_SDIO_SLOT) && (CONFIG_MTK_WCN_CMB_SDIO_SLOT == 3)
    /* MSDC3 settings for MT66xx combo connectivity chip */
    struct msdc_hw msdc3_hw = {
        .clk_src        = MSDC_CLKSRC_200MHZ,
        .cmd_edge       = MSDC_SMPL_FALLING,
        .rdata_edge     = MSDC_SMPL_FALLING,
        .wdata_edge     = MSDC_SMPL_FALLING,
        .clk_drv        = 4,
        .cmd_drv        = 4,
        .dat_drv        = 4,
        .data_pins      = 4,
        .data_offset    = 0,
        //MT6620 use External IRQ, wifi uses high speed. here wifi manage his own suspend and resume, does not support hot plug
        .flags          = MSDC_SDIO_FLAG,//MSDC_SYS_SUSPEND | MSDC_WP_PIN_EN | MSDC_CD_PIN_EN | MSDC_REMOVABLE,
        .dat0rddly      = 0,
        .dat1rddly      = 0,
        .dat2rddly      = 0,
        .dat3rddly      = 0,
        .dat4rddly      = 0,
        .dat5rddly      = 0,
        .dat6rddly      = 0,
        .dat7rddly      = 0,
        .datwrddly      = 0,
        .cmdrrddly      = 0,
        .cmdrddly       = 0,
        .cmdrtactr_sdr50        = 0x1,
        .wdatcrctactr_sdr50     = 0x1,
        .intdatlatcksel_sdr50   = 0x0,
        .cmdrtactr_sdr200       = 0x3,
        .wdatcrctactr_sdr200    = 0x3,
        .intdatlatcksel_sdr200  = 0x0,
        .ett_count              = 0, //should be same with ett_settings array size
        .host_function	= MSDC_SDIO,
        .boot      	    = 0,
        .request_sdio_eirq = mtk_wcn_cmb_sdio_request_eirq,
        .enable_sdio_eirq  = mtk_wcn_cmb_sdio_enable_eirq,
        .disable_sdio_eirq = mtk_wcn_cmb_sdio_disable_eirq,
        .register_pm       = mtk_wcn_cmb_sdio_register_pm,
	};
#endif
#endif

/* MT6575 NAND Driver */
#if defined(CONFIG_MTK_MTD_NAND)
struct mt6575_nand_host_hw mt6575_nand_hw = {
    .nfi_bus_width          = 8,
	.nfi_access_timing		= NFI_DEFAULT_ACCESS_TIMING,
	.nfi_cs_num				= NFI_CS_NUM,
	.nand_sec_size			= 512,
	.nand_sec_shift			= 9,
	.nand_ecc_size			= 2048,
	.nand_ecc_bytes			= 32,
	.nand_ecc_mode			= NAND_ECC_HW,
};
#endif
