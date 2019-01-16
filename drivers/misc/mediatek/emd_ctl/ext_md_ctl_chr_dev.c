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
#include <cust_eint.h>
#include <mach/eint.h>

/******************************************************************************************
 *   Macro definition region
 ******************************************************************************************/

#define DEVICE0_ID			(0x0)
#define USER0_NAME			"USB-Host"
#define DEVICE1_ID			(0x1)
#define USER1_NAME			"UART"

#define EMD_CHR_DEV_MAJOR		(167) // Note, may need to change
#define EMD_DEV_NAME			"ext-md-ctl"
#define EMD_DEV_NODE_NAME		"ext_md_ctl"

#define EXT_MD_IOC_MAGIC		'E'
#define EXT_MD_IOCTL_LET_MD_GO		_IO(EXT_MD_IOC_MAGIC, 1)
#define EXT_MD_IOCTL_REQUEST_RESET	_IO(EXT_MD_IOC_MAGIC, 2)
#define EXT_MD_IOCTL_POWER_ON_HOLD	_IO(EXT_MD_IOC_MAGIC, 3)

#define EMD_ERR_ACCESS_DENY		(1)
#define EMD_ERR_UN_DEF_CMD		(2)

#define EMD_CLIENT_NUM			(3)
#define EMD_MAX_MESSAGE_NUM		(16)
#define WAKEUP_EMD_USER_NUM		(2)

#define EMD_PWR_KEY_LOW_TIME		(5000)
#define EMD_RST_LOW_TIME		(300)
#define MAX_CHK_RDY_TIMES		(50)

/* For ext MD Message, this is for user space deamon use */
enum {
	EXT_MD_MSG_READY = 0xF0A50000,
	EXT_MD_MSG_REQUEST_RST,
	EXT_MD_MSG_WAIT_DONE,
};

/******************************************************************************************
 *   Data structure definition region
 ******************************************************************************************/
typedef struct _emd_dev_client{
	struct kfifo		fifo;
	int			major_dev_id;
	int			sub_dev_id;
	int			user_num;
	spinlock_t		lock;
	wait_queue_head_t 	wait_q;
	struct wake_lock	emd_wake_lock;
	struct mutex		emd_mutex;
}emd_dev_client_t;

typedef struct _wakeup_ctl_info{
	int	dev_major_id;
	int	dev_sub_id;
	int	time_out_req_count;
	int	manual_req_count;
	char	*name;
}wakeup_ctl_info_t;

/******************************************************************************************
 *   Gloabal variables region
 ******************************************************************************************/ 
static struct cdev		*emd_chr_dev;
static emd_dev_client_t		drv_client[EMD_CLIENT_NUM];
static atomic_t			rst_on_going = ATOMIC_INIT(0);
static wakeup_ctl_info_t	wakeup_user[WAKEUP_EMD_USER_NUM]; // 0-USB, 1-UART
static unsigned int		ext_modem_is_ready=0;

static void emd_wakeup_timer_cb(unsigned long);
static DEFINE_TIMER(emd_wakeup_timer,emd_wakeup_timer_cb,0,0);
static unsigned int		timeout_pending_count = 0;
static spinlock_t		wakeup_ctl_lock;

static struct wake_lock		emd_wdt_wake_lock;	// Make sure WDT reset not to suspend

/*****************************************************************************************
 *   Log control section
 *****************************************************************************************/
#define EMD_MSG(fmt, args...)	printk("[ext-md-ctl]" fmt, ##args)
#define EMD_RAW			printk
/* Debug message switch */
#define EMD_DBG_NONE		(0x00000000)    /* No debug log */
#define EMD_DBG_FUNC		(0x00000001)    /* Function entry log */
#define EMD_DBG_PM		(0x00000002)
#define EMD_DBG_MISC		(0x00000004)
#define CCCI_DBG_ALL		(0xffffffff)
static unsigned int		emd_msg_mask = CCCI_DBG_ALL;
/*---------------------------------------------------------------------------*/
#define EMD_DBG_MSG(mask, fmt, args...) \
do {	\
	if ((EMD_DBG_##mask) & emd_msg_mask ) \
            printk("[ext-md-ctl]" fmt , ##args); \
} while(0)
/*---------------------------------------------------------------------------*/
#define EMD_FUNC_ENTER(f)		EMD_DBG_MSG(FUNC, "%s ++\n", __FUNCTION__)
#define EMD_PM_MSG(fmt, args...)	EMD_DBG_MSG(MISC, fmt, ##args)
#define EMD_FUNC_EXIT(f)		EMD_DBG_MSG(FUNC, "%s -- %d\n", __FUNCTION__, f)
#define EMD_MISC_MSG(fmt, args...)	EMD_DBG_MSG(MISC, fmt, ##args)

/******************************************************************************************
 *   External customization functions region
 ******************************************************************************************/
extern void cm_ext_md_rst(void);
extern void cm_gpio_setup(void);
extern void cm_hold_wakeup_md_signal(void);
extern void cm_release_wakeup_md_signal(void);
extern void cm_enable_ext_md_wdt_irq(void);
extern void cm_disable_ext_md_wdt_irq(void);
extern void cm_enable_ext_md_wakeup_irq(void);
extern void cm_disable_ext_md_wakeup_irq(void);
extern int  cm_do_md_power_on(void);
extern int  cm_do_md_go(void);
extern void cm_do_md_rst_and_hold(void);


/******************************************************************************************
 *   Helper functions region
 ******************************************************************************************/ 
static int request_ext_md_reset(void); // Function declaration
static int send_message_to_user(emd_dev_client_t *client, int msg);

static void ext_md_rst(void)
{
	EMD_FUNC_ENTER();
	cm_ext_md_rst();
	ext_modem_is_ready = 0;
}

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
		 		EMD_MSG("E: %s%d mis-match!\n", 
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

void emd_wakeup_timer_cb(unsigned long data  __always_unused)
{
	int release_wakeup_confirm = 1;
	int i;
	unsigned long flags;

	EMD_FUNC_ENTER();
	spin_lock_irqsave(&wakeup_ctl_lock, flags);

	// 1. Clear timeout counter
	for(i=0; i<WAKEUP_EMD_USER_NUM; i++){
		if(wakeup_user[i].time_out_req_count > 0){
			wakeup_user[i].time_out_req_count=0;
		}
	}

	// 2. Update timeout pending counter
	if(timeout_pending_count == 0){
		EMD_MISC_MSG("timeout_pending_count == 0\n");
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
			cm_release_wakeup_md_signal();
			EMD_FUNC_EXIT(1);
		}
	}else{ // Need to run once more
		mod_timer(&emd_wakeup_timer, jiffies+2*HZ);
		EMD_FUNC_EXIT(2);
	}

	EMD_FUNC_EXIT(0);
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

static void enable_ext_md_wakeup_irq(void)
{
	EMD_MISC_MSG("Enable Ext MD Wakeup!\n");
	cm_enable_ext_md_wakeup_irq();
}

static void disable_ext_md_wakeup_irq(void)
{
	EMD_MISC_MSG("Disable Ext MD Wakeup!\n");
	cm_disable_ext_md_wakeup_irq();
}


static void ext_md_wdt_irq_cb(void)
{
	EMD_MSG("Ext MD WDT rst gotten!\n");
	if(ext_modem_is_ready){
		EMD_MSG("Ext MD is ready, request MD reboot!\n");
		request_ext_md_reset();
		wake_lock_timeout(&emd_wdt_wake_lock, 15*HZ);
	}else{
		EMD_MSG("Ext MD is Not ready, ignore it!\n");
	}
}

static void ext_md_wakeup_irq_cb(void)
{
	EMD_MISC_MSG("Ext MD wake up request!\n");
	wake_lock_timeout(&drv_client[0].emd_wake_lock, 2*HZ);
	disable_ext_md_wakeup_irq();
}

static int register_dev_node(const char *name, int major, int sub_id)
{
	char name_str[64];
	dev_t dev;
	struct class *emd_class = NULL;

	EMD_FUNC_ENTER();
	memset(name_str, 0, sizeof(name_str));
	dev = MKDEV(major,0) + sub_id;
	snprintf(name_str, 64, "%s%d", name, sub_id);

	emd_class = class_create(THIS_MODULE, name_str);
	device_create(emd_class, NULL, dev, NULL, name_str);

	EMD_FUNC_EXIT(0);

	return 0;
}

static int let_ext_md_go(void) 
{
	int ret;
	int retry;
	EMD_FUNC_ENTER();

	ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_READY);
	atomic_set(&rst_on_going, 0);
	if( ret==0 ){
		//cm_enable_ext_md_wdt_irq();
		retry = cm_do_md_go();
		EMD_MSG("let_ext_md_go, retry:%d\n", retry);
		cm_enable_ext_md_wdt_irq();
		ext_modem_is_ready = 1;
	}else{
		EMD_MSG("let_ext_md_go fail, msg does not send\n");
		ret = -1;
	}

	EMD_FUNC_EXIT(0);

	return ret;
}

int request_ext_md_reset()
{
	int ret = 0;

	EMD_FUNC_ENTER();

	//disable_ext_md_wdt_irq(); //------------------------------------------------------
	if(atomic_inc_and_test(&rst_on_going) == 0){
		//ext_md_rst();
		ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_REQUEST_RST);
		if(ret!=0){
			EMD_MSG("request_ext_md_reset fail, msg does not send\n");
			atomic_dec(&rst_on_going);
		}
	}else{
		EMD_MSG("reset is on-going\n");
	}

	EMD_FUNC_EXIT(0);

	return ret;
}

static int power_on_md_and_hold_it(void)
{
	EMD_FUNC_ENTER();
	cm_do_md_power_on();
	cm_do_md_rst_and_hold();
	EMD_FUNC_EXIT(0);

	return 0;
}

static int push_data(emd_dev_client_t *client, int data)
{
	int size, ret;

	EMD_FUNC_ENTER();

	if(kfifo_is_full(&client->fifo)){
		ret=-ENOMEM;
		EMD_MSG("sub_dev%d kfifo full\n",client->sub_dev_id);
	}else{
		size=kfifo_in(&client->fifo,&data,sizeof(int));
		WARN_ON(size!=sizeof(int));
		ret=sizeof(int);
	}
	EMD_FUNC_EXIT(0);

	return ret;
}

static int pop_data(emd_dev_client_t *client, int *buf)
{
	int ret = 0;

	EMD_FUNC_ENTER();

	if(!kfifo_is_empty(&client->fifo))
		ret = kfifo_out(&client->fifo, buf, sizeof(int));

	EMD_FUNC_EXIT(0);

	return ret;
}

int send_message_to_user(emd_dev_client_t *client, int msg)
{
	int ret = 0;
	unsigned long flags = 0;

	EMD_FUNC_ENTER();
	spin_lock_irqsave(&client->lock, flags);

	// 1. Push data to fifo
	ret = push_data(client, msg);

	// 2. Wake up read function
	if( sizeof(int) == ret ){
		wake_up_interruptible(&client->wait_q);
		ret = 0;
	}

	spin_unlock_irqrestore(&client->lock, flags);
	EMD_FUNC_EXIT(0);

	return ret;
}

static int client_init(emd_dev_client_t *client, int sub_id)
{
	int ret = 0;
	EMD_FUNC_ENTER();
	if( (sub_id >= EMD_CLIENT_NUM) || (sub_id < 0) ){
		EMD_FUNC_EXIT(1);
		return -1;
	}

	// 1. Clear client
	memset(client, 0, sizeof(emd_dev_client_t));

	// 2. Setting device id
	client->major_dev_id = EMD_CHR_DEV_MAJOR;
	client->sub_dev_id = sub_id;

	// 3. Init wait queue head, wake lock, spin loc and semaphore
	init_waitqueue_head(&client->wait_q);
	wake_lock_init(&client->emd_wake_lock, WAKE_LOCK_SUSPEND, EMD_DEV_NAME);
	spin_lock_init(&client->lock);
	mutex_init(&client->emd_mutex);

	// 4. Set user_num to zero
	client->user_num = 0;

	// 5. Alloc and init kfifo
	ret=kfifo_alloc(&client->fifo, EMD_MAX_MESSAGE_NUM*sizeof(int),GFP_ATOMIC);
	if (ret){
		EMD_MSG("kfifo alloc failed(ret=%d).\n",ret);
		wake_lock_destroy(&client->emd_wake_lock);
		EMD_FUNC_EXIT(2);
		return ret;
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int client_deinit(emd_dev_client_t *client)
{
	EMD_FUNC_ENTER();
	kfifo_free(&client->fifo);
	wake_lock_destroy(&client->emd_wake_lock);
	EMD_FUNC_EXIT(0);
	return 0;
}

static unsigned int generate_eint_flag(int pol, int sen)
{ 
	unsigned int flag = 0;
	if(pol==CUST_EINT_POLARITY_HIGH && sen==CUST_EINT_EDGE_SENSITIVE)
		flag = EINTF_TRIGGER_RISING;
	if(pol==CUST_EINT_POLARITY_LOW && sen==CUST_EINT_EDGE_SENSITIVE)
		flag = EINTF_TRIGGER_FALLING;
	if(pol==CUST_EINT_POLARITY_HIGH && sen==CUST_EINT_LEVEL_SENSITIVE)
		flag = EINTF_TRIGGER_HIGH;
	if(pol==CUST_EINT_POLARITY_LOW && sen==CUST_EINT_LEVEL_SENSITIVE)
		flag = EINTF_TRIGGER_LOW;
	return flag;
}

static void eint_setup(void)
{
	//--- Ext MD wdt irq -------
	mt_eint_mask(CUST_EINT_DT_EXT_MD_WDT_NUM);
	mt_eint_set_hw_debounce(CUST_EINT_DT_EXT_MD_WDT_NUM, CUST_EINT_DT_EXT_MD_WDT_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_DT_EXT_MD_WDT_NUM, 
				 generate_eint_flag(CUST_EINT_DT_EXT_MD_WDT_POLARITY, CUST_EINT_DT_EXT_MD_WDT_SENSITIVE), 
				 ext_md_wdt_irq_cb, 
				 0);

	//--- Ext MD wake up irq ------------
	mt_eint_set_hw_debounce(CUST_EINT_DT_EXT_MD_WK_UP_NUM, CUST_EINT_DT_EXT_MD_WK_UP_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_DT_EXT_MD_WK_UP_NUM, 
				 generate_eint_flag(CUST_EINT_DT_EXT_MD_WK_UP_POLARITY, CUST_EINT_DT_EXT_MD_WK_UP_SENSITIVE), 
				 ext_md_wakeup_irq_cb, 
				 0);
}

extern char usb_h_acm_all_clear(void);
void check_drv_rdy_to_rst(void)
{
	int check_count = 0; // Max 10 seconds
	while(check_count<MAX_CHK_RDY_TIMES){
		if(usb_h_acm_all_clear())
			return;
		msleep(200);
		check_count++;
	}
	EMD_MSG("Wait drv rdy to rst timeout!!\n");
}

/******************************************************************************************
 *   Driver functions region
 ******************************************************************************************/ 
static int emd_dev_open(struct inode *inode, struct file *file)
{
	int index=iminor(inode);
	int ret=0;

	EMD_MSG("Open by %s sub_id:%d\n",current->comm,index);

	if( (index >= EMD_CLIENT_NUM) || (index < 0) ){
		EMD_MSG("Open func get invalid dev sub id\n");
		EMD_FUNC_EXIT(1);
		return -1;
	}

	mutex_lock(&drv_client[index].emd_mutex);
	if(drv_client[index].user_num > 0){
		EMD_MSG("Multi-Open not support!\n");
		mutex_unlock(&drv_client[index].emd_mutex);
		EMD_FUNC_EXIT(2);
		return -1;
	}
	drv_client[index].user_num++;
	mutex_unlock(&drv_client[index].emd_mutex);

	file->private_data=&drv_client[index];	
	nonseekable_open(inode,file);

	EMD_FUNC_EXIT(0);
	return ret;
}

static int emd_dev_release(struct inode *inode, struct file *file)
{
	int ret=0;
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;

	EMD_MSG("clint %d call release\n", client->sub_dev_id);

	// Release resource and check wether need trigger reset
	if(1 == client->sub_dev_id){ // Only ext_md_ctl1 has the ability of  trigger reset
		if( 0!=atomic_read(&rst_on_going) ){
			ext_md_rst();
			msleep(EMD_RST_LOW_TIME);
			check_drv_rdy_to_rst();
			EMD_MSG("send wait done message\n");
			ret = send_message_to_user(&drv_client[0], EXT_MD_MSG_WAIT_DONE);
			if( ret!=0 )
				EMD_MSG("send wait done message fail\n");
		}
	}

	mutex_lock(&client->emd_mutex);
	client->user_num--;
	mutex_unlock(&client->emd_mutex);
	EMD_FUNC_EXIT(0);

	return 0;
}

static ssize_t emd_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;
	int ret=0,data;

	DEFINE_WAIT(wait);

	EMD_FUNC_ENTER();
	WARN_ON(client==NULL);

	ret=pop_data(client,&data);

	if( sizeof(int) == ret){
		EMD_MISC_MSG("client%d get data:%08x, ret=%d\n", client->sub_dev_id, data, ret);
		if( copy_to_user(buf,&data,sizeof(int)) ){
			EMD_MSG("copy_to_user fialed\n");
			ret= -EFAULT;
		}else{
			ret = sizeof(int);
		}
	}else{
		if (file->f_flags & O_NONBLOCK){
			ret=-EAGAIN;
		}else{
			prepare_to_wait(&client->wait_q,&wait,TASK_INTERRUPTIBLE);
			schedule();

			if (signal_pending(current)){
				EMD_MSG("Interrupted syscall.signal_pend=0x%llx\n",
					*(long long *)current->pending.signal.sig);
				ret=-EINTR;
			}
		}
	}

	finish_wait(&client->wait_q,&wait);
	EMD_FUNC_EXIT(0);

	return ret;
}

static long emd_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	emd_dev_client_t *client=(emd_dev_client_t *)file->private_data;

	EMD_FUNC_ENTER();
	switch (cmd) {
	case EXT_MD_IOCTL_LET_MD_GO:
		if(1 == client->sub_dev_id){ // Only /dev/ext_md_ctl1 has the ablility of let modem go
			ret = let_ext_md_go();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_LET_MD_GO is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;

	case EXT_MD_IOCTL_REQUEST_RESET:
		if(2 == client->sub_dev_id){ // Only /dev/ext_md_ctl1 has the ablility of request modem reset
			ret = request_ext_md_reset();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_REQUEST_RESET is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;
	
	case EXT_MD_IOCTL_POWER_ON_HOLD:
		if(0 == client->sub_dev_id){ // Only /dev/ext_md_ctl0 has the ablility of power on modem
			ret = power_on_md_and_hold_it();
		}else{
			EMD_MSG("sub dev%d call EXT_MD_IOCTL_POWER_ON_HOLD is denied\n", client->sub_dev_id);
			ret = -EMD_ERR_ACCESS_DENY;
		}
		break;

	default:
		ret = -EMD_ERR_UN_DEF_CMD;
		break;
	}
	EMD_FUNC_EXIT(0);

	return ret;
}

static int emd_ctl_drv_probe(struct platform_device *device)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("clint %d probe!!\n", device->id);

	if( (device->id < 0)||(device->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", device->id);
	else{
		register_dev_node(EMD_DEV_NODE_NAME, EMD_CHR_DEV_MAJOR, device->id);
		if(0 == device->id){
			cm_gpio_setup();
			eint_setup();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int emd_ctl_drv_remove(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("remove!!\n" );
	EMD_FUNC_EXIT(0);
	return 0;
}

static void emd_ctl_drv_shutdown(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_MISC_MSG("shutdown!!\n" );
	EMD_FUNC_EXIT(0);
}

static int emd_ctl_drv_suspend(struct platform_device *dev, pm_message_t state)
{
	EMD_FUNC_ENTER();
	EMD_PM_MSG("client:%d suspend!!\n", dev->id);
	if( (dev->id < 0)||(dev->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", dev->id);
	else{
		if(0 == dev->id){
			cm_release_wakeup_md_signal();
			//cm_enable_ext_md_wakeup_irq();
			enable_ext_md_wakeup_irq();
			del_timer(&emd_wakeup_timer);
			reset_wakeup_control_logic();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}

static int emd_ctl_drv_resume(struct platform_device *dev)
{
	EMD_FUNC_ENTER();
	EMD_PM_MSG("client:%d resume!!\n", dev->id);
	if( (dev->id < 0)||(dev->id >= EMD_CLIENT_NUM) )
		EMD_MSG("invalid id%d!!\n", dev->id);
	else{
		if(0 == dev->id){
			disable_ext_md_wakeup_irq();
		}
	}
	EMD_FUNC_EXIT(0);

	return 0;
}


static struct file_operations emd_chr_dev_fops=
{
	.owner		= THIS_MODULE,
	.open		= emd_dev_open,
	.read		= emd_dev_read,
	.release	= emd_dev_release,
	.unlocked_ioctl	= emd_dev_ioctl,

};

static struct platform_driver emd_ctl_driver =
{
	.driver     = {
	.name		= EMD_DEV_NAME,
		},
	.probe		= emd_ctl_drv_probe,
	.remove		= emd_ctl_drv_remove,
	.shutdown	= emd_ctl_drv_shutdown,
	.suspend	= emd_ctl_drv_suspend,
	.resume		= emd_ctl_drv_resume,
};

int __init emd_chrdev_init(void)
{
	int ret=0, i;

	EMD_FUNC_ENTER();
	EMD_MSG("ver: 20111111\n");

	/* 0. Init global ext-md-wdt wake lock */
	wake_lock_init(&emd_wdt_wake_lock, WAKE_LOCK_SUSPEND, "ext_md_wdt_wake_lock");

	/* 1. Register device region. 0~1 */
	if (register_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM,EMD_DEV_NAME) != 0)
	{
		EMD_MSG("Regsiter EMD_CHR_DEV failed\n");
		ret=-1;
		goto register_region_fail;
	}

	/* 2. Alloc charactor devices */
	emd_chr_dev=cdev_alloc();
	if (NULL == emd_chr_dev)
	{
		EMD_MSG("cdev_alloc failed\n");
		ret=-1;
		goto cdev_alloc_fail;
	}

	/* 3. Initialize and add devices */
	cdev_init(emd_chr_dev,&emd_chr_dev_fops);
	emd_chr_dev->owner=THIS_MODULE;
	ret = cdev_add(emd_chr_dev,MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);
	if (ret){
		EMD_MSG("cdev_add failed\n");
		goto cdev_add_fail;
	}

	/* 4. Register platform driver */
	ret = platform_driver_register(&emd_ctl_driver);
	if(ret){	
		EMD_MSG("platform_driver_register failed\n");
		goto platform_driver_register_fail;
	}

	/* 5. Init driver client */
	for(i=0; i<EMD_CLIENT_NUM; i++){
		if( 0!=client_init(&drv_client[i], i) ){
			EMD_MSG("driver client init failed\n");
			goto driver_client_init_fail;
		}
	}

	/* 6. Init wake up ctl structure */
	memset(wakeup_user, 0, sizeof(wakeup_user));
	wakeup_user[0].dev_major_id = DEVICE0_ID;
	wakeup_user[0].dev_sub_id = 0;
	wakeup_user[0].name = USER0_NAME;
	wakeup_user[1].dev_major_id = DEVICE1_ID;
	wakeup_user[1].dev_sub_id = 1;
	wakeup_user[1].name = USER1_NAME;
	spin_lock_init(&wakeup_ctl_lock);

	EMD_FUNC_EXIT(0);
	return 0;

driver_client_init_fail:
platform_driver_register_fail:

cdev_add_fail:
	cdev_del(emd_chr_dev);

cdev_alloc_fail:
	unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);

register_region_fail:
	EMD_FUNC_EXIT(1);
	return ret;
}

void __exit emd_chrdev_exit(void)
{
	EMD_FUNC_ENTER();
	unregister_chrdev_region(MKDEV(EMD_CHR_DEV_MAJOR,0),EMD_CLIENT_NUM);
	cdev_del(emd_chr_dev);
	client_deinit(&drv_client[0]);
	client_deinit(&drv_client[1]);
	wake_lock_destroy(&emd_wdt_wake_lock);
	EMD_FUNC_EXIT(0);
}

module_init(emd_chrdev_init);
module_exit(emd_chrdev_exit);

MODULE_AUTHOR("Chao Song");
MODULE_DESCRIPTION("Ext Modem Control Device Driver");
MODULE_LICENSE("GPL");
