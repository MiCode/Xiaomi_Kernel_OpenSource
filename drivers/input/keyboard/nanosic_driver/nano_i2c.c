/** ***************************************************************************
 * @file nano_i2c.c
 *
 * @brief nanosic i2c client  
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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
//#include <linux/sys_config.h>

#include "nano_macro.h"
//static bool first = true;
/*i2c slave address用单字节表示*/
static const unsigned int const_iaddr_bytes = 1;
/* I2C internal address max length */
#define INT_ADDR_MAX_BYTES   4

/*store 803`s version code*/
char gVers803x[21]={0};

/*store 176`s version code*/
char gVers176x[21]={0};

short gHallStatus = 0;
/** **************************************************************************
 * @brief
 *   
 ** */
static bool
Nanosic_i2c_specified_packets_detect(char* data)
{
    char source;
    char object;
    char command;

    if(!data)
        return false;

    STREAM_TO_UINT8(source,data);
    STREAM_TO_UINT8(object,data);
    STREAM_TO_UINT8(command,data);

    if(command == 0x1 && source == FIELD_803X && object == FIELD_HOST)
    {
        /*803->host 上传803版本号*/
        memcpy(gVers803x, data+2, 20);
        return true;
    }else if(command == 0x1 && source == FIELD_176X && object == FIELD_HOST)
    {
    	/*keypad->host 上传keypad版本号*/
        char len;
        short version;
        STREAM_TO_UINT8(len,data);
        STREAM_TO_UINT16(version,data);
        snprintf(gVers176x,sizeof(gVers176x),"%x",version);
        return true;
    }else if(command == 0xA2 && source == FIELD_176X && object == FIELD_HOST){
        data += 4;
        STREAM_TO_UINT8(gHallStatus,data);
    }

    return false;
}

/** **************************************************************************
 * @brief 驱动probe时的read接口
 *   
 ** */
static int
Nanosic_i2c_read_boot(struct i2c_client* client ,void *buf, size_t len)
{
    unsigned char addr[INT_ADDR_MAX_BYTES];
	struct i2c_msg msg[2];
	int ret;

    if(IS_ERR_OR_NULL(client)){
		dbgprint(ERROR_LEVEL,"client is NULL\n");
        return -1;
    }

	addr[0] = client->addr;
    /*
	 * Send out the register address...
	 */
    msg[0].len   = const_iaddr_bytes;
	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].buf   = addr;
	/*
	 * ...then read back the result.
	 */
	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = len;
	msg[1].buf   = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0){
		dbgprint(ERROR_LEVEL,"i2c_transfer read error\n");
        len = -1;
	}

	return len;
}

/** **************************************************************************
 * @brief   驱动probe时的write接口
 *   
 ** */
static int
Nanosic_i2c_write_boot(struct i2c_client* client, void *buf, size_t len)
{
	struct i2c_msg msg;
    unsigned char tmp_buf[128]={0};
	int ret;

    if(IS_ERR_OR_NULL(client)){
		dbgprint(ERROR_LEVEL,"client is NULL\n");
        return -1;
    }

    tmp_buf[0] = client->addr;
    memcpy(tmp_buf + const_iaddr_bytes , buf, len);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len + const_iaddr_bytes;
	msg.buf = tmp_buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0){
        dbgprint(ERROR_LEVEL,"i2c_transfer write error\n");
        len = -1;
    }

	return len;
}

/** **************************************************************************
 * @brief
 *   
 ** */
int
Nanosic_i2c_read(struct nano_i2c_client* i2c_client ,void *buf, size_t len)
{
	struct i2c_adapter *adap;
    unsigned char addr[INT_ADDR_MAX_BYTES];
	struct i2c_msg msg[2];
	int ret;

    if(IS_ERR_OR_NULL(i2c_client)){
		dbgprint(ERROR_LEVEL,"i2c_client is NULL\n");
        return -1;
    }

	addr[0] = i2c_client->i2c_slave_addr;

	adap = i2c_get_adapter(i2c_client->i2c_bus_id);
	/*
	 * Send out the register address...
	 */
    msg[0].len   = const_iaddr_bytes;
	msg[0].addr  = i2c_client->i2c_slave_addr;
	msg[0].flags = 0;
	msg[0].buf   = addr;
	/*
	 * ...then read back the result.
	 */
	msg[1].addr  = i2c_client->i2c_slave_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = len;
	msg[1].buf   = buf;

	mutex_lock(&i2c_client->read_mutex);

	ret = i2c_transfer(adap, msg, 2);
	if (ret < 0){
		dbgprint(ERROR_LEVEL,"i2c_transfer read error\n");
        len = -1;
	}

	mutex_unlock(&i2c_client->read_mutex);

	return len;
}

/** **************************************************************************
 * @brief
 *   write data to 8030x
 ** */
int
Nanosic_i2c_write(struct nano_i2c_client* i2c_client, void *buf, size_t len)
{
	struct i2c_msg msg;
	struct i2c_adapter *adap;
    unsigned char tmp_buf[128]={0};
	int ret;

    if(IS_ERR_OR_NULL(i2c_client)){
		dbgprint(ERROR_LEVEL,"i2c_client is NULL\n");
        return -1;
    }

	adap = i2c_get_adapter(i2c_client->i2c_bus_id);

    tmp_buf[0] = i2c_client->i2c_slave_addr;
    memcpy(tmp_buf + const_iaddr_bytes , buf, len);

	msg.addr = i2c_client->i2c_slave_addr;
	msg.flags = 0;
	msg.len = len + const_iaddr_bytes;
	msg.buf = tmp_buf;

    mutex_lock(&i2c_client->write_mutex);

	ret = i2c_transfer(adap, &msg, 1);
	if (ret < 0){
	dbgprint(ERROR_LEVEL,"i2c_transfer write error\n");
	len = -1;
	}

    mutex_unlock(&i2c_client->write_mutex);

	return len;
}

/** **************************************************************************
 * @brief   i2c上传数据的解析 , 数据格式参见 <IIC总线协议>文挡
 *   
 ** */

int
Nanosic_i2c_parse(char* data, size_t datalen)
{
    char* p = data;
    char  left;
    unsigned char  first_byte;  /*fixed data 0x57*/
    unsigned char  second_byte; /*sqe*/
    unsigned char  third_byte;  /*fixed data 0x39 , 0x4A , 0x5B , 0x6C*/
    unsigned char  fourth_byte; /*fixed data 0x2: mouse  0x5: keyboard 0x19: touch*/
    unsigned short seq=0;

    if(datalen < 0){
        dbgprint(ERROR_LEVEL , "invalid data length\n");
        return -1;
    }

    left = datalen;

//    atomic_inc(&i2c_received);

    STREAM_TO_UINT8(first_byte,p);
    left--;
    if(first_byte != 0x57){
        dbgprint(DEBUG_LEVEL , "first_byte error\n");
        return -1;
    }

    STREAM_TO_UINT8(second_byte,p);
    left--;

    STREAM_TO_UINT8(third_byte,p);
    left--;

    if(third_byte == 0){ /*null packet*/
        //dbgprint(DEBUG_LEVEL , "third_byte is null\n");
        return -1;
    }

    if(third_byte != 0x39 && third_byte != 0x4A && third_byte != 0x5B && third_byte != 0x6C){
        /*no packets include*/
        dbgprint(DEBUG_LEVEL , "third_byte error\n");
        return -1;
    }

    rawdata_show("_rawdata_",data,datalen);

    while(left > 0)
    {
        STREAM_TO_UINT8(fourth_byte,p);
        STREAM_TO_UINT16(seq,p);
        p-=3;

        switch(fourth_byte)
        {
            case 0x5:
                /*05 00 00 52 00 00 00 00 00 上*/
                /*05 00 00 51 00 00 00 00 00 下*/
                if(g_panel_status){
                    Nanosic_cache_put();
                    Nanosic_input_write(EM_PACKET_KEYBOARD,p,9);
                    p += 9;
                    left -= 9;
                }else{
                    Nanosic_cache_insert(EM_PACKET_KEYBOARD,p,9);
                    p += 9;
                    left -= 9;
                }
                break;
            case 0x6:
                if(g_panel_status){
                    Nanosic_cache_put();
                    Nanosic_input_write(EM_PACKET_CONSUMER,p,5);
                    p += 5;
                    left -= 5;
                }else{
                    Nanosic_cache_insert(EM_PACKET_CONSUMER,p,5);
                    p += 5;
                    left -= 5;
                }
                break;
            case 0x2:
                Nanosic_input_write(EM_PACKET_MOUSE,p,8);
                p += 8;
                left -= 8;
                break;
            case 0x19:
                Nanosic_input_write(EM_PACKET_TOUCH,p,21);
                p += 21;
                left -= 21;
                break;
            case 0x22:
                Nanosic_i2c_specified_packets_detect(p+2);
                Nanosic_chardev_client_write(p,16);     /* upstreams data via /dev/nanodev0 to userspace*/
                p += 16;
                left -= 16;
                break;
            case 0x23:
                Nanosic_i2c_specified_packets_detect(p+2);
                Nanosic_chardev_client_write(p,32);
                Nanosic_input_write(EM_PACKET_VENDOR,p,32);
                p += 32;
                left -= 32;
                break;
            case 0x26:
            case 0x24:
            //case 0x23:
            {
                Nanosic_i2c_specified_packets_detect(p+2);
                Nanosic_chardev_client_write(p,left);  /* upstreams data via /dev/nanodev0 to userspace*/
                return 0;
            }

            default:
                left = 0;
                break;
        }
    }

    return 0;
}

/** ************************************************************************//**
 * @brief   Nanosic_i2c_read_process
 *          工作队列下半部读取i2c数据并解析
 ** */
int
Nanosic_i2c_read_handler(void* data)
{
    struct nano_i2c_client* i2c_client = (struct nano_i2c_client*) data;

    char  buf[I2C_DATA_LENGTH_READ]={0};
    int   datalen;

    if(IS_ERR_OR_NULL(i2c_client))
    {
        dbgprint(ALERT_LEVEL,"Nanosic_i2c_read_handler, null\n");
        return -1;
    }

    datalen = Nanosic_i2c_read(i2c_client,buf,sizeof(buf));
    if(datalen > 0){
        Nanosic_i2c_parse(buf,datalen);
    }else{
        atomic_inc(&i2c_client->i2c_error_count);
    }
    atomic_inc(&i2c_client->i2c_read_count);

    return 0;
}
#ifdef I2C_GPIO_IRQNO
/** ************************************************************************//**
 * @brief   Nanosic_i2c_irq
 *          I2C GPIO 中断处理程序 
 ** */
irqreturn_t
Nanosic_i2c_irq(int irq, void *dev_id)
{
    struct nano_i2c_client* i2c_client = (struct nano_i2c_client*)dev_id;

    if(IS_ERR_OR_NULL(i2c_client)){
        dbgprint(ERROR_LEVEL,"i2c_client is NULL\n");
        return IRQ_HANDLED;
    }

    Nanosic_workQueue_schedule(i2c_client->worker);

	return IRQ_HANDLED;
}
#endif

int
Nanosic_i2c_read_version(struct i2c_client *client)
{
    char rsp[I2C_DATA_LENGTH_READ]={0};
    char cmd[I2C_DATA_LENGTH_WRITE]={0x32,0x00,0x4F,0x30,0x80,0x18,0x01,0x00,0x18};
    u8  retry = 0;
    int ret = -1;

    while(retry++ < 30)
    {
        ret = Nanosic_i2c_write_boot(client,cmd,sizeof(cmd));
        if(ret < 0){
            dbgprint(ALERT_LEVEL,"i2c write cmd failed time %d\n",retry);
            msleep(100);
            continue;
        }
        dbgprint(ALERT_LEVEL,"i2c write OK %d\n",ret);
        msleep(2);
        ret = Nanosic_i2c_read_boot(client,rsp,sizeof(rsp));
        dbgprint(ALERT_LEVEL,"i2c read %d\n",ret);
        if(ret > 0){
            rawdata_show("recv vers",rsp,sizeof(rsp));
            break;
        }
        msleep(2);
    }

    return ret;
}

int
Nanosic_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    struct i2c_adapter *adapter = client->adapter;
    int  ret = -1;

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)){
    	dbgprint(ALERT_LEVEL,"======return=====\n");
        return -ENODEV;
    }
        
    dbgprint(ALERT_LEVEL,"%s: addr = %x\n", __func__, client->addr);

    ret = Nanosic_i2c_read_version(client);

    return ret;   
}

/** ************************************************************************//**
 *  @func Nanosic_i2c_register
 *  
 *  @brief 注册gpio下降沿中断 , 当有中断触发时去获取i2c数据
 *
 ** */
struct nano_i2c_client*
Nanosic_i2c_register(int irqno,u32 irq_flags,int i2c_bus_id, int i2c_slave_addr)
{
#ifdef I2C_GPIO_IRQNO
    int ret;
#endif
    struct nano_i2c_client* i2c_client = kzalloc(sizeof(struct nano_i2c_client),GFP_KERNEL);
    if(IS_ERR_OR_NULL(i2c_client)){
        dbgprint(ERROR_LEVEL,"Could not alloc memory\n");
        return NULL;
    }

    if(i2c_bus_id < 0 || i2c_slave_addr < 0){
        dbgprint(ERROR_LEVEL,"Invalid argments\n");
        return NULL;
    }

    dbgprint(DEBUG_LEVEL,"new i2c %p\n",i2c_client);

    atomic_set(&i2c_client->i2c_read_count, 0);
    atomic_set(&i2c_client->i2c_error_count, 0);
    i2c_client->i2c_bus_id     = i2c_bus_id;
    i2c_client->i2c_slave_addr = i2c_slave_addr;
    i2c_client->irqno = gpio_to_irq(irqno);
    i2c_client->irqflags = irq_flags;
    i2c_client->func = Nanosic_i2c_read_handler;
    /*initialize wakeup_irq spinlock*/
    spin_lock_init(&i2c_client->input_report_lock);
    /*initialize i2c read  mutexlock*/
    mutex_init(&i2c_client->read_mutex);
    /*initialize i2c write mutexlock*/
    mutex_init(&i2c_client->write_mutex);

    i2c_client->input_dev = input_allocate_device();
    if (!i2c_client->input_dev) {
        dbgprint(ERROR_LEVEL, "No such input device defined! \n");
        goto _err3;
    }

    __set_bit(EV_SYN, i2c_client->input_dev->evbit);
    __set_bit(EV_KEY, i2c_client->input_dev->evbit);
    i2c_client->input_dev->name = "xiaomi keyboard WakeUp";
    i2c_client->input_dev->id.product = 0x94;
    i2c_client->input_dev->id.vendor = 0x0827;

    input_set_capability(i2c_client->input_dev, EV_KEY, KEY_WAKEUP);
    input_set_capability(i2c_client->input_dev, EV_SW, SW_LID);
    input_set_capability(i2c_client->input_dev, EV_SW, SW_TABLET_MODE);
    ret = input_register_device(i2c_client->input_dev);
    if (ret) {
        dbgprint(ERROR_LEVEL, "No such input device\n");
        goto _err3;
    }

    /*initialize workqueue*/
    i2c_client->worker = Nanosic_workQueue_register(i2c_client);
    if(IS_ERR_OR_NULL(i2c_client->worker)){
        dbgprint(ERROR_LEVEL,"Could not register workqueue\n");
        goto _err1;
    }

    dbgprint(DEBUG_LEVEL,"new worker %p\n",i2c_client->worker);

#ifdef I2C_GPIO_IRQNO
    dbgprint(INFO_LEVEL,"irq-gpio:%d irq-no:%d irq_flags:%d \n",irqno,i2c_client->irqno,irq_flags);
    if(i2c_client->irqno > 0)
    {
        //ret = request_irq(i2c_client->irqno, Nanosic_i2c_irq, irq_flags/*IRQF_TRIGGER_FALLING*/, "8030_io_irq", i2c_client);
        ret = request_threaded_irq(i2c_client->irqno, NULL, Nanosic_i2c_irq, irq_flags, "8030_io_irq", i2c_client);
        if (ret < 0) {
            dbgprint(ERROR_LEVEL,"Could not register for %s interrupt, irq = %d, ret = %d\n","8030_io_irq", i2c_client->irqno, ret);
            msleep(30);
            goto _err2;
        }
    }else{
        /*initialize timer for test*/
        Nanosic_timer_register(i2c_client);
    }
#endif
    return i2c_client;

_err3:
#ifdef I2C_GPIO_IRQNO
_err2:
    Nanosic_workQueue_release(i2c_client->worker);
#endif
_err1:
    kfree(i2c_client);

    return NULL;
}

/** ************************************************************************//**
 *  @func Nanosic_i2c_release
 *  
 *  @brief
 *
 ** */
void
Nanosic_i2c_release(struct nano_i2c_client* i2c_client)
{
    if(IS_ERR_OR_NULL(i2c_client))
        return;

    Nanosic_workQueue_release(i2c_client->worker);
#ifdef I2C_GPIO_IRQNO
    if(i2c_client->irqno > 0)
        free_irq(i2c_client->irqno , i2c_client);
    else{
        /*release timer module*/
        Nanosic_timer_release();
    }
#endif
    kfree(i2c_client);
}
 
