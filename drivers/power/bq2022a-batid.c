#define pr_fmt(fmt) "BQ2022A:%s: " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/of_batterydata.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include "bq2022a-batid.h"
#include <linux/hardware_info.h>


/* BQ2022A. */
#define	ROM_COMMAND		(0xcc)
#define	CTL_COMMAND		(0xc3)
#define	WADDR_LB			(0x00)
#define	WADDR_HB			(0x00)
#define	GPIO_HIGH 1
#define	GPIO_LOW 0

#define  GPIO_BATT_ID_PIN (902+2)

static struct bq2022a_platform_data *g_bq2022a;
static int bq2022a_bat_id = GPIO_BATT_ID_PIN;

unsigned char bq2022a_sdq_detect(void)
{
	unsigned int PresenceTimer = 50;
	static volatile unsigned char InputData;
	static volatile unsigned char GotPulse;


	gpio_direction_output(bq2022a_bat_id, GPIO_LOW);

	/* Reset time should be > 480 usec */
	udelay(800);
	gpio_direction_input(bq2022a_bat_id);
	udelay(60);


	while ((PresenceTimer > 0) && (GotPulse == 0)) {
		InputData = gpio_get_value(bq2022a_bat_id);
		if (InputData == 0) {
			GotPulse = 1;
		} else {
			GotPulse = 0;
			--PresenceTimer;
		}
	}
	udelay(200);

	return GotPulse;
}

unsigned char bq2022a_sdq_readbyte(int time)
{
	unsigned char data = 0x00;
	unsigned char mask, i;
	unsigned long flags;


	spin_lock_irqsave(&g_bq2022a->bqlock, flags);

	for (i = 0; i < 8; i++) {
		gpio_direction_output(bq2022a_bat_id, GPIO_LOW);

		udelay(7);
		gpio_direction_input(bq2022a_bat_id);
		udelay(time);
		mask = gpio_get_value(bq2022a_bat_id);
		udelay(65);
		mask <<= i;
		data = (data | mask);
	}

	udelay(200);

	spin_unlock_irqrestore(&g_bq2022a->bqlock, flags);

  return data;
}

void bq2022a_sdq_writebyte(u8 value)
{
	unsigned char mask = 1;
	int i;
	unsigned long flags;


	spin_lock_irqsave(&g_bq2022a->bqlock, flags);


	gpio_direction_output(bq2022a_bat_id, GPIO_HIGH);

	for (i = 0; i < 8; i++) {
		gpio_set_value(bq2022a_bat_id, GPIO_LOW);
		udelay(4);
		if (mask & value) {
			udelay(10);
			gpio_set_value(bq2022a_bat_id, GPIO_HIGH);
			udelay(100);
		} else {
			udelay(100);
			gpio_set_value(bq2022a_bat_id, GPIO_HIGH);
			udelay(10);
		}

		udelay(7);
		mask <<= 1;
	}


	spin_unlock_irqrestore(&g_bq2022a->bqlock, flags);

}

static int bat_module_id;
bool is_battery_feedback = false;

static const unsigned char con_bat_id[] = {
	0xed, 0x21, 0x4c, 0xe5,
	0xed, 0xa9, 0x4b, 0x2e,
};

static int bq2022a_read_bat_id(int delay_time, int pimc_pin)
{
	unsigned char bat_id = 0xff;
	unsigned char reset_id = 0;
	int i = 0;

	for (i = 0; i < 10; i++) {
		reset_id = bq2022a_sdq_detect();
		if (reset_id && pimc_pin) {
			is_battery_feedback = true;
			break;
		}
	}

	bq2022a_sdq_writebyte(ROM_COMMAND);
	bq2022a_sdq_writebyte(CTL_COMMAND);
	bq2022a_sdq_writebyte(WADDR_LB);
	bq2022a_sdq_writebyte(WADDR_HB);
	bat_id = bq2022a_sdq_readbyte(delay_time);

	for (i = 0; i < 34; i++) {
		bat_id = bq2022a_sdq_readbyte(delay_time);
		pr_debug("reset_id:%x is_battery_feedback:%d temp ID:%x!!\n", reset_id, is_battery_feedback, bat_id);
		if ((bat_id != con_bat_id[i]) && (i < 8)) {
			pr_err("read family code Error!!\n");
			break;
		}

		if ((i == 8) && (bat_id != 0x62)) {
			bat_module_id = (bat_id & 0x0f);
			break;
		} else if (i == 33) {
			if (bat_id == 0xdb)
				bat_module_id = 17;
			else if (bat_id == 0x3c)
				bat_module_id = 0x02;
			break;
		}
	}
	bat_id = bq2022a_sdq_readbyte(delay_time);
	gpio_direction_output(bq2022a_bat_id, GPIO_HIGH);

	if (((0 < bat_module_id) && (bat_module_id < 11)) || (bat_module_id == 17)) {
		pr_debug("get correct ID!!\n");
	} else {
		if (is_battery_feedback) {
			bat_module_id = 0;
			pr_debug("use common ID!!\n");
		} else {
			bat_module_id = 0xff;
			pr_debug("get wrong ID!!\n");
			return -ENODEV;
		}
	}

	pr_err("bat_module_id= %x\n", bat_module_id);
	return 0;
}
int bq2022a_get_bat_module_id(void)
{
	if (((0 <= bat_module_id) && (bat_module_id < 11)) || (bat_module_id == 17))
		return bat_module_id;
	else
		return 0xff;
}
EXPORT_SYMBOL_GPL(bq2022a_get_bat_module_id);
static const struct of_device_id of_bq2022as_match[] = {
	{.compatible = "bq2022a",},
	{},
};

static int bq2022a_probe(struct platform_device *pdev)
{
	struct bq2022a_platform_data *data;
	int rc = 0;
	int delay_time = 0;
	char bat_id_buf[64] = {0};

	pr_debug(" entry!\n");

	bq2022a_bat_id = of_get_named_gpio(pdev->dev.of_node, "qcom, bq2022a-id-gpio", 0);
	if (bq2022a_bat_id <= 0) {
		pr_err("can't get battery id pin from dts nod, so use BB gpio2\n");
		bq2022a_bat_id = GPIO_BATT_ID_PIN;
	}

	pr_debug("bat_id_pin_num:%d\n", bq2022a_bat_id);

	if (gpio_request(bq2022a_bat_id, "Batt_ID")) {
		pr_err("GPIO request failed!\n");
		rc = -EPROBE_DEFER;
		return rc;
	}

	data = (struct bq2022a_platform_data *) kzalloc(sizeof(struct bq2022a_platform_data), GFP_KERNEL);
	if (data == NULL) {
			pr_err("kzalloc Failed!\n");
			return -ENOMEM;
	}
	g_bq2022a = data;

	spin_lock_init(&g_bq2022a->bqlock);

	delay_time = 5;
	do {
		msleep(10);
		rc =bq2022a_read_bat_id(delay_time, 1);
		delay_time++;
	} while ((rc < 0) && (delay_time < 10));

	if (rc) {
		pr_debug("check batt id Error!\n");
		if (bq2022a_bat_id != GPIO_BATT_ID_PIN) {
			pr_err("use gpio2 to detect battery id!\n");
			bq2022a_bat_id = GPIO_BATT_ID_PIN;
			delay_time = 5;
			bq2022a_read_bat_id(delay_time, 0);
		}
	}

	switch (bq2022a_get_bat_module_id()) {
	case 0:
		sprintf(bat_id_buf, "%s", "0 Common");
		break;

	case 1:
		sprintf(bat_id_buf, "%s", "1 Sunwoda + Samsung");
		break;

	case 2:
		sprintf(bat_id_buf, "%s", "2 Guangyu + Guangyu");
		break;

	case 3:
		sprintf(bat_id_buf, "%s", "3 Sunwoda + Sony");
		break;

	case 4:
		sprintf(bat_id_buf, "%s", "4 Sunwoda + Samsung(customdown)");
		break;

	case 5:
		sprintf(bat_id_buf, "%s", "5 Desay + LG");
		break;

	case 6:
		sprintf(bat_id_buf, "%s", "6 Feimaotui + Sony");
		break;

	case 7:
		sprintf(bat_id_buf, "%s", "7 AAC");
		break;

	case 8:
		sprintf(bat_id_buf, "%s", "8 Guangyu(2200)");
		break;

	case 9:
		sprintf(bat_id_buf, "%s", "9 Desai(2200)");
		break;

	case 10:
		sprintf(bat_id_buf, "%s", "10 Sunwoda(2200)");
		break;

	case 17:
		sprintf(bat_id_buf, "%s", "17 Feimaotui + Samsung(MI2A)");
		break;

	case 0xff:
	default:
		sprintf(bat_id_buf, "%s", "error");
		break;
	}
	pr_debug("battery module:%s", bat_id_buf);

	pr_err("success!!\n");

	return rc;

}

static int bq2022a_remove(struct platform_device *pdev)
{
		gpio_free(bq2022a_bat_id);
		kfree(g_bq2022a);
		g_bq2022a = NULL;

	return 0;
}

static struct platform_driver bq2022a_driver = {
	.probe		= bq2022a_probe,
	.remove		= bq2022a_remove,
	.driver		= {
		.name	= "bq2022a",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(of_bq2022as_match),
	},
};

module_platform_driver(bq2022a_driver);

MODULE_DESCRIPTION("bq2022a-batid driver");
MODULE_LICENSE("GPL");
