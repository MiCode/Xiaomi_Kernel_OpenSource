/**************************************************************************
*  aw87329_audio.c
*
*  author : AWINIC Technology CO., LTD
*
*  Version: v1.1.1
*
*  Date: 2018/03/06
**************************************************************************/

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include "msm-cdc-pinctrl.h"

/*******************************************************************************
 * aw87329 marco
 ******************************************************************************/
#define AW87329_I2C_NAME    "aw87329_pa"

#define AW87329_DRIVER_VERSION  "v1.1.1"

#define AWINIC_CFG_UPDATE_DELAY

#define AW_I2C_RETRIES 5
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2

#define aw87329_REG_CHIPID      0x00
#define aw87329_REG_SYSCTRL     0x01
#define aw87329_REG_MODECTRL    0x02
#define aw87329_REG_CPOVP       0x03
#define aw87329_REG_CPP         0x04
#define aw87329_REG_GAIN        0x05
#define aw87329_REG_AGC3_PO     0x06
#define aw87329_REG_AGC3        0x07
#define aw87329_REG_AGC2_PO     0x08
#define aw87329_REG_AGC2        0x09
#define aw87329_REG_AGC1        0x0A

#define aw87329_CHIP_DISABLE    0x08

#define REG_NONE_ACCESS         0
#define REG_RD_ACCESS           1 << 0
#define REG_WR_ACCESS           1 << 1
#define aw87329_REG_MAX         0x0F

const unsigned char aw87329_reg_access[aw87329_REG_MAX] = {
    [aw87329_REG_CHIPID  ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_SYSCTRL ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_MODECTRL] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_CPOVP   ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_CPP     ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_GAIN    ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_AGC3_PO ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_AGC3    ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_AGC2_PO ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_AGC2    ] = REG_RD_ACCESS|REG_WR_ACCESS,
    [aw87329_REG_AGC1    ] = REG_RD_ACCESS|REG_WR_ACCESS,
};


/*******************************************************************************
 * aw87329 functions
 ******************************************************************************/
unsigned char aw87329_audio_off(void);
unsigned char aw87329_audio_kspk(void);
unsigned char aw87329_audio_drcv(void);
unsigned char aw87329_audio_abrcv(void);


/*******************************************************************************
 * aw87329 variable
 ******************************************************************************/
struct aw87329_t{
    struct i2c_client *i2c_client;
    int reset_gpio;
    unsigned char init_flag;
    unsigned char hwen_flag;
    unsigned char kspk_cfg_update_flag;
    unsigned char drcv_cfg_update_flag;
    unsigned char abrcv_cfg_update_flag;
    struct hrtimer cfg_timer;
    struct work_struct cfg_work;
};
struct aw87329_t *aw87329 = NULL;

struct aw87329_container{
    int len;
    unsigned char data[];
};
struct aw87329_container *aw87329_kspk_cnt;
struct aw87329_container *aw87329_drcv_cnt;
struct aw87329_container *aw87329_abrcv_cnt;

static char *aw87329_kspk_name = "aw87329_kspk.bin";
static char *aw87329_drcv_name = "aw87329_drcv.bin";
static char *aw87329_abrcv_name = "aw87329_abrcv.bin";

static unsigned char aw87329_kspk_cfg_default[] = {
0x39, 0x0E, 0xA3, 0x06, 0x05, 0x10, 0x07, 0x52, 0x06, 0x08, 0x96
};
static unsigned char aw87329_drcv_cfg_default[] = {
0x39, 0x0A, 0xAB, 0x06, 0x05, 0x00, 0x0f, 0x52, 0x09, 0x08, 0x97
};
static unsigned char aw87329_abrcv_cfg_default[] = {
0x39, 0x0A, 0xAF, 0x06, 0x05, 0x00, 0x0f, 0x52, 0x09, 0x08, 0x97
};


/*******************************************************************************
 * i2c write and read
 ******************************************************************************/
static int i2c_write_reg(unsigned char reg_addr, unsigned char reg_data)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_write_byte_data(aw87329->i2c_client, reg_addr, reg_data);
        if(ret < 0) {
            pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

static unsigned char i2c_read_reg(unsigned char reg_addr)
{
    int ret = -1;
    unsigned char cnt = 0;

    while(cnt < AW_I2C_RETRIES) {
        ret = i2c_smbus_read_byte_data(aw87329->i2c_client, reg_addr);
        if(ret < 0) {
            pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, ret);
        } else {
            break;
        }
        cnt ++;
        msleep(AW_I2C_RETRY_DELAY);
    }

    return ret;
}

/*******************************************************************************
 * aw87329 hardware control
 ******************************************************************************/
unsigned char aw87329_hw_on(void)
{
    pr_err("%s enter %d\n", __func__,aw87329->reset_gpio);

    if (aw87329 && gpio_is_valid(aw87329->reset_gpio)) {
        gpio_set_value_cansleep(aw87329->reset_gpio, 0);
        msleep(2);
        gpio_set_value_cansleep(aw87329->reset_gpio, 1);
        msleep(2);
        aw87329->hwen_flag = 1;
    } else {
        dev_err(&aw87329->i2c_client->dev, "%s:  failed\n", __func__);
    }

    return 0;
}

unsigned char aw87329_hw_off(void)
{
    pr_info("%s enter\n", __func__);

    if (aw87329 && gpio_is_valid(aw87329->reset_gpio)) {
        gpio_set_value_cansleep(aw87329->reset_gpio, 0);
        msleep(2);
        aw87329->hwen_flag = 0;
    } else {
        dev_err(&aw87329->i2c_client->dev, "%s:  failed\n", __func__);
    }
    return 0;
}
EXPORT_SYMBOL(aw87329_hw_off);

/*******************************************************************************
 * aw87329 control interface
 ******************************************************************************/
unsigned char aw87329_kspk_reg_val(unsigned char reg)
{
    if(aw87329->kspk_cfg_update_flag) {
        return *(aw87329_kspk_cnt->data+reg);
    } else {
        return aw87329_kspk_cfg_default[reg];
    }
}

unsigned char aw87329_drcv_reg_val(unsigned char reg)
{
    if(aw87329->drcv_cfg_update_flag) {
        return *(aw87329_drcv_cnt->data+reg);
    } else {
        return aw87329_drcv_cfg_default[reg];
    }
}

unsigned char aw87329_abrcv_reg_val(unsigned char reg)
{
    if(aw87329->abrcv_cfg_update_flag) {
        return *(aw87329_abrcv_cnt->data+reg);
    } else {
        return aw87329_abrcv_cfg_default[reg];
    }
}

unsigned char aw87329_audio_kspk(void)
{
   pr_err("%s: enter aw87329_audio_kspk\n", __func__);
    if(aw87329 == NULL) {
        pr_err("%s: aw87329 is NULL\n", __func__);
        return 1;
    }

    if(!aw87329->init_flag) {
        pr_err("%s: aw87329 init failed\n", __func__);
        return 1;
    }

    if(!aw87329->hwen_flag) {
        aw87329_hw_on();
    }

    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_kspk_reg_val(aw87329_REG_SYSCTRL )&0xF7);
    i2c_write_reg(aw87329_REG_MODECTRL, aw87329_kspk_reg_val(aw87329_REG_MODECTRL));
    i2c_write_reg(aw87329_REG_CPOVP   , aw87329_kspk_reg_val(aw87329_REG_CPOVP   ));
    i2c_write_reg(aw87329_REG_CPP     , aw87329_kspk_reg_val(aw87329_REG_CPP     ));
    i2c_write_reg(aw87329_REG_GAIN    , aw87329_kspk_reg_val(aw87329_REG_GAIN    ));
    i2c_write_reg(aw87329_REG_AGC3_PO , aw87329_kspk_reg_val(aw87329_REG_AGC3_PO ));
    i2c_write_reg(aw87329_REG_AGC3    , aw87329_kspk_reg_val(aw87329_REG_AGC3    ));
    i2c_write_reg(aw87329_REG_AGC2_PO , aw87329_kspk_reg_val(aw87329_REG_AGC2_PO ));
    i2c_write_reg(aw87329_REG_AGC2    , aw87329_kspk_reg_val(aw87329_REG_AGC2    ));
    i2c_write_reg(aw87329_REG_AGC1    , aw87329_kspk_reg_val(aw87329_REG_AGC1    ));
    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_kspk_reg_val(aw87329_REG_SYSCTRL ));

    return 0;
}
EXPORT_SYMBOL(aw87329_audio_kspk);

unsigned char aw87329_audio_drcv(void)
{
    if(aw87329 == NULL) {
        pr_err("%s: aw87329 is NULL\n", __func__);
        return 1;
    }

    if(!aw87329->init_flag) {
        pr_err("%s: aw87329 init failed\n", __func__);
        return 1;
    }

    if(!aw87329->hwen_flag) {
        aw87329_hw_on();
    }

    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_drcv_reg_val(aw87329_REG_SYSCTRL )&0xF7);
    i2c_write_reg(aw87329_REG_MODECTRL, aw87329_drcv_reg_val(aw87329_REG_MODECTRL));
    i2c_write_reg(aw87329_REG_CPOVP   , aw87329_drcv_reg_val(aw87329_REG_CPOVP   ));
    i2c_write_reg(aw87329_REG_CPP     , aw87329_drcv_reg_val(aw87329_REG_CPP     ));
    i2c_write_reg(aw87329_REG_GAIN    , aw87329_drcv_reg_val(aw87329_REG_GAIN    ));
    i2c_write_reg(aw87329_REG_AGC3_PO , aw87329_drcv_reg_val(aw87329_REG_AGC3_PO ));
    i2c_write_reg(aw87329_REG_AGC3    , aw87329_drcv_reg_val(aw87329_REG_AGC3    ));
    i2c_write_reg(aw87329_REG_AGC2_PO , aw87329_drcv_reg_val(aw87329_REG_AGC2_PO ));
    i2c_write_reg(aw87329_REG_AGC2    , aw87329_drcv_reg_val(aw87329_REG_AGC2    ));
    i2c_write_reg(aw87329_REG_AGC1    , aw87329_drcv_reg_val(aw87329_REG_AGC1    ));
    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_drcv_reg_val(aw87329_REG_SYSCTRL ));

    return 0;
}
EXPORT_SYMBOL(aw87329_audio_drcv);

unsigned char aw87329_audio_abrcv(void)
{
    if(aw87329 == NULL) {
        pr_err("%s: aw87329 is NULL\n", __func__);
        return 1;
    }

    if(!aw87329->init_flag) {
        pr_err("%s: aw87329 init failed\n", __func__);
        return 1;
    }

    if(!aw87329->hwen_flag) {
        aw87329_hw_on();
    }

    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_abrcv_reg_val(aw87329_REG_SYSCTRL )&0xF7);
    i2c_write_reg(aw87329_REG_MODECTRL, aw87329_abrcv_reg_val(aw87329_REG_MODECTRL));
    i2c_write_reg(aw87329_REG_CPOVP   , aw87329_abrcv_reg_val(aw87329_REG_CPOVP   ));
    i2c_write_reg(aw87329_REG_CPP     , aw87329_abrcv_reg_val(aw87329_REG_CPP     ));
    i2c_write_reg(aw87329_REG_GAIN    , aw87329_abrcv_reg_val(aw87329_REG_GAIN    ));
    i2c_write_reg(aw87329_REG_AGC3_PO , aw87329_abrcv_reg_val(aw87329_REG_AGC3_PO ));
    i2c_write_reg(aw87329_REG_AGC3    , aw87329_abrcv_reg_val(aw87329_REG_AGC3    ));
    i2c_write_reg(aw87329_REG_AGC2_PO , aw87329_abrcv_reg_val(aw87329_REG_AGC2_PO ));
    i2c_write_reg(aw87329_REG_AGC2    , aw87329_abrcv_reg_val(aw87329_REG_AGC2    ));
    i2c_write_reg(aw87329_REG_AGC1    , aw87329_abrcv_reg_val(aw87329_REG_AGC1    ));
    i2c_write_reg(aw87329_REG_SYSCTRL , aw87329_abrcv_reg_val(aw87329_REG_SYSCTRL ));

    return 0;
}

unsigned char aw87329_audio_off(void)
{
    if(aw87329 == NULL) {
        pr_err("%s: aw87329 is NULL\n", __func__);
        return 1;
    }

    if(!aw87329->init_flag) {
        pr_err("%s: aw87329 init failed\n", __func__);
        return 1;
    }

    if(aw87329->hwen_flag) {
        i2c_write_reg(aw87329_REG_SYSCTRL, aw87329_CHIP_DISABLE);
    }
    aw87329_hw_off();

    return 0;
}
EXPORT_SYMBOL(aw87329_audio_off);

/*******************************************************************************
 * aw87329 firmware cfg update
 ******************************************************************************/
static void aw87329_abrcv_cfg_loaded(const struct firmware *cont, void *context)
{
    unsigned int i;

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw87329_abrcv_name);
        release_firmware(cont);
        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw87329_abrcv_name,
                    cont ? cont->size : 0);

    for(i=0; i<cont->size; i++) {
        pr_info("%s: cont: addr:0x%02x, data:0x%02x\n", __func__, i, *(cont->data+i));
    }

    aw87329_abrcv_cnt = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
    if (!aw87329_abrcv_cnt) {
        release_firmware(cont);
        pr_err("%s: error allocating memory\n", __func__);
        return;
    }
    aw87329_abrcv_cnt->len = cont->size;
    memcpy(aw87329_abrcv_cnt->data, cont->data, cont->size);
    release_firmware(cont);

    for(i=0; i<aw87329_abrcv_cnt->len; i++) {
        pr_info("%s: rcv_cnt: addr:0x%02x, data:0x%02x\n",
                __func__, i, aw87329_abrcv_reg_val(i));
    }

    aw87329->abrcv_cfg_update_flag = 1;
}


static void aw87329_drcv_cfg_loaded(const struct firmware *cont, void *context)
{
    unsigned int i;
    int ret;

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw87329_drcv_name);
        release_firmware(cont);
        ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_abrcv_name,
                &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_abrcv_cfg_loaded);
        if(ret) {
            aw87329->abrcv_cfg_update_flag = 0;
            pr_err("%s: request_firmware_nowait failed with read %s",
                    __func__, aw87329_abrcv_name);
        }
        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw87329_drcv_name,
                    cont ? cont->size : 0);

    for(i=0; i<cont->size; i++) {
        pr_info("%s: cont: addr:0x%02x, data:0x%02x\n", __func__, i, *(cont->data+i));
    }

    aw87329_drcv_cnt = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
    if (!aw87329_drcv_cnt) {
        release_firmware(cont);
        pr_err("%s: error allocating memory\n", __func__);
        return;
    }
    aw87329_drcv_cnt->len = cont->size;
    memcpy(aw87329_drcv_cnt->data, cont->data, cont->size);
    release_firmware(cont);

    for(i=0; i<aw87329_drcv_cnt->len; i++) {
        pr_info("%s: rcv_cnt: addr:0x%02x, data:0x%02x\n",
                __func__, i, aw87329_drcv_reg_val(i));
    }

    aw87329->drcv_cfg_update_flag = 1;

    ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_abrcv_name,
            &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_abrcv_cfg_loaded);
    if(ret) {
        aw87329->abrcv_cfg_update_flag = 0;
        pr_err("%s: request_firmware_nowait failed with read %s",
                __func__, aw87329_abrcv_name);
    }
}

static void aw87329_kspk_cfg_loaded(const struct firmware *cont, void *context)
{
    unsigned int i;
    int ret;

    if (!cont) {
        pr_err("%s: failed to read %s\n", __func__, aw87329_kspk_name);
        release_firmware(cont);
        ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_drcv_name,
                &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_drcv_cfg_loaded);
        if(ret) {
            aw87329->drcv_cfg_update_flag = 0;
            pr_err("%s: request_firmware_nowait failed with read %s",
                    __func__, aw87329_drcv_name);
        }
        return;
    }

    pr_info("%s: loaded %s - size: %zu\n", __func__, aw87329_kspk_name,
                    cont ? cont->size : 0);

    for(i=0; i<cont->size; i++) {
        pr_info("%s: cont: addr:0x%02x, data:0x%02x\n",
                __func__, i, *(cont->data+i));
    }

    aw87329_kspk_cnt = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
    if (!aw87329_kspk_cnt) {
        release_firmware(cont);
        pr_err("%s: error allocating memory\n", __func__);
        return;
    }
    aw87329_kspk_cnt->len = cont->size;
    memcpy(aw87329_kspk_cnt->data, cont->data, cont->size);
    release_firmware(cont);

    for(i=0; i<aw87329_kspk_cnt->len; i++) {
        pr_info("%s: spk_cnt: addr:0x%02x, data:0x%02x\n",
                __func__, i, aw87329_kspk_reg_val(i));
    }

    aw87329->kspk_cfg_update_flag = 1;

    ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_drcv_name,
            &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_drcv_cfg_loaded);
    if(ret) {
        aw87329->drcv_cfg_update_flag = 0;
        pr_err("%s: request_firmware_nowait failed with read %s",
                __func__, aw87329_drcv_name);
    }
}

#ifdef AWINIC_CFG_UPDATE_DELAY
static enum hrtimer_restart cfg_timer_func(struct hrtimer *timer)
{
    pr_info("%s enter\n", __func__);

    schedule_work(&aw87329->cfg_work);

    return HRTIMER_NORESTART;
}

static void cfg_work_routine(struct work_struct *work)
{
    int ret = -1;

    pr_info("%s enter\n", __func__);

    ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_kspk_name,
            &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_kspk_cfg_loaded);
    if(ret) {
        pr_err("%s: request_firmware_nowait failed with read %s",
                __func__, aw87329_kspk_name);
    }

}
#endif

static int aw87329_cfg_init(void)
{
    int ret = -1;
#ifdef AWINIC_CFG_UPDATE_DELAY
    int cfg_timer_val = 5000;

    hrtimer_init(&aw87329->cfg_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    aw87329->cfg_timer.function = cfg_timer_func;
    INIT_WORK(&aw87329->cfg_work, cfg_work_routine);
    hrtimer_start(&aw87329->cfg_timer,
            ktime_set(cfg_timer_val/1000, (cfg_timer_val%1000)*1000000),
            HRTIMER_MODE_REL);
    ret = 0;
#else
    ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_kspk_name,
            &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_kspk_cfg_loaded);
    if(ret) {
        pr_err("%s: request_firmware_nowait failed with read %s",
                __func__, aw87329_kspk_name);
    }
#endif
    return ret;
}
/*******************************************************************************
 * aw87329 attribute
 ******************************************************************************/
static ssize_t aw87329_get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
    unsigned char reg_val;
    ssize_t len = 0;
    unsigned char i;
    for(i=0; i<aw87329_REG_MAX ;i++) {
        if(aw87329_reg_access[i] & REG_RD_ACCESS) {
            reg_val = i2c_read_reg(i);
            len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n", i,reg_val);
        }
    }

    return len;
}

static ssize_t aw87329_set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    unsigned int databuf[2];
    if(2 == sscanf(buf,"%x %x",&databuf[0], &databuf[1])) {
        i2c_write_reg(databuf[0],databuf[1]);
    }
    return len;
}


static ssize_t aw87329_get_hwen(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "hwen: %d\n", aw87329->hwen_flag);

    return len;
}

static ssize_t aw87329_set_hwen(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    unsigned int databuf[16];

    sscanf(buf,"%d",&databuf[0]);
    if(databuf[0] == 0) {        // OFF
        aw87329_hw_off();
    } else {                    // ON
        aw87329_hw_on();
    }

    return len;
}

static ssize_t aw87329_get_update(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;

    return len;
}

static ssize_t aw87329_set_update(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    unsigned int databuf[16];
    int ret;

    sscanf(buf,"%d",&databuf[0]);
    if(databuf[0] == 0) {
    } else {
        aw87329->kspk_cfg_update_flag = 0;
        aw87329->drcv_cfg_update_flag = 0;
        aw87329->abrcv_cfg_update_flag = 0;
        ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, aw87329_kspk_name,
                &aw87329->i2c_client->dev, GFP_KERNEL, NULL, aw87329_kspk_cfg_loaded);
        if(ret) {
            pr_err("%s: request_firmware_nowait failed with read %s",
                    __func__, aw87329_kspk_name);
        }
    }

    return len;
}

static ssize_t aw87329_get_mode(struct device* cd,struct device_attribute *attr, char* buf)
{
    ssize_t len = 0;

    len += snprintf(buf+len, PAGE_SIZE-len, "0: off mode\n");
    len += snprintf(buf+len, PAGE_SIZE-len, "1: kspk mode\n");
    len += snprintf(buf+len, PAGE_SIZE-len, "2: drcv mode\n");
    len += snprintf(buf+len, PAGE_SIZE-len, "3: abrcv mode\n");

    return len;
}

static ssize_t aw87329_set_mode(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    unsigned int databuf[16];

    sscanf(buf,"%d",&databuf[0]);
    if(databuf[0] == 0) {
        aw87329_audio_off();
    } else if(databuf[0] == 1) {
        aw87329_audio_kspk();
    } else if(databuf[0] == 2) {
        aw87329_audio_drcv();
    } else if(databuf[0] == 3) {
        aw87329_audio_abrcv();
    } else {
        aw87329_audio_off();
    }

    return len;
}

static DEVICE_ATTR(reg, 0660, aw87329_get_reg,  aw87329_set_reg);
static DEVICE_ATTR(hwen, 0660, aw87329_get_hwen,  aw87329_set_hwen);
static DEVICE_ATTR(update, 0660, aw87329_get_update,  aw87329_set_update);
static DEVICE_ATTR(mode, 0660, aw87329_get_mode,  aw87329_set_mode);

static struct attribute *aw87329_attributes[] = {
    &dev_attr_reg.attr,
    &dev_attr_hwen.attr,
    &dev_attr_update.attr,
    &dev_attr_mode.attr,
    NULL
};

static struct attribute_group aw87329_attribute_group = {
    .attrs = aw87329_attributes
};
struct cdc_pdm_pinctrl_info {
    struct pinctrl *pinctrl;
    struct pinctrl_state *reset_pa_act;
    struct pinctrl_state *reset_pa_sus;
};
static struct cdc_pdm_pinctrl_info pinctrl_info;

/*****************************************************
 * device tree
 *****************************************************/
static int aw87329_parse_dt(struct device *dev, struct device_node *np) {
    struct pinctrl *pinctrl;

    aw87329->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	dev_err(dev, "%s: gpio provided\n", __func__);
    if (aw87329->reset_gpio < 0) {
        dev_err(dev, "%s: no reset gpio provided\n", __func__);
        return -1;
    } else {
        dev_err(dev, "%s: reset gpio pinctrl\n", __func__);
        pinctrl = devm_pinctrl_get(dev);
        if (IS_ERR(pinctrl)) {
            pr_err("%s: Unable to get pinctrl handle\n", __func__);
            return -EINVAL;
        }
        pinctrl_info.pinctrl = pinctrl;
        /* get pinctrl handle for reset_pa_gpio */
        pinctrl_info.reset_pa_act = pinctrl_lookup_state(pinctrl, "reset_pa_act");
        if (IS_ERR(pinctrl_info.reset_pa_act)) {
            pr_err("%s: Unable to get pinctrl disable handle\n", __func__);
            return -EINVAL;
        }
        pinctrl_info.reset_pa_sus = pinctrl_lookup_state(pinctrl, "reset_pa_sus");
        if (IS_ERR(pinctrl_info.reset_pa_sus)) {
            pr_err("%s: Unable to get pinctrl active handle\n", __func__);
            return -EINVAL;
        }
    }
    return 0;
}

int aw87329_hw_reset(void)
{
    pr_info("%s enter\n", __func__);

    if (aw87329 && gpio_is_valid(aw87329->reset_gpio)) {
        gpio_set_value_cansleep(aw87329->reset_gpio, 0);
        msleep(2);
        gpio_set_value_cansleep(aw87329->reset_gpio, 1);
        msleep(2);
        aw87329->hwen_flag = 1;
    } else {
        aw87329->hwen_flag = 0;
        dev_err(&aw87329->i2c_client->dev, "%s:  failed\n", __func__);
    }
    return 0;
}

/*****************************************************
 * check chip id
 *****************************************************/
int aw87329_read_chipid(void)
{
    unsigned int cnt = 0;
    unsigned int reg = 0;

    while(cnt < AW_READ_CHIPID_RETRIES) {
        reg = i2c_read_reg(0x00);
        if(reg == 0x39) {
            pr_info("%s: aw87329 chipid=0x%x\n", __func__, reg);
            return 0;
        } else {
            pr_info("%s: aw87329 chipid=0x%x error\n", __func__, reg);
        }
        cnt ++;

        msleep(AW_READ_CHIPID_RETRY_DELAY);
    }

    return -EINVAL;
}



/*******************************************************************************
 * aw87329 i2c driver
 ******************************************************************************/
static int aw87329_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct device_node *np = client->dev.of_node;
    int ret = -1;
    pr_info("%s Enter\n", __func__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "%s: check_functionality failed\n", __func__);
        ret = -ENODEV;
        goto exit_check_functionality_failed;
    }

    aw87329 = devm_kzalloc(&client->dev, sizeof(struct aw87329_t), GFP_KERNEL);
    if (aw87329 == NULL) {
        ret = -ENOMEM;
        goto exit_devm_kzalloc_failed;
    }

    aw87329->i2c_client = client;
    i2c_set_clientdata(client, aw87329);

    /* aw87329 rst */
    if (np) {
        ret = aw87329_parse_dt(&client->dev, np);
        if (ret) {
            dev_err(&client->dev, "%s: failed to parse device tree node\n", __func__);
            goto exit_gpio_get_failed;
        }
    } else {
        aw87329->reset_gpio = -1;
    }

    if (gpio_is_valid(aw87329->reset_gpio)) {
        ret = devm_gpio_request_one(&client->dev, aw87329->reset_gpio,
            GPIOF_OUT_INIT_LOW, "aw87329_rst");
        if (ret){
            dev_err(&client->dev, "%s: rst request failed\n", __func__);
            goto exit_gpio_request_failed;
        }
    }

    /* hardware reset */
    aw87329_hw_reset();

    /* aw87329 chip id */
    ret = aw87329_read_chipid();
    if (ret < 0) {
        dev_err(&client->dev, "%s: aw87329_read_chipid failed ret=%d\n", __func__, ret);
        goto exit_i2c_check_id_failed;
    }

    ret = sysfs_create_group(&client->dev.kobj, &aw87329_attribute_group);
    if (ret < 0) {
        dev_info(&client->dev, "%s error creating sysfs attr files\n", __func__);
    }

    /* aw87329 cfg update */
    aw87329->kspk_cfg_update_flag = 0;
    aw87329->drcv_cfg_update_flag = 0;
    aw87329->abrcv_cfg_update_flag = 0;
    aw87329_cfg_init();


    /* aw87329 hardware off */
    aw87329_hw_off();

    aw87329->init_flag = 1;
 pr_info("%s ok Enter\n", __func__);
    return 0;

exit_i2c_check_id_failed:
    devm_gpio_free(&client->dev, aw87329->reset_gpio);
exit_gpio_request_failed:
exit_gpio_get_failed:
    devm_kfree(&client->dev, aw87329);
    aw87329 = NULL;
exit_devm_kzalloc_failed:
exit_check_functionality_failed:
    return ret;
}

static int aw87329_i2c_remove(struct i2c_client *client)
{
    if(gpio_is_valid(aw87329->reset_gpio)) {
        devm_gpio_free(&client->dev, aw87329->reset_gpio);
    }

    return 0;
}

static const struct i2c_device_id aw87329_i2c_id[] = {
    { AW87329_I2C_NAME, 0 },
    { }
};


static const struct of_device_id extpa_of_match[] = {
    {.compatible = "awinic,aw87329_pa"},
    {},
};


static struct i2c_driver aw87329_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = AW87329_I2C_NAME,
        .of_match_table = extpa_of_match,
    },
    .probe = aw87329_i2c_probe,
    .remove = aw87329_i2c_remove,
    .id_table    = aw87329_i2c_id,
};

static int __init aw87329_pa_init(void) {
    int ret;

    pr_info("%s enter\n", __func__);
    pr_info("%s: driver version: %s\n", __func__, AW87329_DRIVER_VERSION);

    ret = i2c_add_driver(&aw87329_i2c_driver);
    if (ret) {
        pr_info("****[%s] Unable to register driver (%d)\n",
                __func__, ret);
        return ret;
    }
    return 0;
}

static void __exit aw87329_pa_exit(void) {
    pr_info("%s enter\n", __func__);
    i2c_del_driver(&aw87329_i2c_driver);
}

module_init(aw87329_pa_init);
module_exit(aw87329_pa_exit);

MODULE_AUTHOR("<liweilei@awinic.com.cn>");
MODULE_DESCRIPTION("awinic aw87329 pa driver");
MODULE_LICENSE("GPL v2");
