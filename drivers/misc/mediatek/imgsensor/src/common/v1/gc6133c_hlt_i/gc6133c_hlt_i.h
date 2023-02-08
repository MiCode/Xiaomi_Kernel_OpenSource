#ifndef __GC6133C_H__
#define __GC6133C_H__
#define GC6133C_I2C_RETRIES		(1)
#define GC6133C_I2C_RETRY_DELAY		(2)
/********************************************
 *
 * gc6133c struct
 *
 *******************************************/
struct gc6133c {
	uint8_t i2c_seq;
	uint8_t i2c_addr;
	uint8_t hwen_flag;
	char bus_num[2];
	struct i2c_client *i2c_client;
	struct device *dev;
	struct pinctrl *gc6133c_pinctrl;
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
