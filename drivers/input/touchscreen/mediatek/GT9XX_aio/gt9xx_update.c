/* drivers/input/touchscreen/gt9xx_update.c
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * Version: 2.0   
 * Revision Record: 
 *      V1.0:  first release. by Andrew, 2012/08/27.
 *      V1.2:  modify gt9110p pid map, by Andrew, 2012/10/15
 *      V1.4: 
 *          1. modify gup_enter_update_mode,
 *          2. rewrite i2c read/write func
 *          3. check update file checksum
 *                  by Andrew, 2012/12/12
 *      v1.6:
 *          1. delete GTP_FW_DOWNLOAD related things.
 *          2. add GTP_HEADER_FW_UPDATE switch to update fw by gtp_default_fw in *.h directly
 *                  by Meta, 2013/04/18
 *      V2.0:
 *          1. GT9XXF main clock calibration
 *          2. header fw update no fs related
 *          3. update file searching optimization
 *          4. config update as module, switchable
 *                  by Meta, 2013/08/28
 */
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
#include <linux/namei.h>
#include <linux/mount.h>
#include "cust_gpio_usage.h"
#include <asm/uaccess.h>
#define GUP_FW_INFO
#include "tpd_custom_gt9xx.h"

#if ( (GTP_AUTO_UPDATE && GTP_HEADER_FW_UPDATE) || GTP_COMPATIBLE_MODE )
    #include "gt9xx_firmware.h"
#endif


#define GUP_REG_HW_INFO             0x4220
#define GUP_REG_FW_MSG              0x41E4
#define GUP_REG_PID_VID             0x8140

#define GUP_SEARCH_FILE_TIMES       50
#define UPDATE_FILE_PATH_1          "/data/_goodix_update_.bin"
#define UPDATE_FILE_PATH_2          "/sdcard/_goodix_update_.bin"

#define CONFIG_FILE_PATH_1          "/data/_goodix_config_.cfg"
#define CONFIG_FILE_PATH_2          "/sdcard/_goodix_config_.cfg"

#define FW_HEAD_LENGTH               14
#define FW_DOWNLOAD_LENGTH           0x4000
#define FW_SECTION_LENGTH            0x2000
#define FW_DSP_ISP_LENGTH            0x1000
#define FW_DSP_LENGTH                0x1000
#define FW_BOOT_LENGTH               0x800
#define FW_SS51_LENGTH               (4 * FW_SECTION_LENGTH)
#define FW_BOOT_ISP_LENGTH           0x800                     // 2k
#define FW_LINK_LENGTH               0x3000                    // 12k
#define FW_APP_CODE_LENGTH           (4 * FW_SECTION_LENGTH)   // 32k

#define FIRMWARE_LENGTH             (FW_SS51_LENGTH + FW_DSP_LENGTH + FW_BOOT_LENGTH + FW_DSP_ISP_LENGTH + FW_BOOT_ISP_LENGTH + FW_APP_CODE_LENGTH)

#define FW_HOTKNOT_LENGTH            0x3000
#define PACK_SIZE                    256
#define MAX_FRAME_CHECK_TIME         5


#define _bRW_MISCTL__SRAM_BANK       0x4048
#define _bRW_MISCTL__MEM_CD_EN       0x4049
#define _bRW_MISCTL__CACHE_EN        0x404B
#define _bRW_MISCTL__TMR0_EN         0x40B0
#define _rRW_MISCTL__SWRST_B0_       0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE 0x4184
#define _rRW_MISCTL__BOOTCTL_B0_     0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_    0x4218
#define _rRW_MISCTL__BOOT_CTL_       0x5094

#define AUTO_SEARCH_BIN           0x01
#define AUTO_SEARCH_CFG           0x02
#define BIN_FILE_READY            0x80
#define CFG_FILE_READY            0x08
#define HEADER_FW_READY           0x01

#define FAIL    0
#define SUCCESS 1

#define MAIN_SYSTEM_PATH "/sdcard/goodix/_main_.bin"
#define HOTKNOT_SYSTEM_PATH "/sdcard/goodix/_hotknot_.bin"
#define FX_SYSTEM_PATH "/sdcard/goodix/_authorization_.bin"

#define	GTP_FORCE_UPDATE_FW 0
#define GT917S_UPDATE_RULE 1

#pragma pack(1)
typedef struct
{
    u8  hw_info[4];          //hardware info//
    u8  pid[8];              //product id   //
    u16 vid;                 //version id   //
} st_fw_head;
#pragma pack()

typedef struct
{
    u8 force_update;
    u8 fw_flag;
    struct file *file;
    struct file *cfg_file;
    st_fw_head  ic_fw_msg;
    mm_segment_t old_fs;
    u32 fw_total_len;
    u32 fw_burned_len;
} st_update_msg;

st_update_msg update_msg;
extern struct i2c_client *i2c_client_point;
extern unsigned int touch_irq;
u16 show_len;
u16 total_len;
extern u8 fw_updating;
extern u8 cfg_len;
extern struct mutex i2c_access;
u8 searching_file = 0;
u8 got_file_flag = 0;
u8 load_fw_process = 0;

#if GTP_ESD_PROTECT
extern void gtp_esd_switch(struct i2c_client *client, s32 on);
#endif

#if (GTP_ESD_PROTECT || GTP_COMPATIBLE_MODE)
extern u8 is_reseting;
#endif


#if GTP_COMPATIBLE_MODE
extern CHIP_TYPE_T gtp_chip_type;
extern u8 rqst_processing;
extern u8 gtp_fw_startup(struct i2c_client *client);
static u8 gup_check_and_repair(struct i2c_client *, s32 , u8 *, u32 );
s32 gup_recovery_main_system(void);
s32 gup_load_main_system(char *filepath);
s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
char * gup_load_fw_from_file(char *filepath);
s32 gup_load_system(char *firmware, s32 length, u8 need_check);
#endif

#define _CLOSE_FILE(p_file) if (p_file && !IS_ERR(p_file)) \
                            { \
                                filp_close(p_file, NULL); \
                            }


static u8 gup_burn_fw_app_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u32 len, u8 bank_cmd );

static s32 gup_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
    s32 ret = -1;
    u16 addr = (buf[0] << 8) + buf[1];

    ret = i2c_read_bytes(client, addr, &buf[2], len - 2);

    if (!ret)
    {
        return 2;
    }
    else
    {
        return ret;
    }
}

static s32 gup_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
    s32 ret = -1;
    u16 addr = (buf[0] << 8) + buf[1];

    ret = i2c_write_bytes(client, addr, &buf[2], len - 2);

    if (!ret)
    {
        return 1;
    }
    else
    {
        return ret;
    }
}

static u8 gup_get_ic_msg(struct i2c_client *client, u16 addr, u8 *msg, s32 len)
{
    s32 i = 0;

    msg[0] = (addr >> 8) & 0xff;
    msg[1] = addr & 0xff;

    for (i = 0; i < 5; i++)
    {
        if (gup_i2c_read(client, msg, GTP_ADDR_LENGTH + len) > 0)
        {
            break;
        }
    }

    if (i >= 5)
    {
        GTP_ERROR("Read data from 0x%02x%02x failed!", msg[0], msg[1]);
        return FAIL;
    }

    return SUCCESS;
}

static u8 gup_set_ic_msg(struct i2c_client *client, u16 addr, u8 val)
{
    s32 i = 0;
    u8 msg[3];

    msg[0] = (addr >> 8) & 0xff;
    msg[1] = addr & 0xff;
    msg[2] = val; 
 
    for (i = 0; i < 5; i++)
    {
        if (gup_i2c_write(client, msg, GTP_ADDR_LENGTH + 1) > 0)
        {
            break;
        }
    }

    if (i >= 5)
    {
        GTP_ERROR("Set data to 0x%02x%02x failed!", msg[0], msg[1]);
        return FAIL;
    }

    return SUCCESS;
}

static u8 gup_get_ic_fw_msg(struct i2c_client *client)
{
    s32 ret = -1;
    u8  retry = 0;
    u8  buf[16];
    u8  i;

    //step1:get hardware info
    ret = gtp_i2c_read_dbl_check(client, GUP_REG_HW_INFO, &buf[GTP_ADDR_LENGTH], 4);
    if (FAIL == ret)
    {
        GTP_ERROR("[get_ic_fw_msg]get hw_info failed,exit");
        return FAIL;
    }

    // buf[2~5]: 00 06 90 00
    // hw_info: 00 90 06 00
    for (i = 0; i < 4; i++)
    {
        update_msg.ic_fw_msg.hw_info[i] = buf[GTP_ADDR_LENGTH + 3 - i];
    }

    GTP_INFO("IC Hardware info:%02x%02x%02x%02x", update_msg.ic_fw_msg.hw_info[0], update_msg.ic_fw_msg.hw_info[1],
             update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);

    //step2:get firmware message
    for (retry = 0; retry < 2; retry++)
    {
        ret = gup_get_ic_msg(client, GUP_REG_FW_MSG, buf, 1);

        if (FAIL == ret)
        {
            GTP_ERROR("Read firmware message fail.");
            return ret;
        }

        update_msg.force_update = buf[GTP_ADDR_LENGTH];

        if ((0xBE != update_msg.force_update) && (!retry))
        {
            GTP_INFO("The check sum in ic is error.");
            GTP_INFO("The IC will be updated by force.");
            continue;
        }
        break;
    }

    GTP_INFO("IC force update flag:0x%x", update_msg.force_update);

    //step3:get pid & vid
    ret = gtp_i2c_read_dbl_check(client, GUP_REG_PID_VID, &buf[GTP_ADDR_LENGTH], 6);
    if (FAIL == ret)
    {
        GTP_ERROR("[get_ic_fw_msg]get pid & vid failed,exit");
        return FAIL;
    }

    memset(update_msg.ic_fw_msg.pid, 0, sizeof(update_msg.ic_fw_msg.pid));
    memcpy(update_msg.ic_fw_msg.pid, &buf[GTP_ADDR_LENGTH], 4);


    //GT9XX PID MAPPING
    /*|-----FLASH-----RAM-----|
      |------918------918-----|
      |------968------968-----|
      |------913------913-----|
      |------913P-----913P----|
      |------927------927-----|
      |------927P-----927P----|
      |------9110-----9110----|
      |------9110P----9111----|*/
    if(update_msg.ic_fw_msg.pid[0] != 0)
    {
        if (!memcmp(update_msg.ic_fw_msg.pid, "9111", 4))
        {
            GTP_INFO("IC Mapping Product id:%s", update_msg.ic_fw_msg.pid);
            memcpy(update_msg.ic_fw_msg.pid, "9110P", 5);
        }
    }

    update_msg.ic_fw_msg.vid = buf[GTP_ADDR_LENGTH + 4] + (buf[GTP_ADDR_LENGTH + 5] << 8);
    return SUCCESS;
}

s32 gup_enter_update_mode(struct i2c_client *client)
{
    s32 ret = -1;
    s32 retry = 0;
    u8 rd_buf[3];
    
    //step1:RST output low last at least 2ms
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
    msleep(2);
    
    //step2:select I2C slave addr,INT:0--0xBA;1--0x28.
    GTP_GPIO_OUTPUT(GTP_INT_PORT, (client->addr == 0x14));
    msleep(2);
    
    //step3:RST output high reset guitar
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);
    
    //20121211 modify start
    msleep(5);
    while(retry++ < 200)
    {
        //step4:Hold ss51 & dsp
        ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
        if(ret <= 0)
        {
            GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
            continue;
        }
        
        //step5:Confirm hold
        ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
        if(ret <= 0)
        {
            GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
            continue;
        }
        if(0x0C == rd_buf[GTP_ADDR_LENGTH])
        {
            GTP_DEBUG("Hold ss51 & dsp confirm SUCCESS");
            break;
        }
        GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d", rd_buf[GTP_ADDR_LENGTH]);
    }
    if(retry >= 200)
    {
        GTP_ERROR("Enter update Hold ss51 failed.");
        return FAIL;
    }
    
    //step6:DSP_CK and DSP_ALU_CK PowerOn
    ret = gup_set_ic_msg(client, 0x4010, 0x00);
    
    //20121211 modify end
    return ret;
}

void gup_leave_update_mode(void)
{
    GTP_GPIO_AS_INT(GTP_INT_PORT);
    
    GTP_DEBUG("[leave_update_mode]reset chip.");
#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        force_reset_guitar();
        GTP_INFO("User layer reset GT9XXF.");
        return;
    }
#endif
    gtp_reset_guitar(i2c_client_point, 20);
}

static u8 gup_enter_update_judge(st_fw_head *fw_head)
{
    u16 u16_tmp;
    s32 i = 0;
    u32 fw_len = 0;
    s32 pid_cmp_len = 0;

    //Get the correct nvram data
    //The correct conditions:
    //1. the hardware info is the same
    //2. the product id is the same
    //3. the firmware version in update file is greater than the firmware version in ic
    //or the check sum in ic is wrong

    u16_tmp = fw_head->vid;
    fw_head->vid = (u16)(u16_tmp >> 8) + (u16)(u16_tmp << 8);

    GTP_INFO("FILE HARDWARE INFO:%02x%02x%02x%02x", fw_head->hw_info[0], fw_head->hw_info[1], fw_head->hw_info[2], fw_head->hw_info[3]);
    GTP_INFO("FILE PID:%s", fw_head->pid);
    GTP_INFO("FILE VID:%04x", fw_head->vid);
    GTP_INFO("IC HARDWARE INFO:%02x%02x%02x%02x", update_msg.ic_fw_msg.hw_info[0], update_msg.ic_fw_msg.hw_info[1],
             update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);
    GTP_INFO("IC PID:%s", update_msg.ic_fw_msg.pid);
    GTP_INFO("IC VID:%04x", update_msg.ic_fw_msg.vid);
    
    if (!memcmp(fw_head->hw_info, update_msg.ic_fw_msg.hw_info, sizeof(update_msg.ic_fw_msg.hw_info)))
    {
        fw_len = 42 * 1024;
    }
    else
    {
        fw_len = fw_head->hw_info[3];
        fw_len += (((u32)fw_head->hw_info[2]) << 8);
        fw_len += (((u32)fw_head->hw_info[1]) << 16);
        fw_len += (((u32)fw_head->hw_info[0]) << 24);
    }
    if (update_msg.fw_total_len != fw_len)
    {
        GTP_ERROR("Inconsistent firmware size, Update aborted! Default size: %d(%dK), actual size: %d(%dK)", fw_len, fw_len/1024, update_msg.fw_total_len, update_msg.fw_total_len/1024);
        return FAIL;
    }
    if ((update_msg.fw_total_len < 36*1024) || (update_msg.fw_total_len > 128*1024))
    {
        GTP_ERROR("Invalid firmware length(%d), update aborted!", update_msg.fw_total_len);
        return FAIL;
    }
    GTP_INFO("Firmware length:%d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);
    
    if (update_msg.force_update != 0xBE)
    {
        GTP_INFO("FW chksum error,need enter update.");
        return SUCCESS;
    }

    // 20130523 start
    if (strlen(update_msg.ic_fw_msg.pid) < 3)
    {
        GTP_INFO("Illegal IC pid, need enter update");
        return SUCCESS;
    }
    else
    {
        for (i = 0; i < 3; i++)
        {
            if ((update_msg.ic_fw_msg.pid[i] < 0x30) || (update_msg.ic_fw_msg.pid[i] > 0x39))
            {
                GTP_INFO("Illegal IC pid, out of bound, need enter update");
                return SUCCESS;
            }
        }
    }
    // 20130523 end
    
    pid_cmp_len = strlen(fw_head->pid);
    if (pid_cmp_len < strlen(update_msg.ic_fw_msg.pid))
    {
        pid_cmp_len = strlen(update_msg.ic_fw_msg.pid);
    }
    
    if ((!memcmp(fw_head->pid, update_msg.ic_fw_msg.pid, pid_cmp_len)) ||
            (!memcmp(update_msg.ic_fw_msg.pid, "91XX", 4))||
            (!memcmp(fw_head->pid, "91XX", 4)))
    {
        if(!memcmp(fw_head->pid, "91XX", 4))
        {
            GTP_DEBUG("Force none same pid update mode.");
        }
        else
        {
            GTP_DEBUG("Get the same pid.");
        }

        //The third condition
        if (fw_head->vid > update_msg.ic_fw_msg.vid)
        {

            GTP_INFO("Need enter update.");
            return SUCCESS;
        }

        GTP_INFO("Don't meet the third condition.");
        GTP_ERROR("File VID <= IC VID, update aborted!");
    }
    else
    {
        GTP_ERROR("File PID != IC PID, update aborted!");
    }

    return FAIL;
}

#if GTP_AUTO_UPDATE_CFG
static u8 ascii2hex(u8 a)
{
    s8 value = 0;

    if(a >= '0' && a <= '9')
    {
        value = a - '0';
    }
    else if(a >= 'A' && a <= 'F')
    {
        value = a - 'A' + 0x0A;
    }
    else if(a >= 'a' && a <= 'f')
    {
        value = a - 'a' + 0x0A;
    }
    else
    {
        value = 0xff;
    }
    
    return value;
}
static s8 gup_update_config(struct i2c_client *client)
{
    s32 file_len = 0;
    s32 ret = 0;
    s32 i = 0;
    s32 file_cfg_len = 0;
    s32 chip_cfg_len = 0;
    s32 count = 0;
    u8 *buf;
    u8 *pre_buf;
    u8 *file_config;
    //u8 checksum = 0;
    
    if(NULL == update_msg.cfg_file)
    {
        GTP_ERROR("[update_cfg]No need to upgrade config!");
        return FAIL;
    }
    file_len = update_msg.cfg_file->f_op->llseek(update_msg.cfg_file, 0, SEEK_END);
    
    chip_cfg_len = cfg_len;
    
    GTP_DEBUG("[update_cfg]config file len:%d", file_len);
    GTP_DEBUG("[update_cfg]need config len:%d",chip_cfg_len);
    if((file_len+5) < chip_cfg_len*5)
    {
        GTP_ERROR("Config length error");
        return -1;
    }
    
    buf = (u8*)kzalloc(file_len, GFP_KERNEL);
    pre_buf = (u8*)kzalloc(file_len, GFP_KERNEL);
    file_config = (u8*)kzalloc(chip_cfg_len + GTP_ADDR_LENGTH, GFP_KERNEL);
    update_msg.cfg_file->f_op->llseek(update_msg.cfg_file, 0, SEEK_SET);
    
    GTP_DEBUG("[update_cfg]Read config from file.");
    ret = update_msg.cfg_file->f_op->read(update_msg.cfg_file, (char*)pre_buf, file_len, &update_msg.cfg_file->f_pos);
    if(ret<0)
    {
        GTP_ERROR("[update_cfg]Read config file failed.");
        goto update_cfg_file_failed;
    }
    
    GTP_DEBUG("[update_cfg]Delete illgal charactor.");
    for(i=0,count=0; i<file_len; i++)
    {
        if (pre_buf[i] == ' ' || pre_buf[i] == '\r' || pre_buf[i] == '\n')
        {
            continue;
        }
        buf[count++] = pre_buf[i];
    }
    
    GTP_DEBUG("[update_cfg]Ascii to hex.");
    file_config[0] = GTP_REG_CONFIG_DATA >> 8;
    file_config[1] = GTP_REG_CONFIG_DATA & 0xff;
    for(i=0,file_cfg_len=GTP_ADDR_LENGTH; i<count; i+=5)
    {
        if((buf[i]=='0') && ((buf[i+1]=='x') || (buf[i+1]=='X')))
        {
            u8 high,low;
            high = ascii2hex(buf[i+2]);
            low = ascii2hex(buf[i+3]);
            
            if((high == 0xFF) || (low == 0xFF))
            {
                ret = 0;
                GTP_ERROR("[update_cfg]Illegal config file.");
                goto update_cfg_file_failed;
            }
            file_config[file_cfg_len++] = (high<<4) + low;
        }
        else
        {
            ret = 0;
            GTP_ERROR("[update_cfg]Illegal config file.");
            goto update_cfg_file_failed;
        }
    }
    
    
    GTP_DEBUG("config:");
    GTP_DEBUG_ARRAY(file_config+2, file_cfg_len);
    
    i = 0;
    while(i++ < 5)
    {
        ret = gup_i2c_write(client, file_config, file_cfg_len);
        if(ret > 0)
        {
            GTP_INFO("[update_cfg]Send config SUCCESS.");
            break;
        }
        GTP_ERROR("[update_cfg]Send config i2c error.");
    }
    
update_cfg_file_failed:
    kfree(pre_buf);
    kfree(buf);
    kfree(file_config);
    return ret;
}
#endif

#if (GTP_AUTO_UPDATE && (!GTP_HEADER_FW_UPDATE || GTP_AUTO_UPDATE_CFG))
static void gup_search_file(s32 search_type)
{
    s32 i = 0;
    struct file *pfile = NULL;

    got_file_flag = 0x00;
    
    searching_file = 1;
    for (i = 0; i < GUP_SEARCH_FILE_TIMES; ++i)
    {            
        if (0 == searching_file)
        {
            GTP_INFO("Force exiting file searching");
            got_file_flag = 0x00;
            return;
        }
        
        if (search_type & AUTO_SEARCH_BIN)
        {
            GTP_DEBUG("Search for %s, %s for fw update.(%d/%d)", UPDATE_FILE_PATH_1, UPDATE_FILE_PATH_2, i+1, GUP_SEARCH_FILE_TIMES);
            pfile = filp_open(UPDATE_FILE_PATH_1, O_RDONLY, 0);
            if (IS_ERR(pfile))
            {
                pfile = filp_open(UPDATE_FILE_PATH_2, O_RDONLY, 0);
                if (!IS_ERR(pfile))
                {
                    GTP_INFO("Bin file: %s for fw update.", UPDATE_FILE_PATH_2);
                    got_file_flag |= BIN_FILE_READY;
                    update_msg.file = pfile;
                }
            }
            else
            {
                GTP_INFO("Bin file: %s for fw update.", UPDATE_FILE_PATH_1);
                got_file_flag |= BIN_FILE_READY;
                update_msg.file = pfile;
            }
            if (got_file_flag & BIN_FILE_READY)
            {
            #if GTP_AUTO_UPDATE_CFG
                if (search_type & AUTO_SEARCH_CFG)
                {
                    i = GUP_SEARCH_FILE_TIMES;    // Bin & Cfg File required to be in the same directory
                }
                else
            #endif
                {
                    searching_file = 0;
                    return;
                }
            }
        }
    
    #if GTP_AUTO_UPDATE_CFG
        if ( (search_type & AUTO_SEARCH_CFG) && !(got_file_flag & CFG_FILE_READY) )
        {
            GTP_DEBUG("Search for %s, %s for config update.(%d/%d)", CONFIG_FILE_PATH_1, CONFIG_FILE_PATH_2, i+1, GUP_SEARCH_FILE_TIMES);
            pfile = filp_open(CONFIG_FILE_PATH_1, O_RDONLY, 0);
            if (IS_ERR(pfile))
            {
                pfile = filp_open(CONFIG_FILE_PATH_2, O_RDONLY, 0);
                if (!IS_ERR(pfile))
                {
                    GTP_INFO("Cfg file: %s for config update.", CONFIG_FILE_PATH_2);
                    got_file_flag |= CFG_FILE_READY;
                    update_msg.cfg_file = pfile;
                }
            }
            else
            {
                GTP_INFO("Cfg file: %s for config update.", CONFIG_FILE_PATH_1);
                got_file_flag |= CFG_FILE_READY;
                update_msg.cfg_file = pfile;
            }
            if (got_file_flag & CFG_FILE_READY)
            {
                searching_file = 0;
                return;
            }
        }
    #endif
        msleep(3000);
    }
    searching_file = 0;
}
#endif

static u8 gup_check_update_file(struct i2c_client *client, st_fw_head *fw_head, u8 *path)
{
    s32 ret = 0;
    s32 i = 0;
    s32 fw_checksum = 0;
    u8 buf[FW_HEAD_LENGTH];

    got_file_flag = 0x00;
    if (path)
    {
        //GTP_DEBUG("Update File path:%s, %d", path, strlen(path));
        update_msg.file = filp_open(path, O_RDONLY, 0);

        if (IS_ERR(update_msg.file))
        {
            GTP_ERROR("Open update file(%s) error!", path);
            return FAIL;
        }
        got_file_flag = BIN_FILE_READY;
    }
    else
    {
#if GTP_AUTO_UPDATE
    #if GTP_HEADER_FW_UPDATE
        GTP_INFO("Update by default firmware array");
        update_msg.fw_total_len = sizeof(gtp_default_FW) - FW_HEAD_LENGTH;
        if (sizeof(gtp_default_FW) < (FW_HEAD_LENGTH + FW_SECTION_LENGTH*4+FW_DSP_ISP_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH))
        {
            GTP_INFO("[check_update_file]default firmware array is INVALID!");
            return FAIL;
        }
        GTP_DEBUG("Firmware actual size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);
        memcpy(fw_head, &gtp_default_FW[0], FW_HEAD_LENGTH);
        
        //check firmware legality
        fw_checksum = 0;
        for (i = 0; i < update_msg.fw_total_len; i += 2)
        {
            fw_checksum += (gtp_default_FW[FW_HEAD_LENGTH + i] << 8) + gtp_default_FW[FW_HEAD_LENGTH + i + 1];
        }
        if(fw_checksum&0xFFFF)
        {
            GTP_ERROR("Illegal firmware with checksum(0x%04X)", fw_checksum & 0xFFFF);
            return FAIL;
        }
        got_file_flag = HEADER_FW_READY;
        return SUCCESS;
        
    #else
        #if GTP_AUTO_UPDATE_CFG
            gup_search_file(AUTO_SEARCH_BIN | AUTO_SEARCH_CFG);
            if (got_file_flag & CFG_FILE_READY)
            {
                ret = gup_update_config(client);
            	if(ret <= 0)
                {
                    GTP_ERROR("Update config failed!");
                }
                _CLOSE_FILE(update_msg.cfg_file);
                msleep(500);                //waiting config to be stored in FLASH.
            }
        #else
            gup_search_file(AUTO_SEARCH_BIN);
        #endif

            if (!(got_file_flag & BIN_FILE_READY))
            {
                GTP_ERROR("No bin file for fw Update");
                return FAIL;
            }
    #endif
#else
        {
            GTP_ERROR("NULL file for fw update!");
            return FAIL;
        }
#endif
    }
    
    update_msg.old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
    update_msg.fw_total_len = update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_END);
    
    update_msg.fw_total_len -= FW_HEAD_LENGTH;
    
    GTP_DEBUG("Bin firmware actual size: %d(%dK)", update_msg.fw_total_len, update_msg.fw_total_len/1024);
    
    update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
    ret = update_msg.file->f_op->read(update_msg.file, (char *)buf, FW_HEAD_LENGTH, &update_msg.file->f_pos);

    if (ret < 0)
    {
        GTP_ERROR("Read firmware head in update file error.");
        goto load_failed;
    }

    memcpy(fw_head, buf, FW_HEAD_LENGTH);
    
    //check firmware legality
    fw_checksum = 0;
    for(i=0; i < update_msg.fw_total_len; i+=2)
    {
        u16 temp;
        ret = update_msg.file->f_op->read(update_msg.file, (char*)buf, 2, &update_msg.file->f_pos);
        if (ret < 0)
        {
            GTP_ERROR("Read firmware file error.");
            goto load_failed;
        }
        //GTP_DEBUG("BUF[0]:%x", buf[0]);
        temp = (buf[0]<<8) + buf[1];
        fw_checksum += temp;
    }
    
    GTP_DEBUG("firmware checksum:%x", fw_checksum&0xFFFF);
    if(fw_checksum&0xFFFF)
    {
        GTP_ERROR("Illegal firmware file.");
        goto load_failed;
    }
    
    return SUCCESS;

load_failed:
    set_fs(update_msg.old_fs);
    _CLOSE_FILE(update_msg.file);
    return FAIL;
}

static u8 gup_burn_proc(struct i2c_client *client, u8 *burn_buf, u16 start_addr, u16 total_length)
{
    s32 ret = 0;
    u16 burn_addr = start_addr;
    u16 frame_length = 0;
    u16 burn_length = 0;
    u8  wr_buf[PACK_SIZE + GTP_ADDR_LENGTH];
    u8  rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
    u8  retry = 0;

    GTP_DEBUG("Begin burn %dk data to addr 0x%x", (total_length / 1024), start_addr);

    while (burn_length < total_length)
    {
        GTP_DEBUG("B/T:%04d/%04d", burn_length, total_length);
        frame_length = ((total_length - burn_length) > PACK_SIZE) ? PACK_SIZE : (total_length - burn_length);
        wr_buf[0] = (u8)(burn_addr >> 8);
        rd_buf[0] = wr_buf[0];
        wr_buf[1] = (u8)burn_addr;
        rd_buf[1] = wr_buf[1];
        memcpy(&wr_buf[GTP_ADDR_LENGTH], &burn_buf[burn_length], frame_length);

        for (retry = 0; retry < MAX_FRAME_CHECK_TIME; retry++)
        {
            ret = gup_i2c_write(client, wr_buf, GTP_ADDR_LENGTH + frame_length);

            if (ret <= 0)
            {
                GTP_ERROR("Write frame data i2c error.");
                continue;
            }

            ret = gup_i2c_read(client, rd_buf, GTP_ADDR_LENGTH + frame_length);

            if (ret <= 0)
            {
                GTP_ERROR("Read back frame data i2c error.");
                continue;
            }

            if (memcmp(&wr_buf[GTP_ADDR_LENGTH], &rd_buf[GTP_ADDR_LENGTH], frame_length))
            {
                GTP_ERROR("Check frame data fail,not equal.");
                GTP_DEBUG("write array:");
                GTP_DEBUG_ARRAY(&wr_buf[GTP_ADDR_LENGTH], frame_length);
                GTP_DEBUG("read array:");
                GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
                continue;
            }
            else
            {
                //GTP_DEBUG("Check frame data success.");
                break;
            }
        }

        if (retry > MAX_FRAME_CHECK_TIME)
        {
            GTP_ERROR("Burn frame data time out,exit.");
            return FAIL;
        }

        burn_length += frame_length;
        burn_addr += frame_length;
    }

    return SUCCESS;
}

static u8 gup_load_section_file(u8 *buf, u32 offset, u16 length, u8 set_or_end)
{
#if (GTP_AUTO_UPDATE && GTP_HEADER_FW_UPDATE)
    if (HEADER_FW_READY == got_file_flag)
    {
        if(SEEK_SET == set_or_end)
        {
            memcpy(buf, &gtp_default_FW[FW_HEAD_LENGTH + offset], length);
        }
        else    //seek end
        {
            memcpy(buf, &gtp_default_FW[update_msg.fw_total_len + FW_HEAD_LENGTH - offset], length);
        }
        return SUCCESS;
    }
#endif
    {
        s32 ret = 0;
    
        if ( (update_msg.file == NULL) || IS_ERR(update_msg.file))
        {
            GTP_ERROR("cannot find update file,load section file fail.");
            return FAIL;
        }
        
        if(SEEK_SET == set_or_end)
        {
            update_msg.file->f_pos = FW_HEAD_LENGTH + offset;
        }
        else    //seek end
        {
            update_msg.file->f_pos = update_msg.fw_total_len + FW_HEAD_LENGTH - offset;
        }
    
        ret = update_msg.file->f_op->read(update_msg.file, (char *)buf, length, &update_msg.file->f_pos);
    
        if (ret < 0)
        {
            GTP_ERROR("Read update file fail.");
            return FAIL;
        }
    
        return SUCCESS;
    }
}

static u8 gup_recall_check(struct i2c_client *client, u8 *chk_src, u16 start_rd_addr, u16 chk_length)
{
    u8  rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
    s32 ret = 0;
    u16 recall_addr = start_rd_addr;
    u16 recall_length = 0;
    u16 frame_length = 0;

    while (recall_length < chk_length)
    {
        frame_length = ((chk_length - recall_length) > PACK_SIZE) ? PACK_SIZE : (chk_length - recall_length);
        ret = gup_get_ic_msg(client, recall_addr, rd_buf, frame_length);

        if (ret <= 0)
        {
            GTP_ERROR("recall i2c error,exit");
            return FAIL;
        }

        if (memcmp(&rd_buf[GTP_ADDR_LENGTH], &chk_src[recall_length], frame_length))
        {
            GTP_ERROR("Recall frame data fail,not equal.");
            GTP_DEBUG("chk_src array:");
            GTP_DEBUG_ARRAY(&chk_src[recall_length], frame_length);
            GTP_DEBUG("recall array:");
            GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
            return FAIL;
        }

        recall_length += frame_length;
        recall_addr += frame_length;
    }

    GTP_DEBUG("Recall check %dk firmware success.", (chk_length / 1024));

    return SUCCESS;
}

static u8 gup_burn_fw_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u8 bank_cmd)
{
    s32 ret = 0;
    u8  rd_buf[5];

    //step1:hold ss51 & dsp
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]hold ss51 & dsp fail.");
        return FAIL;
    }

    //step2:set scramble
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]set scramble fail.");
        return FAIL;
    }

    //step3:select bank
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4) & 0x0F);
        return FAIL;
    }

    //step4:enable accessing code
    ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]enable accessing code fail.");
        return FAIL;
    }

    //step5:burn 8k fw section
    ret = gup_burn_proc(client, fw_section, start_addr, FW_SECTION_LENGTH);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_section]burn fw_section fail.");
        return FAIL;
    }

    //step6:hold ss51 & release dsp
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]hold ss51 & release dsp fail.");
        return FAIL;
    }

    //must delay
    msleep(1);

    //step7:send burn cmd to move data to flash from sram
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd & 0x0f);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]send burn cmd fail.");
        return FAIL;
    }

    GTP_DEBUG("[burn_fw_section]Wait for the burn is complete......");

    do
    {
        ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);

        if (ret <= 0)
        {
            GTP_ERROR("[burn_fw_section]Get burn state fail");
            return FAIL;
        }

        msleep(10);
        //GTP_DEBUG("[burn_fw_section]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]);
    }
    while (rd_buf[GTP_ADDR_LENGTH]);

    //step8:select bank
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4) & 0x0F);
        return FAIL;
    }

    //step9:enable accessing code
    ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]enable accessing code fail.");
        return FAIL;
    }

    //step10:recall 8k fw section
    ret = gup_recall_check(client, fw_section, start_addr, FW_SECTION_LENGTH);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_section]recall check %dk firmware fail.", FW_SECTION_LENGTH/1024);
        return FAIL;
    }

    //step11:disable accessing code
    ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]disable accessing code fail.");
        return FAIL;
    }

    return SUCCESS;
}

static u8 gup_burn_dsp_isp(struct i2c_client *client)
{
    s32 ret = 0;
    u8 *fw_dsp_isp = NULL;
    u8  retry = 0;
    //u32 offset;

    GTP_INFO("[burn_dsp_isp]Begin burn dsp isp---->>");

    //step1:alloc memory
    GTP_DEBUG("[burn_dsp_isp]step1:alloc memory");

    while (retry++ < 5)
    {
        fw_dsp_isp = (u8 *)kzalloc(FW_DSP_ISP_LENGTH, GFP_KERNEL);

        if (fw_dsp_isp == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_dsp_isp]Alloc %dk byte memory success.", (FW_DSP_ISP_LENGTH / 1024));
            break;
        }
    }

    if (retry >= 5)
    {
        GTP_ERROR("[burn_dsp_isp]Alloc memory fail,exit.");
        return FAIL;
    }

    //step2:load dsp isp file data
    GTP_DEBUG("[burn_dsp_isp]step2:load dsp isp file data");
    ret = gup_load_section_file(fw_dsp_isp, FW_DSP_ISP_LENGTH, FW_DSP_ISP_LENGTH, SEEK_END);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_dsp_isp]load firmware dsp_isp fail.");
        goto exit_burn_dsp_isp;
    }

    //step3:disable wdt,clear cache enable
    GTP_DEBUG("[burn_dsp_isp]step3:disable wdt,clear cache enable");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]disable wdt fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]clear cache enable fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step4:hold ss51 & dsp
    GTP_DEBUG("[burn_dsp_isp]step4:hold ss51 & dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]hold ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step5:set boot from sram
    GTP_DEBUG("[burn_dsp_isp]step5:set boot from sram");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]set boot from sram fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step6:software reboot
    GTP_DEBUG("[burn_dsp_isp]step6:software reboot");
    ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]software reboot fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step7:select bank2
    GTP_DEBUG("[burn_dsp_isp]step7:select bank2");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]select bank2 fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step8:enable accessing code
    GTP_DEBUG("[burn_dsp_isp]step8:enable accessing code");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]enable accessing code fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }

    //step9:burn 4k dsp_isp
    GTP_DEBUG("[burn_dsp_isp]step9:burn 4k dsp_isp");
    ret = gup_burn_proc(client, fw_dsp_isp, 0xC000, FW_DSP_ISP_LENGTH);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_dsp_isp]burn dsp_isp fail.");
        goto exit_burn_dsp_isp;
    }

    //step10:set scramble
    GTP_DEBUG("[burn_dsp_isp]step10:set scramble");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_dsp_isp]set scramble fail.");
        ret = FAIL;
        goto exit_burn_dsp_isp;
    }
    
    update_msg.fw_burned_len += FW_DSP_ISP_LENGTH;
    GTP_DEBUG("[burn_dsp_isp]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;

exit_burn_dsp_isp:
    kfree(fw_dsp_isp);
    return ret;
}

static u8 gup_burn_fw_ss51(struct i2c_client *client)
{
    u8 *fw_ss51 = NULL;
    u8  retry = 0;
    s32 ret = 0;

    GTP_INFO("[burn_fw_ss51]Begin burn ss51 firmware---->>");

    //step1:alloc memory
    GTP_DEBUG("[burn_fw_ss51]step1:alloc memory");

    while (retry++ < 5)
    {
        fw_ss51 = (u8 *)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);

        if (fw_ss51 == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_ss51]Alloc %dk byte memory success.", (FW_SECTION_LENGTH / 1024));
            break;
        }
    }

    if (retry >= 5)
    {
        GTP_ERROR("[burn_fw_ss51]Alloc memory fail,exit.");
        return FAIL;
    }

    //step2:load ss51 firmware section 1 file data
    //GTP_DEBUG("[burn_fw_ss51]step2:load ss51 firmware section 1 file data");
    //ret = gup_load_section_file(fw_ss51, 0, FW_SECTION_LENGTH, SEEK_SET);
//	
//    if (FAIL == ret)
//    {
//        GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 1 fail.");
//        goto exit_burn_fw_ss51;
//    }
    GTP_INFO("[burn_fw_ss51]Reset first 8K of ss51 to 0xFF.");
    GTP_DEBUG("[burn_fw_ss51]step2: reset bank0 0xC000~0xD000");
    memset(fw_ss51, 0xFF, FW_SECTION_LENGTH);

    //step3:clear control flag
    GTP_DEBUG("[burn_fw_ss51]step3:clear control flag");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_ss51]clear control flag fail.");
        ret = FAIL;
        goto exit_burn_fw_ss51;
    }

    //step4:burn ss51 firmware section 1
    GTP_DEBUG("[burn_fw_ss51]step4:burn ss51 firmware section 1");
    ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 1 fail.");
        goto exit_burn_fw_ss51;
    }

    //step5:load ss51 firmware section 2 file data
    GTP_DEBUG("[burn_fw_ss51]step5:load ss51 firmware section 2 file data");
    ret = gup_load_section_file(fw_ss51, FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 2 fail.");
        goto exit_burn_fw_ss51;
    }

    //step6:burn ss51 firmware section 2
    GTP_DEBUG("[burn_fw_ss51]step6:burn ss51 firmware section 2");
    ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x02);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 2 fail.");
        goto exit_burn_fw_ss51;
    }

    //step7:load ss51 firmware section 3 file data
    GTP_DEBUG("[burn_fw_ss51]step7:load ss51 firmware section 3 file data");
    ret = gup_load_section_file(fw_ss51, 2 * FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 3 fail.");
        goto exit_burn_fw_ss51;
    }

    //step8:burn ss51 firmware section 3
    GTP_DEBUG("[burn_fw_ss51]step8:burn ss51 firmware section 3");
    ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x13);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 3 fail.");
        goto exit_burn_fw_ss51;
    }

    //step9:load ss51 firmware section 4 file data
    GTP_DEBUG("[burn_fw_ss51]step9:load ss51 firmware section 4 file data");
    ret = gup_load_section_file(fw_ss51, 3 * FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 4 fail.");
        goto exit_burn_fw_ss51;
    }

    //step10:burn ss51 firmware section 4
    GTP_DEBUG("[burn_fw_ss51]step10:burn ss51 firmware section 4");
    ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x14);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 4 fail.");
        goto exit_burn_fw_ss51;
    }
    
    update_msg.fw_burned_len += (FW_SECTION_LENGTH*4);
    GTP_DEBUG("[burn_fw_ss51]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;

exit_burn_fw_ss51:
    kfree(fw_ss51);
    return ret;
}

static u8 gup_burn_fw_dsp(struct i2c_client *client)
{
    s32 ret = 0;
    u8 *fw_dsp = NULL;
    u8  retry = 0;
    u8  rd_buf[5];

    GTP_INFO("[burn_fw_dsp]Begin burn dsp firmware---->>");
    //step1:alloc memory
    GTP_DEBUG("[burn_fw_dsp]step1:alloc memory");

    while (retry++ < 5)
    {
        fw_dsp = (u8 *)kzalloc(FW_DSP_LENGTH, GFP_KERNEL);

        if (fw_dsp == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_dsp]Alloc %dk byte memory success.", (FW_SECTION_LENGTH / 1024));
            break;
        }
    }

    if (retry >= 5)
    {
        GTP_ERROR("[burn_fw_dsp]Alloc memory fail,exit.");
        return FAIL;
    }

    //step2:load firmware dsp
    GTP_DEBUG("[burn_fw_dsp]step2:load firmware dsp");
    ret = gup_load_section_file(fw_dsp, 4 * FW_SECTION_LENGTH, FW_DSP_LENGTH, SEEK_SET);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_dsp]load firmware dsp fail.");
        goto exit_burn_fw_dsp;
    }

    //step3:select bank3
    GTP_DEBUG("[burn_fw_dsp]step3:select bank3");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_dsp]select bank3 fail.");
        ret = FAIL;
        goto exit_burn_fw_dsp;
    }

    //step4:hold ss51 & dsp
    GTP_DEBUG("[burn_fw_dsp]step4:hold ss51 & dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_dsp]hold ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_dsp;
    }

    //step5:set scramble
    GTP_DEBUG("[burn_fw_dsp]step5:set scramble");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_dsp]set scramble fail.");
        ret = FAIL;
        goto exit_burn_fw_dsp;
    }

    //step6:release ss51 & dsp
    GTP_DEBUG("[burn_fw_dsp]step6:release ss51 & dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);             //20121212

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_dsp]release ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_dsp;
    }

    //must delay
    msleep(1);

    //step7:burn 4k dsp firmware
    GTP_DEBUG("[burn_fw_dsp]step7:burn 4k dsp firmware");
    ret = gup_burn_proc(client, fw_dsp, 0x9000, FW_DSP_LENGTH);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_dsp]burn fw_section fail.");
        goto exit_burn_fw_dsp;
    }

    //step8:send burn cmd to move data to flash from sram
    GTP_DEBUG("[burn_fw_dsp]step8:send burn cmd to move data to flash from sram");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x05);

    if (ret <= 0)
    {
        GTP_ERROR("[burn_fw_dsp]send burn cmd fail.");
        goto exit_burn_fw_dsp;
    }

    GTP_DEBUG("[burn_fw_dsp]Wait for the burn is complete......");

    do
    {
        ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);

        if (ret <= 0)
        {
            GTP_ERROR("[burn_fw_dsp]Get burn state fail");
            goto exit_burn_fw_dsp;
        }

        msleep(10);
        //GTP_DEBUG("[burn_fw_dsp]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]);
    }
    while (rd_buf[GTP_ADDR_LENGTH]);

    //step9:recall check 4k dsp firmware
    GTP_DEBUG("[burn_fw_dsp]step9:recall check 4k dsp firmware");
    ret = gup_recall_check(client, fw_dsp, 0x9000, FW_DSP_LENGTH);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_dsp]recall check 4k dsp firmware fail.");
        goto exit_burn_fw_dsp;
    }
    
    update_msg.fw_burned_len += FW_DSP_LENGTH;
    GTP_DEBUG("[burn_fw_dsp]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;

exit_burn_fw_dsp:
    kfree(fw_dsp);
    return ret;
}

static u8 gup_burn_fw_boot(struct i2c_client *client)
{
    s32 ret = 0;
    u8* fw_boot = NULL;
    u8  retry = 0;
    u8  rd_buf[5];
    
    GTP_DEBUG("[burn_fw_boot]Begin burn bootloader firmware---->>");
    
    //step1:Alloc memory
    GTP_DEBUG("[burn_fw_boot]step1:Alloc memory");

    while(retry++ < 5)
    {
        fw_boot = (u8*)kzalloc(FW_BOOT_LENGTH, GFP_KERNEL);

        if(fw_boot == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_boot]Alloc %dk byte memory success.", (FW_BOOT_LENGTH/1024));
            break;
        }
    }

    if(retry >= 5)
    {
        GTP_ERROR("[burn_fw_boot]Alloc memory fail,exit.");
        return FAIL;
    }
    
    //step2:load firmware bootloader
    GTP_DEBUG("[burn_fw_boot]step2:load firmware bootloader");
    ret = gup_load_section_file(fw_boot, (4 * FW_SECTION_LENGTH + FW_DSP_LENGTH), FW_BOOT_LENGTH, SEEK_SET);

    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot]load firmware bootcode fail.");
        goto exit_burn_fw_boot;
    }
    
    //step3:hold ss51 & dsp
    GTP_DEBUG("[burn_fw_boot]step3:hold ss51 & dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot]hold ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_boot;
    }
    
    //step4:set scramble
    GTP_DEBUG("[burn_fw_boot]step4:set scramble");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot]set scramble fail.");
        ret = FAIL;
        goto exit_burn_fw_boot;
    }
    
    //step5:hold ss51 & release dsp
    GTP_DEBUG("[burn_fw_boot]step5:hold ss51 & release dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);                 //20121211
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot]release ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_boot;
    }
    //must delay
    msleep(1);
    
    //step6:select bank3
    GTP_DEBUG("[burn_fw_boot]step6:select bank3");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot]select bank3 fail.");
        ret = FAIL;
        goto exit_burn_fw_boot;
    }
    
    //step7:burn 2k bootloader firmware
    GTP_DEBUG("[burn_fw_boot]step7:burn 2k bootloader firmware");
    ret = gup_burn_proc(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot]burn fw_boot fail.");
        goto exit_burn_fw_boot;
    }
    
    //step7:send burn cmd to move data to flash from sram
    GTP_DEBUG("[burn_fw_boot]step7:send burn cmd to move data to flash from sram");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x06);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot]send burn cmd fail.");
        goto exit_burn_fw_boot;
    }
    GTP_DEBUG("[burn_fw_boot]Wait for the burn is complete......");
    do{
        ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
        if(ret <= 0)
        {
            GTP_ERROR("[burn_fw_boot]Get burn state fail");
            goto exit_burn_fw_boot;
        }
        msleep(10);
        //GTP_DEBUG("[burn_fw_boot]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]);
    }while(rd_buf[GTP_ADDR_LENGTH]);
    
    //step8:recall check 2k bootloader firmware
    GTP_DEBUG("[burn_fw_boot]step8:recall check 2k bootloader firmware");
    ret = gup_recall_check(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot]recall check 2k bootcode firmware fail.");
        goto exit_burn_fw_boot;
    }
    
    update_msg.fw_burned_len += FW_BOOT_LENGTH;
    GTP_DEBUG("[burn_fw_boot]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;
    
exit_burn_fw_boot:
    kfree(fw_boot);
    return ret;
}

static u8 gup_burn_fw_boot_isp(struct i2c_client *client)
{
    s32 ret = 0;
    u8* fw_boot_isp = NULL;
    u8  retry = 0;
    u8  rd_buf[5];
    
    if(update_msg.fw_burned_len >= update_msg.fw_total_len)
    {
        GTP_INFO("No need to upgrade the boot_isp code!");
        return SUCCESS;
    }
    GTP_DEBUG("[burn_fw_boot_isp]Begin burn bootloader firmware---->>");
    
    //step1:Alloc memory
    GTP_DEBUG("[burn_fw_boot_isp]step1:Alloc memory");
    while(retry++ < 5)
    {
        fw_boot_isp = (u8*)kzalloc(FW_BOOT_ISP_LENGTH, GFP_KERNEL);
        if(fw_boot_isp == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_boot_isp]Alloc %dk byte memory success.", (FW_BOOT_ISP_LENGTH/1024));
            break;
        }
    }
    if(retry >= 5)
    {
        GTP_ERROR("[burn_fw_boot_isp]Alloc memory fail,exit.");
        return FAIL;
    }
    
    //step2:load firmware bootloader
    GTP_DEBUG("[burn_fw_boot_isp]step2:load firmware bootloader isp");
    //ret = gup_load_section_file(fw_boot_isp, (4*FW_SECTION_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH+FW_DSP_ISP_LENGTH), FW_BOOT_ISP_LENGTH, SEEK_SET);
    ret = gup_load_section_file(fw_boot_isp, (update_msg.fw_burned_len - FW_DSP_ISP_LENGTH), FW_BOOT_ISP_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot_isp]load firmware boot_isp fail.");
        goto exit_burn_fw_boot_isp;
    }
    
    //step3:hold ss51 & dsp
    GTP_DEBUG("[burn_fw_boot_isp]step3:hold ss51 & dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot_isp]hold ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_boot_isp;
    }
    
    //step4:set scramble
    GTP_DEBUG("[burn_fw_boot_isp]step4:set scramble");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot_isp]set scramble fail.");
        ret = FAIL;
        goto exit_burn_fw_boot_isp;
    }
    
    
    //step5:hold ss51 & release dsp
    GTP_DEBUG("[burn_fw_boot_isp]step5:hold ss51 & release dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);                 //20121211
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot_isp]release ss51 & dsp fail.");
        ret = FAIL;
        goto exit_burn_fw_boot_isp;
    }
    //must delay
    msleep(1);
    
    //step6:select bank3
    GTP_DEBUG("[burn_fw_boot_isp]step6:select bank3");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot_isp]select bank3 fail.");
        ret = FAIL;
        goto exit_burn_fw_boot_isp;
    }
    
    //step7:burn 2k bootload_isp firmware
    GTP_DEBUG("[burn_fw_boot_isp]step7:burn 2k bootloader firmware");
    ret = gup_burn_proc(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot_isp]burn fw_section fail.");
        goto exit_burn_fw_boot_isp;
    }
    
    //step7:send burn cmd to move data to flash from sram
    GTP_DEBUG("[burn_fw_boot_isp]step7:send burn cmd to move data to flash from sram");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x07);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_boot_isp]send burn cmd fail.");
        goto exit_burn_fw_boot_isp;
    }
    GTP_DEBUG("[burn_fw_boot_isp]Wait for the burn is complete......");
    do{
        ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
        if(ret <= 0)
        {
            GTP_ERROR("[burn_fw_boot_isp]Get burn state fail");
            goto exit_burn_fw_boot_isp;
        }
        msleep(10);
        //GTP_DEBUG("[burn_fw_boot_isp]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]);
    }while(rd_buf[GTP_ADDR_LENGTH]);
    
    //step8:recall check 2k bootload_isp firmware
    GTP_DEBUG("[burn_fw_boot_isp]step8:recall check 2k bootloader firmware");
    ret = gup_recall_check(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_boot_isp]recall check 2k bootcode_isp firmware fail.");
        goto exit_burn_fw_boot_isp;
    }
    
    update_msg.fw_burned_len += FW_BOOT_ISP_LENGTH;
    GTP_DEBUG("[burn_fw_boot_isp]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;
    
exit_burn_fw_boot_isp:
    kfree(fw_boot_isp);
    return ret;
}

static u8 gup_burn_fw_link(struct i2c_client *client)
{
    s32 ret = 0;
    u8* fw_link = NULL;
    u8  retry = 0;
    u32 offset;
    
    if(update_msg.fw_burned_len >= update_msg.fw_total_len)
    {
        GTP_INFO("No need to upgrade the link code!");
        return SUCCESS;
    }
    GTP_DEBUG("[burn_fw_link]Begin burn link firmware---->>");
    
    //step1:Alloc memory
    GTP_DEBUG("[burn_fw_link]step1:Alloc memory");
    while(retry++ < 5)
    {
        fw_link = (u8*)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
        if(fw_link == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_link]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
            break;
        }
    }
    if(retry >= 5)
    {
        GTP_ERROR("[burn_fw_link]Alloc memory fail,exit.");
        return FAIL;
    }
    
    //step2:load firmware link section 1
    GTP_DEBUG("[burn_fw_link]step2:load firmware link section 1");
    offset = update_msg.fw_burned_len - FW_DSP_ISP_LENGTH;
    ret = gup_load_section_file(fw_link, offset, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_link]load firmware link section 1 fail.");
        goto exit_burn_fw_link;
    }
    
    
    //step3:burn link firmware section 1
    GTP_DEBUG("[burn_fw_link]step3:burn link firmware section 1");
    ret = gup_burn_fw_app_section(client, fw_link, 0x9000, FW_SECTION_LENGTH, 0x38);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_link]burn link firmware section 1 fail.");
        goto exit_burn_fw_link;
    }
    
    //step4:load link firmware section 2 file data
    GTP_DEBUG("[burn_fw_link]step4:load link firmware section 2 file data");
    offset += FW_SECTION_LENGTH;
    ret = gup_load_section_file(fw_link, offset, FW_LINK_LENGTH - FW_SECTION_LENGTH, SEEK_SET);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_link]load link firmware section 2 fail.");
        goto exit_burn_fw_link;
    }
    
    //step5:burn link firmware section 2
    GTP_DEBUG("[burn_fw_link]step4:burn link firmware section 2");
    ret = gup_burn_fw_app_section(client, fw_link, 0x9000, FW_LINK_LENGTH - FW_SECTION_LENGTH, 0x39);

    if (FAIL == ret)
    {
        GTP_ERROR("[burn_fw_link]burn link firmware section 2 fail.");
        goto exit_burn_fw_link;
    }
    
    update_msg.fw_burned_len += FW_LINK_LENGTH;
    GTP_DEBUG("[burn_fw_link]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;
    
exit_burn_fw_link:
    kfree(fw_link);
    return ret;
}

static u8 gup_burn_fw_app_section(struct i2c_client *client, u8 *fw_section, u16 start_addr, u32 len, u8 bank_cmd )
{
    s32 ret = 0;
    u8  rd_buf[5];
  
    //step1:hold ss51 & dsp
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_app_section]hold ss51 & dsp fail.");
        return FAIL;
    }
    
    //step2:set scramble
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_app_section]set scramble fail.");
        return FAIL;
    }
        
    //step3:hold ss51 & release dsp
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_app_section]hold ss51 & release dsp fail.");
        return FAIL;
    }
    //must delay
    msleep(1);
    
    //step4:select bank
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4)&0x0F);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4)&0x0F);
        return FAIL;
    }
    
    //step5:burn fw section
    ret = gup_burn_proc(client, fw_section, start_addr, len);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_section]burn fw_section fail.");
        return FAIL;
    }
    
    //step6:send burn cmd to move data to flash from sram
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd&0x0F);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_app_section]send burn cmd fail.");
        return FAIL;
    }
    GTP_DEBUG("[burn_fw_section]Wait for the burn is complete......");
    do{
        ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
        if(ret <= 0)
        {
            GTP_ERROR("[burn_fw_app_section]Get burn state fail");
            return FAIL;
        }
        msleep(10);
        //GTP_DEBUG("[burn_fw_app_section]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]);
    }while(rd_buf[GTP_ADDR_LENGTH]);
    
    //step7:recall fw section
    ret = gup_recall_check(client, fw_section, start_addr, len);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_section]recall check %dk firmware fail.", len/1024);
        return FAIL;
    }
    
    return SUCCESS;
}

static u8 gup_burn_fw_app_code(struct i2c_client *client)
{
    u8* fw_app_code = NULL;
    u8  retry = 0;
    s32 ret = 0;
    //u16 start_index = 4*FW_SECTION_LENGTH+FW_DSP_LENGTH+FW_BOOT_LENGTH + FW_DSP_ISP_LENGTH + FW_BOOT_ISP_LENGTH; // 32 + 4 + 2 + 4 = 42K
    u16 start_index;
    
    if(update_msg.fw_burned_len >= update_msg.fw_total_len)
    {
        GTP_INFO("No need to upgrade the app code!");
        return SUCCESS;
    }
    start_index = update_msg.fw_burned_len - FW_DSP_ISP_LENGTH;
    GTP_DEBUG("[burn_fw_app_code]Begin burn app_code firmware---->>");
    
    //step1:alloc memory
    GTP_DEBUG("[burn_fw_app_code]step1:alloc memory");
    while(retry++ < 5)
    {
        fw_app_code = (u8*)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
        if(fw_app_code == NULL)
        {
            continue;
        }
        else
        {
            GTP_INFO("[burn_fw_app_code]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
            break;
        }
    }
    if(retry >= 5)
    {
        GTP_ERROR("[burn_fw_app_code]Alloc memory fail,exit.");
        return FAIL;
    }
    
    //step2:load app_code firmware section 1 file data
    GTP_DEBUG("[burn_fw_app_code]step2:load app_code firmware section 1 file data");
    ret = gup_load_section_file(fw_app_code, start_index, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]load app_code firmware section 1 fail.");
        goto exit_burn_fw_app_code;
    }
  
    //step3:burn app_code firmware section 1
    GTP_DEBUG("[burn_fw_app_code]step3:burn app_code firmware section 1");
    ret = gup_burn_fw_app_section(client, fw_app_code, 0x9000, FW_SECTION_LENGTH, 0x3A);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]burn app_code firmware section 1 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step5:load app_code firmware section 2 file data
    GTP_DEBUG("[burn_fw_app_code]step5:load app_code firmware section 2 file data");
    ret = gup_load_section_file(fw_app_code, start_index+FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]load app_code firmware section 2 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step6:burn app_code firmware section 2
    GTP_DEBUG("[burn_fw_app_code]step6:burn app_code firmware section 2");
    ret = gup_burn_fw_app_section(client, fw_app_code, 0x9000, FW_SECTION_LENGTH, 0x3B);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]burn app_code firmware section 2 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step7:load app_code firmware section 3 file data
    GTP_DEBUG("[burn_fw_app_code]step7:load app_code firmware section 3 file data");
    ret = gup_load_section_file(fw_app_code, start_index+2*FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]load app_code firmware section 3 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step8:burn app_code firmware section 3
    GTP_DEBUG("[burn_fw_app_code]step8:burn app_code firmware section 3");
    ret = gup_burn_fw_app_section(client, fw_app_code, 0x9000, FW_SECTION_LENGTH, 0x3C);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]burn app_code firmware section 3 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step9:load app_code firmware section 4 file data
    GTP_DEBUG("[burn_fw_app_code]step9:load app_code firmware section 4 file data");
    ret = gup_load_section_file(fw_app_code, start_index + 3*FW_SECTION_LENGTH, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]load app_code firmware section 4 fail.");
        goto exit_burn_fw_app_code;
    }
    
    //step10:burn app_code firmware section 4
    GTP_DEBUG("[burn_fw_app_code]step10:burn app_code firmware section 4");
    ret = gup_burn_fw_app_section(client, fw_app_code, 0x9000, FW_SECTION_LENGTH, 0x3D);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_app_code]burn app_code firmware section 4 fail.");
        goto exit_burn_fw_app_code;
    }
    
    update_msg.fw_burned_len += FW_APP_CODE_LENGTH;
    GTP_DEBUG("[burn_fw_gwake]Burned length:%d", update_msg.fw_burned_len);
    ret = SUCCESS;
    
exit_burn_fw_app_code:
    kfree(fw_app_code);
    return ret;
}

static u8 gup_burn_fw_finish(struct i2c_client *client)
{
    u8* fw_ss51 = NULL;
    u8  retry = 0;
    s32 ret = 0;
    
    GTP_INFO("[burn_fw_finish]burn first 8K of ss51 and finish update.");
    //step1:alloc memory
    GTP_DEBUG("[burn_fw_finish]step1:alloc memory");
    while(retry++ < 5)
    {
        fw_ss51 = (u8*)kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
        if(fw_ss51 == NULL)
        {
            continue;
        }
        else
        {
            GTP_DEBUG("[burn_fw_finish]Alloc %dk byte memory success.", (FW_SECTION_LENGTH/1024));
            break;
        }
    }
    if(retry >= 5)
    {
        GTP_ERROR("[burn_fw_finish]Alloc memory fail,exit.");
        return FAIL;
    }
    
    GTP_DEBUG("[burn_fw_finish]step2: burn ss51 first 8K.");
    ret = gup_load_section_file(fw_ss51, 0, FW_SECTION_LENGTH, SEEK_SET);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_finish]load ss51 firmware section 1 fail.");
        goto exit_burn_fw_finish;
    }

    GTP_DEBUG("[burn_fw_finish]step3:clear control flag");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_finish]clear control flag fail.");
        goto exit_burn_fw_finish;
    }
    
    GTP_DEBUG("[burn_fw_finish]step4:burn ss51 firmware section 1");
    ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);
    if(FAIL == ret)
    {
        GTP_ERROR("[burn_fw_finish]burn ss51 firmware section 1 fail.");
        goto exit_burn_fw_finish;
    }
    
    //step11:enable download DSP code 
    GTP_DEBUG("[burn_fw_finish]step5:enable download DSP code ");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x99);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_finish]enable download DSP code fail.");
        goto exit_burn_fw_finish;
    }
    
    //step12:release ss51 & hold dsp
    GTP_DEBUG("[burn_fw_finish]step6:release ss51 & hold dsp");
    ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x08);
    if(ret <= 0)
    {
        GTP_ERROR("[burn_fw_finish]release ss51 & hold dsp fail.");
        goto exit_burn_fw_finish;
    }

    if (fw_ss51)
    {
        kfree(fw_ss51);
    }
    return SUCCESS;
    
exit_burn_fw_finish:
    if (fw_ss51)
    {
        kfree(fw_ss51);
    }
    return FAIL;
}

s32 gup_update_proc(void *dir)
{
    s32 ret = 0;
    u8  retry = 0;
    s32 update_ret = FAIL;
    st_fw_head fw_head;
#if GT917S_UPDATE_RULE
	char temp_data[6] = {0};
#endif

    GTP_INFO("[update_proc]Begin update ......");
    
#if GTP_AUTO_UPDATE
    if (1 == searching_file)
    {
        u8 timeout = 0;
        searching_file = 0;     // exit .bin update file searching 
        GTP_INFO("Exiting searching file for auto update.");
        while ((show_len != 200) && (show_len != 100) && (timeout++ < 150))     // wait for auto update quitted completely
        {
            msleep(100);
        }
    }
#endif
    
    show_len = 1;
    total_len = 100; 
    
#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        return gup_fw_download_proc(dir, GTP_FL_FW_BURN);
    }
#endif
    update_msg.file = NULL;
    ret = gup_check_update_file(i2c_client_point, &fw_head, (u8 *)dir);    //20121212

    if (FAIL == ret)
    {
        GTP_ERROR("[update_proc]check update file fail.");
        goto file_fail;
    }
    
    //gtp_reset_guitar(i2c_client_point, 20);
    ret = gup_get_ic_fw_msg(i2c_client_point);

    if (FAIL == ret)
    {
        GTP_ERROR("[update_proc]get ic message fail.");
        goto file_fail;
    }

    ret = gup_enter_update_judge(&fw_head);                             //20121212

#if GT917S_UPDATE_RULE
	i2c_read_bytes(i2c_client_point, 0x8140, temp_data, 6);
	GTP_INFO("917S firmware detect add function temp_data = (%c, %c, %c, %c)!",temp_data[0], temp_data[1],temp_data[2],temp_data[3]);
	if(temp_data[0]==57 && temp_data[1]==49 && temp_data[2]==55 && temp_data[3]==83)
	{
		GTP_INFO("GT917S firmware detect Force Update!");
		ret = SUCCESS;
	}
#endif

#if GTP_FORCE_UPDATE_FW
		ret = SUCCESS;								//for test
#endif

    if (FAIL == ret)
    {
        GTP_ERROR("[update_proc]Check *.bin file fail.");
        goto file_fail;
    }
	
#ifdef CONFIG_OF_TOUCH
	disable_irq(touch_irq);
#else
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

#if GTP_ESD_PROTECT
    gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
    ret = gup_enter_update_mode(i2c_client_point);

    if (FAIL == ret)
    {
        GTP_ERROR("[update_proc]enter update mode fail.");
        goto update_fail;
    }

    while (retry++ < 5)
    {
        show_len = 10;
        total_len = 100;
        update_msg.fw_burned_len = 0;
        GTP_DEBUG("[update_proc]Burned length:%d", update_msg.fw_burned_len);
        ret = gup_burn_dsp_isp(i2c_client_point);
        if(FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn dsp isp fail.");
            continue;
        }
        
        show_len = 30;
        ret = gup_burn_fw_ss51(i2c_client_point);
        if(FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn ss51 firmware fail.");
            continue;
        }
        
        show_len = 40;
        ret = gup_burn_fw_dsp(i2c_client_point);
        if(FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn dsp firmware fail.");
            continue;
        }
        
        show_len = 50;
        ret = gup_burn_fw_boot(i2c_client_point);
        if(FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn bootloader firmware fail.");
            continue;
        }
        show_len = 60;
        
        ret = gup_burn_fw_boot_isp(i2c_client_point);
        if (FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn boot_isp firmware fail.");
            continue;
        }
        
        show_len = 70;
        ret = gup_burn_fw_link(i2c_client_point);
        if (FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn link firmware fail.");
            continue;
        }
        
        show_len = 80;
        ret = gup_burn_fw_app_code(i2c_client_point);
        if (FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn app_code firmware fail.");
            continue;
        }       
        show_len = 90;
        ret = gup_burn_fw_finish(i2c_client_point);
        if (FAIL == ret)
        {
            GTP_ERROR("[update_proc]burn finish fail.");
            continue;
        }
        show_len = 95;
        GTP_INFO("[update_proc]UPDATE SUCCESS.");
        break;
    }

    if (retry >= 5)
    {
        GTP_ERROR("[update_proc]retry timeout,UPDATE FAIL.");
        update_ret = FAIL;
    }
    else
    {
        update_ret = SUCCESS;
    }
    
update_fail:
    GTP_DEBUG("[update_proc]leave update mode.");
    gup_leave_update_mode();    
    
    if (SUCCESS == update_ret)
    {
        GTP_DEBUG("[update_proc]send config.");
        ret = gtp_send_cfg(i2c_client_point);
        if (ret < 0)
        {
            GTP_ERROR("[update_proc]send config fail.");
        }
    }
    
#if GTP_ESD_PROTECT
    gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif

file_fail:

#ifdef CONFIG_OF_TOUCH
	enable_irq(touch_irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif 

    
	if (update_msg.file && !IS_ERR(update_msg.file))
	{
        if (update_msg.old_fs)
        {
            set_fs(update_msg.old_fs);
        }
		filp_close(update_msg.file, NULL);
	}
#if (GTP_AUTO_UPDATE && GTP_HEADER_FW_UPDATE && GTP_AUTO_UPDATE_CFG)
    if (NULL == dir)
    {
        gup_search_file(AUTO_SEARCH_CFG);
        if (got_file_flag & CFG_FILE_READY)
        {
            ret = gup_update_config(i2c_client_point);
            if (FAIL == ret)
            {
                GTP_ERROR("Update config failed!");
            }
        }
    }    
#endif

    total_len = 100;
    if (SUCCESS == update_ret)
    {
        show_len = 100;
        return SUCCESS;
    }
    else
    {
        show_len = 200;
        return FAIL;
    }
}

u8 gup_init_update_proc(struct i2c_client *client)
{
    struct task_struct *thread = NULL;
    
    GTP_INFO("Ready to run auto update thread");

#if GTP_COMPATIBLE_MODE
    if (CHIP_TYPE_GT9F == gtp_chip_type)
    {
        thread = kthread_run(gup_update_proc, "update", "fl_auto_update");
    }
    else
#endif
    {
        thread = kthread_run(gup_update_proc, (void *)NULL, "guitar_update");
    }
    if (IS_ERR(thread))
    {
        GTP_ERROR("Failed to create update thread.\n");
        return -1;
    }

    return 0;
}


//******************* For GT9XXF Start ********************//

#define FL_UPDATE_PATH              "/data/_fl_update_.bin"
#define FL_UPDATE_PATH_SD           "/sdcard/_fl_update_.bin" 

#define GUP_FW_CHK_SIZE              256
#define MAX_CHECK_TIMES              128    // max: 2 * (16 * 1024) / 256 = 128

//for clk cal
#define PULSE_LENGTH      (200)
#define INIT_CLK_DAC      (50)
#define MAX_CLK_DAC       (120)
#define CLK_AVG_TIME      (1)
#define MILLION           1000000

#define _wRW_MISCTL__RG_DMY                       0x4282
#define _bRW_MISCTL__RG_OSC_CALIB                 0x4268
#define _fRW_MISCTL__GIO0                         0x41e9
#define _fRW_MISCTL__GIO1                         0x41ed
#define _fRW_MISCTL__GIO2                         0x41f1
#define _fRW_MISCTL__GIO3                         0x41f5
#define _fRW_MISCTL__GIO4                         0x41f9
#define _fRW_MISCTL__GIO5                         0x41fd
#define _fRW_MISCTL__GIO6                         0x4201
#define _fRW_MISCTL__GIO7                         0x4205
#define _fRW_MISCTL__GIO8                         0x4209
#define _fRW_MISCTL__GIO9                         0x420d
#define _fRW_MISCTL__MEA                          0x41a0
#define _bRW_MISCTL__MEA_MODE                     0x41a1
#define _wRW_MISCTL__MEA_MAX_NUM                  0x41a4
#define _dRO_MISCTL__MEA_VAL                      0x41b0
#define _bRW_MISCTL__MEA_SRCSEL                   0x41a3
#define _bRO_MISCTL__MEA_RDY                      0x41a8
#define _rRW_MISCTL__ANA_RXADC_B0_                0x4250
#define _bRW_MISCTL__RG_LDO_A18_PWD               0x426f
#define _bRW_MISCTL__RG_BG_PWD                    0x426a
#define _bRW_MISCTL__RG_CLKGEN_PWD                0x4269
#define _fRW_MISCTL__RG_RXADC_PWD                 0x426a
#define _bRW_MISCTL__OSC_CK_SEL                   0x4030
#define _rRW_MISCTL_RG_DMY83                      0x4283
#define _rRW_MISCTL__GIO1CTL_B2_                  0x41ee
#define _rRW_MISCTL__GIO1CTL_B1_                  0x41ed

#if GTP_COMPATIBLE_MODE

u8 gup_check_fs_mounted(char *path_name)
{
    struct path root_path;
    struct path path;
    int err;
    err = kern_path("/", LOOKUP_FOLLOW, &root_path);

    if (err){
		path_put(&root_path);
        return FAIL;
    }

    err = kern_path(path_name, LOOKUP_FOLLOW, &path);
    if (err){
		path_put(&root_path);
		path_put(&path);
        return FAIL;
    }

    if (path.mnt->mnt_sb == root_path.mnt->mnt_sb)
    {
        //-- not mounted
        path_put(&root_path);
		path_put(&path);
        return FAIL;
    }
    else
    {
        path_put(&root_path);
		path_put(&path);
        return SUCCESS;
    }
}

s32 gup_hold_ss51_dsp(struct i2c_client *client)
{
    s32 ret = -1;
    s32 retry = 0;
    u8 rd_buf[3];
    
    while(retry++ < 200)
    {
        // step4:Hold ss51 & dsp
        ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
        if(ret <= 0)
        {
            GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
            continue;
        }
        
        // step5:Confirm hold
        ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
        if (ret <= 0)
        {
            GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
            continue;
        }
        if (0x0C == rd_buf[GTP_ADDR_LENGTH])
        {
            GTP_DEBUG("[enter_update_mode]Hold ss51 & dsp confirm SUCCESS");
            break;
        }
        GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d", rd_buf[GTP_ADDR_LENGTH]);
    }
    if(retry >= 200)
    {
        GTP_ERROR("Enter update Hold ss51 failed.");
        return FAIL;
    }
        //DSP_CK and DSP_ALU_CK PowerOn
    ret = gup_set_ic_msg(client, 0x4010, 0x00);
    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]DSP_CK and DSP_ALU_CK PowerOn fail.");
        return FAIL;
    }
    
    //disable wdt
    ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]disable wdt fail.");
        return FAIL;
    }
    
    //clear cache enable
    ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]clear cache enable fail.");
        return FAIL;
    }
    
    //set boot from sram
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]set boot from sram fail.");
        return FAIL;
    }

	//software reboot    
	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);
	if (ret <= 0)
	{    
	    GTP_ERROR("[enter_update_mode]software reboot fail.");
	    return FAIL;
	}
    
    return SUCCESS;
}

s32 gup_enter_update_mode_fl(struct i2c_client *client)
{
    s32 ret = -1;
    //s32 retry = 0;
    //u8 rd_buf[3];
    
    //step1:RST output low last at least 2ms
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 0);
    msleep(2);
    
    //step2:select I2C slave addr,INT:0--0xBA;1--0x28.
    GTP_GPIO_OUTPUT(GTP_INT_PORT, (client->addr == 0x14));
    msleep(2);
    
    //step3:RST output high reset guitar
    GTP_GPIO_OUTPUT(GTP_RST_PORT, 1);
    
    msleep(5);
    
    //select addr & hold ss51_dsp
    ret = gup_hold_ss51_dsp(client);
    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]hold ss51 & dsp failed.");
        return FAIL;
    }
    
    //clear control flag
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]clear control flag fail.");
        return FAIL;
    }
    
    //set scramble
    ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]set scramble fail.");
        return FAIL;
    }
    
    //enable accessing code
    ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]enable accessing code fail.");
        return FAIL;
    }
    
    return SUCCESS;
}


static s32 gup_prepare_fl_fw(char *path, st_fw_head *fw_head)
{
    s32 i = 0;
    s32 ret = 0;
    s32 timeout = 0;
    
    if (!memcmp(path, "update", 6))
    {
        GTP_INFO("Search for Flashless firmware file to update");
        searching_file = 1;
        for (i = 0; i < GUP_SEARCH_FILE_TIMES; ++i)
        {
            if (0 == searching_file)
            {
                GTP_INFO("Force terminate searching file auto update.");
                return FAIL;
            }
            update_msg.file = filp_open(FL_UPDATE_PATH, O_RDONLY, 0);
            if (IS_ERR(update_msg.file))
            {
                update_msg.file = filp_open(FL_UPDATE_PATH_SD, O_RDONLY, 0);
                if (IS_ERR(update_msg.file))
                {
                    msleep(2000);
                    continue;
                }
                else
                {
                    path = FL_UPDATE_PATH_SD;
                    break;
                }
            }
            else
            {
                path = FL_UPDATE_PATH;
                break;
            }
        }
        searching_file = 0;
        if (i >= 50)
        {
            GTP_ERROR("Search timeout, update aborted");
            return FAIL;
        }
        else
        {
            _CLOSE_FILE(update_msg.file);
        }
        while (rqst_processing && (timeout++ < 15))
        {
            GTP_INFO("wait for request process completed!");
            msleep(1000);
        }
    }
    
    GTP_INFO("Firmware update file path: %s", path);
    update_msg.file = filp_open(path, O_RDONLY, 0);
    
    if (IS_ERR(update_msg.file))
    {
        GTP_ERROR("Open update file(%s) error!", path);
        return FAIL;
    }
    
    update_msg.old_fs = get_fs();
    set_fs(KERNEL_DS);
    update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
    update_msg.fw_total_len = update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_END);
    if (sizeof(gtp_default_FW_fl) != update_msg.fw_total_len)
    {
        //GTP_ERROR("Inconsistent firmware size. File size: %d, default fw size: %d", update_msg.fw_total_len, sizeof(gtp_default_FW_fl));
        set_fs(update_msg.old_fs);
        _CLOSE_FILE(update_msg.file);
        return FAIL;
    }
    
    GTP_DEBUG("Firmware size: %d", update_msg.fw_total_len);
    update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
    update_msg.file->f_op->read(update_msg.file, (char*)fw_head, FW_HEAD_LENGTH, &update_msg.file->f_pos);
    update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
    //copy fw file to gtp_default_FW_fl array
    ret = update_msg.file->f_op->read(update_msg.file, 
                                (char*)gtp_default_FW_fl,
                                update_msg.fw_total_len,
                                &update_msg.file->f_pos);
    if (ret < 0)
    {
        GTP_ERROR("Failed to read firmware data from %s, err-code: %d", path, ret);
        ret = FAIL;
    }
    else
    {
        ret = SUCCESS;
    }
    set_fs(update_msg.old_fs);
    _CLOSE_FILE(update_msg.file);
    return ret;
}

static u8 gup_check_update_file_fl(struct i2c_client *client, st_fw_head *fw_head, char *path)
{
    s32 i = 0;
    s32 fw_checksum = 0;
    s32 ret = 0;
    
    if (NULL != path)
    {
        ret = gup_prepare_fl_fw(path, fw_head);
        if (ret == FAIL)
        {
            return FAIL;
        }
    }
    else
    {
        memcpy(fw_head, gtp_default_FW_fl, FW_HEAD_LENGTH);
        GTP_INFO("FILE HARDWARE INFO:%02x%02x%02x%02x", fw_head->hw_info[0], fw_head->hw_info[1], fw_head->hw_info[2], fw_head->hw_info[3]);
        GTP_INFO("FILE PID:%s", fw_head->pid);
        fw_head->vid = ((fw_head->vid & 0xFF00) >> 8) + ((fw_head->vid & 0x00FF) << 8);
        GTP_INFO("FILE VID:%04x", fw_head->vid);
    }

    //check firmware legality
    fw_checksum = 0;
    for (i = FW_HEAD_LENGTH; i < (FW_HEAD_LENGTH + update_msg.fw_total_len); i += 2)
    {
        fw_checksum = (gtp_default_FW_fl[i]<<8) + gtp_default_FW_fl[i+1];
    }
    
    GTP_DEBUG("firmware checksum:%x", fw_checksum&0xFFFF);
    if(fw_checksum&0xFFFF)
    {
        GTP_ERROR("Illegal firmware file.");
        return FAIL;
    }
    
    return SUCCESS;
}


static u8 gup_download_fw_ss51(struct i2c_client *client, u8 dwn_mode)
{
    s32 ret = 0;

    if(GTP_FL_FW_BURN == dwn_mode)
    {
        GTP_INFO("[download_fw_ss51]Begin download ss51 firmware---->>");
    }
    else
    {
        GTP_INFO("[download_fw_ss51]Begin check ss51 firmware----->>");
    }
    //step1:download FW section 1
    GTP_DEBUG("[download_fw_ss51]step1:download FW section 1");
    ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__SRAM_BANK, 0x00);
    
    if (ret <= 0)
    {
        GTP_ERROR("[download_fw_ss51]select bank0 fail.");
        ret = FAIL;
        goto exit_download_fw_ss51;
    }
    
    
    ret = i2c_write_bytes(client, 0xC000, 
                &gtp_default_FW_fl[FW_HEAD_LENGTH], FW_DOWNLOAD_LENGTH);   // write the first bank

    if (ret == -1)
    {
        GTP_ERROR("[download_fw_ss51]download FW section 1 fail.");
        ret = FAIL;
        goto exit_download_fw_ss51;
    }
    
    
    if (GTP_FL_FW_BURN == dwn_mode)
    {
        ret = gup_check_and_repair(i2c_client_point, 
                                   0xC000, 
                                   &gtp_default_FW_fl[FW_HEAD_LENGTH], 
                                   FW_DOWNLOAD_LENGTH);
        if(FAIL == ret)
        {
            GTP_ERROR("[download_fw_ss51]Checked FW section 1 fail.");
            goto exit_download_fw_ss51;
        }
    }
    
    //step2:download FW section 2
    GTP_DEBUG("[download_fw_ss51]step2:download FW section 1");
    ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__SRAM_BANK, 0x01);
    
    if (ret <= 0)
    {
        GTP_ERROR("[download_fw_ss51]select bank1 fail.");
        ret = FAIL;
        goto exit_download_fw_ss51;
    }

    ret = i2c_write_bytes(client, 0xC000, &gtp_default_FW_fl[FW_HEAD_LENGTH+FW_DOWNLOAD_LENGTH],FW_DOWNLOAD_LENGTH);  // write the second bank

    if (ret == -1)
    {
        GTP_ERROR("[download_fw_ss51]download FW section 2 fail.");
        ret = FAIL;
        goto exit_download_fw_ss51;
    }
    
    if (GTP_FL_FW_BURN == dwn_mode)
    {
        ret = gup_check_and_repair(i2c_client_point, 
                                   0xC000, 
                                   &gtp_default_FW_fl[FW_HEAD_LENGTH+FW_DOWNLOAD_LENGTH], 
                                   FW_DOWNLOAD_LENGTH);
        
        if(FAIL == ret)
        {
            GTP_ERROR("[download_fw_ss51]Checked FW section 2 fail.");
            goto exit_download_fw_ss51;
        }
    }
    ret = SUCCESS;

exit_download_fw_ss51:
    
    return ret;
}
#if (!GTP_SUPPORT_I2C_DMA)
static s32 i2c_auto_read(struct i2c_client *client,u8 *rxbuf, int len)
{
    u8 retry;
    u16 left = len;
    u16 offset = 0;
    
    struct i2c_msg msg = 
    {
        //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
        .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)),
        .flags = I2C_M_RD,
        .timing = I2C_MASTER_CLOCK
    };
    
    if(NULL == rxbuf)
    {
        return -1;
    }
    
    while (left > 0)
    {
        msg.buf = &rxbuf[offset];

        if (left > MAX_TRANSACTION_LENGTH)
        {
            msg.len = MAX_TRANSACTION_LENGTH;
            left -= MAX_TRANSACTION_LENGTH;
            offset += MAX_TRANSACTION_LENGTH;
        }
        else
        {
            msg.len = left;
            left = 0;
        }

        retry = 0;

        while (i2c_transfer(client->adapter, &msg, 1) != 1)
        {
            retry++;

            if (retry == 20)
            {
                GTP_ERROR("I2C read 0x%X length=%d failed\n", offset, len);
                return -1;
            }
        }
    }
    
    return 0;
}
#endif
static u8 gup_check_and_repair(struct i2c_client *client, s32 chk_start_addr, u8 *target_fw, u32 chk_total_length)
{
    s32 ret = 0;
    u32 chked_len = 0;
    u8  chked_times = 0;
    u32 chk_addr = 0;
    u8  chk_buf[GUP_FW_CHK_SIZE];
    u32 rd_size = 0;
    u8  flag_err = 0;
    s32 i = 0;
    
    chk_addr = chk_start_addr;
    while((chked_times < MAX_CHECK_TIMES) && (chked_len < chk_total_length))
    {
        rd_size = chk_total_length - chked_len;
        if(rd_size >= GUP_FW_CHK_SIZE)
        {
            rd_size = GUP_FW_CHK_SIZE;
        }
    #if GTP_SUPPORT_I2C_DMA
        ret = i2c_read_bytes(client, chk_addr, chk_buf, rd_size);
    #else
        if (!i)
        {
            ret = i2c_read_bytes(client, chk_addr, chk_buf, rd_size);
        }
        else
        {
            ret = i2c_auto_read(client, chk_buf, rd_size);
        }
    #endif
    
        if(-1 == ret)
        {
            GTP_ERROR("Read chk ram fw i2c error");
            chked_times++;
            continue;
        }
        
        for(i=0; i<rd_size; i++)
        {
            if(chk_buf[i] != target_fw[i])
            {
            	GTP_ERROR("chk_buf[%d] = 0x%x, target_fw[%d] = 0x%x.", i,chk_buf[i],i,target_fw[i]);
                GTP_ERROR("Ram pos[0x%04x] checked failed,rewrite.", chk_addr + i);
                i2c_write_bytes(client, chk_addr+i, &target_fw[i], rd_size-i);
                flag_err = 1;
                i = 0;
                break;
            }
        }
        
        if(!flag_err)
        {
            GTP_DEBUG("Ram pos[0x%04X] check pass!", chk_addr);
            chked_len += rd_size;
            target_fw += rd_size;
            chk_addr += rd_size;
        }
        else
        {
            flag_err = 0;
            chked_times++;
        }      
    }
    
    if(chked_times >= MAX_CHECK_TIMES)
    {
        GTP_ERROR("Ram data check failed.");
        return FAIL;
    }
    return SUCCESS;
}

static u8 gup_download_fw_dsp(struct i2c_client *client, u8 dwn_mode)
{
    s32 ret = 0;
    
    if(GTP_FL_FW_BURN == dwn_mode)
    {
        GTP_INFO("[download_fw_dsp]Begin download dsp fw---->>");
    }
    else
    {
        GTP_INFO("[download_fw_dsp]Begin check dsp fw---->>");
    }

    //step1:select bank2
    GTP_DEBUG("[download_fw_dsp]step1:select bank2");
    ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);

    if (ret <= 0)
    {
        GTP_ERROR("[download_fw_dsp]select bank2 fail.");
        ret = FAIL;
        goto exit_download_fw_dsp;
    }
    
    ret = i2c_write_bytes(client,
                          0xC000,
                          &gtp_default_FW_fl[FW_HEAD_LENGTH+2*FW_DOWNLOAD_LENGTH],
                          FW_DSP_LENGTH); // write the second bank    
    if (ret == -1)
    {
        GTP_ERROR("[download_fw_dsp]download FW dsp fail.");
        ret = FAIL;
        goto exit_download_fw_dsp;
    }

    if (GTP_FL_FW_BURN == dwn_mode)
    {
        ret = gup_check_and_repair(client, 
                                   0xC000, 
                                   &gtp_default_FW_fl[FW_HEAD_LENGTH+2*FW_DOWNLOAD_LENGTH], 
                                   FW_DSP_LENGTH);
        
        if(FAIL == ret)
        {
            GTP_ERROR("[download_fw_dsp]Checked FW dsp fail.");
            goto exit_download_fw_dsp;
        }
    
    }
    ret = SUCCESS;

exit_download_fw_dsp:
    
    return ret;
}

s32 gup_fw_download_proc(void *dir, u8 dwn_mode)
{
    s32 ret = 0;
    u8  retry = 0;
    st_fw_head fw_head;
    
    if(GTP_FL_FW_BURN == dwn_mode)
    {
        GTP_INFO("[fw_download_proc]Begin fw download ......");
    }
    else
    {
        GTP_INFO("[fw_download_proc]Begin fw check ......");
    }
    show_len = 0;
    total_len = 100;
    
    ret = gup_check_update_file_fl(i2c_client_point, &fw_head, (char *)dir);
    
    show_len = 10;
    
    if (FAIL == ret)
    {
        GTP_ERROR("[fw_download_proc]check update file fail.");
        goto file_fail;
    }
    
#ifdef CONFIG_OF_TOUCH
	disable_irq(touch_irq);
#else
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

    if (NULL != dir)
    {
    #if GTP_ESD_PROTECT
        gtp_esd_switch(i2c_client_point, SWITCH_OFF);
    #endif
    }

    ret = gup_enter_update_mode_fl(i2c_client_point);
    show_len = 20;
    
    if (FAIL == ret)
    {
        GTP_ERROR("[fw_download_proc]enter update mode fail.");
        goto download_fail;
    }

    while (retry++ < 5)
    {
        ret = gup_download_fw_ss51(i2c_client_point, dwn_mode);
        show_len = 60;
        if (FAIL == ret)
        {
            GTP_ERROR("[fw_download_proc]burn ss51 firmware fail.");
            continue;
        }

        ret = gup_download_fw_dsp(i2c_client_point, dwn_mode);
        show_len = 80;
        if (FAIL == ret)
        {
            GTP_ERROR("[fw_download_proc]burn dsp firmware fail.");
            continue;
        }

        GTP_INFO("[fw_download_proc]UPDATE SUCCESS.");
        break;
    }

    if (retry >= 5)
    {
        GTP_ERROR("[fw_download_proc]retry timeout,UPDATE FAIL.");
        goto download_fail;
    }

    if (NULL != dir)
    {
        gtp_fw_startup(i2c_client_point);
    #if GTP_ESD_PROTECT
        gtp_esd_switch(i2c_client_point, SWITCH_ON);
    #endif    
    }
    show_len = 100;
	
#ifdef CONFIG_OF_TOUCH
	enable_irq(touch_irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif 

    return SUCCESS;
    
download_fail:
    if (NULL != dir)
    {
        gtp_fw_startup(i2c_client_point);
    #if GTP_ESD_PROTECT
        gtp_esd_switch(i2c_client_point, SWITCH_ON);
    #endif    
    }
    
file_fail:
    show_len = 200;
	
#ifdef CONFIG_OF_TOUCH
	enable_irq(touch_irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif 

    return FAIL;
}


static void gup_bit_write(s32 addr, s32 bit, s32 val)
{
    u8 buf;
    i2c_read_bytes(i2c_client_point, addr, &buf, 1);

    buf = (buf & (~((u8)1 << bit))) | ((u8)val << bit);

    i2c_write_bytes(i2c_client_point, addr, &buf, 1);
}

static void gup_clk_count_init(s32 bCh, s32 bCNT)
{
    u8 buf;
    
    //_fRW_MISCTL__MEA_EN = 0; //Frequency measure enable
    gup_bit_write(_fRW_MISCTL__MEA, 0, 0);
    //_fRW_MISCTL__MEA_CLR = 1; //Frequency measure clear
    gup_bit_write(_fRW_MISCTL__MEA, 1, 1);
    //_bRW_MISCTL__MEA_MODE = 0; //Pulse mode
    buf = 0;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__MEA_MODE, &buf, 1);
    //_bRW_MISCTL__MEA_SRCSEL = 8 + bCh; //From GIO1
    buf = 8 + bCh;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__MEA_SRCSEL, &buf, 1);
    //_wRW_MISCTL__MEA_MAX_NUM = bCNT; //Set the Measure Counts = 1
    buf = bCNT;
    i2c_write_bytes(i2c_client_point, _wRW_MISCTL__MEA_MAX_NUM, &buf, 1);
    //_fRW_MISCTL__MEA_CLR = 0; //Frequency measure not clear
    gup_bit_write(_fRW_MISCTL__MEA, 1, 0);
    //_fRW_MISCTL__MEA_EN = 1;
    gup_bit_write(_fRW_MISCTL__MEA, 0, 1);
}

static u32 gup_clk_count_get(void)
{
    s32 ready = 0;
    s32 temp;
    s8  buf[4];

    while ((ready == 0)) //Wait for measurement complete
    {
        i2c_read_bytes(i2c_client_point, _bRO_MISCTL__MEA_RDY, buf, 1);
        ready = buf[0];
    }

    udelay(50);

    //_fRW_MISCTL__MEA_EN = 0;
    gup_bit_write(_fRW_MISCTL__MEA, 0, 0);
    i2c_read_bytes(i2c_client_point, _dRO_MISCTL__MEA_VAL, buf, 4);
    GTP_INFO("Clk_count 0: %2X", buf[0]);
    GTP_INFO("Clk_count 1: %2X", buf[1]);
    GTP_INFO("Clk_count 2: %2X", buf[2]);
    GTP_INFO("Clk_count 3: %2X", buf[3]);

    temp = (s32)buf[0] + ((s32)buf[1] << 8) + ((s32)buf[2] << 16) + ((s32)buf[3] << 24);
    GTP_INFO("Clk_count : %d", temp);
    return temp;
}
u8 gup_clk_dac_setting(int dac)
{
    s8 buf1, buf2;
    
    i2c_read_bytes(i2c_client_point, _wRW_MISCTL__RG_DMY, &buf1, 1);
    i2c_read_bytes(i2c_client_point, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);

    buf1 = (buf1 & 0xFFCF) | ((dac & 0x03) << 4);
    buf2 = (dac >> 2) & 0x3f;

    i2c_write_bytes(i2c_client_point, _wRW_MISCTL__RG_DMY, &buf1, 1);
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);
    
    return 0;
}

static u8 gup_clk_calibration_pin_select(s32 bCh)
{
    s32 i2c_addr;

    switch (bCh)
    {
        case 0:
            i2c_addr = _fRW_MISCTL__GIO0;
            break;

        case 1:
            i2c_addr = _fRW_MISCTL__GIO1;
            break;

        case 2:
            i2c_addr = _fRW_MISCTL__GIO2;
            break;

        case 3:
            i2c_addr = _fRW_MISCTL__GIO3;
            break;

        case 4:
            i2c_addr = _fRW_MISCTL__GIO4;
            break;

        case 5:
            i2c_addr = _fRW_MISCTL__GIO5;
            break;

        case 6:
            i2c_addr = _fRW_MISCTL__GIO6;
            break;

        case 7:
            i2c_addr = _fRW_MISCTL__GIO7;
            break;

        case 8:
            i2c_addr = _fRW_MISCTL__GIO8;
            break;

        case 9:
            i2c_addr = _fRW_MISCTL__GIO9;
            break;
    }

    gup_bit_write(i2c_addr, 1, 0);
    
    return 0;
}

void gup_output_pulse(int t)
{
    unsigned long flags;
    //s32 i;
    
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
    udelay(10);
    
    local_irq_save(flags);

    mt_set_gpio_out(GTP_INT_PORT, 1);
    udelay(50);
    mt_set_gpio_out(GTP_INT_PORT, 0);
    udelay(t - 50);
    mt_set_gpio_out(GTP_INT_PORT, 1);

    local_irq_restore(flags);

    udelay(20);
    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
}

static void gup_sys_clk_init(void)
{
    u8 buf;
    
    //_fRW_MISCTL__RG_RXADC_CKMUX = 0;
    gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 5, 0);
    //_bRW_MISCTL__RG_LDO_A18_PWD = 0; //DrvMISCTL_A18_PowerON
    buf = 0;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_LDO_A18_PWD, &buf, 1);
    //_bRW_MISCTL__RG_BG_PWD = 0; //DrvMISCTL_BG_PowerON
    buf = 0;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_BG_PWD, &buf, 1);
    //_bRW_MISCTL__RG_CLKGEN_PWD = 0; //DrvMISCTL_CLKGEN_PowerON
    buf = 0;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_CLKGEN_PWD, &buf, 1);
    //_fRW_MISCTL__RG_RXADC_PWD = 0; //DrvMISCTL_RX_ADC_PowerON
    gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 0, 0);
    //_fRW_MISCTL__RG_RXADC_REF_PWD = 0; //DrvMISCTL_RX_ADCREF_PowerON
    gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 1, 0);
    //gup_clk_dac_setting(60);
    //_bRW_MISCTL__OSC_CK_SEL = 1;;
    buf = 1;
    i2c_write_bytes(i2c_client_point, _bRW_MISCTL__OSC_CK_SEL, &buf, 1);
}

u8 gup_clk_calibration(void)
{
    //u8 buf;
    //u8 trigger;
    s32 i;
    struct timeval start, end;
    s32 count;
    s32 count_ref;
    s32 sec;
    s32 usec;
    s32 ret = 0;
    //unsigned long flags;

    //buf = 0x0C; // hold ss51 and dsp
    //i2c_write_bytes(i2c_client_point, _rRW_MISCTL__SWRST_B0_, &buf, 1);
    ret = gup_hold_ss51_dsp(i2c_client_point);
    if (ret <= 0)
    {
        GTP_ERROR("[gup_clk_calibration]hold ss51 & dsp failed.");
        return FAIL;
    }

    //_fRW_MISCTL__CLK_BIAS = 0; //disable clock bias
    gup_bit_write(_rRW_MISCTL_RG_DMY83, 7, 0);

    //_fRW_MISCTL__GIO1_PU = 0; //set TOUCH INT PIN MODE as input
    gup_bit_write(_rRW_MISCTL__GIO1CTL_B2_, 0, 0);

    //_fRW_MISCTL__GIO1_OE = 0; //set TOUCH INT PIN MODE as input
    gup_bit_write(_rRW_MISCTL__GIO1CTL_B1_, 1, 0);

    //buf = 0x00;
    //i2c_write_bytes(i2c_client_point, _rRW_MISCTL__SWRST_B0_, &buf, 1);
    //msleep(1000);

    GTP_INFO("CLK calibration GO");
    gup_sys_clk_init();
    gup_clk_calibration_pin_select(1);//use GIO1 to do the calibration

    GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
 
    for (i = INIT_CLK_DAC; i < MAX_CLK_DAC; i++)
    {
        if (tpd_halt)
        {
            i = 80;     // if sleeping while calibrating main clock, set it default 80
            break;
        }
        GTP_INFO("CLK calibration DAC %d", i);

        gup_clk_dac_setting(i);
        gup_clk_count_init(1, CLK_AVG_TIME);

    #if 0
        gup_output_pulse(PULSE_LENGTH);
        count = gup_clk_count_get();
  
        if (count > PULSE_LENGTH * 60)//60= 60Mhz * 1us
        {
            break;
        }
        
    #else
        GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
        
        //local_irq_save(flags);
        do_gettimeofday(&start);
        mt_set_gpio_out(GTP_INT_PORT, 1);
        //local_irq_restore(flags);
        
        msleep(1);
        mt_set_gpio_out(GTP_INT_PORT, 0);
        msleep(1);
        
        //local_irq_save(flags);
        do_gettimeofday(&end);
        mt_set_gpio_out(GTP_INT_PORT, 1);
        //local_irq_restore(flags);
        
        count = gup_clk_count_get();
        udelay(20);
        GTP_GPIO_OUTPUT(GTP_INT_PORT, 0);
        
        usec = end.tv_usec - start.tv_usec;
        sec = end.tv_sec - start.tv_sec;
        count_ref = 60 * (usec+ sec * MILLION);//60= 60Mhz * 1us
        
        GTP_DEBUG("== time %d, %d, %d", sec, usec, count_ref);
        
        if (count > count_ref)
        {
            GTP_DEBUG("== count_diff %d", count - count_ref);
            break;
        }

    #endif
    }

    //clk_dac = i;

    //gtp_reset_guitar(i2c_client_point, 20);

#if 0//for debug
    //-- ouput clk to GPIO 4
    buf = 0x00;
    i2c_write_bytes(i2c_client_point, 0x41FA, &buf, 1);
    buf = 0x00;
    i2c_write_bytes(i2c_client_point, 0x4104, &buf, 1);
    buf = 0x00;
    i2c_write_bytes(i2c_client_point, 0x4105, &buf, 1);
    buf = 0x00;
    i2c_write_bytes(i2c_client_point, 0x4106, &buf, 1);
    buf = 0x01;
    i2c_write_bytes(i2c_client_point, 0x4107, &buf, 1);
    buf = 0x06;
    i2c_write_bytes(i2c_client_point, 0x41F8, &buf, 1);
    buf = 0x02;
    i2c_write_bytes(i2c_client_point, 0x41F9, &buf, 1);
#endif

    GTP_GPIO_AS_INT(GTP_INT_PORT);
    return i;
}


#define BANK_LENGTH (16*1024)
#define FIRMWARE_HEADER_LEN 14

static u32 current_system_length = 0;

char * gup_load_fw_from_file(char *filepath)
{
    struct file *fw_file = NULL;
	mm_segment_t old_fs;
	long len = 0;
    char *buffer = NULL;

	if(filepath == NULL)
	{
		GTP_ERROR("[Load_firmware]filepath: NULL");
		goto load_firmware_exit;
	}
		
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    
    fw_file = filp_open(filepath, O_RDONLY, 0);
    if (fw_file == NULL || IS_ERR(fw_file))
    {
        GTP_ERROR("[Load_firmware]Failed to open: %s", filepath);
		goto load_firmware_exit;
    }
    fw_file->f_op->llseek(fw_file, 0, SEEK_SET);
    len = fw_file->f_op->llseek(fw_file, 0, SEEK_END);
    if(len<1024)
    {
        GTP_ERROR("[Load_firmware]Firmware is too short: %ld", len);
        goto load_firmware_failed;
    }
    buffer = (char *)kmalloc(sizeof(char) * len, GFP_KERNEL);
    if(buffer == NULL)
    {
        GTP_ERROR("[Load_firmware]Failed to allocate buffer: %ld", len);
        goto load_firmware_failed;
    }
    
    memset(buffer, 0, sizeof(char) * len);
    
    fw_file->f_op->llseek(fw_file, 0, SEEK_SET);
    fw_file->f_op->read(fw_file, buffer, len, &fw_file->f_pos);

    GTP_ERROR("[Load_firmware]Load from file success: %ld %s", len, filepath);


load_firmware_failed:
    filp_close(fw_file, NULL);
    set_fs(old_fs);

load_firmware_exit:
    return buffer;
}

s32 gup_check_firmware(char *fw, u32 length)
{
    u32 i = 0;
    u32 fw_checksum = 0;
    u16 temp;
    
    for(i=0; i<length; i+=2)
    {
        temp = (fw[i]<<8) + fw[i+1];
        fw_checksum += temp;
    }
    
    GTP_DEBUG("firmware checksum: %4X", fw_checksum&0xFFFF);
    if(fw_checksum&0xFFFF)
    {
        return FAIL;
    }
    return SUCCESS;
}

s32 gup_enter_update_mode_noreset(void)
{
    s32 ret = FAIL;
	
	// disable watchdog
    ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__TMR0_EN, 0x00);
    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]disable wdt fail.");
        return FAIL;
    }	
    //select addr & hold ss51_dsp
    ret = gup_hold_ss51_dsp(i2c_client_point);
    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]hold ss51 & dsp failed.");
        return FAIL;
    }
    
    //clear control flag
    ret = gup_set_ic_msg(i2c_client_point, _rRW_MISCTL__BOOT_CTL_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]clear control flag fail.");
        return FAIL;
    }
    
    //set scramble
    ret = gup_set_ic_msg(i2c_client_point, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]set scramble fail.");
        return FAIL;
    }
    
    //enable accessing code
    ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__MEM_CD_EN, 0x01);

    if (ret <= 0)
    {
        GTP_ERROR("[enter_update_mode]enable accessing code fail.");
        return FAIL;
    }
    
    return SUCCESS;
}

s32 gup_load_by_bank(u8 bank, u8 need_check, u8 *fw, u32 length)
{
    s32 ret = FAIL;
    s32 retry = 0;

    GTP_DEBUG("[load_by_bank]begin download [bank:%d,length:%d].",bank,length);    
    while(retry++ < 5)
    {
        ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__SRAM_BANK, bank);    
        if (ret <= 0)
        {
            GTP_ERROR("[load_by_bank]select bank fail.");
            continue;
        }
            
        ret = i2c_write_bytes(i2c_client_point, 0xC000,fw, length);
        if (ret == -1)
        {
            GTP_ERROR("[load_by_bank]download bank fail.");
            continue;
        }    
        
		if(need_check)
		{
			ret = gup_check_and_repair(i2c_client_point, 0xC000, fw, length);
			if(FAIL == ret)
			{
				GTP_ERROR("[load_by_bank]checked FW bank fail.");
				continue;
			}
		}
        break;
    }
    if(retry<5)
    {
        return SUCCESS;
    }
    else
    {
        return FAIL;
    }
}

s32 gup_load_system(char *firmware, s32 length, u8 need_check)
{
    s32 ret = -1;
    u8 bank = 0;

#ifdef CONFIG_OF_TOUCH
	disable_irq(touch_irq);
#else
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

#if GTP_ESD_PROTECT
    gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
    ret = gup_enter_update_mode_noreset();
    if (FAIL == ret)
    {
        goto gup_load_system_exit;
    }
	GTP_DEBUG("enter update mode success.");
    while(length > 0)
    {
        u32 len = length >BANK_LENGTH? BANK_LENGTH:length;
        ret = gup_load_by_bank(bank, need_check, &firmware[bank*BANK_LENGTH], len);
        if(FAIL == ret)
        {
            goto gup_load_system_exit;
        }
		GTP_DEBUG("load bank%d  length:%d  success.", bank, len);
        bank++;
        length -= len;
    }
    
    ret = gtp_fw_startup(i2c_client_point);

gup_load_system_exit:
#if GTP_ESD_PROTECT
#if FLASHLESS_FLASH_WORKROUND

#else
    gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
#endif

#ifdef CONFIG_OF_TOUCH
	enable_irq(touch_irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif 

    return ret;    
}

s32 gup_load_fx_system(void)
{
    s32 ret = -1;
    char *firmware = NULL;
    u32 is_load_from_file = 0;
    u32 length = 0;

	GTP_INFO("[load_fx_system] Load authorization system.");	

	firmware = gtp_default_FW_hotknot2;
	
#if GTP_AUTO_UPDATE    
    firmware = gup_load_fw_from_file( FX_SYSTEM_PATH );
    if(firmware == NULL)
    {
        firmware = gtp_default_FW_hotknot2;
		GTP_INFO("[load_fx_system] Use default firmware.");			
    }
	else
	{
	    is_load_from_file = 1;
	}
#endif

    length = firmware[0]*256*256*256+
             firmware[1]*256*256+
             firmware[2]*256+
             firmware[3];

	if(length > 32*1024 || length < 4*1024 )
	{
		GTP_ERROR("[load_fx_system]firmware's length is invalid.");
		goto load_fx_exit;
	}
			 
    ret = gup_check_firmware(&firmware[FIRMWARE_HEADER_LEN],length);
    if(ret == FAIL)
    {
        GTP_ERROR("[load_fx_system]firmware's checksum is error.");
        goto load_fx_exit;
    }

    current_system_length = length;
    
    ret = gup_load_system(&firmware[FIRMWARE_HEADER_LEN], length, 1);
    
load_fx_exit:

    if(is_load_from_file)
    {
        kfree(firmware);
    }
    
    return ret;
}


s32 gup_load_hotknot_system(void)
{
    s32 ret = -1;
    char *firmware = NULL;
    u32 is_load_from_file = 0;
    u32 length = 0;
    
	GTP_INFO("[load_hotknot_system] Load hotknot system.");
	load_fw_process = 1;

    firmware = gtp_default_FW_hotknot;
	
#if GTP_AUTO_UPDATE  	
    firmware = gup_load_fw_from_file( HOTKNOT_SYSTEM_PATH );
    if(firmware == NULL)
    {
        firmware = gtp_default_FW_hotknot;
		GTP_INFO("[load_hotknot_system] Use default firmware.");
    }
	else
	{
        is_load_from_file = 1;	
	}
#endif

    length = firmware[0]*256*256*256+
             firmware[1]*256*256+
             firmware[2]*256+
             firmware[3];
	if(length > 32*1024 || length < 4*1024 )
	{
		GTP_ERROR("[load_hotknot_system]firmware's length is invalid.");
		goto load_hotknot_exit;
	}

    ret = gup_check_firmware(&firmware[FIRMWARE_HEADER_LEN],length);
    if(ret == FAIL)
    {
        GTP_ERROR("[load_hotknot_system]firmware's checksum is error.");
        goto load_hotknot_exit;
    }

    current_system_length = length;

    ret = gup_load_system(&firmware[FIRMWARE_HEADER_LEN], length, 0);
    
load_hotknot_exit:

    if(is_load_from_file)
    {
        kfree(firmware);
    }
    
    return ret;

}


s32 gup_recovery_main_system(void)
{
    s32 ret = -1;
    char *firmware = NULL;
    u32 is_load_from_file = 0;
    u32 length = 0;
    
	GTP_INFO("[recovery_main_system] Recovery main system.");
    
    if(gtp_chip_type == CHIP_TYPE_GT9)
    {
   /*lenovo-sw xuwen1 modify 20140805 for HotKnot package flow begin*/
                load_fw_process = 0;
        gtp_reset_guitar(i2c_client_point,10);
   /*lenovo-sw xuwen1 modify 20140805 for HotKnot package flow end*/
        return SUCCESS;
    }
	firmware = gtp_default_FW_fl;
	
#if GTP_AUTO_UPDATE
    firmware = gup_load_fw_from_file(MAIN_SYSTEM_PATH);
    if(firmware == NULL)
    {
        firmware = gtp_default_FW_fl;
		GTP_INFO("[recovery_main_system]Use default firmware.");
    }
	else
	{
	    is_load_from_file = 1;
	}
#endif

	if(firmware[0]==0x00&&firmware[1]==0x90&&firmware[2]==0x06&&firmware[3]==0x00)
	{
		length = 36*1024;
	}
	else
	{
		length = firmware[0]*256*256*256+
				 firmware[1]*256*256+
				 firmware[2]*256+
				 firmware[3];
	}
	if(length > 36*1024 || length < 16*1024 )
	{
		GTP_ERROR("[recovery_main_system]firmware's length is invalid.");
		goto recovery_main_system_exit;
	}
	
    ret = gup_check_firmware(&firmware[FIRMWARE_HEADER_LEN],length);
    if(ret == FAIL)
    {
        GTP_ERROR("[recovery_main_system]firmware's checksum is error.");
        goto recovery_main_system_exit;
    }

    if(current_system_length == 0)
    {
        current_system_length = length;
    }
	
	GTP_INFO("[recovery_main_system] Recovery length: %d.", current_system_length);
	
    ret = gup_load_system(&firmware[FIRMWARE_HEADER_LEN], current_system_length, 0);
	
#if (FLASHLESS_FLASH_WORKROUND && GTP_ESD_PROTECT)
    gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif 
  
recovery_main_system_exit:

    if(is_load_from_file)
    {
        kfree(firmware);
    }

	load_fw_process = 0;
    return ret;

}

s32 gup_load_main_system(char *filepath)
{
    s32 ret = -1;
    char *firmware = NULL;
    u32 is_load_from_file = 0;
    u32 length = 0;
    
	GTP_INFO("[load_main_system] Load main system.");

	if(filepath != NULL)
	{
		firmware = gup_load_fw_from_file(filepath);
		if(firmware == NULL)
		{
			GTP_INFO("[load_main_system]can not open file: %s", filepath);
			goto load_main_system_exit;
		}
		else
		{
			is_load_from_file = 1;
		}
	}
	else
	{
		firmware = gtp_default_FW_fl;
	}
#if GTP_AUTO_UPDATE  
    firmware = gup_load_fw_from_file(filepath);
    if(firmware == NULL)
    {
        if(gtp_chip_type == CHIP_TYPE_GT9)
			firmware = gtp_default_FW;
		else
        	firmware = gtp_default_FW_fl;
		GTP_INFO("[load_main_system]Use default firmware. type:%d", gtp_chip_type);
    }
	else
	{
	    is_load_from_file = 1;
	}
#endif
	
	if(firmware[0]==0x00&&firmware[1]==0x90&&firmware[2]==0x06&&firmware[3]==0x00)
	{
		length = 36*1024;
	}
	else
	{
		length = firmware[0]*256*256*256+
				 firmware[1]*256*256+
				 firmware[2]*256+
				 firmware[3];
		if(length > 36*1024)
		{
			length = 36*1024;
		}
	}
    ret = gup_check_firmware(&firmware[FIRMWARE_HEADER_LEN],length);
    if(ret == FAIL)
    {
        GTP_ERROR("[load_main_system]firmware's checksum is error.");
        goto load_main_system_exit;
    }
    GTP_INFO("[load_main_system] Firmware length: %d.", length);
    
	//memcpy(gtp_default_FW_fl, firmware, length);
	
	ret = gup_load_system(&firmware[FIRMWARE_HEADER_LEN], length, 1);
    show_len = 100;

#if (FLASHLESS_FLASH_WORKROUND && GTP_ESD_PROTECT)
    gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif 

load_main_system_exit:

    if(is_load_from_file)
    {
        kfree(firmware);
    }

    return ret;
}
#endif 
//*************** For GT9XXF End ***********************//
