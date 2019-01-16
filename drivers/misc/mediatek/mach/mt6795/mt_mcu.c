#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <mach/mt_mcu.h>
#include <mach/mt_reg_base.h>
#include <asm/system.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <mach/dma.h>
#include <linux/dma-mapping.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#define LDVT
#define MCU_DEBUG

#ifdef MCU_DEBUG
#define dbg_printk printk
#else
#define dbg_printk
#endif
extern void smp_inner_dcache_flush_all();

static struct mt_mcu_driver {
        struct device_driver driver;
        const struct platform_device_id *id_table;
};

static struct mt_mcu_driver mt_mcu_drv = {
    .driver = {
	.name = "mcu",
	.bus = &platform_bus_type,
	.owner = THIS_MODULE,
    },
    .id_table= NULL,
};

#ifdef LDVT
#define BUFF_LEN        (1024*1024)

static int ts_emi_access(void){
    unsigned int *test_array_v;
    unsigned int *test_array_p;
    unsigned int i;
    test_array_v = dma_alloc_coherent(NULL, sizeof(unsigned int ) * BUFF_LEN/sizeof(unsigned int), &test_array_p, GFP_KERNEL );

    for(i = 0; i < BUFF_LEN/sizeof(unsigned int); i++)
        test_array_v[i] = i;
    return 0;
}
static int ts_tran_count(void){

    printk("in %s",__FUNCTION__);
    /* set up performance counter */
    /* enable counter */
#if 1
    printk("in testcases:MCU_BIU_EVENT0_CNT=%x,MCU_BIU_EVENT1_CNT:%x\n",*((unsigned int *) MCU_BIU_EVENT0_CNT),*((unsigned int *) MCU_BIU_EVENT1_CNT));
    *((unsigned int *) MCU_BIU_EVENT0_SEL) = 0x2; //AC_R snoop transaction count MCU->CPU
    //*((unsigned int *) MCU_EVENT1_SEL) = 0x4; //AC_R snoop hit
    *((unsigned int *) MCU_BIU_EVENT1_SEL) = 0x0; // DMA->MCU
    *((unsigned int *) MCU_BIU_EVENT0_CON) = 0x1; // enable counter 0 
    *((unsigned int *) MCU_BIU_EVENT1_CON) = 0x1; // enable counter 1
    *((unsigned int *) MCU_BIU_PMCR) |= 0x2; // retset perf counter
    *((unsigned int *) MCU_BIU_PMCR) &= ~0x2; // reset performance counter
#endif
 
    /* Invoke 4 cores concurrent write to EMI with 1MB data set per core. */
    get_online_cpus();
        on_each_cpu(ts_emi_access, NULL, false);
    put_online_cpus();
    /* check performance counter */
    printk("in testcases:MCU_EVENT0_CNT=%x,MCU_EVENT1_CNT:%x\n",*((unsigned int *) MCU_BIU_EVENT0_CNT),*((unsigned int *) MCU_BIU_EVENT1_CNT));

    return 0;
}
/*
 * mci_dvt_show: To show usage.
 */
static ssize_t mcu_dvt_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "MCU dvt test\n");
}

/*
 * mci_dvt_store: To select mci test case.
 */
static ssize_t mcu_dvt_store(struct device_driver *driver, const char *buf,
			      size_t count)
{
	char *p = (char *)buf;
	unsigned int num;

	num = simple_strtoul(p, &p, 10);
        switch(num){
            case 1:
                break;
            case 2:
                break;
            case 3:
                break;
            case 4:
                break;
            default:
                break;
        }

	return count;
}
DRIVER_ATTR(mcu_dvt, 0664, mcu_dvt_show, mcu_dvt_store);
#endif //!LDVT
int mt_mcu_init(void){
        volatile unsigned int reg_val;
        int ret;
        dbg_printk("[MCU] MCU init...\n");
#if 0
        /* enable Out of order*/
        reg_val = readl(MCU_BIU_CON); 
        reg_val |= 0x1;
        writel(reg_val, MCU_BIU_CON);
#endif

#if 0
        /* set order_depth*/
        reg_val = readl(MCU_BIU_CON); 
        reg_val | (0x1 << 0x8);
        writel(reg_val, MCU_BIU_CON);

#endif

#if 0
        /* enable MCU_BIU DCM function*/
        reg_val = readl(MCU_BIU_CON); 
        reg_val |= (0x1 << 12);
        writel(reg_val, MCU_BIU_CON);
#endif
        
#if 0
        /* enable MCUSYS DCM function */
        reg_val = readl(0xF020005C);
        reg_val |= (0x1 << 9);
        writel(reg_val, 0xF020005C);
#endif

#if 0
        /*PC / FP / SP trapper  */
        dbg_printk("CPU0-PC:0x%x,CPU1-PC:0x%x,CPU2-PC:0x%x,CPU3-PC:0x%x\n",readl(0xF0200300),readl(0xF0200310),readl(0xF0200320),readl(0xF0200330));
        dbg_printk("CPU0-FP:0x%x,CPU1-FP:0x%x,CPU2-FP:0x%x,CPU3-FP:0x%x\n",readl(0xF0200304),readl(0xF0200314),readl(0xF0200324),readl(0xF0200334));
        dbg_printk("CPU0-SP:0x%x,CPU1-SP:0x%x,CPU2-SP:0x%x,CPU3-SP:0x%x\n",readl(0xF0200308),readl(0xF0200318),readl(0xF0200328),readl(0xF0200338));
#endif
        ret = driver_register(&mt_mcu_drv.driver);
#ifdef LDVT
	ret = driver_create_file(&mt_mcu_drv.driver, &driver_attr_mcu_dvt);
#endif
        if (ret == 0)
        dbg_printk("MCU init done...\n");
	return ret;

}
int mt_mcu_exit(void){

        return 0;

}
module_init(mt_mcu_init);
module_exit(mt_mcu_exit);
