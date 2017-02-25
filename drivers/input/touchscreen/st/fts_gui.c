#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/completion.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include "fts.h"
#include "fts_lib/ftsCompensation.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsTest.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"

#ifdef SCRIPTLESS

unsigned int data[CMD_RESULT_STR_LEN] = {0};
unsigned char pAddress_i2c[CMD_RESULT_STR_LEN] = {0};
int byte_count_read;
char Out_buff[TSP_BUF_SIZE];

/*I2C CMd functions: functions to interface with GUI without script */

ssize_t fts_i2c_wr_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int i;
	char buff[16];
	memset(Out_buff, 0x00, ARRAY_SIZE(Out_buff));
	if (byte_count_read == 0) {
		snprintf(Out_buff, sizeof(Out_buff), "{FAILED}");
		return snprintf(buf, TSP_BUF_SIZE, "{%s}\n", Out_buff);
	}
#ifdef SCRIPTLESS_DEBUG
	 printk("%s:DATA READ {", __func__);
	for (i = 0; i < byte_count_read; i++) {
		printk(" %02X", (unsigned int)info->cmd_wr_result[i]);
		if (i < (byte_count_read-1)) {
			printk(" ");
		}
	}
	printk("}\n");
#endif
	snprintf(buff, sizeof(buff), "{");
	strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
	for (i = 0; i < (byte_count_read+2); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);

		} else {
			snprintf(buff, sizeof(buff), "%02X", info->cmd_wr_result[i-2]);
		}
		/* snprintf(buff, sizeof(buff), "%02X", info->cmd_wr_result[i]); */
		strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
		if (i < (byte_count_read+1)) {
			snprintf(buff, sizeof(buff), " ");
			strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
		}
	}
	snprintf(buff, sizeof(buff), "}");
	strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
	return snprintf(buf, TSP_BUF_SIZE, "%s\n", Out_buff);
}

ssize_t fts_i2c_wr_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char pAddress[8] = {0};
	unsigned int byte_count = 0 ;
	int i ;

	unsigned int data[8] = {0};
	memset(data, 0x00, ARRAY_SIZE(data));
	memset(info->cmd_wr_result, 0x00, ARRAY_SIZE(info->cmd_wr_result));
	sscanf(buf, "%x %x %x %x %x %x %x %x ", (data+7), (data), (data+1),
		(data+2), (data+3), (data+4), (data+5), (data+6));

	byte_count = data[7];

	/*if (sizeof(buf) != byte_count )
	{
		printk("%s : Byte count is wrong\n",__func__);
		return count;
	}*/
#ifdef SCRIPTLESS_DEBUG
	printk("\n");
	printk("%s: Input Data 1:", __func__);

	for (i = 0 ; i < 7; i++) {
		 printk(" %02X", data[i]);
		pAddress[i] = (unsigned char)data[i];
	}
	printk("\n");
#else
	for (i = 0 ; i < 7; i++) {
		pAddress[i] = (unsigned char)data[i];
	}
#endif
	byte_count_read = data[byte_count-1];
	ret = fts_writeCmd(pAddress, 3);
	msleep(20);
	ret = fts_readCmd(&pAddress[3], (byte_count-4), info->cmd_wr_result,
		byte_count_read);
#ifdef SCRIPTLESS_DEBUG
	printk("%s:DATA READ\n{", __func__);
	for (i = 0; i < (2+byte_count_read); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			printk("%02X", (unsigned int)temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			printk("%02X", (unsigned int)temp_byte_count_read);

		} else {
			printk("%02X", (unsigned int)info->cmd_read_result[i-2]);
		}
		if (i < (byte_count_read+1)) {
			printk(" ");
		}

	}
	printk("}\n");
#endif
	if (ret)
		dev_err(dev, "Unable to read register\n");
	return count;
}

ssize_t fts_i2c_read_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	int i ;
	char buff[16];

	memset(Out_buff, 0x00, ARRAY_SIZE(Out_buff));
	if (byte_count_read == 0) {
		snprintf(Out_buff, sizeof(Out_buff), "{FAILED}");
		return snprintf(buf, TSP_BUF_SIZE, "{%s}\n", Out_buff);
	}
#ifdef SCRIPTLESS_DEBUG
	printk("%s:DATA READ {", __func__);
	for (i = 0; i < byte_count_read; i++) {
		printk("%02X", (unsigned int)info->cmd_read_result[i]);
		if (i < (byte_count_read-1)) {
			printk(" ");
		}
	}
	printk("}\n");
#endif
	snprintf(buff, sizeof(buff), "{");
	strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
	for (i = 0; i < (byte_count_read+2); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			snprintf(buff, sizeof(buff), "%02X", temp_byte_count_read);

		} else {
			snprintf(buff, sizeof(buff), "%02X", info->cmd_read_result[i-2]);
		}
		strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
		if (i < (byte_count_read+1)) {
			snprintf(buff, sizeof(buff), " ");
			strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));
		}
	}
	snprintf(buff, sizeof(buff), "}");
	strlcat(Out_buff, buff,  ARRAY_SIZE(Out_buff));

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", Out_buff);
}

ssize_t fts_i2c_read_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned char pAddress[8] = {0};
	unsigned int byte_count = 0;
	int i ;
	unsigned int data[8] = {0};

	byte_count_read = 0;
	memset(data, 0x00, ARRAY_SIZE(data));
	memset(info->cmd_read_result, 0x00, ARRAY_SIZE(info->cmd_read_result));
	sscanf(buf, "%x %x %x %x %x %x %x %x ", (data+7), (data), (data+1), (data+2), (data+3), (data+4), (data+5), (data+6));
	byte_count = data[7];

	if (byte_count > 7) {
#ifdef SCRIPTLESS_DEBUG
		printk("%s : Byte count is more than 7\n", __func__);
#endif
		return count;
	}
	/*if (sizeof(buf) != byte_count )
	{
		printk("%s : Byte count is wrong\n",__func__);
		return count;
	}*/
#ifdef SCRIPTLESS_DEBUG
	printk("\n");
	printk("%s: Input Data 1:", __func__);
	for (i = 0 ; i < byte_count; i++) {
		 printk(" %02X", data[i]);
		pAddress[i] = (unsigned char)data[i];
	}
	printk("\n");
#else
	for (i = 0 ; i < byte_count; i++) {
		pAddress[i] = (unsigned char)data[i];
	}
#endif
	byte_count_read = data[byte_count-1];
	ret = fts_readCmd(pAddress, (byte_count-1), info->cmd_read_result, byte_count_read);
#ifdef SCRIPTLESS_DEBUG
	printk("%s:DATA READ\n{", __func__);
	for (i = 0; i < (byte_count_read+2); i++) {
		if ((i == 0)) {
			char temp_byte_count_read = (byte_count_read >> 8) & 0xFF;
			printk("%02X", (unsigned int)temp_byte_count_read);
		} else if (i == 1) {
			char temp_byte_count_read = (byte_count_read) & 0xFF;
			printk("%02X", (unsigned int)temp_byte_count_read);

		} else {
			printk("%02X", (unsigned int)info->cmd_read_result[i-2]);
		}
		if (i < (byte_count_read+1)) {
			printk(" ");
		}
	}
	printk("}\n");
#endif
	if (ret)
		dev_err(dev, "Unable to read register\n");
	return count;
}

ssize_t fts_i2c_write_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	return snprintf(buf, TSP_BUF_SIZE, "%s", info->cmd_write_result);

}

ssize_t fts_i2c_write_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	unsigned int byte_count = 0;
	int i ;
	memset(data, 0x00, ARRAY_SIZE(data));
	memset(pAddress_i2c, 0x00, ARRAY_SIZE(pAddress_i2c));
	memset(info->cmd_write_result, 0x00, ARRAY_SIZE(info->cmd_write_result));
	sscanf(buf, "%x %x", data, (data + 1));
	byte_count = data[0] << 8 | data[1];

	if (byte_count <= ARRAY_SIZE(pAddress_i2c)) {
		for (i = 0; i < (byte_count); i++) {
			sscanf(&buf[3*(i+2)], "%x ", (data+i));
		} } else {
#ifdef SCRIPTLESS_DEBUG
		printk("%s : message size is more than allowed limit of 512 bytes\n", __func__);
#endif
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write NOT OK}\n");
	}
#ifdef SCRIPTLESS_DEBUG
	printk("\n");
	printk("%s: Byte_count=  %02d | Count = %02d | size of buf:%02d\n", __func__, byte_count, (int)count, (int)sizeof(buf));
	printk("%s: Input Data 1:", __func__);
	for (i = 0 ; i < byte_count; i++) {
		printk("%02X", data[i]);
		pAddress_i2c[i] = (unsigned char)data[i];
	}
	printk("\n");
#else
	for (i = 0; i < byte_count; i++) {
		pAddress_i2c[i] = (unsigned char)data[i];
	}
#endif
	if ((pAddress_i2c[0] == 0xb3) && (pAddress_i2c[3] == 0xb1)) {
		ret = fts_writeCmd(pAddress_i2c, 3);
		msleep(20);
		ret = fts_writeCmd(&pAddress_i2c[3], byte_count-3);
	} else {
		ret = fts_writeCmd(pAddress_i2c, byte_count);
	}

#ifdef SCRIPTLESS_DEBUG
	printk("%s:DATA :", __func__);
	for (i = 0; i < byte_count; i++) {
		printk(" %02X", (unsigned int)pAddress_i2c[i]);
	}
	printk(" byte_count: %02X\n", byte_count);
#endif
	if (ret < 0) {
		dev_err(dev, "{Write NOT OK}\n");
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write NOT OK}\n");
	} else {
		snprintf(info->cmd_write_result, sizeof(info->cmd_write_result), "{Write OK}\n");
#ifdef SCRIPTLESS_DEBUG
		printk("%s : {Write OK}\n", __func__);
#endif
	}
	return count;
}

static DEVICE_ATTR(iread, (S_IWUSR|S_IWGRP), NULL, fts_i2c_read_store);
static DEVICE_ATTR(iread_result, (S_IRUSR|S_IRGRP), fts_i2c_read_show, NULL);
static DEVICE_ATTR(iwr, (S_IWUSR|S_IWGRP), NULL, fts_i2c_wr_store);
static DEVICE_ATTR(iwr_result, (S_IRUSR|S_IRGRP), fts_i2c_wr_show, NULL);
static DEVICE_ATTR(iwrite, (S_IWUSR|S_IWGRP), NULL, fts_i2c_write_store);
static DEVICE_ATTR(iwrite_result, (S_IRUSR|S_IRGRP), fts_i2c_write_show, NULL);

static struct attribute *i2c_cmd_attributes[] = {
	&dev_attr_iread.attr,
	&dev_attr_iread_result.attr,
	&dev_attr_iwr.attr,
	&dev_attr_iwr_result.attr,
	&dev_attr_iwrite.attr,
	&dev_attr_iwrite_result.attr,
	NULL,
};

struct attribute_group i2c_cmd_attr_group = {
	.attrs = i2c_cmd_attributes,
};

#endif
