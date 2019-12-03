#ifndef __ONEWIRE_GPIO_H__
#define __ONEWIRE_GPIO_H__

#define ONEWIRE_GPIO_OUTLOW		0
#define ONEWIRE_GPIO_OUTHIGH		1
#define ONEWIRE_GPIO_CONFIG_OUT		2
#define ONEWIRE_GPIO_CONFIG_IN		3
#define ONEWIRE_GPIO_RESET		4
#define ONEWIRE_GPIO_READ_BIT		5
#define ONEWIRE_GPIO_WRITE_BIT		6
#define ONEWIRE_GPIO_READ_ROMID		7

#define ow_info pr_info
#define ow_dbg  pr_debug
#define ow_err  pr_err
#define ow_log  pr_info

#define DRV_STRENGTH_16MA	(0x7 << 6)
#define GPIO_OUTPUT		(0x1 << 9)
#define GPIO_INPUT		(0x0 << 9)
#define GPIO_PULL_UP		0x3
#define OUTPUT_HIGH		(0x1 << 1)
#define OUTPUT_LOW		0x1

#define ONE_WIRE_CONFIG_OUT		writel_relaxed(DRV_STRENGTH_16MA | GPIO_OUTPUT | GPIO_PULL_UP,\
							g_onewire_data->gpio_cfg_reg)// OUT
#define ONE_WIRE_CONFIG_IN		writel_relaxed(DRV_STRENGTH_16MA | GPIO_INPUT | GPIO_PULL_UP,\
							g_onewire_data->gpio_cfg_reg)// IN
#define ONE_WIRE_OUT_HIGH		writel_relaxed(OUTPUT_HIGH, g_onewire_data->gpio_value_reg)// OUT: 1
#define ONE_WIRE_OUT_LOW		writel_relaxed(OUTPUT_LOW, g_onewire_data->gpio_value_reg)// OUT: 0

void Delay_us(unsigned int T);
void Delay_ns(unsigned int T);
unsigned char ow_reset(void);
unsigned char read_byte(void);
void write_byte(char val);

#endif
