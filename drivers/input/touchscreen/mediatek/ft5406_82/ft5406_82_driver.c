
//#define TPD_HAVE_BUTTON

#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <mach/mt_pm_ldo.h>

#ifdef TPD_GPT_TIMER_RESUME
#include <mach/hardware.h>
#include <mach/mt_gpt.h>
#include <linux/timer.h>
#endif
#include <mach/mt_boot.h>
#ifdef TPD_PROXIMITY
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#endif

#ifdef TPD_PROXIMITY

struct hwmsen_object obj_ps;
#define TPD_PROXIMITY_VALID_REG                   0x01
#define TPD_PROXIMITY_ENABLE_REG                  0xB0
static u8 tpd_proximity_flag = 0;
static u8 tpd_proximity_detect = 1;   //0-->close ; 1--> far away
#endif

#include "tpd_custom_ft5406_82.h"

#include "cust_gpio_usage.h"


#define TPD_POWER_SOURCE		MT6323_POWER_LDO_VGP1


extern struct tpd_device *tpd;
 
static struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
 
static void tpd_eint_interrupt_handler(void);
  
extern void mt_eint_unmask(unsigned int line);
extern void  mt_eint_mask(unsigned int line);
extern void mt_eint_set_polarity(unsigned int eint_num, unsigned int pol);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern void mt_eint_registration(unsigned int eint_num, unsigned int flag, 
              void (EINT_FUNC_PTR) (void), unsigned int is_auto_umask);

extern int tpd_type_cap;

 
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
 
static int tpd_flag = 0;
static int point_num = 0;
static int p_point_num = 0;

#define TPD_CLOSE_POWER_IN_SLEEP

#define TPD_OK 0
//register define

#define DEVICE_MODE 0x00
#define GEST_ID 0x01
#define TD_STATUS 0x02
#define FW_ID_ADDR	0xA6

//register define

#define AGOLD_FINGER_NUM_MAX	5 

#if defined(AGOLD_SUPPORT_AUTO_UPG)
#define CFG_SUPPORT_AUTO_UPG
#endif

struct touch_info {
    int y[AGOLD_FINGER_NUM_MAX];
    int x[AGOLD_FINGER_NUM_MAX];
    int p[AGOLD_FINGER_NUM_MAX];
    int count;
};
 
static const struct i2c_device_id tpd_id[] = {{"ft5406_82",0},{}};

//unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
//static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO("ft5406_82", (0x70>>1))};

static int boot_mode = 0; 

#ifdef TPD_HAVE_BUTTON
extern void tpd_button(unsigned int x, unsigned int y, unsigned int down); 
#if 0
#if 1
#define TPD_KEYS {KEY_HOME,KEY_MENU,KEY_BACK,KEY_SEARCH}
#define TPD_KEYS_DIM {{30,850,60,100},{180,850,60,100},{320,850,60,100},{450,850,60,100}}
#define TPD_KEY_COUNT 4
#else
#define TPD_KEYS {KEY_HOME,KEY_MENU,KEY_BACK}
#define TPD_KEYS_DIM {{80,850,60,100},{240,850,60,100},{400,850,60,100}}
#define TPD_KEY_COUNT 3
#endif
#endif
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif
 
/*
static struct i2c_driver tpd_i2c_driver = {
	.driver.name = "mtk-tpd",
	.probe = tpd_probe,
	.remove = __devexit_p(tpd_remove),
	.id_table = tpd_id,
	.detect = tpd_detect,
	.address_list = (const unsigned short*) forces,
};*/

 static struct i2c_driver tpd_i2c_driver = {
  .driver = {
	 .name = "mtk-tpd",//.name = TPD_DEVICE,
//	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = tpd_remove,
  .id_table = tpd_id,
  .detect = tpd_detect,
//  .address_data = &addr_data,
 };

 
#if 0 //[Agold][Talcon.Hu]
static unsigned short i2c_addr[] = {0x72}; 
#endif  

static  void tpd_down(int x, int y, int p) {

#ifdef TPD_HAVE_BUTTON
	if(MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90)
    {
		if(boot_mode!=NORMAL_BOOT && x>=TPD_RES_Y) 
		{ 
			int temp;
			temp = y;
			y = x;
			x = TPD_RES_X-temp;
			tpd_button(x, y, 1);
			return;
		}
		else if(boot_mode!=NORMAL_BOOT && y>=TPD_RES_Y)
		{
			tpd_button(x, y, 1);
			return;		
		}
	}
    else
	{
		if(boot_mode!=NORMAL_BOOT && y>=TPD_RES_Y) 
		{ 
			tpd_button(x, y, 1);
			return;
		}
	}
#endif

	if(MTK_LCM_PHYSICAL_ROTATION == 270 || MTK_LCM_PHYSICAL_ROTATION == 90) 
	{
		int temp;

		temp = y;
		y = x;
		x = TPD_RES_X-temp;
		TPD_DEBUG("rot cinfo->x = %d, cinfo->y = %d, cinfo->p = %d\n",x, y, p);	
	}

	// input_report_abs(tpd->dev, ABS_PRESSURE, p);
	 input_report_key(tpd->dev, BTN_TOUCH, 1);
	 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 1);

	if(MTK_LCM_PHYSICAL_ROTATION == 180 && y <= CONFIG_LCM_HEIGHT && boot_mode ==NORMAL_BOOT)
	{
	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, CONFIG_LCM_WIDTH-x);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, CONFIG_LCM_HEIGHT-y);
	}
	else
	{
	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	}

	 input_mt_sync(tpd->dev);
	 //TPD_DOWN_DEBUG_TRACK(x,y);
	 return;
 }
 
 static  void tpd_up(int x, int y,int p) {
	 //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
		 //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
		 //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //printk("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 //TPD_UP_DEBUG_TRACK(x,y);
		 return;
 }


#ifdef TPD_PROXIMITY
static u8 tpd_get_ps_value(void)
{
	 u8 proximity_status = 0;
	if (tpd_proximity_flag == 1 && obj_ps.polling == 1)  //polling mode 
    {
		i2c_smbus_read_i2c_block_data(i2c_client, 0x01, 1, &proximity_status);
        printk("REG INDEX[0x01]:0x%02X\n", proximity_status);

        if (proximity_status == 0xc0)                //proximity or large touch detect,enable hwm_sensor.
        {
            tpd_proximity_detect = 0;
        }
        else
        {
            tpd_proximity_detect = 1;
        }

        //get raw data
        printk(" ps change\n");
        printk("PROXIMITY STATUS:0x%02X\n", tpd_proximity_detect);
    }

    return tpd_proximity_detect;
}

static u8 tpd_enable_ps(s32 enable)
{
    u8  state;
    s32 ret = -1;
	 //struct hwmsen_object obj_ps;
    if (enable)
    {
        state = 1;
        tpd_proximity_flag = 1;
		if(obj_ps.polling)
		{
			printk("[mcz]\n");
			//mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); 
		}	
		else
		{
			mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		}
        printk("[Agold spl] TPD proximity function to be on.\n");
    }
    else
    {
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
        state = 0;
        tpd_proximity_flag = 0;
        printk("[Agold spl] TPD proximity function to be off.\n");
    }

    ret = i2c_smbus_write_i2c_block_data(i2c_client, TPD_PROXIMITY_ENABLE_REG, 1, &state);

    if (ret < 0)
    {
        printk("TPD %s proximity cmd failed.", state ? "enable" : "disable");
        return ret;
    }

    printk("[Agold spl] TPD proximity function %s success.\n", state ? "enable" : "disable");
    return 0;
}

s32 tpd_ps_operate(void *self, u32 command, void *buff_in, s32 size_in,
                   void *buff_out, s32 size_out, s32 *actualout)
{
    s32 err = 0;
    s32 value;
    hwm_sensor_data *sensor_data;

    switch (command)
    {
        case SENSOR_DELAY:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                printk("Set delay parameter error!");
                err = -EINVAL;
            }

            // Do nothing
            break;

        case SENSOR_ENABLE:
            if ((buff_in == NULL) || (size_in < sizeof(int)))
            {
                printk("Enable sensor parameter error!");
                err = -EINVAL;
            }
            else
            {
                value = *(int *)buff_in;
                err = tpd_enable_ps(value);
            }

            break;

        case SENSOR_GET_DATA:
			printk("[Agold spl] SENSOR_GET_DATA .\n");
            if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data)))
            {
                printk("Get sensor data parameter error!");
                err = -EINVAL;
            }
            else
            {
                sensor_data = (hwm_sensor_data *)buff_out;
                sensor_data->values[0] = tpd_get_ps_value();
				printk("[Agold spl] SENSOR_GET_DATA . sensor_data->values[0] = %d\n", sensor_data->values[0]);
                sensor_data->value_divide = 1;
                sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
            }

            break;

        default:
            printk("proxmy sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }

    return err;
}
#endif

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
{
	int i = 0;
	char data[32] = {0};

	u16 high_byte,low_byte;

#ifdef TPD_PROXIMITY
    s32 err = 0;
    hwm_sensor_data sensor_data;
    u8 proximity_status;
	 //struct hwmsen_object obj_ps;
#endif

	p_point_num = point_num;
	memcpy(pinfo, cinfo, sizeof(struct touch_info));
	memset(cinfo, 0, sizeof(struct touch_info));

	i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 8, &(data[0]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x08, 8, &(data[8]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x10, 8, &(data[16]));
	i2c_smbus_read_i2c_block_data(i2c_client, 0x18, 8, &(data[24]));
	//TPD_DEBUG("FW version=%x]\n",data[24]);

//	TPD_DEBUG("received raw data from touch panel as following:\n");
//	TPD_DEBUG("[data[0]=%x,data[1]= %x ,data[2]=%x ,data[3]=%x ,data[4]=%x ,data[5]=%x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
//	TPD_DEBUG("[data[9]=%x,data[10]= %x ,data[11]=%x ,data[12]=%x]\n",data[9],data[10],data[11],data[12]);
//	TPD_DEBUG("[data[15]=%x,data[16]= %x ,data[17]=%x ,data[18]=%x]\n",data[15],data[16],data[17],data[18]);


#ifdef TPD_PROXIMITY

    if (tpd_proximity_flag == 1 && obj_ps.polling == 0) // interrupt mode
    {
        // proximity_status = point_data[GTP_ADDR_LENGTH];
		i2c_smbus_read_i2c_block_data(i2c_client, 0x01, 1, &proximity_status);
        printk("REG INDEX[0x01]:0x%02X\n", proximity_status);

        if (proximity_status == 0xc0)                //proximity or large touch detect,enable hwm_sensor.
        {
            tpd_proximity_detect = 0;
        }
        else
        {
            tpd_proximity_detect = 1;
        }

        //get raw data
        printk(" ps change\n");
        printk("PROXIMITY STATUS:0x%02X\n", tpd_proximity_detect);
        //map and store data to hwm_sensor_data
        sensor_data.values[0] = tpd_get_ps_value();
        sensor_data.value_divide = 1;
        sensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
        //report to the up-layer
        u32 ret = 0; //mucz add
        ret = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data);

        if (ret)
        {
            printk("Call hwmsen_get_interrupt_data fail = %d\n", err);
        }
    }

#endif

	/* Device Mode[2:0] == 0 :Normal operating Mode*/
	if((data[0] & 0x70) != 0) return false; 

	/*get the number of the touch points*/
	point_num= data[2] & 0x0f;
	
	TPD_DEBUG("point_num =%d\n",point_num);

	if(AGOLD_FINGER_NUM_MAX < point_num)
	{
		TPD_DEBUG("point_num is error\n");
		return false;
	}
	
//	if(point_num == 0) return false;

	TPD_DEBUG("Procss raw data...\n");

		
	for(i = 0; i < point_num; i++)
	{
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 

       	/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] =high_byte |low_byte;

		//cinfo->x[i] =  cinfo->x[i] * 480 >> 11; //calibra
		/*get the Y coordinate, 2 bytes*/		
		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;
		//cinfo->y[i]=  cinfo->y[i] * 800 >> 11;	
		cinfo->count++;
	
#if 0//defined(TPD_RES_X) && defined(AGOLD_TP_WIDTH) /*[Agold][Talcon.Hu]*/
		cinfo->x[i] = cinfo->x[i]*540/480;//AGOLD_TP_WIDTH/TPD_RES_X;
#endif
#if 0//defined(TPD_RES_Y) && defined(AGOLD_TP_HEIGHT) /*[Agold][Talcon.Hu]*/
		cinfo->y[i] = cinfo->y[i]*960/854;//AGOLD_TP_HEIGHT/TPD_RES_Y;
#endif

		TPD_DEBUG(" cinfo->x[i=%d] = %d, cinfo->y[i] = %d, cinfo->p[i] = %d\n", i,cinfo->x[i], cinfo->y[i], cinfo->p[i]);
	}

	//TPD_DEBUG(" cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);	
	//TPD_DEBUG(" cinfo->x[1] = %d, cinfo->y[1] = %d, cinfo->p[1] = %d\n", cinfo->x[1], cinfo->y[1], cinfo->p[1]);		
	//TPD_DEBUG(" cinfo->x[2]= %d, cinfo->y[2]= %d, cinfo->p[2] = %d\n", cinfo->x[2], cinfo->y[2], cinfo->p[2]);	
	  
#if defined(AGOLD_SMT_TP_CONFIG)
	for(i = 0; i < point_num; i++)
	{
		cinfo->x[i] = cinfo->x[i] *36/51;//5.1CM/5.4CM
		cinfo->y[i] = cinfo->y[i] *27/37;//7.4CM/9.0CM
	}
#endif
	  
	return true;
};

static int touch_event_handler(void *unused)
{
	struct touch_info cinfo, pinfo;
	int i = 0;

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);
 
	 do
	 {
		//mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); 
		set_current_state(TASK_INTERRUPTIBLE); 
		wait_event_interruptible(waiter,tpd_flag!=0);
	
		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		if (tpd_touchinfo(&cinfo, &pinfo))
		{
			TPD_DEBUG("point_num = %d\n",point_num);
			  
			if(point_num >0) 
			{
				#if 0
				tpd_down(cinfo.x[0], cinfo.y[0], 1);
				if(point_num>1)
				{
					tpd_down(cinfo.x[1], cinfo.y[1], 1);
					if(point_num >2) 
						tpd_down(cinfo.x[2], cinfo.y[2], 1);
				}
				#else
				while(i<point_num)
				{
					tpd_down(cinfo.x[i], cinfo.y[i], 1);
					i++;
				}
				i = 0;
				#endif
				TPD_DEBUG("press --->\n");
				
		    	} 
			else  
			{
				TPD_DEBUG("release --->\n"); 

				if(p_point_num >1)
				{
					i = 0;
					while(i<p_point_num){
						tpd_up(pinfo.x[i], pinfo.y[i], 1);
						i++;
					}
				}
				else
				{
					tpd_up(pinfo.x[0], pinfo.y[0], 1);
				}
				i = 0;

			#ifdef TPD_HAVE_BUTTON
				if(boot_mode!=NORMAL_BOOT && tpd->btn_state) 
				{ 
					tpd_button(pinfo.x[0], pinfo.y[0], 0);
				}
			#endif

            		}

			input_sync(tpd->dev);
        	}

 	}while(!kthread_should_stop());

	return 0;
}

#if defined(CFG_SUPPORT_AUTO_UPG) || defined(AGOLD_CTP_FW_UPGRATE)

#define FT5x06_REG_FW_VER 0xA6
int upgrate_percent = 0;
static struct i2c_client *this_client;

static int ft5x0x_i2c_rxdata(char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

    //msleep(1);
	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		TPD_DEBUG("msg %s i2c read error: %d\n", __func__, ret);
	
	return ret;
}

static int ft5x0x_i2c_txdata(char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
			.timing = 100,
		},
	};

   	//msleep(1);
	ret = i2c_transfer(this_client->adapter, msg, 1);
	if (ret < 0)
		TPD_DEBUG("%s i2c write error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_write_reg(u8 addr, u8 para)
{
    u8 buf[3];
    int ret = -1;

    buf[0] = addr;
    buf[1] = para;
    ret = ft5x0x_i2c_txdata(buf, 2);
    if (ret < 0) {
        TPD_DEBUG("write reg failed! %#x ret: %d", buf[0], ret);
        return -1;
    }
    
    return 0;
}

static int ft5x0x_read_reg(u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[2];

    //
	buf[0] = addr;    //register address
	
	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;
	msgs[0].timing = 100;
	msgs[1].addr = this_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = buf;
	msgs[1].timing = 100;

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret < 0)
		TPD_DEBUG("[Agold] msg %s i2c read error: %d\n,Line=%d", __func__, ret,__LINE__);
	else
		TPD_DEBUG("[Agolprobed] i2c read success.line=%d\n",__LINE__);

	*pdata = buf[0];
	TPD_DEBUG("[Agold] i2c read success.buf[0]=%d,buf[1]=%d\n",buf[0],buf[1]);
	return ret;
  
}

static unsigned char ft5x0x_read_fw_ver(void)
{
	unsigned char ver;
	ft5x0x_read_reg(FT5x06_REG_FW_VER, &ver);
	TPD_DEBUG("[Agold] ver =0x%x,\n",ver);
	return(ver);
}

typedef enum
{
    ERR_OK,
    ERR_MODE,
    ERR_READID,
    ERR_ERASE,
    ERR_STATUS,
    ERR_ECC,
    ERR_DL_ERASE_FAIL,
    ERR_DL_PROGRAM_FAIL,
    ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

typedef unsigned char         FTS_BYTE;     //8 bit
typedef unsigned short        FTS_WORD;    //16 bit
typedef unsigned int          FTS_DWRD;    //16 bit
typedef unsigned char         FTS_BOOL;    //8 bit

typedef struct _FTS_CTP_PROJECT_SETTING_T
{
    unsigned char uc_i2C_addr;             //I2C slave address (8 bit address)
    unsigned char uc_io_voltage;           //IO Voltage 0---3.3v;	1----1.8v
    unsigned char uc_panel_factory_id;     //TP panel factory ID
}FTS_CTP_PROJECT_SETTING_T;

#define FTS_NULL                0x0
#define FTS_TRUE                0x01
#define FTS_FALSE              0x0

#define I2C_CTPM_ADDRESS       0x70


void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}


/*
[function]: 
    callback: read data from ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[out]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_read_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    
    ret=i2c_master_recv(this_client, pbt_buf, dw_lenth);

    if(ret<=0)
    {
        TPD_DEBUG("[FTS]i2c_read_interface error\n");
        return FTS_FALSE;
    }
  
    return FTS_TRUE;
}

/*
[function]: 
    callback: write data to ctpm by i2c interface,implemented by special user;
[parameters]:
    bt_ctpm_addr[in]    :the address of the ctpm;
    pbt_buf[in]        :data buffer;
    dw_lenth[in]        :the length of the data buffer;
[return]:
    FTS_TRUE     :success;
    FTS_FALSE    :fail;
*/
FTS_BOOL i2c_write_interface(FTS_BYTE bt_ctpm_addr, FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    int ret;
    ret=i2c_master_send(this_client, pbt_buf, dw_lenth);
    if(ret<=0)
    {
        TPD_DEBUG("[FTS]i2c_write_interface error line = %d, ret = %d\n", __LINE__, ret);
        return FTS_FALSE;
    }

    return FTS_TRUE;
}

/*
[function]: 
    send a command to ctpm.
[parameters]:
    btcmd[in]        :command code;
    btPara1[in]    :parameter 1;    
    btPara2[in]    :parameter 2;    
    btPara3[in]    :parameter 3;    
    num[in]        :the valid input parameter numbers, if only command code needed and no parameters followed,then the num is 1;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL cmd_write(FTS_BYTE btcmd,FTS_BYTE btPara1,FTS_BYTE btPara2,FTS_BYTE btPara3,FTS_BYTE num)
{
    FTS_BYTE write_cmd[4] = {0};

    write_cmd[0] = btcmd;
    write_cmd[1] = btPara1;
    write_cmd[2] = btPara2;
    write_cmd[3] = btPara3;
    return i2c_write_interface(I2C_CTPM_ADDRESS, write_cmd, num);
}

/*
[function]: 
    write data to ctpm , the destination address is 0.
[parameters]:
    pbt_buf[in]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_write(FTS_BYTE* pbt_buf, FTS_DWRD dw_len)
{
    
    return i2c_write_interface(I2C_CTPM_ADDRESS, pbt_buf, dw_len);
}

/*
[function]: 
    read out data from ctpm,the destination address is 0.
[parameters]:
    pbt_buf[out]    :point to data buffer;
    bt_len[in]        :the data numbers;    
[return]:
    FTS_TRUE    :success;
    FTS_FALSE    :io fail;
*/
FTS_BOOL byte_read(FTS_BYTE* pbt_buf, FTS_BYTE bt_len)
{
    return i2c_read_interface(I2C_CTPM_ADDRESS, pbt_buf, bt_len);
}


/*
[function]: 
    burn the FW to ctpm.
[parameters]:(ref. SPEC)
    pbt_buf[in]    :point to Head+FW ;
    dw_lenth[in]:the length of the FW + 6(the Head length);    
    bt_ecc[in]    :the ECC of the FW
[return]:
    ERR_OK        :no error;
    ERR_MODE    :fail to switch to UPDATE mode;
    ERR_READID    :read id fail;
    ERR_ERASE    :erase chip fail;
    ERR_STATUS    :status error;
    ERR_ECC        :ecc error.
*/


#define    FTS_PACKET_LENGTH        2

static unsigned char CTPM_FW[]=
{
	#if defined(AGOLD_SUPPORT_AUTO_UPG)
	#include "ft_app.i"
	#else
		//#error "please config right FW!"
	#endif
};

E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
	msleep(300);//mcz add 20130514,about read ID1 and ID2
    FTS_BYTE reg_val[2] = {0};
    FTS_DWRD i = 0;

    FTS_DWRD  packet_number;
    FTS_DWRD  j;
    FTS_DWRD  temp;
    FTS_DWRD  lenght;
    FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
    FTS_BYTE  auc_i2c_write_buf[10];
    FTS_BYTE bt_ecc;
    int      i_ret;

    /*********Step 1:Reset  CTPM *****/
    /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
    msleep(50);
     /*write 0x55 to register 0xfc*/
    ft5x0x_write_reg(0xfc,0x55);
    TPD_DEBUG("[FTS] Step 1: Reset CTPM test\n");
   
    msleep(30);   


    /*********Step 2:Enter upgrade mode *****/
    auc_i2c_write_buf[0] = 0x55;
    auc_i2c_write_buf[1] = 0xaa;
    do
    {
        i ++;
        i_ret = ft5x0x_i2c_txdata(auc_i2c_write_buf, 2);
        msleep(5);
    }while(i_ret <= 0 && i < 5 );

    TPD_DEBUG("[FTS] step 2: Enter upgrade mode\n");

    /*********Step 3:check READ-ID***********************/        
    cmd_write(0x90,0x00,0x00,0x00,4);
    byte_read(reg_val,2);
    if (reg_val[0] == 0x79 && ((reg_val[1] == 0x3) ||(reg_val[1] == 0x7)))
    {
        TPD_DEBUG("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
    }
    else
    {
	TPD_DEBUG("22222[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	return ERR_READID;
	
        //i_is_new_protocol = 1;
    }

    cmd_write(0xcd,0x0,0x00,0x00,1);
    byte_read(reg_val,1);
    TPD_DEBUG("[FTS] bootloader version = 0x%x\n", reg_val[0]);

     /*********Step 4:erase app and panel paramenter area ********************/
    cmd_write(0x61,0x00,0x00,0x00,1);  //erase app area
    msleep(1500); 
    cmd_write(0x63,0x00,0x00,0x00,1);  //erase panel parameter area
    msleep(100);
    TPD_DEBUG("[FTS] Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    TPD_DEBUG("[FTS] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;

	printk("[Agold spl] packet_number = %d\n",packet_number);
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
        
        byte_write(&packet_buf[0],FTS_PACKET_LENGTH + 6);
        //mdelay(1);
	udelay(200);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              TPD_DEBUG("[FTS] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }

		upgrate_percent = (j* 100)/packet_number ; //spl add
		//printk("[Agold spl] j = %d ,upgrate_percent = %d.\n", j, upgrate_percent);
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        byte_write(&packet_buf[0],temp+6);    
        msleep(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];

        byte_write(&packet_buf[0],7);  
        msleep(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    cmd_write(0xcc,0x00,0x00,0x00,1);
    byte_read(reg_val,1);
    TPD_DEBUG("[FTS] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
        return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    cmd_write(0x07,0x00,0x00,0x00,1);

    msleep(300);  //make sure CTP startup normally
    
    return ERR_OK;
}

int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    TPD_DEBUG("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);  
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);  //write command to start calibration
    msleep(300);
    for(i=0;i<100;i++)
    {
        //ft5x0x_read_reg(0,&uc_temp);
		i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0)  //return to normal mode, calibration finish
        {
            break;
        }
        msleep(200);
        TPD_DEBUG("[FTS] waiting calibration %d\n",i);
        
    }
    TPD_DEBUG("[FTS] calibration OK.\n");
    
    msleep(300);
    ft5x0x_write_reg(0, 0x40);  //goto factory mode
    msleep(100);   //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);  //store CLB result
    msleep(300);
    ft5x0x_write_reg(0, 0x0); //return to normal mode 
    msleep(300);
    TPD_DEBUG("[FTS] store CLB result OK.\n");

	
    return 0;
}

int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = FTS_NULL;
   int i_ret;
    
   //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
       TPD_DEBUG("[FTS] upgrade failed i_ret = %d.\n", i_ret);
       //error handling ...
       //TBD
   }
   else
   {
       TPD_DEBUG("[FTS] upgrade successfully.\n");
       fts_ctpm_auto_clb();  //start auto CLB
   }

   return i_ret;
}

unsigned char fts_ctpm_get_i_file_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
    {
        //TBD, error handling?
        return 0xff; //default value
    }
}

static int fts_ctpm_auto_upg(void)
{
    unsigned char uc_host_fm_ver = 0;
    unsigned char uc_tp_fm_ver = 0;
    int           i_ret;

    uc_tp_fm_ver = ft5x0x_read_fw_ver();
	TPD_DEBUG("[FTS] 11111 --uc_tp_fm_ver = 0x%x\n",uc_tp_fm_ver);
	if((i2c_smbus_read_i2c_block_data(i2c_client, 0xA6, 1, &uc_tp_fm_ver))>= 0)
	{
		TPD_DEBUG("[FTS] 22222 --uc_tp_fm_ver = 0x%x\n",uc_tp_fm_ver);
	}
    uc_host_fm_ver = fts_ctpm_get_i_file_ver();
	 TPD_DEBUG("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
            uc_tp_fm_ver, uc_host_fm_ver);
    if ( uc_tp_fm_ver == 0xa6  ||   //the firmware in touch panel maybe corrupted
         uc_tp_fm_ver < uc_host_fm_ver //the firmware in host flash is new, need upgrade
        )
    {
        msleep(100);
        TPD_DEBUG("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
            uc_tp_fm_ver, uc_host_fm_ver);
        i_ret = fts_ctpm_fw_upgrade_with_i_file();    
        if (i_ret == 0)
        {
            msleep(300);
            uc_host_fm_ver = fts_ctpm_get_i_file_ver();
            TPD_DEBUG("[FTS] upgrade to new version 0x%x\n", uc_host_fm_ver);
        }
        else
        {
            TPD_DEBUG("[FTS] upgrade failed ret=%d.\n", i_ret);
        }
    }

    return 0;
}

static int agold_ctp_fw_upgrate(char *buf ,int size)
{
	unsigned char uc_host_fm_ver = 0;
    unsigned char uc_tp_fm_ver = 0;
    int           i_ret;
	FTS_BYTE*     pbt_buf = FTS_NULL;
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	if((i2c_smbus_read_i2c_block_data(i2c_client, 0xA6, 1, &uc_tp_fm_ver))>= 0)
	{
		TPD_DEBUG("[FTS] uc_tp_fm_ver = 0x%x\n",uc_tp_fm_ver);
	}
    uc_host_fm_ver = buf[size-2];

	TPD_DEBUG("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n", uc_tp_fm_ver, uc_host_fm_ver);
	
	//0xa6 : the firmware in touch panel maybe corrupted
	//uc_tp_fm_ver < uc_host_fm_ver : the firmware in host flash is new, need upgrade 

    if ( uc_tp_fm_ver == 0xa6  || uc_tp_fm_ver < uc_host_fm_ver)
    {
        msleep(100);

        TPD_DEBUG("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n", uc_tp_fm_ver, uc_host_fm_ver);	
		
	   //=========FW upgrade========================*/

	   i_ret =  fts_ctpm_fw_upgrade(buf,size);
	   if (i_ret != 0)
	   {
		   TPD_DEBUG("[FTS] upgrade failed i_ret = %d.\n", i_ret);
			return FW_UPGRATE_FAILED;
	   }
	   else
	   {
		   TPD_DEBUG("[FTS] upgrade successfully.\n");
		   fts_ctpm_auto_clb();  //start auto CLB
			
			upgrate_percent = 100; //spl add
			printk("[Agold spl] upgrate_percent = %d.\n",upgrate_percent);
			msleep(300);
            TPD_DEBUG("[FTS] upgrade to new version 0x%x\n", buf[size-2]);
	   }                   
    }
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    return FW_UPGRATE_SUCESS;
}
#endif

 
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
{
	TPD_DEBUG("Eric trace: tpd_detect\n");
	strcpy(info->type, TPD_DEVICE);	
	return 0;
}

static void tpd_eint_interrupt_handler(void)
{
	TPD_DEBUG("TPD interrupt has been triggered\n");
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	 
		int retval = TPD_OK;
		char data;
		int i;
		u8 fw_version = 0;
#if defined(AGOLD_CTP_UP_EVENT)
		int event = 5;
#endif		
		TPD_DEBUG("Eric trace: tpd_probe\n");
		i2c_client = client;

	#ifdef TPD_CLOSE_POWER_IN_SLEEP	 
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	TPD_DEBUG("TPD_CLOSE_POWER_IN_SLEEP\n");
        for(i = 0; i < 2; i++) /*Do Power on again to avoid tp bug*/
	{
		hwPowerDown(TPD_POWER_SOURCE,"TP");
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
		mdelay(10);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		mdelay(50);
		hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP"); 

		msleep(100);
	}

	#else
#if 1 //[Agold][huyl][e1109 V1.1和v1.2兼容]
	hwPowerDown(TPD_POWER_SOURCE,"TP");

	TPD_DEBUG("Eric trace: tpd power on!\n");
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP"); 
	msleep(100);
#endif
	/*
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ONE);
	msleep(100);
	*/
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	#endif

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
	msleep(50);
	
	TPD_DEBUG("Eric trace addr:0x%02x",i2c_client->addr);

	if((i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
	{

		TPD_DEBUG("Eric trace: I2C transfer error, line: %d\n", __LINE__);

		tpd_load_status = -1;
		return -1; 
	}
	
	if((i2c_smbus_read_i2c_block_data(i2c_client, 0xA6, 1, &fw_version))>= 0)
	{
		TPD_DEBUG("[Agold][mcz]fw_version = 0x%x\n",fw_version);
	}
#if 0 //[Agold][Talcon.Hu]
i2c_transfer_sucess:
#endif
	#if defined(AGOLD_CTP_UP_EVENT)
	if((i2c_smbus_write_i2c_block_data(i2c_client, 0X88, 1, &event))< 0)
	{
		TPD_DEBUG("Eric trace: I2C transfer error, line: %d\n", __LINE__);
		return -1; 
	}
	#endif
	tpd_load_status = 1;
/*--modified-by-mcz-20130514------*/
#if defined(CFG_SUPPORT_AUTO_UPG)
	this_client = i2c_client;
	TPD_DEBUG("auto_upg\n");
    fts_ctpm_auto_upg();
#endif 

#if defined(AGOLD_CTP_FW_UPGRATE)
	this_client = i2c_client;
#endif 
/*--------------------------------*/
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread))
	{ 
		retval = PTR_ERR(thread);
		TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
	}

	//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1); 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	msleep(50);

#ifdef TPD_PROXIMITY
    obj_ps.polling = 1;         //0--interrupt mode;1--polling mode;
    obj_ps.sensor_operate = tpd_ps_operate;
    
    u32 err = 0;//mucz add
    if ((err = hwmsen_attach(ID_PROXIMITY, &obj_ps)))
    {
        printk("hwmsen attach fail, return:%d.", err);
    }

#endif

	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	
	return 0;
}

static int tpd_remove(struct i2c_client *client)
{
	TPD_DEBUG("TPD removed\n");

	return 0;
}
 
 
static int tpd_local_init(void)
{
	TPD_DMESG("Focaltech FT5406_82 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);

	boot_mode = get_boot_mode();
	if(boot_mode==3||boot_mode==7) boot_mode = NORMAL_BOOT;
#ifdef TPD_HAVE_BUTTON  
	tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

	if(i2c_add_driver(&tpd_i2c_driver)!=0)
	{
		TPD_DMESG("unable to add i2c driver.\n");
		return -1;
	}

	TPD_DEBUG("pancong tpd_load_status = %d,ft5406_82 driver.\n",tpd_load_status);

	if(-1 == tpd_load_status)
	{
		i2c_del_driver(&tpd_i2c_driver);
		TPD_DEBUG("[pancong] del ft5406_82_i2c_driver successful\n");

		return -1;
	}

	TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
	tpd_type_cap = 1;

	return 0; 
}

#ifdef TPD_GPT_TIMER_RESUME
// GPTimer
void ctp_thread_wakeup(UINT16 i)
{
	//printk("[Agold spl]**** ctp_thread_wakeup****\n" );
	GPT_NUM  gpt_num = GPT6;	
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	GPT_Stop(gpt_num); 
}

void CTP_Thread_XGPTConfig(void)
{
	GPT_CONFIG config;	
	GPT_NUM  gpt_num = GPT6;    
	GPT_CLK_SRC clkSrc = GPT_CLK_SRC_RTC;
	//GPT_CLK_DIV clkDiv = GPT_CLK_DIV_128;
	GPT_CLK_DIV clkDiv = GPT_CLK_DIV_64;

	//printk("[Agold spl]***CTP_Thread_XGPTConfig***\n" );

    GPT_Init (gpt_num, ctp_thread_wakeup);
    config.num = gpt_num;
    config.mode = GPT_REPEAT;
	config.clkSrc = clkSrc;
    config.clkDiv = clkDiv;
    //config.u4Timeout = 10*128;
    config.u4CompareL = 256; // 10s : 512*64=32768
    config.u4CompareH = 0;
	config.bIrqEnable = TRUE;
    
    if (GPT_Config(config) == FALSE )
        return;                       
        
    GPT_Start(gpt_num);  

    return ;  
}
#endif

//static int tpd_resume(struct i2c_client *client)
static void tpd_resume( struct early_suspend *h )
{
	//int retval = TPD_OK;
	char data = 0;
	int retry_num = 0,ret = 0;

	TPD_DEBUG("TPD wake up\n");
		
#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
    {
        //return 0;
    }
#endif

#ifdef TPD_GPT_TIMER_RESUME
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);  	
	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	
	msleep(10);
	hwPowerDown(TPD_POWER_SOURCE,"TP");
	hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP"); 
	// Run GPT timer
	CTP_Thread_XGPTConfig();
#else
	#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	
	do{
		msleep(10);
		hwPowerOn(TPD_POWER_SOURCE,VOL_2800,"TP"); 
		msleep(300);

		if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
		{
			TPD_DEBUG("i2c transf error before reset :ret=%d,retry_num == %d\n",ret,retry_num);

			hwPowerDown(TPD_POWER_SOURCE,"TP");
		}	
		else
		{
			TPD_DEBUG("i2c transfer success after reset :ret=%d,retry_num == %d\n",ret,retry_num);
			break;
		}
		retry_num++;
	}while(retry_num < 10);

	if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
	{
		TPD_DEBUG("i2c transf error before reset :ret=%d,retry_num == %d\n",ret,retry_num);

		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		msleep(100);

		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
		msleep(50);  
		mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
		mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
		msleep(400); 

		if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &data))< 0)
		{
			TPD_DEBUG("i2c transf error after reset :ret = %d,retry_num == %d\n",ret,retry_num);
		}
		else
		{
			TPD_DEBUG("i2c transfer success after reset :ret = %d,retry_num == %d\n",ret,retry_num);
		}
	}
	TPD_DEBUG("retry_num == %d\n",retry_num);

	#else
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(100);
        
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(50);  
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(400);  
	#endif
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
#endif
	//return retval;
	return;
}
 
//static int tpd_suspend(struct i2c_client *client, pm_message_t message)
static void tpd_suspend( struct early_suspend *h )
{
	//int retval = TPD_OK;
#ifndef TPD_CLOSE_POWER_IN_SLEEP	
		static char data = 0x3;
#endif

	struct touch_info cinfo, pinfo;
	int i = 0;

#ifdef TPD_PROXIMITY
    if (tpd_proximity_flag == 1)
    {
        return 0;
    }
#endif
	TPD_DEBUG("TPD enter sleep\n");
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	if (tpd_touchinfo(&cinfo, &pinfo))
	{
		TPD_DEBUG("point_num = %d\n",point_num);
			  
		if(point_num >0) 
		{
			while(i<point_num)
			{
				tpd_up(cinfo.x[i], cinfo.y[i], 1);
				i++;
			}
			TPD_DEBUG("pancong release --->\n");			
		} 
		input_sync(tpd->dev);
	}
	#ifdef TPD_CLOSE_POWER_IN_SLEEP	
	hwPowerDown(TPD_POWER_SOURCE,"TP");

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	#else
	i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  //TP enter sleep mode
	/*
	mt_set_gpio_mode(GPIO_CTP_EN_PIN, GPIO_CTP_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_EN_PIN, GPIO_OUT_ZERO);
        */
	#endif
	//return retval;
} 
#if defined(AGOLD_CTP_FW_UPGRATE)
int agold_ft5406_update_percent(void)
{
	return upgrate_percent;
}

int agold_get_ft5406_fw_version(void)
{
	int fw_version = 0;
	char ret = 0;

	printk("[Agold spl] agold_get_ft5406_fw_version \n");

	if((ret = i2c_smbus_read_i2c_block_data(i2c_client, FW_ID_ADDR, 1, &fw_version))< 0)
	{
		printk("[Agold spl] i2c_smbus_read_i2c_block_data, FW_ID_ADDR :ret=%d,\n",ret);
	}
	printk("[Agold spl] fw_version = 0x%x \n",fw_version);

	return fw_version;
}

int agold_start_ft5406_fw_update(char *buf, int size)
{
	return agold_ctp_fw_upgrate(buf, size);
}

#endif

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "FT5406_82",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
	#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
	#else
	.tpd_have_button = 0,
	#endif		
	#if defined(AGOLD_CTP_FW_UPGRATE)
	.tpd_get_fw_version = agold_get_ft5406_fw_version,
	.tpd_start_fw_update = agold_start_ft5406_fw_update,
	#endif
};
 

/* called when loaded into kernel */
static int __init tpd_driver_init(void) {
	TPD_DEBUG("MediaTek FT5406_82 touch panel driver init\n");
	i2c_register_board_info(0, &i2c_tpd, 1);
	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FT5406 driver failed\n");

	return 0;
}
 
/* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek FT5406 touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
 module_init(tpd_driver_init);
 module_exit(tpd_driver_exit);

