#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/timer.h>

#include "eemcs_kal.h"
#include "lte_hif_sdio.h"
#include "lte_main.h"
#include "lte_dev_test.h"
#include "lte_df_main.h"
//#include "lte_df_fwdl.h"

#ifdef MT_LTE_AUTO_CALIBRATION
#include "mach/mt_boot.h"
extern int wait_sdio_autok_ready(void *);
#endif


//----------------------------------------------

unsigned int LTE_WD_timeout_indicator = 0;

extern void lte_sdio_on(void);
extern void lte_sdio_off(void);

extern void lte_sdio_device_power_on(void);
extern void lte_sdio_device_power_off(void);
extern void lte_sdio_card_identify(void);
extern void lte_sdio_card_remove(void);

extern void lte_sdio_enable_eirq(void);
extern void lte_sdio_disable_eirq(void);

extern void lte_sdio_trigger_wakedevice(void);
extern void lte_sdio_turnoff_wakedevice(void);

#ifdef MT_LTE_AUTO_CALIBRATION
extern BOOTMODE get_boot_mode(void);
#endif

unsigned int lte_kal_debug_level = DBG_ERROR;
 
void mtlte_kal_set_dbglevel(unsigned int level)			
{										
	lte_kal_debug_level = (level>=DBG_OFF && level <=DBG_LOUD)? level : lte_kal_debug_level ;	
}	

#if INTEGRATION_DEBUG
extern KAL_UINT32 eemcs_sdio_throughput_log;
extern KAL_UINT32 log_sdio_ul_now;
extern KAL_UINT32 log_sdio_dl_now;
extern KAL_UINT32 log_sdio_ul_history;
extern KAL_UINT32 log_sdio_dl_history;
extern KAL_UINT32 log_sdio_buf_pool;
extern KAL_UINT32 log_sdio_ul_txqcnt;
#endif

#if USING_WAKE_MD_EINT
KAL_UINT32 give_own_counter = 0;
//KAL_UINT32 ownback_delay_ratio = 100;

// The HZ parameter = 100, but it will have 20ms miss.
// so modify ratio = 30ms -> = 3
KAL_UINT32 ownback_delay_ratio = 3;
#endif

unsigned int str_to_int_main(char *str)
{
	unsigned int ret_int = 0 , tmp = 0;
	unsigned int idx = 0 , len = 0;

	len = strlen(str);
	for (idx = 0 ; idx < len ; idx ++) {
		tmp = str[idx] - 0x30;
		if (idx != 0) {
			ret_int = ret_int * 10;
		}
		ret_int += tmp;
	}
	return ret_int;
} 
static int major; 
static char msg[200];
char msg2[200];

int sdio_on_state = 0;


dev_t dev_sdio_on;
struct device *devices_sdio_on;
struct class *dev_class_sdio_on;

static int mtlte_sys_sdio_kick_thread(unsigned int data);

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
  	return simple_read_from_buffer(buffer, length, offset, msg, 200);
}

#if FORMAL_DL_FLOW_CONTROL_TEST
extern void mtlte_hif_print_fl_ctrl(void);
#endif


static ssize_t device_write(struct file *filp, const char __user *buff, size_t len, loff_t *off)
{
    char *msg_ptr;
    char *cmd_ptr;
    char *level_num_str;
    unsigned int  level_num;
    
	if (len > 199)
		return -EINVAL;

    if(copy_from_user(msg, buff, len))
    {
		return -EINVAL;
    }

    msg[len] = '\0';

    //msg_ptr = msg;
    //msg_ptr2 = msg2;
    //msg_ptr = strsep(&msg_ptr2, " ");

    if(0 == strcmp(msg, "lte_sdio_on"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_on();
    }
    else if(0 == strcmp(msg, "lte_sdio_off"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_off();
    }
    else if(0 == strcmp(msg, "device_pow_on"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_device_power_on();
    }
    else if(0 == strcmp(msg, "device_pow_off"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_device_power_off();
    }
    else if(0 == strcmp(msg, "card_identify"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_card_identify();
    }
    else if(0 == strcmp(msg, "card_remove"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_card_remove();
    }
    else if(0 == strcmp(msg, "eirq_mask"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_disable_eirq();
    }
    else if(0 == strcmp(msg, "eirq_unmask"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        lte_sdio_enable_eirq();
    }
    else if(0 == strcmp(msg, "manual_enable_sleep"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        mtlte_hif_sdio_enable_fw_own_back(1);
        mtlte_sys_sdio_kick_thread(0);
        //temp_sdio_enable_sleep = 1;
    }
    else if(0 == strcmp(msg, "manual_disable_sleep"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        mtlte_hif_sdio_enable_fw_own_back(0);
        mtlte_sys_sdio_kick_thread(0);
        //temp_sdio_enable_sleep = 0;
    }
#if USING_WAKE_MD_EINT
    else if(0 == strcmp(msg, "Read_fw_own"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        KAL_RAWPRINT (("SDIO fw_own = %d \n", mtlte_hif_sdio_check_fw_own()));
    }
    else if(0 == strcmp(msg, "Read_give_own_time"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        KAL_RAWPRINT (("SDIO give_own_counter = %d \n", give_own_counter));
    }
#endif
#if FORMAL_DL_FLOW_CONTROL
    else if(0 == strcmp(msg, "show_flow_control_record"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        for(level_num=0; level_num<RXQ_NUM; level_num++){
            KAL_RAWPRINT (("Biggest num of sk_buff of RXQ%d = %d \n", level_num, mtlte_df_DL_read_fl_ctrl_record(level_num)));
        }
    }
#endif
#if FORMAL_DL_FLOW_CONTROL_TEST
    else if(0 == strcmp(msg, "flow_control_status"))
    {
        KAL_RAWPRINT (("Perform: %s\n", msg));
        for(level_num=0; level_num<RXQ_NUM; level_num++){
            mtlte_df_DL_fl_ctrl_print_status(level_num);
        }

        mtlte_hif_print_fl_ctrl();
    }
#endif
    else
    {
        msg_ptr = msg;
        cmd_ptr = strsep(&msg_ptr, " ");
        
        if(0 == strcmp(cmd_ptr, "set_dbg_msg_level")){
            level_num_str = strsep(&msg_ptr, " ");

            level_num = level_num_str[0] - 0x30;
            mtlte_kal_set_dbglevel(level_num);

            KAL_RAWPRINT (("dbg_msg_level : change to %s\n", level_num_str));
        }
#if USING_WAKE_MD_EINT
        else if(0 == strcmp(cmd_ptr, "set_ownback_delay_ratio")){
            level_num_str = strsep(&msg_ptr, " ");

            level_num = str_to_int_main(level_num_str);
            ownback_delay_ratio = level_num;
            
            if(level_num){
                KAL_RAWPRINT (("set_ownback_delay_ratio : change to %d/%d = 1/%d second. \n", ownback_delay_ratio, HZ, ownback_delay_ratio));
            }else{
                KAL_RAWPRINT (("set_ownback_delay_ratio : Turn off delay ownback!! \n"));
            }
        }
#endif
#if INTEGRATION_DEBUG
        else if(0 == strcmp(cmd_ptr, "switch_throughput_log"))
        {
            level_num_str = strsep(&msg_ptr, " ");
            level_num = level_num_str[0] - 0x30;
            
            KAL_RAWPRINT (("Perform: %s %d \n", msg, level_num));
            if(level_num == 0) {
                eemcs_sdio_throughput_log = 0;
                log_sdio_ul_now = 0;
                log_sdio_dl_now = 0;
                log_sdio_ul_history = 0;
                log_sdio_dl_history = 0;
                log_sdio_buf_pool = 0;
                log_sdio_ul_txqcnt = 0;
            }
            else{eemcs_sdio_throughput_log = 1;}

            if(level_num == 1){log_sdio_ul_now = 1;}
            if(level_num == 2){log_sdio_dl_now = 1;}
            if(level_num == 3){log_sdio_ul_history = 1;}
            if(level_num == 4){log_sdio_dl_history = 1;}
            if(level_num == 5){log_sdio_buf_pool = 1;}
            if(level_num == 6){log_sdio_ul_txqcnt = 1;}

        }
#endif
#if FORMAL_DL_FLOW_CONTROL_TEST
        if(0 == strcmp(cmd_ptr, "manual_release_DL_buff")){
            level_num_str = strsep(&msg_ptr, " ");

            level_num = level_num_str[0] - 0x30;
            level_num_str = strsep(&msg_ptr, " ");
            
            mtlte_df_DL_release_buff(level_num, str_to_int_main(level_num_str), NULL);
            KAL_RAWPRINT (("manual_release_DL_buff : DLQ%d - %d\n", level_num, str_to_int_main(level_num_str)));
        }
#endif
        else{
            KAL_RAWPRINT (("Unknown cmd : %s\n", msg));
        }
    }
   
	return len;
}

static struct file_operations fops = {
	.read = device_read, 
	.write = device_write,
};

static int sdio_onoff_module_init(void)
{
    
	major = register_chrdev(0, "sdio_onoff_dev", &fops);
	if (major < 0) {
     		printk ("Registering the character device failed with %d\n", major);
	     	return major;
	}
	printk("cdev example: assigned major: %d\n", major);

    dev_class_sdio_on = class_create(THIS_MODULE, "sdio_onoff_class");
    dev_sdio_on = MKDEV(major, 0);
	devices_sdio_on = device_create( (struct class *)dev_class_sdio_on, NULL, dev_sdio_on, NULL, "%s", "sdio_onoff_dev" );
 	return 0;
}

static void sdio_onoff_module_exit(void)
{
    device_destroy(dev_class_sdio_on, dev_sdio_on);
    class_destroy(dev_class_sdio_on);
	unregister_chrdev(major, "sdio_onoff_dev");
}  

//----------------------------------------------


static struct mtlte_dev lte_dev ;

struct mtlte_dev *lte_dev_p = &lte_dev; 

static const struct sdio_device_id mt_lte_sdio_id_table[] = {
    //{ SDIO_DEVICE(MT_LTE_SDIO_VENDOR_ID, MT_LTE_SDIO_DEVICE_ID) },
    { SDIO_DEVICE(0x037A, 0x7128) },
    { SDIO_DEVICE(0x037A, 0x7208) },
    { SDIO_DEVICE(0x037A, 0x6290) },    
    { /* end: all zeroes */ },
};


#ifdef MT_LTE_ONLINE_TUNE_SUPPORT
extern int ot_dev_wakeup(void *data);
extern int ot_set_dev_sleep_sts(void *data, int is_sleep);
#endif

inline void mtlte_sys_disable_int_isr(struct sdio_func *func)
{
	int err_ret ;
	unsigned int value ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_sys_disable_int_isr <==========>\r\n")) ;

	value = W_INT_EN_CLR ; 
	sdio_claim_host(func);	
	err_ret = sdio_writesb(func,SDIO_IP_WHLPCR, &value, 4);
	sdio_release_host(func);
	if (err_ret){
		KAL_RAWPRINT(("[ISR] XXXXXX mtlte_sys_disable_int_isr fail, %d \n", err_ret )); 
		return ;
	}

}


static int mtlte_sys_sdio_sleep_thread(unsigned int data) 
{	
	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_sys_sdio_kick_thread <==========>\r\n")) ;
	
	lte_dev.sdio_thread_kick = 0 ;
	wait_event_interruptible( lte_dev.sdio_thread_wq, lte_dev.sdio_thread_kick);
	return 0 ;
}

static int mtlte_sys_sdio_kick_thread(unsigned int data) 
{	
	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_sys_sdio_kick_thread <==========>\r\n")) ;
	
	lte_dev.sdio_thread_kick = 1 ;
	wake_up_all(&lte_dev.sdio_thread_wq);
	return 0 ;
}


static void mtlte_sys_lte_sdio_isr(struct sdio_func *func) // handle the SW int, abnormal int or fw_own_back int in mtlte_hif_sdio_process
{	
	KAL_DBGPRINT(KAL, DBG_TRACE,("[INTERRUPT] interrupt occures\r\n")) ;

    if (0 == LTE_WD_timeout_indicator)
    {
    #ifdef MT_LTE_ONLINE_TUNE_SUPPORT
    ot_set_dev_sleep_sts((void*)lte_dev.sdio_func->card->host, 0);
    ot_dev_wakeup( (void*)lte_dev.sdio_func->card->host );
    #endif
    
	/* disable the interrupt at first */
	mtlte_sys_disable_int_isr(func) ;

        #if USING_WAKE_MD_EINT
        #else
	/* own is back as interrupt occurs */
	mtlte_hif_sdio_clear_fw_own() ;
        #endif

    /* temp re-enable external interrupt here, because system will disable interrupt when interrupt happen */
    lte_sdio_enable_eirq();
	
	    lte_dev.sdio_thread_kick_isr = 1 ;
    wake_up_all(&lte_dev.sdio_thread_wq);
    }
    else
    {
        lte_sdio_disable_eirq();
    }
}


static int mtlte_sys_sdio_setup_irq(struct sdio_func *sdiofunc)
{
	int ret = 0 ;

	KAL_RAWPRINT(("[PROBE] =======> mtlte_sys_sdio_setup_irq\n")); 

	sdio_claim_host(sdiofunc);
    ret = sdio_claim_irq(sdiofunc, mtlte_sys_lte_sdio_isr);
    sdio_release_host(sdiofunc);

    if (ret){
        KAL_RAWPRINT(("[PROBE] XXXXXX mtlte_sys_sdio_setup_irq fail, %d \n", ret )); 
        return (ret);
    }

    mtlte_hif_enable_interrupt_at_probe();
    lte_sdio_enable_eirq();

    KAL_RAWPRINT(("[PROBE] <======= mtlte_sys_sdio_setup_irq\n")); 
    return 0 ;
}

static int mtlte_sys_sdio_remove_irq(struct sdio_func *sdiofunc)
{
	int ret = 0 ;

	KAL_RAWPRINT(("[REMOVE] =======> mtlte_sys_sdio_remove_irq\n")); 

    lte_sdio_disable_eirq();
    
	sdio_claim_host(sdiofunc);
    ret = sdio_release_irq(sdiofunc);
    sdio_release_host(sdiofunc);

    if (ret){
        KAL_RAWPRINT(("[REMOVE] XXXXXX mtlte_sys_sdio_remove_irq fail, %d \n", ret )); 
        return (ret);
    }

    KAL_RAWPRINT(("[REMOVE] <======= mtlte_sys_sdio_remove_irq\n")); 
    return 0 ;
}

static void give_own_timer_callback(unsigned long data)
{
    lte_dev.sdio_thread_kick_own_timer = 1 ;
    wake_up_all(&lte_dev.sdio_thread_wq);
}


static 
int mtlte_sys_sdio_thread(void *_ltedev)
{
    struct mtlte_dev *ltedev = (struct mtlte_dev *)_ltedev;	
    struct timer_list give_own_timer;
    unsigned long give_own_deley_t;

    /* set same task priority with original EMCS */
    struct sched_param param = { .sched_priority = RTPM_PRIO_MTLTE_SYS_SDIO_THREAD };
    sched_setscheduler(current, SCHED_FIFO, &param);

    init_timer(&give_own_timer);
    give_own_timer.function = give_own_timer_callback;
    give_own_timer.data = 0;
    

	lte_dev.sdio_thread_state = SDIO_THREAD_RUNNING ; 
    while (1) {
        wait_event_interruptible(
            ltedev->sdio_thread_wq,
            (kthread_should_stop() /* check this first! */ 
            || ltedev->sdio_thread_kick || ltedev->sdio_thread_kick_isr ));  
        
        if (kthread_should_stop()){
            break ;
        }
        
        if (!ltedev->card_exist){
            ltedev->sdio_thread_kick =0 ;
            ltedev->sdio_thread_kick_isr =0 ;
            continue ;
        }
        
#if USING_WAKE_MD_EINT
        if(1 == mtlte_hif_sdio_check_fw_own())
        {
            if(ltedev->sdio_thread_kick_isr){
                mtlte_hif_sdio_get_driver_own_in_main();
            }
            else{
                 mtlte_hif_sdio_wake_MD_up_EINT();
                 wait_event_interruptible(
                     ltedev->sdio_thread_wq, (kthread_should_stop() || ltedev->sdio_thread_kick_isr)); 
                     if (kthread_should_stop()) {
                         lte_sdio_turnoff_wakedevice();
                         break;
                     }

                 KAL_RAWPRINT((" [SDIO][Sleep] MD acked wake_up_EINT with isr.\n")); 

                 mtlte_hif_sdio_get_driver_own_in_main();
            }
        }
#endif

        ltedev->sdio_thread_kick =0;
        ltedev->sdio_thread_kick_isr =0;
        
        mtlte_hif_sdio_process() ; 	
    
#if USING_WAKE_MD_EINT
        if(0 == ltedev->sdio_thread_kick && 0 == ltedev->sdio_thread_kick_isr){

            if(ownback_delay_ratio){
                ltedev->sdio_thread_kick_own_timer = 0;
                //give_own_deley_t = HZ / ownback_delay_ratio;
                
                /* direct use ownback_delay_ratio as jiffies; */
                give_own_deley_t = ownback_delay_ratio;
                give_own_timer.expires = jiffies + give_own_deley_t;
                add_timer(&give_own_timer);
                //KAL_RAWPRINT((" [SDIO][Sleep] set up timer \n"));

                wait_event_interruptible(
                ltedev->sdio_thread_wq,
                (kthread_should_stop()|| ltedev->sdio_thread_kick || ltedev->sdio_thread_kick_isr || ltedev->sdio_thread_kick_own_timer)); 
                if (kthread_should_stop()) {
                    del_timer_sync(&give_own_timer);
                    ltedev->sdio_thread_kick_own_timer = 0;
                    break;
                }
                /*
                KAL_RAWPRINT((" [SDIO][Sleep] Thread wake up, sdio_thread_kick=%d, sdio_thread_kick_isr=%d, sdio_thread_kick_own_timer=%d \n", \
                    ltedev->sdio_thread_kick, ltedev->sdio_thread_kick_isr, ltedev->sdio_thread_kick_own_timer)); 
                */
                if(0 == ltedev->sdio_thread_kick && 0 == ltedev->sdio_thread_kick_isr && 1 ==ltedev->sdio_thread_kick_own_timer ){
                    KAL_RAWPRINT((" [SDIO][Sleep] wake up by timer \n")); 
                    #ifdef MT_LTE_ONLINE_TUNE_SUPPORT
                    ot_set_dev_sleep_sts((void*)lte_dev.sdio_func->card->host, 1);
                    #endif
                    
                    mtlte_hif_sdio_give_fw_own_in_main() ;
                    give_own_counter++; 
                    
                }else{
                    del_timer_sync(&give_own_timer);
                    ltedev->sdio_thread_kick_own_timer = 0;
                }
            }
            else{
                KAL_RAWPRINT((" [SDIO][Sleep] ownback_delay_ratio == 0, give own back directly \n")); 
                #ifdef MT_LTE_ONLINE_TUNE_SUPPORT
                ot_set_dev_sleep_sts((void*)lte_dev.sdio_func->card->host, 1);
                #endif
                
                mtlte_hif_sdio_give_fw_own_in_main() ;
            }
        }
#endif    
    }

    lte_dev.sdio_thread_state = SDIO_THREAD_IDLE ; 
    KAL_RAWPRINT(("[REMOVE] =======> mtlte_sys_sdio_thread\n")); 
    return 0 ;
	
}

static 
int mtlte_sys_check_sdio_thread_stop(void)
{
    unsigned int cnt = 500 ;	// wait 5 seconds 

    while (cnt--) {
		if (lte_dev.sdio_thread_state == SDIO_THREAD_IDLE)
		{
			KAL_RAWPRINT(("[REMOVE] mtlte_sys_check_sdio_thread_stop OK !!\r\n")); 
			return 0 ;
		}

    	KAL_SLEEP_MSEC(10) ;
    }

	KAL_RAWPRINT(("[REMOVE ERR] mtlte_sys_check_sdio_thread_stop FAIL !!\r\n"));         
    return -1 ;
	
}

#ifdef MT_LTE_AUTO_CALIBRATION
struct proc_dir_entry *s_proc = NULL;

#define LTE_AUTOK_PROC_NAME "lte_autok"
#define PROC_BUF_SIZE 256

volatile int lte_autok_finish = 0;
volatile int lte_autok_running = 0;

#ifndef NATIVE_AUTOK
static int mtlte_sys_trigger_auto_calibration(void *data)
{
    char *envp[3];
    char *p;
    int u4Func = 1;
    int err;
    struct mmc_host *host;

    printk("[%s] Enter %s\n", __func__, __func__);
    
    if(lte_autok_running == 3)
    {
        printk("[%s] LTE auto-K is running\n", __func__);
        return -1;
    }
    
    lte_autok_running++;

	if(lte_dev.sdio_func == NULL)
	{
        printk("[%s] LTE have not probed\n", __func__);
        return -1;
    }

	host = lte_dev.sdio_func->card->host;
	
    
    envp[0] = "FROM=lte_drv";
    envp[1] = "SDIOFUNC=0xXXXXXXXX";
    envp[2] = NULL;
    
    
    //p = strstr(envp[1], "0xXXXXXXXX");
	p = envp[1]+9;	// point to the beginning of 0xXXXXXXXX
    
    sprintf(p, "0x%x", (unsigned int)(host->card->sdio_func[u4Func - 1]));
    
//    printk("[%s] envp[0] = %s, envp[1] = %s, host->class_dev.kobj.name = %s\n", __func__, envp[0], envp[1], host->class_dev.kobj.name);
//    printk("[%s] host = 0x%x, host->class_dev.kobj.name = %s\n", __func__, host, host->class_dev.kobj.name);
    
    KAL_SLEEP_SEC(1);
    err = kobject_uevent_env(&host->class_dev.kobj, KOBJ_ONLINE, envp);
    if(err < 0)
    {
        printk("kobject_uevent_env error = %d\n", err);
        return -1;
    }
    
    return 0;
}
#endif  // #ifndef NATIVE_AUTOK

static int lte_autok_writeproc(struct file *file,const char *buffer,
                           size_t count, loff_t *data)
{
    char bufferContent[PROC_BUF_SIZE];
    
    if(count >= PROC_BUF_SIZE)
    {
        printk(KERN_INFO "[%s] proc input size (%d) is larger than buffer size (%d) \n", __func__, (unsigned int)(count), PROC_BUF_SIZE);
        return -EFAULT;
    }
    
    if (copy_from_user(bufferContent, buffer, count))
        return -EFAULT;
        
    bufferContent[count] = '\0';
    
    //printk(KERN_INFO "[%s] bufferContent = %s \n", __func__, bufferContent);
    //printk(KERN_INFO "[%s] lte_dev.sdio_func = 0x%x\n", __func__, lte_dev.sdio_func);
    
    if(strcmp(bufferContent,"system_server") == 0)
    {
        struct task_struct *task;
        
        // send uevent
        printk(KERN_INFO "[%s] system_server\n", __func__);
		
#ifndef NATIVE_AUTOK		
        task = kthread_run(&mtlte_sys_trigger_auto_calibration,NULL,"trigger_autok");
#else        
        task = kthread_run(&wait_sdio_autok_ready, (void*)lte_dev.sdio_func->card->host, "trigger_autok");
#endif        
    }
    else if(strcmp(bufferContent,"autok_done") == 0)
    {
        // change g_autok_finish
		printk(KERN_INFO "[%s] autok_done\n", __func__);
        //lte_autok_finish = 1;
        lte_autok_finish++;
        lte_autok_running--;
    }
    else
    {
        // send uevent
		printk(KERN_INFO "[%s] %s\n", __func__, bufferContent);
#ifndef NATIVE_AUTOK
        mtlte_sys_trigger_auto_calibration(NULL);
#else        
        wait_sdio_autok_ready((void*)lte_dev.sdio_func->card->host);
#endif        
    }

    return count;
}

static int lte_autok_readproc(struct file *file, char *buffer, size_t count, loff_t *data)
{
    return 0;
}

static const struct file_operations lte_autok_proc_ops = {
    .owner      = THIS_MODULE,
    .read       = lte_autok_readproc,
    .write      = lte_autok_writeproc,
};

static int autok_module_init(void)
{
    //s_proc = create_proc_entry(LTE_AUTOK_PROC_NAME, 0660, NULL);
    s_proc = proc_create(LTE_AUTOK_PROC_NAME, 0660, NULL, &lte_autok_proc_ops);
    
    if (s_proc == NULL) {
		remove_proc_entry(LTE_AUTOK_PROC_NAME, NULL);
		KAL_RAWPRINT(("Error: Could not initialize /proc/%s\n",
			LTE_AUTOK_PROC_NAME));
		return -ENOMEM;
	}
	
    //s_proc->write_proc = lte_autok_writeproc;
    //s_proc->read_proc = lte_autok_readproc;
    //s_proc->gid = 1000;
    proc_set_user(s_proc, 0, 1000);
    
    KAL_RAWPRINT(("/proc/%s created\n", LTE_AUTOK_PROC_NAME));
    
	return 0;	/* everything is ok */
}

static void autok_module_exit(void)
{
	//int ret;
	
    remove_proc_entry(LTE_AUTOK_PROC_NAME, NULL);
    KAL_RAWPRINT(("/proc/%s removed\n", LTE_AUTOK_PROC_NAME));
} 

unsigned int mtlte_sdio_probe_done = 0;

void mtlte_sys_sdio_wait_probe_done(void)
{
    while(0 == mtlte_sdio_probe_done){
        KAL_SLEEP_MSEC(100);
    }
}
#endif

static 
int mtlte_sys_sdio_probe( struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret ; 
	//char net_mac[6] ;
#ifdef MT_LTE_AUTO_CALIBRATION	
  #ifndef NATIVE_AUTOK
	int lte_autok_finish_next;
  #endif
	BOOTMODE btmod;
#endif

    unsigned int orig_WHCR = 0;
    unsigned int changed_WHCR = 0;
    unsigned int orig_WPLRCR = 0;
    unsigned int changed_WPLRCR = 0;
    
    KAL_RAWPRINT(("[PROBE] =======> mt_lte_sdio_probe\n")); 

#ifdef MT_LTE_AUTO_CALIBRATION
    mtlte_sdio_probe_done = 0;
#endif

    LTE_WD_timeout_indicator = 0;
    
	lte_dev.card_exist = 1 ;
	lte_dev.sdio_thread_kick = 0 ;   
    lte_dev.sdio_thread_kick_isr = 0 ; 
    lte_dev.sdio_thread_kick_own_timer = 0 ; 
	lte_dev.sdio_thread_state = SDIO_THREAD_IDLE ; 
	lte_dev.sdio_func = func ;

    /*  Because MT7208 SDIO device add the r/w busy function, 
            We only can use block read at Tx/Rx port.  
            This  quirk is force host to use block access if the transfer byte is equal to n*block size  */
//	    func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;

    /*  Because MT7208 SDIO device not support read 512 byte in data block 
            This  quirk is set byte mode size to max 511 byte  */
    func->card->quirks |= MMC_QUIRK_BROKEN_BYTE_MODE_512;

    /* enable the sdio device function */
	if ((ret = sdio_open_device(func)) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -sdio_open_device fail \n")); 
		goto OPEN_FAIL ;
	}
	
#ifdef MT_LTE_AUTO_CALIBRATION
    btmod = get_boot_mode();
    printk("btmod = %d\n", btmod);
    if ((btmod!=META_BOOT) && (btmod!=FACTORY_BOOT) && (btmod!=ATE_FACTORY_BOOT))
    {
#ifndef NATIVE_AUTOK 
        lte_autok_finish = 0;
        lte_autok_finish_next = lte_autok_finish + 1;
    //    KAL_RAWPRINT(("[AUTOK] =======> mtlte_sys_trigger_auto_calibration\n"));
        mtlte_sys_trigger_auto_calibration(NULL);
    //	KAL_RAWPRINT(("lte_autok_finish = %d\n", lte_autok_finish));
    
        while (lte_autok_finish_next != lte_autok_finish){
            KAL_SLEEP_MSEC(50);
        }
#else
      wait_sdio_autok_ready((void*)lte_dev.sdio_func->card->host);
#endif
      
    //	KAL_RAWPRINT(("lte_autok_finish = %d\n", lte_autok_finish));
    }
#endif

#ifdef MT_LTE_ONLINE_TUNE_SUPPORT
    ot_set_dev_sleep_sts((void*)lte_dev.sdio_func->card->host, 0);
    ot_dev_wakeup( (void*)lte_dev.sdio_func->card->host );
#endif


	mtlte_hif_register_hif_to_sys_wake_callback(mtlte_sys_sdio_kick_thread, 0) ;
    mtlte_hif_register_hif_to_sys_sleep_callback(mtlte_sys_sdio_sleep_thread, 0) ;
	
	if ((ret = mtlte_hif_sdio_probe()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_hif_sdio_probe fail \n")); 
		goto HIF_PROBE_FAIL ;
	}
		
	
	/* sync with the Device FW */
#if 1 
	if ((ret = mtlte_hif_sdio_wait_FW_ready()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_hif_sdio_wait_FW_ready fail \n")); 
		goto HIF_PROBE_FAIL ;
	}	
#endif

	/* do the data flow layer probing */
	if ((ret = mtlte_df_probe()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_df_probe fail \n")); 
		goto DF_PROBE_FAIL ;
	}

    if ((ret = mtlte_expt_probe()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -sdio_register_driver fail \n")); 
		goto DF_PROBE_FAIL ; 
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] set the WPLRCR for NEW FPGA... \n",KAL_FUNC_NAME)) ;
    
    /* Set the Default value of RX MAX Report Num due MT6290 SDIO HW's flexibility */
    sdio_func1_rd(SDIO_IP_WHCR, &orig_WHCR, 4);
    changed_WHCR = orig_WHCR | RPT_OWN_RX_PACKET_LEN;
    sdio_func1_wr(SDIO_IP_WHCR, &changed_WHCR, 4);

    sdio_func1_rd(SDIO_IP_WPLRCR, &orig_WPLRCR, 4);
    changed_WPLRCR = orig_WPLRCR;
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(0)));
    changed_WPLRCR |= MT_LTE_RXQ0_MAX_PKT_REPORT_NUM<<(8*0);
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(1)));
    changed_WPLRCR |= MT_LTE_RXQ1_MAX_PKT_REPORT_NUM<<(8*1);
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(2)));
    changed_WPLRCR |= MT_LTE_RXQ2_MAX_PKT_REPORT_NUM<<(8*2);
    changed_WPLRCR &= (~(RX_RPT_PKT_LEN(3)));
    changed_WPLRCR |= MT_LTE_RXQ3_MAX_PKT_REPORT_NUM<<(8*3);

    sdio_func1_wr(SDIO_IP_WPLRCR, &changed_WPLRCR, 4);	

#if EMCS_SDIO_DRVTST		
	if ((ret = mtlte_dev_test_probe(LTE_TEST_DEVICE_MINOR, &func->dev)) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_dev_test_probe %d fail \n", LTE_TEST_DEVICE_MINOR)); 
		goto PROBE_TEST_DRV_FAIL ;
	}
#endif	
    
	/* Sync with FW */
	//TODO5:
	
	/* start the kthread */
	lte_dev.sdio_thread = kthread_run(mtlte_sys_sdio_thread, &lte_dev, "mt_sdio_kthread");
    if (IS_ERR(lte_dev.sdio_thread)) {
        ret = -EBUSY ;
        KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -kthread_run fail \n")); 
        goto CREATE_THREAD_FAIL;
    }	

	/* Enable the SDIO TXRX handle process */
	mtlte_hif_sdio_txrx_proc_enable(1) ;
	KAL_RAWPRINT(("[PROBE] mtlte_hif_sdio_txrx_proc_enable enalbe it. \n")); 

    // Test the WHISR read before enable interrupt for 6575 debug
    //mtlte_hif_sdio_get_driver_own();
    //sdio_func1_rd(SDIO_IP_WHISR, hif_sdio_handler.enh_whisr_cache,sizeof(sdio_whisr_enhance));

    /* enable the sdio interrupt service */  
    if ((ret = mtlte_sys_sdio_setup_irq(func)) != KAL_SUCCESS){
		KAL_RAWPRINT(("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_sys_sdio_setup_irq fail \n")); 
        goto SETUP_IRQ_FAIL;
    }

#ifdef MT_LTE_AUTO_CALIBRATION
        mtlte_sdio_probe_done = 1;
#endif
	KAL_RAWPRINT(("[PROBE] <======= mt_lte_sdio_probe\n")); 	
	
	return ret ;

SETUP_IRQ_FAIL:
	kthread_stop(lte_dev.sdio_thread); 	
CREATE_THREAD_FAIL:

#if EMCS_SDIO_DRVTST		
	mtlte_dev_test_detach(LTE_TEST_DEVICE_MINOR) ;
PROBE_TEST_DRV_FAIL:
#endif

	mtlte_df_remove_phase1() ;
	mtlte_df_remove_phase2() ;
DF_PROBE_FAIL:
	mtlte_hif_sdio_remove_phase1() ;
	mtlte_hif_sdio_remove_phase2() ;
HIF_PROBE_FAIL:
	sdio_close_device(func) ;
OPEN_FAIL:

	KAL_RAWPRINT(("[PROBE FAIL] <======= mt_lte_sdio_probe\n")); 	
	
	return -ENODEV ;
}

static 
void mtlte_sys_sdio_remove(struct sdio_func *func)
{
	KAL_RAWPRINT(("[REMOVE] =======> mtlte_sys_sdio_remove\n")); 
	
#ifdef MT_LTE_AUTO_CALIBRATION
        mtlte_sdio_probe_done = 0;
#endif
#ifdef MT_LTE_ONLINE_TUNE_SUPPORT
    ot_set_dev_sleep_sts((void*)lte_dev.sdio_func->card->host, 1);
#endif

	
	lte_dev.card_exist = 0 ;

	mtlte_sys_sdio_remove_irq(func) ;
	KAL_RAWPRINT(("[REMOVE] sdio_release_irq Done. \n")); 

	mtlte_hif_sdio_txrx_proc_enable(0) ;
	KAL_RAWPRINT(("[REMOVE] mtlte_hif_sdio_txrx_proc_enable disalbe it. \n")); 
	
	if (!IS_ERR(lte_dev.sdio_thread)){
		kthread_stop(lte_dev.sdio_thread);
	} 
	KAL_RAWPRINT(("[REMOVE] kthread_stop OK. \n")); 
	
	/* check all the hif jobs are stopped */
	mtlte_sys_check_sdio_thread_stop() ;
	KAL_RAWPRINT(("[REMOVE] mtlte_sys_check_sdio_thread_stop Done. \n")); 


#if EMCS_SDIO_DRVTST		
	mtlte_dev_test_detach(LTE_TEST_DEVICE_MINOR) ;
#endif	
  
	mtlte_expt_remove();
    
	/* do phase 1 removing */
	mtlte_hif_sdio_remove_phase1() ;
	KAL_RAWPRINT(("[REMOVE] mtlte_hif_sdio_remove_phase1 Done. \n")); 
	mtlte_df_remove_phase1() ;
	KAL_RAWPRINT(("[REMOVE] mtlte_df_remove_phase1 Done. \n")); 

	/* do phase 2 removing */
	mtlte_hif_sdio_remove_phase2() ;
	KAL_RAWPRINT(("[REMOVE] mtlte_hif_sdio_remove_phase2 Done. \n")); 
	mtlte_df_remove_phase2();
	KAL_RAWPRINT(("[REMOVE] mtlte_df_remove_phase2 Done. \n")); 

	/* disable the SDIO interrupt and */
	sdio_close_device(func) ;
	KAL_RAWPRINT(("[REMOVE] sdio_close_device Done. \n")); 

	KAL_RAWPRINT(("[REMOVE] <======= mtlte_sys_sdio_remove\n")); 
	
    LTE_WD_timeout_indicator = 0;
    
	return ;
}


static struct sdio_driver mtlte_driver = {
    .name		= MT_LTE_SDIO_KBUILD_MODNAME ,
    .id_table	= mt_lte_sdio_id_table,
    .probe		= mtlte_sys_sdio_probe,
    .remove		= mtlte_sys_sdio_remove,
};


//static
//KAL_INT32 __init mtlte_sys_sdio_driver_init(void)
int mtlte_sys_sdio_driver_init(void)
{	
	int ret = KAL_SUCCESS ;
	
    KAL_RAWPRINT(("[INIT] =======> lte_sdio_driver_init\n")); 	

	lte_dev.card_exist = 0 ;
	lte_dev.sdio_func = NULL;
    /* init thread related parameters */
    init_waitqueue_head(&lte_dev.sdio_thread_wq);        

#ifdef MT_LTE_AUTO_CALIBRATION    
    autok_module_init();
#endif
	
	/* init the hif layer */
    if ((ret = mtlte_hif_sdio_init()) != KAL_SUCCESS){
    	KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -mtlte_hif_sdio_init fail \n")); 
		goto HIF_INITFAIL ; 
    }
	/* init the data flow layer */
    if ((ret = mtlte_df_init()) != KAL_SUCCESS){
       	KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -mtlte_df_init fail \n")); 
		goto DF_INITFAIL ; 
    }

    if ((ret = mtlte_expt_init()) != KAL_SUCCESS){
       	KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -mtlte_expt_init fail \n")); 
		goto DF_INITFAIL ; 
    }

#if EMCS_SDIO_DRVTST	
	if ((ret = mtlte_dev_test_drvinit()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -mtlte_dev_test_drvinit fail \n")); 
		goto TEST_DRV_INITFAIL ; 
    }
#endif	


    if ((ret = sdio_register_driver(&mtlte_driver)) != KAL_SUCCESS){
		KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init -sdio_register_driver fail \n")); 
		goto SDIO_REG_FAIL ; 
    }

    KAL_AQUIREMUTEX(&lte_dev.thread_kick_lock) ;

    if ((ret = sdio_onoff_module_init()) != KAL_SUCCESS){
		KAL_RAWPRINT(("[INIT] XXXXXX lte_sdio_driver_init - onoff_char_dev register fail \n")); 
		goto ONOFF_DEV_FAIL ; 
    }
    
	KAL_RAWPRINT(("[INIT] <======= lte_sdio_driver_init\n")); 	
    return ret ;

ONOFF_DEV_FAIL:
    sdio_unregister_driver(&mtlte_driver);
SDIO_REG_FAIL :      	
#if EMCS_SDIO_DRVTST	
	mtlte_dev_test_drvdeinit() ;
TEST_DRV_INITFAIL :
#endif	

	mtlte_df_deinit() ;
DF_INITFAIL :    
	mtlte_hif_sdio_deinit() ;
HIF_INITFAIL :	

	KAL_RAWPRINT(("[INIT FAIL] <======= lte_sdio_driver_init\n")); 	
	return ret ;   
}

//static
//void __exit mtlte_sys_sdio_driver_exit(void)
void mtlte_sys_sdio_driver_exit(void)
{
    KAL_RAWPRINT(("[EXIT] =======> lte_sdio_driver_exit\n"));

    sdio_onoff_module_exit();

    KAL_DESTROYMUTEX(&lte_dev.thread_kick_lock) ;

    sdio_unregister_driver(&mtlte_driver);
    KAL_RAWPRINT(("[EXIT] sdio_unregister_driver OK. \n"));       


#if EMCS_SDIO_DRVTST		
    mtlte_dev_test_drvdeinit() ;    
    KAL_RAWPRINT(("[EXIT] mtlte_dev_test_drvdeinit OK. \n")); 
#endif   

    /* denit the expt layer */
    mtlte_expt_deinit() ;
    KAL_RAWPRINT(("[EXIT] mtlte_expt_deinit OK. \n")); 

    /* denit the data flow layer */
    mtlte_df_deinit() ;
    KAL_RAWPRINT(("[EXIT] mtlte_df_deinit OK. \n")); 

    /* denit the hif layer */
    mtlte_hif_sdio_deinit() ;
    KAL_RAWPRINT(("[EXIT] mtlte_hif_sdio_deinit OK. \n")); 

#ifdef MT_LTE_AUTO_CALIBRATION        
    autok_module_exit();
#endif

    KAL_RAWPRINT(("[EXIT] <======= lte_sdio_driver_exit\n"));
}


void mtlte_sys_sdio_driver_init_after_phase2(void)
{
    #if USING_WAKE_MD_EINT
    mtlte_hif_sdio_enable_fw_own_back(1);
    #endif
}

/*
module_init(mtlte_sys_sdio_driver_init);
module_exit(mtlte_sys_sdio_driver_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek MT72X8 LTE OS glue for SDIO");
MODULE_LICENSE("Dual BSD/GPL");
*/

//#if !UNIT_TEST  
//DULE_FIRMWARE(MT72X8S_FW_FILE_NAME);
//#endif
