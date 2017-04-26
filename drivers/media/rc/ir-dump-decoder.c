/* ir-dump-decoder.c - dump IR pulse/space signal
 *
 * Copyright (C) 2012 by Xiang Xiao <xiaoxiang@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include "rc-core-priv.h"

/**
 * ir_dump_decode() - Dump one pulse or space signal
 * @dev:	the struct rc_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 */
static int ir_dump_decode(struct rc_dev *dev, struct ir_raw_event ev)
{
	const char *n = dev_name(&dev->dev) ?: "unknown";

	if (ev.reset)
		pr_info("%s: reset\n", n);
	else if (ev.timeout)
		pr_info("%s: timeout\t%uus\n", n, TO_US(ev.duration));
	else if (ev.carrier_report)
		pr_info("%s: carrier\t%uHz(%u%%)\n", n, ev.carrier, ev.duty_cycle);
	else
		pr_info("%s: %s\t%uus\n", n, TO_STR(ev.pulse), TO_US(ev.duration));

	return 0;
}

static int ir_dump_register(struct rc_dev *dev)
{
	pr_info("%s: register(%s %s %s %s)\n",
		dev_name(&dev->dev) ?: "unknown",
		dev->driver_name ?: "unknown",
		dev->map_name ?: "unknown",
		dev->input_name ?: "unknown",
		dev->input_phys ?: "unknown");
	return 0;
}

static int ir_dump_unregister(struct rc_dev *dev)
{
	pr_info("%s: unregister(%s %s %s %s)\n",
		dev_name(&dev->dev) ?: "unknown",
		dev->driver_name ?: "unknown",
		dev->map_name ?: "unknown",
		dev->input_name ?: "unknown",
		dev->input_phys ?: "unknown");
	return 0;
}

static struct ir_raw_handler dump_handler = {
	.protocols	= RC_TYPE_OTHER,
	.decode		= ir_dump_decode,
	.raw_register	= ir_dump_register,
	.raw_unregister	= ir_dump_unregister,
};

static int __init ir_dump_decode_init(void)
{
	ir_raw_handler_register(&dump_handler);
	pr_info("ir dump protocol handler initialized\n");
	return 0;
}

static void __exit ir_dump_decode_exit(void)
{
	ir_raw_handler_unregister(&dump_handler);
}

module_init(ir_dump_decode_init);
module_exit(ir_dump_decode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("ir dump protocol decoder");
