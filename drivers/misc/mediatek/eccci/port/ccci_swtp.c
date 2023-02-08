// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <mt-plat/mtk_boot_common.h>
#include "ccci_debug.h"
#include "ccci_config.h"
#include "ccci_modem.h"
#include "ccci_swtp.h"
#include "ccci_fsm.h"

/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/input.h>
/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */

/* must keep ARRAY_SIZE(swtp_of_match) = ARRAY_SIZE(irq_name) */
const struct of_device_id swtp_of_match[] = {
	{ .compatible = SWTP_COMPATIBLE_DEVICE_ID, },
	{},
};

static const char irq_name[][16] = {
	"swtp0-eint",
	"",
};

#define SWTP_MAX_SUPPORT_MD 1
struct swtp_t swtp_data[SWTP_MAX_SUPPORT_MD];
static const char rf_name[] = "RF_cable";
#define MAX_RETRY_CNT 30

/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
static struct input_dev *swtp_input_dev;
#define SWTP_GPIO_STATUS "gpio_status"

int get_swtp_state = -1;
int get_swtp_gpio = -1;
static struct proc_dir_entry  *swtp_gpio_status;

static int swtp_gpio_proc_show(struct seq_file *file, void *data)
{
	get_swtp_state = gpio_get_value(get_swtp_gpio);
	seq_printf(file, "%d\n", get_swtp_state);

	return 0;
}

static int swtp_gpio_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, swtp_gpio_proc_show, inode->i_private);
}

static const struct file_operations swtp_gpio_status_ops = {
	.owner = THIS_MODULE,
	.open = swtp_gpio_proc_open,
	.read = seq_read,
};
/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */

static int swtp_send_tx_power(struct swtp_t *swtp)
{
	unsigned long flags;
	int power_mode, ret = 0;

	if (swtp == NULL) {
		CCCI_LEGACY_ERR_LOG(-1, SYS, "%s:swtp is null\n", __func__);
		return -1;
	}

	spin_lock_irqsave(&swtp->spinlock, flags);

	ret = exec_ccci_kern_func_by_md_id(swtp->md_id, ID_UPDATE_TX_POWER,
		(char *)&swtp->tx_power_mode, sizeof(swtp->tx_power_mode));
	power_mode = swtp->tx_power_mode;
	spin_unlock_irqrestore(&swtp->spinlock, flags);

	if (ret != 0)
		CCCI_LEGACY_ERR_LOG(swtp->md_id, SYS,
			"%s to MD%d,state=%d,ret=%d\n",
			__func__, swtp->md_id + 1, power_mode, ret);

	return ret;
}

static int swtp_switch_state(int irq, struct swtp_t *swtp)
{
	unsigned long flags;
	int i;

	if (swtp == NULL) {
		CCCI_LEGACY_ERR_LOG(-1, SYS, "%s:data is null\n", __func__);
		return -1;
	}

	spin_lock_irqsave(&swtp->spinlock, flags);
	for (i = 0; i < MAX_PIN_NUM; i++) {
		if (swtp->irq[i] == irq)
			break;
	}
	if (i == MAX_PIN_NUM) {
		spin_unlock_irqrestore(&swtp->spinlock, flags);
		CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s:can't find match irq\n", __func__);
		return -1;
	}

	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
	if (swtp->eint_type[i] == IRQ_TYPE_LEVEL_LOW) {
		irq_set_irq_type(swtp->irq[i], IRQ_TYPE_LEVEL_HIGH);
		swtp->eint_type[i] = IRQ_TYPE_LEVEL_HIGH;
		if (swtp_input_dev != NULL) {
			input_report_key(swtp_input_dev, KEY_TABLE0, 1);
			input_sync(swtp_input_dev);
			input_report_key(swtp_input_dev, KEY_TABLE0, 0);
			input_sync(swtp_input_dev);
			printk("[swtp]input keycode = %d", KEY_TABLE0);
		}
	} else {
		irq_set_irq_type(swtp->irq[i], IRQ_TYPE_LEVEL_LOW);
		swtp->eint_type[i] = IRQ_TYPE_LEVEL_LOW;
		if (swtp_input_dev != NULL) {
			input_report_key(swtp_input_dev, KEY_TABLE1, 1);
			input_sync(swtp_input_dev);
			input_report_key(swtp_input_dev, KEY_TABLE1, 0);
			input_sync(swtp_input_dev);
			printk("[swtp]input keycode = %d", KEY_TABLE1);
		}
	}
	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */

	if (swtp->gpio_state[i] == SWTP_EINT_PIN_PLUG_IN)
		swtp->gpio_state[i] = SWTP_EINT_PIN_PLUG_OUT;
	else
		swtp->gpio_state[i] = SWTP_EINT_PIN_PLUG_IN;

	swtp->tx_power_mode = SWTP_NO_TX_POWER;
	for (i = 0; i < MAX_PIN_NUM; i++) {
		/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
		if (swtp->gpio_state[i] == SWTP_EINT_PIN_PLUG_OUT) {
			swtp->tx_power_mode = SWTP_DO_TX_POWER;
			CCCI_LEGACY_ERR_LOG(-1, SYS,
				"%s:swtp send DO\n", __func__);
			break;
		}
		/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */
	}

	inject_pin_status_event(swtp->curr_mode, rf_name);
	spin_unlock_irqrestore(&swtp->spinlock, flags);

	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
	CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s:swtp send %d\n", __func__ , swtp->tx_power_mode);
	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */

	return swtp->tx_power_mode;
}

static void swtp_send_tx_power_state(struct swtp_t *swtp)
{
	int ret = 0;

	if (!swtp) {
		CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s:swtp is null\n", __func__);
		return;
	}

	if (swtp->md_id == 0) {
		ret = swtp_send_tx_power(swtp);
		if (ret < 0) {
			CCCI_LEGACY_ERR_LOG(swtp->md_id, SYS,
				"%s send tx power failed, ret=%d, schedule delayed work\n",
				__func__, ret);
			schedule_delayed_work(&swtp->delayed_work, 5 * HZ);
		}
	} else
		CCCI_LEGACY_ERR_LOG(swtp->md_id, SYS,
			"%s:md is no support\n", __func__);

}

static irqreturn_t swtp_irq_handler(int irq, void *data)
{
	struct swtp_t *swtp = (struct swtp_t *)data;
	int ret = 0;

	ret = swtp_switch_state(irq, swtp);
	if (ret < 0) {
		CCCI_LEGACY_ERR_LOG(swtp->md_id, SYS,
			"%s swtp_switch_state failed in irq, ret=%d\n",
			__func__, ret);
	} else
		swtp_send_tx_power_state(swtp);

	return IRQ_HANDLED;
}

static void swtp_tx_delayed_work(struct work_struct *work)
{
	struct swtp_t *swtp = container_of(to_delayed_work(work),
		struct swtp_t, delayed_work);
	int ret, retry_cnt = 0;

	while (retry_cnt < MAX_RETRY_CNT) {
		ret = swtp_send_tx_power(swtp);
		if (ret != 0) {
			msleep(2000);
			retry_cnt++;
		} else
			break;
	}
}

int swtp_md_tx_power_req_hdlr(int md_id, int data)
{
	struct swtp_t *swtp = NULL;

	if (md_id < 0 || md_id >= SWTP_MAX_SUPPORT_MD) {
		CCCI_LEGACY_ERR_LOG(md_id, SYS,
		"%s:md_id=%d not support\n",
		__func__, md_id);
		return -1;
	}

	swtp = &swtp_data[md_id];
	swtp_send_tx_power_state(swtp);

	return 0;
}

static void swtp_init_delayed_work(struct work_struct *work)
{
	struct swtp_t *swtp = container_of(to_delayed_work(work),
		struct swtp_t, init_delayed_work);
	int md_id;
	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
	int i, ret, ret1 = 0;
	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */
#ifdef CONFIG_MTK_EIC
	u32 ints[2] = { 0, 0 };
#else
	u32 ints[1] = { 0 };
#endif
	u32 ints1[2] = { 0, 0 };
	struct device_node *node = NULL;

	CCCI_NORMAL_LOG(-1, SYS, "%s start\n", __func__);
	CCCI_BOOTUP_LOG(-1, SYS, "%s start\n", __func__);

	md_id = swtp->md_id;

	if (md_id < 0 || md_id >= SWTP_MAX_SUPPORT_MD) {
		ret = -2;
		CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s: invalid md_id = %d\n", __func__, md_id);
		goto SWTP_INIT_END;
	}

	if (ARRAY_SIZE(swtp_of_match) != ARRAY_SIZE(irq_name) ||
		ARRAY_SIZE(swtp_of_match) > MAX_PIN_NUM + 1 ||
		ARRAY_SIZE(irq_name) > MAX_PIN_NUM + 1) {
		ret = -3;
		CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s: invalid array count = %lu(of_match), %lu(irq_name)\n",
			__func__, ARRAY_SIZE(swtp_of_match),
			ARRAY_SIZE(irq_name));
		goto SWTP_INIT_END;
	}

	for (i = 0; i < MAX_PIN_NUM; i++)
		swtp_data[md_id].gpio_state[i] = SWTP_EINT_PIN_PLUG_OUT;

	for (i = 0; i < MAX_PIN_NUM; i++) {
		node = of_find_matching_node(NULL, &swtp_of_match[i]);
		if (node) {
			ret = of_property_read_u32_array(node, "debounce",
				ints, ARRAY_SIZE(ints));
			if (ret) {
				CCCI_LEGACY_ERR_LOG(md_id, SYS,
					"%s:swtp%d get debounce fail\n",
					__func__, i);
				break;
			}

			ret = of_property_read_u32_array(node, "interrupts",
				ints1, ARRAY_SIZE(ints1));
			if (ret) {
				CCCI_LEGACY_ERR_LOG(md_id, SYS,
					"%s:swtp%d get interrupts fail\n",
					__func__, i);
				break;
			}
#ifdef CONFIG_MTK_EIC /* for chips before mt6739 */
			swtp_data[md_id].gpiopin[i] = ints[0];
			swtp_data[md_id].setdebounce[i] = ints[1];
#else /* for mt6739,and chips after mt6739 */
			swtp_data[md_id].setdebounce[i] = ints[0];
			swtp_data[md_id].gpiopin[i] =
				of_get_named_gpio(node, "deb-gpios", 0);
#endif
			gpio_set_debounce(swtp_data[md_id].gpiopin[i],
				swtp_data[md_id].setdebounce[i]);
			swtp_data[md_id].eint_type[i] = ints1[1];
			swtp_data[md_id].irq[i] = irq_of_parse_and_map(node, 0);

			/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
			ret = request_irq(swtp_data[md_id].irq[i],
				swtp_irq_handler, IRQF_TRIGGER_LOW,
				irq_name[i], &swtp_data[md_id]);
			/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */
			if (ret) {
				CCCI_LEGACY_ERR_LOG(md_id, SYS,
					"swtp%d-eint IRQ LINE NOT AVAILABLE\n",
					i);
				break;
			}
		} else {
			CCCI_LEGACY_ERR_LOG(md_id, SYS,
				"%s:can't find swtp%d compatible node\n",
				__func__, i);
			ret = -4;
		}
	}
	register_ccci_sys_call_back(md_id, MD_SW_MD1_TX_POWER_REQ,
		swtp_md_tx_power_req_hdlr);

	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 start */
	get_swtp_gpio = of_get_named_gpio(node, "swtp-gpio", 0);
	ret1 =  gpio_request(get_swtp_gpio, "swtp-gpio");
	if (ret1) {
		pr_err("gpio_request_one get_swtp_gpio(%d)=%d\n",get_swtp_gpio, ret1);
	}
	swtp_gpio_status = proc_create(SWTP_GPIO_STATUS, 0644, NULL, &swtp_gpio_status_ops);
	if (swtp_gpio_status == NULL) {
		printk("tpd, create_proc_entry swtp_gpio_status_ops failed\n");
	}

	swtp_input_dev = input_allocate_device();
	swtp_input_dev->name = "swtp";
	__set_bit(EV_KEY, swtp_input_dev->evbit);

	input_set_capability(swtp_input_dev, EV_KEY, KEY_TABLE0);
	input_set_capability(swtp_input_dev, EV_KEY, KEY_TABLE1);
	ret = input_register_device(swtp_input_dev);
	if (ret) {
		printk("[SWTP]input device register fail \n");
	}
	/* C3T code for HQ-223663 by liunianliang at 2022/07/19 end */

SWTP_INIT_END:
	CCCI_BOOTUP_LOG(md_id, SYS, "%s end: ret = %d\n", __func__, ret);
	CCCI_NORMAL_LOG(md_id, SYS, "%s end: ret = %d\n", __func__, ret);

	return;
}

int swtp_init(int md_id)
{
	/* parameter check */
	if (md_id < 0 || md_id >= SWTP_MAX_SUPPORT_MD) {
		CCCI_LEGACY_ERR_LOG(-1, SYS,
			"%s: invalid md_id = %d\n", __func__, md_id);
		return -1;
	}
	/* init woke setting */
	swtp_data[md_id].md_id = md_id;

	INIT_DELAYED_WORK(&swtp_data[md_id].init_delayed_work,
		swtp_init_delayed_work);
	/* tx work setting */
	INIT_DELAYED_WORK(&swtp_data[md_id].delayed_work,
		swtp_tx_delayed_work);
	swtp_data[md_id].tx_power_mode = SWTP_NO_TX_POWER;

	spin_lock_init(&swtp_data[md_id].spinlock);

	/* schedule init work */
	schedule_delayed_work(&swtp_data[md_id].init_delayed_work, HZ);

	CCCI_BOOTUP_LOG(md_id, SYS, "%s end, init_delayed_work scheduled\n",
		__func__);
	return 0;
}
