
#ifndef __SC080CS_H__
#define __SC080CS_H__
#define SC080CS_I2C_RETRIES		(1)
#define SC080CS_I2C_RETRY_DELAY		(2)
/********************************************
 *
 * sc080cs struct
 *
 *******************************************/
struct sc080cs {
	uint8_t i2c_seq;
	uint8_t i2c_addr;
	uint8_t hwen_flag;
	char bus_num[2];
	int reset_gpio;
	int reset_gpio1;
	int reset_gpio2;
	struct i2c_client *i2c_client;
	struct device *dev;
};
/********************************************
 *
 * print information control
 *
 *******************************************/
#define qvga_dev_err(dev, format, ...) \
			pr_err("[%s]" format, dev_name(dev), ##__VA_ARGS__)
#define qvga_dev_info(dev, format, ...) \
			pr_info("[%s]" format, dev_name(dev), ##__VA_ARGS__)
#define qvga_dev_dbg(dev, format, ...) \
			pr_debug("[%s]" format, dev_name(dev), ##__VA_ARGS__)
#endif
