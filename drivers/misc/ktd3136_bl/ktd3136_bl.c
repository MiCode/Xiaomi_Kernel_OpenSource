#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/ktd3136_bl.h>
#include <linux/delay.h>

#define KTD_I2C_NAME "ktd3136"
#define LM3697_CHIP 1
#define BKL_INFO		"backlight_ic_info"
#define BKL_RW			"backlight_ic_reg"
#define PROC_BACKLIGHT_FOLDER	"ktd3136_backlight"

#define KTD_DEBUG

#ifdef KTD_DEBUG
#define LOG_DBG(fmt, args...) printk(KERN_INFO "[drm][ktd]"fmt"\n", ##args)
#endif

struct i2c_client *g_blclient;
struct ktd3136_data *reg;
static u8 g_bl_id, vaL_show;
static int *bl_mapping_table, reg_show;
static struct proc_dir_entry *proc_backlight_dir = NULL;
static struct proc_dir_entry *backlight_ic_info_entry = NULL;
static struct proc_dir_entry *backlight_ic_reg_entry = NULL;

static int ktd3136_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;
	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int ktd3136_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}
	*val = ret;
	return ret;
}

#if 0
static int ktd3136_masked_write(struct i2c_client *client,
					int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0;
	rc = ktd3136_read_reg(client, reg, &temp);
	if (rc < 0) {
		dev_err(&client->dev, "failed to read reg\n");
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3136_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev, "failed to write masked data\n");
	}
	ktd3136_read_reg(client, reg, &temp);
	return rc;
}
#endif

static void ktd_parse_dt(struct device *dev, struct ktd3136_chip *chip)
{
	struct device_node *np = dev->of_node;

	chip->hwen_gpio = of_get_named_gpio(np, "ktd,hwen-gpio", 0);
	pr_info("hwen: %d\n", chip->hwen_gpio);
}

static int ktd3136_gpio_init(struct ktd3136_chip *chip)
{
	int ret;
	if (gpio_is_valid(chip->hwen_gpio)) {
		ret = gpio_request(chip->hwen_gpio, "ktd_hwen_gpio");
		if (ret < 0) {
			pr_err("failed to request gpio\n");
			return  -ENOMEM;
		}
		pr_err("ktd3136 gpio is valid!\n");
	}
	return 0;
}

struct ktd3136_data {
	int reg;
	u8 val;
};
struct ktd3136_data bl_lm3697_0a[8] = {
  {0x10, 0x03},
  {0x13, 0x11},
  {0x16, 0x00},
  {0x19, 0x03},
  {0x18, 0x14},	//21mA
  {0x1A, 0x0E},
  {0x1C, 0x0E}, //pwm mode
  {0x24, 0x02},
};

struct ktd3136_data bl_lm3697_0b[8] = {
  {0x10, 0x03},
  {0x13, 0x11},
  {0x16, 0x00},
  {0x19, 0x03},
  {0x18, 0x13},	//20.2mA
  {0x1A, 0x0E},
  {0x1C, 0x0E}, //pwm mode
  {0x24, 0x02},
};

struct ktd3136_data bl_ktd3136_0a[5] = {
  {0x02, 0xA1},	//21mA
  {0x03, 0x60},
//  {0x04, 0x07},
//  {0x05, 0xFF},
  {0x06, 0x1B}, //pwm mode
  {0x07, 0x00},
  {0x08, 0x13},
};

struct ktd3136_data bl_ktd3136_0b[5] = {
  {0x02, 0x99},	//20.2mA
  {0x03, 0x60},
//  {0x04, 0x07},
//  {0x05, 0xFF},
  {0x06, 0x1B}, //pwm mode
  {0x07, 0x00},
  {0x08, 0x13},
};

/*--------------------HBM_BEGIN---------------------*/
struct ktd3136_data lm3697_reg_a[4] = {
	{0x18, 0x14},	//OFF 21mA
	{0x18, 0x16},	//L1 22.6mA
	{0x18, 0x19},	//L2 25mA
	{0x18, 0x1C},	//L3 27.4mA
};
struct ktd3136_data lm3697_reg_b[4] = {
	{0x18, 0x13},	//OFF 20.2mA
	{0x18, 0x15},	//L1 21.8mA
	{0x18, 0x18},	//L2 24.2mA
	{0x18, 0x1A},	//L3 25.8mA
};

struct ktd3136_data ktd3136_reg_a[4] = {
	{0x02, 0xA1},	//OFF 21mA
	{0x02, 0xB1},	//L1 22.6mA
	{0x02, 0xC9},	//L2 25mA
	{0x02, 0xE1},	//L3 27.4mA
};
struct ktd3136_data ktd3136_reg_b[4] = {
	{0x02, 0x99},	//OFF 20.2mA
	{0x02, 0xA9},	//L1 21.8mA
	{0x02, 0xC1},	//L2 24.2mA
	{0x02, 0xD1},	//L3 25.8mA
};
/*--------------------HBM_END---------------------*/

void hbm_set_mode(bool panel_m19, u32 hbm_mode)
{
	if(g_bl_id == LM3697_CHIP) {
		if(panel_m19)
			reg = lm3697_reg_a;
		else
			reg = lm3697_reg_b;
	} else {
		if(panel_m19)
			reg = ktd3136_reg_a;
		else
			reg = ktd3136_reg_b;
	}

	switch(hbm_mode){
		case LCD_HBM_OFF:
			ktd3136_write_reg(g_blclient, reg[0].reg, reg[0].val);
			break;
		case LCD_HBM_L1_ON:
			ktd3136_write_reg(g_blclient, reg[1].reg, reg[1].val);
			break;
		case LCD_HBM_L2_ON:
			ktd3136_write_reg(g_blclient, reg[2].reg, reg[2].val);
			break;
		case LCD_HBM_L3_ON:
			ktd3136_write_reg(g_blclient, reg[3].reg, reg[3].val);
			break;
		default:
			pr_info("[drm] unknown hbm mode type:0x%x\n", hbm_mode);
			break;
	}
}
EXPORT_SYMBOL(hbm_set_mode);

void ktd3136_reg_init(bool which_display)
{
	int i;

	struct ktd3136_chip *ktd = i2c_get_clientdata(g_blclient);

	mutex_lock(&ktd->i2c_rw_lock);

	if(which_display) {
		LOG_DBG("into nt36672c, bl_ic[%d] reg init\n", g_bl_id);
		if (g_bl_id == LM3697_CHIP) {//LM3697_id = 1
			for (i = 0; i < 8; i++)
			{
				ktd3136_write_reg(ktd->client, bl_lm3697_0a[i].reg, bl_lm3697_0a[i].val);
			}
		} else {//ktd_id = 0x18(24)
			for (i = 0; i < 5; i++)
			{
				ktd3136_write_reg(ktd->client, bl_ktd3136_0a[i].reg, bl_ktd3136_0a[i].val);
			}
		}
	} else {
		LOG_DBG("into ft8720, bl_ic[%d] reg init\n", g_bl_id);
		if (g_bl_id == LM3697_CHIP) {//LM3697_id = 1
			for (i = 0; i < 8; i++)
			{
				ktd3136_write_reg(ktd->client, bl_lm3697_0b[i].reg, bl_lm3697_0b[i].val);
			}
		} else {//ktd_id = 0x18(24)
			for (i = 0; i < 5; i++)
			{
				ktd3136_write_reg(ktd->client, bl_ktd3136_0b[i].reg, bl_ktd3136_0b[i].val);
			}
		}
	}
	mutex_unlock(&ktd->i2c_rw_lock);
}
EXPORT_SYMBOL(ktd3136_reg_init);

static int pre_state = 1;
int ktd3136_bl_set_brightness(u32 bl_lvl)
{
	int brightness;
	int cur_state;

	if(!bl_mapping_table) {
		LOG_DBG("bl_mapping_table is invalid\n");
		return -1;
	}

	brightness = bl_mapping_table[bl_lvl];
	cur_state =  brightness > 0 ? 1 : 0;
	LOG_DBG("%s: brightness=%d, bl_lvl=%u\n", __func__, brightness, bl_lvl);

	if (pre_state != cur_state) {
		if(g_bl_id == LM3697_CHIP) {
			ktd3136_write_reg(g_blclient, 0x13, 0x0);
		} else {
			ktd3136_write_reg(g_blclient, 0x08, 0x0);
		}
	}

	if(g_bl_id == LM3697_CHIP) {
		ktd3136_write_reg(g_blclient, 0x22, brightness & 0x07);
		ktd3136_write_reg(g_blclient, 0x23, (brightness >> 3) & 0xFF);
	} else {
		ktd3136_write_reg(g_blclient, 0x04, brightness & 0x07);
		ktd3136_write_reg(g_blclient, 0x05, (brightness >> 3) & 0xFF);
	}

	if (pre_state != cur_state) {
		mdelay(3);
		if(g_bl_id == LM3697_CHIP) {
			ktd3136_write_reg(g_blclient, 0x13, 0x11);
		} else {
			ktd3136_write_reg(g_blclient, 0x08, 0x13);
		}
	}
	pre_state = cur_state;
	return 0;
}
EXPORT_SYMBOL(ktd3136_bl_set_brightness);

static int32_t i2c_reg_dump_show(struct seq_file *m, void *v)
{
	u8 table_num = 0, i = 0, reg_value = 0;
	u8 *table_end = NULL;
	u8 lm3679_reg_table[] = {0x00, 0x01, 0x10, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x20, 0x21, 0x22, 0x23, 0x24, 0xB0, 0xB2, 0xB4, 0xFF};	//0xFF : End of array flag bit
	u8 ktd3136_reg_table[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0xFF};

	if(g_bl_id == LM3697_CHIP) {
		table_end = lm3679_reg_table;
		while(*table_end != 0xFF)
			table_end++;
		table_num = table_end - lm3679_reg_table;
		LOG_DBG("lm3679_reg_table number = %d\n", table_num);
		for(i=0; i<table_num; i++) {
			ktd3136_read_reg(g_blclient, lm3679_reg_table[i], &reg_value);
			seq_printf(m, "0x%02x=0x%02x\n", lm3679_reg_table[i], reg_value);
		}
	} else {
		table_end = ktd3136_reg_table;
		while(*table_end != 0xFF)
			table_end++;
		table_num = table_end - ktd3136_reg_table;
		LOG_DBG("ktd3136_reg_table number = %d\n", table_num);
		for(i=0; i<table_num; i++) {
			ktd3136_read_reg(g_blclient, ktd3136_reg_table[i], &reg_value);
			seq_printf(m, "0x%02x=0x%02x\n", ktd3136_reg_table[i], reg_value);
		}
	}

	return 0;
}

static int32_t i2c_reg_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, i2c_reg_dump_show, NULL);
}

static int32_t i2c_reg_read(struct seq_file *m, void *v)
{
	ktd3136_read_reg(g_blclient, reg_show, &vaL_show);
	seq_printf(m, "0x%02x = 0x%02x\n", reg_show, vaL_show);

	return 0;
}

static ssize_t i2c_reg_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	char *temp_buf = NULL, *temp_copy = NULL;
	char *temp;
	const char *delim = " ";
	int temp_sep;

	temp_buf = kzalloc(count + 1, GFP_KERNEL);
	temp_copy = temp_buf;
	if(!temp_buf){
		LOG_DBG("allocate temp_buf failed\n");
		goto kfree;
	}
	if(copy_from_user(temp_buf, buf, count)){
		LOG_DBG("copy from user failed\n");
		goto kfree;
	}
	if(count < 6){
		sscanf(temp_buf, "%x", &reg_show);
		LOG_DBG("reg_show = 0x%x\n",reg_show);
		goto kfree;
	}
	if(!strchr(temp_buf, ' ')){
		LOG_DBG("buf have not ' '\n");
		goto kfree;
	}
	temp_buf = strim(temp_buf);
	temp = strsep(&temp_buf, delim);
	if(temp){
		kstrtoint(temp, 16, &reg_show);
		reg_show = reg_show & 0xff;
	}else
		goto kfree;
	temp = strsep(&temp_buf, delim);
	if(temp){
		kstrtoint(temp, 16, &temp_sep);
		temp_sep = temp_sep & 0xff;
		vaL_show = temp_sep;
	}else
		goto kfree;
	LOG_DBG("reg_show = 0x%x val_show = 0x%x \n", reg_show, vaL_show);
	ktd3136_write_reg(g_blclient, reg_show, vaL_show);

kfree:
	kfree(temp_copy);

	return count;
}

static int32_t i2c_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, i2c_reg_read, NULL);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
static const struct proc_ops backlight_ic_info_fops = {
	.proc_open  = i2c_reg_dump_open,
	.proc_read  = seq_read,
};
static const struct proc_ops backlight_ic_reg_fops = {
	.proc_open = i2c_reg_open,
	.proc_read = seq_read,
	.proc_write = i2c_reg_write,
};
#else
static const struct file_operations backlight_ic_info_fops = {
	.open  = i2c_reg_dump_open,
	.read  = seq_read,
};
static const struct file_operations backlight_ic_reg_fops = {
	.open = i2c_reg_open,
	.read = seq_read,
	.write = i2c_reg_write,
};
#endif
static int ktd_node_init(void)
{
	proc_backlight_dir = proc_mkdir(PROC_BACKLIGHT_FOLDER, NULL);
	if(!proc_backlight_dir) {
		LOG_DBG("%s proc_backlight_dir file create failed\n", __func__);
		remove_proc_entry(PROC_BACKLIGHT_FOLDER, NULL);
	} else {
		LOG_DBG("%s proc_backlight_dir file create success\n", __func__);
	}
	backlight_ic_info_entry = proc_create(BKL_INFO, S_IRUSR, proc_backlight_dir, &backlight_ic_info_fops);
	backlight_ic_reg_entry = proc_create(BKL_RW, S_IRWXU, proc_backlight_dir, &backlight_ic_reg_fops);
	if(!backlight_ic_info_entry) {
		LOG_DBG("%s backlight_ic_info_entry file create failed\n", __func__);
		remove_proc_entry(BKL_INFO, NULL);
	} else {
		LOG_DBG("%s backlight_ic_info_entry file create success\n", __func__);
	}
	if(!backlight_ic_reg_entry) {
		LOG_DBG("%s backlight_ic_reg_entry file create failed\n", __func__);
		remove_proc_entry(BKL_RW, NULL);
	} else {
		LOG_DBG("%s backlight_ic_reg_entry file create success\n", __func__);
	}

	return 0;
}

void ktd_node_deinit(void){
	if(backlight_ic_info_entry){
		remove_proc_entry(BKL_INFO, NULL);
		backlight_ic_info_entry = NULL;
	}

	if(backlight_ic_reg_entry){
		remove_proc_entry(BKL_RW, NULL);
		backlight_ic_reg_entry = NULL;
	}

	if(proc_backlight_dir){
		remove_proc_entry(PROC_BACKLIGHT_FOLDER, NULL);
		proc_backlight_dir = NULL;
	}
}

void select_backlight_ic_mode(bool is_panel)
{
	LOG_DBG("g_bl_id=%d  is_panel=%d\n", g_bl_id, is_panel);
	if(is_panel){
		if(g_bl_id == LM3697_CHIP)
			bl_mapping_table = bl_mapping_table_lm3697_a;
		else
			bl_mapping_table = bl_mapping_table_ktd3136_a;
	}
	else{
		if(g_bl_id == LM3697_CHIP)
			bl_mapping_table = bl_mapping_table_lm3697_b;
		else
			bl_mapping_table = bl_mapping_table_ktd3136_b;
	}
}
EXPORT_SYMBOL(select_backlight_ic_mode);

static int ktd3136_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ktd3136_chip *chip;
	dev_info(&client->dev, " ktd3136_bl probe start\n");
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "check_functionality failed.\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	g_blclient = client;

	ktd_parse_dt(&client->dev, chip);
	ktd3136_gpio_init(chip);
	gpio_set_value(chip->hwen_gpio, 1);
	i2c_set_clientdata(client, chip);

	ktd3136_read_reg(chip->client, 0x00, &g_bl_id);
	if(g_bl_id == LM3697_CHIP)              //lm3697_id = 1
		bl_mapping_table = bl_mapping_table_lm3697_a;
	else                                    //ktd_id = 0x18(24)
		bl_mapping_table = bl_mapping_table_ktd3136_a;
	LOG_DBG("g_bl_id = %d\n", g_bl_id);

	ktd_node_init();

	return 0;
}
static int ktd3136_remove(struct i2c_client *client)
{
	struct ktd3136_chip *chip = i2c_get_clientdata(client);

	gpio_set_value(chip->hwen_gpio, 0);
	gpio_free(chip->hwen_gpio);

	ktd_node_deinit();

	return 0;
}
static const struct i2c_device_id ktd3136_id[] = {
	{KTD_I2C_NAME, 0},
	{ },
};
static const struct of_device_id ktd3136_match_table[] = {
	{ .compatible = "ktd,ktd3136",},
	{ },
};
static struct i2c_driver ktd3136_driver = {
	.driver = {
		.name	= KTD_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ktd3136_match_table,
	},
	.probe = ktd3136_probe,
	.remove = ktd3136_remove,
	.id_table = ktd3136_id,
};

static int __init ktd3136_init(void)
{
	int ret = 0;

	pr_info("%s Entry\n", __func__);

	ret = i2c_add_driver(&ktd3136_driver);
	if (ret != 0)
		pr_err("KTD3136 driver init failed!");

	pr_info("%s Complete\n", __func__);

	return ret;
}

static void __exit ktd3136_exit(void) 
{
	i2c_del_driver(&ktd3136_driver);
}

module_init(ktd3136_init);
module_exit(ktd3136_exit);

MODULE_AUTHOR("KTD");
MODULE_DESCRIPTION("KTD3136 backlight IC Driver");
MODULE_LICENSE("GPL v2");
