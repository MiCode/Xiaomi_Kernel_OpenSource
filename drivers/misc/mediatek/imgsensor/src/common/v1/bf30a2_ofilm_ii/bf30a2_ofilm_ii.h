#ifndef __BF30A2_H__
#define __BF30A2_H__
#define BF30A2_I2C_RETRIES		(1)
#define BF30A2_I2C_RETRY_DELAY		(2)
/********************************************
 *
 * bf30a2 struct
 *
 *******************************************/
/* code at 2022/08/1 start */
struct bf30a2 {
	uint8_t i2c_seq;
	uint8_t i2c_addr;
	uint8_t hwen_flag;
	char bus_num[2];
	int reset_gpio;
	struct i2c_client *i2c_client;
	struct device *dev;
	struct pinctrl *bf30a2_pinctrl;
};
/* code at 2022/08/1 end */
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