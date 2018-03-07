/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spmi.h>

/* PMIC Arbiter configuration registers */
#define VPMIC_ARB_VERSION		0x0000

/* Virtual PMIC Arbiter registers offset*/
#define VPMIC_ARB_CMD			0x00
#define VPMIC_ARB_STATUS		0x04
#define VPMIC_ARB_DATA0			0x08
#define VPMIC_ARB_DATA1			0x10

/* Channel Status fields */
enum pmic_arb_chnl_status {
	PMIC_ARB_STATUS_DONE	= BIT(0),
	PMIC_ARB_STATUS_FAILURE	= BIT(1),
	PMIC_ARB_STATUS_DENIED	= BIT(2),
	PMIC_ARB_STATUS_DROPPED	= BIT(3),
};

/* Command register fields */
#define PMIC_ARB_CMD_MAX_BYTE_COUNT	8

/* Command Opcodes */
enum pmic_arb_cmd_op_code {
	PMIC_ARB_OP_EXT_WRITEL = 0,
	PMIC_ARB_OP_EXT_READL = 1,
	PMIC_ARB_OP_EXT_WRITE = 2,
	PMIC_ARB_OP_RESET = 3,
	PMIC_ARB_OP_SLEEP = 4,
	PMIC_ARB_OP_SHUTDOWN = 5,
	PMIC_ARB_OP_WAKEUP = 6,
	PMIC_ARB_OP_AUTHENTICATE = 7,
	PMIC_ARB_OP_MSTR_READ = 8,
	PMIC_ARB_OP_MSTR_WRITE = 9,
	PMIC_ARB_OP_EXT_READ = 13,
	PMIC_ARB_OP_WRITE = 14,
	PMIC_ARB_OP_READ = 15,
	PMIC_ARB_OP_ZERO_WRITE = 16,
};

/*
 * PMIC arbiter version 5 uses different register offsets for read/write vs
 * observer channels.
 */
enum pmic_arb_channel {
	PMIC_ARB_CHANNEL_RW,
	PMIC_ARB_CHANNEL_OBS,
};

/* Maximum number of support PMIC peripherals */
#define PMIC_ARB_MAX_PERIPHS		512
#define PMIC_ARB_TIMEOUT_US		100
#define PMIC_ARB_MAX_TRANS_BYTES	(8)

struct vspmi_backend_driver_ver_ops;

/**
 * vspmi_pmic_arb - Virtual SPMI PMIC Arbiter object
 *
 * @lock:		lock to synchronize accesses.
 * @spmic:		SPMI controller object
 * @ver_ops:		backend version dependent operations.
 */
struct vspmi_pmic_arb {
	void __iomem		*core;
	resource_size_t		core_size;
	raw_spinlock_t		lock;
	struct spmi_controller	*spmic;
	const struct vspmi_backend_driver_ver_ops *ver_ops;
};
static struct vspmi_pmic_arb *the_pa;

/**
 * pmic_arb_ver: version dependent functionality.
 *
 * @ver_str:		version string.
 * @fmt_cmd:		formats a GENI/SPMI command.
 */
struct vspmi_backend_driver_ver_ops {
	const char *ver_str;
	u32 (*fmt_cmd)(u8 opc, u8 sid, u16 addr, u8 bc);
};

/**
 * vspmi_pa_read_data: reads vspmi backend's register and copy 1..4 bytes to buf
 * @bc:		byte count -1. range: 0..3
 * @reg:	register's address
 * @buf:	output parameter, length must be bc + 1
 */
static void
vspmi_pa_read_data(struct vspmi_pmic_arb *pa, u8 *buf, u32 reg, u8 bc)
{
	u32 data = __raw_readl(pa->core + reg);

	memcpy(buf, &data, (bc & 3) + 1);
}

/**
 * vspmi_pa_write_data: write 1..4 bytes from buf to vspmi backend's register
 * @bc:		byte-count -1. range: 0..3.
 * @reg:	register's address.
 * @buf:	buffer to write. length must be bc + 1.
 */
static void
vspmi_pa_write_data(struct vspmi_pmic_arb *pa, const u8 *buf, u32 reg, u8 bc)
{
	u32 data = 0;

	memcpy(&data, buf, (bc & 3) + 1);
	writel_relaxed(data, pa->core + reg);
}

static int vspmi_pmic_arb_wait_for_done(struct spmi_controller *ctrl,
				  void __iomem *base, u8 sid, u16 addr,
				  enum pmic_arb_channel ch_type)
{
	u32 status = 0;
	u32 timeout = PMIC_ARB_TIMEOUT_US;
	u32 offset;

	offset = VPMIC_ARB_STATUS;

	while (timeout--) {
		status = readl_relaxed(base + offset);

		if (status & PMIC_ARB_STATUS_DONE) {
			if (status & PMIC_ARB_STATUS_DENIED) {
				dev_err(&ctrl->dev,
					"%s: transaction denied (0x%x)\n",
					__func__, status);
				return -EPERM;
			}

			if (status & PMIC_ARB_STATUS_FAILURE) {
				dev_err(&ctrl->dev,
					"%s: transaction failed (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			if (status & PMIC_ARB_STATUS_DROPPED) {
				dev_err(&ctrl->dev,
					"%s: transaction dropped (0x%x)\n",
					__func__, status);
				return -EIO;
			}

			return 0;
		}
		udelay(1);
	}

	dev_err(&ctrl->dev,
		"%s: timeout, status 0x%x\n",
		__func__, status);
	return -ETIMEDOUT;
}

static int vspmi_pmic_arb_read_cmd(struct spmi_controller *ctrl, u8 opc, u8 sid,
			     u16 addr, u8 *buf, size_t len)
{
	struct vspmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x60 && opc <= 0x7F)
		opc = PMIC_ARB_OP_READ;
	else if (opc >= 0x20 && opc <= 0x2F)
		opc = PMIC_ARB_OP_EXT_READ;
	else if (opc >= 0x38 && opc <= 0x3F)
		opc = PMIC_ARB_OP_EXT_READL;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	raw_spin_lock_irqsave(&pa->lock, flags);
	writel_relaxed(cmd, pa->core + VPMIC_ARB_CMD);
	rc = vspmi_pmic_arb_wait_for_done(ctrl, pa->core, sid, addr,
				    PMIC_ARB_CHANNEL_OBS);
	if (rc)
		goto done;

	vspmi_pa_read_data(pa, buf, VPMIC_ARB_DATA0, min_t(u8, bc, 3));

	if (bc > 3)
		vspmi_pa_read_data(pa, buf + 4, VPMIC_ARB_DATA1, bc - 4);

done:
	raw_spin_unlock_irqrestore(&pa->lock, flags);
	return rc;
}

static int vspmi_pmic_arb_write_cmd(struct spmi_controller *ctrl, u8 opc,
			    u8 sid, u16 addr, const u8 *buf, size_t len)
{
	struct vspmi_pmic_arb *pa = spmi_controller_get_drvdata(ctrl);
	unsigned long flags;
	u8 bc = len - 1;
	u32 cmd;
	int rc;

	if (bc >= PMIC_ARB_MAX_TRANS_BYTES) {
		dev_err(&ctrl->dev,
			"pmic-arb supports 1..%d bytes per trans, but:%zu requested",
			PMIC_ARB_MAX_TRANS_BYTES, len);
		return  -EINVAL;
	}

	/* Check the opcode */
	if (opc >= 0x40 && opc <= 0x5F)
		opc = PMIC_ARB_OP_WRITE;
	else if (opc <= 0x0F)
		opc = PMIC_ARB_OP_EXT_WRITE;
	else if (opc >= 0x30 && opc <= 0x37)
		opc = PMIC_ARB_OP_EXT_WRITEL;
	else if (opc >= 0x80)
		opc = PMIC_ARB_OP_ZERO_WRITE;
	else
		return -EINVAL;

	cmd = pa->ver_ops->fmt_cmd(opc, sid, addr, bc);

	/* Write data to FIFOs */
	raw_spin_lock_irqsave(&pa->lock, flags);
	vspmi_pa_write_data(pa, buf, VPMIC_ARB_DATA0, min_t(u8, bc, 3));
	if (bc > 3)
		vspmi_pa_write_data(pa, buf + 4, VPMIC_ARB_DATA1, bc - 4);

	/* Start the transaction */
	writel_relaxed(cmd, pa->core + VPMIC_ARB_CMD);
	rc = vspmi_pmic_arb_wait_for_done(ctrl, pa->core, sid, addr,
				    PMIC_ARB_CHANNEL_RW);
	raw_spin_unlock_irqrestore(&pa->lock, flags);

	return rc;
}

static u32 vspmi_pmic_arb_fmt_cmd_v1(u8 opc, u8 sid, u16 addr, u8 bc)
{
	return (opc << 27) | ((sid & 0xf) << 20) | (addr << 4) |
		   ((bc & 0x7) + 1);
}

static const struct vspmi_backend_driver_ver_ops pmic_arb_v1 = {
	.ver_str		= "v1",
	.fmt_cmd		= vspmi_pmic_arb_fmt_cmd_v1,
};

static int vspmi_pmic_arb_probe(struct platform_device *pdev)
{
	struct vspmi_pmic_arb *pa;
	struct spmi_controller *ctrl;
	struct resource *res;
	u32 backend_ver;
	int err;

	ctrl = spmi_controller_alloc(&pdev->dev, sizeof(*pa));
	if (!ctrl)
		return -ENOMEM;

	pa = spmi_controller_get_drvdata(ctrl);
	pa->spmic = ctrl;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "core");
	if (!res) {
		dev_err(&pdev->dev, "vdev resource not specified\n");
		err = -EINVAL;
		goto err_put_ctrl;
	}

	pa->core = devm_ioremap_resource(&ctrl->dev, res);
	if (IS_ERR(pa->core)) {
		err = PTR_ERR(pa->core);
		goto err_put_ctrl;
	}
	pa->core_size = resource_size(res);

	backend_ver = VPMIC_ARB_VERSION;

	if (backend_ver == VPMIC_ARB_VERSION)
		pa->ver_ops = &pmic_arb_v1;

	dev_info(&ctrl->dev, "PMIC arbiter version %s (0x%x)\n",
		 pa->ver_ops->ver_str, backend_ver);

	platform_set_drvdata(pdev, ctrl);
	raw_spin_lock_init(&pa->lock);

	ctrl->read_cmd = vspmi_pmic_arb_read_cmd;
	ctrl->write_cmd = vspmi_pmic_arb_write_cmd;

	err = spmi_controller_add(ctrl);
	if (err)
		goto err_put_ctrl;

	the_pa = pa;
	return 0;

err_put_ctrl:
	spmi_controller_put(ctrl);
	return err;
}

static int vspmi_pmic_arb_remove(struct platform_device *pdev)
{
	struct spmi_controller *ctrl = platform_get_drvdata(pdev);

	spmi_controller_remove(ctrl);
	the_pa = NULL;
	spmi_controller_put(ctrl);
	return 0;
}

static const struct of_device_id vspmi_pmic_arb_match_table[] = {
	{ .compatible = "qcom,virtspmi-pmic-arb", },
	{},
};
MODULE_DEVICE_TABLE(of, vspmi_pmic_arb_match_table);

static struct platform_driver vspmi_pmic_arb_driver = {
	.probe		= vspmi_pmic_arb_probe,
	.remove		= vspmi_pmic_arb_remove,
	.driver		= {
		.name	= "virtspmi_pmic_arb",
		.of_match_table = vspmi_pmic_arb_match_table,
	},
};

static int __init vspmi_pmic_arb_init(void)
{
	return platform_driver_register(&vspmi_pmic_arb_driver);
}
arch_initcall(vspmi_pmic_arb_init);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:virtspmi_pmic_arb");
