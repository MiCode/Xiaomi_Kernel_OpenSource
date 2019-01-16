/* drivers/input/touchscreen/max1187x.c
 *
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 *
 * Driver Version: 3.1.7
 * Release Date: May 9, 2013
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>

#ifdef CONFIG_USE_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/firmware.h>
#include <linux/crc16.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/jiffies.h>
#include <asm/byteorder.h>
#include "max1187x.h"
#include "max1187x_config.h"
#include "tpd.h"
#include "cust_gpio_usage.h" 
#include "cust_eint.h" 

#ifdef CONFIG_OF_TOUCH
#include <linux/of.h>
#include <linux/of_irq.h>
#endif

#define GTP_SUPPORT_I2C_DMA 1

#if GTP_SUPPORT_I2C_DMA
#include <linux/dma-mapping.h>
#endif

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) MAX1187X_NAME "(%s:%d): " fmt, __func__, __LINE__
#endif

#define pr_info_if(a, b, ...) do { if (debug_mask & a) \
			pr_info(b, ##__VA_ARGS__);	\
			} while (0)
#define debugmask_if(a) (debug_mask & a)

#define NWORDS(a)    (sizeof(a) / sizeof(u16))
#define BYTE_SIZE(a) ((a) * sizeof(u16))
#define BYTEH(a)     ((a) >> 8)
#define BYTEL(a)     ((a) & 0xFF)

#define PDATA(a)      (ts->pdata->a)

static u16 debug_mask;

#ifdef MAX1187X_LOCAL_PDATA
struct max1187x_pdata local_pdata = {
	.gpio_tirq	= 0,
	.num_fw_mappings = 1,
	.fw_mapping[0] = {.config_id = 0x6701, .chip_id = 0x78, .filename = "max11876.bin", .filesize = 0xC000, .file_codesize = 0xC000},
	.defaults_allow = 1,
	.default_config_id = 0x6701,
	.default_chip_id = 0x78,
	//.i2c_words = MAX_WORDS_REPORT,
	.i2c_words = 128,
	//.coordinate_settings = MAX1187X_REVERSE_Y | MAX1187X_SWAP_XY,
	.coordinate_settings = 0,
	.panel_margin_xl = 0,
	.lcd_x = 1300,
	.panel_margin_xh = 0,
	.panel_margin_yl = 0,
	.lcd_y = 700,
	.panel_margin_yh = 0,
	.num_sensor_x = 32,
	.num_sensor_y = 18,
	.button_code0 = 0,
	.button_code1 = 0,
	.button_code2 = 0,
	.button_code3 = 0,
};
#endif

struct report_reader {
	u16 report_id;
	u16 reports_passed;
	struct semaphore sem;
	int status;
};

struct data {
	struct max1187x_pdata *pdata;
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[32];
	
#ifdef TOUCH_WAKEUP_FEATURE
	struct input_dev *input_dev_key;
	char phys_key[32];
#endif

#ifdef CONFIG_USE_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	u8 is_suspended;

	wait_queue_head_t waitqueue_all;
	struct workqueue_struct *wq;
	struct work_struct work_irq;
	u32 irq_receive_time;

	u16 chip_id;
	u16 config_id;

	struct mutex irq_mutex;
	struct mutex i2c_mutex;
	struct mutex report_mutex;
	struct semaphore report_sem;
	struct report_reader report_readers[MAX_REPORT_READERS];
	u8 report_readers_outstanding;

	u16 cmd_buf[CMD_LEN_MAX];
	u16 cmd_len;
	struct semaphore sema_cmd;
	struct work_struct work_cmd;

	struct semaphore sema_rbcmd;
	wait_queue_head_t waitqueue_rbcmd;
	u8 rbcmd_waiting;
	u8 rbcmd_received;
	u16 rbcmd_report_id;
	u16 rbcmd_rx_report[RPT_LEN_MAX];
	u16 rbcmd_rx_report_len;

	u16 rx_report[RPT_LEN_MAX]; /* with header */
	u16 rx_report_len;
	u16 rx_packet[RPT_LEN_PACKET_MAX + 1]; /* with header */
	u32 irq_count;
	u16 framecounter;
	u16 list_finger_ids;
	u8 sysfs_created;
	u8 is_raw_mode;

	u16 button0:1;
	u16 button1:1;
	u16 button2:1;
	u16 button3:1;
};

/************************++MTK add++*************************/
extern struct tpd_device *tpd;
static st_tpd_info tpd_info = {0};

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

static const struct i2c_device_id tpd_i2c_id[] = {{MAX1187X_NAME, 0}, {}};
static struct i2c_board_info __initdata i2c_tpd = { I2C_BOARD_INFO(MAX1187X_NAME, 0x48)};

static bool check_flag = false;

static struct data *ts = NULL;//we need this function to be used in MTK's EINT call back funciotn, so we change it from a tample data to a static data

#if GTP_SUPPORT_I2C_DMA
#define GTP_DMA_MAX_TRANSACTION_LENGTH 255
#define GTP_DMA_MAX_I2C_TRANSFER_SIZE 255

static u8 *gpDMABuf_va = NULL;
static dma_addr_t *gpDMABuf_pa = 0;
#endif

#define MAX_TRANSACTION_LENGTH 8
#define I2C_MASTER_CLOCK 100
#define GTP_ADDR_LENGTH 2
#define MAX_I2C_TRANSFER_SIZE 6
/************************--MTK add--*************************/




#ifdef CONFIG_USE_EARLYSUSPEND
static void early_suspend(struct early_suspend *h);
static void late_resume(struct early_suspend *h);
#endif

static int device_init(struct i2c_client *client);
static int device_deinit(struct i2c_client *client);

static int bootloader_enter(struct data *ts);
static int bootloader_exit(struct data *ts);
static int bootloader_get_crc(struct data *ts, u16 *crc16,
		u16 addr, u16 len, u16 delay);
static int bootloader_set_byte_mode(struct data *ts);
static int bootloader_erase_flash(struct data *ts);
static int bootloader_write_flash(struct data *ts, const u8 *image, u16 length);

static void propagate_report(struct data *ts, int status, u16 *report);
static int get_report(struct data *ts, u16 report_id, ulong timeout);
static void release_report(struct data *ts);
static int cmd_send(struct data *ts, u16 *buf, u16 len);
static int rbcmd_send_receive(struct data *ts, u16 *buf,
		u16 len, u16 report_id, u16 timeout);

#if MAX1187X_TOUCH_REPORT_MODE == 2
#ifndef MAX1187X_REPORT_FAST_CALCULATION
static u16 binary_search(const u16 *array, u16 len, u16 val);
static u16 max1187x_sqrt(u32 num);
static s16 max1187x_orientation(s16 x, s16 y);
#endif
#endif

static u8 init_state;


#if GTP_SUPPORT_I2C_DMA
s32 i2c_dma_read(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
    int ret;
    s32 retry = 0;
    u8 buffer[2];
 
    struct i2c_msg msg[2] =
    {
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .flags = 0,
            .buf = buffer,
            .len = 2,
            .timing = I2C_MASTER_CLOCK
        },
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
            .flags = I2C_M_RD,
            .buf = gpDMABuf_pa,     
            .len = len,
            .timing = I2C_MASTER_CLOCK
        },
    };
	
	mutex_lock(&ts->i2c_mutex);    
    buffer[1] = (addr >> 8) & 0xFF;
    buffer[0] = addr & 0xFF;

    if (rxbuf == NULL){
		mutex_unlock(&ts->i2c_mutex); 
        return -1;
    	}
    //GTP_DEBUG("dma i2c read: 0x%04X, %d bytes(s)", addr, len);
    for (retry = 0; retry < 5; ++retry)
    {
        ret = i2c_transfer(client->adapter, &msg[0], 2);
        if (ret < 0)
        {
            continue;
        }
        memcpy(rxbuf, gpDMABuf_va, len);
		mutex_unlock(&ts->i2c_mutex);
        return 0;
    }
    TPD_DMESG("Dma I2C Read Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
	mutex_unlock(&ts->i2c_mutex);
    return ret;
}


s32 i2c_dma_write(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{
    int ret;
    s32 retry = 0;
    u8 *wr_buf = gpDMABuf_va;
 
    struct i2c_msg msg =
    {
        .addr = (client->addr & I2C_MASK_FLAG),
        .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
        .flags = 0,
        .buf = gpDMABuf_pa,
        .len = 2 + len,
        .timing = I2C_MASTER_CLOCK
    };
	
    mutex_lock(&ts->i2c_mutex);    
    wr_buf[1] = (u8)((addr >> 8) & 0xFF);
    wr_buf[0] = (u8)(addr & 0xFF);

    if (txbuf == NULL){
			mutex_unlock(&ts->i2c_mutex); 
			return -1;
    	}
    //GTP_DEBUG("dma i2c write: 0x%04X, %d bytes(s)", addr, len);
    memcpy(wr_buf+2, txbuf, len);
    for (retry = 0; retry < 5; ++retry)
    {
        ret = i2c_transfer(client->adapter, &msg, 1);
        if (ret < 0)
        {
            continue;
        }
		mutex_unlock(&ts->i2c_mutex); 
        return 0;
    }
    TPD_DMESG("Dma I2C Write Error: 0x%04X, %d byte(s), err-code: %d", addr, len, ret);
	mutex_unlock(&ts->i2c_mutex); 
    return ret;
}

s32 i2c_read_bytes_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, s32 len)
{
    s32 left = len;
    s32 read_len = 0;
    u8 *rd_buf = rxbuf;
    s32 ret = 0;
	
    //mutex_lock(&tp_wr_access);    
    //GTP_DEBUG("Read bytes dma: 0x%04X, %d byte(s)", addr, len);
    while (left > 0)
    {
        if (left > GTP_DMA_MAX_TRANSACTION_LENGTH)
        {
            read_len = GTP_DMA_MAX_TRANSACTION_LENGTH;
        }
        else
        {
            read_len = left;
        }
        ret = i2c_dma_read(client, addr, rd_buf, read_len);
        if (ret < 0)
        {
            TPD_DMESG("dma read failed");
			//mutex_unlock(&tp_wr_access);
            return -1;
        }
        
        left -= read_len;
        addr += read_len/2;
        rd_buf += read_len;
    }
    //mutex_unlock(&tp_wr_access);
    return 0;
}

s32 i2c_write_bytes_dma(struct i2c_client *client, u16 addr, u8 *txbuf, s32 len)
{

    s32 ret = 0;
    s32 write_len = 0;
    s32 left = len;
    u8 *wr_buf = txbuf;
    //mutex_lock(&tp_wr_access);      
    //GTP_DEBUG("Write bytes dma: 0x%04X, %d byte(s)", addr, len);
    while (left > 0)
    {
        if (left > GTP_DMA_MAX_I2C_TRANSFER_SIZE)
        {
            write_len = GTP_DMA_MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            write_len = left;
        }
        ret = i2c_dma_write(client, addr, wr_buf, write_len);
        
        if (ret < 0)
        {
            TPD_DMESG("dma i2c write failed!");
		    //mutex_unlock(&tp_wr_access);  
            return -1;
        }
        
        left -= write_len;
        addr += write_len/2;
        wr_buf += write_len;
    }
    //mutex_unlock(&tp_wr_access);  
    return 0;
}

/* I2C communication */
/* debug_mask |= 0x1 for I2C RX communication */
static int i2c_rx_byte(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	
	ts->client->addr = ts->client->addr & I2C_MASK_FLAG;
	ts->client->addr = ts->client->addr | I2C_DMA_FLAG;
	do {
		ret = i2c_master_recv(ts->client, (char *) gpDMABuf_pa, (int) len);
		memcpy(buf, gpDMABuf_va, len);
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C RX fail (%d)", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_rx_word(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	
	ts->client->addr = ts->client->addr & I2C_MASK_FLAG;
	ts->client->addr = ts->client->addr | I2C_DMA_FLAG;
	do {
		ret = i2c_master_recv(ts->client, (char *) gpDMABuf_pa, (int) (len * 2));
		memcpy(buf, gpDMABuf_va, len*2);
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C RX fail (%d)", ret);
		return ret;
	}

	if ((ret % 2) != 0) {
		pr_err("I2C words RX fail: odd number of bytes (%d)", ret);
		return -EIO;
	}

	len = ret/2;

#ifdef __BIG_ENDIAN
	for (i = 0; i < len; i++)
		buf[i] = (buf[i] << 8) | (buf[i] >> 8);
#endif
	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written,
					8, "0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

/* debug_mask |= 0x2 for I2C TX communication */
static int i2c_tx_byte(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

    memcpy(gpDMABuf_va, buf, len);
	ts->client->addr = ts->client->addr & I2C_MASK_FLAG;
	ts->client->addr = ts->client->addr | I2C_DMA_FLAG;

	do {
		ret = i2c_master_send(ts->client, (char *) gpDMABuf_pa, (int) len);
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C TX fail (%d)", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_tx_word(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

#ifdef __BIG_ENDIAN
	for (i = 0; i < len; i++)
		buf[i] = (buf[i] << 8) | (buf[i] >> 8);
#endif

    memcpy(gpDMABuf_va, buf, len*2);
	ts->client->addr = ts->client->addr & I2C_MASK_FLAG;
	ts->client->addr = ts->client->addr | I2C_DMA_FLAG;

	do {
		ret = i2c_master_send(ts->client, (char *) gpDMABuf_pa, (int) (len * 2));
	} while (ret == -EAGAIN);
	if (ret < 0) {
		pr_err("I2C TX fail (%d)", ret);
		return ret;
	}
	if ((ret % 2) != 0) {
		pr_err("I2C words TX fail: odd number of bytes (%d)", ret);
		return -EIO;
	}

	len = ret/2;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 8,
					"0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

#endif


/* I2C communication */

static int i2c_write_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *txbuf, int len)
{
    u8 buffer[MAX_TRANSACTION_LENGTH];
    u16 left = len;
    u16 offset = 0;
    u8 retry = 0;

    struct i2c_msg msg =
    {
        .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
        //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)),
        .flags = 0,
        .buf = buffer,
        .timing = I2C_MASTER_CLOCK,
    };


    if (txbuf == NULL)
        return -1;

    TPD_DEBUG("i2c_write_bytes to device %02X address %04X len %d\n", client->addr, addr, len);

	mutex_lock(&ts->i2c_mutex);
    while (left > 0)
    {
        retry = 0;

        buffer[1] = ((addr + offset/2) >> 8) & 0xFF;
        buffer[0] = (addr + offset/2) & 0xFF;

        if (left > MAX_I2C_TRANSFER_SIZE)
        {
            memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], MAX_I2C_TRANSFER_SIZE);
            msg.len = MAX_TRANSACTION_LENGTH;
            left -= MAX_I2C_TRANSFER_SIZE;
            offset += MAX_I2C_TRANSFER_SIZE;
        }
        else
        {
            memcpy(&buffer[GTP_ADDR_LENGTH], &txbuf[offset], left);
            msg.len = left + GTP_ADDR_LENGTH;
            left = 0;
        }

        //GTP_DEBUG("byte left %d offset %d\n", left, offset);

        while (i2c_transfer(client->adapter, &msg, 1) != 1)
        {
            retry++;

            if (retry == 5)
            {
                TPD_DMESG("I2C write 0x%X%X length=%d failed\n", buffer[0], buffer[1], len);
				mutex_unlock(&ts->i2c_mutex);
                return -1;
            }
        }
    }
	mutex_unlock(&ts->i2c_mutex);

    return 0;
}

static int i2c_read_bytes_non_dma(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
    u8 buffer[GTP_ADDR_LENGTH];
    u8 retry;
    u16 left = len;
    u16 offset = 0;


    struct i2c_msg msg[2] =
    {
        {
            .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
            //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)),
            .flags = 0,
            .buf = buffer,
            .len = 2,
            .timing = I2C_MASTER_CLOCK
        },
        {
            .addr = ((client->addr &I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
            //.addr = ((client->addr &I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)),
            .flags = I2C_M_RD,
            .timing = I2C_MASTER_CLOCK
        },
    };

    if (rxbuf == NULL)
        return -1;

    TPD_DEBUG("i2c_read_bytes to device %02X address %04X len %d\n", client->addr, addr, len);

	mutex_lock(&ts->i2c_mutex);

    while (left > 0)
    {
        buffer[1] = ((addr + offset/2) >> 8) & 0xFF;
        buffer[0] = (addr + offset/2) & 0xFF;

        msg[1].buf = &rxbuf[offset];

        if (left > MAX_TRANSACTION_LENGTH)
        {
            msg[1].len = MAX_TRANSACTION_LENGTH;
            left -= MAX_TRANSACTION_LENGTH;
            offset += MAX_TRANSACTION_LENGTH;
        }
        else
        {
            msg[1].len = left;
            left = 0;
        }

        retry = 0;
		
        while (i2c_transfer(client->adapter, &msg[0], 2) != 2)
        {
            retry++;

            if (retry == 5)
            {
                TPD_DMESG("I2C read 0x%X length=%d failed\n", addr + offset, len);
				mutex_unlock(&ts->i2c_mutex);
                return -1;
            }
        }
    }
	mutex_unlock(&ts->i2c_mutex);

    return 0;
}

/* debug_mask |= 0x1 for I2C RX communication */
static int i2c_rx_bytes(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];

	u16 command_address = 0;
	command_address = (buf[0])|((buf[1]) << 8);
	
	do {
		#if GTP_SUPPORT_I2C_DMA
		ret = i2c_read_bytes_dma(ts->client, command_address, (char *) buf, (int) len);
		#else
		ret = i2c_read_bytes_non_dma(ts->client, command_address, (char *) buf, (int) len);
		#endif
	} while (ret == -EAGAIN);
	if (ret < 0) {
		TPD_DMESG("I2C RX fail (%d)", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_rx_words(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	
	u16 command_address = 0;

	command_address = buf[0];

	do {
	#if GTP_SUPPORT_I2C_DMA
		ret = i2c_read_bytes_dma(ts->client, command_address,
			(char *) buf, (int) (len * 2));	
	#else
		ret = i2c_read_bytes_non_dma(ts->client, command_address,
			(char *) buf, (int) (len * 2));
	#endif		
	} while (ret == -EAGAIN);
	if (ret < 0) {
		TPD_DMESG("I2C RX fail (%d)\n", ret);
		return ret;
	}

	if ((ret % 2) != 0) {
		TPD_DMESG("I2C words RX fail: odd number of bytes (%d)\n", ret);
		return -EIO;
	}

	len = ret/2;

#ifdef __BIG_ENDIAN
	for (i = 0; i < len; i++)
		buf[i] = (buf[i] << 8) | (buf[i] >> 8);
#endif
	if (debugmask_if(1)) {
		pr_info("I2C RX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written,
					8, "0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

/* debug_mask |= 0x2 for I2C TX communication */
static int i2c_tx_bytes(struct data *ts, u8 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	
	u8 *command_buffer = NULL;
	u16 command_address = 0;

	command_address = (buf[0])|((buf[1]) << 8);
	command_buffer = &buf[2];

	do {
		#if GTP_SUPPORT_I2C_DMA
		ret = i2c_write_bytes_dma(ts->client, command_address, (char *) command_buffer, (int) len);
		#else
		ret = i2c_write_bytes_non_dma(ts->client, command_address, (char *) command_buffer, (int) len);
		#endif
	} while (ret == -EAGAIN);
	if (ret < 0) {
		TPD_DMESG("I2C TX fail (%d)\n", ret);
		return ret;
	}

	len = ret;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 6,
					"0x%02X,", buf[i]);
			if (written + 6 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}

static int i2c_tx_words(struct data *ts, u16 *buf, u16 len)
{
	int i, ret, written;
	char debug_string[DEBUG_STRING_LEN_MAX];
	
	u8 *command_buffer = NULL;
	u16 command_address = 0;

	command_address = buf[0];
	command_buffer = &buf[1];
	
#ifdef __BIG_ENDIAN
	for (i = 0; i < len; i++)
		buf[i] = (buf[i] << 8) | (buf[i] >> 8);
#endif
	do {
		#if GTP_SUPPORT_I2C_DMA
			ret = i2c_write_bytes_dma(ts->client, command_address, 
			(char *) command_buffer, (int) (len * 2));	
		#else
		ret = i2c_write_bytes_non_dma(ts->client, command_address, 
			(char *) command_buffer, (int) (len * 2));
		#endif	
	} while (ret == -EAGAIN);
	if (ret < 0) {
		TPD_DMESG("I2C TX fail (%d)\n", ret);
		return ret;
	}
	if ((ret % 2) != 0) {
		TPD_DMESG("I2C words TX fail: odd number of bytes (%d)\n", ret);
		return -EIO;
	}

	len = ret/2;

	if (debugmask_if(2)) {
		pr_info("I2C TX (%d):", len);
		written = 0;
		for (i = 0; i < len; i++) {
			written += snprintf(debug_string + written, 8,
					"0x%04X,", buf[i]);
			if (written + 8 >= DEBUG_STRING_LEN_MAX) {
				pr_info("%s", debug_string);
				written = 0;
			}
		}
		if (written > 0)
			pr_info("%s", debug_string);
	}

	return len;
}



static int i2c_tx_w(struct data *ts, u16 *buf, u16 len)
{
	int ret = 0;
	
#if GTP_SUPPORT_I2C_DMA
	ret = i2c_tx_word(ts, buf, len);
#else
	ret = i2c_tx_words(ts, buf, len);
#endif

	return ret;
}

static int i2c_tx_b(struct data *ts, u8 *buf, u16 len)
{
	int ret = 0;
	
#if GTP_SUPPORT_I2C_DMA
	ret = i2c_tx_byte(ts, buf, len);
#else
	ret = i2c_tx_bytes(ts, buf, len);
#endif

	return ret;
}

static int i2c_rx_w(struct data *ts, u16 *buf, u16 len)
{
	int ret = 0;
	
#if GTP_SUPPORT_I2C_DMA
	ret = i2c_rx_word(ts, buf, len);
#else
	ret = i2c_rx_words(ts, buf, len);
#endif

	return ret;
}

static int i2c_rx_b(struct data *ts, u8 *buf, u16 len)
{
	int ret = 0;
	
#if GTP_SUPPORT_I2C_DMA
	ret = i2c_rx_byte(ts, buf, len);
#else
	ret = i2c_rx_bytes(ts, buf, len);
#endif

	return ret;
}

/* Read report */
static int read_mtp_report(struct data *ts, u16 *buf)
{
	int words = 1, words_tx, words_rx;
	int ret = 0, remainder = 0, offset = 0;
	u16 address = 0x000A;

	mutex_lock(&ts->i2c_mutex);
	/* read header, get size, read entire report */
	{
		words_tx = i2c_tx_w(ts, &address, 1);
		if (words_tx != 1) {
			mutex_unlock(&ts->i2c_mutex);
			pr_err("Report RX fail: failed to set address");
			return -EIO;
		}

		if (ts->is_raw_mode == 0) {
			words_rx = i2c_rx_w(ts, buf, 2);
			if (words_rx < 0 ||
					BYTEL(buf[0]) > RPT_LEN_PACKET_MAX) {
				ret = -EIO;
				TPD_DEBUG("max1187x Report RX fail: received (%d) " \
						"expected (%d) words, " \
						"header (%04X)",
						words_rx, words, buf[0]);
				mutex_unlock(&ts->i2c_mutex);
				return ret;
			}
			
			TPD_DEBUG("max1187x Report RX 1: received (%d) " \
					"expected (%d) words, " \
					"header (%04X, %04X)\n",
					words_rx, words, buf[0], buf[1]);

			if ((((BYTEH(buf[0])) & 0xF) == 0x1)
				&& buf[1] == 0x0800)
				ts->is_raw_mode = 1;

			words = BYTEL(buf[0]) + 1;

			TPD_DEBUG("max1187x Report RX 2: received (%d) " \
					"expected (%d) words, " \
					"header (%04X, %04X)\n",
					words_rx, words, buf[0], buf[1]);
			
			words_tx = i2c_tx_w(ts, &address, 1);
			if (words_tx != 1) {
				mutex_unlock(&ts->i2c_mutex);
				pr_err("Report RX fail:" \
					"failed to set address");
				return -EIO;
			}

			words_rx = i2c_rx_w(ts, &buf[offset], words);
			if (words_rx < 0) {
				mutex_unlock(&ts->i2c_mutex);
				printk("max1187x Report RX fail 0x%X: received (%d) " \
					"expected (%d) words",
					address, words_rx, remainder);
				return -EIO;

			}
			
			TPD_DEBUG("max1187x Report RX 3: received (%d) " \
					"expected (%d) words, " \
					"header (%04X, %04X, %04X, %04X, %04X, %04X, %04X, %04X, %04X, %04X)\n",
					words_rx, words, buf[0], buf[1], buf[2], buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9]);

		} else {

			words_rx = i2c_rx_w(ts, buf,
					(u16) PDATA(i2c_words));
			if (words_rx < 0 || BYTEL(buf[0])
					> RPT_LEN_PACKET_MAX) {
				ret = -EIO;
				pr_err("max1187x Report RX fail: received (%d) " \
					"expected (%d) words, header (%04X)",
					words_rx, words, buf[0]);
				mutex_unlock(&ts->i2c_mutex);
				return ret;
			}

			if ((((BYTEH(buf[0])) & 0xF) == 0x1)
				&& buf[1] != 0x0800)
				ts->is_raw_mode = 0;

			words = BYTEL(buf[0]) + 1;
			remainder = words;

			if (remainder - (u16) PDATA(i2c_words) > 0) {
				remainder -= (u16) PDATA(i2c_words);
				offset += (u16) PDATA(i2c_words);
				address += (u16) PDATA(i2c_words);
			}

			words_tx = i2c_tx_w(ts, &address, 1);
			if (words_tx != 1) {
				mutex_unlock(&ts->i2c_mutex);
				pr_err("Report RX fail: failed to set " \
					"address 0x%X", address);
				return -EIO;
			}

			words_rx = i2c_rx_w(ts, &buf[offset], remainder);
			if (words_rx < 0) {
				mutex_unlock(&ts->i2c_mutex);
				pr_err("max1187x Report RX fail 0x%X: received (%d) " \
						"expected (%d) words",
						address, words_rx, remainder);
				return -EIO;
			}
		}
	}
	mutex_unlock(&ts->i2c_mutex);
	return ret;
}

/* Send command */
static int send_mtp_command(struct data *ts, u16 *buf, u16 len)
{
	u16 tx_buf[CMD_LEN_PACKET_MAX + 2]; /* with address and header */
	u16 packets, words, words_tx;
	int i, ret = 0;

	/* check basics */
	if (len < 2 || len > CMD_LEN_MAX || (buf[1] + 2) != len) {
		TPD_DMESG("Command length is not valid\n");
		ret = -EINVAL;
		goto err_send_mtp_command;
	}

	/* packetize and send */
	packets = len / CMD_LEN_PACKET_MAX;
	if (len % CMD_LEN_PACKET_MAX)
		packets++;
	tx_buf[0] = 0x0000;

	mutex_lock(&ts->i2c_mutex);
	for (i = 0; i < packets; i++) {
		words = (i == (packets - 1)) ? len : CMD_LEN_PACKET_MAX;
		tx_buf[1] = (packets << 12) | ((i + 1) << 8) | words;
		memcpy(&tx_buf[2], &buf[i * CMD_LEN_PACKET_MAX],
			BYTE_SIZE(words));
		words_tx = i2c_tx_w(ts, tx_buf, words + 2);
		if (words_tx < 0) {
			TPD_DMESG("Command TX fail: transmitted (%d) " \
				"expected (%d) words, packet (%d)\n",
				words_tx, words + 2, i);
			ret = -EIO;
			mutex_unlock(&ts->i2c_mutex);
			goto err_send_mtp_command;
		}
		len -= CMD_LEN_PACKET_MAX;
	}
	mutex_unlock(&ts->i2c_mutex);

	return ret;

err_send_mtp_command:
	return ret;
}

static void max1187x_wfxn_cmd(struct work_struct *work)
{
	struct data *ts = container_of(work, struct data, work_cmd);

	send_mtp_command(ts, ts->cmd_buf, ts->cmd_len);

	up(&ts->sema_cmd);

}

/* Integer math operations */
#if MAX1187X_TOUCH_REPORT_MODE == 2
#ifndef MAX1187X_REPORT_FAST_CALCULATION
u16 max1187x_sqrt(u32 num)
{
	u16 mask = 0x8000;
	u16 guess = 0;
	u32 prod = 0;

	if (num < 2)
		return num;

	while (mask) {
		guess = guess ^ mask;
		prod = guess*guess;
		if (num < prod)
			guess = guess ^ mask;
		mask = mask>>1;
	}
	if (guess != 0xFFFF) {
		prod = guess*guess;
		if ((num - prod) > (prod + 2*guess + 1 - num))
			guess++;
	}

	return guess;
}
/* Returns index of element in array closest to val */
static u16 binary_search(const u16 *array, u16 len, u16 val)
{
	s16 lt, rt, mid;
	if (len < 2)
		return 0;

	lt = 0;
	rt = len - 1;

	while (lt <= rt) {
		mid = (lt + rt)/2;
		if (val == array[mid])
			return mid;
		if (val < array[mid])
			rt = mid - 1;
		else
			lt = mid + 1;
	}

	if (lt >= len)
		return len - 1;
	if (rt < 0)
		return 0;
	if (array[lt] - val > val - array[lt-1])
		return lt-1;
	else
		return lt;
}
/* Given values of x and y, it calculates the orientation
 * with respect to y axis by calculating atan(x/y)
 */
static s16 max1187x_orientation(s16 x, s16 y)
{
	u16 sign = 0;
	s16 angle;
	u16 len = sizeof(tanlist)/sizeof(tanlist[0]);
	u32 quotient;

	if (x == y) {
		angle = 45;
		return angle;
	}
	if (x == 0) {
		angle = 0;
		return angle;
	}
	if (y == 0) {
		if (x > 0)
			angle = 90;
		else
			angle = -90;
		return angle;
	}

	if (x < 0) {
		sign = ~sign;
		x = -x;
	}
	if (y < 0) {
		sign = ~sign;
		y = -y;
	}

	if (x == y)
		angle = 45;
	else if (x < y) {
		quotient = ((u32)x << 16) - (u32)x;
		quotient = quotient / y;
		angle = binary_search(tanlist, len, quotient);
	} else {
		quotient = ((u32)y << 16) - (u32)y;
		quotient = quotient / x;
		angle = binary_search(tanlist, len, quotient);
		angle = 90 - angle;
	}
	if (sign == 0)
		return angle;
	else
		return -angle;
}
#endif
#endif


static void tpd_down(s32 x, s32 y, s32 major, s32 min, s32 id, s32 pressure)
{

   input_report_abs(tpd->dev, ABS_MT_PRESSURE, pressure);
   input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, major);
   /* track id Start 0 */
   input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, id);

    input_report_key(tpd->dev, BTN_TOUCH, 1);
    input_report_abs(tpd->dev, ABS_MT_POSITION_X, y);//Touch FW x/y rotation
    input_report_abs(tpd->dev, ABS_MT_POSITION_Y, x);//Touch FW x/y rotation
    input_mt_sync(tpd->dev);
}

static void tpd_up(void)
{
    //input_report_abs(tpd->dev, ABS_MT_PRESSURE, 0);
    input_report_key(tpd->dev, BTN_TOUCH, 0);
    //input_report_abs(tpd->dev, ABS_MT_TOUCH_MAJOR, 0);
    input_mt_sync(tpd->dev);
}


/* Returns time difference between time_later and time_earlier.
 * time is measures in units of jiffies32 */
u32 time_difference(u32 time_later, u32 time_earlier)
{
	u64	time_elapsed;
	if (time_later >= time_earlier)
		time_elapsed = time_later - time_earlier;
	else
		time_elapsed = time_later +
					0x100000000 - time_earlier;
	return (u32)time_elapsed;
}

/* debug_mask |= 0x4 for touch reports */
static void process_report(struct data *ts, u16 *buf)
{
	u32 i, j;
	u16 x, y, swap_u16, curr_finger_ids, tool_type;
#if MAX1187X_TOUCH_REPORT_MODE == 2
	u32 area;
	s16 swap_s16;
	u32 major_axis, minor_axis;
	s16 xsize, ysize, orientation;
#endif

	struct max1187x_touch_report_header *header;
	struct max1187x_touch_report_basic *reportb;
	struct max1187x_touch_report_extended *reporte;

	header = (struct max1187x_touch_report_header *) buf;

	if (BYTEH(header->header) != 0x11){
		printk("max1187x (header->header) != 0x11\n");
		goto err_process_report_header;
	}
#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&ts->client->dev) && ts->is_suspended == 1) {
		TPD_DMESG("Received gesture: (0x%04X)\n", buf[3]);
		if (header->report_id == MAX1187X_REPORT_POWER_MODE
				&& buf[3] == 0x0006) {
			TPD_DMESG("Received touch wakeup report\n");
			input_report_key(ts->input_dev_key,	KEY_POWER, 1);
			input_sync(ts->input_dev_key);
			input_report_key(ts->input_dev_key,	KEY_POWER, 0);
			input_sync(ts->input_dev_key);
		}
		goto process_report_complete;
	}
#endif

	if (header->report_id != MAX1187X_REPORT_TOUCH_BASIC &&
			header->report_id != MAX1187X_REPORT_TOUCH_EXTENDED)
		goto err_process_report_reportid;

	if (ts->framecounter == header->framecounter) {
		TPD_DEBUG("Same framecounter (%u) encountered at irq (%u)!\n",
				ts->framecounter, ts->irq_count);
		goto err_process_report_framecounter;
	}
	ts->framecounter = header->framecounter;

	if (header->button0 != ts->button0) {
		input_report_key(tpd->dev, PDATA(button_code0), header->button0);
		input_sync(tpd->dev);
		ts->button0 = header->button0;
	}
	if (header->button1 != ts->button1) {
		input_report_key(tpd->dev, PDATA(button_code1), header->button1);
		input_sync(tpd->dev);
		ts->button1 = header->button1;
	}
	if (header->button2 != ts->button2) {
		input_report_key(tpd->dev, PDATA(button_code2), header->button2);
		input_sync(tpd->dev);
		ts->button2 = header->button2;
	}
	if (header->button3 != ts->button3) {
		input_report_key(tpd->dev, PDATA(button_code3), header->button3);
		input_sync(tpd->dev);
		ts->button3 = header->button3;
	}

	if (header->touch_count > 10) {
		TPD_DMESG("Touch count (%u) out of bounds [0,10]!\n",
				header->touch_count);
		goto err_process_report_touchcount;
	}

	if (header->touch_count == 0) {
		TPD_DEBUG("(TOUCH): Finger up (all)\n");
#ifdef MAX1187X_PROTOCOL_A
		//input_mt_sync(tpd->dev);
		tpd_up();
#else
		for (i = 0; i < MAX1187X_TOUCH_COUNT_MAX; i++) {
			input_mt_slot(tpd->dev, i);
			input_mt_report_slot_state(tpd->dev,
						MT_TOOL_FINGER, 0);
		}
#endif
		input_sync(tpd->dev);
		ts->list_finger_ids = 0;
	} else {
		curr_finger_ids = 0;
		reportb = (struct max1187x_touch_report_basic *)
				((u8 *)buf + sizeof(*header));
		reporte = (struct max1187x_touch_report_extended *)
				((u8 *)buf + sizeof(*header));
		for (i = 0; i < header->touch_count; i++) {
			x = reportb->x;
			y = reportb->y;
			if (PDATA(coordinate_settings) & MAX1187X_SWAP_XY) {
				swap_u16 = x;
				x = y;
				y = swap_u16;
			}
			if (PDATA(coordinate_settings) & MAX1187X_REVERSE_X) {
				x = PDATA(panel_margin_xl) + PDATA(lcd_x)
					+ PDATA(panel_margin_xh) - 1 - x;
			}
			if (PDATA(coordinate_settings) & MAX1187X_REVERSE_Y) {
				y = PDATA(panel_margin_yl) + PDATA(lcd_y)
					+ PDATA(panel_margin_yh) - 1 - y;
			}
			if (reportb->z == 0)
				reportb->z++;

			tool_type = reportb->tool_type;
			if (tool_type == 1)
				tool_type = MT_TOOL_PEN;
			else
				tool_type = MT_TOOL_FINGER;

			TPD_DEBUG("(TOUCH): (%u) Finger %u: "\
				"X(%d) Y(%d) Z(%d) TT(%d)\n",
				header->framecounter, reportb->finger_id,
				x, y, reportb->z, tool_type);
			curr_finger_ids |= (1<<reportb->finger_id);
#ifdef MAX1187X_PROTOCOL_A
			//input_report_abs(tpd->dev,ABS_MT_TRACKING_ID,	reportb->finger_id);
			//input_report_abs(tpd->dev,ABS_MT_TOOL_TYPE, tool_type);
#else
			input_mt_slot(tpd->dev, reportb->finger_id);
			input_mt_report_slot_state(tpd->dev,
					tool_type, 1);
#endif
			//input_report_abs(tpd->dev, ABS_MT_POSITION_X, y);//Touch FW x/y rotation
			//input_report_abs(tpd->dev,	ABS_MT_POSITION_Y, x);//Touch FW x/y rotation
			//input_report_abs(tpd->dev,ABS_MT_PRESSURE, reportb->z);
			if (header->report_id
				== MAX1187X_REPORT_TOUCH_EXTENDED) {
#if MAX1187X_TOUCH_REPORT_MODE == 2
				if (PDATA(coordinate_settings)
					& MAX1187X_SWAP_XY) {
					swap_s16 = reporte->xpixel;
					reporte->xpixel = reporte->ypixel;
					reporte->ypixel = swap_s16;
				}
				if (PDATA(coordinate_settings)
						& MAX1187X_REVERSE_X)
					reporte->xpixel = -reporte->xpixel;
				if (PDATA(coordinate_settings)
						& MAX1187X_REVERSE_Y)
					reporte->ypixel = -reporte->ypixel;
				area = reporte->area
					* (PDATA(lcd_x)/PDATA(num_sensor_x))
					* (PDATA(lcd_y)/PDATA(num_sensor_y));
				xsize = reporte->xpixel
				* (s16)(PDATA(lcd_x)/PDATA(num_sensor_x));
				ysize = reporte->ypixel
				* (s16)(PDATA(lcd_y)/PDATA(num_sensor_y));
				TPD_DEBUG("(TOUCH): pixelarea (%u) " \
					"xpixel (%d) ypixel (%d) " \
					"xsize (%d) ysize (%d)\n",
					reporte->area,
					reporte->xpixel, reporte->ypixel,
					xsize, ysize);

#ifndef MAX1187X_REPORT_FAST_CALCULATION
				/* Calculate orientation as
				 * arctan of xsize/ysize) */
				orientation =
					max1187x_orientation(xsize, ysize);
				/* Major axis of ellipse if hypotenuse
				 * formed by xsize and ysize */
				major_axis = xsize*xsize + ysize*ysize;
				major_axis = max1187x_sqrt(major_axis);
				/* Minor axis can be reverse calculated
				 * using the area of ellipse:
				 * Area of ellipse =
				 *		pi / 4 * Major axis * Minor axis
				 * Minor axis =
				 *		4 * Area / (pi * Major axis)
				 */
				minor_axis = (2 * area) / major_axis;
				minor_axis = (minor_axis<<17) / MAX1187X_PI;
#else
				if (xsize < 0)
					xsize = -xsize;
				if (ysize < 0)
					ysize = -ysize;
				orientation = (xsize > ysize) ? 0 : 90;
				major_axis = (xsize > ysize) ? xsize : ysize;
				minor_axis = (xsize > ysize) ? ysize : xsize;
#endif
				TPD_DEBUG("(TOUCH): Finger %u: " \
					"Orientation(%d) Area(%u) Major_axis(%u) Minor_axis(%u)\n",
					reportb->finger_id,	orientation,
					area, major_axis, minor_axis);
				//input_report_abs(tpd->dev,ABS_MT_ORIENTATION, orientation);
				//input_report_abs(tpd->dev,ABS_MT_TOUCH_MAJOR, major_axis);
				//input_report_abs(tpd->dev,ABS_MT_TOUCH_MINOR, minor_axis);
#endif
				reporte++;
				reportb = (struct max1187x_touch_report_basic *)
						((u8 *) reporte);
			} else {
				reportb++;
			}
#ifdef MAX1187X_PROTOCOL_A
            tpd_down(x, y, 0, 0, reportb->finger_id, reportb->z);
			//tpd_down(s32 x, s32 y, s32 major, s32 min, s32 id, s32 pressure);

			//input_mt_sync(tpd->dev);
#endif
		}

		i = 0;
		j = 1;
		while (i < 10) {
			if ((ts->list_finger_ids & j) != 0 &&
					(curr_finger_ids & j) == 0) {
				TPD_DEBUG("(TOUCH): Finger up (%d)\n", i);
#ifndef MAX1187X_PROTOCOL_A
				input_mt_slot(tpd->dev, i);
				input_mt_report_slot_state(tpd->dev,
						MT_TOOL_FINGER, 0);
#endif
			}
			i++;
			j <<= 1;
		}

		input_sync(tpd->dev);
		ts->list_finger_ids = curr_finger_ids;
	}

#ifdef TOUCH_WAKEUP_FEATURE
process_report_complete:
#endif
	return;

err_process_report_touchcount:
err_process_report_header:
err_process_report_reportid:
err_process_report_framecounter:
	return;
}

static void max1187x_wfxn_irq(struct work_struct *work)
{
	//struct data *ts = container_of(work, struct data, work_irq);
	int ret;
	u32	time_elapsed;

	//if (gpio_get_value(ts->pdata->gpio_tirq) != 0)
		//return;

	ret = read_mtp_report(ts, ts->rx_packet);

	time_elapsed = time_difference(jiffies, ts->irq_receive_time);

	/* Verify time_elapsed < 1s */
	//if (ret == 0 || time_elapsed > HZ) {
		process_report(ts, ts->rx_packet);
		propagate_report(ts, 0, ts->rx_packet);
	//}
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, 1);
	
	msleep(10);
	
#ifdef CONFIG_OF_TOUCH
	enable_irq(ts->client->irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

}

#ifdef CONFIG_OF_TOUCH
/* debug_mask |= 0x20 for irq_handler */
static irqreturn_t irq_handler(int irq, void *context)
{
	struct data *ts = (struct data *) context;

	TPD_DEBUG("Enter\n");

	//if (gpio_get_value(ts->pdata->gpio_tirq) != 0)
		//goto irq_handler_complete;

	disable_irq_nosync(ts->client->irq);
	ts->irq_receive_time = jiffies;
	ts->irq_count++;

	queue_work(ts->wq, &ts->work_irq);

irq_handler_complete:
	TPD_DEBUG("Exit\n");
	return IRQ_HANDLED;
}
#else
static void irq_handler(void)
{
	TPD_DEBUG("Enter\n");

	if(ts == NULL)
		goto irq_handler_complete;
	
	//if (gpio_get_value(ts->pdata->gpio_tirq) != 0)
		//goto irq_handler_complete;
	
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
	ts->irq_receive_time = jiffies;
	ts->irq_count++;
	
	queue_work(ts->wq, &ts->work_irq);
	
irq_handler_complete:
	TPD_DEBUG("Exit\n");
	return IRQ_HANDLED;

}

#endif
static ssize_t init_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", init_state);
}

static ssize_t init_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int value, ret;

	if (sscanf(buf, "%d", &value) != 1) {
		TPD_DMESG("bad parameter\n");
		return -EINVAL;
	}
	switch (value) {
	case 0:
		if (init_state == 0)
			break;
		ret = device_deinit(to_i2c_client(dev));
		if (ret != 0) {
			TPD_DMESG("deinit error (%d)\n", ret);
			return ret;
		}
		break;
	case 1:
		if (init_state == 1)
			break;
		ret = device_init(to_i2c_client(dev));
		if (ret != 0) {
			TPD_DMESG("init error (%d)\n", ret);
			return ret;
		}
		break;
	case 2:
		if (init_state == 1) {
			ret = device_deinit(to_i2c_client(dev));
			if (ret != 0) {
				TPD_DMESG("deinit error (%d)\n", ret);
				return ret;
			}
		}
		ret = device_init(to_i2c_client(dev));
		if (ret != 0) {
			TPD_DMESG("init error (%d)\n", ret);
			return ret;
		}
		break;
	default:
		TPD_DMESG("bad value\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t sreset_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);
	u16 cmd_buf[] = {0x00E9, 0x0000};
	int ret;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x01A0, 3 * HZ);
	if (ret)
		TPD_DMESG("Failed to do soft reset.\n");
	return count;
}

static ssize_t irq_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u\n", ts->irq_count);
}

static ssize_t irq_count_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	ts->irq_count = 0;
	return count;
}

static ssize_t dflt_cfg_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u 0x%x 0x%x\n", PDATA(defaults_allow),
			PDATA(default_config_id), PDATA(default_chip_id));
}

static ssize_t dflt_cfg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	(void) sscanf(buf, "%u 0x%x 0x%x", &PDATA(defaults_allow),
			&PDATA(default_config_id), &PDATA(default_chip_id));
	return count;
}

static ssize_t panel_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	return snprintf(buf, PAGE_SIZE, "%u %u %u %u %u %u\n",
			PDATA(panel_margin_xl), PDATA(panel_margin_xh),
			PDATA(panel_margin_yl), PDATA(panel_margin_yh),
			PDATA(lcd_x), PDATA(lcd_y));
}

static ssize_t panel_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	(void) sscanf(buf, "%u %u %u %u %u %u", &PDATA(panel_margin_xl),
			&PDATA(panel_margin_xh), &PDATA(panel_margin_yl),
			&PDATA(panel_margin_yh), &PDATA(lcd_x),
			&PDATA(lcd_y));
	return count;
}

static ssize_t fw_ver_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);

	int ret, count = 0;
	u16 cmd_buf[2];

	/* Read firmware version */
	cmd_buf[0] = 0x0040;
	cmd_buf[1] = 0x0000;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0140, HZ/4);

	if (ret)
		goto err_fw_ver_show;

	ts->chip_id = BYTEH(ts->rbcmd_rx_report[4]);
	count += snprintf(buf, PAGE_SIZE, "fw_ver (%u.%u.%u) " \
					"chip_id (0x%02X)\n",
					BYTEH(ts->rbcmd_rx_report[3]),
					BYTEL(ts->rbcmd_rx_report[3]),
					ts->rbcmd_rx_report[5],
					ts->chip_id);

	/* Read touch configuration */
	cmd_buf[0] = 0x0002;
	cmd_buf[1] = 0x0000;

	ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0102, HZ/4);

	if (ret) {
		TPD_DMESG("Failed to receive chip config\n");
		goto err_fw_ver_show;
	}

	ts->config_id = ts->rbcmd_rx_report[3];

	count += snprintf(buf + count, PAGE_SIZE, "config_id (0x%04X) ",
					ts->config_id);
	count += snprintf(buf + count, PAGE_SIZE,
			"customer_info[1:0] (0x%04X, 0x%04X)\n",
					ts->rbcmd_rx_report[43],
					ts->rbcmd_rx_report[42]);
	return count;

err_fw_ver_show:
	return count;
}

static ssize_t driver_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "3.1.7: May 9, 2013\n");
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%04X\n", debug_mask);
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (sscanf(buf, "%hx", &debug_mask) != 1) {
		TPD_DMESG("bad parameter\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t command_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct data *ts = i2c_get_clientdata(client);
	u16 buffer[CMD_LEN_MAX];
	char scan_buf[5];
	int i, ret;

	count--; /* ignore carriage return */
	if ((count % 4) != 0) {
		TPD_DMESG("words not properly defined\n");
		return -EINVAL;
	}
	scan_buf[4] = '\0';
	for (i = 0; i < count; i += 4) {
		memcpy(scan_buf, &buf[i], 4);
		if (sscanf(scan_buf, "%hx", &buffer[i / 4]) != 1) {
			TPD_DMESG("bad word (%s)\n", scan_buf);
			return -EINVAL;
		}
	}
	ret = cmd_send(ts, buffer, count / 4);
	if (ret)
		TPD_DMESG("MTP command failed\n");
	return ++count;
}

static ssize_t report_read(struct file *file, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	struct data *ts = i2c_get_clientdata(client);
	int printed, i, offset = 0, payload;
	int full_packet;
	int num_term_char;

	if (get_report(ts, 0xFFFF, 0xFFFFFFFF))
		return 0;

	payload = ts->rx_report_len;
	full_packet = payload;
	num_term_char = 2; /* number of term char */
	if (count < (4 * full_packet + num_term_char))
		return -EIO;
	if (count > (4 * full_packet + num_term_char))
		count = 4 * full_packet + num_term_char;

	for (i = 1; i <= payload; i++) {
		printed = snprintf(&buf[offset], PAGE_SIZE, "%04X\n",
			ts->rx_report[i]);
		if (printed <= 0)
			return -EIO;
		offset += printed - 1;
	}
	snprintf(&buf[offset], PAGE_SIZE, ",\n");
	release_report(ts);

	return count;
}

static DEVICE_ATTR(init, S_IRUGO | S_IWUSR, init_show, init_store);
static DEVICE_ATTR(sreset, S_IWUSR, NULL, sreset_store);
static DEVICE_ATTR(irq_count, S_IRUGO | S_IWUSR, irq_count_show,
		irq_count_store);
static DEVICE_ATTR(dflt_cfg, S_IRUGO | S_IWUSR, dflt_cfg_show, dflt_cfg_store);
static DEVICE_ATTR(panel, S_IRUGO | S_IWUSR, panel_show, panel_store);
static DEVICE_ATTR(fw_ver, S_IRUGO, fw_ver_show, NULL);
static DEVICE_ATTR(driver_ver, S_IRUGO, driver_ver_show, NULL);
static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, debug_show, debug_store);
static DEVICE_ATTR(command, S_IWUSR, NULL, command_store);
static struct bin_attribute dev_attr_report = {
		.attr = {.name = "report", .mode = S_IRUGO},
		.read = report_read };

static struct device_attribute *dev_attrs[] = {
		&dev_attr_sreset,
		&dev_attr_irq_count,
		&dev_attr_dflt_cfg,
		&dev_attr_panel,
		&dev_attr_fw_ver,
		&dev_attr_driver_ver,
		&dev_attr_debug,
		&dev_attr_command,
		NULL };

/* Send command to chip.
 */
static int cmd_send(struct data *ts, u16 *buf, u16 len)
{
	int ret;

	ret = down_interruptible(&ts->sema_cmd);
	if (ret != 0)
		goto err_cmd_send_sema_cmd;

	memcpy(ts->cmd_buf, buf, len * sizeof(buf[0]));
	ts->cmd_len = len;
	queue_work(ts->wq, &ts->work_cmd);

	return 0;

err_cmd_send_sema_cmd:
	return -ERESTARTSYS;
}

/* Send command to chip and expect a report with
 * id == report_id within timeout time.
 * timeout is measured in jiffies. 1s = HZ jiffies
 */
static int rbcmd_send_receive(struct data *ts, u16 *buf,
		u16 len, u16 report_id, u16 timeout)
{
	int ret;

	ret = down_interruptible(&ts->sema_rbcmd);
	if (ret != 0)
		goto err_rbcmd_send_receive_sema_rbcmd;

	ts->rbcmd_report_id = report_id;
	ts->rbcmd_received = 0;
	ts->rbcmd_waiting = 1;

	ret = cmd_send(ts, buf, len);
	if (ret)
		goto err_rbcmd_send_receive_cmd_send;

	ret = wait_event_interruptible_timeout(ts->waitqueue_rbcmd,
			ts->rbcmd_received != 0, timeout);
	if (ret < 0 || ts->rbcmd_received == 0)
		goto err_rbcmd_send_receive_timeout;

	ts->rbcmd_waiting = 0;
	up(&ts->sema_rbcmd);

	return 0;

err_rbcmd_send_receive_timeout:
err_rbcmd_send_receive_cmd_send:
	ts->rbcmd_waiting = 0;
	up(&ts->sema_rbcmd);
err_rbcmd_send_receive_sema_rbcmd:
	return -ERESTARTSYS;
}

/* debug_mask |= 0x8 for all driver INIT */
static int read_chip_data(struct data *ts)
{
	int ret;
	u16 loopcounter;
	u16 cmd_buf[2];

	/* Read firmware version */
	cmd_buf[0] = 0x0040;
	cmd_buf[1] = 0x0000;

	loopcounter = 0;
	ret = -1;
	while (loopcounter < MAX_FW_RETRIES && ret != 0) {
		ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0140, HZ/4);
		loopcounter++;
	}

	if (ret) {
		TPD_DMESG("Failed to receive fw version\n");
		goto err_read_chip_data;
	}

	ts->chip_id = BYTEH(ts->rbcmd_rx_report[4]);
	TPD_DMESG("(INIT): fw_ver (%u.%u) " \
					"chip_id (0x%02X)\n",
					BYTEH(ts->rbcmd_rx_report[3]),
					BYTEL(ts->rbcmd_rx_report[3]),
					ts->chip_id);

	/* Read touch configuration */
	cmd_buf[0] = 0x0002;
	cmd_buf[1] = 0x0000;

	loopcounter = 0;
	ret = -1;
	while (loopcounter < MAX_FW_RETRIES && ret != 0) {
		ret = rbcmd_send_receive(ts, cmd_buf, 2, 0x0102, HZ/4);
		loopcounter++;
	}

	if (ret) {
		TPD_DMESG("Failed to receive chip config\n");
		goto err_read_chip_data;
	}

	ts->config_id = ts->rbcmd_rx_report[3];

	TPD_DMESG("(INIT): config_id (0x%04X)\n",
					ts->config_id);
	return 0;

err_read_chip_data:
	return ret;
}

static int device_fw_load(struct data *ts, const struct firmware *fw,
	u16 fw_index)
{
	u16 filesize, file_codesize, loopcounter;
	u16 file_crc16_1, file_crc16_2, local_crc16;
	int chip_crc16_1 = -1, chip_crc16_2 = -1, ret;

	filesize = PDATA(fw_mapping[fw_index]).filesize;
	file_codesize = PDATA(fw_mapping[fw_index]).file_codesize;

	if (fw->size != filesize) {
		TPD_DMESG("filesize (%ld) is not equal to expected size (%d)\n",
				(long)fw->size, filesize);
		return -EIO;
	}

	file_crc16_1 = crc16(0, fw->data, file_codesize);

	loopcounter = 0;
	do {
		ret = bootloader_enter(ts);
		if (ret == 0)
			ret = bootloader_get_crc(ts, &local_crc16,
				0, file_codesize, 200);
		if (ret == 0)
			chip_crc16_1 = local_crc16;
		ret = bootloader_exit(ts);
		loopcounter++;
	} while (loopcounter < MAX_FW_RETRIES && chip_crc16_1 == -1);

	TPD_DMESG("(INIT): file_crc16_1 = 0x%04x, chip_crc16_1 = 0x%04x\n",
			file_crc16_1, chip_crc16_1);

	if (file_crc16_1 != chip_crc16_1) {
		loopcounter = 0;
		file_crc16_2 = crc16(0, fw->data, filesize);

		while (loopcounter < MAX_FW_RETRIES && file_crc16_2
				!= chip_crc16_2) {
			TPD_DMESG("(INIT): Reprogramming chip. Attempt %d\n",
					loopcounter+1);
			ret = bootloader_enter(ts);
			if (ret == 0)
				ret = bootloader_erase_flash(ts);
			if (ret == 0)
				ret = bootloader_set_byte_mode(ts);
			if (ret == 0)
				ret = bootloader_write_flash(ts, fw->data,
					filesize);
			if (ret == 0)
				ret = bootloader_get_crc(ts, &local_crc16,
					0, filesize, 200);
			if (ret == 0)
				chip_crc16_2 = local_crc16;
			TPD_DMESG("(INIT): file_crc16_2 = 0x%04x, "\
					"chip_crc16_2 = 0x%04x\n",
					file_crc16_2, chip_crc16_2);
			ret = bootloader_exit(ts);
			loopcounter++;
		}

		if (file_crc16_2 != chip_crc16_2)
			return -EAGAIN;
	}

	loopcounter = 0;
	do {
		ret = bootloader_exit(ts);
		loopcounter++;
	} while (loopcounter < MAX_FW_RETRIES && ret != 0);

	if (ret != 0)
		return -EIO;

	return 0;
}


static void validate_fw(struct data *ts)
{
	const struct firmware *fw;
	u16 config_id, chip_id;
	int i, ret;
	u16 cmd_buf[3];

#if 0
	ret = read_chip_data(ts);
	if (ret && PDATA(defaults_allow) == 0) {
		TPD_DMESG("Firmware is not responsive "\
				"and default update is disabled\n");
		return;
	}
#endif

	if (ts->chip_id != 0)
		chip_id = ts->chip_id;
	else
		chip_id = PDATA(default_chip_id);

	if (ts->config_id != 0)
		config_id = ts->config_id;
	else
		config_id = PDATA(default_config_id);

#ifdef FW_DOWNLOAD_FEATURE

	for (i = 0; i < PDATA(num_fw_mappings); i++) {
		if (PDATA(fw_mapping[i]).config_id == config_id &&
			PDATA(fw_mapping[i]).chip_id == chip_id)
			break;
	}

	if (i == PDATA(num_fw_mappings)) {
		TPD_DMESG("FW not found for configID(0x%04X) and chipID(0x%04X)\n",
			config_id, chip_id);
		return;
	}

	TPD_DMESG("(INIT): Firmware file (%s)\n",
		PDATA(fw_mapping[i]).filename);

	ret = request_firmware(&fw, PDATA(fw_mapping[i]).filename,
					&ts->client->dev);

	if (ret || fw == NULL) {
		TPD_DMESG("firmware request failed (ret = %d, fwptr = %p)\n",
			ret, fw);
		return;
	}

	ret = down_interruptible(&ts->sema_cmd);
	if (ret) {
		release_firmware(fw);
		TPD_DMESG("Could not get lock\n");
		return;
	}
	
#ifdef CONFIG_OF_TOUCH
	disable_irq(ts->client->irq);
#else
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
	
	flush_workqueue(ts->wq);
	if (device_fw_load(ts, fw, i)) {
		release_firmware(fw);
		TPD_DMESG("firmware download failed\n");
		
#ifdef CONFIG_OF_TOUCH
	    enable_irq(ts->client->irq);
#else
	    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif		
		up(&ts->sema_cmd);
		return;
	}

	release_firmware(fw);
	TPD_DMESG("(INIT): firmware okay\n");
#ifdef CONFIG_OF_TOUCH
	enable_irq(ts->client->irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

	up(&ts->sema_cmd);
	ret = read_chip_data(ts);

#endif

	cmd_buf[0] = 0x0018;
	cmd_buf[1] = 0x0001;
	cmd_buf[2] = MAX1187X_TOUCH_REPORT_MODE;
	ret = cmd_send(ts, cmd_buf, 3);
	if (ret) {
		TPD_DMESG("Failed to set up touch report mode\n");
		return;
	}
}

/* #ifdef CONFIG_OF */
static struct max1187x_pdata *max1187x_get_platdata_dt(struct device *dev)
{
	struct max1187x_pdata *pdata = NULL;
	struct device_node *devnode = dev->of_node;
	u32 i;
	u32 datalist[MAX1187X_NUM_FW_MAPPINGS_MAX];

	if (!devnode)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		TPD_DMESG("Failed to allocate memory for pdata\n");
		return NULL;
	}

	/* Parse gpio_tirq */
	if (of_property_read_u32(devnode, "gpio_tirq", &pdata->gpio_tirq)) {
		TPD_DMESG("Failed to get property: gpio_tirq\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse num_fw_mappings */
	if (of_property_read_u32(devnode, "num_fw_mappings",
		&pdata->num_fw_mappings)) {
		TPD_DMESG("Failed to get property: num_fw_mappings\n");
		goto err_max1187x_get_platdata_dt;
	}

	if (pdata->num_fw_mappings > MAX1187X_NUM_FW_MAPPINGS_MAX)
		pdata->num_fw_mappings = MAX1187X_NUM_FW_MAPPINGS_MAX;

	/* Parse config_id */
	if (of_property_read_u32_array(devnode, "config_id", datalist,
			pdata->num_fw_mappings)) {
		TPD_DMESG("Failed to get property: config_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].config_id = datalist[i];

	/* Parse chip_id */
	if (of_property_read_u32_array(devnode, "chip_id", datalist,
			pdata->num_fw_mappings)) {
		TPD_DMESG("Failed to get property: chip_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].chip_id = datalist[i];

	/* Parse filename */
	for (i = 0; i < pdata->num_fw_mappings; i++) {
		if (of_property_read_string_index(devnode, "filename", i,
			(const char **) &pdata->fw_mapping[i].filename)) {
				TPD_DMESG("Failed to get property: "\
					"filename[%d]\n", i);
				goto err_max1187x_get_platdata_dt;
			}
	}

	/* Parse filesize */
	if (of_property_read_u32_array(devnode, "filesize", datalist,
		pdata->num_fw_mappings)) {
		TPD_DMESG("Failed to get property: filesize\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].filesize = datalist[i];

	/* Parse file_codesize */
	if (of_property_read_u32_array(devnode, "file_codesize", datalist,
		pdata->num_fw_mappings)) {
		TPD_DMESG("Failed to get property: file_codesize\n");
		goto err_max1187x_get_platdata_dt;
	}

	for (i = 0; i < pdata->num_fw_mappings; i++)
		pdata->fw_mapping[i].file_codesize = datalist[i];

	/* Parse defaults_allow */
	if (of_property_read_u32(devnode, "defaults_allow",
		&pdata->defaults_allow)) {
		TPD_DMESG("Failed to get property: defaults_allow\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse default_config_id */
	if (of_property_read_u32(devnode, "default_config_id",
		&pdata->default_config_id)) {
		TPD_DMESG("Failed to get property: default_config_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse default_chip_id */
	if (of_property_read_u32(devnode, "default_chip_id",
		&pdata->default_chip_id)) {
		TPD_DMESG("Failed to get property: default_chip_id\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse i2c_words */
	if (of_property_read_u32(devnode, "i2c_words", &pdata->i2c_words)) {
		TPD_DMESG("Failed to get property: i2c_words\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse coordinate_settings */
	if (of_property_read_u32(devnode, "coordinate_settings",
		&pdata->coordinate_settings)) {
		TPD_DMESG("Failed to get property: coordinate_settings\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_xl */
	if (of_property_read_u32(devnode, "panel_margin_xl",
		&pdata->panel_margin_xl)) {
		TPD_DMESG("Failed to get property: panel_margin_xl\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse lcd_x */
	if (of_property_read_u32(devnode, "lcd_x", &pdata->lcd_x)) {
		TPD_DMESG("Failed to get property: lcd_x\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_xh */
	if (of_property_read_u32(devnode, "panel_margin_xh",
		&pdata->panel_margin_xh)) {
		TPD_DMESG("Failed to get property: panel_margin_xh\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_yl */
	if (of_property_read_u32(devnode, "panel_margin_yl",
		&pdata->panel_margin_yl)) {
		TPD_DMESG("Failed to get property: panel_margin_yl\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse lcd_y */
	if (of_property_read_u32(devnode, "lcd_y", &pdata->lcd_y)) {
		TPD_DMESG("Failed to get property: lcd_y\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse panel_margin_yh */
	if (of_property_read_u32(devnode, "panel_margin_yh",
		&pdata->panel_margin_yh)) {
		TPD_DMESG("Failed to get property: panel_margin_yh\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse row_count */
	if (of_property_read_u32(devnode, "num_sensor_x",
		&pdata->num_sensor_x)) {
		TPD_DMESG("Failed to get property: num_sensor_x\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse num_sensor_y */
	if (of_property_read_u32(devnode, "num_sensor_y",
		&pdata->num_sensor_y)) {
		TPD_DMESG("Failed to get property: num_sensor_y\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code0 */
	if (of_property_read_u32(devnode, "button_code0",
		&pdata->button_code0)) {
		TPD_DMESG("Failed to get property: button_code0\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code1 */
	if (of_property_read_u32(devnode, "button_code1",
		&pdata->button_code1)) {
		TPD_DMESG("Failed to get property: button_code1\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code2 */
	if (of_property_read_u32(devnode, "button_code2",
		&pdata->button_code2)) {
		TPD_DMESG("Failed to get property: button_code2\n");
		goto err_max1187x_get_platdata_dt;
	}

	/* Parse button_code3 */
	if (of_property_read_u32(devnode, "button_code3",
		&pdata->button_code3)) {
		TPD_DMESG("Failed to get property: button_code3\n");
		goto err_max1187x_get_platdata_dt;
	}

	return pdata;

err_max1187x_get_platdata_dt:
	devm_kfree(dev, pdata);
	return NULL;
}
/*
#else
static inline struct max1187x_pdata *
	max1187x_get_platdata_dt(struct device *dev)
{
	return NULL;
}
#endif
*/

static int validate_pdata(struct max1187x_pdata *pdata)
{
	if (pdata == NULL) {
		TPD_DMESG("Platform data not found!\n");
		goto err_validate_pdata;
	}

	if (pdata->gpio_tirq == 0) {
		TPD_DMESG("gpio_tirq (%u) not defined!\n", pdata->gpio_tirq);
		goto err_validate_pdata;
	}

	if (pdata->lcd_x < 480 || pdata->lcd_x > 0x7FFF) {
		TPD_DMESG("lcd_x (%u) out of range!\n", pdata->lcd_x);
		goto err_validate_pdata;
	}

	if (pdata->lcd_y < 240 || pdata->lcd_y > 0x7FFF) {
		TPD_DMESG("lcd_y (%u) out of range!\n", pdata->lcd_y);
		goto err_validate_pdata;
	}

	if (pdata->num_sensor_x == 0 || pdata->num_sensor_x > 40) {
		TPD_DMESG("num_sensor_x (%u) out of range!\n",
				pdata->num_sensor_x);
		goto err_validate_pdata;
	}

	if (pdata->num_sensor_y == 0 || pdata->num_sensor_y > 40) {
		TPD_DMESG("num_sensor_y (%u) out of range!\n",
				pdata->num_sensor_y);
		goto err_validate_pdata;
	}

	return 0;

err_validate_pdata:
	return -ENXIO;
}

static int max1187x_auto_detect_check(struct data *ts)
{
	u8 buffer[] = { 0x00, 0x00, 0x00, 0x00 };
	int length = 0;
	u16 buff[245] = {0};
	mutex_lock(&ts->i2c_mutex);
	if (i2c_tx_b(ts, buffer, 2) < 0) {
		mutex_unlock(&ts->i2c_mutex);
		TPD_DMESG("max1187x_auto_detect_check RX fail\n");
		return -EIO;
	}
	mutex_unlock(&ts->i2c_mutex);

	/*
	u8 buffer1[] = {0x40, 0x00, 0x00, 0x00};
	
	if (i2c_tx_b(ts, buffer1, 2) < 0) {
		TPD_DMESG("TX fail");
		return -EIO;
	}

	u16 buffer2[] = {0x000A, 0x0000, 0x0000, 0x0000};

	if (i2c_rx_w(ts, buffer2, 2) < 0) {
		TPD_DMESG("RX fail\n");
		return -EIO;
	}	

	length = BYTEL(buffer2[0]) + 1;
	printk("max1187x read length: %d\n", length);
	printk("max1187x: 0x%x, 0x%x, 0x%x, 0x%x\n", buffer2[0], buffer2[1], buffer2[2], buffer2[3]);
	
	buff[0] = 0x000B;
	if (i2c_rx_w(ts, buff, length) < 0) {
		TPD_DMESG("RX fail\n");
		return -EIO;
	}

	printk("max1187x: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6]);
	*/
	check_flag = true;
	tpd_load_status = 1;
	return 0;
}

static int max1187x_chip_init(void)
{
	// set INT mode
	mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CTP_RST_PIN, 1);
	msleep(50);

	
	mt_set_gpio_mode(GPIO_CTP_EINT_PIN, GPIO_CTP_EINT_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_CTP_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_CTP_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_CTP_EINT_PIN, 1);
	msleep(50);
	return 0;
}

#ifdef CONFIG_OF_TOUCH 
static int tpd_irq_registration(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = {0,0};
	
	node = of_find_compatible_node(NULL, NULL, "mediatek, TOUCH_PANEL-eint");
	if(node){
		of_property_read_u32_array(node , "debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		irq = irq_of_parse_and_map(node, 0);

		ret = request_irq(irq, handler, flags, "TOUCH_PANEL-eint", dev);
		if(ret > 0){
		    ret = -1;
		    printk("tpd request_irq IRQ LINE NOT AVAILABLE!.");
		}
	}else{
		printk("tpd request_irq can not find touch eint device node!.");
		ret = -1;
	}
	printk("[%s]irq:%d, debounce:%d-%d:", __FUNCTION__, irq, ints[0], ints[1]);
	return ret;
}
#endif

static int device_init_thread(void *arg)
{
	return device_init((struct i2c_client *) arg);
}

static int device_init(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct max1187x_pdata *pdata = NULL;
	struct device_attribute **dev_attr = dev_attrs;
	int ret = 0;

	init_state = 1;
	dev_info(dev, "(INIT): Start");

	/* if I2C functionality is not present we are done */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TPD_DMESG("I2C core driver does not support I2C functionality\n");
		ret = -ENXIO;
		goto err_device_init;
	}
	TPD_DMESG("(INIT): I2C functionality OK\n");

	/* allocate control block; nothing more to do if we can't */
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (!ts) {
		TPD_DMESG("Failed to allocate control block memory\n");
		ret = -ENOMEM;
		goto err_device_init;
	}

	/* Get platform data  this is used to get related custom data for touchpanel*/
#ifdef MAX1187X_LOCAL_PDATA
	pdata = &local_pdata;
	if (!pdata) {
		TPD_DMESG("Platform data is missing\n");
		ret = -ENXIO;
		goto err_device_init_pdata;
	}
#else
	pdata = dev_get_platdata(dev);
	/* If pdata is missing, try to get pdata from device tree (dts) */
	if (!pdata)
		pdata = max1187x_get_platdata_dt(dev);

	/* Validate if pdata values are okay */
	ret = validate_pdata(pdata);
	if (ret < 0)
		goto err_device_init_pdata;
	TPD_DMESG("(INIT): Platform data OK\n");
#endif

	ts->pdata = pdata;
	ts->client = client;
	i2c_set_clientdata(client, ts);
	mutex_init(&ts->irq_mutex);
	mutex_init(&ts->i2c_mutex);
	mutex_init(&ts->report_mutex);
	sema_init(&ts->report_sem, 1);
	sema_init(&ts->sema_cmd, 1);
	sema_init(&ts->sema_rbcmd, 1);
	ts->button0 = 0;
	ts->button1 = 0;
	ts->button2 = 0;
	ts->button3 = 0;

	/* Create workqueue with maximum 1 running task */
	ts->wq = alloc_workqueue("max1187x_wq", WQ_UNBOUND | WQ_HIGHPRI, 1);
	if (ts->wq == NULL) {
		TPD_DMESG("Not able to create workqueue\n");
		ret = -ENOMEM;
		goto err_device_init_memalloc;
	}
	INIT_WORK(&ts->work_irq, max1187x_wfxn_irq);
	INIT_WORK(&ts->work_cmd, max1187x_wfxn_cmd);
	init_waitqueue_head(&ts->waitqueue_all);
	init_waitqueue_head(&ts->waitqueue_rbcmd);

#if GTP_SUPPORT_I2C_DMA
	tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, GTP_DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
	if(!gpDMABuf_va){
		TPD_DMESG("[Error] Allocate DMA I2C Buffer failed!\n");
	}
	memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#endif

	TPD_DMESG("(INIT): Memory allocation OK\n");

	/* Initialize GPIO pins */
	if (max1187x_chip_init() < 0) {
		ret = -EIO;
		goto err_device_init_gpio;
	}
	TPD_DMESG("(INIT): chip GPIO init OK\n");

	/*chip i2c fucntion check for touch panel auto detect*/
	if(max1187x_auto_detect_check(ts) < 0){
		ret = -EIO;
		goto err_device_init_gpio;
	}
	TPD_DMESG("(INIT): chip detect OK\n");

	/* allocate and register touch device */
	//no need to allocate input device because of we will use MTK common input device to report input events
	/*
	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		TPD_DMESG("Failed to allocate touch input device");
		ret = -ENOMEM;
		goto err_device_init_alloc_inputdev;
	}
	snprintf(ts->phys, sizeof(ts->phys), "%s/input0",
			dev_name(dev));
	ts->input_dev->name = MAX1187X_TOUCH;
	ts->input_dev->phys = ts->phys;
	ts->input_dev->id.bustype = BUS_I2C;
	__set_bit(EV_SYN, ts->input_dev->evbit);
	__set_bit(EV_ABS, ts->input_dev->evbit);
	__set_bit(EV_KEY, ts->input_dev->evbit);

#ifdef MAX1187X_PROTOCOL_A
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);
#else
	input_mt_init_slots(ts->input_dev, MAX1187X_TOUCH_COUNT_MAX, INPUT_MT_POINTER);
#endif
	*/
	ts->list_finger_ids = 0;
	/*
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			PDATA(panel_margin_xl),
			PDATA(panel_margin_xl) + PDATA(lcd_x), 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			PDATA(panel_margin_yl),
			PDATA(panel_margin_yl) + PDATA(lcd_y), 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOOL_TYPE,
			0, MT_TOOL_MAX, 0, 0);
#if MAX1187X_TOUCH_REPORT_MODE == 2
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
			0, PDATA(lcd_x) + PDATA(lcd_y), 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR,
			0, PDATA(lcd_x) + PDATA(lcd_y), 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_ORIENTATION, -90, 90, 0, 0);
#endif
	//this is for touch key report
	if (PDATA(button_code0) != KEY_RESERVED)
		set_bit(pdata->button_code0, ts->input_dev->keybit);
	if (PDATA(button_code1) != KEY_RESERVED)
		set_bit(pdata->button_code1, ts->input_dev->keybit);
	if (PDATA(button_code2) != KEY_RESERVED)
		set_bit(pdata->button_code2, ts->input_dev->keybit);
	if (PDATA(button_code3) != KEY_RESERVED)
		set_bit(pdata->button_code3, ts->input_dev->keybit);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		TPD_DMESG("Failed to register touch input device");
		ret = -EPERM;
		goto err_device_init_register_inputdev;
	}
	TPD_DMESG("(INIT): Input touch device OK");
	*/
#ifdef TOUCH_WAKEUP_FEATURE
	ts->input_dev_key = input_allocate_device();
	if (!ts->input_dev_key) {
		TPD_DMESG("Failed to allocate touch input key device\n");
		ret = -ENOMEM;
		goto err_device_init_alloc_inputdevkey;
	}
	snprintf(ts->phys_key, sizeof(ts->phys_key), "%s/input1",
		dev_name(&client->dev));
	ts->input_dev_key->name = MAX1187X_KEY;
	ts->input_dev_key->phys = ts->phys_key;
	ts->input_dev_key->id.bustype = BUS_I2C;
	__set_bit(EV_KEY, ts->input_dev_key->evbit);
	set_bit(KEY_POWER, ts->input_dev_key->keybit);
	ret = input_register_device(ts->input_dev_key);
	if (ret) {
		TPD_DMESG("Failed to register touch input key device\n");
		ret = -EPERM;
		goto err_device_init_register_inputdevkey;
	}
	TPD_DMESG("(INIT): Input key device OK\n");
#endif

	/* Setup IRQ and handler */
#ifdef CONFIG_OF_TOUCH
	ret = tpd_irq_registration(ts->client->irq, irq_handler,
			IRQF_TRIGGER_FALLING, client->name, ts);
	if (ret != 0) {
			TPD_DMESG("Failed to setup IRQ handler\n");
			ret = -EIO;
			goto err_device_init_irq;
	}
	TPD_DMESG("(INIT): IRQ handler OK\n");
#else
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, IRQF_TRIGGER_FALLING, irq_handler, 1); 
    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif
	/* collect controller ID and configuration ID data from firmware   */
	/* and perform firmware comparison/download if we have valid image */
	validate_fw(ts);

	/* configure suspend/resume */
#ifdef CONFIG_USE_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
	ts->early_suspend.suspend = early_suspend;
	ts->early_suspend.resume = late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	ts->is_suspended = 0;
	TPD_DMESG("(INIT): suspend/resume registration OK\n");

	/* set up debug interface */
	while (*dev_attr) {
		if (device_create_file(&client->dev, *dev_attr) < 0) {
			TPD_DMESG("failed to create sysfs file\n");
			return 0;
		}
		ts->sysfs_created++;
		dev_attr++;
	}

	if (device_create_bin_file(&client->dev, &dev_attr_report) < 0) {
		TPD_DMESG("failed to create sysfs file [report]\n");
		return 0;
	}
	ts->sysfs_created++;

#ifdef TOUCH_WAKEUP_FEATURE
	pr_info("Touch Wakeup Feature enabled\n");
	device_init_wakeup(&client->dev, 1);
#endif

	pr_info("(INIT): Done\n");
	return 0;

err_device_init_irq:
#ifdef TOUCH_WAKEUP_FEATURE
err_device_init_register_inputdevkey:
	input_free_device(ts->input_dev_key);
	ts->input_dev_key = NULL;
err_device_init_alloc_inputdevkey:
#endif
err_device_init_register_inputdev:
	//input_free_device(ts->input_dev);
	//ts->input_dev = NULL;
err_device_init_alloc_inputdev:
err_device_init_gpio:
err_device_init_memalloc:
err_device_init_pdata:
	kfree(ts);
err_device_init:
	return ret;
}

static int device_deinit(struct i2c_client *client)
{
	struct max1187x_pdata *pdata = ts->pdata;
	struct device_attribute **dev_attr = dev_attrs;

	if (ts == NULL)
		return 0;

	propagate_report(ts, -1, NULL);

	init_state = 0;

#ifdef TOUCH_WAKEUP_FEATURE
	device_init_wakeup(&client->dev, 0);
#endif

	while (*dev_attr) {
		if (ts->sysfs_created && ts->sysfs_created--)
			device_remove_file(&client->dev, *dev_attr);
		dev_attr++;
	}
	if (ts->sysfs_created && ts->sysfs_created--)
		device_remove_bin_file(&client->dev, &dev_attr_report);

#ifdef CONFIG_USE_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

	if (client->irq)
			free_irq(client->irq, ts);

#ifdef TOUCH_WAKEUP_FEATURE
	input_unregister_device(ts->input_dev_key);
#endif

	//input_unregister_device(ts->input_dev);

	flush_workqueue(ts->wq);
	destroy_workqueue(ts->wq);
	//kfree(ts);
	ts = NULL;
	pr_info("(INIT): Deinitialized\n");
	return 0;
}

static int max_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int count = 0;
	
	if (device_create_file(&client->dev, &dev_attr_init) < 0) {
		TPD_DMESG("failed to create sysfs file [init]\n");
		return 0;
	}

	if (IS_ERR(kthread_run(device_init_thread, (void *) client,
			"max1187x_probe"))) {
		TPD_DMESG("failed to start kernel thread\n");
		return -EAGAIN;
	}

	do{
		msleep(10);
		count++;
		if(check_flag)
			break;
	}while(count < 50);
	printk("max_i2c_probe done.count = %d, flag = %d",count,check_flag);

	return 0;
}

static int max_i2c_remove(struct i2c_client *client)
{
	int ret = device_deinit(client);

	device_remove_file(&client->dev, &dev_attr_init);
	return ret;
}

/*
 Commands
 */

static void process_rbcmd(struct data *ts)
{
	if (ts->rbcmd_waiting == 0)
		return;
	if (ts->rbcmd_report_id != ts->rx_report[1])
		return;

	ts->rbcmd_received = 1;
	memcpy(ts->rbcmd_rx_report, ts->rx_report, (ts->rx_report_len + 1)<<1);
	ts->rbcmd_rx_report_len = ts->rx_report_len;
	wake_up_interruptible(&ts->waitqueue_rbcmd);
}

static int combine_multipacketreport(struct data *ts, u16 *report)
{
	u16 packet_header = report[0];
	u8 packet_seq_num = BYTEH(packet_header);
	u8 packet_size = BYTEL(packet_header);
	u16 total_packets, this_packet_num, offset;
	static u16 packet_seq_combined;

	if (packet_seq_num == 0x11) {
		memcpy(ts->rx_report, report, (packet_size + 1) << 1);
		ts->rx_report_len = packet_size;
		packet_seq_combined = 1;
		return 0;
	}

	total_packets = (packet_seq_num & 0xF0) >> 4;
	this_packet_num = packet_seq_num & 0x0F;

	if (this_packet_num == 1) {
		if (report[1] == 0x0800) {
			ts->rx_report_len = report[2] + 2;
			packet_seq_combined = 1;
			memcpy(ts->rx_report, report, (packet_size + 1) << 1);
			return -EAGAIN;
		} else {
			return -EIO;
		}
	} else if (this_packet_num == packet_seq_combined + 1) {
		packet_seq_combined++;
		offset = (this_packet_num - 1) * 0xF4 + 1;
		memcpy(ts->rx_report + offset, report + 1, packet_size << 1);
		if (total_packets == this_packet_num)
			return 0;
		else
			return -EIO;
	}
	return -EIO;
}

static void propagate_report(struct data *ts, int status, u16 *report)
{
	int i, ret;

	down(&ts->report_sem);
	mutex_lock(&ts->report_mutex);

	if (report) {
		ret = combine_multipacketreport(ts, report);
		if (ret) {
			up(&ts->report_sem);
			mutex_unlock(&ts->report_mutex);
			return;
		}
	}
	process_rbcmd(ts);

	for (i = 0; i < MAX_REPORT_READERS; i++) {
		if (status == 0) {
			if (ts->report_readers[i].report_id == 0xFFFF
				|| (ts->rx_report[1] != 0
				&& ts->report_readers[i].report_id
				== ts->rx_report[1])) {
				up(&ts->report_readers[i].sem);
				ts->report_readers[i].reports_passed++;
				ts->report_readers_outstanding++;
			}
		} else {
			if (ts->report_readers[i].report_id != 0) {
				ts->report_readers[i].status = status;
				up(&ts->report_readers[i].sem);
			}
		}
	}
	if (ts->report_readers_outstanding == 0)
		up(&ts->report_sem);
	mutex_unlock(&ts->report_mutex);
}

static int get_report(struct data *ts, u16 report_id, ulong timeout)
{
	int i, ret, status;

	mutex_lock(&ts->report_mutex);
	for (i = 0; i < MAX_REPORT_READERS; i++)
		if (ts->report_readers[i].report_id == 0)
			break;
	if (i == MAX_REPORT_READERS) {
		mutex_unlock(&ts->report_mutex);
		TPD_DMESG("maximum readers reached\n");
		return -EBUSY;
	}
	ts->report_readers[i].report_id = report_id;
	sema_init(&ts->report_readers[i].sem, 1);
	down(&ts->report_readers[i].sem);
	ts->report_readers[i].status = 0;
	ts->report_readers[i].reports_passed = 0;
	mutex_unlock(&ts->report_mutex);

	if (timeout == 0xFFFFFFFF)
		ret = down_interruptible(&ts->report_readers[i].sem);
	else
		ret = down_timeout(&ts->report_readers[i].sem,
			(timeout * HZ) / 1000);

	mutex_lock(&ts->report_mutex);
	if (ret && ts->report_readers[i].reports_passed > 0)
		if (--ts->report_readers_outstanding == 0)
			up(&ts->report_sem);
	status = ts->report_readers[i].status;
	ts->report_readers[i].report_id = 0;
	mutex_unlock(&ts->report_mutex);

	return (status == 0) ? ret : status;
}

static void release_report(struct data *ts)
{
	mutex_lock(&ts->report_mutex);
	if (--ts->report_readers_outstanding == 0)
		up(&ts->report_sem);
	mutex_unlock(&ts->report_mutex);
}

/* debug_mask |= 0x10 for pm functions */

static void set_suspend_mode(struct data *ts)
{
	u16 cmd_buf[] = {0x0020, 0x0001, 0x0000};
	int ret;

	TPD_DMESG("Enter\n");

#ifdef CONFIG_OF_TOUCH
	disable_irq(ts->client->irq);
#else
	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

	ts->is_suspended = 1;

	flush_workqueue(ts->wq);

#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&ts->client->dev))
		cmd_buf[2] = 0x6;
#endif
	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		TPD_DMESG("Failed to set sleep mode\n");

	flush_workqueue(ts->wq);

#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&ts->client->dev))
#ifdef CONFIG_OF_TOUCH
		enable_irq(ts->client->irq);
#else
		mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

#endif

	TPD_DMESG("Exit\n");
	return;
}

static void set_resume_mode(struct data *ts)
{
	u16 cmd_buf[] = {0x0020, 0x0001, 0x0002};
	int ret;

	TPD_DMESG("Enter\n");

#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&ts->client->dev))
#ifdef CONFIG_OF_TOUCH
		disable_irq(ts->client->irq);
#else
		mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

#endif

	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		TPD_DMESG("Failed to set active mode\n");

	cmd_buf[0] = 0x0018;
	cmd_buf[1] = 0x0001;
	cmd_buf[2] = MAX1187X_TOUCH_REPORT_MODE;
	ret = cmd_send(ts, cmd_buf, 3);
	if (ret)
		TPD_DMESG("Failed to set up touch report mode\n");

	flush_workqueue(ts->wq);

	ts->is_suspended = 0;

#ifdef CONFIG_OF_TOUCH
	enable_irq(ts->client->irq);
#else
	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif

	TPD_DMESG("Exit\n");

	return;
}

#ifdef CONFIG_USE_EARLYSUSPEND
static void early_suspend(struct early_suspend *h)
{
	struct data *ts = container_of(h, struct data, early_suspend);

	TPD_DMESG("Enter\n");

	set_suspend_mode(ts);

	TPD_DMESG("Exit\n");
	return;
}

static void late_resume(struct early_suspend *h)
{
	struct data *ts = container_of(h, struct data, early_suspend);

	TPD_DMESG("Enter\n");

	set_resume_mode(ts);

	TPD_DMESG("Exit\n");
}
#endif

static int suspend(struct device *dev)
{
#ifdef TOUCH_WAKEUP_FEATURE
	struct i2c_client *client = to_i2c_client(dev);
#endif

	TPD_DMESG("Enter\n");

#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&client->dev))
		enable_irq_wake(client->irq);
#endif

	TPD_DMESG("Exit\n");

	return 0;
}

static int resume(struct device *dev)
{
#ifdef TOUCH_WAKEUP_FEATURE
	struct i2c_client *client = to_i2c_client(dev);
#endif

	TPD_DMESG("Enter\n");

#ifdef TOUCH_WAKEUP_FEATURE
	if (device_may_wakeup(&client->dev))
		disable_irq_wake(client->irq);
#endif

	TPD_DMESG("Exit\n");

	return 0;
}


static int bootloader_read_status_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[] = { STATUS_ADDR_L, STATUS_ADDR_H }, i;

	for (i = 0; i < 3; i++) {
		mutex_lock(&ts->i2c_mutex);
		if (i2c_tx_b(ts, buffer, 2) < 0) {
			pr_err("TX fail");
			mutex_unlock(&ts->i2c_mutex);
			return -EIO;
		}
		if (i2c_rx_b(ts, buffer, 2) < 0) {
			mutex_unlock(&ts->i2c_mutex);
			TPD_DMESG("RX fail\n");
			return -EIO;
		}
		mutex_unlock(&ts->i2c_mutex);
		if (buffer[0] == byteL && buffer[1] == byteH)
			break;
	}
	if (i == 3) {
		TPD_DMESG("Unexpected status => %02X%02X vs %02X%02X\n",
				buffer[0], buffer[1], byteL, byteH);
		return -EIO;
	}

	return 0;
}

static int bootloader_write_status_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[] = { STATUS_ADDR_L, STATUS_ADDR_H, byteL, byteH };
	mutex_lock(&ts->i2c_mutex);
	if (i2c_tx_b(ts, buffer, 4) < 0) {
		mutex_unlock(&ts->i2c_mutex);
		TPD_DMESG("TX fail\n");
		return -EIO;
	}
	mutex_unlock(&ts->i2c_mutex);
	return 0;
}

static int bootloader_rxtx_complete(struct data *ts)
{
	return bootloader_write_status_reg(ts, RXTX_COMPLETE_L,
				RXTX_COMPLETE_H);
}

static int bootloader_read_data_reg(struct data *ts, u8 *byteL, u8 *byteH)
{
	u8 buffer[] = { DATA_ADDR_L, DATA_ADDR_H, 0x00, 0x00 };
	mutex_lock(&ts->i2c_mutex);
	if (i2c_tx_b(ts, buffer, 2) < 2) {
		mutex_unlock(&ts->i2c_mutex);
		pr_err("TX fail");
		return -EIO;
	}
	if (i2c_rx_b(ts, buffer, 4) < 0) {
		mutex_unlock(&ts->i2c_mutex);
		TPD_DMESG("RX fail\n");
		return -EIO;
	}
	mutex_unlock(&ts->i2c_mutex);
	if (buffer[2] != 0xCC && buffer[3] != 0xAB) {
		TPD_DMESG("Status is not ready\n");
		return -EIO;
	}

	*byteL = buffer[0];
	*byteH = buffer[1];
	return bootloader_rxtx_complete(ts);
}

static int bootloader_write_data_reg(struct data *ts, const u8 byteL,
	const u8 byteH)
{
	u8 buffer[6] = { DATA_ADDR_L, DATA_ADDR_H, byteL, byteH,
			RXTX_COMPLETE_L, RXTX_COMPLETE_H };

	if (bootloader_read_status_reg(ts, STATUS_READY_L,
		STATUS_READY_H) < 0) {
		TPD_DMESG("read status register fail\n");
		return -EIO;
	}
	mutex_lock(&ts->i2c_mutex);
	if (i2c_tx_b(ts, buffer, 6) < 0) {
		mutex_unlock(&ts->i2c_mutex);
		TPD_DMESG("TX fail\n");
		return -EIO;
	}
	mutex_unlock(&ts->i2c_mutex);
	return 0;
}

static int bootloader_rxtx(struct data *ts, u8 *byteL, u8 *byteH,
	const int tx)
{
	if (tx > 0) {
		if (bootloader_write_data_reg(ts, *byteL, *byteH) < 0) {
			TPD_DMESG("write data register fail\n");
			return -EIO;
		}
		return 0;
	}

	if (bootloader_read_data_reg(ts, byteL, byteH) < 0) {
		TPD_DMESG("read data register fail\n");
		return -EIO;
	}
	return 0;
}

static int bootloader_get_cmd_conf(struct data *ts, int retries)
{
	u8 byteL, byteH;

	do {
		if (bootloader_read_data_reg(ts, &byteL, &byteH) >= 0) {
			if (byteH == 0x00 && byteL == 0x3E)
				return 0;
		}
		retries--;
	} while (retries > 0);

	return -EIO;
}

static int bootloader_write_buffer(struct data *ts, u8 *buffer, int size)
{
	u8 byteH = 0x00;
	int k;

	for (k = 0; k < size; k++) {
		if (bootloader_rxtx(ts, &buffer[k], &byteH, 1) < 0) {
			TPD_DMESG("bootloader RX-TX fail\n");
			return -EIO;
		}
	}
	return 0;
}

static int bootloader_enter(struct data *ts)
{
	int i;
	u16 enter[3][2] = { { 0x7F00, 0x0047 }, { 0x7F00, 0x00C7 }, { 0x7F00,
			0x0007 } };
	mutex_lock(&ts->i2c_mutex);
	for (i = 0; i < 3; i++) {
		if (i2c_tx_w(ts, enter[i], 2) < 0) {
			mutex_unlock(&ts->i2c_mutex);
			TPD_DMESG("Failed to enter bootloader\n");
			return -EIO;
		}
	}
	mutex_unlock(&ts->i2c_mutex);

	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		TPD_DMESG("Failed to enter bootloader mode\n");
		return -EIO;
	}
	return 0;
}

static int bootloader_exit(struct data *ts)
{
	int i;
	u16 exit[3][2] = { { 0x7F00, 0x0040 }, { 0x7F00, 0x00C0 }, { 0x7F00,
			0x0000 } };
	mutex_lock(&ts->i2c_mutex);
	for (i = 0; i < 3; i++) {
		if (i2c_tx_w(ts, exit[i], 2) < 0) {
			TPD_DMESG("Failed to exit bootloader\n");
			mutex_unlock(&ts->i2c_mutex);
			return -EIO;
		}
	}
	mutex_unlock(&ts->i2c_mutex);
	return 0;
}

static int bootloader_get_crc(struct data *ts, u16 *crc16,
		u16 addr, u16 len, u16 delay)
{
	u8 crc_command[] = {0x30, 0x02, BYTEL(addr),
			BYTEH(addr), BYTEL(len), BYTEH(len)};
	u8 byteL = 0, byteH = 0;
	u16 rx_crc16 = 0;

	if (bootloader_write_buffer(ts, crc_command, 6) < 0) {
		TPD_DMESG("write buffer fail\n");
		return -EIO;
	}
	msleep(delay);

	/* reads low 8bits (crcL) */
	if (bootloader_rxtx(ts, &byteL, &byteH, 0) < 0) {
		TPD_DMESG("Failed to read low byte of crc response!\n");
		return -EIO;
	}
	rx_crc16 = (u16) byteL;

	/* reads high 8bits (crcH) */
	if (bootloader_rxtx(ts, &byteL, &byteH, 0) < 0) {
		TPD_DMESG("Failed to read high byte of crc response!\n");
		return -EIO;
	}
	rx_crc16 = (u16)(byteL << 8) | rx_crc16;

	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		TPD_DMESG("CRC get failed!\n");
		return -EIO;
	}
	*crc16 = rx_crc16;

	return 0;
}

static int bootloader_set_byte_mode(struct data *ts)
{
	u8 buffer[2] = { 0x0A, 0x00 };

	if (bootloader_write_buffer(ts, buffer, 2) < 0) {
		TPD_DMESG("write buffer fail\n");
		return -EIO;
	}
	if (bootloader_get_cmd_conf(ts, 10) < 0) {
		TPD_DMESG("command confirm fail\n");
		return -EIO;
	}
	return 0;
}

static int bootloader_erase_flash(struct data *ts)
{
	u8 byteL = 0x02, byteH = 0x00;
	int i, verify = 0;

	if (bootloader_rxtx(ts, &byteL, &byteH, 1) < 0) {
		TPD_DMESG("bootloader RX-TX fail\n");
		return -EIO;
	}

	for (i = 0; i < 10; i++) {
		msleep(60); /* wait 60ms */

		if (bootloader_get_cmd_conf(ts, 0) < 0)
			continue;

		verify = 1;
		break;
	}

	if (verify != 1) {
		TPD_DMESG("Flash Erase failed\n");
		return -EIO;
	}

	return 0;
}

static int bootloader_write_flash(struct data *ts, const u8 *image, u16 length)
{
	u8 buffer[130];
	u8 length_L = length & 0xFF;
	u8 length_H = (length >> 8) & 0xFF;
	u8 command[] = { 0xF0, 0x00, length_H, length_L, 0x00 };
	u16 blocks_of_128bytes;
	int i, j;

	if (bootloader_write_buffer(ts, command, 5) < 0) {
		TPD_DMESG("write buffer fail\n");
		return -EIO;
	}

	blocks_of_128bytes = length >> 7;

	for (i = 0; i < blocks_of_128bytes; i++) {
		for (j = 0; j < 100; j++) {
			usleep_range(1500, 2000);
			if (bootloader_read_status_reg(ts, STATUS_READY_L,
			STATUS_READY_H)	== 0)
				break;
		}
		if (j == 100) {
			TPD_DMESG("Failed to read Status register!\n");
			return -EIO;
		}

		buffer[0] = ((i % 2) == 0) ? 0x00 : 0x40;
		buffer[1] = 0x00;
		memcpy(buffer + 2, image + i * 128, 128);
		mutex_lock(&ts->i2c_mutex);
		if (i2c_tx_b(ts, buffer, 130) < 0) {
			mutex_unlock(&ts->i2c_mutex);
			TPD_DMESG("Failed to write data (%d)\n", i);
			return -EIO;
		}
		mutex_unlock(&ts->i2c_mutex);
		
		if (bootloader_rxtx_complete(ts) < 0) {
			TPD_DMESG("Transfer failure (%d)\n", i);
			return -EIO;
		}
	}

	usleep_range(10000, 11000);
	if (bootloader_get_cmd_conf(ts, 5) < 0) {
		TPD_DMESG("Flash programming failed\n");
		return -EIO;
	}
	return 0;
}

/****************************************
 *
 * Standard Driver Structures/Functions
 *
 ****************************************/

MODULE_DEVICE_TABLE(i2c, id);//what is it used for?
/**********************************MTK Add****************************/
static struct i2c_driver max_i2c_driver = {
		.probe = max_i2c_probe,
		.remove = max_i2c_remove,
		.id_table = tpd_i2c_id,
		.driver = {
			.name = MAX1187X_NAME,
			.owner	= THIS_MODULE,
		},
};

/* Function to manage power-off suspend */
static void tpd_suspend(struct early_suspend *h)
{

}

/* Function to manage power-on resume */
static void tpd_resume(struct early_suspend *h)
{

}

static int tpd_local_init(void)
{
    if (i2c_add_driver(&max_i2c_driver) != 0)
    {
        TPD_DMESG("unable to add i2c driver.\n");
        return -1;
    }

    if (tpd_load_status == 0) //if(tpd_load_status == 0) // disable auto load touch driver for linux3.0 porting
    {
        TPD_DMESG("add error touch panel driver.\n");
        i2c_del_driver(&max_i2c_driver);
        return -1;
    }

#ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

    // set vendor string
    tpd->dev->id.vendor = 0x00;
    tpd->dev->id.product = tpd_info.pid;
    tpd->dev->id.version = tpd_info.vid;

    TPD_DMESG("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;
#if GTP_SUPPORT_I2C_DMA
		tpd->dev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		gpDMABuf_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, GTP_DMA_MAX_TRANSACTION_LENGTH, &gpDMABuf_pa, GFP_KERNEL);
		if(!gpDMABuf_va){
			TPD_DMESG("[Error] Allocate DMA I2C Buffer failed!\n");
		}
		memset(gpDMABuf_va, 0, GTP_DMA_MAX_TRANSACTION_LENGTH);
#endif

    return 0;
}

static struct tpd_driver_t tpd_device_driver =
{
    .tpd_device_name = MAX1187X_NAME,
    .tpd_local_init = tpd_local_init,
    .suspend = tpd_suspend,
    .resume = tpd_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};



static int __init max1187x_init(void)
{
	TPD_DMESG("MediaTek max1187x touch panel driver init\n");

	i2c_register_board_info(2, &i2c_tpd, 1);

	if (tpd_driver_add(&tpd_device_driver) < 0)
		TPD_DMESG("add generic driver failed\n");
	
	return 0;

}

static void __exit max1187x_exit(void)
{
    TPD_DMESG("MediaTek max1187x touch panel driver exit\n");
    tpd_driver_remove(&tpd_device_driver);

}

module_init(max1187x_init);
module_exit(max1187x_exit);

MODULE_AUTHOR("Maxim Integrated Products, Inc.");
MODULE_DESCRIPTION("MAX1187X Touchscreen Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("3.1.7");
