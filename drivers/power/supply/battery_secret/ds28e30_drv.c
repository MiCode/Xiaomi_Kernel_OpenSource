/*
 *  ds28e30_drv.c - maxium ds28e30 driver
 */
#define pr_fmt(fmt) "[ds28e30 %s,%d] " fmt "\n", __func__, __LINE__

#include <linux/of.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/crc16.h>
#include <linux/platform_device.h>
#include "battery_secret_class.h"
#include "secret_common.h"

#define MODULE_TAG "ds28e30_drv"
//#include "lc_logfs_class.h"

#define DS28E30_CMD_START           0x66
#define DS28E30_CMD_SKIP_ROM        0xCC
#define DS28E30_CMD_READ_MEM        0x44
#define DS28E30_CMD_WRITE_MEM       0x96
#define DS28E30_CMD_DC_DECREASE     0xC9
#define DS28E30_DC_PAGE             106
#define DS28E30_CHIP_NAME           "DS28E30"

#define DELAY_EE_READ_TRM           50
#define DELAY_EE_WRITE_TWM          100
#define DELAY_DC_WRITE_TWM          150

static u16 CRC16 = 0x00;
static struct w1_data *gDs_info;
static u32 ds28e30_timings_cfg[10] = { 0, 16, 8, 8, 0, 0, 16, 64, 8 };
static const short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

static u16 docrc16(u16 data)
{
    data = (data ^ (CRC16 & 0xff)) & 0xff;
    CRC16 >>= 8;
    if (oddparity[data & 0xf] ^ oddparity[data >> 4])
        CRC16 ^= 0xc001;
    data <<= 6;
    CRC16 ^= data;
    data <<= 1;
    CRC16 ^= data;

    return CRC16;
}

static int ds28e30_standard_cmd_flow(
    u8 *write_buf, int write_len,
    int delay_ms, u8 *read_buf, int read_len)
{
    u8  pkt[256] = { 0 };
    int i, pkt_len = 0, get_len;

    if (w1_reset_bus()) {
        pr_err("reset bus failed\n");
        return W1_BUS_ERROR;
    }
    w1_write_byte(DS28E30_CMD_SKIP_ROM);
    w1_hex_dump("ds28e30 <- CC", NULL, 0);
    pkt[pkt_len++] = DS28E30_CMD_START;
    pkt[pkt_len++] = write_len;
    memcpy(&pkt[pkt_len], write_buf, write_len);
    pkt_len += write_len;
    //send packet to DS28E30
    w1_write_block(pkt, pkt_len);
    w1_hex_dump("ds28e30 <- ", pkt, pkt_len);
    // read two CRC bytes
    pkt[pkt_len++] = w1_read_byte();
    pkt[pkt_len++] = w1_read_byte();
    w1_hex_dump("ds28e30 -> ", &pkt[pkt_len - 2], 2);
    // check CRC16
    CRC16 = 0;
    for (i = 0; i < pkt_len; i++)
        docrc16(pkt[i]);
    // skip crc error just in dc decrease cmd
    if (CRC16 != 0xB001) {
        pr_err("write data crc error\n");
        w1_bus_recovery();
        return W1_CMD_ERROR;
    }
    if (delay_ms > 0) {
        w1_write_byte(0xAA);
        w1_hex_dump("ds28e30 <- AA", NULL, 0);
        msleep(delay_ms);
    }
    // read FF and the length byte
    pkt[0] = w1_read_byte();
    pkt[1] = w1_read_byte();
    get_len = pkt[1];
    w1_hex_dump("ds28e30 -> ", &pkt[0], 2);
    w1_read_block(pkt, read_len + 3);
    w1_hex_dump("ds28e30 -> ", pkt, read_len + 3);
    CRC16 = 0;
    docrc16(read_len + 1);
    for (i = 0; i < (read_len + 3); i++)
        docrc16(pkt[i]);
    if (read_len != get_len - 1)
        pr_err("read length error:%02x\n", get_len);
    if (CRC16 == 0xB001) {
        if (pkt[0] == W1_RESPONSE_SUCCESS) {
            if (read_len)
                memcpy(read_buf, &pkt[1], read_len);
            memcpy(gDs_info->cmd_buffer, &pkt[0], read_len + 1);
            msleep(10);
            return W1_CMD_OK;
        } else {
            pr_err("receive status invalid:%02X\n", pkt[1]);
            w1_bus_recovery();
            return W1_CMD_ERROR;
        }
    } else {
        pr_err("read data crc error\n");
        w1_bus_recovery();
        return W1_RECV_CRC_ERROR;
    }   
}

// ds28e30 <- CC66024400
// ds28e30 -> 73B7
// ds28e30 <- AA
// ds28e30 -> FF21
// ds28e30 -> AA4E56424E355334313038303030393034414D44314130303030303030303030305D47
static int ds28e30_memory_read(int index, u8 *read_buf)
{
    int write_len = 0;
    u8  write_buf[10];

	write_buf[write_len++] = DS28E30_CMD_READ_MEM;
    write_buf[write_len++] = index;
	if (!ds28e30_standard_cmd_flow(write_buf, write_len, DELAY_EE_READ_TRM, read_buf, 32)) {
		return 0;
	} else {
		pr_err("failed, page index:%d\n", index);
    	return -EINVAL;
	}
}

// ds28e30 <- CC6622960200112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF
// ds28e30 -> 1353
// ds28e30 <- AA
// ds28e30 -> FF01
// ds28e30 -> AA7E10
static int ds28e30_memory_write(int index, const u8 *write_data)
{
    int write_len;
    u8 write_buf[64];

	write_len = 0;
    write_buf[write_len++] = DS28E30_CMD_WRITE_MEM;
    write_buf[write_len++] = index;
    memcpy(&write_buf[write_len], write_data, 32);
    write_len += 32;
    if (!ds28e30_standard_cmd_flow(write_buf, write_len, DELAY_EE_WRITE_TWM, NULL, 0)) {
		return 0;
	} else {
		pr_err("failed, page index:%d\n", index);
    	return -EINVAL;
	}

}

// ds28e30 <- CC6602446A
// ds28e30 -> F398
// ds28e30 <- AA
// ds28e30 -> FF21
// ds28e30 -> AAFEFF010000000000000000000000000000000000000000000000000000000000CA22
static int ds28e30_dc_get(u32 *dc_cnt)
{
	u8 page_buf[32];

	if (!ds28e30_memory_read(DS28E30_DC_PAGE, page_buf)) {
		*dc_cnt = (page_buf[2] << 16) | (page_buf[1] << 8) | page_buf[0];
		return 0;
	} else {
		pr_err("read dc failed\n");
    	return -EINVAL;
	}
}

// ds28e30 <- CC6601C9
// ds28e30 -> DE26
// ds28e30 <- AA
// ds28e30 -> FF01
// ds28e30 -> AA7E10
static int ds28e30_dc_decrease(void)
{
    u8 write_cmd = DS28E30_CMD_DC_DECREASE;

	if (!ds28e30_standard_cmd_flow(&write_cmd, 1, DELAY_DC_WRITE_TWM, NULL, 0)) {
		return 0;
	} else {
		pr_err("failed\n");
    	return -EINVAL;
	}
}

static int ds28e30_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct w1_data *info;

	pr_err("enter\n");
	info = devm_kzalloc(&pdev->dev, sizeof(struct w1_data), GFP_KERNEL);
	if (!info) {
		pr_err("alloc mem fail\n");
		return -ENOMEM;
	}
	gDs_info = info;
	info->dev = &pdev->dev;
	info->w1_timings_cfg = ds28e30_timings_cfg;
	info->page_read = ds28e30_memory_read;
	info->page_write = ds28e30_memory_write;
	info->dc_get = ds28e30_dc_get;
	info->dc_decrease = ds28e30_dc_decrease;
	info->w1_bus_send_recv = ds28e30_standard_cmd_flow;
	ret = w1_sec_dev_init(info, DS28E30_CHIP_NAME);
	if (ret) {
		pr_err("w1 secret device init fail\n");
		return -EINVAL;
	}
	pr_info("drv probe succeeded\n");

	return 0;
}

static int ds28e30_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id ds28e30_of_ids[] = {
	{ .compatible = "maxim,ds28e30" },
	{ },
};

static struct platform_driver ds28e30_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "maxim,ds28e30",
		.of_match_table = ds28e30_of_ids,
	},
	.probe = ds28e30_probe,
	.remove = ds28e30_remove,
};

static int __init ds28e30_init(void)
{
	pr_err("enter\n");
	return platform_driver_register(&ds28e30_driver);
}

static void __exit ds28e30_exit(void)
{
	pr_err("enter\n");
	platform_driver_unregister(&ds28e30_driver);
}

module_init(ds28e30_init);
module_exit(ds28e30_exit);
MODULE_LICENSE("GPL");
