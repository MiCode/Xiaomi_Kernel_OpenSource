#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/interrupt.h>
#include <mach/mt_reg_base.h>
#include <mach/systracker.h>
#include <mach/sync_write.h>
#include <asm/signal.h>
#include <linux/mtk_ram_console.h>
#include <linux/sched.h>
extern asmlinkage void c_backtrace_ramconsole_print(unsigned long fp, int pmode);

#define TRACKER_DEBUG 1

struct systracker_entry_t 
{
    unsigned int dbg_con;
    unsigned int ar_track_l[BUS_DBG_NUM_TRACKER];
    unsigned int ar_track_h[BUS_DBG_NUM_TRACKER];
    unsigned int ar_trans_tid[BUS_DBG_NUM_TRACKER];
    unsigned int aw_track_l[BUS_DBG_NUM_TRACKER];
    unsigned int aw_track_h[BUS_DBG_NUM_TRACKER];
    unsigned int aw_trans_tid[BUS_DBG_NUM_TRACKER];
};

/* Some chip do not have reg dump, define a weak to avoid build error */
int __weak mt_reg_dump(char *buf) { return 1; }
EXPORT_SYMBOL(mt_reg_dump);

int enable_watch_point(void);
int disable_watch_point(void);
static int set_watch_point_address(unsigned int wp_phy_address);

static int systracker_probe(struct platform_device *pdev);
static int systracker_remove(struct platform_device *pdev);
static int systracker_suspend(struct platform_device *pdev, pm_message_t state);
static int systracker_resume(struct platform_device *pdev);

static void test_systracker(void);

struct systracker_config_t
{
        int state;
        int enable_timeout;
        int enable_slave_err;
        int enable_wp;
        int enable_irq;
        int timeout_ms;
        int wp_phy_address;
};

static struct systracker_config_t track_config;
static struct systracker_entry_t track_entry;

static struct platform_driver systracker_driver =
{
    .probe = systracker_probe,
    .remove = systracker_remove,
    .suspend = systracker_suspend,
    .resume = systracker_resume,
    .driver = {
        .name = "systracker",
        .bus = &platform_bus_type,
        .owner = THIS_MODULE,
    },
};

unsigned int is_systracker_device_registered = 0;
unsigned int is_systracker_irq_registered = 0;


int tracker_dump(char *buf)
{

    char *ptr = buf;
    unsigned int reg_value;
    int i;
    unsigned int entry_valid;
    unsigned int entry_tid;
    unsigned int entry_id;
    unsigned int entry_address;
    unsigned int entry_data_size;
    unsigned int entry_burst_length;


    //if(is_systracker_device_registered)
    {
        /* Get tracker info and save to buf */

        /* BUS_DBG_AR_TRACK_L(__n)
         * [31:0] ARADDR: DBG read tracker entry read address
         */

        /* BUS_DBG_AR_TRACK_H(__n)
         * [14] Valid:DBG read tracker entry valid
         * [13:7] ARID:DBG read tracker entry read ID
         * [6:4] ARSIZE:DBG read tracker entry read data size
         * [3:0] ARLEN: DBG read tracker entry read burst length
         */

        /* BUS_DBG_AR_TRACK_TID(__n)
         * [2:0] BUS_DBG_AR_TRANS0_ENTRY_ID: DBG read tracker entry ID of 1st transaction
         */

#ifdef TRACKER_DEBUG
        printk("Sys Tracker Dump\n");
#endif

        for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
            entry_address       = track_entry.ar_track_l[i];
            reg_value           = track_entry.ar_track_h[i];
            entry_valid         = extract_n2mbits(reg_value,19,19);
            entry_id            = extract_n2mbits(reg_value,7,18);
            entry_data_size     = extract_n2mbits(reg_value,4,6);
            entry_burst_length  = extract_n2mbits(reg_value,0,3);
            entry_tid           = track_entry.ar_trans_tid[i];

            ptr += sprintf(ptr, " \
read entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
read id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);

#ifdef TRACKER_DEBUG
            printk("\
read entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
read id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);
#endif
        }

        /* BUS_DBG_AW_TRACK_L(__n)
         * [31:0] AWADDR: DBG write tracker entry write address
         */

        /* BUS_DBG_AW_TRACK_H(__n)
         * [14] Valid:DBG   write tracker entry valid
         * [13:7] ARID:DBG  write tracker entry write ID
         * [6:4] ARSIZE:DBG write tracker entry write data size
         * [3:0] ARLEN: DBG write tracker entry write burst length
         */

        /* BUS_DBG_AW_TRACK_TID(__n)
         * [2:0] BUS_DBG_AW_TRANS0_ENTRY_ID: DBG write tracker entry ID of 1st transaction
         */
        
      for (i = 0; i < BUS_DBG_NUM_TRACKER; i++) {
            entry_address       = track_entry.aw_track_l[i];
            reg_value           = track_entry.aw_track_h[i];
            entry_valid         = extract_n2mbits(reg_value,19,19);
            entry_id            = extract_n2mbits(reg_value,7,18);
            entry_data_size     = extract_n2mbits(reg_value,4,6);
            entry_burst_length  = extract_n2mbits(reg_value,0,3);
            entry_tid           = track_entry.aw_trans_tid[i];

            ptr += sprintf(ptr, " \
write entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
write id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);

#ifdef TRACKER_DEBUG
            printk("\
write entry = %d, \
valid = 0x%x, \
tid = 0x%x, \
write id = 0x%x, \
address = 0x%x, \
data_size = 0x%x, \
burst_length = 0x%x\n",
                        i,
                        entry_valid,
                        entry_tid,
                        entry_id,
                        entry_address,
                        entry_data_size,
                        entry_burst_length);
#endif
      }
      
      return strlen(buf);
    }

    return -1;
}

static ssize_t tracker_run_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", readl(IOMEM(BUS_DBG_CON)));
}

static ssize_t tracker_run_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    if (unlikely(sscanf(buf, "%u", &value) != 1))
        return -EINVAL;

    if (value == 1) {
        enable_systracker();
    } else if(value == 0) {
        disable_systracker();
    } else {
        return -EINVAL;
    }

    return count;
}

DRIVER_ATTR(tracker_run, 0644, tracker_run_show, tracker_run_store);

static ssize_t enable_wp_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", track_config.enable_wp);
}

static ssize_t enable_wp_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    if (unlikely(sscanf(buf, "%u", &value) != 1))
        return -EINVAL;

    if (value == 1) {
        enable_watch_point();
    } else if(value == 0) {
        disable_watch_point();
    } else {
        return -EINVAL;
    }

    return count;
}

DRIVER_ATTR(enable_wp, 0644, enable_wp_show, enable_wp_store);

static ssize_t set_wp_address_show(struct device_driver *driver, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%x\n", track_config.wp_phy_address);
}

static ssize_t set_wp_address_store(struct device_driver * driver, const char *buf, size_t count)
{
    unsigned int value;

    sscanf(buf, "0x%x", &value);
    printk("watch address:0x%x\n",value);
    set_watch_point_address(value);

    return count;
}

DRIVER_ATTR(set_wp_address, 0644, set_wp_address_show, set_wp_address_store);

static ssize_t tracker_entry_dump_show(struct device_driver *driver, char *buf)
{
    int ret = tracker_dump(buf);
    if (ret == -1)
        printk(KERN_CRIT "Dump error in %s, %d\n", __func__, __LINE__);

    ///*FOR test*/
    //test_systracker();

    return strlen(buf);;
}

static ssize_t tracker_entry_dump_store(struct device_driver * driver, const char *buf, size_t count)
{
#ifdef TRACKER_DEBUG
    test_systracker();
#endif
    return count;
}

DRIVER_ATTR(tracker_entry_dump, 0664, tracker_entry_dump_show, tracker_entry_dump_store);

static ssize_t tracker_last_status_show(struct device_driver *driver, char *buf)
{

    
    if(track_entry.dbg_con & (BUS_DBG_CON_IRQ_AR_STA | BUS_DBG_CON_IRQ_AW_STA)){
        return snprintf(buf, PAGE_SIZE, "1\n");
    } else {
        return snprintf(buf, PAGE_SIZE, "0\n");
    }
}

static ssize_t tracker_last_status_store(struct device_driver * driver, const char *buf, size_t count)
{
    return count;
}

DRIVER_ATTR(tracker_last_status, 0664, tracker_last_status_show, tracker_last_status_store);

static irqreturn_t systracker_isr(int irq, void *dev_id)
{
    unsigned int con;
    static char reg_buf[512];

    printk("Sys Tracker ISR\n");            

    con = readl(IOMEM(BUS_DBG_CON));
    writel(con | BUS_DBG_CON_IRQ_CLR, IOMEM(BUS_DBG_CON));
    dsb();

    if (con & BUS_DBG_CON_IRQ_WP_STA) {
        printk("[TRACKER] Watch address: 0x%x was touched\n", track_config.wp_phy_address);
        if (mt_reg_dump(reg_buf) == 0) {
            printk("%s\n", reg_buf);
        }
    }

    return IRQ_HANDLED;
}

int enable_watch_point(void)
{
    /* systracker interrupt registration */
    if (!is_systracker_irq_registered) {
        if (request_irq(BUS_DBG_TRACKER_IRQ_BIT_ID, systracker_isr, IRQF_TRIGGER_LOW, "SYSTRACKER", NULL)) {
            printk(KERN_ERR "SYSTRACKER IRQ LINE NOT AVAILABLE!!\n");
        }
        else {
            is_systracker_irq_registered = 1;
        }
    }
    writel(track_config.wp_phy_address, IOMEM(BUS_DBG_WP));
    writel(0x0000000F, IOMEM(BUS_DBG_WP_MASK));
    track_config.enable_wp = 1;
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_WP_EN, IOMEM(BUS_DBG_CON));
    dsb();

    return 0;
}

int disable_watch_point(void)
{
    track_config.enable_wp = 0;
    writel(readl(IOMEM(BUS_DBG_CON)) & ~BUS_DBG_CON_WP_EN, IOMEM(BUS_DBG_CON));
    dsb();

    return 0;
}

static int set_watch_point_address(unsigned int wp_phy_address)
{
   track_config.wp_phy_address = wp_phy_address;

   return 0;
}


void enable_systracker(void)
{
    unsigned int timer_control_value;

    /* prescale = (266 * (10 ^ 6)) / 16 = 16625000/s = 16625/ms */
    timer_control_value = (BUS_DBG_BUS_MHZ * 1000 / 16) * track_config.timeout_ms;
    writel(timer_control_value, IOMEM(BUS_DBG_TIMER_CON));

    track_config.state = 1;
    
    writel(BUS_DBG_CON_DEFAULT_VAL & (~BUS_DBG_CON_SW_RST_DN), IOMEM(BUS_DBG_CON));  
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SW_RST_DN, IOMEM(BUS_DBG_CON));
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_SW_RST, IOMEM(BUS_DBG_CON));
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_IRQ_CLR, IOMEM(BUS_DBG_CON));
    writel(readl(IOMEM(BUS_DBG_CON)) | BUS_DBG_CON_BUS_DBG_EN, IOMEM(BUS_DBG_CON));

    dsb();

}

void disable_systracker(void)
{
    track_config.state = 0;
    writel(readl(IOMEM(BUS_DBG_CON)) & ~BUS_DBG_CON_BUS_DBG_EN, IOMEM(BUS_DBG_CON));
    dsb();

}

static int systracker_probe(struct platform_device *pdev)
{
    int ret;

#if 0
    /* FOR test */
    static char buf[4096];
    tracker_dump(buf);
#endif

    printk("systracker probe\n");
    is_systracker_device_registered = 1;

    memset(&track_config, sizeof(struct systracker_config_t), 0);
    /* To latch last PC when tracker timeout, we need to enable interrupt mode */
    track_config.enable_timeout = 1;
    track_config.timeout_ms = 100;

    enable_systracker();
    
    /* Create sysfs entry */
    ret = driver_create_file(&systracker_driver.driver, &driver_attr_tracker_entry_dump);
    ret |= driver_create_file(&systracker_driver.driver, &driver_attr_tracker_run);
    ret |= driver_create_file(&systracker_driver.driver, &driver_attr_enable_wp);
    ret |= driver_create_file(&systracker_driver.driver, &driver_attr_tracker_last_status);
    ret |= driver_create_file(&systracker_driver.driver, &driver_attr_set_wp_address);
    if (ret) {
        pr_err("Fail to create systracker_drv sysfs files");
    }

    /*FOR test*/
    //static char buf[4096];
    //tracker_dump(buf);

    return 0;
}

static int systracker_remove(struct platform_device *pdev)
{
    return 0;
}

static int systracker_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

static int systracker_resume(struct platform_device *pdev)
{
    if (track_config.state) {
        enable_systracker();
    }

    if (track_config.enable_wp) {
        enable_watch_point();
    }

    return 0;
}

#ifdef TRACKER_DEBUG
static void test_systracker(void)
{

    *(volatile unsigned int*)0xF00062c4 &= 0xfffffffe;
    dsb();
    *(volatile unsigned int*)0xf3000000;
    while (1);
}
#endif 


void dump_backtrace_entry_ramconsole_print(unsigned long where, unsigned long from, unsigned long frame)
{
    char str_buf[256];

#ifdef CONFIG_KALLSYMS
    snprintf(str_buf, sizeof(str_buf), "[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where, (void *)where, from, (void *)from);
#else
    snprintf(str_buf, sizeof(str_buf), "Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif
    aee_sram_fiq_log(str_buf);
}

void dump_regs(const char *fmt, const char v1, const unsigned int reg, const unsigned int reg_val)
{
    char str_buf[256];
    snprintf(str_buf, sizeof(str_buf), fmt, v1, reg, reg_val);
    aee_sram_fiq_log(str_buf);
}

static int verify_stack(unsigned long sp)
{
    if (sp < PAGE_OFFSET ||(sp > (unsigned long)high_memory && high_memory != NULL))
        return -EFAULT;

    return 0;
}

static void dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
    char str_buf[256];
    unsigned int fp, mode;
    int ok = 1;
    
    snprintf(str_buf, sizeof(str_buf), "PC is 0x%lx, LR is 0x%lx\n", regs->ARM_pc, regs->ARM_lr); 
    aee_sram_fiq_log(str_buf);
	
    if (!tsk)
        tsk = current;

    if (regs) {
        fp = regs->ARM_fp;
        mode = processor_mode(regs);
    } else if (tsk != current) {
        fp = thread_saved_fp(tsk);
        mode = 0x10;
    } else {
        asm("mov %0, fp" : "=r" (fp) : : "cc");
        mode = 0x10;
    }

    if (!fp) {
        aee_sram_fiq_log("no frame pointer");
        ok = 0;
    } else if (verify_stack(fp)) {
        aee_sram_fiq_log("invalid frame pointer");
        ok = 0;
    } else if (fp < (unsigned long)end_of_stack(tsk))
        aee_sram_fiq_log("frame pointer underflow");
    aee_sram_fiq_log("\n");

    if (ok)
        c_backtrace_ramconsole_print(fp, mode);
}

int systracker_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
    int i;    
    
    dump_backtrace(regs, NULL);
    
    if(readl(IOMEM(BUS_DBG_CON)) & BUS_DBG_CON_IRQ_AR_STA){
        for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
            printk(KERN_ALERT "AR_TRACKER Timeout Entry[%d]: ReadAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n", i, readl(IOMEM(BUS_DBG_AR_TRACK_L(i))), readl(IOMEM(BUS_DBG_AR_TRACK_H(i))), readl(IOMEM(BUS_DBG_AR_TRANS_TID(i))));
        }
    } 
    
    if(readl(IOMEM(BUS_DBG_CON)) & BUS_DBG_CON_IRQ_AW_STA){
        for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
            printk(KERN_ALERT "AW_TRACKER Timeout Entry[%d]: WriteAddr:0x%x, Length:0x%x, TransactionID:0x%x!\n", i, readl(IOMEM(BUS_DBG_AW_TRACK_L(i))), readl(IOMEM(BUS_DBG_AW_TRACK_H(i))), readl(IOMEM(BUS_DBG_AW_TRANS_TID(i))));
        }
    }
    
    return -1;
}
/*
 * save entry info early
*/
void save_entry(void)
{
    int i;
    track_entry.dbg_con =  readl(IOMEM(BUS_DBG_CON));
    
    for(i = 0; i < BUS_DBG_NUM_TRACKER; i++){
        track_entry.ar_track_l[i]   = readl(IOMEM(BUS_DBG_AR_TRACK_L(i)));
        track_entry.ar_track_h[i]   = readl(IOMEM(BUS_DBG_AR_TRACK_H(i)));
        track_entry.ar_trans_tid[i] = readl(IOMEM(BUS_DBG_AR_TRANS_TID(i)));
        track_entry.aw_track_l[i]   = readl(IOMEM(BUS_DBG_AW_TRACK_L(i)));
        track_entry.aw_track_h[i]   = readl(IOMEM(BUS_DBG_AW_TRACK_H(i)));
        track_entry.aw_trans_tid[i] = readl(IOMEM(BUS_DBG_AW_TRANS_TID(i)));
    }
    
}
/*
 * driver initialization entry point
 */
static int __init systracker_init(void)
{
    int err;
    
    save_entry();
    err = platform_driver_register(&systracker_driver);
    if (err) {
        return err;
    }

#ifdef CONFIG_ARM_LPAE
    hook_fault_code(0x11, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
#else
    hook_fault_code(0x16, systracker_handler, SIGTRAP, 0, "Systracker debug exception");
#endif

    printk(KERN_ALERT "systracker init done\n");
    return 0;
}

/*
 * driver exit point
 */
static void __exit systracker_exit(void)
{
}

module_init(systracker_init);
module_exit(systracker_exit);
