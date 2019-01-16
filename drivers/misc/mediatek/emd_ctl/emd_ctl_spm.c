#include <linux/sched.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/wakelock.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <emd_ctl.h>
/******************************************************************************************
 *   Macro definition region
 ******************************************************************************************/
#define DEVICE0_ID            (0x0)
#define USER0_NAME            "USB-Host"
#define DEVICE1_ID            (0x1)
#define USER1_NAME            "UART"
#define WAKEUP_EMD_USER_NUM        (2)
/******************************************************************************************
 *   Data structure definition region
 ******************************************************************************************/
typedef struct _wakeup_ctl_info{
    int    dev_major_id;
    int    dev_sub_id;    
    int    time_out_req_count;
    int    manual_req_count;
    char   *name;
}wakeup_ctl_info_t;

/******************************************************************************************
 *   Gloabal variables region
 ******************************************************************************************/ 
static wakeup_ctl_info_t   wakeup_user[WAKEUP_EMD_USER_NUM]; // 0-USB, 1-UART
static void emd_wakeup_timer_cb(unsigned long);
static void emd_wdt_work_func(struct work_struct *data);
static DECLARE_WORK(emd_wdt_work, emd_wdt_work_func);
static DEFINE_TIMER(emd_wakeup_timer,emd_wakeup_timer_cb,0,0);
static unsigned int         timeout_pending_count = 0;
static spinlock_t           wakeup_ctl_lock;
static struct wake_lock     emd_wake_lock;    // Make sure WDT reset not to suspend
/******************************************************************************************
 *   External customization functions region
 ******************************************************************************************/
void request_wakeup_md_timeout(unsigned int dev_id, unsigned int dev_sub_id)
{
    int i;
    unsigned long flags;

    for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
        if( (wakeup_user[i].dev_major_id == dev_id)
          &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
             spin_lock_irqsave(&wakeup_ctl_lock, flags);
            wakeup_user[i].time_out_req_count++;
            if(0 == timeout_pending_count){
                cm_hold_wakeup_md_signal();
                mod_timer(&emd_wakeup_timer, jiffies+2*HZ);
            }
            timeout_pending_count++;
            if(timeout_pending_count > 2)
                timeout_pending_count = 2;
            spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
            break;
        }
    }
}

void request_wakeup_md(unsigned int dev_id, unsigned int dev_sub_id)
{
    int i;
    unsigned long flags;

    for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
        if( (wakeup_user[i].dev_major_id == dev_id)
          &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
             spin_lock_irqsave(&wakeup_ctl_lock, flags);
             if(wakeup_user[i].manual_req_count == 0)
                 cm_hold_wakeup_md_signal();
            wakeup_user[i].manual_req_count++;
            spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
            break;
        }
    }
}

void release_wakeup_md(unsigned int dev_id, unsigned int dev_sub_id)
{
    int i;
    unsigned long flags;

    for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
        if( (wakeup_user[i].dev_major_id == dev_id)
          &&(wakeup_user[i].dev_sub_id == dev_sub_id) ){
             if(wakeup_user[i].manual_req_count == 0){
                 EMD_MSG_INF("chr","E: %s%d mis-match!\n", 
                     wakeup_user[i].name, wakeup_user[i].dev_sub_id);
             }else{
                 spin_lock_irqsave(&wakeup_ctl_lock, flags);
                wakeup_user[i].manual_req_count--;
                if(0 == timeout_pending_count){
                    timeout_pending_count++;
                    mod_timer(&emd_wakeup_timer, jiffies+HZ); // Let time to release
                }
                spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
            }
            break;
        }
    }
}

static void emd_wakeup_timer_cb(unsigned long data  __always_unused)
{
    int release_wakeup_confirm = 1;
    int i;
    unsigned long flags;

    spin_lock_irqsave(&wakeup_ctl_lock, flags);

    // 1. Clear timeout counter
    for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
        if(wakeup_user[i].time_out_req_count > 0){
            wakeup_user[i].time_out_req_count=0;
        }
    }

    // 2. Update timeout pending counter
    if(timeout_pending_count == 0){
        EMD_MSG_INF("chr","timeout_pending_count == 0\n");
    }else{
        timeout_pending_count--;
    }

    // 3. Check whether need to release wakeup signal
    if(timeout_pending_count == 0){ // Need check whether to release
        for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
            if(wakeup_user[i].manual_req_count > 0){
                release_wakeup_confirm = 0;
                break;
            }
        }
        if(release_wakeup_confirm){
            EMD_MSG_INF("chr","cm_release_wakeup_md_signal before\n");
            cm_release_wakeup_md_signal();
        }
    }else{ // Need to run once more
        mod_timer(&emd_wakeup_timer, jiffies+2*HZ);
    }

    spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
}

static void reset_wakeup_control_logic(void)
{
    int i;
    unsigned long flags;

    spin_lock_irqsave(&wakeup_ctl_lock, flags);
    for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
        wakeup_user[i].time_out_req_count=0;
        wakeup_user[i].manual_req_count = 0;
    }
    timeout_pending_count = 0;
    spin_unlock_irqrestore(&wakeup_ctl_lock, flags);
}

static void emd_wdt_work_func(struct work_struct *data)
{
    EMD_MSG_INF("chr","emd_wdt_work_func:Ext MD WDT rst!\n");
    if(emd_request_reset()==0)
    {
        wake_lock_timeout(&emd_wake_lock, 15*HZ);
    }
}
void ext_md_wdt_irq_cb(void)
{
    EMD_MSG_INF("chr","ext_md_wdt_irq_cb:Ext MD WDT rst!\n");
    cm_hold_rst_signal();
    cm_do_md_power_off();
    schedule_work(&emd_wdt_work);
}

void ext_md_wakeup_irq_cb(void)
{
    EMD_MSG_INF("chr","Ext MD wake up request!\n");
    wake_lock_timeout(&emd_wake_lock, 2*HZ);
    cm_disable_ext_md_wakeup_irq();
}

void ext_md_exception_irq_cb(void)
{ 
    EMD_MSG_INF("chr","TODO:Ext MD exception!\n");
    if(emd_md_exception()==0)
    {
        wake_lock_timeout(&emd_wake_lock, 1*HZ);
    }
}

void emd_spm_suspend(bool md_power_off)
{
    EMD_MSG_INF("chr","emd_spm_suspend modem power off::%d\n", md_power_off);
    cm_release_wakeup_md_signal();
    if (md_power_off) {
        cm_disable_ext_md_wakeup_irq();
    } else {
        cm_enable_ext_md_wakeup_irq();
    }
    del_timer(&emd_wakeup_timer);
    reset_wakeup_control_logic();
}
void emd_spm_resume(void)
{
    EMD_MSG_INF("chr","emd_spm_resume!\n");
    cm_disable_ext_md_wakeup_irq();
}

int emd_spm_init(int md_id)
{
    int ret=0, i;
    int major,minor;
    /* 2. Init t */
    wake_lock_init(&emd_wake_lock, WAKE_LOCK_SUSPEND, "ext_md_wake_lock"); 
    cm_gpio_setup();
    /* 3. Init wake up ctl structure */
    memset(wakeup_user, 0, sizeof(wakeup_user));
    wakeup_user[0].dev_major_id = DEVICE0_ID;
    wakeup_user[0].dev_sub_id = 0;
    wakeup_user[0].name = USER0_NAME;
    wakeup_user[1].dev_major_id = DEVICE1_ID;
    wakeup_user[1].dev_sub_id = 1;
    wakeup_user[1].name = USER1_NAME;
    spin_lock_init(&wakeup_ctl_lock);
    return 0;
}

void emd_spm_exit(int md_id)
{
    wake_lock_destroy(&emd_wake_lock);
}

