#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <generated/autoconf.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <linux/delay.h> //Add for msleep
#include <linux/seq_file.h>
#include <mach/wd_api.h>

#include "power_loss_test.h"

#define TAG                 "[MVG_TEST]:"
//#define PWR_LOSS_MT6575      
//#define PWR_LOSS_MT6573
#define PWR_LOSS_MT6582      

/* use software reset to do power loss test, 
 * if not defined means to use hardware(ATE) 
 * reset */
#define PWR_LOSS_SW_RESET         
#define PWR_LOSS_RANDOM_SW_RESET (1)

/* CONFIG_** macro will defined in linux .config file */
#ifdef CONFIG_PWR_LOSS_MTK_DEBUG
#define PWR_LOSS_DEBUG
#endif


#define PWR_LOSS_DEVNAME            "power_loss_test"  //name in /proc/devices & sysfs
#define PWR_LOSS_FIRST_MINOR         0
#define PWR_LOSS_MAX_MINOR_COUNT     1
#define PWR_LOSS_CBUF_LEN            32

#ifndef HZ
#define HZ	100
#endif

#ifdef PWR_LOSS_SW_RESET
    #ifdef PWR_LOSS_RANDOM_SW_RESET
        #define PWR_LOSS_SLEEP_MAX_TIME      (8000)    
    #else
        #define PWR_LOSS_SLEEP_TIME          (6000)    //60second   
    #endif /* end of PWR_LOSS_RANDOM_SW_RESET */
#endif /* end of PWR_LOSS_SW_RESET */

#ifdef PWR_LOSS_MT6575
extern void wdt_arch_reset(char mode);
#elif defined PWR_LOSS_MT6573
    #include "../../../core/include/mach/mt6573_reg_base.h"
    #include "../../../core/include/mach/mt6573_typedefs.h"
    #define PWR_LOSS_WDT_MODE               (AP_RGU_BASE+0)    
    #define PWR_LOSS_WDT_LENGTH             (AP_RGU_BASE+4)  
    #define PWR_LOSS_WDT_RESTART            (AP_RGU_BASE+8)
#endif /* end of PWR_LOSS_MT6575 */


static dev_t sg_pwr_loss_devno;
static struct cdev*   sg_pwr_loss_dev       = NULL;
static struct class*  sg_pwr_loss_dev_class = NULL;
static struct device* sg_pwr_loss_dev_file  = NULL;
//Add for proc debug
#ifdef PWR_LOSS_SW_RESET
static wdt_reboot_info power_loss_info;
static char cmd_buf[256];
#endif

//End of proc debug

int pwr_loss_open(struct inode *inode, struct file *file)
{
#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: Open operation !\n", TAG);
#endif
    return 0;
}

int pwr_loss_release(struct inode *inode, struct file *file)
{
#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: Release operation !\n", TAG);
#endif
    return 0;
}

int pwr_loss_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    char l_buf[PWR_LOSS_CBUF_LEN] = {0};
   
#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: IOCTL operation ! CMD=%d\n", TAG, cmd);
#endif
    
    switch (cmd){
        case PRINT_REBOOT_TIMES:
            ret  = copy_from_user((int *)l_buf, (int *)arg, sizeof(int));
            if (ret != 0){
                printk(KERN_ERR "%s Power Loss Test: IOCTL->PRINT_REBOOT_TIMES %d Bytes can't be copied \n", TAG, ret);
            }
            printk(KERN_ERR "%s Power Loss Test: -----------System Reboot Successfully Times= %d---------------!\n", TAG, ((int *)l_buf)[0]);
            break;
        case PRINT_DATA_COMPARE_ERR:
            printk(KERN_ERR "%s Power Loss Test: -----------Data Compare Error---------------!\n", TAG);
            break;
        case PRINT_FILE_OPERATION_ERR:
            printk(KERN_ERR "%s Power Loss Test: -----------File Operation Error---------------!\n", TAG);
            break;
        case PRINT_GENERAL_INFO:
            ret  = copy_from_user(l_buf,(int *)arg,(sizeof(l_buf[0])*(sizeof(l_buf))));
            if (ret != 0){
                printk(KERN_ERR "%s Power Loss Test: IOCTL->PRINT_REBOOT_TIMES %d Bytes can't be copied \n", TAG, ret);
            }
            
            l_buf[(sizeof(l_buf[0])*(sizeof(l_buf)))-1] = '\0';

#ifdef PWR_LOSS_DEBUG
            printk(KERN_WARNING "%s %s", TAG, l_buf);
#endif
            break;
        case PRINT_RAW_RW_INFO:
            ret  = copy_from_user(l_buf,(int *)arg,(sizeof(l_buf[0])*(sizeof(l_buf))));
            if (ret != 0){
                printk(KERN_ERR "%s Power Loss Test: %d Bytes can't be copied \n", TAG, ret);
            }
            
            l_buf[(sizeof(l_buf[0])*(sizeof(l_buf)))-1] = '\0';

#ifdef PWR_LOSS_DEBUG
            printk(KERN_WARNING "%s %s\n", TAG, l_buf);
#endif
            break;
        default:
            printk(KERN_ERR "%s Power Loss Test: cmd code Error!\n", TAG);
            break;
    }
    
    return 0;    
}

extern void get_random_bytes(void *buf, int nbytes);
static struct file_operations pwr_loss_fops =
{
    .owner = THIS_MODULE,
    .open = pwr_loss_open,
    .release = pwr_loss_release,
    .unlocked_ioctl	= pwr_loss_ioctl,
};

#ifdef PWR_LOSS_SW_RESET
#if PWR_LOSS_RANDOM_SW_RESET
int pwr_loss_reset_thread(void *p)
{    
    signed long ret = 0;
    int HZ_val = HZ;
    struct timespec current_time;
    long sec_time = 0;
    long nsec_time = 0;
    signed long sleep_time = 0;
    struct wd_api *wd_api = NULL;
   
   
#ifdef PWR_LOSS_MT6573
    volatile unsigned short *Reg1 = (unsigned short *)PWR_LOSS_WDT_MODE;
    volatile unsigned short *Reg2 = (unsigned short *)PWR_LOSS_WDT_LENGTH;
    volatile unsigned short *Reg3 = (unsigned short *)PWR_LOSS_WDT_RESTART;
#endif 

    get_random_bytes(&sleep_time, sizeof(signed long));
    if (sleep_time < 0)
        sleep_time &= 0x7fffffff; 
    sleep_time %= PWR_LOSS_SLEEP_MAX_TIME;

#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: sleep time =%ld\n", TAG, sleep_time);
#endif

    while (1){
        printk(KERN_WARNING "%s Power Loss Test: wait for reset...!\n", TAG);
        set_current_state(TASK_UNINTERRUPTIBLE);
        ret = schedule_timeout(sleep_time);
        down_read(&power_loss_info.rwsem);
        if(power_loss_info.wdt_reboot_support == WDT_REBOOT_OFF) {
            up_read(&power_loss_info.rwsem);
            msleep(1000);
            printk(KERN_WARNING "%s Power Loss Test: wdt reboot pause...!\n", TAG);
            continue;
        }
        up_read(&power_loss_info.rwsem);
        printk(KERN_ERR "%s Power Loss Test: ret = %ld, do reset now...\n", TAG, ret);

#ifdef PWR_LOSS_MT6575
    #ifdef CONFIG_MTK_MTD_NAND
    #endif
        wdt_arch_reset(0xff);
#elif defined PWR_LOSS_MT6582
    get_wd_api(&wd_api);
    printk(KERN_ERR "%s Power Loss Test: ret = %ld, do reset now...wd_api = 0x%p\n", TAG, ret,wd_api);
    if (wd_api)
        wd_api->wd_sw_reset(0Xff); 
#elif defined PWR_LOSS_MT6573
    #ifdef CONFIG_MTK_MTD_NAND
        if(!mt6573_nandchip_Reset()){
            printk(KERN_ERR "%s NAND_MVG mt6573_nandchip_Reset Failed!\n", TAG);
        }
    #endif 

        /* reset by watch dog */
        *Reg1 = 0x2200;
        *Reg2 = (0x3F<<5)|0x8;
        *Reg3 = 0x1971;
        *Reg1 = 0x2217;
#endif
        while(1);
    }
}
#else
int pwr_loss_reset_thread(void *p)
{
    signed long ret = 0;
    signed long l_val1 = 0;
    signed long l_val2 = 0;
    signed long l_count = 0;
    struct wd_api *wd_api = NULL;

#ifdef PWR_LOSS_MT6573
    volatile unsigned short *Reg1 = (unsigned short *)PWR_LOSS_WDT_MODE;
    volatile unsigned short *Reg2 = (unsigned short *)PWR_LOSS_WDT_LENGTH;
    volatile unsigned short *Reg3 = (unsigned short *)PWR_LOSS_WDT_RESTART;
#endif 

#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: sleep time = 100sec\n", TAG);
#endif

    while (1){
        printk(KERN_WARNING "%s Power Loss Test: wait for reset...!\n", TAG);
        set_current_state(TASK_UNINTERRUPTIBLE);
        ret = schedule_timeout(PWR_LOSS_SLEEP_TIME);
        down_read(&power_loss_info.rwsem);
        if(power_loss_info.wdt_reboot_support == WDT_REBOOT_OFF) {
            up_read(&power_loss_info.rwsem);
            printk(KERN_WARNING "%s Power Loss Test: wdt reboot pause...!\n", TAG);
            msleep(1000);
            continue;
        }
        up_read(&power_loss_info.rwsem);
        printk(KERN_ERR "%s Power Loss Test: ret = %ld, do reset now...\n", TAG, ret);

#ifdef PWR_LOSS_MT6575
    #ifdef CONFIG_MTK_MTD_NAND
    #endif
        wdt_arch_reset(0xff);
#elif defined PWR_LOSS_MT6582
    get_wd_api(&wd_api);
    printk(KERN_ERR "%s Power Loss Test: ret = %ld, do reset now...wd_api = 0x%x\n", TAG, ret,wd_api);
    if (wd_api)
        wd_api->wd_sw_reset(0Xff); 
#elif defined  PWR_LOSS_MT6573
    #ifdef CONFIG_MTK_MTD_NAND
        if(!mt6573_nandchip_Reset()){
            printk(KERN_ERR "%s NAND_MVG mt6573_nandchip_Reset Failed!\n", TAG);
        }
    #endif 

        /* reset by watch dog */
        *Reg1 = 0x2200;
        *Reg2 = (0x3F<<5)|0x8;
        *Reg3 = 0x1971;
        *Reg1 = 0x2217;
#endif
        while(1);
    }
}
#endif /* end of PWR_LOSS_RANDOM_SW_RESET */
#endif /* end of PWR_LOSS_SW_RESET */

//Add for proc debug
#ifdef PWR_LOSS_SW_RESET
static int power_loss_info_init(void)
{
    init_rwsem(&power_loss_info.rwsem);
    down_write(&power_loss_info.rwsem);
    power_loss_info.wdt_reboot_support = WDT_REBOOT_ON;
    up_write(&power_loss_info.rwsem);
    
    return 0;
}

static int power_loss_debug_write(struct file *file, const char *buf, size_t count, loff_t *data)
{
    int ret;
    int wdt_reboot_support;

    if (count == 0) return -1;
    if (count > 255) count = 255;

    ret = copy_from_user(cmd_buf, buf, count);
    if (ret < 0) return -1;

    cmd_buf[count] = '\0';

    if (1 == sscanf(cmd_buf, "%x", &wdt_reboot_support)) {
        if(wdt_reboot_support < 0){
            printk(KERN_ERR "%s [%s] : command format is error, please help to check!!!\n", TAG, __func__);
            return -1;
        }
        else {
            down_write(&power_loss_info.rwsem);
            power_loss_info.wdt_reboot_support = (wdt_reboot_support == WDT_REBOOT_OFF) ? WDT_REBOOT_OFF : WDT_REBOOT_ON;
            up_write(&power_loss_info.rwsem);

            printk(KERN_NOTICE "%s [****PWR_LOSS_DEBUG****]\n", TAG);
            printk(KERN_WARNING "%s WDT REBOOT\t", TAG);
            if(wdt_reboot_support == WDT_REBOOT_ON)
            {
                printk(KERN_WARNING "%s ON\n", TAG);
            }
            else
                printk(KERN_WARNING "%s OFF\n", TAG);
        }
    }else {
        printk(KERN_ERR "%s [%s] : command format is error, please help to check!!!\n", TAG, __func__);
        return -1;
    }

    return 0;
}

static int power_loss_debug_show(struct seq_file *m, void *v)
{
    int wdt_reboot_support = 0;

    down_read(&power_loss_info.rwsem);
    wdt_reboot_support = power_loss_info.wdt_reboot_support;
    up_read(&power_loss_info.rwsem);
    seq_printf(m, "\n WDT REBOOT STATUS\t%d\n", wdt_reboot_support);   

    return 0;
}

static int power_loss_open(struct inode *inode, struct file *file) 
{ 
    return single_open(file, power_loss_debug_show, inode->i_private); 
} 

static const struct file_operations power_loss_fops = { 
    .open = power_loss_open, 
    .write = power_loss_debug_write,
    .read = seq_read, 
    .llseek = seq_lseek, 
    .release = single_release, 
};

static int power_loss_debug_init(void)
{
    struct proc_dir_entry *prEntry;
    struct proc_dir_entry *tune;
    struct proc_dir_entry *tune_flag;
    prEntry = proc_create("power_loss_debug", 0660, NULL, &power_loss_fops);
    if(prEntry)
    {
        printk("[%s]: successfully create /proc/power_loss_debug\n", __func__);
    }else{
        printk("[%s]: failed to create /proc/power_loss_debug\n", __func__);
        return -1; 
    }

    return 0;
}

#endif
//End for proc debug

static int __init power_loss_init(void)
{
    int err;

    printk(KERN_NOTICE "%s Power Loss Test Module Init\n", TAG);

    err = alloc_chrdev_region(&sg_pwr_loss_devno, PWR_LOSS_FIRST_MINOR, PWR_LOSS_MAX_MINOR_COUNT, PWR_LOSS_DEVNAME);
    if (err != 0){
        printk(KERN_ERR "%s Power Loss Test: alloc_chardev_region Failed!\n", TAG);
        return err;
    }

#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: MAJOR =%d, MINOR=%d\n", TAG, 
                      MAJOR(sg_pwr_loss_devno), MINOR(sg_pwr_loss_devno));
#endif
    
    sg_pwr_loss_dev = cdev_alloc();
    if (NULL == sg_pwr_loss_dev){
        printk(KERN_ERR "%s Power Loss Test: cdev_alloc Failed\n", TAG);
        goto out2;
    }

    sg_pwr_loss_dev->owner = THIS_MODULE;
    sg_pwr_loss_dev->ops   = &pwr_loss_fops;

    err = cdev_add(sg_pwr_loss_dev, sg_pwr_loss_devno, 1);
    if (err != 0){
        printk(KERN_ERR "%s Power Loss Test: cdev_add Failed!\n", TAG);
        goto out2;
    }
    
    sg_pwr_loss_dev_class = class_create(THIS_MODULE, PWR_LOSS_DEVNAME);
    if (NULL == sg_pwr_loss_dev_class){
        printk(KERN_ERR "%s Power Loss Test: class_create Failed!\n", TAG);
        goto out1;
    }

    sg_pwr_loss_dev_file = device_create(sg_pwr_loss_dev_class, NULL, sg_pwr_loss_devno, NULL, PWR_LOSS_DEVNAME);
    if (NULL == sg_pwr_loss_dev_file){
        printk(KERN_ERR "%s Power Loss Test: device_create Failed!\n", TAG);
        goto out;
    }

#ifdef PWR_LOSS_SW_RESET
    power_loss_info_init();
    err = power_loss_debug_init();
    if(err  < 0)
        goto out;
#endif

    printk(KERN_ERR "%s Power Loss Test: Init Successfully!\n", TAG);
    
#ifdef PWR_LOSS_SW_RESET
    kernel_thread(pwr_loss_reset_thread, NULL, CLONE_VM);    //CLONE_KERNEL
    printk(KERN_ERR "%s Power Loss Test: kernel thread create Successful!\n", TAG);
#endif

    return 0;
out:
    class_destroy(sg_pwr_loss_dev_class);
out1:
    cdev_del(sg_pwr_loss_dev);
out2:
    unregister_chrdev_region(sg_pwr_loss_devno, PWR_LOSS_MAX_MINOR_COUNT);
#ifdef PWR_LOSS_SW_RESET
    remove_proc_entry("power_loss_debug", NULL);
#endif
    
    return err;
}

static void __exit power_loss_exit(void)
{
#ifdef PWR_LOSS_DEBUG
    printk(KERN_NOTICE "%s Power Loss Test: Module Exit\n", TAG);
#endif

    unregister_chrdev_region(sg_pwr_loss_devno, PWR_LOSS_MAX_MINOR_COUNT);
    cdev_del(sg_pwr_loss_dev);
    device_destroy(sg_pwr_loss_dev_class, sg_pwr_loss_devno);
    class_destroy(sg_pwr_loss_dev_class);
#ifdef PWR_LOSS_SW_RESET
    remove_proc_entry("power_loss_debug", NULL);
#endif
    
    printk(KERN_ERR "%s Power Loss Test:module exit Successfully!\n", TAG);
}

module_init(power_loss_init);
module_exit(power_loss_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("feifei.wang <feifei.wang@mediatek.com>");
MODULE_DESCRIPTION(" This module is for power loss test");

