/*
 * drivers/mmc/card/modem_sdio.c
 *
 * VIA CBP SDIO driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/circ_buf.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/kfifo.h>
#include <linux/slab.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/mmc/host.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include "modem_sdio.h"
#include "viatel.h"

static int sdio_tx_cnt = 0;
static int sdio_rx_cnt = 0;
#define FIFO_SIZE	8*PAGE_SIZE
#define SDIO_WAKEUP_CHARS		8*256

#define TRANSMIT_SHIFT	(10)
#define TRANSMIT_BUFFER_SIZE	(1UL << TRANSMIT_SHIFT)
#define TRANSMIT_MAX_SIZE     ((1UL << TRANSMIT_SHIFT)  - 4) 

static struct tty_driver *modem_sdio_tty_driver;
static struct cbp_platform_data *cbp_pdata;
static struct sdio_modem_port *sdio_modem_table[SDIO_TTY_NR];
static DEFINE_SPINLOCK(sdio_modem_table_lock);
int sdio_log_level = LOG_DEBUG;
#define SDIO_AT_CHANNEL_NUM			4
extern unsigned char cbp_power_state;
static struct sdio_modem *via_modem = NULL;

static unsigned int dtr_value = 0;
static unsigned int dcd_state = 0;

static struct work_struct dtr_work;
static struct work_struct dcd_query_work;

static unsigned int modem_remove = 1;
static spinlock_t modem_remove_lock;

/**
 * sdio_tx_rx_printk - print sdio tx and rx data, when log level is LOG_NOTICE or larger
 * @buf: the point of data buffer
 * @type: print type, 0:rx  1:tx
 *
 * no return
 */
static void sdio_tx_rx_printk(const void *buf, unsigned char type)
{
	unsigned int count;
	const unsigned char *print_buf = (const unsigned char *)buf;
	int i;
	
	count =(((*(print_buf+2) & 0x0F) << 8) | (*(print_buf+3) & 0xFF));
	if(type == 1){
		printk("[MODEM SDIO] write %d to channel%d/[%d]>>", 
			count, *(print_buf+1), sdio_tx_cnt);
	}
	else{
		printk("[MODEM SDIO] read %d from channel%d/[%d]<<", 
			count, *(print_buf+1), sdio_rx_cnt);
	}

	if(count > 20)
		count = 20;
	for(i = 0; i < count + 4; i++)
	{
		printk("%x-", *(print_buf+i));
	}
	printk("\n");
}

extern int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer,
       unsigned int length);       
extern int rawbulk_bind_sdio_channel(int transfer_id);
extern void rawbulk_unbind_sdio_channel(int transfer_id);
void modem_dtr_send(struct work_struct *work);

static struct sdio_modem_port *sdio_modem_port_get(unsigned index)
{
	struct sdio_modem_port *port;
	unsigned long flags = 0;

	if (index >= SDIO_TTY_NR)
		return NULL;

	spin_lock_irqsave(&sdio_modem_table_lock, flags);
	port = sdio_modem_table[index];
	spin_unlock_irqrestore(&sdio_modem_table_lock, flags);

	return port;
}
static void sdio_modem_port_destroy(struct kref *kref)
{
	struct sdio_modem_port *port = container_of(kref, struct sdio_modem_port, kref);
	int index;

	if(port){
		index = port->index;
		LOGPRT(LOG_NOTICE, "%s %d: index = %d .\n", __func__, __LINE__, index);
		kfifo_free(&port->transmit_fifo);
		kfree(port);
	}else{
		LOGPRT(LOG_ERR,  "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static void sdio_modem_port_put(struct sdio_modem_port *port)
{
	if(port){
		LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
		kref_put(&port->kref, sdio_modem_port_destroy);
	}else{
		LOGPRT(LOG_ERR,  "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static int check_port(struct sdio_modem_port *port) {
	if (!port || !port->func || (cbp_power_state ==0)){
		LOGPRT(LOG_ERR, "%s %d: cbp_power_state=%d.\n", __func__, __LINE__,cbp_power_state);
		return -ENODEV;
	}
	//WARN_ON(!port->port.count);
	return 0;
}

static void modem_sdio_write(struct sdio_modem *modem, int addr,
		void *buf, size_t len);

/* CBP control message type */
enum cbp_contrl_message_type {
	CHAN_ONOFF_MSG_ID = 0,
	MDM_STATUS_IND_MSG_ID,
	MDM_STATUS_QUERY_MSG_ID,
	CHAN_SWITCH_REQ_MSG_ID,
	CHAN_STATUS_QUERY_MSG_ID,
	FLOW_CONTROL_MSG_ID,
	CHAN_LOOPBACK_TST_MSG_ID,
	MESSAGE_COUNT,
};

typedef enum {
	OPT_LOOPBACK_NON  = 0,		//no operation, default 0
	OPT_LOOPBACK_OPEN  = 1,		//open loopback test
	OPT_LOOPBACK_CLOSE  = 2,		//close loopback test
	OPT_LOOPBACK_QUERY = 3,		//query loopback test
	OPT_LOOPBACK_NUM
}IOP_OPT_LOOPBACK;

typedef enum {
	RSLT_LOOPBACK_SUCCESS  = 0,	//operation succed
	RSLT_LOOPBACK_WORK = 1,		//loopback testing
	RSLT_LOOPBACK_CLOSED = 2,		//no loopback test
	RSLT_LOOPBACK_INVALID = 3,	//invalid operation
	RSLT_LOOPBACK_FAIL = 4,		//loopback test failed
	RSLT_LOOPBACK_NUM
}IOP_RSLT_LOOPBACK;


static int contruct_ctrl_chan_msg(struct sdio_modem_ctrl_port *ctrl_port , int msg, 
									unsigned char chan_num, unsigned char opt)
{
	if (unlikely(ctrl_port == NULL)){
		LOGPRT(LOG_ERR, "%s %d: control channel is null.\n", __func__, __LINE__);
		return -EINVAL;
	}
	
	ctrl_port->chan_ctrl_msg.head.start_flag	= 0xFE;			
	ctrl_port->chan_ctrl_msg.head.chanInfo	= 0;			
	ctrl_port->chan_ctrl_msg.head.tranHi		= 0;			/*High byte of the following payload length*/
	ctrl_port->chan_ctrl_msg.head.tranLow	= 4;			/*Low byte of the following payload length*/
	ctrl_port->chan_ctrl_msg.id_hi			= msg >> 8;/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
	ctrl_port->chan_ctrl_msg.id_low			= msg;/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
	ctrl_port->chan_ctrl_msg.chan_num		= chan_num;			/*ChanNum ,same as ChanInfo*/
	ctrl_port->chan_ctrl_msg.option			= opt;

	return 0;
}
static unsigned char loop_back[12];

int modem_on_off_ctrl_chan(unsigned char on)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port;
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_NOTICE,  "%s: enter, on = %d \n", __func__, on);
	port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}
	
	ret = contruct_ctrl_chan_msg(ctrl_port, CHAN_ONOFF_MSG_ID, 0, on);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s contruct_ctrl_chan_msg failed\n", __func__);
		goto up_sem;
	}
	msg_len = sizeof(struct ctrl_port_msg);
	msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
	modem_sdio_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);

up_sem:
	up(&modem->sem);
down_out:
	return ret;
}
EXPORT_SYMBOL(modem_on_off_ctrl_chan);

void modem_dtr_set(int on, int low_latency)
{
	unsigned long flags;
    struct sdio_modem_port *port;
    int ret = 0;
    
    port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		return;
	}
	spin_lock_irqsave(&modem_remove_lock, flags);
	if(!modem_remove) {
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		dtr_value = on;
		if(low_latency)
			modem_dtr_send(&dtr_work);
		else
			schedule_work(&dtr_work);
	}
	else{
		spin_unlock_irqrestore(&modem_remove_lock, flags);
	}
}
EXPORT_SYMBOL(modem_dtr_set);

void modem_dtr_send(struct work_struct *work)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port; 
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char control_signal=0;
	unsigned char msg_len = 0;
	int ret = 0;
	unsigned int on = 0;

    on = dtr_value;
	LOGPRT(LOG_NOTICE,  "%s: enter, on =%d\n", __func__, on);
	port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}
	
	if(ctrl_port->chan_state == 1){
		if(on){
			control_signal |= 0x04;
		}
		else{
			control_signal &= 0xFB;
		}
		
		ret = contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_IND_MSG_ID, 2, control_signal);
		if (ret < 0){
			LOGPRT(LOG_ERR,  "%s contruct_ctrl_chan_msg failed\n", __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_sdio_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);
	}
	else{
		ret = -1;
		LOGPRT(LOG_ERR,  "%s: ctrl channel is off, please turn on first\n", __func__);
	}
	
up_sem:
	up(&modem->sem);
down_out:
	return;
}

int modem_dcd_state(void)
{   
    unsigned long flags;
    int ret = 0;
    struct sdio_modem_port *port;
    
    port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		return ret;
	}
    spin_lock_irqsave(&modem_remove_lock, flags);
    if(!modem_remove) {
        spin_unlock_irqrestore(&modem_remove_lock, flags);
        schedule_work(&dcd_query_work);        
    } else {
        spin_unlock_irqrestore(&modem_remove_lock, flags);
        dcd_state = 0;   
    }
    return dcd_state;
}
EXPORT_SYMBOL(modem_dcd_state);

void modem_dcd_query(struct work_struct *work)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port; 
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_NOTICE,  "%s: enter\n", __func__);
	port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	
	if(ctrl_port->chan_state == 1){
		wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
		if(down_interruptible(&modem->sem)){
			LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
			ret =  -ERESTARTSYS;
			goto down_out;
		}
			
		ret = contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_QUERY_MSG_ID, 2, 0);
		if (ret < 0){
			LOGPRT(LOG_ERR,  "%s contruct_ctrl_chan_msg failed\n", __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_sdio_write(modem, 0x00, &(ctrl_port->chan_ctrl_msg), msg_len);
up_sem:
		up(&modem->sem);
        msleep(10);
		dcd_state = port->dtr_state;
	}
	else{
	    dcd_state = 0;
		LOGPRT(LOG_ERR,  "%s: ctrl channel is off, please turn on first\n", __func__);
	}
down_out:
	return;
}

int modem_loop_back_chan(unsigned char chan_num, unsigned char opt)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port;
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_NOTICE,  "%s: enter\n", __func__);
	port = sdio_modem_port_get(0);
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&ctrl_port->sflow_ctrl_state)||(cbp_power_state==0)));
	if(down_interruptible(&modem->sem)){
		LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}

	if(ctrl_port->chan_state == 1){
		loop_back[0]		= 0xFE;			
		loop_back[1]		= 0;			
		loop_back[2]		= 0;			/*High byte of the following payload length*/
		loop_back[3]		= 6;			/*Low byte of the following payload length*/
		loop_back[4]		= 0x00;		/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
		loop_back[5]		= 0x06;		/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID*/
		loop_back[6]		= 3;			/*device id sdio = 3*/
		loop_back[7]		= opt;		
		loop_back[8]		= chan_num;/*ChanNum ,same as ChanInfo*/
		loop_back[9]		= 0;
		
		msg_len = 12;
		msg_len = (msg_len + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_sdio_write(modem, 0x00, &(loop_back[0]), msg_len);
	}
	else{
		ret = -1;
		LOGPRT(LOG_ERR,  "%s: ctrl channel is off, please turn on first\n", __func__);
	}
	up(&modem->sem);
down_out:
	return ret;
}
EXPORT_SYMBOL(modem_loop_back_chan);

static int ctrl_msg_analyze(struct sdio_modem *modem)
{
	struct sdio_modem_ctrl_port *ctrl_port;
	const unsigned char msg_id_high 	= *modem->msg->buffer;
	const unsigned char msg_id_low 	= *(modem->msg->buffer+1);
	const unsigned int msg_id 			= (msg_id_high << 8) + msg_id_low;
	unsigned char option=*(modem->msg->buffer+3);
	struct sdio_modem_port *port;
	unsigned char chan_num;

	ctrl_port = modem->ctrl_port;

	switch(msg_id)
	{
		case CHAN_ONOFF_MSG_ID:
			if(option == 1){
				ctrl_port->chan_state = 1;
				LOGPRT(LOG_NOTICE,  "%s: ctrl channel is open\n", __func__);
			}
			else if(option == 0){
				ctrl_port->chan_state = 0;
				LOGPRT(LOG_NOTICE,  "%s: ctrl channel is close\n", __func__);
			}
			else{
				LOGPRT(LOG_ERR,  "%s: err option value = %d\n", 
				__func__, option);
			}
		case MDM_STATUS_IND_MSG_ID:
			port = modem->port[0];
			if(option & 0x80)/*connetc*/
			{
				port->dtr_state = 1;
			}
			else/*disconnetc*/
			{
				port->dtr_state = 0;
			}
			break;
		case MDM_STATUS_QUERY_MSG_ID:
			port = modem->port[0];
			if(option & 0x80)/*connetc*/
			{
				port->dtr_state = 1;
			}
			else/*disconnetc*/
			{
				port->dtr_state = 0;
			}
			/*to be contionue*/
			break;
		case CHAN_LOOPBACK_TST_MSG_ID:
			{
				chan_num =*(modem->msg->buffer+4);
				unsigned char res =*(modem->msg->buffer+5);
				if(option == OPT_LOOPBACK_OPEN)/*open*/
				{
					LOGPRT(LOG_NOTICE,  "%s %d: open chan %d, result = %d\n", 
									__func__, __LINE__,chan_num, res);
				}
				else if(option == OPT_LOOPBACK_CLOSE)/*close*/
				{
					LOGPRT(LOG_NOTICE,  "%s %d: close chan %d, result = %d\n", 
									__func__, __LINE__,chan_num, res);
				}
				else if(option == OPT_LOOPBACK_QUERY)/*close*/
				{
					LOGPRT(LOG_NOTICE,  "%s %d: query chan %d, result = %d\n", 
									__func__, __LINE__,chan_num, res);
				}
				else
				{
					LOGPRT(LOG_ERR,  "%s %d: unknow option %d\n", __func__, __LINE__, option);
				}
			}
			break;
		case FLOW_CONTROL_MSG_ID:
			chan_num = *(modem->msg->buffer+2);
			if(chan_num > 0 && chan_num < (SDIO_TTY_NR + 1)){
				chan_num = chan_num - 1;
				port = modem->port[chan_num];
				if(option == 1){
					LOGPRT(LOG_INFO,  "%s %d: channel%d soft flow ctrl enable!\n", __func__, __LINE__, (port->index + 1));
					atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_ENABLE);
				}
				else if(option == 0){
					LOGPRT(LOG_INFO,  "%s %d: channel%d soft flow ctrl disable!\n", __func__, __LINE__, (port->index + 1));
					atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
					wake_up(&port->sflow_ctrl_wait_q);
				}
			}
			else if(chan_num == 0){
				if(option == 1){
					LOGPRT(LOG_INFO,  "%s %d: ctrl channel soft flow ctrl enable!\n", __func__, __LINE__);
					atomic_set(&ctrl_port->sflow_ctrl_state, SFLOW_CTRL_ENABLE);
				}
				else if(option == 0){
					LOGPRT(LOG_INFO,  "%s %d: ctrl channel soft flow ctrl disable!\n", __func__, __LINE__);
					atomic_set(&ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
					wake_up(&ctrl_port->sflow_ctrl_wait_q);
				}
			}else{
				LOGPRT(LOG_ERR,  "%s %d: unkown channel num%d!\n", __func__, __LINE__, chan_num);
			}
			break;
		case CHAN_SWITCH_REQ_MSG_ID:
			/*to be contionue*/
			break;
		case CHAN_STATUS_QUERY_MSG_ID:
			/*to be contionue*/
			break;
		default:
			LOGPRT(LOG_ERR,  "%s %d: unknow control message received\n", __func__, __LINE__);
			goto err_wrong_format;
	}
	return 0;
	
err_wrong_format:
	return -1;
}

static void sdio_buffer_in_print(struct sdio_modem_port *port, struct sdio_buf_in_packet *packet)
{
	unsigned int count;
	int i;
	
	printk("[MODEM SDIO] sdio channel%d buffer in %d bytes data<<", (port->index+1), packet->size);
	count = packet->size;
	if( count > 20){
		count = 20;
	}
	for(i = 0; i < count; i++)
	{
		printk("%x-", *(packet->buffer+i));
	}
	printk("\n");
}

static void sdio_buf_in_tty_work(struct sdio_modem_port *port)
{
	struct sdio_buf_in_packet *packet = NULL;
	struct tty_struct *tty;
	int room;

	tty = tty_port_tty_get(&port->port);
	if(tty){
		while(!list_empty(&port->sdio_buf_in_list)){
			packet = list_first_entry(&port->sdio_buf_in_list, struct sdio_buf_in_packet, node);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
			room = tty_buffer_request_room(&port->port, packet->size);
#else			
			room = tty_buffer_request_room(tty, packet->size);
#endif
			if(room < packet->size){
				LOGPRT(LOG_ERR,  "%s %d: no room in tty rx buffer!\n", __func__,__LINE__);
			}
			else{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
				room = tty_insert_flip_string(&port->port, packet->buffer, packet->size);
#else			
				room = tty_insert_flip_string(tty, packet->buffer, packet->size);
#endif
				if(room < packet->size){
					LOGPRT(LOG_ERR,  "%s %d: couldn't insert all characters (TTY is full?)!\n", __func__,__LINE__);
				}
				else{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))					
					tty_flip_buffer_push(&port->port);
#else
					tty_flip_buffer_push(tty);
#endif
				}
			}

			sdio_buffer_in_print(port, packet);
			
			list_del(&packet->node);
			if(packet){
				port->sdio_buf_in_size -= packet->size;
				kfree(packet->buffer);
				kfree(packet);
			}
			port->sdio_buf_in_num--;
		}
	}
	tty_kref_put(tty);
}

/*****************************************************************************
 * tty driver interface functions
 *****************************************************************************/
/**
 *	sdio_uart_install	-	install method
 *	@driver: the driver in use (sdio_uart in our case)
 *	@tty: the tty being bound
 *
 *	Look up and bind the tty and the driver together. Initialize
 *	any needed private data (in our case the termios)
 */

static int modem_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct sdio_modem_port *port ;
	int idx = tty->index;
	int ret;

	port = sdio_modem_port_get(idx);
	LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
	if (!port) {
		tty->driver_data = NULL;
		LOGPRT(LOG_ERR,  "%s %d can't find sdio modem port.\n", __func__, __LINE__);
		return -ENODEV;
	}
	
	kref_get(&port->kref);
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
			ret = tty_port_install(&port->port,driver, tty);
#else			
			ret = tty_standard_install(driver, tty);
#endif

	if (ret == 0)
		/* This is the ref sdio_uart_port get provided */
		tty->driver_data = port;
	else
		sdio_modem_port_put(port);
	return ret;
}

/**
 *	sdio_uart_cleanup	-	called on the last tty kref drop
 *	@tty: the tty being destroyed
 *
 *	Called asynchronously when the last reference to the tty is dropped.
 *	We cannot destroy the tty->driver_data port kref until this point
 */

static void modem_tty_cleanup(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	tty->driver_data = NULL;	/* Bug trap */
	if(port){
		LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
		sdio_modem_port_put(port);
	}else{
		LOGPRT(LOG_NOTICE,  "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static int modem_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct sdio_modem_port *port = tty->driver_data;
	LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
	return tty_port_open(&port->port, tty, filp);
}

static void modem_tty_close(struct tty_struct *tty, struct file * filp)
{
	struct sdio_modem_port *port = tty->driver_data;
	
	LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_close(&port->port, tty, filp);
}

static void modem_tty_hangup(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	LOGPRT(LOG_NOTICE,  "%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_hangup(&port->port);
}

static int modem_tty_write(struct tty_struct *tty, const unsigned char *buf,
		int count)
{
	struct sdio_modem_port *port = tty->driver_data;
	unsigned long flags;
	int ret=0;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		return ret;
	}

	if(port->inception){
		return -EBUSY;
	}

	if(count > FIFO_SIZE){
		LOGPRT(LOG_ERR,  "%s %d FIFO size is not enough!\n", __func__,__LINE__);
		return -1;
	}
	
	spin_lock_irqsave(&modem_remove_lock, flags);
	if(modem_remove == 0){
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		ret = kfifo_in_locked(&port->transmit_fifo, buf, count, &port->write_lock);
		queue_work(port->write_q, &port->write_work);
	}
	else{
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		LOGPRT(LOG_NOTICE,  "%s %d: port%d is removed!\n", __func__, __LINE__, port->index);
	}
	
	LOGPRT(LOG_DEBUG,  "%s %d: port%d\n", __func__,__LINE__, port->index);
	
	return ret;
}

static int modem_tty_write_room(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	LOGPRT(LOG_DEBUG, "%s %d: port %d free size %d.\n", __func__, __LINE__, port->index, data_len);
	return data_len;
}
#if 0
static void modem_tty_flush_chars(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	struct sdio_modem *modem;
	unsigned int count;
	unsigned int left, todo;
	unsigned int write_len;
	unsigned int fifo_size;
	unsigned long flags = 0;
	int ret = 0;

	modem = port->modem;
	if(down_interruptible(&modem->sem)){
		LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}
	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	if(count == 0){
		up(&modem->sem);
		goto down_out;
	}
	
	left = count;
	do{
		todo = left;
		if(todo > TRANSMIT_MAX_SIZE - 1){
			todo = TRANSMIT_MAX_SIZE;
		}else if (todo > 508){
			todo = 508;
		}

		*modem->trans_buffer = 0xFE;
		*(modem->trans_buffer + 1) = 0x0F & (port->index + 1);
		*(modem->trans_buffer + 2) = 0x0F & (todo >> 8);
		*(modem->trans_buffer + 3) = 0xFF & todo;

		fifo_size = kfifo_out_locked(&port->transmit_fifo, modem->trans_buffer + 4, todo, &port->write_lock);
		if(todo != fifo_size){
			LOGPRT(LOG_ERR,  "%s %d: port%d todo !=  kfifo lock out size.\n", __func__, __LINE__,port->index);
			todo = fifo_size;
		}

		write_len = (todo + 4 + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_sdio_write(modem, 0x00, modem->trans_buffer, write_len);
		left -= todo;
	}while(left);
	up(&modem->sem);

down_out:
	//for compile warning
	LOGPRT(LOG_DEBUG,  "%s %d: port%d.\n", __func__,__LINE__, port->index);
	ret = ret;
}
#endif
static int modem_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	LOGPRT(LOG_DEBUG, "%s %d: port %d chars in buffer %d.\n", __func__, __LINE__, port->index, data_len);
	return data_len;
}

static void modem_tty_set_termios(struct tty_struct *tty,
		struct ktermios *old_termios)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d ret=%d\n", __func__,__LINE__,ret);
		return ;
	}
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 100))
  tty_termios_copy_hw(&tty->termios, old_termios);
#else
	tty_termios_copy_hw(tty->termios, old_termios);
#endif
}

static int modem_tty_tiocmget(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}
	return 0;
}

static int modem_tty_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d ret=%d\n", __func__,__LINE__,ret);
		return ret;
	}
	return 0;
}

static int sdio_modem_activate(struct tty_port *tport, struct tty_struct *tty)
{
	struct sdio_modem_port *port = NULL;

	LOGPRT(LOG_NOTICE,  "%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct sdio_modem_port, port);

	kfifo_reset(&port->transmit_fifo);
	mutex_lock(&port->sdio_buf_in_mutex);
	if (port->sdio_buf_in == 1){
		sdio_buf_in_tty_work(port);
		port->sdio_buf_in = 0;
	}
	mutex_unlock(&port->sdio_buf_in_mutex);
	LOGPRT(LOG_NOTICE,  "%s %d: Leave.\n", __func__, __LINE__);
	return 0;
}

static void sdio_modem_shutdown(struct tty_port *tport)
{
	struct sdio_modem_port *port = NULL;
	struct sdio_buf_in_packet *packet = NULL;

	LOGPRT(LOG_NOTICE,  "%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct sdio_modem_port, port);
	mutex_lock(&port->sdio_buf_in_mutex);
	while(!list_empty(&port->sdio_buf_in_list)){
		packet = list_first_entry(&port->sdio_buf_in_list, struct sdio_buf_in_packet, node);
		list_del(&packet->node);
		if(packet){
			kfree(packet->buffer);
			kfree(packet);
		}
	}
	mutex_unlock(&port->sdio_buf_in_mutex);
	LOGPRT(LOG_NOTICE,  "%s %d: Leave.\n", __func__, __LINE__);
}


static const struct tty_port_operations sdio_modem_port_ops = {
	.shutdown = sdio_modem_shutdown,
	.activate = sdio_modem_activate,
};

static const struct tty_operations modem_tty_ops = {
	.open			= modem_tty_open,
	.close			= modem_tty_close,
	.write			= modem_tty_write,
	.write_room		= modem_tty_write_room,
	.chars_in_buffer	= modem_tty_chars_in_buffer,
	//.flush_chars		= modem_tty_flush_chars,
	.set_termios		= modem_tty_set_termios,
	.tiocmget			= modem_tty_tiocmget,
	.tiocmset			= modem_tty_tiocmset,
	.hangup			= modem_tty_hangup,
	.install			= modem_tty_install,
	.cleanup			= modem_tty_cleanup,
};

static void sdio_port_work_name(const char	*name, int index, char *p)
{
	sprintf(p, "%s%d", name, index);
}

static void sdio_write_port_work(struct work_struct *work)
{
	struct sdio_modem_port *port;
	struct sdio_modem *modem;
	struct tty_struct *tty;
	unsigned int count;
	unsigned int left, todo;
	unsigned int write_len;
	unsigned int fifo_size;
	unsigned long flags = 0;
	int ret = 0;
    
	port = container_of(work, struct sdio_modem_port, write_work);
	modem = port->modem;
	
	if(down_interruptible(&port->write_sem)){
		LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		ret =  -ERESTARTSYS;
		goto down_out;
	}
	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	//for AT command problem of /r;
	if(count == 0){
		up(&port->write_sem);
		goto down_out;
	}
	
	left = count;
	do{
		wait_event(port->sflow_ctrl_wait_q, (SFLOW_CTRL_DISABLE == atomic_read(&port->sflow_ctrl_state)||(cbp_power_state==0)));
		if(down_interruptible(&modem->sem)){
			LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
			ret =  -ERESTARTSYS;
			goto down_sem_fail;
		}

		todo = left;
		if(todo > TRANSMIT_MAX_SIZE - 1){
			todo = TRANSMIT_MAX_SIZE;
		}else if (todo > 508){
			todo = 508;
		}

		*modem->trans_buffer = 0xFE;
		*(modem->trans_buffer + 1) = 0x0F & (port->index + 1);
		*(modem->trans_buffer + 2) = 0x0F & (todo >> 8);
		*(modem->trans_buffer + 3) = 0xFF & todo;

		fifo_size = kfifo_out_locked(&port->transmit_fifo, modem->trans_buffer + 4, todo, &port->write_lock);
		if(todo != fifo_size){
			LOGPRT(LOG_ERR,  "%s %d: port%d todo !=  kfifo lock out size.\n", __func__, __LINE__,port->index);
			todo = fifo_size;
		}

		write_len = (todo + 4 + 3) & ~0x03;  /* Round up to nearest multiple of 4 */
		modem_sdio_write(modem, 0x00, modem->trans_buffer, write_len);
		left -= todo;
		up(&modem->sem);
	}while(left);

	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);
	
	if (count < SDIO_WAKEUP_CHARS) {
		tty = tty_port_tty_get(&port->port);
		if(tty){
			tty_wakeup(tty);
			tty_kref_put(tty);
		}
	}
	
down_sem_fail:
	up(&port->write_sem);
down_out:
	//for compile warning
	ret = ret;
}

/*query modem func's pending irq flag*/
static int modem_irq_query(struct sdio_func *func,unsigned char *pendingirq)
{
	int func_num = 0;
	int ret = 0;

	/* Hack to access Fuction-0 */
	func_num = func->num;
	func->num = 0;

	*pendingirq = sdio_readb(func, SDIO_CCCR_INTx, &ret);
	if (ret){
		LOGPRT(LOG_ERR,  "%s %d: read SDIO_CCCR_INTx err ret= %d\n", __func__,__LINE__,ret);
	}
	func->num = func_num;

	return ret;
}
/*
 * This SDIO interrupt handler.
 */
static void modem_sdio_irq(struct sdio_func *func)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port;
	unsigned char reg = 0;
	int  bytecnt = 0;
	int ret = 0;
	int iir =0;
	int readcnt = 0;
	struct tty_struct *tty;
	unsigned char index = 0;
	unsigned char payload_offset = 0;
	struct sdio_buf_in_packet *packet = NULL;
	unsigned char pending = 0;

	ret = modem_irq_query(func,&pending);
	if (ret) {
		LOGPRT(LOG_ERR,  "%s %d: read SDIO_CCCR_INTx err ret= %d\n", __func__,__LINE__,ret);
		goto err_out;
	}
	if((pending & SDIO_FUNC_1) ==0){
		LOGPRT(LOG_NOTICE2,  "pending=%d ret= %d\n", pending,ret);
		goto out;
	}
	sdio_rx_cnt++;
	modem = sdio_get_drvdata(func);
	do
	{
		/* Reading the IIR register on the slave clears the interrupt. Since host and
		   slave run asynchronously, must ensure int bit is set before reading
		   transfer count register  */
		iir =  sdio_readb(func, 0x04, &ret);
	}while( (iir != 1)  && (readcnt++ <= 10));

	if(ret){
		LOGPRT(LOG_ERR,  "%s %d: read iir err ret= %d\n", __func__,__LINE__,ret);
		goto err_out;
	}

	if(iir != 1)
	{
		LOGPRT(LOG_ERR,  "%s %d error iir value = %d!!!\n", __func__,__LINE__,iir);
		goto out;
	}

	/* Read byte count */
	reg =  sdio_readb(func, 0x08, &ret);
	if(ret){
		LOGPRT(LOG_ERR,  "%s %d: read data cnt err ret= %d\n", __func__,__LINE__,ret);
		goto err_out;
	}

	bytecnt = reg;
	reg =  sdio_readb(func, 0x09, &ret);
	if(ret){
		LOGPRT(LOG_ERR,  "%s %d: read data cnt ret= %d\n", __func__,__LINE__,ret);
		goto err_out;
	}
	bytecnt |= (reg << 8);
	
	if (bytecnt == 0) {
		LOGPRT(LOG_ERR,  "%s %d error read size %d.\n", __func__,__LINE__, bytecnt);
		goto out;
	}
	modem->msg->head.start_flag = 0;
	modem->msg->head.chanInfo = 0;
	modem->msg->head.tranHi = 0;
	modem->msg->head.tranLow = 0;
	memset(modem->msg->buffer, 0, sizeof(modem->msg->buffer));

	if(modem->cbp_data->data_ack_enable){
		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);
	}

	ret = sdio_readsb(func, modem->msg, 0x00, bytecnt);
	if (ret){
		LOGPRT(LOG_ERR,  "%s %d: port%d sdio read with error code = %d, read bytecount = %d!!!\n", 
			__func__, __LINE__, modem->msg->head.chanInfo, ret, bytecnt);
		goto err_out;
	}

	if((modem->msg->head.chanInfo  == SDIO_AT_CHANNEL_NUM) || (sdio_log_level > LOG_NOTICE)) {//
		sdio_tx_rx_printk(modem->msg, 0);
	}
	//sdio_tx_rx_printk(modem->msg, 0);

	if (modem->msg->head.start_flag != 0xFE){
		LOGPRT(LOG_ERR,  "%s %d: start_flag != 0xFE and value is 0x%x, go out.\n",
			__func__, __LINE__, modem->msg->head.start_flag);
		goto out;
	}

	if (modem->msg->head.chanInfo > 0 && modem->msg->head.chanInfo < (SDIO_TTY_NR + 1))
	{   
		index = modem->msg->head.chanInfo -1;
		payload_offset = ((modem->msg->head.tranHi & 0xC0) >> 6);
		if (payload_offset)
		{
			LOGPRT(LOG_DEBUG,  "%s %d: payload_offset = %d.\n",__func__, __LINE__, payload_offset);
		}
		modem->data_length =(((modem->msg->head.tranHi & 0x0F) << 8) |
							(modem->msg->head.tranLow & 0xFF));
		if(modem->data_length == 0){
			LOGPRT(LOG_ERR,  "%s %d: data_length is 0\n",__func__,__LINE__);
			goto out;
		}
		port = modem->port[index];
		ret = check_port(port);
		if (ret < 0)
		{
			LOGPRT(LOG_ERR,  "%s %d: check port error\n",__func__,__LINE__);
			goto out;
		}
		
		if(port->inception){
		       rawbulk_push_upstream_buffer(index,(modem->msg->buffer + payload_offset), (modem->data_length - payload_offset));
	        } else {
			tty = tty_port_tty_get(&port->port);
			if(!tty)
			{
				LOGPRT(LOG_ERR, "tty is NULL");
				mutex_lock(&port->sdio_buf_in_mutex);
				port->sdio_buf_in_size += (modem->data_length - payload_offset);
				if(port->sdio_buf_in_size > SDIO_BUF_IN_MAX_SIZE){
					port->sdio_buf_in_size -= (modem->data_length - payload_offset);
					mutex_unlock(&port->sdio_buf_in_mutex);
					LOGPRT(LOG_ERR,  "%s %d: ttySDIO%d data buffer overrun!\n", __func__,__LINE__, index);
				}
				else{
					packet = kzalloc(sizeof(struct sdio_buf_in_packet), GFP_KERNEL);
					if(!packet){
						LOGPRT(LOG_ERR,  "%s %d: kzalloc packet error\n",__func__,__LINE__);
						ret = -ENOMEM;
						mutex_unlock(&port->sdio_buf_in_mutex);
						tty_kref_put(tty);
						goto wait_ack;
					}
					packet->size = modem->data_length - payload_offset;
					packet->buffer = kzalloc(packet->size, GFP_KERNEL);
					if(!packet->buffer){
						LOGPRT(LOG_ERR,  "%s %d: kzalloc packet buffer error\n",__func__,__LINE__);
						ret = -ENOMEM;
						kfree(packet);
						mutex_unlock(&port->sdio_buf_in_mutex);
						tty_kref_put(tty);
						goto wait_ack;
					}
					memcpy(packet->buffer, (modem->msg->buffer + payload_offset), packet->size);
					
					if(port->sdio_buf_in_num < port->sdio_buf_in_max_num){
						list_add_tail(&packet->node, &port->sdio_buf_in_list);
						port->sdio_buf_in_num++;
					}
					else{
						struct sdio_buf_in_packet *old_packet = NULL;
						old_packet = list_first_entry(&port->sdio_buf_in_list, struct sdio_buf_in_packet, node);
						list_del(&old_packet->node);
						if(old_packet){
							port->sdio_buf_in_size -= old_packet->size;
							kfree(old_packet->buffer);
							kfree(old_packet);
						}
						list_add_tail(&packet->node, &port->sdio_buf_in_list);
					}
					port->sdio_buf_in = 1;
					mutex_unlock(&port->sdio_buf_in_mutex);
					LOGPRT(LOG_ERR,  "%s %d: ttySDIO%d data buffered! \n", __func__,__LINE__, index);
				}
			}

			if (tty && modem->data_length) {
				if(port->sdio_buf_in ==1){
					mutex_lock(&port->sdio_buf_in_mutex);/*make sure data in list bufeer had been pushed to tty buffer*/
					mutex_unlock(&port->sdio_buf_in_mutex);
				}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
				ret = tty_buffer_request_room(&port->port, (modem->data_length - payload_offset));
#else			
				ret = tty_buffer_request_room(tty, (modem->data_length - payload_offset));
#endif				
				if(ret < (modem->data_length - payload_offset)){
					LOGPRT(LOG_ERR,  "%s %d: ttySDIO%d no room in tty rx buffer!\n", __func__,__LINE__, index);
				}
				else{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
				ret = tty_insert_flip_string(&port->port, (modem->msg->buffer + payload_offset), (modem->data_length - payload_offset));
#else			
				ret = tty_insert_flip_string(tty, (modem->msg->buffer + payload_offset), (modem->data_length - payload_offset));
#endif	
					if(ret < (modem->data_length - payload_offset)){
						LOGPRT(LOG_ERR,  "%s %d: ttySDIO%d couldn't insert all characters (TTY is full?)!\n", __func__,__LINE__, index);
					}
					else{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))					
						tty_flip_buffer_push(&port->port);
#else
						tty_flip_buffer_push(tty);
#endif						
					}
				}
			}
			tty_kref_put(tty);
		}
	}
	else if(modem->msg->head.chanInfo ==0)/*control message analyze*/
	{
		ctrl_msg_analyze(modem);
	}
	else{
		LOGPRT(LOG_ERR,  "%s %d: error chanInfo is %d, go out.\n",
			__func__, __LINE__, modem->msg->head.chanInfo);
		goto out;
	}
wait_ack:
	//LOGPRT(LOG_ERR,  "%s %d: port%d data ack before!\n", __func__, __LINE__, port->index);
	if(modem->cbp_data->data_ack_enable){
		modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
	}
	//LOGPRT(LOG_ERR,  "%s %d: port%d data ack after!\n", __func__, __LINE__, port->index);
out:
	return;

err_out:
	LOGPRT(LOG_ERR,  "%s %d: let cbp die now.\n",__func__, __LINE__);
	modem_err_indication_usr(1);
	return;
}

static int func_enable_irq(struct sdio_func *func, int enable)
{
	int func_num = 0;
	u8 cccr = 0;
	int ret = 0;

	/* Hack to access Fuction-0 */
	func_num = func->num;
	func->num = 0;

	cccr = sdio_readb(func, SDIO_CCCR_IENx, &ret);
	if (WARN_ON(ret))
		goto set_func;

	if (enable){
		/* Master interrupt enable ... */
		cccr |= BIT(0);
		/* ... for our function */
		cccr |= BIT(func_num);
	}
	else{
		/* Master interrupt enable ... */
		cccr &= ~(BIT(0));
		/* ... for our function */
		cccr &= ~(BIT(func_num));
	}

	sdio_writeb(func, cccr, SDIO_CCCR_IENx, &ret);
	if (WARN_ON(ret))
		goto set_func;

	/* Restore the modem function number */
	func->num = func_num;
	return 0;

set_func:
	func->num = func_num;
	return ret;
}
static int modem_fc_flag = 0;
static void modem_sdio_write(struct sdio_modem *modem, int addr,
		void *buf, size_t len)
{
	struct sdio_func *func = modem->func;
	struct mmc_host *host = func->card->host;
	unsigned char *print_buf = (unsigned char *)buf;
	unsigned char index = *(print_buf + 1);
	int err_flag = 0;
	int ret;

	sdio_tx_cnt++;

	if(modem->cbp_data->flow_ctrl_enable){
		if(oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio)!=modem->cbp_data->gpio_flow_ctrl_polar){
			if(FLOW_CTRL_ENABLE == atomic_read(&modem->cbp_data->cbp_flow_ctrl->state))
				atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		}else{
			while(1){/*print added for testing,to be removed*/
				if(cbp_power_state==0){
					LOGPRT(LOG_ERR,  "%s %d: card is removed when channel%d flow is enable,data is dropped\n", __func__, __LINE__, index);
					sdio_tx_rx_printk(buf, 1);
					goto terminate;
				}

				if(modem_fc_flag < MODEM_FC_PRINT_MAX)
					LOGPRT(LOG_ERR,  "%s %d: channel%d flow ctrl before!\n", __func__, __LINE__, index);
				atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_ENABLE);
				modem->cbp_data->flow_ctrl_wait_event(modem->cbp_data->cbp_flow_ctrl);
				if(modem_fc_flag < MODEM_FC_PRINT_MAX){
					LOGPRT(LOG_ERR,  "%s %d: channel%d flow ctrl after!\n", __func__, __LINE__, index);
					modem_fc_flag++;
				}
				if(oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio)!=modem->cbp_data->gpio_flow_ctrl_polar){
					LOGPRT(LOG_ERR,  "%s %d: channel%d flow ctrl ok!\n", __func__, __LINE__, index);
					atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
					modem_fc_flag = 0;
					break;
				}
			}
		}
	}

	if(modem->cbp_data->ipc_enable){
		asc_tx_auto_ready(modem->cbp_data->tx_handle->name, 1);
	}
	if(cbp_power_state==0){
		LOGPRT(LOG_ERR,  "%s %d: card is removed when channel%d flow is enable,data is dropped\n", __func__, __LINE__, index);
		sdio_tx_rx_printk(buf, 1);
		goto terminate;
	}
	sdio_claim_host(func);
	
	if(modem->cbp_data->tx_disable_irq){
		ret = func_enable_irq(func, 0);
		if (ret){
			LOGPRT(LOG_ERR,  "%s %d: channel%d func_disable_irq failed ret=%d\n", __func__, __LINE__, index,ret);
			err_flag =1;
			goto release_host;
		}
	}

	if(modem->cbp_data->data_ack_enable){
		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_TX_RX);
	}

	if((index != 3) || (sdio_log_level > LOG_NOTICE)) {//(index == SDIO_AT_CHANNEL_NUM) || 
		sdio_tx_rx_printk(buf, 1);
	}
	//sdio_tx_rx_printk(buf, 1);

	ret = sdio_writesb(func, addr, buf, len);
	if (ret){
		LOGPRT(LOG_ERR,  "%s %d: channel%d failed ret=%d\n", __func__, __LINE__, index,ret);
		err_flag =1;
		goto release_host;
	}

	//LOGPRT(LOG_ERR,  "%s %d: channel%d data ack before!\n", __func__, __LINE__, index);
	if(modem->cbp_data->data_ack_enable){
		modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
	}
	//LOGPRT(LOG_ERR,  "%s %d: channel%d data ack after!\n", __func__, __LINE__, index);

	if(modem->cbp_data->tx_disable_irq){
		ret = func_enable_irq(func, 1);
		if (ret){
			LOGPRT(LOG_ERR,  "%s %d: channel%d func_enable_irq failed ret=%d\n", __func__, __LINE__, index,ret);
			err_flag =1;
		}
	}
release_host:
	sdio_release_host(func);
	if(err_flag!=0){
		LOGPRT(LOG_ERR,  "%s %d: channel%d ret =%d signal err to user space\n", __func__, __LINE__, index,ret);
		modem_err_indication_usr(1);
	}
terminate:
		return;
}

static void modem_port_remove(struct sdio_modem *modem)
{
	struct sdio_modem_port *port;
	struct tty_struct *tty;
	unsigned long flags = 0;
	int index;
	LOGPRT(LOG_NOTICE,  "%s %d: Enter.\n", __func__, __LINE__);

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
		wake_up(&port->sflow_ctrl_wait_q);
		atomic_set(&modem->ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
		wake_up(&modem->ctrl_port->sflow_ctrl_wait_q);
		atomic_set(&modem->cbp_data->cbp_flow_ctrl->state, FLOW_CTRL_DISABLE);
		wake_up(&modem->cbp_data->cbp_flow_ctrl->wait_q);
		atomic_set(&modem->cbp_data->cbp_data_ack->state, MODEM_ST_READY);
		wake_up(&modem->cbp_data->cbp_data_ack->wait_q);
		if(port->write_q){
			LOGPRT(LOG_NOTICE,	"%s %d: port%d cancel_work_sync before.\n", __func__, __LINE__, index);
			cancel_work_sync(&port->write_work);
			LOGPRT(LOG_NOTICE,	"%s %d: port%d cancel_work_sync after.\n", __func__, __LINE__, index);
			destroy_workqueue(port->write_q);
			LOGPRT(LOG_NOTICE,	"%s %d: port%d destroy queue after.\n", __func__, __LINE__, index);
		}
		BUG_ON(sdio_modem_table[port->index] != port);

		LOGPRT(LOG_NOTICE,	"%s %d: sdio_modem_table cleared.\n", __func__, __LINE__);
		mutex_lock(&port->port.mutex);
		port->func = NULL;
		tty = tty_port_tty_get(&port->port);
		/* tty_hangup is async so is this safe as is ?? */
		if (tty) {
			LOGPRT(LOG_NOTICE,"%s %d destory tty,index=%d port->index=%d\n",__func__,__LINE__,index,port->index);
			tty_hangup(tty);
			tty_kref_put(tty);
		}
		mutex_unlock(&port->port.mutex);

		sdio_modem_port_put(port);
	}
	LOGPRT(LOG_NOTICE,  "%s %d: Leave.\n", __func__, __LINE__);
}

static void sdio_buffer_in_set_max_len(struct sdio_modem_port *port)
{
	unsigned int	index = port->index;
	switch(index)
	{
		case 0:
			port->sdio_buf_in_max_num = SDIO_PPP_BUF_IN_MAX_NUM;
			break;
		case 1:
			port->sdio_buf_in_max_num = SDIO_ETS_BUF_IN_MAX_NUM;
			break;
		case 2:
			port->sdio_buf_in_max_num = SDIO_IFS_BUF_IN_MAX_NUM;
			break;
		case 3:
			port->sdio_buf_in_max_num = SDIO_AT_BUF_IN_MAX_NUM;
			break;
		case 4:
			port->sdio_buf_in_max_num = SDIO_PCV_BUF_IN_MAX_NUM;
			break;
		default:
			port->sdio_buf_in_max_num = SDIO_DEF_BUF_IN_MAX_NUM;
			break;
	}
}

static int sdio_modem_port_init(struct sdio_modem_port *port, int index)
{
	int ret = 0;
	unsigned long flags = 0;

	kref_init(&port->kref);
	spin_lock_init(&port->write_lock);
	
	if (kfifo_alloc(&port->transmit_fifo, FIFO_SIZE, GFP_KERNEL)){
		LOGPRT(LOG_ERR,  "%s %d : Couldn't allocate transmit_fifo\n",__func__,__LINE__);
		return -ENOMEM;
	}
	
	/* create port's write work queue */
	port->name = "modem_sdio_write_wq";
	sdio_port_work_name(port->name, index, port->work_name);
	port->write_q = create_singlethread_workqueue(port->work_name);
	if (port->write_q == NULL) {
		LOGPRT(LOG_ERR,  "%s %d error creat write workqueue \n",__func__, __LINE__);
		return -ENOMEM;
	}
	INIT_WORK(&port->write_work, sdio_write_port_work);

	spin_lock_irqsave(&sdio_modem_table_lock, flags);
	if (!sdio_modem_table[index]) {
		port->index = index;
		sdio_modem_table[index] = port;
	}
	spin_unlock_irqrestore(&sdio_modem_table_lock, flags);

	mutex_init(&port->sdio_buf_in_mutex);
	INIT_LIST_HEAD(&port->sdio_buf_in_list);
	port->sdio_buf_in = 0;
	port->sdio_buf_in_num = 0;
	port->sdio_buf_in_size = 0;
	sdio_buffer_in_set_max_len(port);
	
	init_waitqueue_head(&port->sflow_ctrl_wait_q);
	atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	sema_init(&port->write_sem, 1);

	return ret;
}

static ssize_t modem_log_level_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
    char *s = buf;
    s += sprintf(s, "%d\n", sdio_log_level);

    return (s - buf);
}

static ssize_t modem_log_level_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
    unsigned long val;

    if (strict_strtoul(buf, 10, &val))
        return -EINVAL;

    if (val < 0)
	return -EINVAL;

    sdio_log_level = val;

    return n;
}

static ssize_t modem_refer_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
    char *s = buf;
    s += sprintf(s, "Tx:  times %d\n", sdio_tx_cnt);
    s += sprintf(s, "\n");
    s += sprintf(s, "Rx:  times %d\n", sdio_rx_cnt);

    return (s - buf);
}

static ssize_t modem_refer_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
    return n;
}

static ssize_t modem_ctrl_on_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
   struct sdio_modem_ctrl_port *ctrl_port;
   struct sdio_modem *modem;
   struct sdio_modem_port *port;
   char *s = buf;
   int ret=-1;

   LOGPRT(LOG_NOTICE,  "%s: enter\n", __func__);

   port = sdio_modem_port_get(0);
   ret = check_port(port);
   if (ret < 0){
      LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
      goto out;
   }
   modem = port->modem;
   ctrl_port = modem->ctrl_port;
out:
   s += sprintf(s, "ctrl state: %s\n", ctrl_port->chan_state ?"enable":"disable");
   return (s - buf);
}

static ssize_t modem_ctrl_on_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val){
		modem_on_off_ctrl_chan(1);
	}
	else{
		modem_on_off_ctrl_chan(0);
	}
	return n;
}
static ssize_t modem_dtr_send_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
   char query_mode=1;
   int status=-1,ret=-1;
   char *s = buf;

   LOGPRT(LOG_NOTICE,  "%s: enter\n", __func__);

   status=modem_dcd_state();
   if(ret <0){
      LOGPRT(LOG_NOTICE,	"query cp ctrl channel state failed ret=%d\n",ret);
   }
   s += sprintf(s, "ctrl state: %d\n", status);

   return (s - buf);
}

static ssize_t modem_dtr_send_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
	unsigned int val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	dtr_value = val;
	modem_dtr_set(val, 1);

	return n;
}

static ssize_t modem_dtr_query_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
   char query_mode=1;
   int status=-1,ret=-1;
   char *s = buf;

   LOGPRT(LOG_NOTICE,  "%s: enter\n", __func__);

   status = modem_dcd_state();
   if(status <0){
      LOGPRT(LOG_NOTICE,	"query cp ctrl channel state failed ret=%d\n",ret);
      s += sprintf(s, "ctrl state: %s\n","N/A" );
   }else{
      s += sprintf(s, "ctrl state: %d\n", status);
   }
   return (s - buf);
}

static ssize_t modem_dtr_query_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
	unsigned long val;
	int data;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	modem_dcd_state();	
	return n;
}

static unsigned char loop_back_chan = 0;
static ssize_t modem_loop_back_chan_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_chan_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;
	if (val < 6){
		loop_back_chan = val;
	}
	else{
		LOGPRT(LOG_ERR,  "%s %d error channel select, please < 6!\n", __func__,__LINE__);
	}
	
	return n;
}

static ssize_t modem_loop_back_mod_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_mod_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	
	unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val < 4){
		modem_loop_back_chan(loop_back_chan, val);
	}
	else{
		LOGPRT(LOG_ERR,  "%s %d error channel select, please check the option!\n", __func__,__LINE__);
	}
	return n;
}

static ssize_t modem_ack_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
    char *s = buf;
   struct sdio_modem *modem;
   struct sdio_modem_port *port;
   int ret =-1;
  
   if((cbp_pdata!=NULL) && (cbp_pdata->cbp_data_ack!=NULL )){
      s += sprintf(s, "gpio[%d]\t state:[%d]\t polar[%d]\t ", 
	  	cbp_pdata->cbp_data_ack->wait_gpio ,atomic_read(&cbp_pdata->cbp_data_ack->state),cbp_pdata->cbp_data_ack->wait_polar);
      port = sdio_modem_port_get(0);
      ret = check_port(port);
      if (ret < 0){
         LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
         goto out;
      }
      modem = port->modem;
      s += sprintf(s, "stored:[%d]\n", oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio));
   }
out:
    return (s - buf);
}

static ssize_t modem_ack_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	

    return n;
}

static ssize_t modem_flw_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
   char *s = buf;
   struct sdio_modem *modem;
   struct sdio_modem_port *port;
   int ret =-1;

   if((cbp_pdata!=NULL) && (cbp_pdata->cbp_flow_ctrl!=NULL )){
      s += sprintf(s, "gpio[%d] \tstate:[%d]\t polar[%d]\t ",
	  	cbp_pdata->cbp_flow_ctrl->wait_gpio,atomic_read(&cbp_pdata->cbp_flow_ctrl->state),cbp_pdata->cbp_flow_ctrl->wait_polar);
      port = sdio_modem_port_get(0);
      ret = check_port(port);
      if (ret < 0){
         LOGPRT(LOG_ERR,  "%s %d check_port failed\n", __func__,__LINE__);
         goto out;
      }
      modem = port->modem;
      s += sprintf(s, "stored:[%d]\n", oem_gpio_get_value(modem->cbp_data->cbp_flow_ctrl->wait_gpio));
   }
out:
    return (s - buf);
}

static ssize_t modem_flw_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t n)
{	

    return n;
}


#define modem_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0660,			\
	},					\
	.show	= modem_##_name##_show,			\
	.store	= modem_##_name##_store,		\
}
modem_attr(log_level);
modem_attr(refer);
modem_attr(ctrl_on);
modem_attr(dtr_send);
modem_attr(dtr_query);
modem_attr(loop_back_chan);
modem_attr(loop_back_mod);
modem_attr(ack);
modem_attr(flw);


static struct attribute * modem_sdio_attr[] = {
	&log_level_attr.attr,
	&refer_attr.attr,
	&ctrl_on_attr.attr,
	&dtr_send_attr.attr,
	&dtr_query_attr.attr,
	&loop_back_chan_attr.attr,
	&loop_back_mod_attr.attr,
	&ack_attr.attr,
	&flw_attr.attr,
	NULL,
};
static struct kobject *modem_sdio_kobj;
static struct attribute_group g_modem_attr_group = {
	.attrs =modem_sdio_attr,
};

int sdio_rawbulk_intercept(int port_num, unsigned int inception) {
    int ret = -ENODEV;
    
    if(port_num >= 2)
		port_num++; 
    struct sdio_modem_port *port = sdio_modem_port_get(port_num);
  
	if (!port || !port->func){
		LOGPRT(LOG_ERR,  "%s %d failed\n", __FUNCTION__,__LINE__);
		return ret;
	}
	LOGPRT(LOG_DEBUG, "modem inception = %d\n", inception);
    spin_lock(&port->inception_lock);
    if ((!!inception) == port->inception) {
        spin_unlock(&port->inception_lock);
        return 0;
    }
    spin_unlock(&port->inception_lock);
	
    spin_lock(&port->inception_lock);
    if (inception != port->inception)
          port->inception = !!inception;
    spin_unlock(&port->inception_lock);

    return 0;
}
EXPORT_SYMBOL_GPL(sdio_rawbulk_intercept);

int modem_buffer_push(int port_num, void *buf, int count)
{
	int ret, data_len, flags;

	if(port_num >= 2)
		port_num++;   
	struct sdio_modem_port *port = sdio_modem_port_get(port_num);
  
	ret = check_port(port);
	if (ret < 0){
		LOGPRT(LOG_ERR,  "%s %d invalid port\n", __FUNCTION__,__LINE__);
		return ret;
	}
	
    if (count == 0)
        return 0;

    spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);
    if(data_len < count) {
        LOGPRT(LOG_DEBUG, "%s %d: SDIO driver buffer is full!\n", __FUNCTION__,__LINE__);
        return -ENOMEM;
    } 

	spin_lock_irqsave(&modem_remove_lock, flags);
	if(modem_remove == 0){
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		ret = kfifo_in_locked(&port->transmit_fifo, buf, count, &port->write_lock);
		queue_work(port->write_q, &port->write_work);
	}
	else{
		spin_unlock_irqrestore(&modem_remove_lock, flags);
		LOGPRT(LOG_NOTICE,  "%s %d: port%d is removed!\n", __func__, __LINE__, port->index);
	}
	
	return 0;
}
EXPORT_SYMBOL_GPL(modem_buffer_push);
extern void SRC_trigger_signal(int i_on);

static int modem_sdio_probe(struct sdio_func *func,
		const struct sdio_device_id *id)
{
	struct sdio_modem *modem = NULL;
	struct sdio_modem_port *port = NULL;
	int ret = 0;
	int index = 0;
    unsigned long flags;
	printk("%s %d\n",__func__,__LINE__);
	LOGPRT(LOG_NOTICE,  "%s %d: enter.\n", __func__, __LINE__);
	
	modem= via_modem;
	modem->func = func;
	sdio_set_drvdata(func, modem);

	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	if (ret){
		LOGPRT(LOG_ERR,  "%s %d sdio enable func failed with ret = %d\n",__func__, __LINE__, ret);
		goto err_enable_func;
	}

	ret = sdio_set_block_size(func, 512);
	if (ret){
		LOGPRT(LOG_ERR,  "%s %d: set block size failed with ret = %d\n",__func__, __LINE__, ret);
		goto error_set_block_size;
	}

	sdio_writeb(func, 0x01, 0x28, &ret);
	if(ret){
		LOGPRT(LOG_ERR,  "%s %d: sdio_writeb 0x28 failed with ret = %d\n",__func__, __LINE__, ret);
		goto error_set_block_size;
	}

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		port->func = func;
		//LOGPRT(LOG_INFO,  "%s %d %d port(0x%x), func(0x%x).\n",__func__,__LINE__, index, port, port->func);
	}

	ret = sdio_claim_irq(func, modem_sdio_irq);
	if (ret) 
	{
		LOGPRT(LOG_ERR,  "%s %d sdio claim irq failed.\n",__func__,__LINE__);
		goto err_sdio_claim_irq;
	}
	
	
	LOGPRT(LOG_NOTICE,  "%s %d: exit.\n", __func__, __LINE__);
	sdio_release_host(func);
	spin_lock_irqsave(&modem_remove_lock, flags);
	modem_remove = 0;
	spin_unlock_irqrestore(&modem_remove_lock, flags);
	cbp_power_state = 1;
	for (index = 0; index < SDIO_TTY_NR; index++) {
		rawbulk_bind_sdio_channel(index);
	}
	
	SRC_trigger_signal(1);
	//VIA_trigger_signal(2);
	return ret;

err_sdio_claim_irq:
error_set_block_size:
	sdio_disable_func(func);
err_enable_func:
	sdio_release_host(func);
	return ret;
}

void modem_reset_handler(void)
{
	struct sdio_modem *modem = via_modem;
	struct sdio_modem_port *port;
	//struct sdio_func *func = modem->func;
	struct sdio_func *func = NULL;
	int index;
	int ret=-1;
	unsigned long flags;
	
	LOGPRT(LOG_NOTICE,  "%s %d: Enter.\n", __func__, __LINE__);
	if(modem == NULL){
		LOGPRT(LOG_NOTICE,  "%s %d: modem is NULL.\n", __func__, __LINE__);
		goto out;
	}
	asc_tx_reset(modem->cbp_data->tx_handle->name);
	func = modem->func;

	for (index= 0; index< SDIO_TTY_NR; index++) {
		port = modem->port[index];
		LOGPRT(LOG_NOTICE,  "%s %d: port%d rawbulk_unbind_sdio_channel before.\n", __func__, __LINE__, index);
		rawbulk_unbind_sdio_channel(index);
		LOGPRT(LOG_NOTICE,  "%s %d: port%d rawbulk_unbind_sdio_channel after.\n", __func__, __LINE__, index);
	}
	
	spin_lock_irqsave(&modem_remove_lock, flags);
	modem_remove = 1;
	spin_unlock_irqrestore(&modem_remove_lock, flags);
	LOGPRT(LOG_NOTICE,  "%s %d: cancel_work_sync(&dtr_work) before.\n", __func__, __LINE__);
	cancel_work_sync(&dtr_work);
	LOGPRT(LOG_NOTICE,  "%s %d: cancel_work_sync(&dtr_work) after.\n", __func__, __LINE__);
	cancel_work_sync(&dcd_query_work);
	LOGPRT(LOG_NOTICE,  "%s %d: cancel_work_sync(&dcd_query_work) after.\n", __func__, __LINE__);
	dcd_state = 0;

	//modem_port_remove(modem);

	sdio_claim_host(func);
	ret = sdio_disable_func(func);
	if (ret <0) {
		LOGPRT(LOG_ERR,  "%s: sdio_disable_func failed.\n",__func__);
	}
	ret = sdio_release_irq(func);
	if (ret <0) {
		LOGPRT(LOG_ERR,  "%s: sdio_release_irq failed.\n",__func__);
	}
	sdio_release_host(func);
out:
	LOGPRT(LOG_NOTICE,  "%s %d: Leave.\n", __func__, __LINE__);
}
EXPORT_SYMBOL_GPL(modem_reset_handler);

static void modem_sdio_remove(struct sdio_func *func)
{
	struct sdio_modem *modem = sdio_get_drvdata(func);

	LOGPRT(LOG_NOTICE,  "%s %d: Enter.\n", __func__, __LINE__);
    
	//sysfs_remove_group(modem_sdio_kobj, &g_modem_attr_group);
	//kobject_put(modem_sdio_kobj);
	//c2k_modem = NULL;
	//kfree(modem->trans_buffer);
	//kfree(modem->ctrl_port);
	//kfree(modem->msg);
	//kfree(modem);
	LOGPRT(LOG_NOTICE,  "%s %d: Leave.\n", __func__, __LINE__);
}

#define SDIO_VENDOR_ID_CBP		0x0296
#define SDIO_DEVICE_ID_CBP		0x5347

static const struct sdio_device_id modem_sdio_ids[] = {
	{ SDIO_DEVICE(SDIO_VENDOR_ID_CBP, SDIO_DEVICE_ID_CBP) }, /* VIA-Telecom CBP */
	{} /* Terminating entry */
};

MODULE_DEVICE_TABLE(sdio, modem_sdio_ids);

static struct sdio_driver modem_sdio_driver = {
	.probe		= modem_sdio_probe,
	.remove		= modem_sdio_remove,
	.name		= "modem_sdio",
	.id_table		= modem_sdio_ids,
};

int  modem_sdio_init(struct cbp_platform_data *pdata)
{
	int ret;
	struct tty_driver *tty_drv;
	struct sdio_modem *modem = NULL;
	struct sdio_modem_port *port = NULL;
	int index = 0;
	printk("%s %d: Enter.\n", __func__, __LINE__);
	modem_sdio_tty_driver = tty_drv = alloc_tty_driver(SDIO_TTY_NR);
	cbp_pdata = pdata;
	if (!tty_drv)
		return -ENOMEM;

	tty_drv->owner = THIS_MODULE;
	tty_drv->driver_name = "modem_sdio";
	tty_drv->name =   "ttySDIO";
	tty_drv->major = 0;  // dynamically allocated 
	tty_drv->minor_start = 0;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
       tty_drv->init_termios = tty_std_termios;
       tty_drv->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
       tty_drv->init_termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);   
       tty_drv->init_termios.c_cflag |= CREAD | CLOCAL | CS8 ;
       tty_drv->init_termios.c_cflag &= ~(CRTSCTS);
       tty_drv->init_termios.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
       tty_drv->init_termios.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
       tty_drv->init_termios.c_iflag &= ~(IXON | IXOFF);
       tty_drv->init_termios.c_oflag &= ~(OPOST | OCRNL);
       tty_drv->init_termios.c_ispeed = 9600;
       tty_drv->init_termios.c_ospeed = 9600;
       tty_set_operations(tty_drv, &modem_tty_ops);
	
	ret = tty_register_driver(tty_drv);
	if (ret) {
		LOGPRT(LOG_ERR,  "%s: tty_register_driver failed.\n", __func__);
		goto exit_reg_driver;
	}

	ret = sdio_register_driver(&modem_sdio_driver);
	if (ret) {
		LOGPRT(LOG_ERR,  "%s: sdio_register_driver failed.\n",__func__);
		goto exit_tty;
	}
	spin_lock_init(&modem_remove_lock);
	
	modem = kzalloc(sizeof(struct sdio_modem), GFP_KERNEL);
	if (!modem){
		LOGPRT(LOG_ERR,  "%s %d kzalloc sdio_modem failed.\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_sdio_modem;	
	}
	
	modem->ctrl_port = kzalloc(sizeof(struct sdio_modem_ctrl_port), GFP_KERNEL);
	if (!modem->ctrl_port) {
		LOGPRT(LOG_ERR,  "%s %d kzalloc ctrl_port failed \n",__func__, __LINE__);
		ret =  -ENOMEM;
		goto err_kzalloc_ctrl_port;
	}
		
	modem->msg = kzalloc(sizeof(struct sdio_msg), GFP_KERNEL);
	if (!modem->msg){
		LOGPRT(LOG_ERR,  "%s %d kzalloc sdio_msg failed \n",__func__, __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_sdio_msg;
	}
	
	modem->trans_buffer = kzalloc(TRANSMIT_BUFFER_SIZE, GFP_KERNEL);
	if (!modem->trans_buffer) {
		LOGPRT(LOG_ERR,  "%s %d kzalloc trans_buffer failed \n",__func__, __LINE__);
		ret =  -ENOMEM;
		goto err_kzalloc_trans_buffer;
	}

	sema_init(&modem->sem, 1);
	
	modem->cbp_data = cbp_pdata;
	modem->ctrl_port->chan_state = 0;
	init_waitqueue_head(&modem->ctrl_port->sflow_ctrl_wait_q);
	atomic_set(&modem->ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	INIT_WORK(&dtr_work, modem_dtr_send);
	INIT_WORK(&dcd_query_work, modem_dcd_query);

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = kzalloc(sizeof(struct sdio_modem_port), GFP_KERNEL);
		if (!port) 
		{
			LOGPRT(LOG_ERR,  "%s %d kzalloc sdio_modem_port %d failed.\n",
				__func__, __LINE__, index);
			ret = -ENOMEM;
			goto err_kazlloc_sdio_modem_port;
		}
		//printk("[MODEM SDIO] %s index[%d] 0x%x\n", __FUNCTION__, index, port);
		tty_port_init(&port->port);
		port->port.ops = &sdio_modem_port_ops;
		port->modem = modem;
		modem->port[index] = port;        
		spin_lock_init(&port->inception_lock);
		port->inception = false;
	}

	for (index = 0; index < SDIO_TTY_NR; index++) 
	{
		port = modem->port[index];
		ret = sdio_modem_port_init(port, index);
		if (ret){
			LOGPRT(LOG_ERR,  "%s %d sdio add port failed.\n",__func__, __LINE__);
			goto err_sdio_modem_port_init;
		}
		else {
			struct device *dev;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))					
			dev = tty_port_register_device(&port->port, modem_sdio_tty_driver,
																			port->index, NULL);
#else
			dev = tty_register_device(modem_sdio_tty_driver,
																port->index, NULL);
#endif			
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOGPRT(LOG_ERR,  "%s %d tty register failed \n",__func__,__LINE__);
				goto err_sdio_modem_port_init;
			}
		}
	}

	modem_sdio_kobj = viatel_kobject_add("modem_sdio");
	if (!modem_sdio_kobj){
		ret = -ENOMEM;
		goto err_create_kobj;
	}
	sysfs_create_group(modem_sdio_kobj, &g_modem_attr_group);
		
	via_modem = modem;
	
	LOGPRT(LOG_INFO,  " %s: sdio driver is initialized!\n",__func__);
	printk("%s %d: Exit.\n", __func__, __LINE__);
	return ret;

err_create_kobj:
err_sdio_modem_port_init:
	modem_port_remove(modem);
err_kazlloc_sdio_modem_port:
	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		if(port){
			kfree(port);
		}
	}

err_kzalloc_trans_buffer:
	kfree(modem->msg);
err_kzalloc_sdio_msg:
	kfree(modem->ctrl_port);
err_kzalloc_ctrl_port:
	kfree(modem);
err_kzalloc_sdio_modem:
	return ret;

exit_tty:
	tty_unregister_driver(tty_drv);
exit_reg_driver:
	LOGPRT(LOG_ERR,  "%s: returning with error %d\n",__func__, ret);
	put_tty_driver(tty_drv);
	return ret;
}

void  modem_sdio_exit(void)
{
	sdio_unregister_driver(&modem_sdio_driver);
	tty_unregister_driver(modem_sdio_tty_driver);
	put_tty_driver(modem_sdio_tty_driver);
}

