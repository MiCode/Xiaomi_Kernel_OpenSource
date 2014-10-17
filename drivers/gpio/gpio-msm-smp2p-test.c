/* drivers/gpio/gpio-msm-smp2p-test.c
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/bitmap.h>
#include "../soc/qcom/smp2p_private.h"
#include "../soc/qcom/smp2p_test_common.h"

/* Interrupt callback data */
struct gpio_info {
	int gpio_base_id;
	int irq_base_id;

	bool initialized;
	struct completion cb_completion;
	int cb_count;
	DECLARE_BITMAP(triggered_irqs, SMP2P_BITS_PER_ENTRY);
};

/* GPIO Inbound/Outbound callback info */
struct gpio_inout {
	struct gpio_info in;
	struct gpio_info out;
};

static struct gpio_inout gpio_info[SMP2P_NUM_PROCS];

/**
 * Init/reset the callback data.
 *
 * @info: Pointer to callback data
 */
static void cb_data_reset(struct gpio_info *info)
{
	int n;

	if (!info)
		return;

	if (!info->initialized) {
		init_completion(&info->cb_completion);
		info->initialized = true;
	}
	info->cb_count = 0;

	for (n = 0; n < SMP2P_BITS_PER_ENTRY; ++n)
		clear_bit(n,  info->triggered_irqs);

	INIT_COMPLETION(info->cb_completion);
}

static int smp2p_gpio_test_probe(struct platform_device *pdev)
{
	int id;
	int cnt;
	struct device_node *node = pdev->dev.of_node;
	struct gpio_info *gpio_info_ptr = NULL;

	/*
	 * NOTE:  This does a string-lookup of the GPIO pin name and doesn't
	 * actually directly link to the SMP2P GPIO driver since all
	 * GPIO/Interrupt access must be through standard
	 * Linux GPIO / Interrupt APIs.
	 */
	if (strcmp("qcom,smp2pgpio_test_smp2p_1_in", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_MODEM_PROC].in;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_1_out", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_MODEM_PROC].out;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_2_in", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_AUDIO_PROC].in;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_2_out", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_AUDIO_PROC].out;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_3_in", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_SENSOR_PROC].in;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_3_out", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_SENSOR_PROC].out;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_4_in", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_WIRELESS_PROC].in;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_4_out", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_WIRELESS_PROC].out;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_7_in", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_REMOTE_MOCK_PROC].in;
	} else if (strcmp("qcom,smp2pgpio_test_smp2p_7_out", node->name) == 0) {
		gpio_info_ptr = &gpio_info[SMP2P_REMOTE_MOCK_PROC].out;
	} else {
		pr_err("%s: unable to match device type '%s'\n",
				__func__, node->name);
		return -ENODEV;
	}

	/* retrieve the GPIO and interrupt ID's */
	cnt = of_gpio_count(node);
	if (cnt && gpio_info_ptr) {
		/*
		 * Instead of looping through all 32-bits, we can just get the
		 * first pin to get the base IDs.  This saves on the verbosity
		 * of the device tree nodes as well.
		 */
		id = of_get_gpio(node, 0);
		if (id == -EPROBE_DEFER)
			return id;
		gpio_info_ptr->gpio_base_id = id;
		gpio_info_ptr->irq_base_id = gpio_to_irq(id);
	}
	return 0;
}

/*
 * NOTE:  Instead of match table and device driver, you may be able to just
 * call of_find_compatible_node() in your init function.
 */
static struct of_device_id msm_smp2p_match_table[] = {
	/* modem */
	{.compatible = "qcom,smp2pgpio_test_smp2p_1_out", },
	{.compatible = "qcom,smp2pgpio_test_smp2p_1_in", },

	/* audio (adsp) */
	{.compatible = "qcom,smp2pgpio_test_smp2p_2_out", },
	{.compatible = "qcom,smp2pgpio_test_smp2p_2_in", },

	/* sensor */
	{.compatible = "qcom,smp2pgpio_test_smp2p_3_out", },
	{.compatible = "qcom,smp2pgpio_test_smp2p_3_in", },

	/* wcnss */
	{.compatible = "qcom,smp2pgpio_test_smp2p_4_out", },
	{.compatible = "qcom,smp2pgpio_test_smp2p_4_in", },

	/* mock loopback */
	{.compatible = "qcom,smp2pgpio_test_smp2p_7_out", },
	{.compatible = "qcom,smp2pgpio_test_smp2p_7_in", },
	{},
};

static struct platform_driver smp2p_gpio_driver = {
	.probe = smp2p_gpio_test_probe,
	.driver = {
		.name = "smp2pgpio_test",
		.owner = THIS_MODULE,
		.of_match_table = msm_smp2p_match_table,
	},
};

/**
 * smp2p_ut_local_gpio_out - Verify outbound functionality.
 *
 * @s:   pointer to output file
 */
static void smp2p_ut_local_gpio_out(struct seq_file *s)
{
	int failed = 0;
	struct gpio_info *cb_info = &gpio_info[SMP2P_REMOTE_MOCK_PROC].out;
	int ret;
	int id;
	struct msm_smp2p_remote_mock *mock;

	seq_printf(s, "Running %s\n", __func__);
	do {
		/* initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		mock = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(mock, !=, NULL);

		mock->rx_interrupt_count = 0;
		memset(&mock->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		smp2p_init_header((struct smp2p_smem *)&mock->remote_item,
			SMP2P_REMOTE_MOCK_PROC, SMP2P_APPS_PROC,
			0, 1);
		strlcpy(mock->remote_item.entries[0].name, "smp2p",
			SMP2P_MAX_ENTRY_NAME);
		SMP2P_SET_ENT_VALID(
			mock->remote_item.header.valid_total_ent, 1);
		msm_smp2p_set_remote_mock_exists(true);
		mock->tx_interrupt();

		/* open GPIO entry */
		smp2p_gpio_open_test_entry("smp2p",
				SMP2P_REMOTE_MOCK_PROC, true);

		/* verify set/get functions */
		UT_ASSERT_INT(0, <, cb_info->gpio_base_id);
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			int pin = cb_info->gpio_base_id + id;

			mock->rx_interrupt_count = 0;
			gpio_set_value(pin, 1);
			UT_ASSERT_INT(1, ==, mock->rx_interrupt_count);
			UT_ASSERT_INT(1, ==, gpio_get_value(pin));

			gpio_set_value(pin, 0);
			UT_ASSERT_INT(2, ==, mock->rx_interrupt_count);
			UT_ASSERT_INT(0, ==, gpio_get_value(pin));
		}
		if (failed)
			break;

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}

	smp2p_gpio_open_test_entry("smp2p",
			SMP2P_REMOTE_MOCK_PROC, false);
}

/**
 * smp2p_gpio_irq - Interrupt handler for inbound entries.
 *
 * @irq:         Virtual IRQ being triggered
 * @data:        Cookie data (struct gpio_info * in this case)
 * @returns:     Number of bytes written
 */
static irqreturn_t smp2p_gpio_irq(int irq, void *data)
{
	struct gpio_info *gpio_ptr = (struct gpio_info *)data;
	int offset;

	if (!gpio_ptr) {
		pr_err("%s: gpio_ptr is NULL for irq %d\n", __func__, irq);
		return IRQ_HANDLED;
	}

	offset = irq - gpio_ptr->irq_base_id;
	if (offset >= 0 &&  offset < SMP2P_BITS_PER_ENTRY)
		set_bit(offset, gpio_ptr->triggered_irqs);
	else
		pr_err("%s: invalid irq offset base %d; irq %d\n",
			__func__, gpio_ptr->irq_base_id, irq);

	++gpio_ptr->cb_count;
	complete(&gpio_ptr->cb_completion);
	return IRQ_HANDLED;
}

/**
 * smp2p_ut_local_gpio_in - Verify inbound functionality.
 *
 * @s:   pointer to output file
 */
static void smp2p_ut_local_gpio_in(struct seq_file *s)
{
	int failed = 0;
	struct gpio_info *cb_info = &gpio_info[SMP2P_REMOTE_MOCK_PROC].in;
	int id;
	int ret;
	int virq;
	struct msm_smp2p_remote_mock *mock;

	seq_printf(s, "Running %s\n", __func__);

	cb_data_reset(cb_info);
	do {
		/* initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		mock = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(mock, !=, NULL);

		mock->rx_interrupt_count = 0;
		memset(&mock->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		smp2p_init_header((struct smp2p_smem *)&mock->remote_item,
			SMP2P_REMOTE_MOCK_PROC, SMP2P_APPS_PROC,
			0, 1);
		strlcpy(mock->remote_item.entries[0].name, "smp2p",
			SMP2P_MAX_ENTRY_NAME);
		SMP2P_SET_ENT_VALID(
			mock->remote_item.header.valid_total_ent, 1);
		msm_smp2p_set_remote_mock_exists(true);
		mock->tx_interrupt();

		smp2p_gpio_open_test_entry("smp2p",
				SMP2P_REMOTE_MOCK_PROC, true);

		/* verify set/get functions locally */
		UT_ASSERT_INT(0, <, cb_info->gpio_base_id);
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			int pin;
			int current_value;

			/* verify pin value cannot be set */
			pin = cb_info->gpio_base_id + id;
			current_value = gpio_get_value(pin);

			gpio_set_value(pin, 0);
			UT_ASSERT_INT(current_value, ==, gpio_get_value(pin));
			gpio_set_value(pin, 1);
			UT_ASSERT_INT(current_value, ==, gpio_get_value(pin));

			/* verify no interrupts */
			UT_ASSERT_INT(0, ==, cb_info->cb_count);
		}
		if (failed)
			break;

		/* register for interrupts */
		UT_ASSERT_INT(0, <, cb_info->irq_base_id);
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;
			UT_ASSERT_PTR(NULL, !=, irq_to_desc(virq));
			ret = request_irq(virq,
					smp2p_gpio_irq,	IRQF_TRIGGER_RISING,
					"smp2p_test", cb_info);
			UT_ASSERT_INT(0, ==, ret);
		}
		if (failed)
			break;

		/* verify both rising and falling edge interrupts */
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;
			irq_set_irq_type(virq, IRQ_TYPE_EDGE_BOTH);
			cb_data_reset(cb_info);

			/* verify rising-edge interrupt */
			mock->remote_item.entries[0].entry = 1 << id;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 1);
			UT_ASSERT_INT(0, <,
				test_bit(id, cb_info->triggered_irqs));
			test_bit(id, cb_info->triggered_irqs);

			/* verify falling-edge interrupt */
			mock->remote_item.entries[0].entry = 0;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 2);
			UT_ASSERT_INT(0, <,
					test_bit(id, cb_info->triggered_irqs));
		}
		if (failed)
			break;

		/* verify rising-edge interrupts */
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;
			irq_set_irq_type(virq, IRQ_TYPE_EDGE_RISING);
			cb_data_reset(cb_info);

			/* verify only rising-edge interrupt is triggered */
			mock->remote_item.entries[0].entry = 1 << id;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 1);
			UT_ASSERT_INT(0, <,
				test_bit(id, cb_info->triggered_irqs));
			test_bit(id, cb_info->triggered_irqs);

			mock->remote_item.entries[0].entry = 0;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 1);
			UT_ASSERT_INT(0, <,
				test_bit(id, cb_info->triggered_irqs));
		}
		if (failed)
			break;

		/* verify falling-edge interrupts */
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;
			irq_set_irq_type(virq, IRQ_TYPE_EDGE_FALLING);
			cb_data_reset(cb_info);

			/* verify only rising-edge interrupt is triggered */
			mock->remote_item.entries[0].entry = 1 << id;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 0);
			UT_ASSERT_INT(0, ==,
				test_bit(id, cb_info->triggered_irqs));

			mock->remote_item.entries[0].entry = 0;
			mock->tx_interrupt();
			UT_ASSERT_INT(cb_info->cb_count, ==, 1);
			UT_ASSERT_INT(0, <,
				test_bit(id, cb_info->triggered_irqs));
		}
		if (failed)
			break;

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}

	/* unregister for interrupts */
	if (cb_info->irq_base_id) {
		for (id = 0; id < SMP2P_BITS_PER_ENTRY; ++id)
			free_irq(cb_info->irq_base_id + id, cb_info);
	}

	smp2p_gpio_open_test_entry("smp2p",
			SMP2P_REMOTE_MOCK_PROC, false);
}

/**
 * smp2p_ut_local_gpio_in_update_open - Verify combined open/update.
 *
 * @s:   pointer to output file
 *
 * If the remote side updates the SMP2P bits and sends before negotiation is
 * complete, then the UPDATE event will have to be delayed until negotiation is
 * complete.  This should result in both the OPEN and UPDATE events coming in
 * right after each other and the behavior should be transparent to the clients
 * of SMP2P GPIO.
 */
static void smp2p_ut_local_gpio_in_update_open(struct seq_file *s)
{
	int failed = 0;
	struct gpio_info *cb_info = &gpio_info[SMP2P_REMOTE_MOCK_PROC].in;
	int id;
	int ret;
	int virq;
	struct msm_smp2p_remote_mock *mock;

	seq_printf(s, "Running %s\n", __func__);

	cb_data_reset(cb_info);
	do {
		/* initialize mock edge */
		ret = smp2p_reset_mock_edge();
		UT_ASSERT_INT(ret, ==, 0);

		mock = msm_smp2p_get_remote_mock();
		UT_ASSERT_PTR(mock, !=, NULL);

		mock->rx_interrupt_count = 0;
		memset(&mock->remote_item, 0,
			sizeof(struct smp2p_smem_item));
		smp2p_init_header((struct smp2p_smem *)&mock->remote_item,
			SMP2P_REMOTE_MOCK_PROC, SMP2P_APPS_PROC,
			0, 1);
		strlcpy(mock->remote_item.entries[0].name, "smp2p",
			SMP2P_MAX_ENTRY_NAME);
		SMP2P_SET_ENT_VALID(
			mock->remote_item.header.valid_total_ent, 1);

		/* register for interrupts */
		smp2p_gpio_open_test_entry("smp2p",
				SMP2P_REMOTE_MOCK_PROC, true);

		UT_ASSERT_INT(0, <, cb_info->irq_base_id);
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;
			UT_ASSERT_PTR(NULL, !=, irq_to_desc(virq));
			ret = request_irq(virq,
					smp2p_gpio_irq,	IRQ_TYPE_EDGE_BOTH,
					"smp2p_test", cb_info);
			UT_ASSERT_INT(0, ==, ret);
		}
		if (failed)
			break;

		/* update the state value and complete negotiation */
		mock->remote_item.entries[0].entry = 0xDEADDEAD;
		msm_smp2p_set_remote_mock_exists(true);
		mock->tx_interrupt();

		/* verify delayed state updates were processed */
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			virq = cb_info->irq_base_id + id;

			UT_ASSERT_INT(cb_info->cb_count, >, 0);
			if (0x1 & (0xDEADDEAD >> id)) {
				/* rising edge should have been triggered */
				if (!test_bit(id, cb_info->triggered_irqs)) {
					seq_printf(s, "%s:%d bit %d clear, ",
						__func__, __LINE__, id);
					seq_puts(s, "expected set\n");
					failed = 1;
					break;
				}
			} else {
				/* edge should not have been triggered */
				if (test_bit(id, cb_info->triggered_irqs)) {
					seq_printf(s, "%s:%d bit %d set, ",
						__func__, __LINE__, id);
					seq_puts(s, "expected clear\n");
					failed = 1;
					break;
				}
			}
		}
		if (failed)
			break;

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", __func__);
		seq_puts(s, "\tFailed\n");
	}

	/* unregister for interrupts */
	if (cb_info->irq_base_id) {
		for (id = 0; id < SMP2P_BITS_PER_ENTRY; ++id)
			free_irq(cb_info->irq_base_id + id, cb_info);
	}

	smp2p_gpio_open_test_entry("smp2p",
			SMP2P_REMOTE_MOCK_PROC, false);
}

/**
 * smp2p_gpio_write_bits - writes value to each GPIO pin specified in mask.
 *
 * @gpio: gpio test structure
 * @mask: 1 = write gpio_value to this GPIO pin
 * @gpio_value: value to write to GPIO pin
 */
static void smp2p_gpio_write_bits(struct gpio_info *gpio, uint32_t mask,
	int gpio_value)
{
	int n;

	for (n = 0; n < SMP2P_BITS_PER_ENTRY; ++n) {
		if (mask & 0x1)
			gpio_set_value(gpio->gpio_base_id + n, gpio_value);
		mask >>= 1;
	}
}

static void smp2p_gpio_set_bits(struct gpio_info *gpio, uint32_t mask)
{
	smp2p_gpio_write_bits(gpio, mask, 1);
}

static void smp2p_gpio_clr_bits(struct gpio_info *gpio, uint32_t mask)
{
	smp2p_gpio_write_bits(gpio, mask, 0);
}

/**
 * smp2p_gpio_get_value - reads entire 32-bits of GPIO
 *
 * @gpio: gpio structure
 * @returns: 32 bit value of GPIO pins
 */
static uint32_t smp2p_gpio_get_value(struct gpio_info *gpio)
{
	int n;
	uint32_t value = 0;

	for (n = 0; n < SMP2P_BITS_PER_ENTRY; ++n) {
		if (gpio_get_value(gpio->gpio_base_id + n))
			value |= 1 << n;
	}
	return value;
}

/**
 * smp2p_ut_remote_inout_core - Verify inbound/outbound functionality.
 *
 * @s:   pointer to output file
 * @remote_pid:  Remote processor to test
 * @name:        Name of the test for reporting
 *
 * This test verifies inbound/outbound functionality for the remote processor.
 */
static void smp2p_ut_remote_inout_core(struct seq_file *s, int remote_pid,
		const char *name)
{
	int failed = 0;
	uint32_t request;
	uint32_t response;
	struct gpio_info *cb_in;
	struct gpio_info *cb_out;
	int id;
	int ret;

	seq_printf(s, "Running %s for '%s' remote pid %d\n",
		   __func__, smp2p_pid_to_name(remote_pid), remote_pid);

	cb_in = &gpio_info[remote_pid].in;
	cb_out = &gpio_info[remote_pid].out;
	cb_data_reset(cb_in);
	cb_data_reset(cb_out);
	do {
		/* open test entries */
		msm_smp2p_deinit_rmt_lpb_proc(remote_pid);
		smp2p_gpio_open_test_entry("smp2p", remote_pid, true);

		/* register for interrupts */
		UT_ASSERT_INT(0, <, cb_in->gpio_base_id);
		UT_ASSERT_INT(0, <, cb_in->irq_base_id);
		for (id = 0; id < SMP2P_BITS_PER_ENTRY && !failed; ++id) {
			int virq = cb_in->irq_base_id + id;
			UT_ASSERT_PTR(NULL, !=, irq_to_desc(virq));
			ret = request_irq(virq,
				smp2p_gpio_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"smp2p_test", cb_in);
			UT_ASSERT_INT(0, ==, ret);
		}
		if (failed)
			break;

		/* write echo of data value 0 */
		UT_ASSERT_INT(0, <, cb_out->gpio_base_id);
		request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(request, 1);
		SMP2P_SET_RMT_CMD(request, SMP2P_LB_CMD_ECHO);
		SMP2P_SET_RMT_DATA(request, 0x0);

		smp2p_gpio_set_bits(cb_out, SMP2P_RMT_IGNORE_MASK);
		smp2p_gpio_clr_bits(cb_out, ~SMP2P_RMT_IGNORE_MASK);
		smp2p_gpio_set_bits(cb_out, request);

		UT_ASSERT_INT(cb_in->cb_count, ==, 0);
		smp2p_gpio_clr_bits(cb_out, SMP2P_RMT_IGNORE_MASK);

		/* verify response */
		do {
			/* wait for up to 32 changes */
			if (wait_for_completion_timeout(
					&cb_in->cb_completion, HZ / 2) == 0)
				break;
			INIT_COMPLETION(cb_in->cb_completion);
		} while (cb_in->cb_count < 32);
		UT_ASSERT_INT(cb_in->cb_count, >, 0);
		response = smp2p_gpio_get_value(cb_in);
		SMP2P_SET_RMT_CMD_TYPE(request, 0);
		UT_ASSERT_HEX(request, ==, response);

		/* write echo of data value of all 1's */
		request = 0x0;
		SMP2P_SET_RMT_CMD_TYPE(request, 1);
		SMP2P_SET_RMT_CMD(request, SMP2P_LB_CMD_ECHO);
		SMP2P_SET_RMT_DATA(request, ~0);

		smp2p_gpio_set_bits(cb_out, SMP2P_RMT_IGNORE_MASK);
		cb_data_reset(cb_in);
		smp2p_gpio_clr_bits(cb_out, ~SMP2P_RMT_IGNORE_MASK);
		smp2p_gpio_set_bits(cb_out, request);

		UT_ASSERT_INT(cb_in->cb_count, ==, 0);
		smp2p_gpio_clr_bits(cb_out, SMP2P_RMT_IGNORE_MASK);

		/* verify response including 24 interrupts */
		do {
			UT_ASSERT_INT(
				(int)wait_for_completion_timeout(
					&cb_in->cb_completion, HZ / 2),
			   >, 0);
			INIT_COMPLETION(cb_in->cb_completion);
		} while (cb_in->cb_count < 24);
		response = smp2p_gpio_get_value(cb_in);
		SMP2P_SET_RMT_CMD_TYPE(request, 0);
		UT_ASSERT_HEX(request, ==, response);
		UT_ASSERT_INT(24, ==, cb_in->cb_count);

		seq_puts(s, "\tOK\n");
	} while (0);

	if (failed) {
		pr_err("%s: Failed\n", name);
		seq_puts(s, "\tFailed\n");
	}

	/* unregister for interrupts */
	if (cb_in->irq_base_id) {
		for (id = 0; id < SMP2P_BITS_PER_ENTRY; ++id)
			free_irq(cb_in->irq_base_id + id, cb_in);
	}

	smp2p_gpio_open_test_entry("smp2p",	remote_pid, false);
	msm_smp2p_init_rmt_lpb_proc(remote_pid);
}

/**
 * smp2p_ut_remote_inout - Verify inbound/outbound functionality for all.
 *
 * @s:   pointer to output file
 *
 * This test verifies inbound and outbound functionality for all
 * configured remote processor.
 */
static void smp2p_ut_remote_inout(struct seq_file *s)
{
	struct smp2p_interrupt_config *int_cfg;
	int pid;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg) {
		seq_puts(s, "Remote processor config unavailable\n");
		return;
	}

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid) {
		if (!int_cfg[pid].is_configured)
			continue;

		smp2p_ut_remote_inout_core(s, pid, __func__);
	}
}

static int __init smp2p_debugfs_init(void)
{
	/* register GPIO pins */
	(void)platform_driver_register(&smp2p_gpio_driver);

	/*
	 * Add Unit Test entries.
	 *
	 * The idea with unit tests is that you can run all of them
	 * from ADB shell by doing:
	 *  adb shell
	 *  cat ut*
	 *
	 * And if particular tests fail, you can then repeatedly run the
	 * failing tests as you debug and resolve the failing test.
	 */
	smp2p_debug_create("ut_local_gpio_out", smp2p_ut_local_gpio_out);
	smp2p_debug_create("ut_local_gpio_in", smp2p_ut_local_gpio_in);
	smp2p_debug_create("ut_local_gpio_in_update_open",
		smp2p_ut_local_gpio_in_update_open);
	smp2p_debug_create("ut_remote_gpio_inout", smp2p_ut_remote_inout);
	return 0;
}
late_initcall(smp2p_debugfs_init);
