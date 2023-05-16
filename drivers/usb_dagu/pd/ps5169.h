#ifndef _PS5169_REGULATOR_H_
#define _PS5169_REGULATOR_H_

#define REG_CONFIG_MODE 0x40
#define REG_AUX_ENABLE	0xa0
#define REG_HPD_PLUG	0xa1

#define REG_CHIP_ID_L	0xac
#define REG_CHIP_ID_H	0xad
#define REG_REVISION_L	0xae
#define REG_REVISION_H	0xaf

struct ps5169_info {
	char                        *name;
	struct device               *dev;
	struct i2c_client           *client;
	struct mutex                i2c_lock;
	struct regmap               *regmap;
	struct pinctrl              *ps5169_pinctrl;
	struct pinctrl_state        *ps5169_gpio_active;
	struct pinctrl_state        *ps5169_gpio_suspend;
	struct power_supply         *ps_psy;
	struct power_supply_desc    ps_psy_desc;
	unsigned int                enable_gpio;
	unsigned int                ps_enable;
	unsigned int                pre_ps_enable;
	bool                        present_flag;
	struct delayed_work         ps_en_work;
	int                         flip;
};

void ps5169_cfg_usb(void);

#endif

