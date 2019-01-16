#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <mach/mt6516_pll.h>
#include <mach/mt6516_gpio.h>
#include <mach/mt6516_devs.h>
#include <mach/mt6516_typedefs.h>

#include <linux/interrupt.h>
#include <linux/time.h>
#include <mach/mt6516_boot.h>

#include "tpd_custom_eeti.h"
#include "tpd.h"

#include <cust_eint.h>
#include <linux/rtpm_prio.h>

#define TPD_SLAVE_ADDR 0x14

struct touch_info {
    int x1, y1;
    int x2, y2;
    int p, count, pending;
};
struct touch_elapse {
    int t1, t2, i;
    int buf[5];
};

extern struct tpd_device *tpd;
extern int tpd_firmware_version[2];

static void tpd_eint_interrupt_handler(void);
static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int tpd_i2c_remove(struct i2c_client *client);
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
extern int tpd_trembling_tolerance(int t, int p); 

static unsigned short normal_i2c[] = { TPD_SLAVE_ADDR,  I2C_CLIENT_END };
static unsigned short ignore = I2C_CLIENT_END;
static struct i2c_client *i2c_client = NULL;
struct task_struct *thread = NULL;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int tpd_flag=0;
static int boot_mode = 0;
static int touch_event_handler(void *unused);

static int raw_x1, raw_y1, raw_x2, raw_y2;
static int tpd_status = 0;

static const struct i2c_device_id tpd_i2c_id[] = {{"mt6516-tpd",0},{}};
unsigned short force[] = {2, 0x14, I2C_CLIENT_END, I2C_CLIENT_END};
static const unsigned short * const forces[] = { force, NULL };
static struct i2c_client_address_data addr_data = { .forces = forces, };

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

static struct i2c_driver tpd_i2c_driver = {
    .probe = tpd_i2c_probe,
    .remove = tpd_i2c_remove,
    .detect = tpd_i2c_detect,
    .driver.name = "mt6516-tpd",
    .id_table = tpd_i2c_id,
    .address_data = &addr_data,
};

static int tpd_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) {
    strcpy(info->type, "mt6516-tpd");
    return 0;
}

static int tpd_i2c_remove(struct i2c_client *client) {
    i2c_unregister_device(client);
    printk("[mt6516-tpd] touch panel i2c device is removed.\n");
    return 0;
}

static int tpd_local_init(void) {
    boot_mode = get_boot_mode();
    // Software reset mode will be treated as normal boot
    if(boot_mode==3) boot_mode = NORMAL_BOOT;
    //boot_mode = UNKNOWN_BOOT;
    if(i2c_add_driver(&tpd_i2c_driver)!=0) {
      TPD_DMESG("unable to add i2c driver.\n");
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
    memcpy(tpd_calmat, tpd_def_calmat_local, 8*4);
    memcpy(tpd_def_calmat, tpd_def_calmat_local, 8*4);	
#endif  
		TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);  
		tpd_type_cap = 1;
    return 0;
}

static int tpd_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    char wakeup[2] = {0x07,0x02};
    char threshold[2] = {0x09, 0x04};
    char gesture[2]   = {0x08, 0x11};
    char idletime[2]  = {0x0c, 0xff};
    char sleeptime[2]   = {0x0d, 0x01};
    char firmware[2] = {0x0a, 0x44};
    int err = 0;
    i2c_client = client;
    hwPowerDown(TPD_POWER_SOURCE,"TP");
    hwPowerOn(TPD_POWER_SOURCE,VOL_3300,"TP");
    msleep(50);

    /* added in android 2.2, for configuring EINT2 */
    mt_set_gpio_mode(GPIO61, 0x01);
    mt_set_gpio_pull_enable(GPIO61, 1);
    mt_set_gpio_pull_select(GPIO61,1);

    //MT6516_EINT_Set_Sensitivity(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE);
    //MT6516_EINT_Set_HW_Debounce(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, tpd_eint_interrupt_handler, 1);
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    //msleep(20);
    if(i2c_master_send(i2c_client,wakeup,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }
    if(i2c_master_send(i2c_client,wakeup,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }
    if(i2c_master_send(i2c_client,threshold,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }
    if(i2c_master_send(i2c_client,gesture,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }
    if(i2c_master_send(i2c_client,idletime,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }
    if(i2c_master_send(i2c_client,sleeptime,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1;  
    }    
    if(i2c_master_send(i2c_client,firmware,2) < 0)
    {
    	TPD_DMESG("I2C transfer error, line: %d\n", __LINE__);
    	return -1; 
    }  
   tpd_load_status = 1;

    thread = kthread_run(touch_event_handler, 0, TPD_DEVICE);
    if (IS_ERR(thread)) { 
        err = PTR_ERR(thread);
        TPD_DMESG(TPD_DEVICE " failed to create kernel thread: %d\n", err);
    }    
    tpd_status = 1;
    
    return 0;
}

static void tpd_eint_interrupt_handler(void) {
    TPD_DEBUG_PRINT_INT; tpd_flag=1; wake_up_interruptible(&waiter);
}


static void tpd_down(int raw_x, int raw_y, int x, int y, int p) {
    input_report_abs(tpd->dev, ABS_PRESSURE, p);
    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, p);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
    //printk("D[%4d %4d %4d] ", x, y, p);
    input_mt_sync(tpd->dev);
    TPD_DOWN_DEBUG_TRACK(x,y);
    TPD_EM_PRINT(raw_x, raw_y, x, y, p, 1);
}

static int tpd_up(int raw_x, int raw_y, int x, int y,int *count) {
    if(*count>0) {
        //input_report_abs(tpd->dev, ABS_PRESSURE, 0);
        input_report_key(tpd->dev, BTN_TOUCH, 0);
        //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
        //input_report_abs(tpd->dev, ABS_MT_POSITION_X, x);
        //input_report_abs(tpd->dev, ABS_MT_POSITION_Y, y);
        //printk("U[%4d %4d %4d] ", x, y, 0);
        input_mt_sync(tpd->dev);
        TPD_UP_DEBUG_TRACK(x,y);
        TPD_EM_PRINT(raw_x, raw_y, x, y, 0, 0);
        (*count)--;
        return 1;
    } return 0;
}

static void tpd_findmapping(struct touch_info *cinfo, struct touch_info *sinfo, int *mapping) {
    int p1, p2, p3, p4;
    if(sinfo->count>=2) {
        p1 = cinfo->x1-sinfo->x1; p2 = cinfo->x2-sinfo->x1;
        p3 = cinfo->x1-sinfo->x2; p4 = cinfo->x2-sinfo->x2;
        p1 = p1*p1; p2 = p2*p2; p3 = p3*p3; p4 = p4*p4;
        if((p1<p2 && p1<p3) || (p4<p3 && p4<p2)) mapping[0]=2, mapping[1]=3;
        else mapping[0]=3, mapping[1]=2;

        p1 = cinfo->y1-sinfo->y1; p2 = cinfo->y2-sinfo->y1;
        p3 = cinfo->y1-sinfo->y2; p4 = cinfo->y2-sinfo->y2;
        p1 = p1*p1; p2 = p2*p2; p3 = p3*p3; p4 = p4*p4;
        if((p1<p2 && p1<p3) || (p4<p3 && p4<p2)) mapping[2]=4, mapping[3]=5;
        else mapping[2]=5, mapping[3]=4;
    } else {
        p1 = cinfo->x1 - sinfo->x1; p2 = cinfo->x1-sinfo->x2; p1 = p1*p1; p2 = p2*p2;
        if(p1<p2) mapping[0]=2, mapping[1]=3; else mapping[0]=3, mapping[1]=2;
        p1 = cinfo->y1 - sinfo->y1; p2 = cinfo->y1-sinfo->y2; p1 = p1*p1; p2 = p2*p2;
        if(p1<p2) mapping[2]=4, mapping[3]=5; else mapping[2]=5, mapping[3]=4;
    }
}

static int tpd_gettouchinfo(struct touch_info *cinfo, struct touch_info *sinfo) {
    char data[6] = {0,0,0,0,0,0};
    static struct touch_elapse elapse;
    static struct touch_info oinfo;
    static int px = 0, py = 0;
    static int mapping[4] = {2,3,4,5};
    static int pdata[6] = {-1,-1,-1,-1,-1,-1};
    static int repeat[6] = { 0, 0, 0, 0, 0, 0};
    static int fat_touch = 0;
    int x, y, i, average, count;
    if(i2c_master_recv(i2c_client, data, 6)==6) {
        TPD_DEBUG("[%x %x %x %x %x %x]\n",data[0],data[1],data[2],data[3],data[4],data[5]);
        if(data[0]==0x81 || data[0]==0xc0 || data[0]==0x80) {
            x   = (((unsigned int)(data[1] & 0x0f))<<7)+((unsigned int)(data[2] & 0x7f));
            y   = (((unsigned int)(data[3] & 0x0f))<<7)+((unsigned int)(data[4] & 0x7f));
            raw_x1 = x; raw_y1 = y;
            cinfo->p   = data[5]+1;
            if(cinfo->p>TPD_FAT_TOUCH) fat_touch = 1; // don't react if too fat 
            tpd_calibrate(&x, &y);
            #ifdef TPD_HAVE_TREMBLE_ELIMINATION
            /*tremble elimination */
            if(data[0] & 0x01) {
                elapse.t2 = jiffies;
                if(!elapse.t1) elapse.t1 = elapse.t2;
                elapse.buf[elapse.i] = elapse.t2 - elapse.t1;
                elapse.i = ((elapse.i+1)%5);
                for(i=0,average=0,count=0;i<5;i++)
                    if(elapse.buf[i]) count++, average+=elapse.buf[i];
                if(count) average /= count;
								#ifdef TPD_CUSTOM_TREMBLE_TOLERANCE                	
                if(( (px-x)*(px-x)+(py-y)*(py-y)) < tpd_trembling_tolerance(average, cinfo->p)) {
                #else
                if(( (px-x)*(px-x)+(py-y)*(py-y)) < tpd_trembling_tolerance_defalut(average, cinfo->p)) {
                #endif	
                    x=px,y=py;
                } else px = x, py = y;
                elapse.t1 = elapse.t2;
            }
            #endif
            cinfo->x1 = x;
            cinfo->y1 = y;
            cinfo->count=1;
            if(data[0]==0x80) cinfo->count=0;
            #ifdef TPD_HAVE_BUTTON
            if(boot_mode!=NORMAL_BOOT && cinfo->y1>=TPD_RES_Y && cinfo->y1<TPD_BUTTON_HEIGHT) cinfo->count = 0;
            #endif
        } else if(data[0]==0x4a && data[1]==0x04) {
            
            for(i=2;i<6;i++) if(pdata[i]==-1) pdata[i]=(int)data[i];
            if(data[2]>7 || data[2]-pdata[2]>1) data[2]=pdata[2];
            if(data[3]>7 || data[3]-(int)pdata[3]<-1) data[3]=(char)pdata[3];
            if(data[4]>11 || data[4]-(int)pdata[4]<-1) data[4]=(char)pdata[4];
            if(data[5]>11 || data[5]-(int)pdata[5]>1) data[5]=(char)pdata[5];
            for(i=2;i<6;i++) {
                // trembling elimination - currently disabled
                //if(repeat[i]>2 && (data[i]-pdata[i]==1 || data[i]-pdata[i]==-1)) { data[i]=pdata[i]; repeat[i]-=2; }
                //if(pdata[i]==data[i]) repeat[i]++; else repeat[i]=0;
                pdata[i] = (int)data[i];
            }

            // ghost point resolving
            oinfo.count = sinfo->count;
            oinfo.x1 = (2048*(data[2]+1))/8;
            oinfo.x2 = (2048*(data[3]+1))/8;
            oinfo.y1 = (2048*(data[4]+1))/12;
            oinfo.y2 = (2048*(data[5]+1))/12;
            tpd_calibrate(&(oinfo.x1), &(oinfo.y1));
            tpd_calibrate(&(oinfo.x2), &(oinfo.y2));
            tpd_findmapping(cinfo, &oinfo, mapping);

            cinfo->x1 = (2048*(data[mapping[0]]+1))/8;
            cinfo->x2 = (2048*(data[mapping[1]]+1))/8;
            cinfo->y1 = (2048*(data[mapping[2]]+1))/12;
            cinfo->y2 = (2048*(data[mapping[3]]+1))/12;
            raw_x1 = cinfo->x1; raw_y1 = cinfo->y1;
            raw_x2 = cinfo->x2; raw_y2 = cinfo->y2;
            tpd_calibrate(&(cinfo->x1), &(cinfo->y1));
            tpd_calibrate(&(cinfo->x2), &(cinfo->y2));
            cinfo->count = 2;
        } else if(data[0]==0x0a && data[1]==0x03 && data[2]==0x38 && data[4]==0) {
            cinfo->count = 0;
        } else if(data[0]==0x0a && data[1]==0x04 && data[2]==0x44) {
            tpd_firmware_version[0] = data[3]-48;
            tpd_firmware_version[1] = ((data[4]-48)*10) + (data[5]-48);

            printk("[mt6516-tpd] EETI Touch Panel Firmware Version %d.%d\n", 
                tpd_firmware_version[0], tpd_firmware_version[1]);
            if(tpd_firmware_version[0]!=1 || tpd_firmware_version[1]!=15)
                printk("[mt6516-tpd] Panel Firmware Version Mismatched!");
        }
        if(cinfo->count!=1) memset(&elapse, 0, sizeof(elapse));
        if(data[0]==0xc0 || (cinfo->count==0 && sinfo->count==2)) {
            cinfo->pending = 0;
            cinfo->count = 0;
        }
        if(cinfo->count!=2) {
            // trembling elimination - currently disabled
            //repeat[2]= 0; repeat[3] =0; repeat[4]= 0; repeat[5]= 0;
            pdata[2]=-1;  pdata[3]=-1; pdata[4]=-1; pdata[5]=-1;
        }
    } else { 
        printk(TPD_DEVICE " - failed to retrieve touch data\n"); 
        return 1;
    }
    if(cinfo->count==0 && fat_touch==1) {
        fat_touch = 0;
        return 1;
    } else return fat_touch?1:0;
}

static void tpd_smoothing(struct touch_info *cinfo,struct touch_info *sinfo) {
    sinfo->x1 = cinfo->x1; sinfo->y1 = cinfo->y1;
    sinfo->x2 = cinfo->x2; sinfo->y2 = cinfo->y2;
}

static int touch_event_handler(void *unused) {
    struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };
    struct touch_info cinfo, sinfo;
    int pending = 0, down = 0;
    struct touch_info buf[3];
    int buf_p=1, buf_c=2, buf_f=0; 
    int dx;

    cinfo.pending=0;
    sched_setscheduler(current, SCHED_RR, &param);
    do {
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
        set_current_state(TASK_INTERRUPTIBLE);
        if (!kthread_should_stop()) {
            TPD_DEBUG_CHECK_NO_RESPONSE;
            do {
                if(pending) wait_event_interruptible_timeout(waiter, tpd_flag!=0, HZ/10);
                else wait_event_interruptible_timeout(waiter,tpd_flag!=0, HZ*2);
            } while(0);
            if(tpd_flag==0 && !pending) continue; // if timeout for no touch, then re-wait.
            if(tpd_flag!=0 && pending>0)  pending=0;
            tpd_flag=0;
            TPD_DEBUG_SET_TIME;
        }
        set_current_state(TASK_RUNNING);
        
        if(!pending) if(tpd_gettouchinfo(&cinfo, &sinfo)) continue; 
        if(pending>1) { pending--; continue; }
        if(cinfo.count==-1) continue;
        if(tpd_mode==TPD_MODE_KEYPAD && 
            ((tpd_mode_axis==0 && cinfo.y1>=tpd_mode_min && cinfo.y1<=tpd_mode_max) ||
             (tpd_mode_axis==1 && cinfo.x1>=tpd_mode_min && cinfo.x1<=tpd_mode_max))) {
            buf_f = ((buf_f+1)%3);
            buf_c = ((buf_f+2)%3);
            buf_p = ((buf_f+1)%3);
            buf[buf_f].x1 = cinfo.x1;
            buf[buf_f].y1 = cinfo.y1;
            dx = cinfo.x1 - buf[buf_c].x1;
            buf[buf_f].count = (cinfo.count?(dx*dx<tpd_mode_keypad_tolerance?buf[buf_c].count+1:1):0);
            if(buf[buf_c].count<2) if(tpd_up(raw_x1, raw_y1, buf[buf_p].x1, buf[buf_p].y1,&down)) input_sync(tpd->dev);
            if(buf[buf_c].count>1 ||
               (buf[buf_c].count==1 && (
                buf[buf_p].count==0 || buf[buf_f].count==0
                || (buf[buf_f].x1-buf[buf_c].x1)*(buf[buf_c].x1-buf[buf_p].x1)<=0))) {
                tpd_down(raw_x1, raw_y1, buf[buf_c].x1, buf[buf_c].y1, 1); 
                input_sync(tpd->dev);
                down=1;
            } 
            if(cinfo.count==0) if(tpd_up(raw_x1, raw_y1, buf[buf_p].x1, buf[buf_p].y1,&down)) input_sync(tpd->dev);
        } else {

        switch(cinfo.count) {
            case 0:
                if(cinfo.pending>0) pending+=cinfo.pending, cinfo.pending=0;
                else {
                    if(sinfo.count>=2) {
                        if(pending==0) pending+=1; 
                        else {
                            if(tpd_up(raw_x1, raw_y1, sinfo.x1, sinfo.y1, &down) + tpd_up(raw_x2, raw_y2, sinfo.x2,sinfo.y2, &down)) 
                                input_sync(tpd->dev);
                            sinfo.count = 0;
                            pending = 0;
                        }
                    } else if(sinfo.count==1) {
                        #ifdef TPD_HAVE_BUTTON
                        if(boot_mode!=NORMAL_BOOT && tpd->btn_state) tpd_button(cinfo.x1, cinfo.y1,0);
                        #endif
                        if(tpd_up(raw_x1, raw_y1, cinfo.x1,cinfo.y1, &down)) input_sync(tpd->dev);
                        sinfo.count = 0;
                        pending=0;
                    } else pending = 0;
                }
                TPD_DEBUG_PRINT_UP;
                break;
            case 1:
                if(sinfo.count>=3 || pending==1) {
                    pending = 0;
                    if(sinfo.count==3 && down>1) {
                        if(tpd_up(raw_x1, raw_y1, sinfo.x1,sinfo.y1, &down)) input_sync(tpd->dev);
                        /*tpd_down(cinfo.x1, cinfo.y1, 1);
                        if(
                          (cinfo.x1-sinfo.x1)*(cinfo.x1-sinfo.x1)+(cinfo.y1-sinfo.y1)*(cinfo.y1-sinfo.y1)
                        > (cinfo.x1-sinfo.x2)*(cinfo.x1-sinfo.x2)+(cinfo.y1-sinfo.y2)*(cinfo.y1-sinfo.y2)
                        ) {
                          if(tpd_up(sinfo.x1,sinfo.y1, &down)) input_sync(tpd->dev);
                        } else {
                          if(tpd_up(sinfo.x2,sinfo.y2, &down)) input_sync(tpd->dev);
                        }*/
                    }
                } else if(sinfo.count==2) {
                    if(pending==0) pending=1;
                    else {
                        if(tpd_up(raw_x1, raw_y1, cinfo.x1,cinfo.y1, &down) + tpd_up(raw_x2, raw_y2, sinfo.x2,sinfo.y2, &down))
                            input_sync(tpd->dev);
                        sinfo.x1 = cinfo.x1; sinfo.y1=cinfo.y1;
                    }
                    sinfo.count = 3;
                } else {
                    #ifdef TPD_HAVE_BUTTON
                    if(boot_mode!=NORMAL_BOOT && cinfo.y1>=TPD_RES_Y) { 
                        if(tpd_up(raw_x1, raw_y1, cinfo.x1, cinfo.y1, &down)) input_sync(tpd->dev);
                        tpd_button(cinfo.x1, cinfo.y1, 1);
                        sinfo.count = 1;
                    } else 
                    #endif 
                    do {
                        #ifdef TPD_HAVE_BUTTON
                        if(boot_mode!=NORMAL_BOOT && tpd->btn_state) tpd_button(cinfo.x1,cinfo.y1,0);
                        #endif 
                        tpd_down(raw_x1, raw_y1, cinfo.x1,cinfo.y1, cinfo.p);
                        input_sync(tpd->dev);
                        down = 1;
                        sinfo.count = 1;
                    } while(0);
                }
                TPD_DEBUG_PRINT_DOWN;
                break;
            case 2:
                // hold one finger, press another, this code will release both fingers
                if(sinfo.count==3) {
                    if(tpd_up(raw_x1, raw_y1, sinfo.x1, sinfo.y1, &down) + tpd_up(raw_x2, raw_y2, sinfo.x2, sinfo.y2, &down))
                        input_sync(tpd->dev);
                }
                tpd_smoothing(&cinfo, &sinfo);
                tpd_down(raw_x1, raw_y1, sinfo.x1, sinfo.y1, 1);
                tpd_down(raw_x2, raw_y2, sinfo.x2, sinfo.y2, 1);
                down = 2;
                sinfo.count = 2;
                input_sync(tpd->dev);
                TPD_DEBUG_PRINT_DOWN;
                break;
            default: break;
        }
    } 
    } while (!kthread_should_stop());
    return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND

/* platform device functions */
static void tpd_suspend(struct early_suspend *h) {
    char sleep[2] = {0x07,0x01};
    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    //MT6516_IRQMask(MT6516_TOUCH_IRQ_LINE);
    i2c_master_send(i2c_client,sleep,2);
    // workaround: power down tp will also pull down ic2 bus, affect other drivers
    //             so not pull down it.
    //hwPowerDown(TPD_POWER_SOURCE,"TP");
}
static void tpd_resume(struct early_suspend *h) {
    char wakeup[2] = {0x07,0x02};
    char threshold[2] = {0x09, 0x04};
    char gesture[2]   = {0x08, 0x11};
    char sleeptime[2]   = {0x0d, 0x01};
    char idletime[2]  = {0x0c, 0xff};
    int i;
    for(i=0;i<TPD_WAKEUP_TRIAL;i++) {
        i2c_master_send(i2c_client,wakeup,2);
        if(i2c_master_send(i2c_client,wakeup,2)==2) break;
        msleep(TPD_WAKEUP_DELAY);
    }
    i2c_master_send(i2c_client,gesture,2);
    i2c_master_send(i2c_client,threshold,2);
    i2c_master_send(i2c_client,idletime,2);
    i2c_master_send(i2c_client,sleeptime,2);
    
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    //MT6516_EINTIRQUnmask(CUST_EINT_TOUCH_PANEL_NUM);    
}
#endif


/* switch touch panel into single scan mode for decreasing interference */
 void _tpd_switch_single_mode(void) {
    
    char mode[2] = {0x0b,0x01};
    TPD_DMESG("switch tpd into single scan mode\n");
    i2c_master_send(i2c_client,mode,2);
    
}

/* switch touch panel into multiple scan mode for better performance */
 void _tpd_switch_multiple_mode(void) {
    
    char mode[2] = {0x0b,0x00};
    TPD_DMESG("switch tpd into multiple scan mode\n");
    i2c_master_send(i2c_client,mode,2);
    
}

/* switch touch panel into deep sleep mode */
 void _tpd_switch_sleep_mode(void) {
    
    char sleep[2] = {0x07,0x00};
    
    if (!tpd_status) {
        TPD_DMESG("do not need to switch tpd into deep sleep m mode\n");
        return;
    }
    
    TPD_DMESG("switch tpd into deep sleep mode\n");

    mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
    //MT6516_IRQMask(MT6516_TOUCH_IRQ_LINE);
    i2c_master_send(i2c_client,sleep,2);
    
    tpd_status = 0;
    
    // workaround: power down tp will also pull down ic2 bus, affect other drivers
    //             so not pull down it.
    //hwPowerDown(TPD_POWER_SOURCE,"TP");
}

/* switch touch panel back to normal mode */
 void _tpd_switch_normal_mode(void) {
    
    char wakeup[2] = {0x07,0x02};
    char threshold[2] = {0x09, 0x04};
    char gesture[2] = {0x08, 0x11};
    char sleeptime[2] = {0x0d, 0x01};
    char idletime[2] = {0x0c, 0xff};
    int i;
    
    if (tpd_status) {
        TPD_DMESG("do not need to switch tpd back to normal mode\n");
        return;
    }
    
    TPD_DMESG("switch tpd back to normal mode\n");
    
    for(i=0;i<TPD_WAKEUP_TRIAL;i++) {
        i2c_master_send(i2c_client,wakeup,2);
        if(i2c_master_send(i2c_client,wakeup,2)==2) break;
        msleep(TPD_WAKEUP_DELAY);
    }
    i2c_master_send(i2c_client,gesture,2);
    i2c_master_send(i2c_client,threshold,2);
    i2c_master_send(i2c_client,idletime,2);
    i2c_master_send(i2c_client,sleeptime,2);
    
    //MT6516_IRQUnmask(MT6516_TOUCH_IRQ_LINE);    
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
    
    tpd_status = 1; 
}

static struct tpd_driver_t tpd_device_driver = {
		.tpd_device_name = "eeti_pcap7200",
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
    printk("MediaTek eeti_pcap7200 touch panel driver init\n");
		if(tpd_driver_add(&tpd_device_driver) < 0)
			TPD_DMESG("add generic driver failed\n");
    return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void) {
    TPD_DMESG("MediaTek eeti_pcap7200 touch panel driver exit\n");
    //input_unregister_device(tpd->dev);
    tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

