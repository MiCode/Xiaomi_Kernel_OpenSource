/*
 * This software is licensed under the terms of the GNU General Public 
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms. 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * * VERSION      	DATE			AUTHOR          Note
 *    1.0		  2013-7-16			Focaltech        initial  based on MTK platform
 * 
 */

#include "tpd.h"

#include "tpd_custom_fts.h"
#ifdef FTS_CTL_IIC
#include "focaltech_ctl.h"
#endif
#include "focaltech_ex_fun.h"

#include "cust_gpio_usage.h"
#include <linux/input/mt.h>		//slot

static bool TP_gesture_Switch;
#define GESTURE_SWITCH_FILE 		"/data/data/com.example.setgesture/shared_prefs/gesture.xml"  //总开关文件,获取第一个value的值,为1开,为0关

//wangcq327 --- add start
static int TP_VOL;
//wangcq327 --- add end
  struct Upgrade_Info fts_updateinfo[] =
{
        {0x55,"FT5x06",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
        {0x08,"FT5606",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x06, 100, 2000},
	{0x0a,"FT5x16",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x07, 1, 1500},
	{0x05,"FT6208",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,60, 30, 0x79, 0x05, 10, 2000},
	{0x06,"FT6x06",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x08, 10, 2000},
	{0x36,"FT6x36",TPD_MAX_POINTS_2,AUTO_CLB_NONEED,100, 30, 0x79, 0x18, 10, 2000},//CHIP ID error
	{0x55,"FT5x06i",TPD_MAX_POINTS_5,AUTO_CLB_NEED,50, 30, 0x79, 0x03, 1, 2000},
	{0x14,"FT5336",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x13,"FT3316",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x12,"FT5436i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x11,"FT5336i",TPD_MAX_POINTS_5,AUTO_CLB_NONEED,30, 30, 0x79, 0x11, 10, 2000},
	{0x54,"FT5x46",TPD_MAXPOINTS_10,AUTO_CLB_NONEED,2, 2, 0x54, 0x2c, 10, 2000},
};
				
struct Upgrade_Info fts_updateinfo_curr;

extern struct tpd_device *tpd;
 
static struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;

u8 *I2CDMABuf_va = NULL;
dma_addr_t *I2CDMABuf_pa = 0;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
//static DEFINE_MUTEX(i2c_access);
 
 
static void tpd_eint_interrupt_handler(void);
extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);
extern void mt_eint_set_hw_debounce(unsigned int eint_num, unsigned int ms);
extern unsigned int mt_eint_set_sens(unsigned int eint_num, unsigned int sens);
//extern void mt_eint_registration(unsigned int eint_num, unsigned int is_deb_en, unsigned int pol, void (EINT_FUNC_PTR)(void), unsigned int is_auto_umask);
 
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info);
static int tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);

static int tpd_flag 					= 0;
static int tpd_halt						= 0;
static int point_num 					= 0;
static int p_point_num 					= 0;

int up_flag=0,up_count=0;

//#define TPD_CLOSE_POWER_IN_SLEEP
#define TPD_OK 							0
//register define
#define DEVICE_MODE 					0x00
#define GEST_ID 						0x01
#define TD_STATUS 						0x02
//point1 info from 0x03~0x08
//point2 info from 0x09~0x0E
//point3 info from 0x0F~0x14
//point4 info from 0x15~0x1A
//point5 info from 0x1B~0x20
//register define

#define TPD_RESET_ISSUE_WORKAROUND

#define TPD_MAX_RESET_COUNT 			2

struct ts_event 
{
	u16 au16_x[CFG_MAX_TOUCH_POINTS];				/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];				/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];		/*touch event: 0 -- down; 1-- up; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];			/*touch ID */
	u16 pressure[CFG_MAX_TOUCH_POINTS];
	u16 area[CFG_MAX_TOUCH_POINTS];
	u8 touch_point;
	int touchs;
	u8 touch_point_num;
};


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

#define VELOCITY_CUSTOM_FT5206
#ifdef VELOCITY_CUSTOM_FT5206
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

// for magnify velocity********************************************

#ifndef TPD_VELOCITY_CUSTOM_X
#define TPD_VELOCITY_CUSTOM_X 			10
#endif
#ifndef TPD_VELOCITY_CUSTOM_Y
#define TPD_VELOCITY_CUSTOM_Y 			10
#endif

#define TOUCH_IOC_MAGIC 				'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC,0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC,1)


static int g_v_magnify_x =TPD_VELOCITY_CUSTOM_X;
static int g_v_magnify_y =TPD_VELOCITY_CUSTOM_Y;


#ifdef CONFIG_DEVINFO_CTP
#include<linux/dev_info.h>
static char *Version;
static void devinfo_ctp_regchar(char *ic, char *module,char * vendor,char *version,char *used)
{
 	struct devinfo_struct *s_DEVINFO_ctp =(struct devinfo_struct*) kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);	
	s_DEVINFO_ctp->device_type="CTP";
	s_DEVINFO_ctp->device_module=module;
	s_DEVINFO_ctp->device_vendor=vendor;
	s_DEVINFO_ctp->device_ic=ic;
	s_DEVINFO_ctp->device_info=DEVINFO_NULL;
	s_DEVINFO_ctp->device_version=version;
	s_DEVINFO_ctp->device_used=used;
	printk("ft5346: version:%s\n", version);
	devinfo_check_add_device(s_DEVINFO_ctp);
}
#endif

#define LCT_ADD_TP_VERSION
#ifdef LCT_ADD_TP_VERSION
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#define CTP_PROC_FILE "tp_info"
static struct proc_dir_entry *ctp_status_proc = NULL;
char tp_version;
char vendor_id = 0;

static int ctp_proc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	int len, err = -1;

	printk("Enter ctp_proc_read\n");

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);	
	if (!page) 
	{		
		printk("Enter ctp_proc_read, !page\n");
		kfree(page);		
		return -ENOMEM;	
	}
	printk("ctp_proc_read, 0xa6 =0x%x\n", tp_version);
	
	ptr = page; 

	if(vendor_id == 0x3B)
	{
		ptr += sprintf(ptr, "[Vendor]BoEn, [fw]T%d.%d, [ic]%s\n", tp_version / 16, tp_version % 16, "ft5346");
	}
	else	if(vendor_id == 0x53)
	{
		ptr += sprintf(ptr, "[Vendor]MutTon, [fw]T%d.%d, [ic]%s\n", (tp_version % 16) / 10, (tp_version % 16) % 10, "ft5346");
	}
	else
	{
		ptr += sprintf(ptr, "[Vendor]BoEn, [fw]T%d.%d, [ic]%s\n", tp_version / 16, tp_version % 16, "ft5346");
	}
	
	len = ptr - page; 			 	
	if(*ppos >= len)
	{	
		printk("Enter ctp_proc_read, *ppos >= len\n");
		kfree(page); 		
		return 0; 	
	}	
	err = copy_to_user(buffer,(char *)page,len); 			
	*ppos += len; 
	
	if(err) 
	{		
		printk("Enter ctp_proc_read, err\n");
		kfree(page); 		
		return err; 	
	}	
	
	kfree(page); 	
	return len;	
}

static const struct file_operations ctp_proc_fops = {
	.read = ctp_proc_read,
};
#endif

#define LCT_ADD_TP_LOCKDOWN_INFO
#ifdef LCT_ADD_TP_LOCKDOWN_INFO
#define CTP_PROC_LOCKDOWN_FILE "tp_lockdown_info"
static struct proc_dir_entry *ctp_lockdown_status_proc = NULL;
char tp_lockdown_info[128];

static int ctp_lockdown_proc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	char *page = NULL;
	char *ptr = NULL;
	int len, err = -1;

	printk("Enter ctp_lockdown_proc_read\n");

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);	
	if (!page) 
	{		
		printk("Enter ctp_lockdown_proc_read, !page\n");
		kfree(page);		
		return -ENOMEM;	
	}
	printk("ctp_lockdown_proc_read, tp_lockdown_info =%s\n", tp_lockdown_info);
	
	ptr = page; 

	ptr += sprintf(ptr, "%s\n", tp_lockdown_info);

	len = ptr - page; 			 	
	if(*ppos >= len)
	{	
		printk("Enter ctp_lockdown_proc_read, *ppos >= len\n");
		kfree(page); 		
		return 0; 	
	}	
	err = copy_to_user(buffer,(char *)page,len); 			
	*ppos += len; 
	
	if(err) 
	{		
		printk("Enter ctp_lockdown_proc_read, err\n");
		kfree(page); 		
		return err; 	
	}	
	
	kfree(page); 	
	return len;	
}

static const struct file_operations ctp_lockdown_proc_fops = {
	.read = ctp_lockdown_proc_read,
};
#endif

static int tpd_misc_open(struct inode *inode, struct file *file)
{
/*
	file->private_data = adxl345_i2c_client;

	if(file->private_data == NULL)
	{
		printk("tpd: null pointer!!\n");
		return -EINVAL;
	}
	*/
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int tpd_misc_release(struct inode *inode, struct file *file)
{
	//file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int adxl345_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long tpd_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	//struct i2c_client *client = (struct i2c_client*)file->private_data;
	//struct adxl345_i2c_data *obj = (struct adxl345_i2c_data*)i2c_get_clientdata(client);	
	//char strbuf[256];
	void __user *data;
	
	long err = 0;
	
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		printk("tpd: access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case TPD_GET_VELOCITY_CUSTOM_X:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_x, sizeof(g_v_magnify_x)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

	   case TPD_GET_VELOCITY_CUSTOM_Y:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &g_v_magnify_y, sizeof(g_v_magnify_y)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		default:
			printk("tpd: unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
	}

	return err;
}


static struct file_operations tpd_fops = {
//	.owner = THIS_MODULE,
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TPD_NAME,
	.fops = &tpd_fops,
};

//**********************************************
#endif

struct touch_info {
    int y[10];
    int x[10];
    int p[10];
    int id[10];
};
 
static const struct i2c_device_id ft5206_tpd_id[] = {{TPD_NAME,0},{}};
//unsigned short force[] = {0,0x70,I2C_CLIENT_END,I2C_CLIENT_END}; 
//static const unsigned short * const forces[] = { force, NULL };
//static struct i2c_client_address_data addr_data = { .forces = forces, };
static struct i2c_board_info __initdata ft5206_i2c_tpd={ I2C_BOARD_INFO(TPD_NAME, (0x70>>1))};
 
static struct i2c_driver tpd_i2c_driver = {
  	.driver = {
	 	.name 	= TPD_NAME,
	//	.owner 	= THIS_MODULE,
  	},
  	.probe 		= tpd_probe,
  	.remove 	= tpd_remove,
  	.id_table 	= ft5206_tpd_id,
  	.detect 	= tpd_detect,
// 	.shutdown	= tpd_shutdown,
//  .address_data = &addr_data,
};


static  void tpd_down(int x, int y, int p) {
	// input_report_abs(tpd->dev, ABS_PRESSURE, p);
	input_report_key(tpd->dev, BTN_TOUCH, 1);
	input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 20);
#ifndef MT_PROTOCOL_B	
	input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0x3f);//zax
#endif	
	input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);

	printk("%s, D[x: %4d Y: %4d P: %4d]\n", __func__, x, y, p);
	/* track id Start 0 */
	//input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, p); 
	input_mt_sync(tpd->dev);
	TPD_EM_PRINT(x, y, x, y, p-1, 1);
//    if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
//    {   
//      tpd_button(x, y, 1);  
//    }
}
 
static  void tpd_up(int x, int y) {
	//input_report_abs(tpd->dev, ABS_PRESSURE, 0);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	//input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	//input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	printk("%s, U[%4d %4d %4d]\n", __func__, x, y, 0);
	input_mt_sync(tpd->dev);
	TPD_EM_PRINT(x, y, x, y, 0, 0);
//    if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode())
//    {   
//       tpd_button(x, y, 0); 
//    }   		 
}

static int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo,struct touch_info *ptest)
{
	int i = 0;
	char data[128] = {0};
	u16 high_byte,low_byte,reg;
	u8 report_rate =0;
	p_point_num = point_num;
	if (tpd_halt)
	{
		TPD_DMESG( "tpd_touchinfo return ..\n");
		return false;
	}
	//mutex_lock(&i2c_access);

	reg = 0x00;
	fts_i2c_Read(i2c_client, &reg, 1, data, 64);
	//mutex_unlock(&i2c_access);
	
	/*get the number of the touch points*/

	point_num= data[2] & 0x0f;
	if(up_flag==2)
	{
		up_flag=0;
		for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)  
		{
			cinfo->p[i] = data[3+6*i] >> 6; //event flag 
			cinfo->id[i] = data[3+6*i+2]>>4; //touch id
			/*get the X coordinate, 2 bytes*/
			high_byte = data[3+6*i];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i + 1];
			cinfo->x[i] = high_byte |low_byte;	
			high_byte = data[3+6*i+2];
			high_byte <<= 8;
			high_byte &= 0x0f00;
			low_byte = data[3+6*i+3];
			cinfo->y[i] = high_byte |low_byte;

			
			if(point_num>=i+1)
				continue;
			if(up_count==0)
				continue;
			cinfo->p[i] = ptest->p[i-point_num]; //event flag 
			
			
			cinfo->id[i] = ptest->id[i-point_num]; //touch id
	
			cinfo->x[i] = ptest->x[i-point_num];	
			
			cinfo->y[i] = ptest->y[i-point_num];
			//dev_err(&fts_i2c_client->dev," zax add two x = %d, y = %d, evt = %d,id=%d\n", cinfo->x[i], cinfo->y[i], cinfo->p[i], cinfo->id[i]);
			up_count--;
			
				
		}
		
		return true;
	}
	up_count=0;
	for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)  
	{
		cinfo->p[i] = data[3+6*i] >> 6; //event flag 
		
		if(0==cinfo->p[i])
		{
			//dev_err(&fts_i2c_client->dev,"\n	zax enter add	\n");
			up_flag=1;
		}
			cinfo->id[i] = data[3+6*i+2]>>4; //touch id
		/*get the X coordinate, 2 bytes*/
		high_byte = data[3+6*i];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i + 1];
		cinfo->x[i] = high_byte |low_byte;	
		high_byte = data[3+6*i+2];
		high_byte <<= 8;
		high_byte &= 0x0f00;
		low_byte = data[3+6*i+3];
		cinfo->y[i] = high_byte |low_byte;

		if(up_flag==1 && 1==cinfo->p[i])
		{
			up_flag=2;
			point_num++;
			ptest->x[up_count]=cinfo->x[i];
			ptest->y[up_count]=cinfo->y[i];
			ptest->id[up_count]=cinfo->id[i];
			ptest->p[up_count]=cinfo->p[i];
			//dev_err(&fts_i2c_client->dev," zax add x = %d, y = %d, evt = %d,id=%d\n", ptest->x[j], ptest->y[j], ptest->p[j], ptest->id[j]);
			cinfo->p[i]=2;
			up_count++;
		}
	}
	if(up_flag==1)
		up_flag=0;
	//printk(" tpd cinfo->x[0] = %d, cinfo->y[0] = %d, cinfo->p[0] = %d\n", cinfo->x[0], cinfo->y[0], cinfo->p[0]);
	return true;

}
 /************************************************************************
* Name: fts_read_Touchdata
* Brief: report the point information
* Input: event info
* Output: get touch data in pinfo
* Return: success is zero
***********************************************************************/
#ifdef MT_PROTOCOL_B
static int fts_read_Touchdata(struct ts_event *pinfo)
{
       u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	u8 pointid = 0x0F;

	if (tpd_halt)
	{
		DBG( "tpd_touchinfo return ..\n");
		return false;
	}

	//mutex_lock(&i2c_access);
	ret = fts_i2c_Read(i2c_client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "%s read touchdata failed.\n",__func__);
		//mutex_unlock(&i2c_access);
		return ret;
	}
	//mutex_unlock(&i2c_access);
	memset(pinfo, 0, sizeof(struct ts_event));
	pinfo->touch_point_num=buf[FT_TOUCH_POINT_NUM] & 0x0F;
	
	pinfo->touch_point = 0;
	//printk("tpd  fts_updateinfo_curr.TPD_MAX_POINTS=%d fts_updateinfo_curr.chihID=%d \n", fts_updateinfo_curr.TPD_MAX_POINTS,fts_updateinfo_curr.CHIP_ID);
	for (i = 0; i < 10; i++)
	{
		pointid = (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;
		if ((pointid >= FTS_MAX_ID)||(pinfo->touch_point_num>MT_MAX_TOUCH_POINTS))
			break;
		else
			pinfo->touch_point++;
		pinfo->au16_x[i] =
		    (s16) (buf[FTS_TOUCH_X_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_X_L_POS + FTS_TOUCH_STEP * i];
		pinfo->au16_y[i] =
		    (s16) (buf[FTS_TOUCH_Y_H_POS + FTS_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_Y_L_POS + FTS_TOUCH_STEP * i];
		pinfo->au8_touch_event[i] =
		    buf[FTS_TOUCH_EVENT_POS + FTS_TOUCH_STEP * i] >> 6;
		pinfo->au8_finger_id[i] =
		    (buf[FTS_TOUCH_ID_POS + FTS_TOUCH_STEP * i]) >> 4;
		pinfo->pressure[i] =
			(buf[FTS_TOUCH_XY_POS + FTS_TOUCH_STEP * i]);//cannot constant value
		pinfo->area[i] =
			(buf[FTS_TOUCH_MISC + FTS_TOUCH_STEP * i]) >> 4;
		if((pinfo->au8_touch_event[i]==0 || pinfo->au8_touch_event[i]==2)&&((pinfo->touch_point_num==0)||(pinfo->pressure[i]==0 && pinfo->area[i]==0  )))
			return 1;
	}
		
	return 0;
}


/************************************************************************
* Name: fts_report_value
* Brief: report the point information
* Input: event info
* Output: no
* Return: success is zero
***********************************************************************/
static void fts_report_value(struct ts_event *data)
 {
	struct ts_event *event = data;
	int i = 0, j = 0;
	int touchs = 0;
	int up_point = 0;
	
	 for (i = 0; i < event->touch_point; i++) 
	 {
		 input_mt_slot(tpd->dev, event->au8_finger_id[i]);
 
		 if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
			 {
				 input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,true);
				 input_report_abs(tpd->dev, ABS_MT_PRESSURE,event->pressure[i]);
				 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR,event->area[i]);
				 input_report_abs(tpd->dev, ABS_MT_POSITION_X,event->au16_x[i]);
				 input_report_abs(tpd->dev, ABS_MT_POSITION_Y,event->au16_y[i]);
				 touchs |= BIT(event->au8_finger_id[i]);
				 event->touchs |= BIT(event->au8_finger_id[i]);
			  //printk("tpd D x[%d] =%d,y[%d]= %d",i,event->au16_x[i],i,event->au16_y[i]);
			 }
			 else
			 {
				 up_point++;
				 input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER,false);
				 event->touchs &= ~BIT(event->au8_finger_id[i]);
			 }				 
		 
	 }

	for(i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
	{
		if(BIT(i) & (event->touchs ^ touchs))
		{
			//up_point++; //fts change 2015-0701 for no up event.
			event->touchs &= ~BIT(i);
			input_mt_slot(tpd->dev, i);
			input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
		}
	}
	
	event->touchs = touchs;
	
	if (event->touch_point_num == 0)
	{ 
		// release all touches  
		for(j = 0; j < CFG_MAX_TOUCH_POINTS; j++)
		{
			input_mt_slot( tpd->dev, j);
			input_mt_report_slot_state( tpd->dev, MT_TOOL_FINGER, false);
		}
		event->touchs = 0;
		input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_sync(tpd->dev);
		return;
	}
	
	 if(event->touch_point == up_point)
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
	else
		 input_report_key(tpd->dev, BTN_TOUCH, 1);
 
	 input_sync(tpd->dev);
	//printk("tpd D x =%d,y= %d",event->au16_x[0],event->au16_y[0]);
 }

#endif

extern int FG_charging_status ;
int close_to_ps_flag_value = 1;	// 1: close ; 0: far away
int charging_flag = 0;
 static int touch_event_handler(void *unused)
{ 
	struct touch_info cinfo, pinfo, ptest;
#ifdef MT_PROTOCOL_B
	struct ts_event pevent;
	int ret = 0;
#endif
	int i=0;

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	sched_setscheduler(current, SCHED_RR, &param);

	u8 state;
	u8 data;

	do
	{
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		set_current_state(TASK_INTERRUPTIBLE); 
		//DBG("wangcq327 --- waitting\n");
		wait_event_interruptible(waiter,tpd_flag!=0);				 
		//DBG("wangcq327 --- pass\n");
		
		tpd_flag = 0;

		set_current_state(TASK_RUNNING);

		if((FG_charging_status != 0) && (charging_flag == 0))
		{
			data = 0x1;
			charging_flag = 1;
			fts_write_reg(i2c_client, 0x8B, 0x01);  
		}
		else
		{
			if((FG_charging_status  == 0) && (charging_flag == 1))
			{
				charging_flag = 0;
				data = 0x0;
				fts_write_reg(i2c_client, 0x8B, 0x00);  					
			}
		}

		if(close_to_ps_flag_value == 0)
		{
			printk("lizhiye, hand touch the ps and far away the ps\n");
			close_to_ps_flag_value = 1;
			for(i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
			{
				input_mt_slot(tpd->dev, i);
				input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 0);
			}
			input_mt_report_pointer_emulation(tpd->dev, false);
			input_sync(tpd->dev);
			continue;
		}
		
#ifdef MT_PROTOCOL_B
		{
        	ret = fts_read_Touchdata(&pevent);
			if (ret == 0)
				fts_report_value(&pevent);
		}
#else
		if (tpd_touchinfo(&cinfo, &pinfo,&ptest)) 
		{
			TPD_DEBUG("point_num = %d\n",point_num);
			TPD_DEBUG_SET_TIME;
			if(point_num >0) 
			{
				for(i = 0; i < fts_updateinfo_curr.TPD_MAX_POINTS; i++)  
				{
					if((0==cinfo.p[i]) || (2==cinfo.p[i]))
					{
			       			tpd_down(cinfo.x[i], cinfo.y[i], cinfo.id[i]);
					}
					
				}
				input_sync(tpd->dev);
			}
			else
			{
				tpd_up(cinfo.x[0], cinfo.y[0]);
				//TPD_DEBUG("release --->\n"); 
				//input_mt_sync(tpd->dev);
				input_sync(tpd->dev);
			}
		}
#endif
		if(tpd_mode==12)
		{
			//power down for desence debug
			//power off, need confirm with SA
#ifdef TPD_POWER_SOURCE_CUSTOM
			hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
#endif
			msleep(20);
		}
	}while(!kthread_should_stop());

	return 0;
}
 
static int tpd_detect (struct i2c_client *client, struct i2c_board_info *info) 
{
	strcpy(info->type, TPD_DEVICE);	
	return 0;
}
 
static void tpd_eint_interrupt_handler(void)
{
	//TPD_DEBUG("TPD interrupt has been triggered\n");
	TPD_DEBUG_PRINT_INT;
	tpd_flag = 1;
	wake_up_interruptible(&waiter);
}

void focaltech_get_upgrade_array(void)
{
	u8 chip_id;
	u32 i;

#ifdef Boot_Upgrade_Protect
	chip_id = 0x54; //0x54,FT5x46Chip_ID
#else
	i2c_smbus_read_i2c_block_data(i2c_client,FT_REG_CHIP_ID,1,&chip_id);
#endif


	DBG("tpd %s chip_id = %x\n", __func__, chip_id);
	
	for(i=0;i<sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info);i++)
	{
		if(chip_id==fts_updateinfo[i].CHIP_ID)
		{
			memcpy(&fts_updateinfo_curr, &fts_updateinfo[i], sizeof(struct Upgrade_Info));
			break;
		}
	}

	if(i >= sizeof(fts_updateinfo)/sizeof(struct Upgrade_Info))
	{
		memcpy(&fts_updateinfo_curr, &fts_updateinfo[0], sizeof(struct Upgrade_Info));
	}
}

extern unsigned char ft5x46_ctpm_LockDownInfo_get_from_boot(  struct i2c_client *client,char *pProjectCode );

static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	 
	int retval = TPD_OK;
	char data = 1;
	u8 report_rate=0;
	int err=0;
	int reset_count = 0;
	u8 chip_id,i;

	i2c_client = client;
	//power on, need confirm with SA
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(5);

	DBG(" fts ic reset\n");
	
#ifdef TPD_POWER_SOURCE_CUSTOM
	hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
#endif

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(5);

reset_proc:

	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(5);
	DBG(" fts ic reset\n");
	
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	msleep(250);

	err = i2c_smbus_read_i2c_block_data(i2c_client, 0x00, 1, &data);
	printk("tpd_probe: %d, data:%d\n", err, data);
	if(err< 0 || data!=0)// reg0 data running state is 0; other state is not 0
	{
		printk("I2C transfer error, line: %d\n", __LINE__);
#ifdef TPD_RESET_ISSUE_WORKAROUND
	        if ( ++reset_count < TPD_MAX_RESET_COUNT )
	        {
	            goto reset_proc;
	        }
#endif

#ifdef TPD_POWER_SOURCE_CUSTOM
		hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
#endif
		return -1; 
	}

	msleep(200);

	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, GPIO_PULL_UP);
 
	//mt_eint_set_sens(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	//mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1); 
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

#ifndef TPD_SYSFS_DEBUG
	I2CDMABuf_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, FTS_DMA_BUF_SIZE, &I2CDMABuf_pa, GFP_KERNEL);
	if(!I2CDMABuf_va){
		printk("%s, [Error] Allocate DMA I2C Buffer failed!\n", __func__);
	}
#endif

	tpd_load_status = 1;
 
	focaltech_get_upgrade_array();

#ifdef FTS_APK_DEBUG
	ft5x0x_create_apk_debug_channel(client);
#endif

#ifdef TPD_SYSFS_DEBUG
	fts_create_sysfs(i2c_client);
#endif

#ifdef FTS_CTL_IIC
	if (ft_rw_iic_drv_init(i2c_client) < 0)
		printk("%s:[FTS] create fts control iic driver failed\n", __func__);
#endif
	
#ifdef VELOCITY_CUSTOM_FT5206
	if((err = misc_register(&tpd_misc_device)))
	{
		printk("mtk_tpd: tpd_misc_device register failed\n");
	}
#endif

#ifdef TPD_AUTO_UPGRADE
	printk("**************tpd******Enter CTP Auto Upgrade********************\n");
	fts_ctpm_auto_upgrade(i2c_client);
#endif

	ft5x46_ctpm_LockDownInfo_get_from_boot(i2c_client, tp_lockdown_info);
	printk("tpd_probe, ft5x46_ctpm_LockDownInfo_get_from_boot, tp_lockdown_info=%s\n", tp_lockdown_info);

	fts_read_reg(i2c_client, 0xa6, &tp_version);
	printk("tpd_probe, fts_read_reg(i2c_client, 0xa6 =0x%x)\n", tp_version);

	fts_read_reg(i2c_client, 0xa8, &vendor_id);
	
	thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	if (IS_ERR(thread))
	{
		retval = PTR_ERR(thread);
		DBG(TPD_DEVICE " failed to create kernel thread: %d\n", retval);
	}

	DBG("FTS Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");

#ifdef LCT_ADD_TP_VERSION
	ctp_status_proc = proc_create(CTP_PROC_FILE, 0644, NULL, &ctp_proc_fops);
	if (ctp_status_proc == NULL)
	{
		DBG("tpd, create_proc_entry ctp_status_proc failed\n");
	}
#endif    

#ifdef LCT_ADD_TP_LOCKDOWN_INFO
	ctp_lockdown_status_proc = proc_create(CTP_PROC_LOCKDOWN_FILE, 0644, NULL, &ctp_lockdown_proc_fops);
	if (ctp_lockdown_status_proc == NULL)
	{
		DBG("tpd, create_proc_entry ctp_lockdown_status_proc failed\n");
	}
#endif

#ifdef CONFIG_DEVINFO_CTP
{
	Version = (char *)kmalloc(10, GFP_KERNEL);
	memset(Version, 0, sizeof(char) * 10);
	if((tp_version / 16) >= 1)
	{
		sprintf(Version, "T%d.%d", tp_version / 16, tp_version % 16);
	}
	else
	{
		sprintf(Version, "T%d.%d", (tp_version % 16) / 10, (tp_version % 16) % 10);
	}
	devinfo_ctp_regchar("mxt336t", "o-film", "Atmel", DEVINFO_NULL, DEVINFO_UNUSED);
	if(vendor_id == 0x3B)
	{
		devinfo_ctp_regchar("ft5346", "BoEn", "FocalTech", Version, DEVINFO_USED);
		devinfo_ctp_regchar("ft5346", "MutTon", "FocalTech", DEVINFO_NULL, DEVINFO_UNUSED);
	}
	else	if(vendor_id == 0x53)
	{
		devinfo_ctp_regchar("ft5346", "BoEn", "FocalTech", DEVINFO_NULL, DEVINFO_UNUSED);
		devinfo_ctp_regchar("ft5346", "MutTon", "FocalTech", Version, DEVINFO_USED);
	}
	else
	{
		devinfo_ctp_regchar("ft5346", "BoEn", "FocalTech", Version, DEVINFO_USED);
		devinfo_ctp_regchar("ft5346", "MutTon", "FocalTech", DEVINFO_NULL, DEVINFO_UNUSED);
	}
		
}
#endif

#ifdef MT_PROTOCOL_B
	#if 1//(LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0))
		input_mt_init_slots(tpd->dev, MT_MAX_TOUCH_POINTS,1);
	#endif
		input_set_abs_params(tpd->dev, ABS_MT_TOUCH_MAJOR,0, 255, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif

	return 0;
}

 static int tpd_remove(struct i2c_client *client)
{

#ifdef FTS_APK_DEBUG
	ft5x0x_release_apk_debug_channel();
#endif
#ifdef TPD_SYSFS_DEBUG
	fts_release_sysfs(client);
#endif

#ifdef FTS_CTL_IIC
	ft_rw_iic_drv_exit();
#endif

	TPD_DEBUG("TPD removed\n");

   	return 0;
}
 
static int tpd_local_init(void)
{
  	DBG("FTS I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 
   	if(i2c_add_driver(&tpd_i2c_driver)!=0)
   	{
  		DBG("FTS unable to add i2c driver.\n");
      	return -1;
    }
    if(tpd_load_status == 0) 
    {
    	DBG("FTS add error touch panel driver.\n");
    	i2c_del_driver(&tpd_i2c_driver);
    	return -1;
    }
#ifdef MT_PROTOCOL_B
#else
		input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (MT_MAX_TOUCH_POINTS-1), 0, 0);
#endif

#ifdef TPD_HAVE_BUTTON     
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif   

#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))    
    TPD_DO_WARP = 1;
    memcpy(tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT*4);
    memcpy(tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT*4);
#endif 

#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
    DBG("end %s, %d\n", __FUNCTION__, __LINE__);  
    tpd_type_cap = 1;
    return 0; 
 }

 static void tpd_resume( struct early_suspend *h )
{
	static char i = 0;
	DBG("TPD wake up\n");

	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);  
	msleep(5);  
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
	//mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);  
	msleep(120);//200
	tpd_halt = 0;

	DBG("zax TPD clear 1\n");
	for(i = 0; i < CFG_MAX_TOUCH_POINTS; i++)
	{
		input_mt_slot(tpd->dev, i);
		input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 0);
		DBG("zax TPD clear 1234.\n");
	}
	//input_mt_report_pointer_emulation(tpd->dev, false);
	input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_sync(tpd->dev);
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	if(FG_charging_status != 0)
	{
		charging_flag = 0;
	}
	else
	{
		charging_flag = 1;
	}
	DBG("zax TPD wake up done\n");
}

 static void tpd_suspend( struct early_suspend *h )
 {
	 static char data = 0x3;
	 int i;


	DBG("zax TPD enter sleep123\n");
	for (i = 0; i <CFG_MAX_TOUCH_POINTS; i++) 
	{
		input_mt_slot(tpd->dev, i);
		input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, 0);
		DBG("zax TPD enter sleep1234\n");
	}
		//input_mt_report_pointer_emulation(tpd->dev, false);
		input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_sync(tpd->dev);
	
	DBG("zax TPD enter sleep done\n");
 	 tpd_halt = 1;

	DBG("TPD enter sleep\n");
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	//mutex_lock(&i2c_access);
	i2c_smbus_write_i2c_block_data(i2c_client, 0xA5, 1, &data);  //TP enter sleep mode
	//mutex_unlock(&i2c_access);
	//return retval;
 } 


 static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TPD_NAME,
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
	printk("MediaTek FTS touch panel driver init\n");

	i2c_register_board_info(2, &ft5206_i2c_tpd, 1);

	if(tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add FTS driver failed\n");
	 return 0;
 }
 
 /* should never be called */
static void __exit tpd_driver_exit(void) {
	TPD_DMESG("MediaTek FTS touch panel driver exit\n");
	//input_unregister_device(tpd->dev);
	tpd_driver_remove(&tpd_device_driver);
}
 
module_init(tpd_driver_init);
module_exit(tpd_driver_exit);
