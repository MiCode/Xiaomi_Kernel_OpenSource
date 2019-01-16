#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>

#include "tpd_custom_cy8ctma300.h"
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>
#include "tpd.h"
#include <cust_eint.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif
//#define POLL_MODE

#define CHR_CON0	(0xF7000000+0x2FA00)
extern struct tpd_device *tpd;
extern int tpd_show_version;
extern int tpd_debuglog;
extern int tpd_em_log;
extern int tpd_register_flag;
extern int tpd_load_status;

static int tpd_flag = 0;
static int tpd_halt=0;
static struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);

#ifdef TPD_HAVE_BUTTON 
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
static int tpd_wb_start_local[TPD_WARP_CNT] = TPD_WARP_START;
static int tpd_wb_end_local[TPD_WARP_CNT]   = TPD_WARP_END;
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
static int tpd_calmat_local[8]     = TPD_CALIBRATION_MATRIX;
static int tpd_def_calmat_local[8] = TPD_CALIBRATION_MATRIX;
#endif

static void tpd_eint_interrupt_handler(void);
static int touch_event_handler(void *unused);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
extern void i2c_del_driver(struct i2c_driver *);

static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"mtk-tpd",0},{}};
static unsigned short force[] = {0, 0xce, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
 static struct i2c_board_info __initdata i2c_tpd={ I2C_BOARD_INFO("mtk-tpd", (0xce>>1))};
struct i2c_driver tpd_i2c_driver = {                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    .detect = tpd_i2c_detect,                           
    .driver.name = "mtk-tpd", 
    .id_table = tpd_i2c_id,                             
    //.address_data = &addr_data,      
       .address_list = (const unsigned short*) forces,                   
}; 

static int tpd_i2c_detect(struct i2c_client *client, struct i2c_board_info *info){
    strcpy(info->type, "mtk-tpd");
    return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
    int err = 0;
    char buffer[2];
    int status=0;
    #ifdef TPD_NO_GPIO
    u32 temp;
    
    temp = *(volatile u32 *) TPD_GPIO_GPO_ADDR;
    //temp = temp | 0x40;
    temp = temp |(1<<16) ;
    temp = temp |(1<<17);
    //temp = (temp | 1<<16);
    //mt65xx_reg_sync_write(TPD_GPIO_GPO_ADDR, temp);
    //*(volatile u32 *) TPD_GPIO_GPO_ADDR = temp;
    printk("TPD_GPIO_GPO_ADDR:0x%x\n", *(volatile u32 *) TPD_GPIO_GPO_ADDR);
	
    temp = *(volatile u32 *) TPD_GPIO_OE_ADDR;
    //temp = temp | 0x40;
    temp = temp |(1<<16) ;
    temp = temp |(1<<17);
	//temp = (temp | 1<<16) ;
	//mt65xx_reg_sync_write(TPD_GPIO_OE_ADDR, temp);
   // *(volatile u32 *) TPD_GPIO_OE_ADDR = temp;
    printk("TPD_GPIO_OE_ADDR:0x%x\n", *(volatile u32 *) TPD_GPIO_OE_ADDR);
    #endif
    
    i2c_client = client;    
    TPD_DMESG("[mtk-tpd], cy8ctma300 tpd_i2c_probe ++++\n");
	
    #ifndef TPD_NO_GPIO 
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
    
 //   mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
 //   mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
 //   mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
 //   msleep(10);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
    #endif 

    msleep(50);
   // buffer[0]=0x00;
   // i2c_client->ext_flag = I2C_WR_FLAG;
   // status=i2c_master_send(i2c_client ,buffer, 0x101);
   TPD_DMESG("1...........\n");
    status = i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
    if(status<0) 
    {
            TPD_DMESG("fwq read error\n");
		TPD_DMESG("[mtk-tpd], cy8ctma300 tpd_i2c_probe failed!!\n");
		status = i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
		if(status<0) {
			TPD_DMESG("[mtk-tpd], cy8ctma300 tpd_i2c_probe retry failed!!\n");
			return status;
		}
    }
    TPD_DMESG("fwq buffer=%x \n",buffer[0]);

    TPD_DMESG("[mtk-tpd], cy8ctma300 tpd_i2c_probe success!!\n");		
    tpd_load_status = 1;
        
    if ((buffer[0] & 0x70) != 0x00) 
    {
        buffer[0] = 0x00; // switch to operation mode
       	
        i2c_smbus_write_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
        if(status < 0)
	 {
	    TPD_DMESG("fwq write error\n");
	 }
        msleep(50);
    }

    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }    
#ifndef POLL_MODE //mt6575t fpga debug 0: enable polling mode
    //mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_DEBOUNCE_DISABLE);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    TPD_DMESG("EINT num=%d\n",CUST_EINT_TOUCH_PANEL_NUM);
#endif 
	TPD_DMESG("[mtk-tpd], cy8ctma300 tpd_i2c_probe ----\n");

    return 0;
}

void tpd_eint_interrupt_handler(void) { 
//	if(tpd_debuglog==1) TPD_DMESG("[mtk-tpd], %s\n", __FUNCTION__);
	if(1) TPD_DMESG("[mtk-tpd], %s\n", __FUNCTION__);
    TPD_DEBUG_PRINT_INT; tpd_flag=1; wake_up_interruptible(&waiter);
} 
static int tpd_i2c_remove(struct i2c_client *client) {return 0;}

void tpd_down(int raw_x, int raw_y, int x, int y, int p) {

    printk("mtk-tpd: D[rx:%4d,ry:%4d, %4d %4d %4d]\n", raw_x, raw_y, x, y, p);
	if(tpd && tpd->dev && tpd_register_flag==1) {
    input_report_abs(tpd->dev, ABS_PRESSURE, p/PRESSURE_FACTOR);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p/PRESSURE_FACTOR);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, p/PRESSURE_FACTOR);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("D[%4d %4d %4d]\n", x, y, p);
    printk("mtk-tpd: [%4d %4d %4d]\n", x, y, p);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
  }  
}

void tpd_up(int raw_x, int raw_y, int x, int y, int p) {
	if(tpd && tpd->dev && tpd_register_flag==1) {
    //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("U[%4d %4d %4d]\n", x, y, 0);
    printk("mtk-tpd:U[%4d %4d %4d]\n", x, y, 0);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
  }  
}

volatile unsigned long g_temptimerdiff;
int x_min=0, y_min=0, x_max=0, y_max=0, counter_pointer=0;
static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    int x1=0, y1=0, x2=0, y2=0, x3=0, y3=0, x4=0, y4=0, p1=0, p2=0, p3=0, p4=0, id1=0xf, id2=0xf, id3=0xf, id4 = 0xf, pre_id1 = 0xf, pre_id2 = 0xf, pre_id3 = 0xf, pre_id4 = 0xf, pre_tt_mode = 0, finger_num = 0, pre_num = 0;
    int raw_x1=0, raw_y1=0, raw_x2=0, raw_y2=0, raw_x3=0, raw_y3=0, raw_x4=0, raw_y4=0;
    static char toggle;
    static char buffer[32];//[16];
//    int pending = 0;
	//u32 temp;
    sched_setscheduler(current, SCHED_RR, &param); 
    g_temptimerdiff=get_jiffies_64();//jiffies;
    do {
		if(tpd_debuglog==1) {
			TPD_DMESG("[mtk-tpd] %s\n", __FUNCTION__); 
		}	    	
        set_current_state(TASK_INTERRUPTIBLE);
	if(tpd_debuglog==1)
		TPD_DMESG("[mtk-tpd], %s, tpd_halt=%d\n", __FUNCTION__, tpd_halt);
        while (tpd_halt) {tpd_flag = 0; msleep(20);}
        #ifndef POLL_MODE
        wait_event_interruptible(waiter, tpd_flag != 0);
        tpd_flag = 0;
        #endif
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 
	//	#ifndef CY8CTMA300_CHARGE
		#if 0			
		temp =  *(volatile u32 *)CHR_CON0;	
		temp &= (1<<13);
		if(temp!=0)
		{
			   if(tpd_debuglog==1)	
			   	TPD_DMESG("[mtk-tpd], write 0x01 to 0x1D register!!\n");
			   buffer[0] = 0x01;
			   buffer[0] = 0x01;
			   i2c_smbus_write_i2c_block_data(i2c_client, 0x1D, 1, &(buffer[0]));    		
		}
		else
		{
			   if(tpd_debuglog==1)			
			   	TPD_DMESG("[mtk-tpd], write 0x00 to 0x1D register!!\n");
			   buffer[0] = 0x00;
			   i2c_smbus_write_i2c_block_data(i2c_client, 0x1D, 1, &(buffer[0])); 			
		} 
		#endif	
		#ifndef TPD_NO_GPIO 	    // for mt6575T fpga early porting    
        if (tpd_show_version) {
            tpd_show_version = 0;
                        
            mt_set_gpio_mode(GPIO1, 0x00);
            mt_set_gpio_dir(GPIO1, GPIO_DIR_OUT);
            mt_set_gpio_pull_enable(GPIO1, GPIO_PULL_ENABLE);
            mt_set_gpio_pull_select(GPIO1, GPIO_PULL_UP);
            
            mt_set_gpio_out(GPIO1, GPIO_OUT_ZERO);
            msleep(100);            
                        
            buffer[0] = 0x01; // reset touch panel mode
            i2c_smbus_write_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
            msleep(200);
            
            buffer[0] = 0x10; // swith to system information mode
            i2c_smbus_write_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
            msleep(200);
            
            i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(buffer[0x0]));
            i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(buffer[0x8]));
            i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(buffer[0x10]));
            i2c_smbus_read_i2c_block_data(i2c_client, 0x18, 8, &(buffer[0x18]));
            printk("[mtk-tpd] Cypress Touch Panel ID %x.%x\n", buffer[0x07], buffer[0x08]);            
            printk("[mtk-tpd] Cypress Touch Panel Firmware Version %x.%x\n", buffer[0x15], buffer[0x16]); 
            buffer[0] = 0x04; // switch to operation mode
            i2c_smbus_write_i2c_block_data(i2c_client, 0x00, 1, &(buffer[0]));
            msleep(200);
                      
            mt_set_gpio_out(GPIO1, GPIO_OUT_ONE);            
            mt_set_gpio_mode(GPIO1, 0x01);
            mt_set_gpio_pull_enable(GPIO1, GPIO_PULL_ENABLE);
            mt_set_gpio_pull_select(GPIO1, GPIO_PULL_UP);           
            continue;
        }        
      #endif  
        i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(buffer[0]));
        i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(buffer[8]));
        i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(buffer[16]));
        i2c_smbus_read_i2c_block_data(i2c_client, 0x18, 8, &(buffer[24]));		
	if(tpd_debuglog==1)
	{
        TPD_DMESG("[mtk-tpd]HST_MODE  : %x\n", buffer[0]); 
        TPD_DMESG("[mtk-tpd]TT_MODE   : %x\n", buffer[1]); 
        TPD_DMESG("[mtk-tpd]TT_STAT   : %x\n", buffer[2]);
       // TPD_DEBUG("[mtk-tpd]TOUCH_ID  : %x\n", buffer[8]);
		TPD_DMESG("[mtk-tpd]TOUCH_12ID  : %x\n", buffer[8]);
		TPD_DMESG("[mtk-tpd]TOUCH_34ID  : %x\n", buffer[21]);
	}	
        
        finger_num = buffer[2] & 0x0f;
        
        if (finger_num == 0 && pre_num ==0) {
            msleep(10);
            tpd_flag = 0;
            pre_tt_mode = buffer[1];
	if(tpd_em_log==1)
		TPD_DMESG("[mtk-tpd], continue from finger number.\n");        
            continue;   
        }
        
        if (pre_tt_mode == buffer[1]) {
            msleep(5);
            tpd_flag = 0;
            pre_tt_mode = buffer[1];
	if(tpd_em_log==1)
		TPD_DMESG("[mtk-tpd], continue from TT MODE.\n");    
            continue;  
        }
        
        if (buffer[1] & 0x20) {
	if(tpd_em_log==1)
		TPD_DMESG("[mtk-tpd], buffer not ready.\n");    
            tpd_flag = 0;
            pre_tt_mode = buffer[1];
            continue; // buffer is not ready for use
        }   
        id1 = (buffer[8] & 0xf0) >> 4;
        id2 = (buffer[8] & 0x0f);
		id3 = (buffer[21] & 0xf0) >> 4;
		id4 = (buffer[21] & 0x0f);
                
//        if (id1 != 0xf) { 
	   if(finger_num>=1) {
            x1 = (((int)buffer[3]) << 8) + buffer[4]; 
            y1 = (((int)buffer[5]) << 8) + buffer[6]; 
            if(x1>2048 || y1>2048) {
	            tpd_flag = 0;
	            pre_tt_mode = buffer[1];  
	            continue;          		
            	}
            p1 = buffer[7];
            raw_x1 = x1; raw_y1 = y1;
            tpd_calibrate(&x1, &y1);
            tpd_down(raw_x1, raw_y1, x1, y1, p1);
            if(counter_pointer==0)
            	g_temptimerdiff=get_jiffies_64();//jiffies;
            if(x_min==0&&y_min==0&&x_max==0&&y_max==0) {
            		x_min = x1;
            		y_min = y1;
            		x_max = x1;
            		y_max = y1;
            	}
            if(x1<x_min)
            	x_min = x1;
            if(x1>x_max)
            	x_max = x1;
            if(y1<y_min)
            	y_min = y1;
            if(y1>y_max)
            	y_max = y1;
            counter_pointer++;
            if (time_after(jiffies, g_temptimerdiff + 100)){	//1s
            	TPD_DMESG("[mtk-tpd], x_min=%d, y_min=%d, x_max=%d, y_max=%d, counter_pointer=%d!!\n", x_min, y_min, x_max, y_max, counter_pointer);
            	x_min=0;
            	y_min=0;
            	x_max=0;
            	y_max=0;
            	counter_pointer=0;
            }
        }
        
//        if (id2 != 0xf || finger_num==2) {
	    if(finger_num>=2) {
            x2 = (((int)buffer[9]) << 8) + buffer[10]; 
            y2 = (((int)buffer[11]) << 8) + buffer[12]; 
            p2 = buffer[13];
            raw_x2 = x2; raw_y2 = y2;
            tpd_calibrate(&x2, &y2);
            tpd_down(raw_x2, raw_y2, x2, y2, p2);
        }
//	if(id3 != 0xf || finger_num==3) {
	    if(finger_num>=3) {
            x3 = (((int)buffer[16]) << 8) + buffer[17]; 
            y3= (((int)buffer[18]) << 8) + buffer[19]; 
            p3= buffer[20];
            raw_x3 = x3; raw_y3 = y3;
            tpd_calibrate(&x3, &y3);
            tpd_down(raw_x3, raw_y3, x3, y3, p3); 
		}

//	if(id4 != 0xf || finger_num==4) {
	    if(finger_num>=4) {
            x4 = (((int)buffer[22]) << 8) + buffer[23]; 
            y4= (((int)buffer[24]) << 8) + buffer[25]; 
            p4= buffer[26];
            raw_x4 = x4; raw_y4 = y4;
            tpd_calibrate(&x4, &y4);
            tpd_down(raw_x4, raw_y4, x4, y4, p4); 
		}	
        
	if(pre_num>=1 && pre_id1==0xf) {
		if(finger_num>=1 && id1==0xf) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger1 is still down!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger1 is up!!\n");
			tpd_up(raw_x1, raw_y1, x1, y1, p1);
		}		
		
	} else if(pre_num>=1 && pre_id1 !=0xf) {
		if(id1==pre_id1) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger1 is still down!!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger1 is up!\n");
			tpd_up(raw_x1, raw_y1, x1, y1, p1);
		}
	}
	
	if(pre_num>=2 && pre_id2==0xf) {
		if((finger_num>=2 && id2==0xf) || (finger_num==1 && id1==0xf)) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger2 is still down!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger2 is up!!\n");
			tpd_up(raw_x2, raw_y2, x2, y2, p2);
		}		
		
	} else if(pre_num>=2 && pre_id2 !=0xf) {
		if(id2==pre_id2 || id1==pre_id2) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger2 is still down!!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger2 is up!\n");
			tpd_up(raw_x2, raw_y2, x2, y2, p2);
		}
	}

	
	if(pre_num>=3 && pre_id3==0xf) {
		if((finger_num>=3 && id3==0xf) || (finger_num==2 && id2==0xf) || (finger_num==1 && id1==0xf)) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger3 is still down!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger3 is up!!\n");
			tpd_up(raw_x3, raw_y3, x3, y3, p3);
		}		
		
	} else if(pre_num>=3 && pre_id3 !=0xf) {
		if(id3==pre_id3 || id2==pre_id3 || id1==pre_id3) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger3 is still down!!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger3 is up!\n");
			tpd_up(raw_x3, raw_y3, x3, y3, p3);
		}
	}

	if(pre_num==4 && pre_id4==0xf) {
		if((finger_num>=4 && id4==0xf) || (finger_num==3 && id3==0xf) || (finger_num==2 && id2==0xf) || (finger_num==1 && id1==0xf)) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger4 is still down!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger4 is up!!\n");
			tpd_up(raw_x4, raw_y4, x4, y4, p4);
		}		
		
	} else if(pre_num==4 && pre_id4 !=0xf) {
		if(id4==pre_id4 || id3==pre_id4 || id2==pre_id4 || id1==pre_id4) {
			if(tpd_debuglog==1)
				TPD_DMESG("finger4 is still down!!\n");
		} else {
			if(tpd_debuglog==1)
				TPD_DMESG("finger4 is up!\n");
			tpd_up(raw_x4, raw_y4, x4, y4, p4);
		}
	}
		if(tpd_debuglog==1)	
			TPD_DMESG("pre_id1=%d, pre_id2=%d, pre_id3=%d, pre_id4=%d, id1=%d, id2=%d, id3=%d, id4=%d\n", pre_id1, pre_id2, pre_id3, pre_id4, id1, id2, id3, id4);				
        pre_id1 = id1; pre_id2 = id2; pre_id3 = id3; pre_id4 = id4; pre_tt_mode = buffer[1];
	pre_num = finger_num;
       
	if(tpd && tpd->dev && tpd_register_flag==1) {      
	        input_sync(tpd->dev);
	}
        
        i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &toggle);
        if((toggle & 0x80) == 0) 
            toggle = toggle | 0x80;
        else 
            toggle = toggle & (~0x80);
        i2c_smbus_write_i2c_block_data(i2c_client, 0x00, 1, &toggle); // switch the read toggle bit to do next sampling 
        tpd_flag = 0;
    #ifndef POLL_MODE
    } while (!kthread_should_stop());
    #else 
    }while(1);
    #endif
    return 0;
}

int tpd_local_init(void) 
{
	//tpd_debuglog = 1;
	if(tpd_debuglog==1) {
		TPD_DMESG("[mtk-tpd] %s\n", __FUNCTION__); 
	}
     if(i2c_add_driver(&tpd_i2c_driver)!=0) {
      TPD_DMESG("unable to add i2c driver.\n");
      return -1;
    }
    if(tpd_load_status == 0)
    {
    	TPD_DMESG("add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   
  
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
		TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		tpd_type_cap = 1;
    return 0;
}

/* Function to manage low power suspend */
void tpd_suspend(struct early_suspend *h)
{
	if(tpd_debuglog==1) {
		TPD_DMESG("[mtk-tpd] %s\n", __FUNCTION__); 
	}
  	tpd_halt = 1;	
	while(1){
		if(tpd_flag == 1) msleep(1000);
		else break;	
	}
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#ifndef TPD_NO_GPIO
    #ifdef TPD_HAVE_POWER_ON_OFF
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
    mdelay(1);
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);    
    #endif
#endif    
}

/* Function to manage power-on resume */
void tpd_resume(struct early_suspend *h) 
{
	if(tpd_debuglog==1) {
		TPD_DMESG("[mtk-tpd] %s\n", __FUNCTION__); 
	}
#ifndef TPD_NO_GPIO	
    #ifdef TPD_HAVE_POWER_ON_OFF
   // msleep(100);
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);    
    msleep(100);
    #endif

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
    tpd_halt = 0;
#endif    
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "cy8ctma300",
		.tpd_local_init = tpd_local_init,
		.suspend = tpd_suspend,
		.resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
		.tpd_have_button = 1,
#else
		.tpd_have_button = 0,
#endif		
};
/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
    printk("fwq MediaTek cy8ctma300 touch panel driver init\n");
    i2c_register_board_info(TPD_I2C_NUMBER, &i2c_tpd, 1);//modify I2C0 ==> I2C3
		if(tpd_driver_add(&tpd_device_driver) < 0)
			TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek cy8ctma300 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

