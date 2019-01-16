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

#include <linux/proc_fs.h>
#include <asm/uaccess.h>


#include "tpd.h"
#include <cust_eint.h>
#include <linux/jiffies.h>
#include "tpd_custom_tangleM32_16.h"
#include "tpd_calibrate.h"
#include <mach/mt_pm_ldo.h>

#ifndef TPD_NO_GPIO 
#include "cust_gpio_usage.h"
#endif



#define CHR_CON0	(0xF7000000+0x2FA00)
extern struct tpd_device *tpd;
extern int tpd_show_version;
extern int tpd_debuglog;
extern int tpd_register_flag;

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
//static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
#if 0
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
extern void mt65xx_eint_set_hw_debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 mt65xx_eint_set_sens(kal_uint8 eintno, kal_bool sens);
extern void mt65xx_eint_registration(kal_uint8 eintno, kal_bool Dbounce_En,
                                     kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
                                     kal_bool auto_umask);
#endif

//static int i2c_write_dummy( struct i2c_client *client, u16 addr );


static struct i2c_client *i2c_client = NULL;
static const struct i2c_device_id tpd_i2c_id[] = {{"tanglem32_16",0},{}};
static unsigned short force[] = {0, 0xB8, I2C_CLIENT_END,I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces,};
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("tanglem32_16", ( 0xB8 >> 1))};
struct i2c_driver tpd_i2c_driver = {                       
    .probe = tpd_i2c_probe,                                   
    .remove = tpd_i2c_remove,                           
    //.detect = tpd_i2c_detect,                           
    .driver.name = "tanglem32_16", 
    .id_table = tpd_i2c_id,                             
    .address_list = (const unsigned short*) forces,
}; 

#define C_I2C_FIFO_SIZE         8       /*according i2c_mt6575.c*/

/*
static int tangleM32_16_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf;
    int ret = 0;
	
    client->addr = client->addr& I2C_MASK_FLAG | I2C_WR_FLAG |I2C_RS_FLAG;
    buf = addr;
	ret = i2c_master_send(client, (const char*)&buf, 1<<8 | 1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        printk("tangleM32_16_read_byte_sr send command error!!\n");
        return -EFAULT;
    }

    *data = buf;
	client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}
*/

static int tangleM32_16_write_byte(struct i2c_client *client, u8 addr, u8 data)
{
    u8 buf[] = {addr, data};
    int ret = 0;

    ret = i2c_master_send(client, (const char*)buf, sizeof(buf));
    if (ret < 0) {
        printk("tangleM32_16_write_byte send command error!!\n");
        return -EFAULT;
    } else {
#if defined(tangleM32_16_DEBUG)    
        printk("%s(0x%02X)= %02X\n", __func__, addr, data);
#endif
    }
    return 0;
}


static int tangleM32_16_read_byte(struct i2c_client *client, u8 addr, u8 *data)
{
    u8 buf;
    int ret = 0;
    
    buf = addr;
    ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret < 0) {
        printk("tangleM32_16_read_byte send command error!!\n");
        return -EFAULT;
    }
    ret = i2c_master_recv(client, (char*)&buf, 1);
    if (ret < 0) {
        printk("tangleM32_16_read_byte reads data error!!\n");
        return -EFAULT;
    } else {
#if defined(tangleM32_16_DEBUG)    
        printk("%s(0x%02X) = %02X\n", __func__, addr, buf);    
#endif
    }
    *data = buf;
    return 0;
}

static int tangleM32_16_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    if (len == 1) {
        return tangleM32_16_read_byte(client, addr, data);
    } else {
        u8 beg = addr; 
        struct i2c_msg msgs[2] = {
            {
                .addr = client->addr,    .flags = 0,
                .len = 1,                .buf= &beg
            },
            {
                .addr = client->addr,    .flags = I2C_M_RD,
                .len = len,             .buf = data,
            }
        };
        int err;

        if (!client)
            return -EINVAL;
        else if (len > C_I2C_FIFO_SIZE) {        
            printk(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
            return -EINVAL;
        }

        err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
        if (err != 2) {
            printk("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
            err = -EIO;
        } else {
#if defined(tangleM32_16_DEBUG)        
            static char buf[128];
            int idx, buflen = 0;
            for (idx = 0; idx < len; idx++)
                buflen += snprintf(buf+buflen, sizeof(buf)-buflen, "%02X ", data[idx]);
            printk("%s(0x%02X,%2d) = %s\n", __func__, addr, len, buf);
#endif             
            err = 0;    /*no error*/
        }
        return err;
    }

}



/*
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mtk-tpd");
    return 0;
}
*/

static void setResolution(struct i2c_client *client)
{
    //int err =0;
	u8 buffer[10]={0};
	//u8 data[10]={0};
	
	//read eeprom
	/*
	tangleM32_16_write_byte(client,0x37,0x01);
	//firmware version
	buffer[1]= 0x01;
	buffer[0]= 0x39;
	i2c_master_send(client, buffer, 2);
	i2c_master_recv(client, data, 4);
	TPD_DMESG("firmware %x %x %x %x \n",data[0],data[1],data[2],data[3]);
	
	buffer[1]= 0x01;
	buffer[0]= 0x3D;
	i2c_master_send(client, buffer, 2);
	i2c_master_recv(client, data, 2);
	
	TPD_DMESG("x resolution %x %x  \n",data[0],data[1]);

	buffer[1]= 0x01;
	buffer[0]= 0x3F;
	i2c_master_send(client, buffer, 2);
	i2c_master_recv(client, data, 2);

	
	TPD_DMESG("y resolution %x %x  \n",data[0],data[1]);
	//dummy read
	buffer[0]=0x37;
	i2c_master_recv(client, buffer, 1);
	msleep(20);
	*/
	//write eeprom
	tangleM32_16_write_byte(client,0x37,0x02);
	//write x resolution
	
	buffer[1]= 0x01;
	buffer[0]= 0x3D;
	buffer[2]= 0x1c;//data//02
	i2c_master_send(client, buffer, 3);
	
	buffer[1]= 0x01;
	buffer[0]= 0x3E;
	buffer[2]= 0x02;//data//1c
	i2c_master_send(client, buffer, 3);

	msleep(10);
	//write y resolution
	buffer[1]= 0x01;
	buffer[0]= 0x3F;
	buffer[2]= 0xc0;//data//c0
	i2c_master_send(client, buffer, 3);
	msleep(10);
	buffer[1]= 0x01;
	buffer[0]= 0x40;
	buffer[2]= 0x03;//data//03
	i2c_master_send(client, buffer, 3);
	msleep(10);
	//dummy read
	buffer[0]=0x37;
	i2c_master_recv(client, buffer, 1);

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(100);  
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(75);  
	//calibration
	tangleM32_16_write_byte(client,0x37,0x03);
	msleep(500);
	
	
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {             
    int err = 0;
    char buffer[2];
	u8 data[10]={0};
	int i =0;
	//int ret =0;
    i2c_client = client;
    
    #ifdef TPD_NO_GPIO
    u16 temp;
    temp = *(volatile u16 *) TPD_RESET_PIN_ADDR;
    temp = temp | 0x40;
    *(volatile u16 *) TPD_RESET_PIN_ADDR = temp;
    #endif
    
    #ifndef TPD_NO_GPIO 

    TPD_DMESG(TPD_DEVICE " power on !!\n");
	  //power on, need confirm with SA
#ifdef TPD_POWER_SOURCE_CUSTOM
		  hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#else
		  hwPowerOn(MT65XX_POWER_LDO_VGP2, VOL_2800, "TP");
#endif
#ifdef TPD_POWER_SOURCE_1800
		  hwPowerOn(TPD_POWER_SOURCE_1800, VOL_1800, "TP");
#endif

	
	msleep(100);
	
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
    msleep(10);  
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
   
    mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);

    msleep(10);  
    #endif 

   // msleep(100);
	
	//mt65xx_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //mt65xx_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	 msleep(100);
  
	device_init_wakeup(&client->dev, 1);
    TPD_DMESG("tangleM32........................\n" );
	i2c_client->addr = (i2c_client->addr & I2C_MASK_FLAG )|(I2C_ENEXT_FLAG);
	//i2c_client->timing = 1;
	//eeprom write
	setResolution(i2c_client);
	
	//read power mode
	tangleM32_16_read_block(i2c_client ,20,data,1);
	TPD_DMESG("tangleM32 power mode =%x \n",data[0] );
	//set and read INT mode
	//tangleM32_16_write_byte(i2c_client ,21,0x0a);
	tangleM32_16_read_block(i2c_client ,21,data,1);
	TPD_DMESG("tangleM32 INT mode =%x \n",data[0] );
	//read int width
	tangleM32_16_read_block(i2c_client ,22,data,1);
	TPD_DMESG("tangleM32 INT width =%x \n",data[0] );

	//read version
	
	 tangleM32_16_read_block(i2c_client ,48,data,4);
	 
     for(i=0;i<4;i++)
     {
	    TPD_DMESG("[mtk-tpd version], data[48+%d]=%x \n",i,data[i]);
     }

	 tangleM32_16_read_byte(i2c_client ,52,buffer);
	 TPD_DMESG("[mtk-tpd sub version firmware]   %x \n",buffer[0] );
	 
	
    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }

	tpd_load_status = 1;
    
    return 0;
}

void tpd_eint_interrupt_handler(void) { 
	TPD_DMESG("[mtk-tpd], %s\n", __FUNCTION__);
    TPD_DEBUG_PRINT_INT; tpd_flag=1; wake_up_interruptible(&waiter);
} 
static int tpd_i2c_remove(struct i2c_client *client) {return 0;}

void tpd_down(int raw_x, int raw_y, int x, int y, int p) {
	if(tpd && tpd->dev && tpd_register_flag==1) {
    input_report_abs(tpd->dev, ABS_PRESSURE, 128);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 128);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("D[%4d %4d %4d]\n", x, y, p);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
  }  
}

void tpd_up(int raw_x, int raw_y, int x, int y, int p) {
	if(tpd && tpd->dev && tpd_register_flag==1) {
    input_report_abs(tpd->dev, ABS_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_WIDTH_MAJOR, 0);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    input_mt_sync(tpd->dev);
    TPD_DEBUG("U[%4d %4d %4d]\n", x, y, 0);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 0);
  }  
}

static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    //int x, y, id, size, finger_num = 0;
    //static u8 buffer[ TPD_POINT_INFO_LEN*TPD_MAX_POINTS ];
    //static char buf_status;
    //static u8 id_mask = 0;
    //u8 cur_mask;
    //int idx;
	//int z = 50;
	//int w = 15;
  	//unsigned char Rdbuf[10],Wrbuf[1];
	int ret;
	int posx1, posy1;
	//unsigned char touching = 0,fingerid;
	//struct sched_param param = { .sched_priority = RTPM_PRIO_TPD }; 
    //static int x1, y1, x2, y2, raw_x1, raw_y1, raw_x2, raw_y2;
    //int temp_x1 = x1, temp_y1 = y1, temp_raw_x1 = raw_x1, temp_raw_y1 = raw_y1;
	//int i =0;
	char buffer[10];
    int touching=0;

	memset(buffer, 0, sizeof(buffer));
	//memset(Rdbuf, 0, sizeof(Rdbuf));

	//Wrbuf[0] = 0;
	
#ifdef TPD_CONDITION_SWITCH
    u8 charger_plug = 0;
    u8 *cfg;
    u32 temp;    
#endif

    sched_setscheduler(current, SCHED_RR, &param); 

    do
    {
        set_current_state(TASK_INTERRUPTIBLE);

        while ( tpd_halt )
        {
            tpd_flag = 0;
            msleep(20);
        }

		TPD_DMESG("[mtk-tpd] %s: wait for touch event \n", __FUNCTION__);

        wait_event_interruptible(waiter, tpd_flag != 0);
        
        tpd_flag = 0;
        TPD_DEBUG_SET_TIME;
        set_current_state(TASK_RUNNING); 
        
        touching =0;
        ret = tangleM32_16_read_block(i2c_client ,0x00,buffer,8);
        if(ret)
        {
           TPD_DMESG("[mtk-tpd] tangleM32_16_read_block error ret =%d \n" ,ret);
        }	

	    posx1 = ((buffer[3] << 8) | buffer[2]);
	    posy1 = ((buffer[5] << 8) | buffer[4]);
	    //posx2 = ((Rdbuf[7] << 8) | Rdbuf[6]);
	    // posy2 = ((Rdbuf[9] << 8) | Rdbuf[8]);
		
		//fingerid = Rdbuf[0]&0x30;
	    touching = buffer[0]&0x03;

		printk("touching:%-3d,,x1:%-6d,y1:%-6d\n",touching, posx1, posy1);
	
        if (touching == 1) 
		{
		   //tpd_calibrate(&posx1,&posy1);
		  // posx1= posx1*540/480;
		  // posy1= (-posy1 + 800)*960/800;
		   posy1= (-posy1 + 960);
		   TPD_DMESG("[mtk-tpd] after mappingx1:%-6d,y1:%-6d \n",posx1,posy1);
		   
		   tpd_down(0, 0, posx1, posy1, 1);
		}
		else if(touching == 2)
		{
		    TPD_DMESG("[mtk-tpd] touching == 2!!!!! we have no handler \n");
	    }
		else
		{
		   tpd_up(0, 0, posx1, posy1, 0);
		}
		
		input_sync(tpd->dev);
		
    } while ( !kthread_should_stop() ); 

    return 0;
}

int tpd_local_init(void) 
{
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
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

    #ifdef TPD_HAVE_POWER_ON_OFF
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);    
    #endif
}

/* Function to manage power-on resume */
void tpd_resume(struct early_suspend *h) 
{
	if(tpd_debuglog==1) {
		TPD_DMESG("[mtk-tpd] %s\n", __FUNCTION__); 
	}
    #ifdef TPD_HAVE_POWER_ON_OFF
    msleep(100);
    mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    msleep(1);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);    
    msleep(100);
    #endif

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
    tpd_halt = 0;
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "tangleM32_16",
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
	printk("MediaTek tangleM32_16 touch panel driver init\n");
	
	i2c_register_board_info(0, &i2c_tpd, 1);
		if(tpd_driver_add(&tpd_device_driver) < 0)
			TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek tangleM32_16 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

