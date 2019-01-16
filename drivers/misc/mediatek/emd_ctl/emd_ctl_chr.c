#include <linux/sched.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/mutex.h>
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
#include <linux/aee.h>
#include <linux/slab.h>
#include <emd_ctl.h>
#include <mach/mtk_ccci_helper.h>
/******************************************************************************************
 *   Macro definition region
 ******************************************************************************************/
#define EMD_DEV_NAME            "ext-md-ctl"
#define EMD_DEV_NODE_NAME        "ext_md_ctl"

#define CCCI_IOC_MAGIC 'C'

#define CCCI_IOC_MD_RESET                _IO(CCCI_IOC_MAGIC, 0) // mdlogger // META // muxreport
#define CCCI_IOC_GET_MD_STATE            _IOR(CCCI_IOC_MAGIC, 1, unsigned int) // audio
#define CCCI_IOC_DO_STOP_MD                _IO(CCCI_IOC_MAGIC, 12) // md_init
#define CCCI_IOC_DO_START_MD            _IO(CCCI_IOC_MAGIC, 13) // md_init
#define CCCI_IOC_ENTER_DEEP_FLIGHT        _IO(CCCI_IOC_MAGIC, 14) // RILD // factory
#define CCCI_IOC_LEAVE_DEEP_FLIGHT        _IO(CCCI_IOC_MAGIC, 15) // RILD // factory
#define CCCI_IOC_POWER_ON_MD            _IO(CCCI_IOC_MAGIC, 16) // md_init
#define CCCI_IOC_POWER_OFF_MD            _IO(CCCI_IOC_MAGIC, 17) // md_init
#define CCCI_IOC_SIM_SWITCH                _IOW(CCCI_IOC_MAGIC, 20, unsigned int) // RILD // factory
#define CCCI_IOC_SIM_SWITCH_TYPE        _IOR(CCCI_IOC_MAGIC, 22, unsigned int) // RILD
#define CCCI_IOC_STORE_SIM_MODE            _IOW(CCCI_IOC_MAGIC, 23, unsigned int) // RILD
#define CCCI_IOC_GET_SIM_MODE            _IOR(CCCI_IOC_MAGIC, 24, unsigned int) // RILD
#define CCCI_IOC_GET_MD_PROTOCOL_TYPE	   _IOR(CCCI_IOC_MAGIC, 42, char[16]) /*metal tool to get modem protocol type: AP_TST or DHL*/
#define CCCI_IOC_IGNORE_MD_EXCP            _IO(CCCI_IOC_MAGIC, 42) // RILD

#define CCCI_IOC_GET_MD_ASSERTLOG       _IOW(CCCI_IOC_MAGIC, 200, unsigned int)    /*Block to get extern md assert flag for mdlogger */
#define CCCI_IOC_GET_MD_ASSERTLOG_STATUS  _IOW(CCCI_IOC_MAGIC, 201, unsigned int)    /*get extern md assert log status*/
#define CCCI_IOC_ENTER_MD_DL_MODE         _IOW(CCCI_IOC_MAGIC, 202, unsigned int)    /*Enter dwonload extern md img mode*/

#define EMD_ERR_UN_DEF_CMD         (2)

#define EMD_MAX_MESSAGE_NUM        (16)
#define EMD_PWR_KEY_LOW_TIME    (5000)
#define EMD_RST_LOW_TIME        (300)
#define MAX_CHK_RDY_TIMES       (50)
#define EMD_RESET_USER_NAME_SIZE  (16)
#define NR_EMD_RESET_USER (5)
/* For ext MD Message, this is for user space deamon use */
enum {
    EMD_MSG_READY = 0xF0A50000,
    EMD_MSG_REQUEST_RST,
    EMD_MSG_WAIT_DONE,
    EMD_MSG_ENTER_FLIGHT_MODE,
    EMD_MSG_LEAVE_FLIGHT_MODE,
};
enum{
    EMD_STATE_NOT_READY=0,    
    EMD_STATE_READY=2,
    EMD_STATE_EXCEPTION=3,
};
/******************************************************************************************
 *   Data structure definition region
 ******************************************************************************************/
typedef struct _emd_dev_client{
    struct kfifo        fifo;
    int                 major_dev_id;
    int                 sub_dev_id;
    int                 user_num;
    spinlock_t          lock;
    wait_queue_head_t   wait_q;
    struct mutex        emd_mutex;
}emd_dev_client_t;
struct emd_reset_sta
{
    int is_allocate;
    int is_reset;
    char name[EMD_RESET_USER_NAME_SIZE];
};
/******************************************************************************************
 *   Gloabal variables region
 ******************************************************************************************/ 
static struct cdev        *emd_chr_dev;
static emd_dev_client_t    drv_client[EMD_CHR_CLIENT_NUM];
static atomic_t            rst_on_going = ATOMIC_INIT(0);
static unsigned int        emd_status=EMD_STATE_NOT_READY;
static unsigned int           emd_aseert_log_wait_timeout = 0;

static void emd_aseert_log_work_func(struct work_struct *data);
static DECLARE_WAIT_QUEUE_HEAD(emd_aseert_log_wait);
static DEFINE_MUTEX(emd_aseert_log_lock);
static DECLARE_WORK(emd_aseert_log_work, emd_aseert_log_work_func);
static struct mutex     emd_reset_mutex;
static struct emd_reset_sta emd_reset_sta[NR_EMD_RESET_USER];
/******************************************************************************************
 *   External customization functions region
 ******************************************************************************************/
#if defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT)
bool usb_h_acm_all_clear(void);
#else
static char usb_h_acm_all_clear(void)
{
	EMD_MSG_INF("chr","TODO: Dummy usb_h_acm_all_clear!\n");
	return 1;
}
#endif

/******************************************************************************************
 *   Helper functions region
 ******************************************************************************************/
static int request_ext_md_reset(void); // Function declaration
static int send_message_to_user(emd_dev_client_t *client, int msg);
static void emd_power_off(void);
void check_drv_rdy_to_rst(void)
{
    int check_count = 0; // Max 10 seconds
    while(check_count<MAX_CHK_RDY_TIMES){
        if(usb_h_acm_all_clear())
            return;
        msleep(200);
        check_count++;
    }
    EMD_MSG_INF("chr","Wait drv rdy to rst timeout!!\n");
}

// emd_reset_register: register a user for emd reset
// @name: user name
// return a handle if success; return negative value if failure
int emd_reset_register(char *name)
{
    int handle;
    if (name == NULL) {
        EMD_MSG_INF("chr","[Error]emd_reset_register name=null\n");
        return -1;
    }
    mutex_lock(&emd_reset_mutex);
    for (handle = 0; handle < NR_EMD_RESET_USER; handle++) {
        if (emd_reset_sta[handle].is_allocate == 0) {
            emd_reset_sta[handle].is_allocate = 1;
            break;
        }
    }
    if (handle < NR_EMD_RESET_USER) {
        emd_reset_sta[handle].is_reset = 0;
        mutex_unlock(&emd_reset_mutex);
        snprintf(emd_reset_sta[handle].name,EMD_RESET_USER_NAME_SIZE,name);
        EMD_MSG_INF("chr","Register a reset handle by %s(%d)\n", current->comm, handle);
        return handle;
    } 
    else 
    {
        EMD_MSG_INF("chr","[Error]no reset handler\n");
        mutex_unlock(&emd_reset_mutex);
        return -1;
    }
}



// ccci_user_ready_to_reset: ready to reset and request to reset md
// @handle: a user handle gotten from ccci_reset_register()
// return 0 if emd is reset; return negative value for failure
int emd_user_ready_to_reset(int handle)
{
    int i,ret;
    int reset_ready = 1;
    if( 0==atomic_read(&rst_on_going) ){
        EMD_MSG_INF("chr", "Ignore reset request\n");
        mutex_lock(&emd_reset_mutex);
        emd_reset_sta[handle].is_allocate = 0;
        emd_reset_sta[handle].is_reset = 0;
        mutex_unlock(&emd_reset_mutex);
        return 0;
    }
    if (handle >= NR_EMD_RESET_USER) {
        EMD_MSG_INF("chr", "reset_request: invalid handle:%d \n", handle);
        return -1;
    }

    if (emd_reset_sta[handle].is_allocate == 0) {
        EMD_MSG_INF("chr", "reset_request: handle(%d) not alloc: alloc=%d \n", 
                handle, emd_reset_sta[handle].is_allocate);
        return -1;
    } 
    EMD_MSG_INF("chr", "%s (%d) call reset request \n",current->comm, handle);

    mutex_lock(&emd_reset_mutex);
    emd_reset_sta[handle].is_allocate = 0;
    emd_reset_sta[handle].is_reset = 1;
    EMD_MSG_INF("chr", "Dump not ready list++++\n");
    for (i = 0; i < NR_EMD_RESET_USER; i++) {
        if (emd_reset_sta[i].is_allocate && (emd_reset_sta[i].is_reset == 0)) {
            reset_ready = 0;
            EMD_MSG_INF("chr", " ==> %s\n", emd_reset_sta[i].name);
        }
    }
    EMD_MSG_INF("chr", "Dump not ready list----\n");
    mutex_unlock(&emd_reset_mutex);

    if (reset_ready == 0) 
        return -1;

    // All service ready, send reset request
    EMD_MSG_INF("chr", "Reset MD by %s(%d) \n", current->comm, handle);
    if(emd_status != EMD_STATE_NOT_READY)
    {        
        emd_power_off();
    }
    msleep(EMD_RST_LOW_TIME);
    check_drv_rdy_to_rst();
    EMD_MSG_INF("chr","send wait done message\n");
    ret = send_message_to_user(&drv_client[0], EMD_MSG_WAIT_DONE);
    if( ret!=0 )
    EMD_MSG_INF("chr","send wait done message fail\n");

    return 0;
}




static int get_curr_emd_state(void)
{
    return emd_status;
}

static int enter_md_download_mode(void)
{
    return cm_enter_md_download_mode();
}
static int let_ext_md_go(void) 
{
    int ret=0;
    int retry;

    ret = send_message_to_user(&drv_client[0], EMD_MSG_READY);
    atomic_set(&rst_on_going, 0);
  
    if( ret==0 ){
        retry = cm_do_md_go();
        EMD_MSG_INF("chr","cm_do_md_go, ret=%d\n", retry);
        emd_status = EMD_STATE_READY;
    }else{
        EMD_MSG_INF("chr","let_ext_md_go fail, msg does not send\n");
        ret = -1;
    }
    return ret;
}

int request_ext_md_reset()
{
    int ret = 0;

    if(atomic_add_return(1, &rst_on_going) == 1){
        ret = send_message_to_user(&drv_client[0], EMD_MSG_REQUEST_RST);
        if(ret!=0){
            EMD_MSG_INF("chr","request_ext_md_reset fail, msg does not send\n");
            atomic_dec(&rst_on_going);
        }
    }else{
        EMD_MSG_INF("chr","reset is on-going\n");
    }
    return ret;
}
#if defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT)
#ifdef CONFIG_PM_RUNTIME
void usb11_auto_resume(void);
#endif
#endif
static void emd_power_off(void)
{
    EMD_MSG_INF("chr","emd_power_off\n");
#if defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT)
#ifdef CONFIG_PM_RUNTIME
	/* make sure usb device tree is waked up so that usb is ready */
	usb11_auto_resume();
#endif
#endif
    cm_do_md_power_off();
    emd_status = EMD_STATE_NOT_READY;
}

static int emd_power_on(int bootmode)
{
	static int irq_registered = 0;
    EMD_MSG_INF("chr","emd_power_on, bootmode=%d\n",bootmode);
#if defined(CONFIG_MTK_DT_SUPPORT) && !defined(CONFIG_EVDO_DT_SUPPORT)
#ifdef CONFIG_PM_RUNTIME
	/* make sure usb device tree is waked up so that usb is ready */
	usb11_auto_resume();
#endif
#endif
    cm_do_md_power_on(bootmode);

    EMD_MSG_INF("chr","let_ext_md_go...\n");
    let_ext_md_go();

	if(!irq_registered) {
        irq_registered = 1;
    	cm_register_irq_cb(0,ext_md_wdt_irq_cb);    
    	cm_register_irq_cb(1,ext_md_wakeup_irq_cb);
    	cm_register_irq_cb(2,ext_md_exception_irq_cb);
	}

    return 0;
}

static int push_data(emd_dev_client_t *client, int data)
{
    int size, ret;
    if(kfifo_is_full(&client->fifo)){
        ret=-ENOMEM;
        EMD_MSG_INF("chr","sub_dev%d kfifo full\n",client->sub_dev_id);
    }else{
        EMD_MSG_INF("chr","push data=0x%08x into sub_dev%d kfifo\n",data,client->sub_dev_id);
        size=kfifo_in(&client->fifo,&data,sizeof(int));
        WARN_ON(size!=sizeof(int));
        ret=sizeof(int);
    }
    return ret;
}

static int pop_data(emd_dev_client_t *client, int *buf)
{
    int ret = 0;

    if(!kfifo_is_empty(&client->fifo))
    {
        ret = kfifo_out(&client->fifo, buf, sizeof(int));
        EMD_MSG_INF("chr","pop data=0x%08x from sub_dev%d kfifo.\n",*buf,client->sub_dev_id);
    }

    return ret;
}

int send_message_to_user(emd_dev_client_t *client, int msg)
{
    int ret = 0;
    unsigned long flags = 0;

    spin_lock_irqsave(&client->lock, flags);

    // 1. Push data to fifo
    ret = push_data(client, msg);

    // 2. Wake up read function
    if( sizeof(int) == ret ){
        wake_up_interruptible(&client->wait_q);
        ret = 0;
    }
    else
    {
        EMD_MSG_INF("chr","send_message_to_user,push_data ret=%d!!\n",ret);
    }

    spin_unlock_irqrestore(&client->lock, flags);

    return ret;
}

static int client_init(emd_dev_client_t *client, int major,int sub_id)
{
    int ret = 0;
    if( (sub_id >= EMD_CHR_CLIENT_NUM) || (sub_id < 0) ){
        EMD_MSG_INF("chr","client_init:sub_id(%d) error\n",sub_id);
        return -1;
    }

    // 1. Clear client
    memset(client, 0, sizeof(emd_dev_client_t));

    // 2. Setting device id
    client->major_dev_id = major;
    client->sub_dev_id = sub_id;

    // 3. Init wait queue head, wake lock, spin loc and semaphore
    init_waitqueue_head(&client->wait_q);

    spin_lock_init(&client->lock);
    mutex_init(&client->emd_mutex);

    // 4. Set user_num to zero
    client->user_num = 0;

    // 5. Alloc and init kfifo
    ret=kfifo_alloc(&client->fifo, EMD_MAX_MESSAGE_NUM*sizeof(int),GFP_ATOMIC);
    if (ret){
        EMD_MSG_INF("chr","kfifo alloc failed(ret=%d).\n",ret);

        return ret;
    }
    EMD_MSG_INF("chr","client_init:sub_id=%d\n",client->sub_dev_id); 

    return 0;
}

static int client_deinit(emd_dev_client_t *client)
{
    kfifo_free(&client->fifo);
    EMD_MSG_INF("chr","client_deinit:sub_id=%d\n",client->sub_dev_id);    
    return 0;
}

static void emd_aseert_log_work_func(struct work_struct *data)
{
#if defined (CONFIG_MTK_AEE_FEATURE)
    char log[]="ExtMD exception\nMD:G*MT6261_S01*11C.unknown*0000/00/00 00:00\nAP:WG*MT6595_S00\n(MD)Debug";
    EMD_MSG_INF("chr","Ext MD exception,%s\n",log);
    emd_aseert_log_wait_timeout = 1;
    wake_up_interruptible(&emd_aseert_log_wait);
    aed_md_exception((int *)log, sizeof(log), (int *)log, sizeof(log), log);
#else
    EMD_MSG_INF("chr","Ext MD ASSERT -> RESET\n");
    emd_aseert_log_wait_timeout = 1;
    wake_up_interruptible(&emd_aseert_log_wait);
    emd_request_reset();    
#endif    
}


static int emd_send_enter_flight_mode(void)
{
    int ret=0;
    if(emd_status!=EMD_STATE_READY)
    {
        EMD_MSG_INF("chr","emd_send_enter_flight_mode:ext md not ready!\n");
        return -ENODEV;
    }
    EMD_MSG_INF("chr","emd_send_enter_flight_mode\n");
    emd_power_off();
    send_message_to_user(&drv_client[0], EMD_MSG_ENTER_FLIGHT_MODE);
    return ret;
}

static int emd_send_leave_flight_mode(void)
{
    int ret=0;
    if(emd_status==EMD_STATE_READY)
    {
        EMD_MSG_INF("chr","emd_send_leave_flight_mode:ext md is ready,cannot leave flight mode!\n");
        return -EPERM;
    }    
    if(atomic_add_return(1, &rst_on_going) == 1){
        ret = send_message_to_user(&drv_client[0], EMD_MSG_LEAVE_FLIGHT_MODE);
        if(ret!=0){
            EMD_MSG_INF("chr","emd_send_leave_flight_mode fail, msg does not send\n");
            atomic_dec(&rst_on_going);
        }
    }else{
        EMD_MSG_INF("chr","emd_send_leave_flight_mode:reset is on-going\n");
    }    

    return ret;
}

/******************************************************************************************
 *   Driver functions region
 ******************************************************************************************/ 
static int emd_dev_open(struct inode *inode, struct file *file)
{
    int index=iminor(inode);
    int ret=0;

    EMD_MSG_INF("chr","Open by %s sub_id:%d\n",current->comm,index);

    if( (index >= EMD_CHR_CLIENT_NUM) || (index < 0) ){
        EMD_MSG_INF("chr","Open func get invalid dev sub id\n");
        return -1;
    }

    mutex_lock(&drv_client[index].emd_mutex);
    if(drv_client[index].user_num > 0){
        EMD_MSG_INF("chr","Multi-Open not support!\n");
        mutex_unlock(&drv_client[index].emd_mutex);
        return -1;
    }
    drv_client[index].user_num++;
    mutex_unlock(&drv_client[index].emd_mutex);

    file->private_data=&drv_client[index];    
    nonseekable_open(inode,file);
    return ret;
}

static int emd_dev_release(struct inode *inode, struct file *file)
{
    int ret=0;
    emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;

    EMD_MSG_INF("chr","client %d call release\n", client->sub_dev_id);
    mutex_lock(&client->emd_mutex);
    client->user_num--;
    mutex_unlock(&client->emd_mutex);

    return 0;
}

static ssize_t emd_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;
    int ret=0,size, data;

    WARN_ON(client==NULL);
    do{
        size=pop_data(client,&data);
        if( sizeof(int) == size){
            EMD_MSG_INF("chr","client%d get data:%08x, ret=%d\n", client->sub_dev_id, data, ret);
            if( copy_to_user(buf,&data,sizeof(int)) ){
                EMD_MSG_INF("chr","copy_to_user fialed\n");
                ret = -EFAULT;
                goto _OUT;
            }else{
                ret = size;
                goto _OUT;
            }
        }else{
            if (file->f_flags & O_NONBLOCK){
                ret = -EAGAIN;
                goto _OUT; 
            }else{
                ret = wait_event_interruptible(client->wait_q, !kfifo_is_empty(&client->fifo));
                if(ret == -ERESTARTSYS) {
                    EMD_MSG_INF("chr","Interrupted syscall.signal_pend=0x%llx\n",
                        *(long long *)current->pending.signal.sig);
                    ret = -EINTR;
                    goto _OUT;
                }
            }
        }
    }while(ret==0);
_OUT:
    return ret;
}

static long emd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    unsigned int sim_mode=0, boot_mode=0;
    int value;
    emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;
    switch (cmd) {
		case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
		{
			char md_protol[] = "AP_TST";
			unsigned int data_size = sizeof(md_protol) / sizeof(char);

			EMD_MSG_INF("chr","Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");


            if (copy_to_user((void __user *)arg, md_protol, data_size)) 
			{
				EMD_MSG_INF("chr","copy_to_user MD_PROTOCOL failed !!\n");

                return -EFAULT;
			}
			
			break;
		}

    case CCCI_IOC_GET_MD_STATE:
        EMD_MSG_INF("chr", "Get md state ioctl called by %s\n", current->comm);
        if(arg!=0)
        {
            value = get_curr_emd_state();
            value+='0'; // Make number to charactor
            ret = put_user((unsigned int)value, (unsigned int __user *)arg);
        }else{
            EMD_MSG_INF("chr", "Get md state ioctl: arg is null\n");
        }
        break;
    case CCCI_IOC_DO_START_MD:
        EMD_MSG_INF("chr", "Start md ioctl called by %s\n", current->comm);
        if(copy_from_user(&boot_mode, (void __user *)arg, sizeof(unsigned int))) {
            EMD_MSG_INF("chr", "CCCI_IOC_DO_START_MD: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            ret = emd_power_on(boot_mode);
            EMD_MSG_INF("chr", "CCCI_IOC_DO_START_MD,%d\n",boot_mode);
        }
        break;
    case CCCI_IOC_ENTER_MD_DL_MODE:        
        EMD_MSG_INF("chr", "Enter md download md ioctl called by %s\n", current->comm);
        ret = enter_md_download_mode();
        break;
    case CCCI_IOC_MD_RESET:
        EMD_MSG_INF("chr", "Reset on md ioctl called by %s\n", current->comm);
        ret = emd_request_reset();
        break;
    
    case CCCI_IOC_POWER_ON_MD:
        EMD_MSG_INF("chr", "Power on md ioctl called by %s\n", current->comm);
        if(copy_from_user(&boot_mode, (void __user *)arg, sizeof(unsigned int))) {
            EMD_MSG_INF("chr", "CCCI_IOC_POWER_ON_MD: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            ret = emd_power_on(boot_mode);
            EMD_MSG_INF("chr", "CCCI_IOC_POWER_ON_MD(%x): %d\n", boot_mode, ret);  
        }
        break;
    case CCCI_IOC_ENTER_DEEP_FLIGHT:
        EMD_MSG_INF("chr", "Enter MD flight mode ioctl called by %s\n", current->comm);
        ret = emd_send_enter_flight_mode();
        break;

    case CCCI_IOC_LEAVE_DEEP_FLIGHT:
        EMD_MSG_INF("chr","Leave MD flight mode ioctl called by %s\n", current->comm);
        ret = emd_send_leave_flight_mode();
        break;
    case CCCI_IOC_GET_MD_ASSERTLOG:
        emd_aseert_log_wait_timeout=0;
        EMD_MSG_INF("chr","CCCI_IOC_GET_MD_ASSERTLOG ioctl called by %s, sub_dev=%d\n", current->comm,client->sub_dev_id);
        if(wait_event_interruptible(emd_aseert_log_wait, emd_aseert_log_wait_timeout) == -ERESTARTSYS)
            ret = -EINTR;
        else {
            EMD_MSG_INF("chr","sub_dev=%d call CCCI_IOC_GET_MD_ASSERTLOG is exit\n", client->sub_dev_id);
            emd_aseert_log_wait_timeout = 0;
        }
        break;
    case CCCI_IOC_GET_MD_ASSERTLOG_STATUS:
        EMD_MSG_INF("chr","CCCI_IOC_GET_MD_ASSERTLOG_STATUS ioctl called by %s, sub_dev=%d\n", current->comm,client->sub_dev_id);
        value = cm_get_assertlog_status();
        if (copy_to_user((int *)arg, &value, sizeof(int)))
        {                 
          return -EACCES;
        }           
        EMD_MSG_INF("chr", "CCCI_IOC_GET_MD_ASSERTLOG_STATUS %d\n", value);
        break;
        
    case CCCI_IOC_SIM_SWITCH:
        if(copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
            EMD_MSG_INF("chr", "IOC_SIM_SWITCH: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            ret = switch_sim_mode(0,(char*)&sim_mode,sizeof(unsigned int));//switch_sim_mode(sim_mode);
            EMD_MSG_INF("chr", "IOC_SIM_SWITCH(%x): %d\n", sim_mode, ret);
        }
        break;        
    case CCCI_IOC_SIM_SWITCH_TYPE:
        value = get_sim_switch_type();
        ret = put_user(value, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_STORE_SIM_MODE:
        EMD_MSG_INF("chr","store sim mode ioctl called by %s!\n",  current->comm);
        if(copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
            EMD_MSG_INF("chr","store sim mode fail: copy_from_user fail!\n");
            ret = -EFAULT;
        }
        else 
        {
            EMD_MSG_INF("chr","storing sim mode(%d) in kernel space!\n", sim_mode);
            exec_ccci_kern_func_by_md_id(0, ID_STORE_SIM_SWITCH_MODE, (char *)&sim_mode, sizeof(unsigned int));
        }
        break;
    
    case CCCI_IOC_GET_SIM_MODE:
        EMD_MSG_INF("chr", "get sim mode ioctl called by %s\n", current->comm);
        exec_ccci_kern_func_by_md_id(0, ID_GET_SIM_SWITCH_MODE, (char *)&sim_mode, sizeof(unsigned int));
        ret = put_user(sim_mode, (unsigned int __user *)arg);
        break;

	case CCCI_IOC_IGNORE_MD_EXCP:
        EMD_MSG_INF("chr", "ignore md excp ioctl called by %s\n", current->comm);
        cm_disable_ext_md_wdt_irq();
	    cm_disable_ext_md_wakeup_irq();
    	cm_disable_ext_md_exp_irq();
        break;

    default:
        ret = -EMD_ERR_UN_DEF_CMD;
        EMD_MSG_INF("chr","undefined ioctl called by %s\n", current->comm);
        break;
    }

    return ret;
}

static int emd_ctl_drv_probe(struct platform_device *device)
{
    EMD_MSG_INF("chr","client %d probe!!\n", device->id);
    return 0;
}

static int emd_ctl_drv_remove(struct platform_device *dev)
{
    EMD_MSG_INF("chr","remove!!\n" );
    return 0;
}

static void emd_ctl_drv_shutdown(struct platform_device *dev)
{
    EMD_MSG_INF("chr","shutdown!!\n" );
}

static int emd_ctl_drv_suspend(struct platform_device *dev, pm_message_t state)
{
    EMD_MSG_INF("chr","client:%d suspend!!\n", dev->id);
    if( (dev->id < 0)||(dev->id >= EMD_CHR_CLIENT_NUM) )
        EMD_MSG_INF("chr","invalid id%d!!\n", dev->id);
    else{
        if(0 == dev->id){
            emd_spm_suspend(emd_status==EMD_STATE_NOT_READY);
        }
    }
    return 0;
}

static int emd_ctl_drv_resume(struct platform_device *dev)
{
    EMD_MSG_INF("chr","client:%d resume!!\n", dev->id);
    if( (dev->id < 0)||(dev->id >= EMD_CHR_CLIENT_NUM) )
        EMD_MSG_INF("chr","invalid id%d!!\n", dev->id);
    else{
        if(0 == dev->id){
            emd_spm_resume();
        }
    }

    return 0;
}


static struct file_operations emd_chr_dev_fops=
{
    .owner          = THIS_MODULE,
    .open           = emd_dev_open,
    .read           = emd_dev_read,
    .release        = emd_dev_release,
    .unlocked_ioctl = emd_dev_ioctl,

};

static struct platform_driver emd_ctl_driver =
{
    .driver    = {
    .name      = EMD_DEV_NAME,
        },
    .probe     = emd_ctl_drv_probe,
    .remove    = emd_ctl_drv_remove,
    .shutdown  = emd_ctl_drv_shutdown,
    .suspend   = emd_ctl_drv_suspend,
    .resume    = emd_ctl_drv_resume,
};
int emd_request_reset(void)
{
    EMD_MSG_INF("chr","Ext MD request MD reboot!\n");
    request_ext_md_reset();
}

#if defined(MTK_DT_SUPPORT) && !defined(EVDO_DT_SUPPORT)
#ifdef	CONFIG_PM_RUNTIME
void issue_usb11_keep_resume_work(void);
#endif
#endif
int emd_md_exception(void)
{
	/* keep usb is awaken becoz ext MD will lose remote wakeup ability (becomes single-thread mode)when EE happens */

#if defined(MTK_DT_SUPPORT) && !defined(EVDO_DT_SUPPORT)
#ifdef	CONFIG_PM_RUNTIME
	issue_usb11_keep_resume_work();
#endif
#endif

    emd_status=EMD_STATE_EXCEPTION;
    EMD_MSG_INF("chr","emd_md_exception happened!\n");
    schedule_work(&emd_aseert_log_work);
    return 0;

}
int emd_chr_init(int md_id)
{
    int ret=0, i;
    int major,minor;
    mutex_init(&emd_reset_mutex);

    EMD_MSG_INF("chr","ver: 20111111\n");
    ret = emd_get_dev_id_by_md_id(md_id, "chr", &major, &minor);
    if(ret < 0)
    {
        EMD_MSG_INF("chr","emd_get_dev_id_by_md_id(chr) ret=%d fail\n",ret);
        goto _ERR;
    }
    /* 1. Register device region. 0~2 */
    if (register_chrdev_region(MKDEV(major,minor),EMD_CHR_CLIENT_NUM,EMD_DEV_NAME) != 0)
    {
        EMD_MSG_INF("chr","Regsiter EMD_CHR_DEV failed\n");
        ret=-1;
        goto _ERR;
    }

    /* 2. Alloc charactor devices */
    emd_chr_dev=cdev_alloc();
    if (NULL == emd_chr_dev)
    {
        EMD_MSG_INF("chr","cdev_alloc failed\n");
        ret=-1;
        goto cdev_alloc_fail;
    }

    /* 3. Initialize and add devices */
    cdev_init(emd_chr_dev,&emd_chr_dev_fops);
    emd_chr_dev->owner=THIS_MODULE;
    ret = cdev_add(emd_chr_dev,MKDEV(major,minor),EMD_CHR_CLIENT_NUM);
    if (ret){
        EMD_MSG_INF("chr","cdev_add failed\n");
        goto cdev_add_fail;
    }

    /* 4. Register platform driver */
    ret = platform_driver_register(&emd_ctl_driver);
    if(ret){    
        EMD_MSG_INF("chr","platform_driver_register failed\n");
        goto platform_driver_register_fail;
    }

    /* 5. Init driver client */
    for(i=0; i<EMD_CHR_CLIENT_NUM; i++){
        if( 0!=client_init(&drv_client[i],major, i) ){
            EMD_MSG_INF("chr","driver client init failed\n");
            goto driver_client_init_fail;
        }
    }
    return 0;

driver_client_init_fail:
platform_driver_register_fail:

cdev_add_fail:
    cdev_del(emd_chr_dev);

cdev_alloc_fail:
    unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CHR_CLIENT_NUM);
_ERR:    
    return ret;
}

void emd_chr_exit(int md_id)
{
    unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CHR_CLIENT_NUM);
    cdev_del(emd_chr_dev);
    client_deinit(&drv_client[0]);
    client_deinit(&drv_client[1]);
}

