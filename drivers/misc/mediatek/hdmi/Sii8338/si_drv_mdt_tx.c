/*
 Silicon Image Driver Extension

 Copyright (C) 2012 Silicon Image Inc.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation version 2.

 This program is distributed .as is. WITHOUT ANY WARRANTY of any
 kind, whether express or implied; without even the implied warranty
 of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the
 GNU General Public License for more details.
*/
/* !file     si_drv_mdt_tx.c */
/* !brief    Silicon Image implementation */
/*  */

#ifdef MDT_SUPPORT

#include "si_common.h"
#ifndef	__KERNEL__
#include "hal_timers.h"
#endif				/* not defined __KERNEL */

#include "sii_hal.h"
#include "si_cra.h"
#include "si_cra_cfg.h"
#include "si_mhl_defs.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_drv_mdt_tx.h"
#include "si_mdt_inputdev.h"

#include "../../platform/hal/sii_hal_priv.h"

#ifdef MDT_SUPPORT
unsigned char g_mdt_is_ready = 0;
struct device *g_mdt_dev = 0;
struct class *g_mdt_class = 0;

static struct g_mdt_t {

	struct i2c_client *i2c_cbus_client;
	unsigned char is_ready;

} g_mdt = {
.i2c_cbus_client = 0, .is_ready = 0,};
#endif

/* #define mdt_i2c_cbus_read_block() */
/* #define mdt_i2c_cbus_write_block() */

static void mdt_cbus_read_block(unsigned char reg_offset, unsigned char byte_count,
				unsigned char *data)
{
	unsigned char ret;
#if 0
	struct i2c_msg i2cMsg[2];

	i2cMsg[0].addr = (0xC8 >> 1);
	i2cMsg[0].flags = 0;
	i2cMsg[0].len = 1;
	i2cMsg[0].buf = &reg_offset;

	i2cMsg[1].addr = (0xC8 >> 1);
	i2cMsg[1].flags = 1;
	i2cMsg[1].len = byte_count;
	i2cMsg[1].buf = data;

	if (g_mdt.i2c_cbus_client == 0) {
		return;
	}

	ret = i2c_transfer(g_mdt.i2c_cbus_client->adapter, i2cMsg, 2);

	if (ret < 0) {
		return;
	}
#else

	int32_t status;
	u32 client_main_addr = g_mdt.i2c_cbus_client->addr;
	g_mdt.i2c_cbus_client->addr = 0xC8 >> 1;

	/* set spedd, panjie */
	g_mdt.i2c_cbus_client->ext_flag |= I2C_HS_FLAG;
	/*  */

	g_mdt.i2c_cbus_client->ext_flag |= I2C_DIRECTION_FLAG;
	status = i2c_master_send(g_mdt.i2c_cbus_client, (const char *)&reg_offset, 1);
	if (status < 0) {
		printk(" mdt_cbus_read_block,%d send error\n", __LINE__);
	}
	/* set spedd, panjie */
	g_mdt.i2c_cbus_client->ext_flag |= I2C_HS_FLAG;
	/*  */


	status = i2c_master_recv(g_mdt.i2c_cbus_client, data, byte_count);

	g_mdt.i2c_cbus_client->addr = client_main_addr;
#endif




#ifdef I2C_ANALYZER
	MHL_log_event(I2C_BLOCK_0x73, reg_offset, data[0]);
#endif
}

static void mdt_cbus_write_block(unsigned char reg_offset, unsigned char byte_count,
				 unsigned char *data)
{
	unsigned char ret;
	unsigned char buffer[0x0F + 1];

	if (byte_count > 0x0F)
		return;

	buffer[0] = reg_offset;

	memcpy(&buffer[1], data, byte_count);
	ret = i2c_master_send(g_mdt.i2c_cbus_client, buffer, (byte_count + 1));
	if (ret < 0) {
		return;
	}
#ifdef I2C_ANALYZER
	MHL_log_event(I2C_BLOCK_0x72, reg_offset, data[0]);
#endif
}

static void mdt_cbus_write_byte(unsigned char reg_offset, unsigned char value)
{
	mdt_cbus_write_block(reg_offset, 1, &value);
}


static unsigned char mdt_cbus_read_byte(unsigned char reg_offset)
{
	unsigned char ret;
	mdt_cbus_read_block(reg_offset, 1, &ret);
	return ret;

}

#ifdef MDT_SUPPORT_DEBUG
struct input_event g_events[15000];
struct input_event *g_e;
static void init_log_file(void);
static void deregster_log_file(void);
#endif


struct msc_request g_prior_msc_request = { 0, 0 };


extern struct si_mdt_inputdevs_t mdt;

static uint8_t sii8338_parse_received_burst_for_mdt(union mdt_event_t *mdt_packet,
						    SiiReg_t scratchpad_offset)
{
	memset(mdt_packet->bytes, 0x0, 0x0F);

	mdt_cbus_read_block(scratchpad_offset, MDT_MIN_PACKET_LENGTH, mdt_packet->bytes);

	if (mdt_packet->header.isHID == 0)
		return 0xFF;

	if ((mdt_packet->header.isKeyboard) || (mdt_packet->header.isNotLast)
	    || (mdt_packet->event_mouse.header.isNotMouse))

		mdt_cbus_read_block(scratchpad_offset + MDT_MIN_PACKET_LENGTH,
				    MDT_KEYBOARD_PACKET_TAIL_LENGTH,
				    (mdt_packet->bytes + MDT_MIN_PACKET_LENGTH));

	printk(KERN_ERR "MHL data: %x %x %x %x %x %x %x %x %x\n",
	       mdt_packet->bytes[0],
	       mdt_packet->bytes[1],
	       mdt_packet->bytes[2],
	       mdt_packet->bytes[3],
	       mdt_packet->bytes[4],
	       mdt_packet->bytes[5],
	       mdt_packet->bytes[6], mdt_packet->bytes[7], mdt_packet->bytes[8]);


	printk(KERN_ERR "MHL info: %x %x %x\n",
	       mdt_packet->header.isKeyboard,
	       mdt_packet->event_cursor.header.touch.isNotMouse,
	       mdt_packet->event_cursor.body.suffix.isGame);


	if (mdt_packet->header.isKeyboard) {
		mdt_generate_event_keyboard(&(mdt_packet->event_keyboard));
	} else if (mdt_packet->event_cursor.header.touch.isNotMouse == 0) {
		mdt_generate_event_mouse(&(mdt_packet->event_mouse));
	} else if (mdt_packet->event_cursor.body.suffix.isGame == 0) {
		mdt_generate_event_touchscreen(&(mdt_packet->event_cursor), 1);
	} else {
		mdt_generate_event_gamepad(&(mdt_packet->event_cursor));
	}
	return 0;
}

static void sii8338_msc_req_for_mdt(uint8_t req_type, uint8_t offset, uint8_t first_data)
{
	if ((offset != g_prior_msc_request.offset) &&
	    (first_data != g_prior_msc_request.first_data)) {
		g_prior_msc_request.offset = offset;
		g_prior_msc_request.first_data = first_data;
		mdt_cbus_write_block(0x13, 2, (uint8_t *) &g_prior_msc_request);
	} else if (offset != g_prior_msc_request.offset) {
		g_prior_msc_request.offset = offset;
		SiiRegWrite(TX_PAGE_CBUS | 0x0013, g_prior_msc_request.offset);
	} else if (first_data != g_prior_msc_request.first_data) {
		g_prior_msc_request.first_data = first_data;
		SiiRegWrite(TX_PAGE_CBUS | 0x0014, g_prior_msc_request.first_data);
	}

	SiiRegWrite(TX_PAGE_CBUS | 0x0012, req_type);
}


uint8_t sii8338_irq_for_mdt(enum mdt_state *mdt_state)
{
	uint8_t ret = 0;
	uint8_t intr;
	union mdt_event_t mdt_packet;	/* Should not use typedef. Change this in the future. */

	MHL_log_event(ISR_MDT_BEGIN, 0, *mdt_state);

	mdt_init();
	switch (*mdt_state) {
	case WAIT_FOR_REQ_WRT:
		intr = SiiRegRead(TX_PAGE_CBUS | 0x00A0);
		MHL_log_event(ISR_WRITEBURST_CAUGHT, TX_PAGE_CBUS | 0x00A0, intr);
		if (intr & MHL_INT_REQ_WRT) {
			sii8338_msc_req_for_mdt(0x01 << 3, OFFSET_SET_INT, GRT_WRT);
			SiiRegWrite(TX_PAGE_CBUS | 0x00A0, (MHL_INT_REQ_WRT | MHL_INT_DSCR_CHG));

			ret = MDT_EVENT_HANDLED;
			*mdt_state = WAIT_FOR_GRT_WRT_COMPLETE;

		}
		break;
	case WAIT_FOR_GRT_WRT_COMPLETE:
		intr = SiiRegRead(TX_PAGE_CBUS | 0x0008);
		MHL_log_event(ISR_WRITEBURST_CAUGHT, TX_PAGE_CBUS | 0x0008, intr);
		if (intr & BIT4) {
			SiiRegWrite(TX_PAGE_CBUS | 0x0008, BIT4);

			ret = MDT_EVENT_HANDLED;
			*mdt_state = WAIT_FOR_WRITE_BURST_COMPLETE;
		}
		break;
	case WAIT_FOR_WRITE_BURST_COMPLETE:
		intr = SiiRegRead(TX_PAGE_CBUS | 0x001E);
		MHL_log_event(ISR_WRITEBURST_CAUGHT, TX_PAGE_CBUS | 0x001E, intr);
		if (intr & BIT0) {
			sii8338_parse_received_burst_for_mdt(&mdt_packet, TX_PAGE_CBUS | 0x00C2);
			if (mdt_packet.header.isNotLast)
				sii8338_parse_received_burst_for_mdt(&mdt_packet,
								     TX_PAGE_CBUS | 0x00C9);
			SiiRegWrite(TX_PAGE_CBUS | 0x001E, BIT0);

			ret = MDT_EVENT_HANDLED;
			*mdt_state = WAIT_FOR_REQ_WRT;
		}
		break;
	case IDLE:
	default:
		break;
	}

	MHL_log_event(ISR_MDT_END, 0xFF, 0xFF);

	return ret;
}

void mdt_init(void)
{
	if (g_mdt_is_ready != 0)
		return;

	if (gMhlDevice.pI2cClient == 0)
		return;

	g_mdt.i2c_cbus_client = gMhlDevice.pI2cClient;

#ifdef MDT_SUPPORT_DEBUG
	init_log_file();
#endif
	mdt_input_init();

	g_mdt_is_ready = 1;
}

void mdt_deregister(void)
{
	if (g_mdt_is_ready == 0)
		return;

#ifdef MDT_SUPPORT_DEBUG
	deregster_log_file();
#endif
	mdt_input_deregister();

	g_mdt_is_ready = 0;
}


#ifdef MDT_SUPPORT_DEBUG

static void init_events(void)
{
	int wevent;
	g_e = g_events;
	for (wevent = (int)0; wevent < (int)ARRAY_SIZE(g_events); wevent++)
		g_events[wevent].type = (int)-1;
}

inline void MHL_log_event(int type, int code, int value)
{
	printk(KERN_ERR "MDT %x %x %x\n", type, code, value);
/*
	struct input_event *e	 = g_e;

	if (g_mdt_is_ready == 0)
		return;

	do_gettimeofday(&(e->time));
	e->type = type;
	e->code = code;
	e->value = value;

	if ((++e - g_events) >= ARRAY_SIZE(g_events))
		e = g_events;
	e->type = -1;
	g_e = e;
*/
}

struct input_event *for_each_valid_event(struct input_event *e)
{
	struct input_event *estart = e++;

	if ((e - g_events) >= ARRAY_SIZE(g_events))
		e = g_events;

	while (e->type == (__u16) -1 && e != estart) {
		e++;
		if ((e - g_events) >= ARRAY_SIZE(g_events))
			e = g_events;
	}

	if (e == estart || e->type == (__u16) -1)
		return NULL;
	return e;
}
#endif

/* This part always compiles to support conditional debug control */
#ifdef MDT_SUPPORT_DEBUG
static ssize_t show_events(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_event *e = g_e;
	int ret = 0;

	if (g_mdt_is_ready == 0)
		return 0;

	while ((e = for_each_valid_event(e)) != NULL) {
		ret += sprintf(buf + ret, "%8lu.%06lu,", e->time.tv_sec, e->time.tv_usec);
		switch (e->type) {
		case IRQ_HEARTBEAT:
			ret += sprintf(buf + ret, "%s", "IRQ_HEARTBEAT,");
			break;
		case IRQ_WAKE:
			ret += sprintf(buf + ret, "%s", "IRQ_WAKE,");
			break;
		case IRQ_RECEIVED:
			ret += sprintf(buf + ret, "%s", "IRQ_RECEIVED,");
			break;
		case ISR_WRITEBURST_CAUGHT:
			ret += sprintf(buf + ret, "%s", "ISR_WRITEBURST_CAUGHT,");
			break;
		case ISR_WRITEBURST_MISSED:
			ret += sprintf(buf + ret, "%s", "ISR_WRITEBURST_MISSED,");
			break;

		case ISR_DEFFER_SCHEDULED:
			ret += sprintf(buf + ret, "%s", "ISR_DEFFER_SCHEDULED,");
			break;
		case ISR_DEFFER_NO:
			ret += sprintf(buf + ret, "%s", "ISR_DEFFER_NO,");
			break;
		case ISR_DEFFER_BEGIN:
			ret += sprintf(buf + ret, "%s", "ISR_DEFFER_BEGIN,");
			break;
		case ISR_DEFFER_END:
			ret += sprintf(buf + ret, "%s", "ISR_DEFFER_END,");
			break;
		case I2C_BLOCK_R_UNKNOWN:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_UNKNOWN_R,");
			break;
		case I2C_BLOCK_W_UNKNOWN:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_UNKNOWN_W,");
			break;
		case I2C_BLOCK_0x72:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x72,");
			break;
		case I2C_BLOCK_0x7A:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x7A,");
			break;
		case I2C_BLOCK_0x92:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x93,");
			break;
		case I2C_BLOCK_0xC8:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0xC8,");
			break;
		case I2C_BLOCK_0x73:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x73,");
			break;
		case I2C_BLOCK_0x7B:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x7B,");
			break;
		case I2C_BLOCK_0x93:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0x93,");
			break;
		case I2C_BLOCK_0xC9:
			ret += sprintf(buf + ret, "%s", "I2C_BLOCK_0xC9,");
			break;
		case I2C_BYTE_0x72:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x72,");
			break;
		case I2C_BYTE_0x7A:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x7A,");
			break;
		case I2C_BYTE_0x92:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x93,");
			break;
		case I2C_BYTE_0xC8:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0xC8,");
			break;
		case I2C_BYTE_0x73:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x73,");
			break;
		case I2C_BYTE_0x7B:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x7B,");
			break;
		case I2C_BYTE_0x93:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0x93,");
			break;
		case I2C_BYTE_0xC9:
			ret += sprintf(buf + ret, "%s", "I2C_BYTE_0xC9,");
			break;
		case ISR_THREADED_BEGIN:
			ret += sprintf(buf + ret, "%s", "ISR_THREADED_BEGIN,");
			break;
		case ISR_THREADED_END:
			ret += sprintf(buf + ret, "%s", "ISR_THREADED_END,");
			break;
		case ISR_MDT_BEGIN:
			ret += sprintf(buf + ret, "%s", "ISR_MDT_BEGIN,");
			break;
		case ISR_MDT_END:
			ret += sprintf(buf + ret, "%s", "ISR_MDT_END,");
			break;
		case ISR_FULL_BEGIN:
			ret += sprintf(buf + ret, "%s", "ISR_FULL_BEGIN,");
			break;
		case ISR_FULL_END:
			ret += sprintf(buf + ret, "%s", "ISR_FULL_END,");
			break;
		case TOUCHPAD_BEGIN:
			ret += sprintf(buf + ret, "%s", "TOUCHPAD_BEGIN,");
			break;
		case TOUCHPAD_END:
			ret += sprintf(buf + ret, "%s", "TOUCHPAD_END,");
			break;
		case MHL_ESTABLISHED:
			ret += sprintf(buf + ret, "%s", "MHL_ESTABLISHED,");
			break;
		case MSC_READY:
			ret += sprintf(buf + ret, "%s", "MSC_READY,");
			break;
		case MDT_EVENT_PARSED:
			ret += sprintf(buf + ret, "%s", "MDT_EVENT_PARSED,");
			break;
		case MDT_UNLOCK:
			ret += sprintf(buf + ret, "%s", "MDT_UNLOCK,");
			break;
		case MDT_LOCK:
			ret += sprintf(buf + ret, "%s", "MDT_LOCK,");
			break;
		case MDT_DISCOVER_REQ:
			ret += sprintf(buf + ret, "%s", "MDT_DISCOVERY_REQ,");
			break;
		}

		ret += sprintf(buf + ret, "%02x,%02x,%02x\n", e->type, e->code, e->value);
		e->type = -1;

		if (ret > (PAGE_SIZE - 512))
			return ret;
	}

	return ret;
}

static DEVICE_ATTR(MDT_file, S_IRUGO, show_events, NULL);

static void deregster_log_file(void)
{
	if (g_mdt_is_ready == 0)
		return;
	if (g_mdt_class == 0)
		return;
	if (g_mdt_dev == 0)
		return;

	device_destroy(g_mdt_class, 0);
	class_destroy(g_mdt_class);

	g_mdt_class = 0;
	g_mdt_dev = 0;
}

static void init_log_file(void)
{
	if (g_mdt_is_ready != 0)
		return;
	if (g_mdt_class != 0)
		return;
	if (g_mdt_dev != 0)
		return;

	init_events();

	g_mdt_is_ready = 1;

	g_mdt_class = class_create(THIS_MODULE, "mdt");
	if (IS_ERR(g_mdt_class)) {
		printk(KERN_ERR "MDT ERR: Failed to create debug helper class.\n");
		return;
	}

	g_mdt_dev = device_create(g_mdt_class, NULL, 0, NULL, "mdt_debug_dev");

	if (IS_ERR(g_mdt_dev)) {
		printk(KERN_ERR "MDT ERR: Failed to create debug helper device.\n");
		return;
	}

	if (device_create_file(g_mdt_dev, &dev_attr_MDT_file) < 0) {
		printk(KERN_ERR "MDT ERR: Failed to create debug helper device file.\n");
		return;
	}

}


#else
inline void MHL_log_event(int type, int code, int value)
{
	printk(KERN_ERR "MDT %x %x %x\n", type, code, value);
}


#endif

#endif
