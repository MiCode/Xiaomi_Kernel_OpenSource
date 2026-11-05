// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>

#include <dt-bindings/interrupt-controller/arm-gic.h>
#include <linux/gunyah/gh_rm_drv.h>

#include <asm/gunyah.h>

#define GIC_V3_SPI_MAX		1019

#define GH_RM_NO_IRQ_ALLOC	-1

#define IRQ_OFFSET 32

static DEFINE_IDR(gh_rm_free_virq_idr);

/**
 * gh_get_irq: Get a Linux IRQ from a Gunyah-compatible vIRQ
 * @virq: Gunyah-compatible vIRQ
 * @type: IRQ trigger type (IRQ_TYPE_EDGE_RISING)
 * @fw_handle: fw node handle
 *
 * Returns the mapped Linux IRQ# at Gunyah's IRQ domain (i.e. GIC SPI)
 */
int gh_get_irq(u32 virq, u32 type, struct fwnode_handle *fw_handle)
{
	struct irq_fwspec fwspec = {};
	int ret;

	ret = arch_gunyah_fill_irq_fwspec_params(virq, &fwspec);
	if (ret) {
		pr_err("Failed to translate interrupt: type: %d virq: %d: ret: %d\n",
		       type, virq, ret);
		return ret;
	}

	fwspec.fwnode = fw_handle;
	fwspec.param[2] = type;

	return irq_create_fwspec_mapping(&fwspec);
}
EXPORT_SYMBOL_GPL(gh_get_irq);

/**
 * gh_get_virq: Allocate a new IRQ if RM-VM hasn't already done already
 * @base_virq: The base virtual IRQ number.
 * @virq: The virtual IRQ number.
 *
 * Returns Gunyah compatible vIRQ to bind to.
 */
int gh_get_virq(int base_virq, int virq)
{
	int ret;

	/* Get the next free vIRQ.
	 * Subtract IRQ_OFFSET from the base virq to get the base SPI.
	 *
	 * Assoiate the address of the idr variable itself as a lookup
	 * ptr. This will help us to free the virq later.
	 */
	ret = virq = idr_alloc(&gh_rm_free_virq_idr,
				&gh_rm_free_virq_idr,
				base_virq - IRQ_OFFSET,
				GIC_V3_SPI_MAX, GFP_KERNEL);
	if (ret < 0)
		return ret;

	/* Add IRQ_OFFSET offset to make interrupt as hwirq */
	virq += IRQ_OFFSET;

	return virq;
}
EXPORT_SYMBOL_GPL(gh_get_virq);

/**
 * gh_put_virq: Deallocates a vIRQ.
 * @irq: The IRQ number.
 *
 * Returns 0 on success and EINVAL if no IRQ was found.
 */
int gh_put_virq(int virq)
{
	void *idr_ptr;
	int virq_num;

	virq_num = virq - IRQ_OFFSET;
	/* If the idr_find() returns a valid ptr, it means that the
	 * virq was allocated by the kernel itself and not by hyp.
	 * Release the IRQ and free the allocation if that's true.
	 */
	idr_ptr = idr_find(&gh_rm_free_virq_idr, virq_num);
	if (idr_ptr) {
		idr_remove(&gh_rm_free_virq_idr, virq_num);
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(gh_put_virq);

/**
 * gh_put_irq: Deallocate an Linux IRQ.
 * @irq: The IRQ number.
 *
 * Returns 0 on success and EINVAL if no IRQ was found.
 */
int gh_put_irq(int irq)
{
	struct irq_data *irq_data;
	unsigned long virq;

	if (irq <= 0)
		return -EINVAL;

	irq_data = irq_get_irq_data(irq);
	if (!irq_data)
		return -EINVAL;

	virq = irq_data->hwirq;

	irq_dispose_mapping(irq);

	return gh_put_virq(virq);
}
EXPORT_SYMBOL_GPL(gh_put_irq);
