#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/rtpm_prio.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#include "tpd.h"
#include "cust_eint.h"

#define TPD_SLAVE_ADDR 0x8c

extern struct tpd_device *tpd;
extern int tpd_firmware_version[2];
extern int tpd_calibrate_en;
extern int tpd_show_version;

static void tpd_eint_interrupt_handler(void);
static void tpd_do_calibrate(void);
static void tpd_print_version(void);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_remove(struct i2c_client *client);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
#if 0
// TODO: should be moved into mach/xxx.h 
extern void MT6516_EINTIRQUnmask(unsigned int line);
extern void MT6516_EINTIRQMask(unsigned int line);
extern void MT6516_EINT_Set_HW_Debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 MT6516_EINT_Set_Sensitivity(kal_uint8 eintno, kal_bool sens);
extern void MT6516_EINT_Registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);

extern void MT6516_IRQMask(unsigned int line);
extern void MT6516_IRQUnmask(unsigned int line);
extern void MT6516_IRQClearInt(unsigned int line);
#endif
struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag=0;
static int boot_mode = 0;
static int tpd_halt=0;
static int touch_event_handler(void *unused);

static struct i2c_client *i2c_client = NULL;
static struct i2c_client *i2c_rs_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"mtk-tpd",0},{}};
static unsigned short force[] = {2, 0x8c, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = { .forces = forces,};
struct i2c_driver tpd_i2c_driver = {
    .probe = tpd_i2c_probe,
    .remove = tpd_i2c_remove,
    .detect = tpd_i2c_detect,
    .driver.name = "mtk-tpd",
    .id_table = tpd_i2c_id,
    .address_data = &addr_data,
};

int tpd_local_init(void) {
    boot_mode = get_boot_mode();
    // Software reset mode will be treated as normal boot
    if(boot_mode==3) boot_mode = NORMAL_BOOT;
    if(i2c_add_driver(&tpd_i2c_driver)!=0)
        TPD_DMESG("unable to add i2c driver.\n");
    return 0;
}

static int tpd_i2c_remove(struct i2c_client *client) {return 0;}
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int err;
    i2c_client = client;
    i2c_rs_client = client;
    i2c_rs_client->addr = i2c_rs_client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
    
    printk("[mtk-tpd] i2c device probe\n");
    
    /* added in android 2.2, for configuring EINT2 to GPIO mode */
    mt_set_gpio_mode(GPIO61, 0x00);
    mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
    mt_set_gpio_dir(GPIO61, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO61, GPIO_OUT_ZERO);
    
    hwPowerDown(TPD_POWER_SOURCE,"TP");
    hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
    msleep(20);
    
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }
        
    /* added in android 2.2, for configuring EINT2 to EINT mode */
    mt_set_gpio_mode(GPIO61, 0x01);
    mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO61, GPIO_PULL_UP);
    
    //MT6516_EINT_Set_Sensitivity(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //MT6516_EINT_Set_HW_Debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
   
    return 0;
}

static void tpd_eint_interrupt_handler(void) {
    TPD_DEBUG_PRINT_INT; tpd_flag=1; wake_up_interruptible(&waiter);
}

void tpd_down(int raw_x, int raw_y, int x, int y, int p) {
    input_report_abs(tpd->dev, ABS_PRESSURE, 128);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("D[%4d %4d %4d]\n", x, y, p);
    TPD_DEBUG_PRINT_DOWN;
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
}

void tpd_up(int raw_x, int raw_y, int x, int y, int p) {
    int pending = 0;
    //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("U[%4d %4d %4d]\n", x, y, 0);
    TPD_DEBUG_PRINT_UP;
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
}

static void tpd_do_calibrate(void) {
    char buffer[10];
    printk("[mtk-tpd] collect no-touch info, please do not hold the screen\n");
    buffer[0] = 0x20;
    buffer[1] = 0x13;
    buffer[2] = 0x00;
    buffer[3] = 0x00;
    buffer[4] = 0x00;
    buffer[5] = 0x00;    
    buffer[6] = 0x00;
    buffer[7] = 0x00;
    i2c_master_send(i2c_client, buffer, 8);
    msleep(1500);
    printk("[mtk-tpd] collect no-touch info finished\n");
}

static void tpd_print_version(void) {
    char buffer[10];
    struct i2c_msg msg[2];
      
    buffer[0] = 0x80;
    while (1) {
        i2c_master_send(i2c_rs_client, &buffer[0], (1 << 8 | 1));
        TPD_DEBUG("[mtk-tpd] buffer: %x\n", buffer[0]);
        if (buffer[0] == 0x00) break;
        msleep(10);
    }
        
    buffer[0] = 0x20;
    buffer[1] = 0xE1;
    buffer[2] = 0x08;
    buffer[3] = 0x01;
    buffer[4] = 0x08;
    buffer[5] = 0x00;    
    buffer[6] = 0x00;
    buffer[7] = 0xd0;
    i2c_master_send(i2c_client, buffer, 8);

    msleep(20);
    
    buffer[0] = 0x80;
    while (1) {
        i2c_master_send(i2c_rs_client, &buffer[0], (1 << 8 | 1));
        TPD_DEBUG("[mtk-tpd] buffer: %x\n", buffer[0]);
        if (buffer[0] == 0x00) break;
        msleep(10);
    }
    
    buffer[0] = 0xa0;
    i2c_master_send(i2c_rs_client, &buffer[0], (8 << 8 | 1));
    
    printk("[mtk-tpd] ITE Touch Panel Firmware Version %d.%d Subversion %02x.%02x\n", 
                buffer[0], buffer[1], buffer[2], buffer[3]);  
}

static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    char index;
    char buffer[14];
    int i, pre_touch1 = 0, pre_touch2 = 0, touch = 0, finger_num = 0;
    int x1, y1, p1, x2, y2, p2, raw_x1, raw_y1, raw_x2, raw_y2;
    struct i2c_msg msg[2];
    
    sched_setscheduler(current, SCHED_RR, &param);
    
    do {
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
        set_current_state(TASK_INTERRUPTIBLE);
        if (!kthread_should_stop()) {
            while (tpd_halt) {tpd_flag=0; msleep(20);}
            if (pre_touch1 || pre_touch2)
                wait_event_interruptible_timeout(waiter, tpd_flag!=0, HZ/8);
            else
                wait_event_interruptible_timeout(waiter, tpd_flag!=0, HZ*2);
            TPD_DEBUG_SET_TIME;
        }
        set_current_state(TASK_RUNNING);
        
        if (tpd_calibrate_en | tpd_show_version) {
            mt_set_gpio_mode(GPIO61, 0x00);
            mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
            mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
            mt_set_gpio_dir(GPIO61, GPIO_DIR_OUT);
            mt_set_gpio_out(GPIO61, GPIO_OUT_ZERO);
            
            hwPowerDown(TPD_POWER_SOURCE,"TP");
            hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
            msleep(20);
    
            if (tpd_calibrate_en) {
                tpd_do_calibrate();
                tpd_calibrate_en = 0;
            } else {
                tpd_print_version();
                tpd_show_version = 0;
            }
                
            /* added in android 2.2, for configuring EINT2 to EINT mode */
            mt_set_gpio_mode(GPIO61, 0x01);
            mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
            mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
            continue;
        } 
        
        if (!tpd_flag) {
            if (pre_touch1==1) {
                tpd_up(raw_x1, raw_y1, x1,y1,p1); pre_touch1 = 0;
            }
            if (pre_touch2==1) {
                tpd_up(raw_x2, raw_y2, x2,y2,p2); pre_touch2 = 0;
            }
            input_sync(tpd->dev);
            continue;
        } else {
            tpd_flag = 0;
        }
        
        buffer[0] = 0x80;
        i2c_master_send(i2c_rs_client, &buffer[0], (1 << 8 | 1));
    
        touch = (buffer[0] & 0x08) >> 3; // finger up or finger down
        //printk("buffer[0] = %d\n", buffer[0]);
        
        if (buffer[0]&0x80) {
           
            buffer[6] = 0xc0;
            i2c_master_send(i2c_rs_client, &buffer[6], (8 << 8 | 1));
              
            buffer[0] = 0xe0;
            i2c_master_send(i2c_rs_client, &buffer[0], (6 << 8 | 1));
            
            if (finger_num == 0x03 && buffer[0] == 0x00) {
                TPD_DEBUG("[mtk-tpd] firmware bug. hold one finger, hold another finger, and then tap the first finger, it will happen.\n");
                continue;
            }
            
            if ((buffer[0] & 0xF0)) {
                TPD_DEBUG("[mtk-tpd] this is not a position information\n");
                continue;
            }
            
            if (buffer[1] == 0x01) {
                TPD_DEBUG("[mtk-tpd] fat touch detect\n");
                continue;
            }
            
            finger_num = buffer[0]&0x07;
            if (finger_num&1) {
                x1 = buffer[2]+((buffer[3]&0x0f)<<8);
                y1 = buffer[4]+((buffer[3]&0xf0)<<4);
                p1 = buffer[5]&0x0f;
                raw_x1 = x1; raw_y1 = y1;
                tpd_calibrate(&x1, &y1);
                tpd_down(raw_x1, raw_y1, x1, y1, p1);
                pre_touch1=1;
            } else {
                if (pre_touch1!=0) {
                    tpd_up(raw_x1, raw_y1, x1, y1, p1); 
                    pre_touch1 = 0;
                }
            }
                
            if (finger_num&2) {
                x2 = buffer[6]+((buffer[7]&0x0f)<<8);
                y2 = buffer[8]+((buffer[7]&0xf0)<<4);
                p2 = buffer[9]&0x0f;
                raw_x2 = x2; raw_y2 = y2;
                tpd_calibrate(&x2, &y2);
                tpd_down(raw_x2, raw_y2, x2, y2, p2);
                pre_touch2=1;
            } else {
                if (pre_touch2!=0) {
                    tpd_up(raw_x2, raw_y2, x2, y2, p2); 
                    pre_touch2 = 0;
                }
            }
            
            input_sync(tpd->dev);
        } else {
            msleep(10);
        }       
    } while (!kthread_should_stop());
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

/* platform device functions */
void tpd_suspend(struct early_suspend *h) {
    tpd_halt = 1;
    mt_set_gpio_mode(GPIO61, 0x00);
    mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
    mt_set_gpio_dir(GPIO61, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO61, GPIO_OUT_ONE);
    
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    //MT6516_IRQMask(MT6516_TOUCH_IRQ_LINE);
    msleep(50);
    
    hwPowerDown(TPD_POWER_SOURCE,"TP");
}
void tpd_resume(struct early_suspend *h) {
    /* added in android 2.2, for configuring EINT2 to GPIO mode */
    mt_set_gpio_mode(GPIO61, 0x00);
    mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
    mt_set_gpio_dir(GPIO61, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO61, GPIO_OUT_ZERO);
    
    hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
    mdelay(20);
    
    /* added in android 2.2, for configuring EINT2 to EINT mode */
    mt_set_gpio_mode(GPIO61, 0x01);
    mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO61,GPIO_PULL_UP);
        
    //MT6516_IRQUnmask(MT6516_TOUCH_IRQ_LINE);    
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    tpd_halt = 0;
}
#endif
