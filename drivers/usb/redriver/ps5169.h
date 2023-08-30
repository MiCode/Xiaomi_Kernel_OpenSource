#ifndef _PS5169_REGULATOR_H_
#define _PS5169_REGULATOR_H_

#define REG_CONFIG_MODE 0x40
#define REG_AUX_ENABLE	0xa0
#define REG_HPD_PLUG	0xa1

#define REG_CHIP_ID_L	0xac
#define REG_CHIP_ID_H	0xad
#define REG_REVISION_L	0xae
#define REG_REVISION_H	0xaf

enum operation_mode {
	OP_MODE_NONE,		/* 4 lanes disabled */
	OP_MODE_USB,		/* 2 lanes for USB and 2 lanes disabled */
	OP_MODE_DP,		/* 4 lanes DP */
	OP_MODE_USB_AND_DP,	/* 2 lanes for USB and 2 lanes DP */
	OP_MODE_DEFAULT,	/* 4 lanes USB */
};

enum cc_orientation {
	CC1_ORIENTATION = 1,
	CC2_ORIENTATION,
};

struct ps5169_info {
	char                        *name;
	struct device               *dev;
	struct i2c_client           *client;
	struct mutex                i2c_lock;
	struct regmap               *regmap;
	struct pinctrl              *ps5169_pinctrl;
	struct pinctrl_state        *ps5169_gpio_active;
	struct pinctrl_state        *ps5169_gpio_suspend;
	struct class         		ps_class;
	unsigned int                enable_gpio;
	unsigned int                cc_gpio;
	unsigned int                ps_enable;
	unsigned int                pre_ps_enable;
	struct notifier_block 		ps5169_nb;
	//struct delayed_work         ps_en_work;
	int                         flip;
	int							cc_flag;
	int							initCFG_flag;
	enum operation_mode         op_mode;
	struct workqueue_struct *pullup_wq;
	struct work_struct	pullup_work;
	bool                         host;


	bool			work_ongoing;
};
static const char * const opmode_string[] = {
	[OP_MODE_NONE] = "NONE",
	[OP_MODE_USB] = "USB",
	[OP_MODE_DP] = "DP",
	[OP_MODE_USB_AND_DP] = "USB and DP",
	[OP_MODE_DEFAULT] = "DEFAULT",
};
#define OPMODESTR(x) opmode_string[x]

#define ps5169_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[PS5169] " fmt, ##__VA_ARGS__);	\
} while (0)

#define ps5169_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[PS5169] " fmt, ##__VA_ARGS__);	\
} while (0)

#define ps5169_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[PS5169] " fmt, ##__VA_ARGS__);	\
} while (0)

void ps5169_get_chipcfg_and_modeselection(void);
void ps5169_notify_connect(bool host);
void ps5169_notify_disconnect(void);
void ps5169_release_usb_lanes(int lanes);
 int ps5169_gadget_pullup_enter( int is_on);
 int ps5169_gadget_pullup_exit( int is_on);

#endif

