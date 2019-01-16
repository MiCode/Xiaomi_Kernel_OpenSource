#include "tpd.h"
#include <linux/interrupt.h>
#include <cust_eint.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/time.h>
 
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif 
 
extern struct tpd_device *tpd;
 
struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
 
static DECLARE_WAIT_QUEUE_HEAD(waiter);
 
struct early_suspend early_suspend;
 
#ifdef CONFIG_HAS_EARLYSUSPEND
static void tpd_early_suspend(struct early_suspend *handler);
static void tpd_late_resume(struct early_suspend *handler);
#endif 
 
#if 0
extern void MT6516_EINTIRQUnmask(unsigned int line);
extern void MT6516_EINTIRQMask(unsigned int line);
extern void MT6516_EINT_Set_HW_Debounce(kal_uint8 eintno, kal_uint32 ms);
extern kal_uint32 MT6516_EINT_Set_Sensitivity(kal_uint8 eintno, kal_bool sens);
extern void MT6516_EINT_Registration(kal_uint8 eintno, kal_bool Dbounce_En,
									  kal_bool ACT_Polarity, void (EINT_FUNC_PTR)(void),
									  kal_bool auto_umask);
#endif
 
static void tpd_eint_interrupt_handler(void);
static int tpd_get_bl_info(int show);
static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int __devexit tpd_remove(struct i2c_client *client);
static int touch_event_handler(void *unused);
static int tpd_initialize(struct i2c_client * client);
 
 
static int tpd_flag = 0;
 
#define TPD_OK 0
 
#define TPD_REG_BASE 0x00
#define TPD_SOFT_RESET_MODE 0x01
#define TPD_OP_MODE 0x00
#define TPD_LOW_PWR_MODE 0x04
#define TPD_SYSINFO_MODE 0x10
#define GET_HSTMODE(reg)  ((reg & 0x70) >> 4)  // in op mode or not 
#define GET_BOOTLOADERMODE(reg) ((reg & 0x10) >> 4)  // in bl mode 
 
 
 static u8 bl_cmd[] = {
	 0x00, 0xFF, 0xA5,
	 0x00, 0x01, 0x02,
	 0x03, 0x04, 0x05,
	 0x06, 0x07};
	 //exit bl mode 
	 struct tpd_operation_data_t{
		 U8 hst_mode;
		 U8 tt_mode;
		 U8 tt_stat;
		 
		 U8 x1_M,x1_L;
		 U8 y1_M,y1_L;
		 U8 z1;
		 U8 touch12_id;
	 
		 U8 x2_M,x2_L;
		 U8 y2_M,y2_L;
		 U8 z2;
		 U8 gest_cnt;
		 U8 gest_id;
		 U8 gest_set; 
	 
		  
		 U8 x3_M,x3_L;
		 U8 y3_M,y3_L;
		 U8 z3;
		 U8 touch34_id;
	 
		 U8 x4_M,x4_L;
		 U8 y4_M,y4_L;
		 U8 z4;
	 };
	 struct tpd_bootloader_data_t{
		 U8 bl_file;
		 U8 bl_status;
		 U8 bl_error;
		 U8 blver_hi,blver_lo;
		 U8 bld_blver_hi,bld_blver_lo;
	 
		 U8 ttspver_hi,ttspver_lo;
		 U8 appid_hi,appid_lo;
		 U8 appver_hi,appver_lo;
	 
		 U8 cid_0;
		 U8 cid_1;
		 U8 cid_2;
		 
	 };
	 struct tpd_sysinfo_data_t{
				  U8   hst_mode;
				  U8  mfg_cmd;
				  U8  mfg_stat;
				  U8 cid[3];
				  u8 tt_undef1;
 
				  u8 uid[8];
				  U8  bl_verh;
				  U8  bl_verl;
 
				  u8 tts_verh;
				  u8 tts_verl;

				  U8 app_idh;
				   U8 app_idl;
				   U8 app_verh;
				   U8 app_verl;

				   u8 tt_undef2[6];
				   U8  act_intrvl;
				   U8  tch_tmout;
				   U8  lp_intrvl;
	 
				  
				 
					  
	 };

struct touch_info {
    int x1, y1;
    int x2, y2;
	int x3, y3;
    int p1, p2,p3;
    int count;
};
 
 static struct tpd_operation_data_t g_operation_data;
 static struct tpd_bootloader_data_t g_bootloader_data;
 static struct tpd_sysinfo_data_t g_sysinfo_data;
 
 static const struct i2c_device_id tpd_id[] = {{TPD_DEVICE,0},{}};
 unsigned short force[] = {2,0xCE,I2C_CLIENT_END,I2C_CLIENT_END};
 static const unsigned short * const forces[] = { force, NULL };
 static struct i2c_client_address_data addr_data = { .forces = forces, };
 
 
 static struct i2c_driver tpd_driver = {
  .driver = {
	 .name = TPD_DEVICE,
	 .owner = THIS_MODULE,
  },
  .probe = tpd_probe,
  .remove = __devexit_p(tpd_remove),
  .id_table = tpd_id,
  .detect = tpd_detect,
  .address_data = &addr_data,
 };
 
 void tpd_down(int x, int y, int p) {
	 input_report_abs(tpd->dev, ABS_PRESSURE, p);
	 input_report_key(tpd->dev, BTN_TOUCH, 1);
	 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
	 //printk("D[%4d %4d %4d] ", x, y, p);
	 input_mt_sync(tpd->dev);
	 TPD_DOWN_DEBUG_TRACK(x,y);
 }
 
 int tpd_up(int x, int y,int *count) {
	// if(*count>0) {
	//	 input_report_abs(tpd->dev, ABS_PRESSURE, 0);
		 input_report_key(tpd->dev, BTN_TOUCH, 0);
	//	 input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
	//	 input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
	//	 input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
		 //printk("U[%4d %4d %4d] ", x, y, 0);
		 input_mt_sync(tpd->dev);
		 TPD_UP_DEBUG_TRACK(x,y);
	//	 (*count)--;
		 return 1;
	 //} return 0;
 }

 int tpd_touchinfo(struct touch_info *cinfo, struct touch_info *pinfo)
 	{

	u32 retval;
	static u8 tt_mode;
	pinfo->count = cinfo->count;
	
	TPD_DEBUG("pinfo->count =%d\n",pinfo->count);
	
	retval = i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE, 8, (u8 *)&g_operation_data);
	retval += i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE + 8, 8, (((u8 *)(&g_operation_data)) + 8));
	retval += i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE + 16, 8, (((u8 *)(&g_operation_data)) + 16));
	retval += i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE + 24, 8, (((u8 *)(&g_operation_data)) + 24));


	 TPD_DEBUG("received raw data from touch panel as following:\n");
		 
		 TPD_DEBUG("hst_mode = %02X, tt_mode = %02X, tt_stat = %02X\n", \
				 g_operation_data.hst_mode,\
				 g_operation_data.tt_mode,\
				 g_operation_data.tt_stat);
		#if 0 
		 TPD_DEBUG("x1_M = %02X, x1_L = %02X, y1_M = %02X, y1_L = %02X, z1 = %02X, touch12_id = %02X\n", \
				 g_operation_data.x1_M,\
				   g_operation_data.x1_L,\
				 g_operation_data.y1_M,\
				   g_operation_data.y1_L,\
				 g_operation_data.z1, \
				 g_operation_data.touch12_id);
		 TPD_DEBUG("x2_M = %02X, x2_L = %02X, y2_M = %02X, y2_L = %02X,z2 = %02X\n", \
				 g_operation_data.x2_M,\
				   g_operation_data.x2_L,\
				 g_operation_data.y2_M,\
				   g_operation_data.y2_L,\
				 g_operation_data.z2);
		 TPD_DEBUG("gest_cnt = %02X, gest_id = %02X, gest_set = %02X\n", \
				 g_operation_data.gest_cnt, \
				 g_operation_data.gest_id, \
				 g_operation_data.gest_set);
 
		 TPD_DEBUG("x3_M = %02X, x3_L = %02X, y3_M = %02X, y3_L = %02X, z3 = %02X, touch34_id = %02X\n", \
				 g_operation_data.x3_M,\
				   g_operation_data.x3_L,\
				 g_operation_data.y3_M,\
				   g_operation_data.y3_L,\
				 g_operation_data.z3, \
				 g_operation_data.touch34_id);
		 TPD_DEBUG("x4_M = %02X, x4_L = %02X, y4_M = %02X, y4_L = %02X, z4 = %02X\n", \
				 g_operation_data.x4_M,\
				   g_operation_data.x4_L,\
				 g_operation_data.y4_M,\
				   g_operation_data.y4_L,\
				 g_operation_data.z4);
        #endif 
	cinfo->count = (g_operation_data.tt_stat & 0x0f) ; //point count

	TPD_DEBUG("cinfo->count =%d\n",cinfo->count);

	TPD_DEBUG("Procss raw data...\n");

	cinfo->x1 = (( g_operation_data.x1_M << 8) | ( g_operation_data.x1_L)); //point 1		
	cinfo->y1  = (( g_operation_data.y1_M << 8) | ( g_operation_data.y1_L));
	cinfo->p1 = g_operation_data.z1;

	TPD_DEBUG("Before: cinfo->x1= %3d, 	cinfo->y1 = %3d, cinfo->p1 = %3d\n", cinfo->x1, cinfo->y1 , cinfo->p1);		
		
		cinfo->x1 =  cinfo->x1 * 480 >> 11; //calibrate
		cinfo->y1 =  cinfo->y1 * 800 >> 11; 
		
	TPD_DEBUG("After:	cinfo->x1 = %3d, cinfo->y1 = %3d, cinfo->p1 = %3d\n", cinfo->x1 ,cinfo->y1 ,cinfo->p1);

	if(cinfo->count >1) {
		
		 cinfo->x2 = (( g_operation_data.x2_M << 8) | ( g_operation_data.x2_L)); //point 2
		 cinfo->y2 = (( g_operation_data.y2_M << 8) | ( g_operation_data.y2_L));
		 cinfo->p2 = g_operation_data.z2;
			
	TPD_DEBUG("before:	 cinfo->x2 = %3d, cinfo->y2 = %3d,  cinfo->p2 = %3d\n", cinfo->x2, cinfo->y2,  cinfo->p2);	  
                      cinfo->x2 =  cinfo->x2 * 480 >> 11; //calibrate
					  cinfo->y2 =  cinfo->y2 * 800 >> 11; 
	TPD_DEBUG("After:	 cinfo->x2 = %3d, cinfo->y2 = %3d,  cinfo->p2 = %3d\n", cinfo->x2, cinfo->y2, cinfo->p2);	
		
	     if (cinfo->count > 2)
		   {
		     cinfo->x3 = (( g_operation_data.x3_M << 8) | ( g_operation_data.x3_L)); //point 3
	         cinfo->y3 = (( g_operation_data.y3_M << 8) | ( g_operation_data.y3_L));
		     cinfo->p3 = g_operation_data.z3;

		 TPD_DEBUG("before:	 cinfo->x3 = %3d, cinfo->y3 = %3d, cinfo->p3 = %3d\n", cinfo->x3, cinfo->y3, cinfo->p3);	  		  
                      cinfo->x3 =  cinfo->x3 * 480 >> 11; //calibrate
					  cinfo->y3 =  cinfo->y3 * 800 >> 11; 	

	     TPD_DEBUG("After:	 cinfo->x3 = %3d, cinfo->y3 = %3d, cinfo->p3= %3d\n", cinfo->x3, cinfo->y3, cinfo->p3);	  		  
	      }
		}
	
	if (!cinfo->count) return true; // this is a touch-up event
    
    if (g_operation_data.tt_mode & 0x20) return false; // buffer is not ready for use
    
// data toggle 
	u8 data0,data1;
	
	data0 = i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE, 1, (u8*)&g_operation_data);
	TPD_DEBUG("before hst_mode = %02X \n", g_operation_data.hst_mode);
	
	if((g_operation_data.hst_mode & 0x80)==0)
				  g_operation_data.hst_mode = g_operation_data.hst_mode|0x80;
			 else
				 g_operation_data.hst_mode = g_operation_data.hst_mode & (~0x80);
	
			 TPD_DEBUG("after hst_mode = %02X \n", g_operation_data.hst_mode);
	data1 = i2c_smbus_write_i2c_block_data(i2c_client, TPD_REG_BASE, sizeof(g_operation_data.hst_mode), &g_operation_data.hst_mode);
	

	if (tt_mode == g_operation_data.tt_mode) return false; // sampling not completed
	 else tt_mode = g_operation_data.tt_mode;
	 
	 return true;

 	};

 static int touch_event_handler(void *unused)
 {

     int pending = 0;
     struct touch_info cinfo, pinfo;
	 struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
	 sched_setscheduler(current, SCHED_RR, &param);
 
	 do
	 {
		 mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
		 set_current_state(TASK_INTERRUPTIBLE); 
		 if(!kthread_should_stop())
		 {
			 TPD_DEBUG_CHECK_NO_RESPONSE;
			 do
			 {
			   if (pending) 
					 wait_event_interruptible_timeout(waiter, tpd_flag != 0, HZ/10);
				 else 
					 wait_event_interruptible_timeout(waiter,tpd_flag != 0, HZ*2);
				 
			 }while(0);
			 
			 if (tpd_flag == 0 && !pending) 
				 continue; // if timeout for no touch, then re-wait.
			 
			 if (tpd_flag != 0 && pending > 0)	
				 pending = 0;
			 
			 tpd_flag = 0;
			 TPD_DEBUG_SET_TIME;
			 
		 }
		 set_current_state(TASK_RUNNING);

		  if (tpd_touchinfo(&cinfo, &pinfo)) {
            if(cinfo.count >0) {
                tpd_down(cinfo.x1, cinfo.y1, cinfo.p1);
             if(cinfo.count>1)
             	{
			 	tpd_down(cinfo.x2, cinfo.y2, cinfo.p2);
			   if(cinfo.count>2) tpd_down(cinfo.x3, cinfo.y3, cinfo.p3);
             	}
                input_sync(tpd->dev);
				printk("press --->\n");
				
            } else if(cinfo.count==0 && pinfo.count!=0) {
            printk("release --->\n"); 
	    tpd_up(0, 0, 0);
                input_mt_sync(tpd->dev);
                input_sync(tpd->dev);
            }
        }
		   
 }while(!kthread_should_stop());
 
	 return 0;
 }
 
 static int tpd_detect (struct i2c_client *client, int kind, struct i2c_board_info *info) 
 {
	int error;
 
	 hwPowerDown(TPD_POWER_SOURCE,"TP");
	 hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
 
	 
	 mt_set_gpio_mode(GPIO61, 0x01);
	 mt_set_gpio_dir(GPIO61, GPIO_DIR_IN);
	 mt_set_gpio_pull_enable(GPIO61, GPIO_PULL_ENABLE);
	 mt_set_gpio_pull_select(GPIO61, GPIO_PULL_UP);
	 msleep(100);
 
	 strcpy(info->type, TPD_DEVICE);
 
	 
	 thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
	  if (IS_ERR(thread))
		 { 
		  error = PTR_ERR(thread);
		  TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", error);
		}
 
	  return 0;
 }
 
 static void tpd_eint_interrupt_handler(void)
 {
	// MT6516_EINTIRQMask(CUST_EINT_TOUCH_PANEL_NUM); //1009 mask eint
	 TPD_DEBUG("TPD interrupt has been triggered\n");
	 tpd_flag = 1;
	 wake_up_interruptible(&waiter);
	 
 
 }
 static int __devinit tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
 {	 
 
	int error;
	int retval = TPD_OK;
	i2c_client = client;
	
 
	error = tpd_initialize(client);
  if (error)
	 {
	   TPD_DEBUG("tpd_initialize error\n");
	 }
 
#ifdef CONFIG_HAS_EARLYSUSPEND
		  if (!(retval < TPD_OK)) 
		  {
			  early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
			  early_suspend.suspend = tpd_early_suspend;
			  early_suspend.resume = tpd_late_resume;
			  register_early_suspend(&early_suspend);
		  }
#endif
  
	  //MT6516_EINT_Set_Sensitivity(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
	  //MT6516_EINT_Set_HW_Debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
	  mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
	  mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
 
	msleep(100);
 
	TPD_DMESG("Touch Panel Device Probe %s\n", (retval < TPD_OK) ? "FAIL" : "PASS");
	  
   return retval;
   
 }
 
 static int tpd_get_bl_info (int show)
 {
   int retval = TPD_OK;
 
	retval = i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE, 8, (u8 *)&g_bootloader_data);
	retval += i2c_smbus_read_i2c_block_data(i2c_client, TPD_REG_BASE + 8, 8, (((u8 *)&g_bootloader_data) + 8));
	if (show)
	 {
 
	   TPD_DEBUG("BL: bl_file = %02X, bl_status = %02X, bl_error = %02X, blver = %02X%02X, bld_blver = %02X%02X\n", \
				 g_bootloader_data.bl_file, \
				 g_bootloader_data.bl_status, \
				 g_bootloader_data.bl_error, \
				 g_bootloader_data.blver_hi, g_bootloader_data.blver_lo, \
				 g_bootloader_data.bld_blver_hi, g_bootloader_data.bld_blver_lo);
	
		 TPD_DEBUG("BL: ttspver = 0x%02X%02X, appid=0x%02X%02X, appver=0x%02X%02X\n", \
				 g_bootloader_data.ttspver_hi, g_bootloader_data.ttspver_lo, \
				 g_bootloader_data.appid_hi, g_bootloader_data.appid_lo, \
				 g_bootloader_data.appver_hi, g_bootloader_data.appver_lo);
	 
		 TPD_DEBUG("BL: cid = 0x%02X%02X%02X\n", \
				 g_bootloader_data.cid_0, \
				 g_bootloader_data.cid_1, \
				 g_bootloader_data.cid_2);
	 
	 }
	mdelay(1000);
	
	return retval;
 }
 
 static int tpd_power_on ()
 {
   int retval = TPD_OK;
   int tries = 0;
   u8 host_reg;
 
   
  TPD_DEBUG("Power on\n");
 
 host_reg = TPD_SOFT_RESET_MODE;
 
 retval = i2c_smbus_write_i2c_block_data(i2c_client,TPD_REG_BASE,sizeof(host_reg),&host_reg);
 
 do{
	 mdelay(1000);
	 retval = tpd_get_bl_info(TRUE);
	 }while(!(retval < TPD_OK) && !GET_BOOTLOADERMODE(g_bootloader_data.bl_status)&& 
	 !(g_bootloader_data.bl_file == TPD_OP_MODE + TPD_LOW_PWR_MODE) && tries++ < 10);
 
	 if(!(retval < TPD_OK))
		 {
		 host_reg = TPD_OP_MODE;
		 retval = i2c_smbus_write_i2c_block_data(i2c_client,TPD_REG_BASE,sizeof(host_reg),&host_reg);
		 mdelay(1000);
		 }
	 if(!(retval < TPD_OK))
		 {
		 TPD_DEBUG("Switch to sysinfo mode \n");
		 host_reg = TPD_SYSINFO_MODE;
		 retval = i2c_smbus_write_i2c_block_data(i2c_client,TPD_REG_BASE,sizeof(host_reg),&host_reg);
		 mdelay(1000);
 
		if(!(retval < TPD_OK))
		 {
		 retval = i2c_smbus_read_i2c_block_data(i2c_client,TPD_REG_BASE, 8, (u8 *)&g_sysinfo_data);
		 retval += i2c_smbus_read_i2c_block_data(i2c_client,TPD_REG_BASE + 8, 8, (((u8 *)(&g_sysinfo_data)) + 8));
		 retval += i2c_smbus_read_i2c_block_data(i2c_client,TPD_REG_BASE + 16, 8, (((u8 *)(&g_sysinfo_data)) + 16));
		 retval += i2c_smbus_read_i2c_block_data (i2c_client,TPD_REG_BASE + 24, 8, (((u8 *)(&g_sysinfo_data)) + 24));
		 
		 TPD_DEBUG("SI: hst_mode = 0x%02X, mfg_cmd = 0x%02X, mfg_stat = 0x%02X\n", \
					 g_sysinfo_data.hst_mode, \
					 g_sysinfo_data.mfg_cmd, \
					 g_sysinfo_data.mfg_stat);
			 TPD_DEBUG("SI: bl_ver = 0x%02X%02X\n", \
					 g_sysinfo_data.bl_verh, \
					 g_sysinfo_data.bl_verl);
			 TPD_DEBUG("SI: act_int = 0x%02X, tch_tmout = 0x%02X, lp_int = 0x%02X\n", \
					 g_sysinfo_data.act_intrvl, \
					 g_sysinfo_data.tch_tmout, \
					 g_sysinfo_data.lp_intrvl);
			 TPD_DEBUG("SI: tver = %02X%02X, a_id = %02X%02X, aver = %02X%02X\n", \
					 g_sysinfo_data.tts_verh, \
					 g_sysinfo_data.tts_verl, \
					 g_sysinfo_data.app_idh, \
					 g_sysinfo_data.app_idl, \
					 g_sysinfo_data.app_verh, \
					 g_sysinfo_data.app_verl);
			 TPD_DEBUG("SI: c_id = %02X%02X%02X\n", \
					 g_sysinfo_data.cid[0], \
					 g_sysinfo_data.cid[1], \
					 g_sysinfo_data.cid[2]);
		 }
 
		 TPD_DEBUG("Switch back to operational mode \n");
		  if (!(retval < TPD_OK)) 
			 {
			 host_reg = TPD_OP_MODE;
			 retval = i2c_smbus_write_i2c_block_data(i2c_client, TPD_REG_BASE, sizeof(host_reg), &host_reg);
			 mdelay(1000);
			 }
		 
		 }
 
	 return retval;
	 
 }
 static int tpd_initialize(struct i2c_client * client)
 {
   int retval = TPD_OK;
 
   retval = tpd_power_on();
 
   return retval;
 }
 static int __devexit tpd_remove(struct i2c_client *client)
 
 {
   int error;
 
  #ifdef CONFIG_HAS_EARLYSUSPEND
	 unregister_early_suspend(&early_suspend);
  #endif /* CONFIG_HAS_EARLYSUSPEND */
   
	 TPD_DEBUG("TPD removed\n");
 
   return 0;
 }
 
 
 int tpd_local_init(void)
 {
   int retval;
 
  TPD_DMESG("Cypress CY8CTMA300 I2C Touchscreen Driver (Built %s @ %s)\n", __DATE__, __TIME__);
 
 
   retval = i2c_add_driver(&tpd_driver);
 
   return retval;
 
 }
 
 int tpd_resume(struct i2c_client *client)
 {
  int retval = TPD_OK;
  hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
   TPD_DEBUG("TPD wake up\n");
   mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
	
	 return retval;
 }
 
 int tpd_suspend(struct i2c_client *client, pm_message_t message)
 {
	 int retval = TPD_OK;
 
	 TPD_DEBUG("TPD enter sleep\n");
	 mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	 hwPowerDown(TPD_POWER_SOURCE,"TP");
	 return retval;
 }
 
#ifdef CONFIG_HAS_EARLYSUSPEND
 static void tpd_early_suspend(struct early_suspend *handler)
 {
	 tpd_suspend(i2c_client, PMSG_SUSPEND);
 }
 
 static void tpd_late_resume(struct early_suspend *handler)
 {
	 tpd_resume(i2c_client);
 }
#endif
 
