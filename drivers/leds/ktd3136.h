#define REG_DEV_ID				0x00
#define REG_SW_RESET			0x01
#define	REG_MODE				0x02
#define REG_CONTROL				0x03
#define REG_RATIO_LSB			0x04
#define REG_RATIO_MSB			0x05
#define REG_PWM					0x06
#define REG_RAMP_ON				0x07
#define REG_TRANS_RAMP			0x08
#define REG_FLASH_SETTING		0x09
#define REG_STATUS				0x0A

#define BIT_CH3_FAULT			BIT(7)
#define BIT_CH2_FAULT			BIT(6)
#define BIT_CH1_FAULT			BIT(5)
#define BIT_FLASH_TIMEOUT		BIT(4)
#define BIT_OVP					BIT(3)
#define BIT_UVLO				BIT(2)
#define BIT_OCP					BIT(1)
#define BIT_THERMAL_SHUTDOWN	BIT(0)

#define RESET_CONDITION_BITS		\
	(BIT_CH3_FAULT | BIT_CH2_FAULT | BIT_CH1_FAULT | BIT_OVP | BIT_OCP)

#define KTD_I2C_NAME            "ktd,ktd3137"
#define DEFAULT_PWM_NAME    "ktd-backlight"

//struct i2c_client *ktd3137_client;

struct ktd3137_bl_pdata {
	bool pwm_mode;
	bool using_lsb;
	int hwen_gpio;
	unsigned int pwm_period;
	unsigned int full_scale_led;
	unsigned int ramp_on_time;
	unsigned int ramp_off_time;
	unsigned int pwm_trans_dim;
	unsigned int i2c_trans_dim;
	unsigned int channel;
	unsigned int ovp_level;
	unsigned int frequency;
	unsigned int induct_current;
	unsigned int linear_ramp;
	unsigned int linear_backlight;
	unsigned int default_brightness;
	unsigned int max_brightness;
	unsigned int flash_current;
	unsigned int flash_timeout;
	unsigned int prev_bl_current;
};

struct ktd3137_chip {
	struct i2c_client       *client;
	struct backlight_device *bl;
	struct ktd3137_bl_pdata *pdata;
	struct led_classdev     cdev_flash;
	struct delayed_work      work;
	struct device *dev;
	struct pwm_device *pwm;

	unsigned int ktd_chip_id;
	unsigned int ktd_mode_reg;
	unsigned int ktd_brightness_reg;
	unsigned int ktd_ctrl_reg;
	unsigned int ktd_pwm_reg;
	unsigned int ktd_ramp_reg;
	unsigned int ktd_trans_ramp_reg;
	unsigned int ktd_flash_setting_reg;
	unsigned int ktd_status_reg;
};

