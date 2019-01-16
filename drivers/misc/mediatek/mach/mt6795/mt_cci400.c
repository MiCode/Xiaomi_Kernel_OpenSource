#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <mach/mt_reg_base.h>
#include "mach/sync_write.h"
#include <linux/earlysuspend.h> // for suspend/resume , to disable speculative fetch when resume
#include <mach/mt_boot.h>

#define CTL_OVERRIDE (CCI400_BASE+0x0)
#define SPEC_CTL     (CCI400_BASE+0x4)

#define XBARREG (volatile unsigned int*)(MCUCFG_BASE + 0x74)
struct mt_cci400_driver {

        struct device_driver driver;
        const struct platform_device_id *id_table;
};

static struct mt_cci400_driver mt_cci400_drv = {
    .driver = {
	.name = "cci400",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
    },
    .id_table= NULL,
};

int cci400_spec_fetch_query(void)
{
    unsigned int val_ctl_override;
    unsigned int val_spec_ctl;
    val_ctl_override = readl(IOMEM(CTL_OVERRIDE));
    val_spec_ctl =  readl(IOMEM(SPEC_CTL));

    if (val_ctl_override & (0x1 << 2))
    {
       if ((val_spec_ctl & (0x7 << 0)) && (val_spec_ctl & (0x1F << 16)))
       {
            return 0;  //disable spec_fetch feature
       }
    }
    return 1; //enable spec_fetch feature
}

int cci400_spec_fetch_set(int enable)
{
    unsigned int val;
        switch(enable){
            case 0:
                val = readl(IOMEM(CTL_OVERRIDE));
                val |= (0x1 << 2);
                writel(val, IOMEM(CTL_OVERRIDE));

                val = readl(IOMEM(SPEC_CTL));
                val |= (0x7 << 0);
                val |= (0x1F << 16);
                writel(val, IOMEM(SPEC_CTL));

                //printk("[CCI] Disable Speculative Fetch feature,0x%x,0x%x\n",readl(IOMEM(CTL_OVERRIDE)),readl(IOMEM(SPEC_CTL)));
                break;
            case 1:
                val = readl(IOMEM(CTL_OVERRIDE));
                val &= ~(0x1 << 2);
                writel(val, IOMEM(CTL_OVERRIDE));

                val = readl(IOMEM(SPEC_CTL));
                val &= ~(0x7 << 0);
                val &= ~(0x1F << 16);
                writel(val, IOMEM(SPEC_CTL));

                //printk("[CCI] Enable Speculative Fetch feature,0x%x,0x%x\n",readl(IOMEM(CTL_OVERRIDE)),readl(IOMEM(SPEC_CTL)));
                break;
            default:
                break;
        }
  return 0; 

}

/*
 * enable_spec_fetch_show: To enable speculative fetch in CCI400.
 */
static ssize_t enable_spec_fetch_show(struct device_driver *driver, char *buf)
{
        unsigned int val;
        char *ptr = buf;
        val = readl(IOMEM(CTL_OVERRIDE));
        if (1 == cci400_spec_fetch_query())
             ptr += sprintf(ptr, "[CCI] Enable speculative fetches\n");
        else
             ptr += sprintf(ptr, "[CCI] Disable speculative fetches\n");

        if (val & (0x1 << 2))
             ptr += sprintf(ptr, "[CCI] Disable speculative fetches in Control Override Register:0x%x\n",val);
        else
             ptr += sprintf(ptr, "[CCI] Enable speculative fetches in Control Override Register:0x%x\n",val);

        val = readl(IOMEM(SPEC_CTL));
        ptr += sprintf(ptr, "[CCI] Speculative fetches info in Speculation Control Register:0x%x\n",val);
        ptr += sprintf(ptr, "M0[APB]:%s\n",(val & (1<<0))?"Disable":"Enable");
        ptr += sprintf(ptr, "M1[?]:%s\n",(val & (1<<1))?"Disable":"Enable");
        ptr += sprintf(ptr, "M2[EMI]:%s\n",(val & (1<<2))?"Disable":"Enable");
        ptr += sprintf(ptr, "S0[?]:%s\n",(val & (1<<16))?"Disable":"Enable");
        ptr += sprintf(ptr, "S1[?]:%s\n",(val & (1<<17))?"Disable":"Enable");
        ptr += sprintf(ptr, "S2[?]:%s\n",(val & (1<<18))?"Disable":"Enable");
        ptr += sprintf(ptr, "S3[CLUSTER1]:%s\n",(val & (1<<19))?"Disable":"Enable");
        ptr += sprintf(ptr, "S4[CLUSTER0]:%s\n",(val & (1<<20))?"Disable":"Enable");
        return strlen(buf);
}
void mcu_div(void)
{
        unsigned int *mcu_div = (unsigned int *)(MCUCFG_BASE + 0x240);
        *mcu_div = 0x14;
        dsb();
        *mcu_div = 0x12;
        dsb();
}
/*
 * enable_spec_fetch_store:
 */
static ssize_t enable_spec_fetch_store(struct device_driver *driver, const char *buf,
			      size_t count)
{
	char *p = (char *)buf;
	unsigned int num;
	num = simple_strtoul(p, &p, 10);
        cci400_spec_fetch_set(num);
	return count;
}
//#ifdef CONFIG_HAS_EARLYSUSPEND
//static struct early_suspend mt_cci400_early_suspend_handler =
//{
//    .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 200,
//    .suspend = NULL,
//    .resume  = NULL,
//};
//
//static void mt_cci400_late_resume(struct early_suspend *h)
//{
//        //to disable speculative fetch when resume from sleep
//        cci400_spec_fetch_set(0);
//}
//#endif
DRIVER_ATTR(enable_spec_fetch, 0664, enable_spec_fetch_show, enable_spec_fetch_store);

static ssize_t xbar_setting_show(struct device_driver *driver, char *buf)
{
    char *ptr = buf;
    ptr += sprintf(ptr, "hazard_disabled: %s\n", *XBARREG & 0x40 ? "yes" : "no");
    sprintf(ptr, "enable AR / AW: %s\n", *XBARREG & 0x11 ? "yes" : "no");
    return strlen(buf);
}

static ssize_t xbar_setting_store(struct device_driver *driver, const char *buf,
			      size_t count)
{
	char *p = (char *)buf;
	unsigned int num;
	num = simple_strtoul(p, &p, 10);
    switch(num) {
    case 0:
        *XBARREG &= ~0x40;
        *XBARREG &= ~0x11;
        break;
    case 1:
        *XBARREG |= 0x40;
        *XBARREG |= 0x11;
        break;
    }
	return count;
}
DRIVER_ATTR(xbar_setting, 0664, xbar_setting_show, xbar_setting_store);

int mt_cci400_init(void){
        int ret;
        CHIP_SW_VER ver = mt_get_chip_sw_ver();
        ret = driver_register(&mt_cci400_drv.driver);
        ret = driver_create_file(&mt_cci400_drv.driver, &driver_attr_enable_spec_fetch);
        ret = driver_create_file(&mt_cci400_drv.driver, &driver_attr_xbar_setting);
        
        //for better performance
        if(ver == CHIP_SW_VER_01){
#ifdef CONFIG_MTK_FORCE_CLUSTER1
            *XBARREG |= 0x40; //hazard_disable
            *XBARREG |= 0x11; //enable AR and AW
#else
            *XBARREG |= 0x40; //hazard_disable
            *XBARREG |= 0x10; //enable AW
#endif
        } else {
            *XBARREG |= 0x40; //hazard_disable
            *XBARREG |= 0x10; //enable AW
        }
        if (ret == 0)
            printk("[CCI] CCI 400 driver init done.\n");
//        /*disable speculative fetch*/
//        cci400_spec_fetch_set(0);
//
//#ifdef CONFIG_HAS_EARLYSUSPEND
//    mt_cci400_early_suspend_handler.resume = mt_cci400_late_resume;
//    register_early_suspend(&mt_cci400_early_suspend_handler);
//#endif
        
	return 0;

}
arch_initcall(mt_cci400_init);
