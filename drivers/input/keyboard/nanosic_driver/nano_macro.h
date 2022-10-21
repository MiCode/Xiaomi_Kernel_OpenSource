/** ***************************************************************************
 * @file nano_macro.h
 *
 */
#ifndef _NANO_MACRO_H_
#define _NANO_MACRO_H_

#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>

/*SDK版本号*/
#define DRV_VERSION  "1.3.2"

#define DRV_TOKEN    "nanodev"

#define DEBUG_LEVEL	            (7)
#define INFO_LEVEL	            (5)
#define ERROR_LEVEL	            (3)
#define ALERT_LEVEL	            (1)

/*设备挂载的i2c bus id*/
#define I2C_BUS_ID              (2)

/*设备的i2c addr*/
#define I2C_SLAVE_ADDR          (0x4c)  /*high 7bits   从设备地址，第一个字节的高7位，后一位是读写位*/

/*默认写的i2c长度是66bytes*/
#define I2C_DATA_LENGTH_WRITE   (66)

/*默认读的i2c长度是67bytes*/
#define I2C_DATA_LENGTH_READ    (68)

/*I2C GPIO中断线的中断编号 , 需要依据平台重新设置*/
#define I2C_GPIO_IRQNO          (60)

#define FIELD_HOST              (0x80)
#define FIELD_803X              (0x18)
#define FIELD_176X              (0x38)

/*input设备结构体*/
struct nano_input_create_dev {
	__u8 name[128];
	__u8 phys[64];
	__u8 uniq[64];
	__u8 *rd_data;
	__u16 rd_size;
	__u16 bus;
	__u32 vendor;
	__u32 product;
	__u32 version;
	__u32 country;
} __attribute__((__packed__));

typedef int (*handler_expired)(void);
//typedef int (*handler_tasklet)(void);
typedef int (*handler_workqueue)(void* data);

/*worker 结构体信息*/
struct nano_worker_client{
    struct work_struct worker;
    struct workqueue_struct* worker_queue;
    struct delayed_work worker_delay;
    handler_workqueue worker_func;
    void * worker_data;
    atomic_t schedule_count;
    atomic_t schedule_delay_count;
};

/*i2c 设备信息*/
struct nano_i2c_client{
    handler_workqueue func; 	/*i2c read function*/
    atomic_t i2c_read_count;  	/*i2c读次数*/
    atomic_t i2c_error_count; 	/*错误包数*/
    int  i2c_bus_id;        	/*I2C bus id*/
    int  i2c_slave_addr;    	/*I2C slave device address*/
    int  irqno;
    int  irqflags;
    struct nano_worker_client* worker;
    struct mutex read_mutex;
    struct mutex write_mutex;
    struct device *dev;
    struct input_dev *input_dev;
    struct spinlock input_report_lock;
};

/*input数据包类型*/
typedef enum{
    EM_PACKET_KEYBOARD = 0, /*键盘*/
    EM_PACKET_CONSUMER,    /*多媒体*/
    EM_PACKET_MOUSE,        /*鼠标*/
    EM_PACKET_TOUCH,        /*触摸*/
    EM_PACKET_VENDOR,       /*vendor*/
    EM_PACKET_UNKOWN,       /*未知*/
}EM_PacketType;;

/*
 * Logging and Debugging
 */ 
    /* The main debug macro.  Uncomment to enable debug messages */
#define DEBUG

    /* Make sure we have a per-module token */
#ifndef DRV_TOKEN
#error Module must #define DRV_TOKEN to be a short nickname of the module.
#endif
    /* Error messages -- always sent to log */
#undef errprint
#undef errmsg
#define errprint(__format_and_args...) \
    printk(DRV_TOKEN ": ERROR: " __format_and_args) 

#define errmsg(__msg...) errprint(DRV_TOKEN,##__msg)

//module_param(debug,int,S_IRUSR|S_IWUSR|S_IWGRP|S_IRGRP);
//MODULE_PARM_DESC(debug, "Set internal debugging level, higher is more verbose");

/*设置默认打印级别*/
extern int debuglevel;

#undef dbgprint
#undef dbgmsg
#define dbgprint(__level, __format,__args...)   \
do {                                           \
    if ((int)debuglevel >= (int)__level) {\
        struct timex  txc;\
        struct rtc_time tm;\
        do_gettimeofday(&(txc.time));\
        txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;\
        rtc_time_to_tm(txc.time.tv_sec,&tm);\
        switch((int)debuglevel) {      \
            case 0: \
            case 1: \
            case 2:  printk("[%02d:%02d:%02d.%03zu] <2>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
            case 3:  printk("[%02d:%02d:%02d.%03zu] <3>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
            case 4:  printk("[%02d:%02d:%02d.%03zu] <4>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
            case 5:  printk("[%02d:%02d:%02d.%03zu] <5>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
            case 6:  printk("[%02d:%02d:%02d.%03zu] <6>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
            default: printk("[%02d:%02d:%02d.%03zu] <7>" DRV_TOKEN ": " __format , tm.tm_hour, tm.tm_min, tm.tm_sec, txc.time.tv_usec/1000, ##__args); break; \
        } \
    }\
} while (0)

#ifdef DEBUG
    /**
     * @brief Send a debug message to the log facility.
     * @ingroup albrxdoc_util
     * @details This macro sends a message to the logging facility with a
     * simple token prefix.  This message is delivered iff the __level
     * parameter is not greater than the value of the \c debug static variable.
     * @note If \c DEBUG is undefined (i.e. the \#define is not active), no
     * debug messages are sent to the logging facility, regardless of level.
     */
#define dbgmsg(__level,__msg...) dbgprint(__level,DRV_TOKEN,##__msg)
#else /* !DEBUG */
#define dbgmsg(__level,__msg...)
#endif
    /* Assertions -- only checked if DEBUG is defined */
#undef ASSERT
#ifdef DEBUG
#define ASSERT(__condition,__fail_action) \
    do { \
        if (!(__condition)) { \
            errmsg("ASSERT(%s:%d): %s\n",__FILE__,__LINE__,#__condition); \
            __fail_action ; \
        } \
    } while (0)
#else
#define ASSERT(__c,__f)
#endif

extern struct nano_i2c_client* gI2c_client;

/*打印raw data数组*/
static inline void rawdata_show(const char* descp,char* buf , size_t size)
{
	int i=0;
	char display[300]={0};

    if(!descp)
        return;

	strcat(display,"[[[");
	for (i = 0; i < (size>15?15:size); i++){
		char  str[4]={0};
		snprintf(str,sizeof(str),"%02X",buf[i]);
		strcat(display,str);
	}
	strcat(display,"]]]");

	dbgprint(DEBUG_LEVEL,"%s -> %s\n",descp,display);
}

extern int  Nanosic_chardev_register(void);
extern void Nanosic_chardev_release(void);
extern int  Nanosic_chardev_client_write(char* data, size_t datalen);
extern int  Nanosic_chardev_client_notify_close(void);
extern void Nanosic_timer_register(struct nano_i2c_client* i2c_client);
extern void Nanosic_timer_release(void);
extern void Nanosic_sysfs_create(struct device* dev);
extern void Nanosic_sysfs_release(struct device* dev);
extern struct nano_worker_client*   
            Nanosic_workQueue_register(struct nano_i2c_client* i2c_client);
extern void Nanosic_workQueue_release(struct nano_worker_client* worker_client);
extern void Nanosic_workQueue_schedule(struct nano_worker_client* worker_client);
extern int  Nanosic_i2c_read(struct nano_i2c_client* i2c_client,void *buf, size_t len);
extern int  Nanosic_i2c_write(struct nano_i2c_client* i2c_client,void *buf, size_t len);
extern int  Nanosic_i2c_parse(char* data, size_t datalen);
extern int  Nanosic_i2c_read_handler(void* data);
extern struct nano_i2c_client*
            Nanosic_i2c_register(int irqno,u32 irq_flags,int i2c_bus_id, int i2c_slave_addr);
extern void Nanosic_i2c_release(struct nano_i2c_client* i2c_client);
extern int  Nanosic_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
extern int  Nanosic_i2c_read_version(struct i2c_client *client);
extern irqreturn_t
            Nanosic_i2c_irq(int irq, void *dev_id);
extern int  Nanosic_input_register(void);
extern int  Nanosic_input_release(void);
extern int  Nanosic_input_write(EM_PacketType type ,void *buf, size_t len);
extern int  Nanosic_GPIO_recovery(struct nano_i2c_client* client , char* data, int datalen);
extern int  Nanosic_GPIO_register(int vdd_pin, int reset_pin, int status_pin, int irq_pin,int sleep_pin);
extern int  Nanosic_Hall_notify(int hall_n_pin, int hall_s_pin);
extern int  Nanosic_RequestGensor_notify(void);
extern void Nanosic_GPIO_release(void);
extern void Nanosic_GPIO_sleep(bool sleep);
extern void Nanosic_GPIO_set(int gpio_pin,bool gpio_level);
extern void Nanosic_cache_expire(struct timer_list *t);
extern int  Nanosic_cache_insert(EM_PacketType type ,void *data, size_t datalen);
extern int  Nanosic_cache_init(void);
extern int  Nanosic_cache_release(void);
extern int  Nanosic_cache_put(void);

extern char gVers803x[21];
extern char gVers176x[21];
extern short gHallStatus;
extern bool g_panel_status;
extern int gpio_hall_n_pin;
extern int gpio_hall_s_pin;
extern int g_wakeup_irqno;

#if 0
extern void Nanosic_GPIO_set(bool status);
#endif

#define STREAM_TO_UINT8(u8, p)   {u8 = (unsigned char)(*(p)); (p) += 1;}
#define STREAM_TO_UINT16(u16, p) {u16 = ((unsigned short)(*(p)) + (((unsigned short)(*((p) + 1))) << 8)); (p) += 2;}


extern void Nanosic_GPIO_sleep(bool sleep);

struct xiaomi_keyboard_data {
    struct notifier_block drm_notif;
    bool dev_pm_suspend;
    int irq;
    struct workqueue_struct *event_wq;
    struct work_struct resume_work;
    struct work_struct suspend_work;
};

static struct xiaomi_keyboard_data *mdata;

__attribute__((unused)) static xiaomi_keyboard_init(struct nano_i2c_client* i2c_client);
__attribute__((unused)) static void keyboard_resume_work(struct work_struct *work);
__attribute__((unused)) static void keyboard_suspend_work(struct work_struct *work);
__attribute__((unused)) static int xiaomi_keyboard_pm_suspend(struct device *dev);
__attribute__((unused)) static int xiaomi_keyboard_pm_resume(struct device *dev);
__attribute__((unused)) static int xiaomi_keyboard_remove(void);
__attribute__((unused)) static const struct dev_pm_ops xiaomi_keyboard_pm_ops;
__attribute__((unused)) static int keyboard_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);


#endif /* _ALBRX_H_ */

