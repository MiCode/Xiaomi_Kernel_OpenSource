/** ***************************************************************************
 * @file nano_timer.c
 *
 * @brief nanosic timer file  
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com 
 * */

/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "nano_macro.h"
#include <linux/interrupt.h>

static bool registered  = false;
static int gpio_reset_pin = -1;
static int gpio_status_pin = -1;
static int gpio_vdd_pin = -1;
static int gpio_irq_pin = -1;
static int gpio_sleep_pin = -1;
int gpio_hall_n_pin = -1;
int gpio_hall_s_pin = -1;
int g_wakeup_irqno  = -1;

bool g_panel_status = false;/*1:on, 0:off*/
#define SUPPORT_GPIO_SLEEP_FUNCTION

/** ************************************************************************//**
 * @brief   Nanosic_i2c_irq
 *          GPIO 唤醒中断处理程序 
 ** */
irqreturn_t
Nanosic_wakeup_irq(int irq, void *dev_id)
{
    if(g_panel_status == false){
        pm_wakeup_event(gI2c_client->dev,0);

        spin_lock(&gI2c_client->input_report_lock);

        input_report_key(gI2c_client->input_dev,KEY_WAKEUP,1);
        input_sync(gI2c_client->input_dev);
        input_report_key(gI2c_client->input_dev,KEY_WAKEUP,0);
        input_sync(gI2c_client->input_dev);

        spin_unlock(&gI2c_client->input_report_lock);
        dbgprint(ERROR_LEVEL,"Nanosic_wakeup_irq wakeup panel\n");
    }

	return IRQ_HANDLED;
}

/** ************************************************************************//**
 *  @func Nanosic_GPIO_register
 *  
 *  @brief gpio申请及配置
 *
 ** */
int
Nanosic_GPIO_register(int vdd_pin,int reset_pin, int status_pin, int irq_pin,int sleep_pin)
{
    int err;

    if(vdd_pin < 0 || reset_pin < 0 || status_pin < 0 || irq_pin < 0 ){
        dbgprint(ERROR_LEVEL,"invalid pin value\n");
        return -EFAULT;
    }

    gpio_reset_pin = reset_pin;
    gpio_status_pin = status_pin;
    gpio_vdd_pin = vdd_pin;
    gpio_irq_pin = irq_pin;
#ifdef SUPPORT_GPIO_SLEEP_FUNCTION
	gpio_sleep_pin = sleep_pin;
#endif
    dbgprint(ERROR_LEVEL,"Nanosic_GPIO_register :gpio_reset_pin:%d,gpio_status_pin:%d,gpio_vdd_pin:%d,gpio_irq_pin:%d\n",gpio_reset_pin,gpio_status_pin,gpio_vdd_pin,gpio_irq_pin);

    if(registered == true){
        dbgprint(ERROR_LEVEL,"need register first\n");
        return -EFAULT;
    }
    err = gpio_request(gpio_reset_pin,NULL);
    if(err){
        dbgprint(ERROR_LEVEL,"request reset gpio fail\n");
        return -EFAULT;
    }
    err = gpio_request(gpio_status_pin,NULL);
    if(err){
        dbgprint(ERROR_LEVEL,"request status gpio fail\n");
        goto _err1;
    }
    err = gpio_request(gpio_vdd_pin,NULL);
    if(err){
        dbgprint(ERROR_LEVEL,"request vdd gpio fail\n");
        goto _err2;
    }

    err = gpio_export(gpio_reset_pin,true);
    if(err)
        dbgprint(ERROR_LEVEL,"export reset gpio fail\n");
    err = gpio_export(gpio_status_pin,true);
    if(err)
        dbgprint(ERROR_LEVEL,"export status gpio fail\n");
    err = gpio_export(gpio_vdd_pin,true);
    if(err)
        dbgprint(ERROR_LEVEL,"export vdd gpio fail\n");

#ifdef SUPPORT_GPIO_SLEEP_FUNCTION
    err = gpio_request(gpio_sleep_pin,NULL);
    if(err){
        dbgprint(ERROR_LEVEL,"request sleep gpio fail\n");
        goto _err3;
    }
#endif

    if (gpio_is_valid(gpio_irq_pin)) {
        //err = gpio_request_one(gpio_irq_pin, GPIOF_IN, "8030_io_irq");
        err = gpio_request(gpio_irq_pin,NULL);
        if (err) {
            dbgprint(ERROR_LEVEL,"Failed to request gpio_irq_pin GPIO:%d\n",gpio_irq_pin);
            goto _err4;
        }
        err = gpio_direction_input(gpio_irq_pin);
        if(err){
            dbgprint(ERROR_LEVEL,"set gpio to input&output fail\n");
            goto _err4;
        }
    } else {
        dbgprint(ERROR_LEVEL,"gpio_irq_pin is not valid\n");
    }

    gpio_direction_output(gpio_reset_pin,0);			/*output 0 from reset pin*/
    gpio_direction_output(gpio_sleep_pin,0);			/*output 0 from sleep pin*/
    //gpio_direction_input(gpio_reset_pin);// for WN803X_DEBUG_V2.1 update 8030 fw
    gpio_direction_input(gpio_status_pin);				/*input mode for status pin*/
    gpio_direction_input(gpio_irq_pin);           /*input mode for irq pin*/

    gpio_direction_output(gpio_vdd_pin,1);              /*output 1 from vdd pin*/

    msleep(2);

    gpio_set_value(gpio_reset_pin,1);             /*output 1 from reset pin*/
    gpio_set_value(gpio_sleep_pin,1);             /*output 1 from reset pin*/

    if(gpio_is_valid(gpio_status_pin))
    {
        g_wakeup_irqno = gpio_to_irq(gpio_status_pin);
        err = request_threaded_irq(g_wakeup_irqno, NULL, Nanosic_wakeup_irq, IRQF_TRIGGER_RISING | IRQF_ONESHOT, "8030_io_wakeup", (void*)&gpio_status_pin);
        if (err < 0) {
            dbgprint(ERROR_LEVEL,"Could not register for %s interrupt, irq = %d, ret = %d\n","8030_io_wakeup", g_wakeup_irqno, err);
            goto _err5;
        }
    }

    registered = true;
    g_panel_status = false;

    dbgprint(ERROR_LEVEL,"register gpio succeed\n");

    return 0;

_err5:
    gpio_free(gpio_irq_pin);
_err4:
#ifdef SUPPORT_GPIO_SLEEP_FUNCTION
    gpio_free(gpio_sleep_pin);
#endif
_err3:
    gpio_free(gpio_vdd_pin);
_err2:
    gpio_free(gpio_status_pin);
_err1:
    gpio_free(gpio_reset_pin);

    return -EFAULT;
}

/** ************************************************************************//**
 *  @func Nanosic_Gpio_Recovery
 *  
 *  @brief 操作gpio时序使803进入出厂恢复
 *
 ** */
int
Nanosic_GPIO_recovery(struct nano_i2c_client* client , char* data, int datalen)
{
    int err = -1;
    char readbuf[I2C_DATA_LENGTH_READ]={0};
    int read_retry = 30;
    int write_retry = 30;

    if(IS_ERR_OR_NULL(data) || IS_ERR_OR_NULL(client) || (datalen < 0)){
        dbgprint(ERROR_LEVEL,"fail recovery reason invalid argment\n");
        return err;
    }

    if(registered == false){
        dbgprint(ERROR_LEVEL,"need register first\n");
        return err;
    }

    if(client->irqno < 0) {
        dbgprint(INFO_LEVEL,"Nanosic_GPIO_recovery: Nanosic_timer_release\n");
        Nanosic_timer_release();							/*定时器测试模式,中断不使用情况下使用*/
    }else{
        dbgprint(INFO_LEVEL,"Nanosic_GPIO_recovery: free_irq\n");
        free_irq(client->irqno,client);				/*irq需要切换成input模式,这里先关闭中断,结束时再次注册*/
    }

    if(gpio_is_valid(gpio_status_pin))
    {
        free_irq(gpio_to_irq(gpio_status_pin),&gpio_status_pin);
    }

    gpio_set_value(gpio_reset_pin,0);					/*output 0 from reset pin*/

    mdelay(2);
    /*status/sleep/irq switch to output mode*/
    gpio_direction_output(gpio_status_pin,0);	/*output 0 from status pin*/
    gpio_direction_output(gpio_sleep_pin,0);	/*output 0 from sleep pin */
    gpio_direction_output(gpio_irq_pin,0);		/*output 0 from irq pin */

    mdelay(2);

    gpio_set_value(gpio_status_pin,1);				/*output 1 from status pin*/
    gpio_set_value(gpio_sleep_pin,1);					/*output 1 from sleep pin*/

    mdelay(2);							/*等待sleep/status输出电平稳定*/

    gpio_set_value(gpio_reset_pin,1);					/*output 1 from reset pin*/
    dbgprint(DEBUG_LEVEL,"control 803 reset\n");
    
    /*延时350ms,等待803进入boot模式*/
    mdelay(350);

    while(read_retry > 0)
    {   /*查询803是否进入boot模式,读3次*/
        err = Nanosic_i2c_read(client,readbuf,sizeof(readbuf));
        if(err > 0){
            rawdata_show("boot recv",readbuf,sizeof(readbuf));
            break;
        }
        mdelay(30);
        read_retry--;
        dbgprint(ERROR_LEVEL,"i2c read retry %d\n",read_retry);
    }

    if(err > 0)/*已经进入boot模式*/
    {
        mdelay(2);
        while(write_retry > 0)
        {
            dbgprint(DEBUG_LEVEL,"send recovery command\n");
            /*803 reset后有500ms的i2c通讯窗口期,需要在500ms之内发送恢复出厂command指令*/
            err = Nanosic_i2c_write(client,data,datalen);
            dbgprint(DEBUG_LEVEL,"i2c write ret=%d\n",err);
            if(err > 0){
                dbgprint(DEBUG_LEVEL,"send recovery command succeed\n");
                break;
            }
            mdelay(30);
            write_retry--;
            dbgprint(DEBUG_LEVEL,"i2c write fail then retry %d\n",write_retry);
        }
    }

    gpio_direction_input(gpio_status_pin);		/*switch to input mode*/
    gpio_direction_input(gpio_irq_pin);				/*switch to input mode*/
    gpio_set_value(gpio_sleep_pin,1);					/*sleep继续保持高电平*/

    //mdelay(10000);
    if(client->irqno < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic_GPIO_recovery: Nanosic_timer_register\n");
        Nanosic_timer_register(client);
    }else{																		/*重新注册中断*/
        err = request_threaded_irq(client->irqno, NULL, Nanosic_i2c_irq, client->irqflags, "8030_io_irq", client);
        if (err < 0) {
            dbgprint(ERROR_LEVEL,"Could not register for %s interrupt, irq = %d, ret = %d, irqflags = 0x%x\n","8030_io_irq", client->irqno, err,client->irqflags);
        }
    }

    if(gpio_is_valid(gpio_status_pin))
    {
        g_wakeup_irqno = gpio_to_irq(gpio_status_pin);
        err = request_threaded_irq(g_wakeup_irqno, NULL, Nanosic_wakeup_irq, IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND, "8030_io_wakeup", (void*)&gpio_status_pin);
        if (err < 0) {
            dbgprint(ERROR_LEVEL,"Could not register for %s interrupt, irq = %d, ret = %d\n","8030_io_wakeup", g_wakeup_irqno, err);
        }
    }

    return 0;
}

/** ************************************************************************//**
 *  @func Nanosic_GPIO_sleep
 *  
 *  @brief switch to sleep mode , low level valid 低电平有效
 *  bool sleep : 0 -> 进入睡眠模式
 *               1 -> 退出睡眠模式
 ** */
void
Nanosic_GPIO_sleep(bool sleep)
{
    if(registered == false){
        dbgprint(ERROR_LEVEL,"need register first\n");
        return;
    }

    /*GPIO_SLEEP set*/
    gpio_set_value(gpio_sleep_pin,sleep);
    g_panel_status = sleep;
    dbgprint(DEBUG_LEVEL,"set gpio sleep pin %d\n",sleep);
}

/** ************************************************************************//**
 *  @func Nanosic_GPIO_test
 *  
 *  @brief set gpio pin level
 *
 ** */
void
Nanosic_GPIO_set(int gpio_pin,bool gpio_level)
{
    int gpio_pin_value;
    gpio_pin_value = gpio_pin + 1100;
    if(registered == false){
        dbgprint(ERROR_LEVEL,"need register first\n");
        return;
    }

    if(!gpio_is_valid(gpio_pin)){
        dbgprint(ERROR_LEVEL,"invalid gpio pin %d\n",gpio_pin);
        return;
    }
    gpio_set_value(gpio_pin_value,gpio_level);
    dbgprint(DEBUG_LEVEL,"set gpio pin %d level %d\n",gpio_pin_value,gpio_level);
}

/** ************************************************************************//**
 *  @func Nanosic_Hall_notify
 *
 *  @brief Hall notify
 *
 ** */
int
Nanosic_Hall_notify(int hall_n_pin, int hall_s_pin)
{
    int err;
    int hall_n_value = -1;
    int hall_s_value = -1;
    unsigned char hall_data[66] = {0x24,0x20,0x80,0x80,0xE1,0x01,0x00};

    if(hall_n_pin < 0 || hall_s_pin < 0){
        dbgprint(ERROR_LEVEL,"invalid pin value\n");
        return -EFAULT;
    }
/*
    err = gpio_export(hall_n_pin,true);
    if(err)
        dbgprint(ERROR_LEVEL,"export hall_n gpio fail\n");
    err = gpio_export(hall_s_pin,true);
    if(err)
        dbgprint(ERROR_LEVEL,"export hall_s gpio fail\n");
*/
    hall_n_value = gpio_get_value(hall_n_pin);
    hall_s_value = gpio_get_value(hall_s_pin);
    dbgprint(DEBUG_LEVEL,"hall_n:%d hall_s:%d\n",hall_n_value,hall_s_value);

    spin_lock(&gI2c_client->input_report_lock);


    if(hall_n_value == 0 && hall_s_value == 1){
        hall_data[6] = 0x01;
    }
    if(hall_n_value == 1 && hall_s_value == 0){
        hall_data[6] = 0x10;
    }
    if(hall_n_value == 1 && hall_s_value == 1) {
        hall_data[6] = 0x11;
    }
    //err = Nanosic_i2c_write(gI2c_client,hall_data,sizeof(hall_data));
    err = Nanosic_chardev_client_write(hall_data,sizeof(hall_data));

    dbgprint(DEBUG_LEVEL,"Hall notify, err:%d report 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n",
        err,hall_data[0],hall_data[1],hall_data[2],hall_data[3],hall_data[4],hall_data[5],hall_data[6],hall_data[7],hall_data[8]);
    spin_unlock(&gI2c_client->input_report_lock);

    return err;
}

/** ************************************************************************//**
 *  @func Nanosic_RequestGensor_notify
 *
 *  @brief request fw get gsensor
 *
 ** */
int
Nanosic_RequestGensor_notify(void)
{
    int err;
    unsigned char requestgensor_data[66] = {0x32,0x00,0x4E,0x31,FIELD_HOST,FIELD_176X,0x52,0x00,0x89};

    //spin_lock(&gI2c_client->input_report_lock);

    err = Nanosic_i2c_write(gI2c_client,requestgensor_data,sizeof(requestgensor_data));

    dbgprint(DEBUG_LEVEL,"Nanosic_RequestGensor_notify, err:%d report 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        err,requestgensor_data[0],requestgensor_data[1],requestgensor_data[2],requestgensor_data[3],requestgensor_data[4],requestgensor_data[5],requestgensor_data[6],requestgensor_data[7],requestgensor_data[8],requestgensor_data[9],requestgensor_data[10]);
    //spin_unlock(&gI2c_client->input_report_lock);

    return err;
}


/** ************************************************************************//**
 *  @func Nanosic_Gpio_Release
 *  
 *  @brief gpio free
 *
 ** */
void
Nanosic_GPIO_release(void)
{
    if(registered == true)
    {
#ifdef SUPPORT_GPIO_IRQ_FUNCTION
        gpio_free(gpio_irq_pin);
#endif
#ifdef SUPPORT_GPIO_SLEEP_FUNCTION
        gpio_free(gpio_sleep_pin);
#endif
        if(gpio_is_valid(gpio_status_pin)){
            free_irq(gpio_to_irq(gpio_status_pin) , &gpio_status_pin);
        }
        gpio_free(gpio_status_pin);
        gpio_free(gpio_reset_pin);
        gpio_free(gpio_vdd_pin);
        registered = false;
    }
}

