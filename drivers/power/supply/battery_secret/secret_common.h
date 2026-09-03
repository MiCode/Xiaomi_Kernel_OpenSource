#ifndef _SECRET_COMMON_H_
#define _SECRET_COMMON_H_

#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>

#define ENABLE_GPIO_REGISTER_CTRL

#define BATT_INFO_PAGE              0
#define FIRST_USE_PAGE              1
#define CYCLE_SOH_PAGE              2
#define W1_SKIP_ROM                 0xCC
#define W1_RESPONSE_SUCCESS         0xAA
#define DC_INIT_VALUE               0x1FFFF

#define W1_CMD_OK                   0
#define W1_BUS_ERROR                -201
#define W1_ERR_VALUE                -202
#define W1_CMD_ERROR                -203
#define W1_RECV_CRC_ERROR           -204

struct w1_data {
	int rawsoh_curr;
	int cycle_count_curr;
	int dc_value_curr;
	int uisoh_valid;
	int fst_use_time_valid;
	u8 uisoh_buf[UISOH_LEN];
	u8 fst_use_time_buf[FST_USE_TIME_LEN];
	int is_auth;
	struct device *dev;
	const char *role;
	const char *chip_name;
	raw_spinlock_t io_lock;
	struct gpio_desc *w1_gpiod;
	struct gpio_desc *enable_gpiod;
	struct secret_device *secret_dev;
	struct regulator *vdd_power;
	uint8_t batt_id;
    int page1_valid;
	int page2_valid;
	u8 page1_buf[32];
    u8 page2_buf[32];
	u32 cmd_len;
	u8 cmd_buffer[256];
    u32 *w1_timings_cfg;
    volatile u32 *w1_gpio_dir;
    volatile u32*w1_gpio_data;
    volatile u32 *w1_gpio_inen;
	int (*page_read)(int index, u8 *read_buf);
    int (*page_write)(int index, const u8 *write_data);
    int (*dc_get)(u32 *dc_cnt);
    int (*dc_decrease)(void);
	int (*w1_bus_send_recv)(u8 *write_buf, int write_len, int delay_ms, u8 *read_buf, int read_len);
};

void w1_hex_dump(const char *prefix, u8 *data_buf, int length);
void w1_mdelay(int ms);
int  w1_reset_bus(void);
void w1_bus_recovery(void);
void w1_write_byte(u8 byte);
void w1_write_block(const u8 *buf, int len);
u8   w1_read_byte(void);
void w1_read_block(u8 *buf, int len);
int  w1_sec_dev_init(struct w1_data *w1_data_prv, const char *name);

#endif
