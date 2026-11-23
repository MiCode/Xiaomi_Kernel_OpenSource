#ifndef RSMC_SPI_X801_H
#define RSMC_SPI_X801_H

//#include <cetc_platform/log/cetc_log.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <uapi/linux/sched/types.h>
#include <linux/regulator/consumer.h>

#include "rsmc_msg_loop_x801.h"

#define GPIO_LOW 0
#define GPIO_HIGH 1
#define SMC_SPI_SPEED_DEFAULT (1 * 1000 * 1000)
#define SMC_SPI_DEFAULT_BITS_PER_WORD 8
#define ENABLE_SIGNAL_DELAY_MS 5
#define MCU_RST_SIGNAL_DELAY_MS 5
#define SPI_MSLEEP_SHORT_TIME 5

#define X801_MIN_BITS_PER_WORD 8
#define X801_MIN_SPEED (1625 * 1000)

#define X801_MAX_BITS_PER_WORD 8
#define X801_MAX_SPEED (5000 * 1000)

#define REG00_VALUE 0x527091
#define REG03_VALUE 0x465
#define REG04_VALUE 0x476

//Use CS pin control
//#define X801_SPI_CS

enum x801_reg_addr {
	REG_00 = 0x00,
	REG_01 = 0x01,
	REG_02 = 0x02,
	REG_03 = 0x03,
	REG_04 = 0x04,
	REG_05 = 0x05,
	REG_06 = 0x06
};

struct reg02_status_stru {
	u32 bit0 : 1;
	u32 bit1 : 1;
	u32 pll : 1;
	u32 spi_enable : 1;
	u32 bit_reserv : 28;
};

enum RSMC_power_type {
	RSMC_POWER_UNUSED = 0,
	RSMC_POWER_GPIO = 1,
	RSMC_POWER_LDO = 2,
	RSMC_POWER_INVALID_TYPE,
};

enum SPI_STATUS {
	SPI_NORMAL = 0,
	SPI_TRANS = 1,
	SPI_RECV = 2,
};

struct smc_gpios {
	u32 soc_pwr_gpio;
	u32 soc_en_gpio;
	u32 mcu_reset_gpio;
	u32 tx_ant_gpio;
	u32 rx_ant_gpio;
	u32 pa_pwr_gpio;
	u32 pa_enable_gpio;
  	u32 rd_ls_gpio;
	u32 cs_gpio;
	u32 irq_gpio;
	u32 vc1_ant_gpio;
	u32 vc2_ant_gpio;
	u32 vc3_ant_gpio;
	u32 tt_bd_sel_gpio;
	u32 vdet_sel_gpio;
	u32 vc1_default_value;
	u32 vc2_default_value;
	u32 vc3_default_value;
};

struct smc_spi_config {
	u32 max_speed_hz;
	u16 mode;
	u8 bits_per_word;
	u8 bus_id;
};

struct smc_device {
	char *ic_name;
	char *dev_node_name;
	struct spi_device *sdev;
	struct smc_core_data *smc_core;
	struct smc_gpios *gpios;
	void *private_data;
};

struct rsmc_power_supply {
	int use_count;
	int type;
	int gpio;
	int ldo_value;
	u32 tcxo_pwr_after_delay_ms;
	struct regulator *regulator;
};

struct smc_core_data {
	struct spi_transfer t;
	struct spi_message msg;
	u8 tx_buf[512];
	u8 tx_buff[4];
	u8 rx_buff[4];
	struct spi_device *sdev;
	struct smc_device *smc_dev;
	struct device_node *smc_node;
	struct smc_spi_config spi_config;
	struct mutex spi_mutex;
	atomic_t register_flag;
	int irq;
	struct smc_gpios gpios;
	struct rsmc_power_supply rsmc_powers;
	struct regulator *pa_power;
};

struct smc_core_data *smc_get_core_data_x801(void);
int rsmc_register_dev_x801(struct smc_device *dev);
int rsmc_unregister_dev_x801(struct smc_device *dev);
int rsmc_spi_read_x801(unsigned char *data, unsigned int length);
int rsmc_spi_write_x801(unsigned char *data, unsigned int length);
int rsmc_spi_reg_read(u32 addr, u32 *value);
int rsmc_spi_reg_write(u32 addr, u32 value);
int rsmc_spi_set_min_speed(void);
int rsmc_spi_set_max_speed(void);
void rsmc_spi_cs_set(u32 control);
int rsmc_spi_init_x801(void);
void rsmc_spi_exit_x801(void);
void rsmc_spi_irq_ctl_x801(bool ctl);

extern volatile int is_pic_mode;

#endif /* RSMC_SPI_DRV_H */

