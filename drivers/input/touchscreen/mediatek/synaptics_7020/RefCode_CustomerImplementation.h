#include <linux/i2c.h>

extern int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf);
extern int synaptics_ts_read_f54(struct i2c_client *client, u8 reg, int num, u8 *buf);
extern int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 * buf, int len);
extern struct i2c_client* ds4_i2c_client;

void device_I2C_read(unsigned char add, unsigned char *value, unsigned short len);
void device_I2C_write(unsigned char add, unsigned char *value, unsigned short len);
void InitPage(void);
void SetPage(unsigned char page);
void readRMI(unsigned short add, unsigned char *value, unsigned short len);
void longReadRMI(unsigned short add, unsigned char *value, unsigned short len);
void writeRMI(unsigned short add, unsigned char *value, unsigned short len);
void delayMS(int val);
void cleanExit(int code);
int waitATTN(int code, int time);
void write_log(char *data);
