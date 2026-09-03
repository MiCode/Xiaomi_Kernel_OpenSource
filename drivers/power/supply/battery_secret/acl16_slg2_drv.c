/*
 *  acl16_slg2_drv.c - Aisinochip SLG2 driver
 *
 * Copyright (c) Shanghai Aisinochip Electronics Techology Co., Ltd.
 *
 */
#define pr_fmt(fmt) "[slg2 %s,%d] " fmt "\n", __func__, __LINE__

#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/crc16.h>
#include <linux/platform_device.h>
#include <linux/device/driver.h>
#include "battery_secret_class.h"
#include "secret_common.h"

#define MODULE_TAG "slg2_drv"
//#include "lc_logfs_class.h"

#define CRC16_INIT                  0
#define READ_PAGE_MAX               2
#define WRITE_PAGE_MAX              10
#define GENERAL_WAIT_MAX            10

#define SLG2_MEMORY_WRITE           0xDA
#define SLG2_MEMORY_READ            0xCA
#define SLG2_DC_DECREASE            0x24
#define SLG2_DC_GET                 0xC0
#define SLG2_CHIP_NAME             "ACL16_SLG2"

static struct w1_data *gSlg2_info;
static u32 slg2_timings_cfg[10] = { 500, 16, 8, 8, 500, 200, 16, 64, 25 };

static int slg2_bus_send_recv(
	u8 *input, int input_len,
	int delay_ms, u8 *output, int output_len)
{
	int index;
	unsigned short calc_crc;
	unsigned short recv_crc;
	u8 input_buf[64];
	u8 recv_buf[64];

	memcpy(input_buf, input, input_len);
	index = input_len;
	calc_crc = crc16(CRC16_INIT, input_buf, index);
	calc_crc ^= 0xFFFF;
	input_buf[index++] = calc_crc >> 8;
	input_buf[index++] = calc_crc & 0xFF;
	if (w1_reset_bus()) {
		pr_err("reset bus failed");
		return W1_BUS_ERROR;
	}
	// skip ROM
	w1_write_byte(W1_SKIP_ROM);
	w1_hex_dump("slg2 <- CC", NULL, 0);
	w1_mdelay(1);
	w1_write_block(input_buf, index);
	w1_hex_dump("slg2 <- ", input_buf, index);
	w1_mdelay(delay_ms);
	w1_read_block(recv_buf, output_len + 4);
	w1_hex_dump("slg2 -> ", recv_buf, output_len + 4);
	if ((output_len + 1) != recv_buf[0])
		pr_err("invalid length:%u", recv_buf[0]);
	calc_crc = crc16(CRC16_INIT, recv_buf, output_len + 2);
	calc_crc ^= 0xFFFF;
	recv_crc = (recv_buf[output_len + 2] << 8) + recv_buf[output_len + 3];
	if (recv_crc == calc_crc) {
		if (recv_buf[1] != W1_RESPONSE_SUCCESS) {
			pr_err("invalid status:%02X", recv_buf[1]);
			w1_bus_recovery();
			return W1_CMD_ERROR;
		} else {
			memcpy(output, &recv_buf[2], output_len);
			memcpy(gSlg2_info->cmd_buffer, &recv_buf[1], output_len + 1);
			w1_mdelay(10);
			return W1_CMD_OK;
		}
	} else {
		pr_err("crc error");
		w1_bus_recovery();
		return W1_RECV_CRC_ERROR;
	}
}

// slg2 <- CCCA010051DE
// slg2 -> 21AA534C424E355334323138303032333730574D443141303030303030303030303088CE
static int slg2_memory_read(int index, u8 *read_buf)
{
	u8 send[8];

	send[0] = SLG2_MEMORY_READ;
	send[1] = 1;
	send[2] = index;
	if (!slg2_bus_send_recv(send, 3, READ_PAGE_MAX, read_buf, 32)) {
		return 0;
	} else {
		pr_err("failed, page index:%d", index);
		return -EINVAL;
	}
}

// slg2 <- CCDA2102FFEEDDCCBBAA99887766554433221100FFEEDDCCBBAA99887766554433221100DB78
// slg2 -> 01AA107E
static int slg2_memory_write(int index, const u8 *write_data)
{
	u8 send[64], recv[8];

	send[0] = SLG2_MEMORY_WRITE;
	send[1] = 0x21;
	send[2] = index;
	memcpy(&send[3], write_data, 32);
	if (!slg2_bus_send_recv(send, 35, WRITE_PAGE_MAX, recv, 0)) {
		return 0;
	} else {
		pr_err("failed, page index:%d", index);
		return -EINVAL;
	}

}

// slg2 <- CCC000FFAF
// slg2 -> 05AA0001F907E0B5
static int slg2_dc_get(u32 *dc_cnt)
{
	u8 send[8], recv[8];

	send[0] = SLG2_DC_GET;
	send[1] = 0x00;
	if (!slg2_bus_send_recv(send, 2, GENERAL_WAIT_MAX, recv, 4)) {
		*dc_cnt = (recv[0] << 24) + (recv[1] << 16) + (recv[2] << 8) + (recv[3]);
		return 0;
	} else {
		pr_err("read dc failed");
		return -EINVAL;
	}
}

// slg2 <- CC2400FFE4
// slg2 -> 01AA107E
static int slg2_dc_decrease(void)
{
	u8 send[8], recv[8];

	send[0] = SLG2_DC_DECREASE;
	send[1] = 0x00;
	memset(recv, 0xFF, sizeof(recv));
	if (!slg2_bus_send_recv(send, 2, GENERAL_WAIT_MAX, recv, 0)) {
		return 0;
	} else {
		pr_err("dc decrease failed");
		return -EINVAL;
	}
}

static int slg2_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct w1_data *info;

	pr_err("enter");
	info = devm_kzalloc(&pdev->dev, sizeof(struct w1_data), GFP_KERNEL);
	if (!info) {
		pr_err("alloc mem fail\n");
		return -ENOMEM;
	}
	gSlg2_info = info;
	info->dev = &pdev->dev;
	info->w1_timings_cfg = slg2_timings_cfg;
	info->page_read = slg2_memory_read;
	info->page_write = slg2_memory_write;
	info->dc_get = slg2_dc_get;
	info->dc_decrease = slg2_dc_decrease;
	info->w1_bus_send_recv = slg2_bus_send_recv;
	ret = w1_sec_dev_init(info, SLG2_CHIP_NAME);
	if (ret) {
		pr_err("w1 secret device init fail\n");
		return -EINVAL;
	}
	pr_info("drv probe succeeded\n");

	return 0;
}

static int slg2_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id slg2_of_ids[] = {
	{ .compatible = "acl,slg2" },
	{ },
};

static struct platform_driver slg2_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "acl,slg2",
		.of_match_table = slg2_of_ids,
	},
	.probe = slg2_probe,
	.remove = slg2_remove,
};

static int __init slg2_init(void)
{
	pr_err("enter");
	return platform_driver_register(&slg2_driver);
}

static void __exit slg2_exit(void)
{
	pr_err("enter");
	platform_driver_unregister(&slg2_driver);
}

module_init(slg2_init);
module_exit(slg2_exit);
MODULE_LICENSE("GPL");
