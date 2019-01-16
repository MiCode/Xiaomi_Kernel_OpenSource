#include <generated/autoconf.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/memblock.h>
#include <asm/setup.h>
#include <asm/mach/arch.h>
#include <linux/sysfs.h>
#include <asm/io.h>
#include <linux/spi/spi.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/musb/musb.h>
#include <linux/musbfsh.h>
#include "mach/memory.h"
#include "mach/irqs.h"
#include <mach/mt_reg_base.h>
#include <mach/devs.h>
#include <mach/mt_boot.h>
#include <linux/version.h>
#include <mach/mtk_memcfg.h>
#include <mach/mtk_ccci_helper.h>
#include <linux/mrdump.h>
#include <linux/of_fdt.h>
#include <linux/types.h>
#ifdef I2C_E2_ECO
#include <mach/sync_write.h>
#endif

#define SERIALNO_LEN 32
static char serial_number[SERIALNO_LEN];

extern BOOTMODE get_boot_mode(void);
extern u32 get_devinfo_with_index(u32 index);
extern u32 g_devinfo_data[];
extern u32 g_devinfo_data_size;
extern void adjust_kernel_cmd_line_setting_for_console(char*, char*);
phys_addr_t mtk_get_max_DRAM_size(void);
#ifdef CONFIG_MTK_ECCCI_DRIVER
extern int ccci_parse_meta_md_setting(unsigned char args[]);
extern void ccci_md_mem_reserve(void);
#endif

#if 0 //def CONFIG_MTK_XHCI
#if def CONFIG_USB_MTK_DUALMODE
extern int mtk_xhci_eint_iddig_init(void);
#endif
#endif

struct {
	u32 base;
	u32 size;
} bl_fb = {0, 0};

static int use_bl_fb = 0;

/*=======================================================================*/
/*   USB GADGET                                                     */
/*=======================================================================*/
static u64 usb_dmamask = DMA_BIT_MASK(32);
static struct musb_hdrc_config musb_config_mt65xx = {
	.multipoint     = true,
	.dyn_fifo       = true,
	.soft_con       = true,
	.dma            = true,
	.num_eps        = 16,
	.dma_channels   = 8,
};

static struct musb_hdrc_platform_data usb_data = {
#ifdef CONFIG_USB_MTK_OTG
	.mode           = MUSB_OTG,
#else
	.mode           = MUSB_PERIPHERAL,
#endif
	.config         = &musb_config_mt65xx,
};
struct platform_device mt_device_usb = {
	.name		  = "mt_usb",
	.id		  = -1,   //only one such device
	.dev = {
		.platform_data          = &usb_data,
		.dma_mask               = &usb_dmamask,
		.coherent_dma_mask      = DMA_BIT_MASK(32),
        /*.release=musbfsh_hcd_release,*/
	},
};

#if 0 /*CONFIG_USB_MU3D_DRV*/

static struct musb_hdrc_config mtu3d_config = {
	.multipoint     = false,
	/* FIXME:Seems no need*/
	/* .soft_con       = true,*/
	.dma            = true,

	.num_eps        = 9, /*EP0 ~ EP8*/
	.dma_channels   = 8,
	.ram_bits       = 12,
};

static struct musb_hdrc_platform_data mtu3d_data = {
#ifdef CONFIG_USB_GADGET_MUSB_HDRC
	.mode	= MUSB_OTG,
#else
	.mode	= MUSB_PERIPHERAL,
#endif
	.config	= &mtu3d_config,
};

static struct resource mtu3d_resources[] = {
	{ /* physical address */
		.start	= USB3_BASE,
		.end 	= USB3_BASE + 0x2000 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{ /* general IRQ */
		.start	= SSUSB_DEV_INT_ID,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device mtu3d_dev = {
	.name	= "musb-mtu3d",
	.id	= -1,
	.dev = {
		.platform_data		= &mtu3d_data,
		.dma_mask		= &usb_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= mtu3d_resources,
	.num_resources	= ARRAY_SIZE(mtu3d_resources),
};

#endif

/*=======================================================================*/
/*   USB11 Host                      */
/*=======================================================================*/
#ifndef CONFIG_OF
#if defined(CONFIG_MTK_USBFSH)
static u64 usb11_dmamask = DMA_BIT_MASK(32);
extern void musbfsh_hcd_release (struct device *dev);

static struct musbfsh_hdrc_config musbfsh_config_mt65xx = {
	.multipoint     = false,
	.dyn_fifo       = true,
	.soft_con       = true,
	.dma            = true,
	.num_eps        = 6, /*EP0 ~ EP5, shoulde match epx_cfg*/
	.dma_channels   = 4,
};
static struct musbfsh_hdrc_platform_data usb_data_mt65xx = {
	.mode           = 1,
	.config         = &musbfsh_config_mt65xx,
};
static struct platform_device mt_usb11_dev = {
	.name           = "musbfsh_hdrc",
	.id             = -1,
	.dev = {
		.platform_data          = &usb_data_mt65xx,
		.dma_mask               = &usb11_dmamask,
		.coherent_dma_mask      = DMA_BIT_MASK(64),
		.release		= musbfsh_hcd_release,
	},
};
#endif
#endif

/*=======================================================================*/
/*   UART Ports                                                     */
/*=======================================================================*/
#if defined(CFG_DEV_UART0)
static struct resource mtk_resource_uart0[] = {
	{
		.start		= IO_VIRT_TO_PHYS(AP_UART0_BASE),
		.end		= IO_VIRT_TO_PHYS(AP_UART0_BASE) + MTK_UART_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= UART0_IRQ_BIT_ID,
		.flags		= IORESOURCE_IRQ,
	},
};
#endif

#if defined(CFG_DEV_UART1)
static struct resource mtk_resource_uart1[] = {
	{
		.start		= IO_VIRT_TO_PHYS(AP_UART1_BASE),
		.end		= IO_VIRT_TO_PHYS(AP_UART1_BASE) + MTK_UART_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= UART1_IRQ_BIT_ID,
		.flags		= IORESOURCE_IRQ,
	},
};
#endif

#if defined(CFG_DEV_UART2)
static struct resource mtk_resource_uart2[] = {
	{
		.start		= IO_VIRT_TO_PHYS(AP_UART2_BASE),
		.end		= IO_VIRT_TO_PHYS(AP_UART2_BASE) + MTK_UART_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= UART2_IRQ_BIT_ID,
		.flags		= IORESOURCE_IRQ,
	},
};
#endif

#if defined(CFG_DEV_UART3)
static struct resource mtk_resource_uart3[] = {
	{
		.start		= IO_VIRT_TO_PHYS(AP_UART3_BASE),
		.end		= IO_VIRT_TO_PHYS(AP_UART3_BASE) + MTK_UART_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= UART3_IRQ_BIT_ID,
		.flags		= IORESOURCE_IRQ,
	},
};
#endif

static struct platform_device mtk_device_btif = {
    .name			= "mtk_btif",
    .id				= -1,
};
#if (defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT))
static struct platform_device mtk_device_ext_md_ctl = {
    .name			= "ext-md-ctl",
    .id				= 0,
};
#endif
extern unsigned long max_pfn;
#define RESERVED_MEM_MODEM  (0x0) // do not reserve memory in advance, do it in mt_fixup
#ifndef CONFIG_RESERVED_MEM_SIZE_FOR_PMEM
#define CONFIG_RESERVED_MEM_SIZE_FOR_PMEM 	1
#endif

#if defined(CONFIG_MTK_FB)
char temp_command_line[1024] = {0};
extern unsigned int DISP_GetVRamSizeBoot(char* cmdline);
#define RESERVED_MEM_SIZE_FOR_FB (DISP_GetVRamSizeBoot((char*)&temp_command_line))
extern void   mtkfb_set_lcm_inited(bool isLcmInited);
#ifdef CONFIG_OF
extern phys_addr_t mtkfb_get_fb_base(void);
#endif
#else
#define RESERVED_MEM_SIZE_FOR_FB (0x400000)
#endif

/*
 * The memory size reserved for PMEM
 *
 * The size could be varied in different solutions.
 * The size is set in mt65xx_fixup function.
 * -   in develop should be 0x3600000
 * -   SQC should be 0x0
 */
__u64 RESERVED_MEM_SIZE_FOR_PMEM = 0x0;
__u64 pmem_start = 0x12345678;  // pmem_start is inited in mt_fixup
__u64 kernel_mem_sz = 0x0;       // kernel_mem_sz is inited in mt_fixup
__u64 RESERVED_MEM_SIZE_FOR_TEST_3D = 0x0;
__u64 FB_SIZE_EXTERN = 0x0;
__u64 RESERVED_MEM_SIZE_FOR_FB_MAX = 0x1500000;

#define TOTAL_RESERVED_MEM_SIZE (RESERVED_MEM_SIZE_FOR_PMEM + \
                                 RESERVED_MEM_SIZE_FOR_FB)

#define MAX_PFN        ((max_pfn << PAGE_SHIFT) + PHYS_OFFSET)

#define PMEM_MM_START  (pmem_start)
#define PMEM_MM_SIZE   (RESERVED_MEM_SIZE_FOR_PMEM)

#define TEST_3D_START  (PMEM_MM_START + PMEM_MM_SIZE)
#define TEST_3D_SIZE   (RESERVED_MEM_SIZE_FOR_TEST_3D)

#define FB_START      (TEST_3D_START + RESERVED_MEM_SIZE_FOR_TEST_3D)
#define FB_SIZE       (RESERVED_MEM_SIZE_FOR_FB)

static struct platform_device mtk_device_uart[] = {

    #if defined(CFG_DEV_UART0)
    {
    	.name			= "mtk-uart",
    	.id				= 0,
    	.num_resources	= ARRAY_SIZE(mtk_resource_uart0),
    	.resource		= mtk_resource_uart0,
    },
    #endif
    #if defined(CFG_DEV_UART1)
    {
    	.name			= "mtk-uart",
    	.id				= 1,
    	.num_resources	= ARRAY_SIZE(mtk_resource_uart1),
    	.resource		= mtk_resource_uart1,
    },
    #endif
    #if defined(CFG_DEV_UART2)
    {
    	.name			= "mtk-uart",
    	.id				= 2,
    	.num_resources	= ARRAY_SIZE(mtk_resource_uart2),
    	.resource		= mtk_resource_uart2,
    },
    #endif

    #if defined(CFG_DEV_UART3)
    {
    	.name			= "mtk-uart",
    	.id				= 3,
    	.num_resources	= ARRAY_SIZE(mtk_resource_uart3),
    	.resource		= mtk_resource_uart3,
    },
    #endif
};
//FIX-ME: marked for early porting
#if defined(CONFIG_FIQ_DEBUGGER)
extern void fiq_uart_fixup(int uart_port);
extern struct platform_device mt_fiq_debugger;
#endif

#define MAX_DFO_ENTRIES 32
struct dfo_db {
    tag_dfo_boot dfo_tag[MAX_DFO_ENTRIES];
    int cnt;
};

/* ========================================================================= */
/* implementation of serial number attribute                                 */
/* ========================================================================= */
#define to_sysinfo_attribute(x) container_of(x, struct sysinfo_attribute, attr)

struct sysinfo_attribute{
    struct attribute attr;
    ssize_t (*show)(char *buf);
    ssize_t (*store)(const char *buf, size_t count);
};

static struct kobject sn_kobj;

static ssize_t sn_show(char *buf){
    return snprintf(buf, 4096, "%s\n", serial_number);
}

struct sysinfo_attribute sn_attr = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
    .attr = {"serial_number", THIS_MODULE, 0644},
#else
    .attr = {"serial_number", 0644},
#endif
    .show = sn_show,
    .store = NULL
};

static ssize_t sysinfo_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
    struct sysinfo_attribute *sysinfo_attr = to_sysinfo_attribute(attr);
    ssize_t ret = -EIO;

    if(sysinfo_attr->show)
        ret = sysinfo_attr->show(buf);

    return ret;
}

static ssize_t sysinfo_store(struct kobject *kobj, struct attribute *attr, const char *buf, size_t count)
{
    struct sysinfo_attribute *sysinfo_attr = to_sysinfo_attribute(attr);
    ssize_t ret = -EIO;

    if(sysinfo_attr->store)
        ret = sysinfo_attr->store(buf, count);

    return ret;
}

static struct sysfs_ops sn_sysfs_ops = {
    .show = sysinfo_show,
    .store = sysinfo_store
};

static struct attribute *sn_attrs[] = {
    &sn_attr.attr,
    NULL
};

static struct kobj_type sn_ktype = {
    .sysfs_ops = &sn_sysfs_ops,
    .default_attrs = sn_attrs
};

#define HASH_ARRAY_SIZE 4

static char udc_chr[32] = {"ABCDEFGHIJKLMNOPQRSTUVWSYZ456789"};
int get_serial(uint64_t hwkey, uint32_t chipid, char ser[SERIALNO_LEN])
{
    uint16_t hashkey[HASH_ARRAY_SIZE];
    int idx, ser_idx;
    uint32_t digit, id;
    uint64_t tmp = hwkey;

    memset(ser, 0x00, SERIALNO_LEN);

    /* split to 4 key with 16-bit width each */
    tmp = hwkey;
    for (idx = 0; idx < HASH_ARRAY_SIZE; idx++) {
        hashkey[idx] = (uint16_t)(tmp & 0xffff);
        tmp >>= 16;
    }

    /* hash the key with chip id */
    id = chipid;
    for (idx = 0; idx < HASH_ARRAY_SIZE; idx++) {
        digit = (id % 10);
        hashkey[idx] = (hashkey[idx] >> digit) | (hashkey[idx] << (16-digit));
        id = (id / 10);
    }

    /* generate serail using hashkey */
    ser_idx = 0;
    for (idx = 0; idx < HASH_ARRAY_SIZE; idx++) {
        ser[ser_idx++] = (hashkey[idx] & 0x001f);
        ser[ser_idx++] = (hashkey[idx] & 0x00f8) >> 3;
        ser[ser_idx++] = (hashkey[idx] & 0x1f00) >> 8;
        ser[ser_idx++] = (hashkey[idx] & 0xf800) >> 11;
    }
    for (idx = 0; idx < ser_idx; idx++)
        ser[idx] = udc_chr[(int)ser[idx]];
    ser[ser_idx] = 0x00;
    return 0;
}

/*=======================================================================*/
/*   GPIO                                                           */
/*=======================================================================*/
struct platform_device gpio_dev =
{
    .name = "mt-gpio",
    .id   = -1,
};
struct platform_device fh_dev =
{
    .name = "mt-freqhopping",
    .id   = -1,
};

#if defined(CONFIG_SERIAL_AMBA_PL011)
static struct amba_device uart1_device =
{
    .dev =
    {
        .coherent_dma_mask = ~0,
        .init_name = "dev:f1",
        .platform_data = NULL,
    },
    .res =
    {
        .start  = 0xE01F1000,
        .end = 0xE01F1000 + SZ_4K - 1,
        .flags  = IORESOURCE_MEM,
    },
    .dma_mask = ~0,
    .irq = MT_UART1_IRQ_ID,
};
#endif

/*=======================================================================*/
/*   MSDC Hosts                                                       */
/*=======================================================================*/
#if defined(CFG_DEV_MSDC0)
static struct resource mt_resource_msdc0[] = {
    {
        .start  = IO_VIRT_TO_PHYS(MSDC0_BASE),
        .end    = IO_VIRT_TO_PHYS(MSDC0_BASE) + 0x228,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = MSDC0_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};
#endif

#if defined(CFG_DEV_MSDC1)
static struct resource mt_resource_msdc1[] = {
    {
        .start  = IO_VIRT_TO_PHYS(MSDC1_BASE),
        .end    = IO_VIRT_TO_PHYS(MSDC1_BASE) + 0x108,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = MSDC1_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};
#endif

#if defined(CFG_DEV_MSDC2)
static struct resource mt_resource_msdc2[] = {
    {
        .start  = IO_VIRT_TO_PHYS(MSDC2_BASE),
        .end    = IO_VIRT_TO_PHYS(MSDC2_BASE) + 0x108,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = MSDC2_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};
#endif

#if defined(CFG_DEV_MSDC3)
static struct resource mt_resource_msdc3[] = {
    {
        .start  = IO_VIRT_TO_PHYS(MSDC3_BASE),
        .end    = IO_VIRT_TO_PHYS(MSDC3_BASE) + 0x108,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = MSDC3_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};
#endif

#if defined(CONFIG_MTK_FB)
static u64 mtkfb_dmamask = ~(u32)0;

static struct resource resource_fb[] = {
	{
		.start		= 0, /* Will be redefined later */
		.end		= 0,
		.flags		= IORESOURCE_MEM
	}
};

static struct platform_device mt6575_device_fb = {
    .name = "mtkfb",
    .id   = 0,
    .num_resources = ARRAY_SIZE(resource_fb),
    .resource      = resource_fb,
    .dev = {
        .dma_mask = &mtkfb_dmamask,
        .coherent_dma_mask = 0xffffffff,
    },
};
#endif

/*=======================================================================*/
/* MT65xx XHCI Hosts                                                     */
/*=======================================================================*/

#if 0//defined(CONFIG_MTK_XHCI)

#if defined(CONFIG_MTK_LM_MODE)
#define XHCI_DMA_BIT_MASK DMA_BIT_MASK(64)
#else
#define XHCI_DMA_BIT_MASK DMA_BIT_MASK(32)
#endif

static u64 xhci_dma_mask = XHCI_DMA_BIT_MASK;
static struct resource resource_xhci[] = {
	{
		.start		= IO_VIRT_TO_PHYS(USB3_BASE), /* Will be redefined later */
		.end		= IO_VIRT_TO_PHYS(USB3_BASE) + 0x10000,
		.flags		= IORESOURCE_MEM
	},
	{
		.start = SSUSB_XHCI_INT_B_ID,
		.flags = IORESOURCE_IRQ,
	},
};

static void xhci_hcd_release (struct device *dev)
{
    printk(KERN_INFO "dev = 0x%08X.\n", (uint32_t)dev);
}

static struct platform_device mtk_xhci_dev = {
    .name = "xhci-hcd",
    .id   = -1,
    .num_resources = ARRAY_SIZE(resource_xhci),
    .resource      = resource_xhci,
    .dev = {
    		.coherent_dma_mask = XHCI_DMA_BIT_MASK,
    		.dma_mask = &xhci_dma_mask,
    		.release = xhci_hcd_release,
    },
};
#endif

#ifdef CONFIG_MTK_MULTIBRIDGE_SUPPORT
static struct platform_device mtk_multibridge_dev = {
    .name = "multibridge",
    .id   = 0,
};
#endif
#if 0 ///def CONFIG_MTK_HDMI_SUPPORT
static struct platform_device mtk_hdmi_dev = {
    .name = "hdmitx",
    .id   = 0,
};
#endif


#ifdef CONFIG_MTK_MT8193_SUPPORT
static struct platform_device mtk_ckgen_dev = {
    .name = "mt8193-ckgen",
    .id   = 0,
};
#endif

#if defined (CONFIG_MTK_SPI)
static struct resource mt_spi_resources[] =
{
	[0]={
		.start = IO_VIRT_TO_PHYS(SPI1_BASE),
		.end = IO_VIRT_TO_PHYS(SPI1_BASE) + 0x0024,
		.flags = IORESOURCE_MEM,
	},
	[1]={
		.start = SPI0_IRQ_BIT_ID,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device mt_spi_device = {
	.name = "mt-spi",
	.num_resources = ARRAY_SIZE(mt_spi_resources),
	.resource=mt_spi_resources
};

#endif

#if defined(CONFIG_USB_MTK_ACM_TEMP)
struct platform_device usbacm_temp_device = {
	.name	  ="USB_ACM_Temp_Driver",
	.id		  = -1,
};
#endif

#if defined(CONFIG_MTK_ACCDET)
#ifndef CONFIG_OF
struct platform_device accdet_device = {
	.name	  ="Accdet_Driver",
	.id		  = -1,
	//.dev    ={
	//.release = accdet_dumy_release,
	//}
};
#endif
#endif

#if defined(CONFIG_MTK_TVOUT_SUPPORT)

static struct resource mt6575_TVOUT_resource[] = {
    [0] = {//TVC
        .start = TVC_BASE,
        .end   = TVC_BASE + 0x78,
        .flags = IORESOURCE_MEM,
    },
    [1] = {//TVR
        .start = TV_ROT_BASE,
        .end   = TV_ROT_BASE + 0x378,
        .flags = IORESOURCE_MEM,
    },
    [2] = {//TVE
        .start = TVE_BASE,
        .end   = TVE_BASE + 0x84,
        .flags = IORESOURCE_MEM,
    },
};

static u64 mt6575_TVOUT_dmamask = ~(u32)0;

static struct platform_device mt6575_TVOUT_dev = {
	.name		  = "TV-out",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(mt6575_TVOUT_resource),
	.resource	  = mt6575_TVOUT_resource,
	.dev              = {
		.dma_mask = &mt6575_TVOUT_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};
#endif
static struct platform_device mt_device_msdc[] =
{
#if defined(CFG_DEV_MSDC0)
    {
        .name           = "mtk-msdc",
        .id             = 0,
        .num_resources  = ARRAY_SIZE(mt_resource_msdc0),
        .resource       = mt_resource_msdc0,
        .dev = {
            .platform_data = &msdc0_hw,
        },
    },
#endif
#if defined(CFG_DEV_MSDC1)
    {
        .name           = "mtk-msdc",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(mt_resource_msdc1),
        .resource       = mt_resource_msdc1,
        .dev = {
            .platform_data = &msdc1_hw,
        },
    },
#endif
#if defined(CFG_DEV_MSDC2)
    {
        .name           = "mtk-msdc",
        .id             = 2,
        .num_resources  = ARRAY_SIZE(mt_resource_msdc2),
        .resource       = mt_resource_msdc2,
        .dev = {
            .platform_data = &msdc2_hw,
        },
    },
#endif
#if defined(CFG_DEV_MSDC3)
    {
        .name           = "mtk-msdc",
        .id             = 3,
        .num_resources  = ARRAY_SIZE(mt_resource_msdc3),
        .resource       = mt_resource_msdc3,
        .dev = {
            .platform_data = &msdc3_hw,
        },
    },
#endif
#if defined(CFG_DEV_MSDC4)
    {
        .name           = "mtk-msdc",
        .id             = 4,
        .num_resources  = ARRAY_SIZE(mt_resource_msdc4),
        .resource       = mt_resource_msdc4,
        .dev = {
            .platform_data = &msdc4_hw,
        },
    },
#endif

};

/*=======================================================================*/
/* Keypad                                                         */
/*=======================================================================*/
#ifndef CONFIG_OF
#ifdef CONFIG_MTK_KEYPAD
static struct platform_device kpd_pdev = {
	.name	= "mtk-kpd",
	.id	= -1,
};
#endif
#endif

#ifdef CONFIG_RFKILL
/*=======================================================================*/
/*   RFKill module (BT and WLAN)                                             */
/*=======================================================================*/
/* MT66xx RFKill BT */
struct platform_device mt_rfkill_device = {
    .name   = "mt-rfkill",
    .id     = -1,
};
#endif

/*=======================================================================*/
/* HID Keyboard  add by zhangsg                                                 */
/*=======================================================================*/

#if defined(CONFIG_KEYBOARD_HID)
static struct platform_device mt_hid_dev = {
    .name = "hid-keyboard",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* UIBC input device, add by Seraph                                      */
/*=======================================================================*/

#if defined(CONFIG_MTK_WFD_SUPPORT)
static struct platform_device mt_uibc_dev = {
    .name = "uibc",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* MT6575 Touch Panel                                                    */
/*=======================================================================*/
static struct platform_device mtk_tpd_dev = {
    .name = "mtk-tpd",
    .id   = -1,
};

/*=======================================================================*/
/* MT6575 ofn                                                           */
/*=======================================================================*/
#if defined(CUSTOM_KERNEL_OFN)
static struct platform_device ofn_driver =
{
    .name = "mtofn",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* CPUFreq                                                               */
/*=======================================================================*/
#ifdef CONFIG_CPU_FREQ
static struct platform_device cpufreq_pdev = {
    .name = "mt-cpufreq",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* GPUFreq                                                               */
/*=======================================================================*/
#if 1
static struct platform_device gpufreq_pdev = {
    .name = "mt-gpufreq",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* MT6575 Thermal Controller module                                      */
/*=======================================================================*/
struct platform_device thermal_pdev = {
    .name = "mtk-thermal",
    .id   = -1,
};

#if 1
struct platform_device mtk_therm_mon_pdev = {
    .name = "mtk-therm-mon",
    .id   = -1,
};
#endif

/*=======================================================================*/
/* PTPOD                                                                 */
/*=======================================================================*/
struct platform_device ptp_pdev = {
    .name = "mt-ptp",
    .id   = -1,
};

/*=======================================================================*/
/*   SPM-MCDI module                                      */
/*=======================================================================*/
struct platform_device spm_mcdi_pdev = {
    .name = "mtk-spm-mcdi",
    .id   = -1,
};

/*=======================================================================*/
/*   USIF-DUMCHAR                                                          */
/*=======================================================================*/

static struct platform_device dummychar_device =
{
       .name           = "dummy_char",
        .id             = 0,
};

/*=======================================================================*/
/* MASP                                                                  */
/*=======================================================================*/
static struct platform_device masp_device =
{
       .name           = "masp",
       .id             = -1,
};


/*=======================================================================*/
/*   NAND                                                           */
/*=======================================================================*/
#if defined(CONFIG_MTK_MTD_NAND)
#define NFI_base    NFI_BASE//0x80032000
#define NFIECC_base NFIECC_BASE//0x80038000
static struct resource mtk_resource_nand[] = {
	{
		.start		= IO_VIRT_TO_PHYS(NFI_base),
		.end		= IO_VIRT_TO_PHYS(NFI_base) + 0x1A0,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IO_VIRT_TO_PHYS(NFIECC_base),
		.end		= IO_VIRT_TO_PHYS(NFIECC_base) + 0x150,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= MT_NFI_IRQ_ID,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= MT_NFIECC_IRQ_ID,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device mtk_nand_dev = {
    .name = "mtk-nand",
    .id   = 0,
   	.num_resources	= ARRAY_SIZE(mtk_resource_nand),
   	.resource		= mtk_resource_nand,
    .dev            = {
        .platform_data = &mtk_nand_hw,
    },
};
#endif

/*=======================================================================*/
/* Audio                                                 */
/*=======================================================================*/

#if defined(CONFIG_MTK_SOUND)

static u64        AudDrv_dmamask      = 0xffffffffUL;
static struct platform_device AudDrv_device = {
	.name  = "AudDrv_driver_device",
	.id    = 0,
	.dev   = {
		        .dma_mask = &AudDrv_dmamask,
		        .coherent_dma_mask =  0xffffffffUL
	         }
};

static u64        AudDrv_btcvsd_dmamask      = 0xffffffffUL;
static struct platform_device AudDrv_device2 = {
    .name  = "AudioMTKBTCVSD",
    .id    = 0,
    .dev   = {
        .dma_mask = &AudDrv_btcvsd_dmamask,
        .coherent_dma_mask =  0xffffffffUL
    }
};

#endif

/*=======================================================================*/
/* MTK I2C                                                            */
/*=======================================================================*/
//static struct i2c_board_info __initdata i2c_devs0[]={
//	{ I2C_BOARD_INFO("ADXL345", 0x53),},
//	{ I2C_BOARD_INFO("A320", 0x57),},
//	{ I2C_BOARD_INFO("ami304", (0x1E>>1)),},//0x1E>>1
//	{ I2C_BOARD_INFO("CM3623", (0x92>>1)),},//0x92>>1
//	{ I2C_BOARD_INFO("MPU3000", (0xD0>>1)),},//0xD0>>1
//	{ I2C_BOARD_INFO("mtk-tpd", (0xBA>>1)),},
//};
//static struct i2c_board_info __initdata i2c_devs1[]={
//	{ I2C_BOARD_INFO("kd_camera_hw", 0xfe), },
//	{ I2C_BOARD_INFO("FM50AF", 0x18), },
//	{ I2C_BOARD_INFO("dummy_eeprom", 0xA0),},
//	{ I2C_BOARD_INFO("EEPROM_S24CS64A", 0xAA),},
//	{},
//};
//static struct i2c_board_info __initdata i2c_devs2[]={
//#ifndef CONFIG_ _FPGA
//	{ I2C_BOARD_INFO("mt6329_pmic", 0x60), },
//#endif	//End of CONFIG_ _FPGA
//	{ I2C_BOARD_INFO("mt6329_pmic_bank1", 0x61), },
//};
static struct resource mt_resource_i2c0[] = {
    {
        .start  = IO_VIRT_TO_PHYS(I2C0_BASE),
        .end    = IO_VIRT_TO_PHYS(I2C0_BASE) + 0x70,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = I2C0_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};

static struct resource mt_resource_i2c1[] = {
    {
        .start  = IO_VIRT_TO_PHYS(I2C1_BASE),
        .end    = IO_VIRT_TO_PHYS(I2C1_BASE) + 0x70,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = I2C1_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};

static struct resource mt_resource_i2c2[] = {
    {
        .start  = IO_VIRT_TO_PHYS(I2C2_BASE),
        .end    = IO_VIRT_TO_PHYS(I2C2_BASE) + 0x70,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = I2C2_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};

static struct resource mt_resource_i2c3[] = {
    {
        .start  = IO_VIRT_TO_PHYS(I2C3_BASE),
        .end    = IO_VIRT_TO_PHYS(I2C3_BASE) + 0x70,
        .flags  = IORESOURCE_MEM,
    },
    {
        .start  = I2C3_IRQ_BIT_ID,
        .flags  = IORESOURCE_IRQ,
    },
};

static struct platform_device mt_device_i2c[] = {
    {
        .name           = "mt-i2c",
        .id             = 0,
        .num_resources  = ARRAY_SIZE(mt_resource_i2c0),
        .resource       = mt_resource_i2c0,
    },
    {
        .name           = "mt-i2c",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(mt_resource_i2c1),
        .resource       = mt_resource_i2c1,
    },
    {
        .name           = "mt-i2c",
        .id             = 2,
        .num_resources  = ARRAY_SIZE(mt_resource_i2c2),
        .resource       = mt_resource_i2c2,
    },
    {
        .name           = "mt-i2c",
        .id             = 3,
        .num_resources  = ARRAY_SIZE(mt_resource_i2c3),
        .resource       = mt_resource_i2c3,
    },
};



static u64 mtk_smi_dmamask = ~(u32)0;

static struct platform_device mtk_smi_dev = {
	.name		  = "MTK_SMI",
	.id		  = 0,
	.dev              = {
		.dma_mask = &mtk_smi_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};


static u64 mtk_cmdq_dmamask = ~(u32)0;

static struct platform_device mtk_cmdq_dev = {
	.name		  = "mtk_cmdq",
	.id		  = -1,
	.dev              = {
		.dma_mask = &mtk_cmdq_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(64)
	}
};

static u64 mtk_mjc_dmamask = ~(u32)0;

static struct platform_device mtk_mjc_dev = {
	.name		  = "MJC",
	.id		  = 0,
	.dev              = {
		.dma_mask = &mtk_mjc_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};


/*=======================================================================*/
/* MT6573 GPS module                                                    */
/*=======================================================================*/
/* MT3326 GPS */
#ifdef CONFIG_MTK_GPS
struct platform_device mt3326_device_gps = {
	.name	       = "mt3326-gps",
	.id            = -1,
	.dev = {
        .platform_data = &mt3326_gps_hw,
    },
};
#endif

/*=======================================================================*/
/* MT6573 PMEM                                                           */
/*=======================================================================*/
#if defined(CONFIG_ANDROID_PMEM)
static struct android_pmem_platform_data  pdata_multimedia = {
        .name = "pmem_multimedia",
        .no_allocator = 0,
        .cached = 1,
        .buffered = 1
};

static struct platform_device pmem_multimedia_device = {
        .name = "android_pmem",
        .id = 1,
        .dev = { .platform_data = &pdata_multimedia }
};
#endif

#if defined(CONFIG_ANDROID_VMEM)
static struct android_vmem_platform_data  pdata_vmultimedia = {
        .name = "vmem_multimedia",
        .no_allocator = 0,
        .cached = 1,
        .buffered = 1
};

static struct platform_device vmem_multimedia_device = {
        .name = "android_vmem",
        .id = -1,
        .dev = { .platform_data = &pdata_vmultimedia }
};
#endif

/*=======================================================================*/
/* MT6575 SYSRAM                                                         */
/*=======================================================================*/
static struct platform_device camera_sysram_dev = {
	.name	= "camera-sysram", /* FIXME. Sync to driver, init.rc, MHAL */
	.id     = 0,
};

/*=======================================================================*/
/*=======================================================================*/
/* Commandline filter                                                    */
/* This function is used to filter undesired command passed from LK      */
/*=======================================================================*/
static void cmdline_filter(struct tag *cmdline_tag, char *default_cmdline)
{
	const char *undesired_cmds[] = {
	                             "console=",
                                     "root=",
                                     "lk_t=",
                                     "pl_t=",
			             };

	int i;
	int ck_f = 0;
	char *cs,*ce;
	char ch;

	cs = cmdline_tag->u.cmdline.cmdline;
	ce = cs;
	while((__u32)ce < (__u32)tag_next(cmdline_tag)) {

	    while(*cs == ' ' || *cs == '\0') {
	    	cs++;
	    	ce = cs;
	    }

	    if (*ce == ' ' || *ce == '\0') {
	    	for (i = 0; i < sizeof(undesired_cmds)/sizeof(char *); i++){
	    	    if (memcmp(cs, undesired_cmds[i], strlen(undesired_cmds[i])) == 0) {
			ck_f = 1;
                        break;
                    }
	    	}

                if(ck_f == 0){
                ch = *ce;
		    *ce = '\0';
                    //Append to the default command line
                    strcat(default_cmdline, " ");
                    strcat(default_cmdline, cs);
                *ce = ch;
		}
		ck_f = 0;
	    	cs = ce + 1;
	    }
	    ce++;
	}
	if (strlen(default_cmdline) >= COMMAND_LINE_SIZE)
	{
		panic("Command line length is too long.\n\r");
	}
}
/*=======================================================================*/
/* Parse the framebuffer info						 */
/*=======================================================================*/
//Comment for build warning
//static int __init parse_tag_videofb_fixup(const struct tag *tags)
//{
//	bl_fb.base = tags->u.videolfb.lfb_base;
//	bl_fb.size = tags->u.videolfb.lfb_size;
//        use_bl_fb++;
//	return 0;
//}

//FIX-ME early porting
static int __init parse_tag_devinfo_data_fixup(const struct tag *tags)
{
    int i=0;
    int size = tags->u.devinfo_data.devinfo_data_size;
    for (i=0;i<size;i++){
        g_devinfo_data[i] = tags->u.devinfo_data.devinfo_data[i];
    }

    /* print chip id for debugging purpose */
    printk("tag_devinfo_data_rid, indx[%d]:0x%x\n", 12,g_devinfo_data[12]);
    printk("tag_devinfo_data size:%d\n", size);
    g_devinfo_data_size = size;
	return 0;
}
#ifdef DFO_DEBUG
/*
 * create a dummy dfo tag for developing
 */
struct dummy_dfo {
	struct tag_header hdr;
        tag_dfo_boot dfo_data[8];
};

struct dummy_dfo dummy_dfo_tag = {
    .hdr.size = ((sizeof(struct tag_header) + sizeof(tag_dfo_boot) * 8) >> 2),
    .hdr.tag = 0,
    .dfo_data[0].info = {"key1", 0x00000001},
    .dfo_data[1].info = {"key2", 0x00000002},
    .dfo_data[2].info = {"key3", 0x00000003},
    .dfo_data[3].info = {"key4", 0x00000004},
    .dfo_data[4].info = {"key5", 0x00000005},
    .dfo_data[5].info = {"key6", 0x00000006},
    .dfo_data[6].info = {"key7", 0x00000007},
    .dfo_data[7].info = {"key8", 0x00000008},
};
#endif

/*
 * all dfo data
 */
struct dfo_db dfo_db;

static void init_dfo_db(void)
{
    memset(&dfo_db, 0x0, sizeof(struct dfo_db));
}

/*
 * dfo_query
 * s: querying string
 * v: value
 * return 0 on success
 */
int dfo_query(const char *s, unsigned long *v)
{
    int i;
    for (i = 0; i < dfo_db.cnt; i++) {
        if (!strcmp(dfo_db.dfo_tag[i].info[0].name, s)) {
            *v = dfo_db.dfo_tag[i].info[0].value;
            return 0;   /* success */
        }
    }
    return 1; /* fail */
}
EXPORT_SYMBOL(dfo_query);

static void parse_dfo_tag(struct tag *t)
{
    int i;
    int nr = ((t->hdr.size << 2) - sizeof(struct tag_header)) / sizeof(tag_dfo_boot);
    tag_dfo_boot *p = 0;
    p = &(t->u.dfo_data);
    printk(KERN_ALERT"%s start\n", __FUNCTION__);
    printk(KERN_ALERT"tag_header size: %d, tag_dfo_boot size: %d\n"
            "hdr.size: %d\n",
            sizeof(struct tag_header), sizeof(tag_dfo_boot),
            t->hdr.size);
    printk(KERN_ALERT"number of tags: %d\n", nr);
    for (i = 0; i < nr; i++) {
        printk(KERN_ALERT"dfo[%d]: %s, 0x%08lx\n", i,
              p->info[0].name, p->info[0].value);
        /*
         * add dfo tags into dfo_db
         */
        if (dfo_db.cnt <= MAX_DFO_ENTRIES) {
            strncpy(dfo_db.dfo_tag[dfo_db.cnt].info[0].name, p->info[0].name, 32);
            dfo_db.dfo_tag[dfo_db.cnt].info[0].name[31] = '\0'; /* force null terminated */
            dfo_db.dfo_tag[dfo_db.cnt].info[0].value = p->info[0].value;
            dfo_db.cnt++; /* increase cnt */
        } else {
            printk(KERN_ERR"ERR: number of dfo(%d) excceed MAX_DFO_ENTRIES(%d),"
                    "skip it\n",
                    nr, MAX_DFO_ENTRIES);
        }
        p++;
    }
}

#ifdef CONFIG_OF
static int __init early_init_dt_get_chosen(unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 ||
	    (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

  return node;
}
#endif

void mt_fixup(struct tag *tags, char **cmdline, struct meminfo *mi)
{
    struct tag *cmdline_tag = NULL;
    struct tag *reserved_mem_bank_tag = NULL;
    struct tag *none_tag = NULL;

    __u64 max_limit_size = CONFIG_MAX_DRAM_SIZE_SUPPORT -
                             RESERVED_MEM_MODEM;
    __u64 avail_dram = 0;
    __u64 bl_mem_sz = 0;
    unsigned char md_inf_from_meta[4] = {0};

#if defined(CONFIG_MTK_FB)
	struct tag *temp_tags = tags;
	for (; temp_tags->hdr.size; temp_tags = tag_next(temp_tags))
	{
		if(temp_tags->hdr.tag == ATAG_CMDLINE)
			cmdline_filter(temp_tags, (char*)&temp_command_line);
	}
#endif

    printk(KERN_ALERT"Load default dfo data...\n");

    init_dfo_db(); /* init dfo_db */

    for (; tags->hdr.size; tags = tag_next(tags)) {
        if (tags->hdr.tag == ATAG_MEM64) {
	    bl_mem_sz += tags->u.mem64.size;

	    /*
             * Modify the memory tag to limit available memory to
             * CONFIG_MAX_DRAM_SIZE_SUPPORT
             */
            if (max_limit_size > 0) {
                if (max_limit_size >= tags->u.mem64.size) {
                    max_limit_size -= tags->u.mem64.size;
                    avail_dram += tags->u.mem64.size;
                } else {
                    tags->u.mem64.size = max_limit_size;
                    avail_dram += max_limit_size;
                    max_limit_size = 0;
                }
		// By Keene:
		// remove this check to avoid calcuate pmem size before we know all dram size
		// Assuming the minimum size of memory bank is 256MB
                //if (tags->u.mem64.size >= (TOTAL_RESERVED_MEM_SIZE)) {
                    reserved_mem_bank_tag = tags;
                //}
            } else {
                tags->u.mem64.size = 0;
            }
	}
        else if (tags->hdr.tag == ATAG_CMDLINE) {
            cmdline_tag = tags;
        } else if (tags->hdr.tag == ATAG_BOOT) {
            g_boot_mode = tags->u.boot.bootmode;
        } else if (tags->hdr.tag == ATAG_VIDEOLFB) {
        //FIX-ME early porting
        //    parse_tag_videofb_fixup(tags);
        }else if (tags->hdr.tag == ATAG_DEVINFO_DATA){
            parse_tag_devinfo_data_fixup(tags);
        }
        else if(tags->hdr.tag == ATAG_META_COM)
        {
          g_meta_com_type = tags->u.meta_com.meta_com_type;
          g_meta_com_id = tags->u.meta_com.meta_com_id;
        } else if (tags->hdr.tag == ATAG_DFO_DATA) {
            parse_dfo_tag(tags);
        }
		else if(tags->hdr.tag == ATAG_MDINFO_DATA) {
            printk(KERN_ALERT "Get MD inf from META\n");
            printk(KERN_ALERT "md_inf[0]=%d\n",tags->u.mdinfo_data.md_type[0]);
            printk(KERN_ALERT "md_inf[1]=%d\n",tags->u.mdinfo_data.md_type[1]);
            printk(KERN_ALERT "md_inf[2]=%d\n",tags->u.mdinfo_data.md_type[2]);
            printk(KERN_ALERT "md_inf[3]=%d\n",tags->u.mdinfo_data.md_type[3]);
            md_inf_from_meta[0]=tags->u.mdinfo_data.md_type[0];
            md_inf_from_meta[1]=tags->u.mdinfo_data.md_type[1];
            md_inf_from_meta[2]=tags->u.mdinfo_data.md_type[2];
            md_inf_from_meta[3]=tags->u.mdinfo_data.md_type[3];
        }
    }

    if ((g_boot_mode == META_BOOT) || (g_boot_mode == ADVMETA_BOOT)) {
        /*
         * Always use default dfo setting in META mode.
         * We can fix abnormal dfo setting this way.
         */
        printk(KERN_ALERT"(META mode) Load default dfo data...\n");
#ifdef CONFIG_MTK_ECCCI_DRIVER
        ccci_parse_meta_md_setting(md_inf_from_meta);
#endif
    }

    kernel_mem_sz = avail_dram; // keep the DRAM size (limited by CONFIG_MAX_DRAM_SIZE_SUPPORT)
    /*
    * If the maximum memory size configured in kernel
    * is smaller than the actual size (passed from BL)
    * Still limit the maximum memory size but use the FB
    * initialized by BL
    */
    if (bl_mem_sz >= (CONFIG_MAX_DRAM_SIZE_SUPPORT - RESERVED_MEM_MODEM)) {
	use_bl_fb++;
    }

#ifdef DFO_DEBUG
    parse_dfo_tag((struct tag *)(&dummy_dfo_tag));
    /*
     * dfo sanity test
     */
    unsigned long v = 0x5a;
    /* normal case test */
    if (!dfo_query("key1", &v)) {
        printk(KERN_ALERT"dfo_query normal case PASS (%s, 0x%08lx)\n",
                "key1", v);
    } else {
        printk(KERN_ERR"dfo_query normal case FAIL\n");
    }
    if (!dfo_query("key3", &v)) {
        printk(KERN_ALERT"dfo_query normal case PASS (%s, 0x%08lx)\n",
                "key3", v);
    } else {
        printk(KERN_ERR"dfo_query normal case FAIL\n");
    }
    if (!dfo_query("key5", &v)) {
        printk(KERN_ALERT"dfo_query normal case PASS (%s, 0x%08lx)\n",
                "key5", v);
    } else {
        printk(KERN_ERR"dfo_query normal case FAIL\n");
    }
    /* fail case test */
    if (!dfo_query("none-exist-key", &v)) {
        printk(KERN_ERR"dfo_query fail case FAIL\n");
    } else {
        printk(KERN_ALERT"dfo_query fail case PASS\n");
    }
#endif

    /*
     * Setup PMEM size
     */
    /*
    if (avail_dram < 0x10000000)
        RESERVED_MEM_SIZE_FOR_PMEM = 0x1700000;
    else */
        RESERVED_MEM_SIZE_FOR_PMEM = 0x0;

    /* Reserve memory in the last bank */
    if (reserved_mem_bank_tag) {
        reserved_mem_bank_tag->u.mem64.size -= ((__u32)TOTAL_RESERVED_MEM_SIZE);
	if(g_boot_mode == FACTORY_BOOT) {
            /* we need to reserved the maximum FB_SIZE to get a fixed TEST_3D pa. */
            __u64 rest_fb_size = RESERVED_MEM_SIZE_FOR_FB_MAX - FB_SIZE;
            RESERVED_MEM_SIZE_FOR_TEST_3D = 0x9a00000 + rest_fb_size;
            reserved_mem_bank_tag->u.mem64.size -= RESERVED_MEM_SIZE_FOR_TEST_3D;
        }
        FB_SIZE_EXTERN = FB_SIZE;
        pmem_start = reserved_mem_bank_tag->u.mem64.start + reserved_mem_bank_tag->u.mem64.size;
    } else // we should always have reserved memory
    	BUG();

    MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
            "[PHY layout]avaiable DRAM size (lk) = 0x%llx\n"
            "[PHY layout]avaiable DRAM size = 0x%llx\n"
            "[PHY layout]FB       :   0x%llx - 0x%llx  (0x%llx)\n",
            (unsigned long long)bl_mem_sz,
            (unsigned long long)kernel_mem_sz,
            (unsigned long long)FB_START,
            (unsigned long long)(FB_START + FB_SIZE - 1),
            (unsigned long long)FB_SIZE);
    if(g_boot_mode == FACTORY_BOOT)
        MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
                "[PHY layout]3D       :   0x%llx - 0x%llx  (0x%llx)\n",
                (unsigned long long)TEST_3D_START,
                (unsigned long long)(TEST_3D_START + TEST_3D_SIZE - 1),
                (unsigned long long)TEST_3D_SIZE);
    if (PMEM_MM_SIZE) {
        MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
                "[PHY layout]PMEM     :   0x%llx - 0x%llx  (0x%llx)\n",
                (unsigned long long)PMEM_MM_START,
                (unsigned long long)(PMEM_MM_START + PMEM_MM_SIZE - 1),
                (unsigned long long)PMEM_MM_SIZE);
    }

    if(tags->hdr.tag == ATAG_NONE)
	none_tag = tags;
    if (cmdline_tag != NULL) {
#ifdef CONFIG_FIQ_DEBUGGER
        char *console_ptr;
        int uart_port;
#endif
	char *br_ptr;
        // This function may modify ttyMT3 to ttyMT0 if needed
        adjust_kernel_cmd_line_setting_for_console(cmdline_tag->u.cmdline.cmdline, *cmdline);
#ifdef CONFIG_FIQ_DEBUGGER
        if ((console_ptr=strstr(*cmdline, "ttyMT")) != 0)
        {
            uart_port = console_ptr[5] - '0';
            if (uart_port > 3)
                uart_port = -1;

            fiq_uart_fixup(uart_port);
        }
#endif

        cmdline_filter(cmdline_tag, *cmdline);
		if ((br_ptr = strstr(*cmdline, "boot_reason=")) != 0) {
			/* get boot reason */
			g_boot_reason = br_ptr[12] - '0';
		}
        /* Use the default cmdline */
        memcpy((void*)cmdline_tag,
               (void*)tag_next(cmdline_tag),
               /* ATAG_NONE actual size */
               (uint32_t)(none_tag) - (uint32_t)(tag_next(cmdline_tag)) + 8);
    }
}

#ifdef CONFIG_OF
void __init mt_dt_init_early(void)
{
    unsigned long node;
    struct tag *tags;
#ifdef CONFIG_FIQ_DEBUGGER
    char cmdline[COMMAND_LINE_SIZE];
    char *console_ptr;
    int uart_port;
#endif

    node = of_scan_flat_dt(early_init_dt_get_chosen, NULL);

    printk(KERN_ALERT"Load default dfo data...\n");

    init_dfo_db(); /* init dfo_db */

    tags = of_get_flat_dt_prop(node, "atag,dfo", NULL);
    if (tags)
      parse_dfo_tag(tags);

#ifdef CONFIG_FIQ_DEBUGGER
    strlcpy(cmdline, boot_command_line, COMMAND_LINE_SIZE);

    adjust_kernel_cmd_line_setting_for_console(cmdline, cmdline);

    if ((console_ptr=strstr(cmdline, "ttyMT")) != 0)
    {
        uart_port = console_ptr[5] - '0';
        if (uart_port > 3)
            uart_port = -1;

        fiq_uart_fixup(uart_port);
    }
#endif
}
#endif

struct platform_device auxadc_device = {
    .name   = "mt-auxadc",
    .id     = -1,
};

/*=======================================================================*/
/* MT6575 sensor module                                                  */
/*=======================================================================*/
struct platform_device sensor_gsensor = {
	.name	       = "gsensor",
	.id            = -1,
};

struct platform_device sensor_msensor = {
	.name	       = "msensor",
	.id            = -1,
};

struct platform_device sensor_orientation = {
	.name	       = "orientation",
	.id            = -1,
};

struct platform_device sensor_alsps = {
	.name	       = "als_ps",
	.id            = -1,
};

struct platform_device sensor_gyroscope = {
	.name	       = "gyroscope",
	.id            = -1,
};

struct platform_device sensor_barometer = {
	.name	       = "barometer",
	.id            = -1,
};
struct platform_device sensor_temperature = {
	.name	       = "temperature",
	.id            = -1,
};

struct platform_device sensor_batch = {
	.name	       = "batchsensor",
	.id            = -1,
};
/* hwmon sensor */
struct platform_device hwmon_sensor = {
	.name	       = "hwmsensor",
	.id            = -1,
};

struct platform_device acc_sensor = {
	.name	       = "m_acc_pl",
	.id            = -1,
};
struct platform_device mag_sensor = {
	.name	       = "m_mag_pl",
	.id            = -1,
};

struct platform_device alsps_sensor = {
	.name	       = "m_alsps_pl",
	.id            = -1,
};

struct platform_device gyro_sensor = {
	.name	       = "m_gyro_pl",
	.id            = -1,
};

struct platform_device barometer_sensor = {
	.name	       = "m_baro_pl",
	.id            = -1,
};

struct platform_device temp_sensor = {
	.name	       = "m_temp_pl",
	.id            = -1,
};
struct platform_device step_c_sensor = {
	.name	       = "m_step_c_pl",
	.id            = -1,
};

struct platform_device batch_sensor = {
	.name	       = "m_batch_pl",
	.id            = -1,
};

/*=======================================================================*/
/* DISP DEV                                                              */
/*=======================================================================*/
static u64 disp_dmamask = ~(u32)0;

static struct platform_device disp_device = {
	.name	 = "mtk_disp",
	.id      = 0,
	.dev     = {
		.dma_mask = &disp_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = 0,
};


/*=======================================================================*/
/* Camera ISP                                                            */
/*=======================================================================*/
static struct resource mt_resource_isp[] = {
//FIX-ME early porting

    { // ISP configuration
        .start = IO_VIRT_TO_PHYS(IMGSYS_BASE),
        .end   = IO_VIRT_TO_PHYS(IMGSYS_BASE) + 0xE000,
        .flags = IORESOURCE_MEM,
    },
    { // ISP IRQ
        .start = CAM0_IRQ_BIT_ID,
        .flags = IORESOURCE_IRQ,
    }

};
static u64 mt_isp_dmamask = ~(u32) 0;
//
static struct platform_device mt_isp_dev = {
	.name		   = "camera-isp",
	.id		       = 0,
	.num_resources = ARRAY_SIZE(mt_resource_isp),
	.resource	   = mt_resource_isp,
	.dev           = {
		.dma_mask  = &mt_isp_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

#if 0
/*=======================================================================*/
/* MT6575 EIS                                                            */
/*=======================================================================*/
static struct resource mt_resource_eis[] = {
    [0] = { // EIS configuration
        .start = EIS_BASE,
        .end   = EIS_BASE + 0x2C,
        .flags = IORESOURCE_MEM,
    }
};
static u64 mt_eis_dmamask = ~(u32) 0;
//
static struct platform_device mt_eis_dev = {
	.name		   = "camera-eis",
	.id		       = 0,
	.num_resources = ARRAY_SIZE(mt_resource_eis),
	.resource	   = mt_resource_eis,
	.dev           = {
		.dma_mask  = &mt_eis_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

#endif
//
/*=======================================================================*/
/* Image sensor                                                        */
/*=======================================================================*/
static struct platform_device sensor_dev = {
	.name		  = "image_sensor",
	.id		  = -1,
};
static struct platform_device sensor_dev_bus2 = {
	.name		  = "image_sensor_bus2",
	.id		  = -1,
};

//
/*=======================================================================*/
/* Lens actuator                                                        */
/*=======================================================================*/
static struct platform_device actuator_dev = {
	.name		  = "lens_actuator",
	.id		  = -1,
};
static struct platform_device actuator_dev2 = {
	.name		  = "lens_actuator2",
	.id		  = -1,
};
/*=======================================================================*/
/* MT6575 jogball                                                        */
/*=======================================================================*/
#ifdef CONFIG_MOUSE_PANASONIC_EVQWJN
static struct platform_device jbd_pdev = {
	.name = "mt6575-jb",
	.id = -1,
};
#endif

#ifndef CONFIG_OF
static struct platform_device mt65xx_leds_device = {
	.name = "leds-mt65xx",
	.id = -1
};
#endif
/*=======================================================================*/
/* NFC                                                                          */
/*=======================================================================*/
static struct platform_device mtk_nfc_6605_dev = {
    .name   = "mt6605",
    .id     = -1,
};

//#ifdef CONFIG_MTK_WIFI
/*=======================================================================*/
/* MT6572/82 WIFI module                                                 */
/*=======================================================================*/
struct platform_device mt_device_wifi = {
	.name	       = "mt-wifi",
	.id            = -1,
};
//#endif

/*=======================================================================*/
/* Unused Memory Allocation                                              */
/*=======================================================================*/
#ifdef MTK_USE_RESERVED_EXT_MEM
static struct platform_device mt_extmem = {
	.name           = "mt-extmem",
	.id             = 0,
};
#endif
/*=======================================================================*/
/*   Board Device Initialization                                    */
/* Sim switch driver                                                         */
/*=======================================================================*/
#if defined (CONFIG_CUSTOM_KERNEL_SSW)
static struct platform_device ssw_device = {
	.name = "sim-switch",
	.id = -1};
#endif

#if defined (CONFIG_MTK_ECCCI_DRIVER)
static struct ccci_dev_cfg cldma_md_config = {
	.index = 0,
	.major = 184,
	.minor_base = 0,
	.capability = MODEM_CAP_NAPI,
};
static struct platform_device ccci_cldma_device = {
	.name = "cldma_modem",
	.id = -1,
	.dev = {
		.platform_data = &cldma_md_config,
	},
};
#endif

#if 0
/*=======================================================================*/
/* battery driver                                                         */
/*=======================================================================*/
struct platform_device battery_device = {
    .name   = "battery",
    .id        = -1,
};
#endif

/*=======================================================================*/
/* CPU Hoptlug mechanism                                                 */
/*=======================================================================*/
struct platform_device hotplug_mechanism_pdev = {
	.name	       = "hotplug_mechanism",
	.id            = -1,
};

/*=======================================================================*/
/* CPU Hoptlug strategy                                                  */
/*=======================================================================*/
//struct platform_device hotplug_strategy_pdev = {
//	.name	       = "hps",
//	.id            = -1,
//};

/*=======================================================================*/
/*   Board Device Initialization                                    */
/*=======================================================================*/
__init int mt_board_init(void)
{
    int i = 0, retval = 0;

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_SERIAL)
    for (i = 0; i < ARRAY_SIZE(mtk_device_uart); i++){
        retval = platform_device_register(&mtk_device_uart[i]);
        if (retval != 0){
            return retval;
        }
    }
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_MTK_FORCE_CLUSTER1
#ifdef CONFIG_FIQ_DEBUGGER
        retval = platform_device_register(&mt_fiq_debugger);
        printk("[%s]: mt_fiq_debugger finished probe, retval=%d\n", __func__, retval);
        if (retval != 0){
                return retval;
        }
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

	{
		uint64_t key;
#if defined(CONFIG_MTK_USB_UNIQUE_SERIAL) || defined(MTK_SEC_FASTBOOT_UNLOCK_SUPPORT)
		key = get_devinfo_with_index(13);
		key = (key << 32) | get_devinfo_with_index(12);
#else
		key = 0;
#endif
		if (key != 0)
			get_serial(key, get_chip_code(), serial_number);
		else
			memcpy(serial_number, "0123456789ABCDEF", 16);

		retval = kobject_init_and_add(&sn_kobj, &sn_ktype, NULL, "sys_info");
    printk("[%s]: sn_kobj finished probe, retval=%d\n", __func__, retval);

		if (retval < 0)
			printk("[%s] fail to add kobject\n", "sys_info");
	}

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_MTD_NAND)
    retval = platform_device_register(&mtk_nand_dev);
    printk("[%s]: mtk_nand_dev finished probe, retval=%d\n", __func__, retval);
    if (retval != 0) {
        printk(KERN_ERR "register nand device fail\n");
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
	retval = platform_device_register(&gpio_dev);
  printk("[%s]: gpio_dev finished probe, retval=%d\n", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif
    printk("[mt_board_init] %d\n", __LINE__);

	retval = platform_device_register(&fh_dev);
  printk("[%s]: fh_dev finished probe, retval=%d\n", __func__, retval);
	if (retval != 0){
		return retval;
	}

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#ifdef CONFIG_MTK_KEYPAD
	retval = platform_device_register(&kpd_pdev);
  printk("[%s]: kpd_pdev finished probe, retval=%d\n", __func__, retval);
	if (retval != 0) {
		return retval;
	}
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_MOUSE_PANASONIC_EVQWJN
	retval = platform_device_register(&jbd_pdev);
  printk("[%s]: jbd_pdev finished probe, retval=%d\n", __func__, retval);
	if (retval != 0) {
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_KEYBOARD_HID)
	retval = platform_device_register(&mt_hid_dev);
  printk("[%s]: mt_hid_dev finished probe, retval=%d\n", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_WFD_SUPPORT)
	retval = platform_device_register(&mt_uibc_dev);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_I2C)
	//i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	//i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	//i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
#ifdef I2C_E2_ECO

	    //E2 eco
	    if (mt_get_chip_sw_ver() == CHIP_SW_VER_02)
  		{
     		mt_reg_sync_writel(1<<19, 0xF0003328);
			printk("i2c E2 dev read i2c swap=%x \n",__raw_readl((void*)(0xF0003328)));
			mt_resource_i2c1[0].start = IO_VIRT_TO_PHYS(I2C2_BASE);
			mt_resource_i2c1[0].end = IO_VIRT_TO_PHYS(I2C2_BASE) + 0x70;
			mt_resource_i2c1[1].start  = I2C2_IRQ_BIT_ID;

			mt_resource_i2c2[0].start = IO_VIRT_TO_PHYS(I2C1_BASE);
			mt_resource_i2c2[0].end = IO_VIRT_TO_PHYS(I2C1_BASE) + 0x70;
			mt_resource_i2c2[1].start  = I2C1_IRQ_BIT_ID;
  		}
#endif
		for (i = 0; i < ARRAY_SIZE(mt_device_i2c); i++){
			retval = platform_device_register(&mt_device_i2c[i]);
			printk("[%s]: mt_device_i2c[%d] finished probe, retval=%d\n", __func__, i, retval);
			if (retval != 0){
				return retval;
			}
		}
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_MMC)
    for (i = 0; i < ARRAY_SIZE(mt_device_msdc); i++){
        retval = platform_device_register(&mt_device_msdc[i]);
        printk("[%s]: msdc finished probe, retval=%d\n", __func__, retval);
			if (retval != 0){
				return retval;
			}
		}
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_SOUND)
    retval = platform_device_register(&AudDrv_device);
    printk("[%s]:AudDrv_driver_device, retval=%d \n!", __func__, retval);
    if (retval != 0){
       return retval;
    }

	retval = platform_device_register(&AudDrv_device2);
	printk("[%s]: AudioMTKBTCVSD AudDrv_device2, retval=%d\n!", __func__, retval);
	if (retval != 0){
		 printk("AudioMTKBTCVSD AudDrv_device2 Fail:%d \n", retval);
		return retval;
	}

#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_MTK_MULTIBRIDGE_SUPPORT
    retval = platform_device_register(&mtk_multibridge_dev);
    printk("[%s]: multibridge_driver_device, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

    retval = platform_device_register(&mtk_device_btif);
    printk("[%s]: mtk_device_btif, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }

    printk("[mt_board_init] %d\n", __LINE__);

#if (defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT))
    retval = platform_device_register(&mtk_device_ext_md_ctl);
    printk("[%s]: mtk_device_ext_md_ctl, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

//=====SMI/M4U devices===========
    printk("register MTK_SMI device\n");
    retval = platform_device_register(&mtk_smi_dev);
    printk("[%s]: mtk_smi_dev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }

#if 0
    printk("[mt_board_init] %d\n", __LINE__);
    printk("register CMDQ device: %d\n", retval);
    retval = platform_device_register(&mtk_cmdq_dev);
    printk("[%s]: mtk_cmdq_dev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);
#if 1
    retval = platform_device_register(&disp_device);
    printk("[%s]: disp_device, retval=%d \n!", __func__, retval);
    if (retval != 0)
    {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

//===========================

/*
    retval = platform_device_register(&mtk_mjc_dev);
    printk("register MJC device: %d\n", retval);
    if (retval != 0) {
        return retval;
    }
*/
    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_MTK_MT8193_SUPPORT
    printk("register 8193_CKGEN device\n");
    retval = platform_device_register(&mtk_ckgen_dev);
    printk("[%s]: mtk_ckgen_dev, retval=%d \n!", __func__, retval);
    if (retval != 0){

        printk("register 8193_CKGEN device FAILS!\n");
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_FB)
    /*
     * Bypass matching the frame buffer info. between boot loader and kernel
     * if the limited memory size of the kernel is smaller than the
     * memory size from bootloader
     */
#if 0
    if (((bl_fb.base == FB_START) && (bl_fb.size == FB_SIZE)) ||
         (use_bl_fb == 2)) {
        printk("FB is initialized by BL(%d)\n", use_bl_fb);
        mtkfb_set_lcm_inited(1);
    } else if ((bl_fb.base == 0) && (bl_fb.size == 0)) {
        printk("FB is not initialized(%d)\n", use_bl_fb);
        mtkfb_set_lcm_inited(0);
    } else {
        printk(
"******************************************************************************\n"
"   WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING\n"
"******************************************************************************\n"
"\n"
"  The default FB base & size values are not matched between BL and kernel\n"
"    - BOOTLD: start 0x%08x, size %d\n"
"    - KERNEL: start 0x%llx, size %lld\n"
"\n"
"  If you see this warning message, please update your uboot.\n"
"\n"
"******************************************************************************\n"
"   WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING\n"
"******************************************************************************\n"
"\n",   bl_fb.base, bl_fb.size, (unsigned long long)FB_START, (unsigned long long)FB_SIZE);
        /* workaround for TEST_3D_START */
        mtkfb_set_lcm_inited(1);
        {
            int delay_sec = 5;

            while (delay_sec >= 0) {
                printk("\rcontinue after %d seconds ...", delay_sec--);
                mdelay(1000);
            }
            printk("\n");
        }
#if 0
        panic("The default base & size values are not matched "
              "between BL and kernel\n");
#endif
    }
#endif

#ifdef CONFIG_OF
    resource_fb[0].start = mtkfb_get_fb_base();
    resource_fb[0].end   = mtkfb_get_fb_base() + FB_SIZE - 1;
    printk("[DT]FB start: 0x%llx end: 0x%llx\n", (unsigned long long)resource_fb[0].start,
                                             (unsigned long long)resource_fb[0].end);
#else
    resource_fb[0].start = FB_START;
    resource_fb[0].end   = FB_START + FB_SIZE - 1;

    printk("FB start: 0x%llx end: 0x%llx\n", (unsigned long long)resource_fb[0].start,
                                         (unsigned long long)resource_fb[0].end);
    retval = platform_device_register(&mt6575_device_fb);
    printk("[%s]: mt6575_device_fb, retval=%d \n!", __func__, retval);
    if (retval != 0) {
         return retval;
    }
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_LEDS)
	retval = platform_device_register(&mt65xx_leds_device);
    printk("[%s]: mt65xx_leds_device, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;
	printk("device LEDS register\n");
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0///def CONFIG_MTK_HDMI_SUPPORT
	retval = platform_device_register(&mtk_hdmi_dev);
    printk("[%s]: mtk_hdmi_dev, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_SPI)
//    spi_register_board_info(spi_board_devs, ARRAY_SIZE(spi_board_devs));
    platform_device_register(&mt_spi_device);
#endif
#endif
    printk("[mt_board_init] %d\n", __LINE__);





#if defined(CONFIG_MTK_TVOUT_SUPPORT)
    retval = platform_device_register(&mt6575_TVOUT_dev);
    printk("[%s]: mt6575_TVOUT_dev, retval=%d \n!", __func__, retval);
	printk("register TV-out device\n");
    if (retval != 0) {
         return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
  retval = platform_device_register(&auxadc_device);
    printk("[%s]: auxadc_device, retval=%d \n!", __func__, retval);
  if(retval != 0)
  {
     printk("****[auxadc_driver] Unable to device register(%d)\n", retval);
	 return retval;
  }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_ACCDET)
#ifndef CONFIG_OF
    retval = platform_device_register(&accdet_device);
    printk("[%s]: accdet_device, retval=%d \n!", __func__, retval);

	if (retval != 0)
	{
		printk("platform_device_accdet_register error:(%d)\n", retval);
		return retval;
	}
	else
	{
		printk("platform_device_accdet_register done!\n");
	}
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_USB_MTK_ACM_TEMP)

    retval = platform_device_register(&usbacm_temp_device);

    printk("[%s]: usbacm_temp_device, retval=%d \n!", __func__, retval);
	if (retval != 0)
	{
		printk("platform_device_usbacm_register error:(%d)\n", retval);
		return retval;
	}
	else
	{
		printk("platform_device_usbacm_register done!\n");
	}

#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0 //defined(CONFIG_MDP_MT6575)
    //printk("[MDP]platform_device_register\n\r");
    retval = platform_device_register(&mt6575_MDP_dev);
    printk("[%s]: mt6575_MDP_dev, retval=%d \n!", __func__, retval);
    if(retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_SENSOR_SUPPORT)

	retval = platform_device_register(&hwmon_sensor);
    printk("[%s]: hwmon_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&batch_sensor);
    printk("[%s]: batch_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&acc_sensor);
    printk("[%s]: acc_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&mag_sensor);
    printk("[%s]: mag_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&gyro_sensor);
    printk("[%s]: gyro_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&alsps_sensor);
    printk("[%s]: alsps_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&barometer_sensor);
    printk("[%s]: barometer_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&temp_sensor);
    printk("[%s]: temp_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&step_c_sensor);
    printk("[%s]: step_c_sensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

#if defined(CONFIG_CUSTOM_KERNEL_ACCELEROMETER)
	retval = platform_device_register(&sensor_gsensor);
    printk("[%s]: sensor_gsensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_CUSTOM_KERNEL_MAGNETOMETER)
	retval = platform_device_register(&sensor_msensor);
    printk("[%s]: sensor_msensor, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

	retval = platform_device_register(&sensor_orientation);
    printk("[%s]: sensor_orientation, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;

#endif

#if defined(CONFIG_CUSTOM_KERNEL_GYROSCOPE)
	retval = platform_device_register(&sensor_gyroscope);
    printk("[%s]: sensor_gyroscope, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_CUSTOM_KERNEL_BAROMETER)
	retval = platform_device_register(&sensor_barometer);
    printk("[%s]: sensor_barometer, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;
#endif

#if defined(CONFIG_CUSTOM_KERNEL_ALSPS)
	retval = platform_device_register(&sensor_alsps);
    printk("[%s]: sensor_alsps, retval=%d \n!", __func__, retval);
	if (retval != 0)
		return retval;
#endif

#if defined(CUSTOM_KERNEL_TEMPERATURE)
	retval = platform_device_register(&sensor_temperature);
    printk("[%s]: sensor_temperature, retval=%d \n!", __func__, retval);
		printk("sensor_temperature device!");
	if (retval != 0)
		return retval;
#endif
#endif
#endif
    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_USBFSH)
	retval = platform_device_register(&mt_usb11_dev);
    printk("[%s]: mt_usb11_dev, retval=%d \n!", __func__, retval);
	if (retval != 0){
		printk("register musbfsh device fail!\n");
		return retval;
	}
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0 /*defined(CONFIG_USB_MU3D_DRV)*/
	printk("mtu3d_dev register\n");
	retval = platform_device_register(&mtu3d_dev);
	if (retval != 0){
		printk("mtu3d_dev register fail\n");
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0 //defined(CONFIG_MTK_XHCI)
	printk("%s(%d): register xhci device\n", __func__, __LINE__);
	retval = platform_device_register(&mtk_xhci_dev);
	if (retval != 0){
		printk("register xhci device fail!\n");
		return retval;
	}
    #ifdef CONFIG_USB_MTK_DUALMODE
    retval = mtk_xhci_eint_iddig_init();
	if (retval != 0){
		printk("%s(%d): eint idpin init fail!\n", __func__, __LINE__);
		//return retval;
	}
    #endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_USB_MTK_HDRC)
	retval = platform_device_register(&mt_device_usb);
        printk("[%s]: mt_device_usb, retval=%d \n!", __func__, retval);
	if (retval != 0){
	printk("mt_device_usb register fail\n");
        return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0
   retval = platform_device_register(&battery_device);
   if (retval) {
	   printk("[battery_driver] Unable to device register\n");
   return retval;
   }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
#if defined(CONFIG_MTK_TOUCHPANEL)
    retval = platform_device_register(&mtk_tpd_dev);
    printk("[%s]: mtk_tpd_dev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CUSTOM_KERNEL_OFN)
    retval = platform_device_register(&ofn_driver);
    printk("[%s]: ofn_driver, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if (defined(CONFIG_MTK_MTD_NAND) ||defined(CONFIG_MTK_MMC))
retval = platform_device_register(&dummychar_device);
    printk("[%s]: dummychar_device, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_ANDROID_PMEM)
    pdata_multimedia.start = PMEM_MM_START;;
    pdata_multimedia.size = PMEM_MM_SIZE;
    printk("PMEM start: 0x%lx size: 0x%lx\n", pdata_multimedia.start, pdata_multimedia.size);

    retval = platform_device_register(&pmem_multimedia_device);
    printk("[%s]: pmem_multimedia_device, retval=%d \n!", __func__, retval);
    if (retval != 0){
       return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_ANDROID_VMEM)
    pdata_vmultimedia.start = PMEM_MM_START;;
    pdata_vmultimedia.size = PMEM_MM_SIZE;
    printk("VMEM start: 0x%lx size: 0x%lx\n", pdata_vmultimedia.start, pdata_vmultimedia.size);

    retval = platform_device_register(&vmem_multimedia_device);
    printk("[%s]: vmem_multimedia_device, retval=%d \n!", __func__, retval);
    if (retval != 0){
	printk("vmem platform register failed\n");
       return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_CPU_FREQ
    retval = platform_device_register(&cpufreq_pdev);
    printk("[%s]: cpufreq_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 1
    retval = platform_device_register(&gpufreq_pdev);
    printk("[%s]: gpufreq_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifndef CONFIG_OF
    retval = platform_device_register(&thermal_pdev);
    printk("[%s]: thermal_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 1
    retval = platform_device_register(&mtk_therm_mon_pdev);
    printk("[%s]: mtk_therm_mon_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

    retval = platform_device_register(&ptp_pdev);
    printk("[%s]: ptp_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }

    printk("[mt_board_init] %d\n", __LINE__);

    retval = platform_device_register(&spm_mcdi_pdev);
    printk("[%s]: spm_mcdi_pdev, retval=%d \n!", __func__, retval);
    if (retval != 0) {
        return retval;
    }

    printk("[mt_board_init] %d\n", __LINE__);

//
//=======================================================================
// Image sensor
//=======================================================================
#if 1 ///defined(CONFIG_VIDEO_CAPTURE_DRIVERS)
    retval = platform_device_register(&sensor_dev);
    printk("[%s]: sensor_dev, retval=%d \n!", __func__, retval);
    if (retval != 0){
    	return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 1 ///defined(CONFIG_VIDEO_CAPTURE_DRIVERS)
    retval = platform_device_register(&sensor_dev_bus2);
    printk("[%s]: sensor_dev_bus2, retval=%d \n!", __func__, retval);
    if (retval != 0){
    	return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

//
//=======================================================================
// Lens motor
//=======================================================================
#if 1  //defined(CONFIG_ACTUATOR)
    retval = platform_device_register(&actuator_dev);
    printk("[%s]: actuator_dev, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
    retval = platform_device_register(&actuator_dev2);
    printk("[%s]: actuator_dev2, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

//
//=======================================================================
// DISP DEV
//=======================================================================

    printk("[mt_board_init] %d\n", __LINE__);
//
//=======================================================================
// Camera ISP
//=======================================================================
#if 1
    retval = platform_device_register(&mt_isp_dev);
    printk("[%s]: mt_isp_dev, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 0
    retval = platform_device_register(&mt_eis_dev);
    printk("[%s]: mt_eis_dev, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_RFKILL
    retval = platform_device_register(&mt_rfkill_device);
    printk("[%s]: mt_rfkill_device, retval=%d \n!", __func__, retval);
    if (retval != 0){
        return retval;
    }
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 1
	retval = platform_device_register(&camera_sysram_dev);
    printk("[%s]: camera_sysram_dev, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined(CONFIG_MTK_GPS)
	retval = platform_device_register(&mt3326_device_gps);
    printk("[%s]: mt3326_device_gps, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if 1//defined(CONFIG_MTK_NFC) //NFC
	retval = platform_device_register(&mtk_nfc_6605_dev);
    printk("[%s]: mtk_nfc_6605_dev, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

//#if defined(CONFIG_MTK_WIFI)
	retval = platform_device_register(&mt_device_wifi);
    printk("[%s]: mt_device_wifi, retval=%d \n!", __func__, retval);
	if (retval != 0){
		return retval;
	}
//#endif

    printk("[mt_board_init] %d\n", __LINE__);

#if defined (CONFIG_CUSTOM_KERNEL_SSW)
	retval = platform_device_register(&ssw_device);
    printk("[%s]: ssw_device, retval=%d \n!", __func__, retval);
	if (retval != 0) {
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
	retval = platform_device_register(&mt_extmem);

    printk("[%s]: mt_extmem, retval=%d \n!", __func__, retval);
	printk("%s[%d] ret: %d\n", __FILE__, __LINE__, retval);
	if (retval != 0){
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

    retval = platform_device_register(&masp_device);
    if (retval != 0){
        return retval;
    }

    printk("[mt_board_init] %d\n", __LINE__);

#if defined (CONFIG_MTK_ECCCI_DRIVER)
	retval = platform_device_register(&ccci_cldma_device);
	if (retval != 0) {
		printk("[ccci/ctl] (0)cldma modem platform device register fail(%d)\n", retval);
		return retval;
	}
#endif

    printk("[mt_board_init] %d\n", __LINE__);

    retval = platform_device_register(&hotplug_mechanism_pdev);
    if (retval != 0){
        return retval;
    }

    printk("[mt_board_init] %d\n", __LINE__);


    return 0;
}

/*
 * is_pmem_range
 * Input
 *   base: buffer base physical address
 *   size: buffer len in byte
 * Return
 *   1: buffer is located in pmem address range
 *   0: buffer is out of pmem address range
 */
int is_pmem_range(unsigned long *base, unsigned long size)
{
        unsigned long start = (unsigned long)base;
        unsigned long end = start + size;

        //printk("[PMEM] start=0x%p,end=0x%p,size=%d\n", start, end, size);
        //printk("[PMEM] PMEM_MM_START=0x%p,PMEM_MM_SIZE=%d\n", PMEM_MM_START, PMEM_MM_SIZE);

        if (start < PMEM_MM_START)
                return 0;
        if (end >= PMEM_MM_START + PMEM_MM_SIZE)
                return 0;

        return 1;
}
EXPORT_SYMBOL(is_pmem_range);

// return the actual physical DRAM size
phys_addr_t mtk_get_max_DRAM_size(void)
{
        return kernel_mem_sz + RESERVED_MEM_MODEM;
}

#include <asm/sections.h>
void get_text_region (unsigned int *s, unsigned int *e)
{
    *s = (unsigned int)_text, *e=(unsigned int)_etext ;
}
EXPORT_SYMBOL(get_text_region) ;

extern void DFS_Reserved_Memory(void);

void mt_reserve(void)
{
    mrdump_reserve_memory();

#if defined(CONFIG_MTK_RAM_CONSOLE_USING_DRAM)
	memblock_reserve(CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR, CONFIG_MTK_RAM_CONSOLE_DRAM_SIZE);
#endif
        mrdump_mini_reserve_memory();
	DFS_Reserved_Memory();

#ifdef CONFIG_MTK_ECCCI_DRIVER
	ccci_md_mem_reserve();
#endif
}
