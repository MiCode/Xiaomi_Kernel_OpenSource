/*
 * Copyright (c) 2013, Sony Mobile Communications AB.
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
#ifndef __PINCTRL_MSM_H__
#define __PINCTRL_MSM_H__

struct pinctrl_pin_desc;

/**
 * struct msm_function - a pinmux function
 * @name:    Name of the pinmux function.
 * @groups:  List of pingroups for this function.
 * @ngroups: Number of entries in @groups.
 */
struct msm_function {
	const char *name;
	const char * const *groups;
	unsigned ngroups;
};

/**
 * struct msm_pingroup - Qualcomm pingroup definition
 * @name:                 Name of the pingroup.
 * @pins:	          A list of pins assigned to this pingroup.
 * @npins:	          Number of entries in @pins.
 * @funcs:                A list of pinmux functions that can be selected for
 *                        this group. The index of the selected function is used
 *                        for programming the function selector.
 *                        Entries should be indices into the groups list of the
 *                        struct msm_pinctrl_soc_data.
 * @ctl_reg:              Offset of the register holding control bits for this group.
 * @io_reg:               Offset of the register holding input/output bits for this group.
 * @intr_cfg_reg:         Offset of the register holding interrupt configuration bits.
 * @intr_status_reg:      Offset of the register holding the status bits for this group.
 * @intr_target_reg:      Offset of the register specifying routing of the interrupts
 *                        from this group.
 * @dir_conn_reg:         Offset of the register specifying direct connect
 *                        setup of this group.
 * @mux_bit:              Offset in @ctl_reg for the pinmux function selection.
 * @pull_bit:             Offset in @ctl_reg for the bias configuration.
 * @drv_bit:              Offset in @ctl_reg for the drive strength configuration.
 * @oe_bit:               Offset in @ctl_reg for controlling output enable.
 * @in_bit:               Offset in @io_reg for the input bit value.
 * @out_bit:              Offset in @io_reg for the output bit value.
 * @intr_enable_bit:      Offset in @intr_cfg_reg for enabling the interrupt for this group.
 * @intr_status_bit:      Offset in @intr_status_reg for reading and acking the interrupt
 *                        status.
 * @intr_target_bit:      Offset in @intr_target_reg for configuring the interrupt routing.
 * @intr_target_kpss_val: Value in @intr_target_bit for specifying that the interrupt from
 *                        this gpio should get routed to the KPSS processor.
 * @intr_raw_status_bit:  Offset in @intr_cfg_reg for the raw status bit.
 * @intr_polarity_bit:    Offset in @intr_cfg_reg for specifying polarity of the interrupt.
 * @intr_detection_bit:   Offset in @intr_cfg_reg for specifying interrupt type.
 * @intr_detection_width: Number of bits used for specifying interrupt type,
 *                        Should be 2 for SoCs that can detect both edges in hardware,
 *                        otherwise 1.
 */
struct msm_pingroup {
	const char *name;
	const unsigned *pins;
	unsigned npins;

	unsigned *funcs;
	unsigned nfuncs;

#ifdef CONFIG_FRAGMENTED_GPIO_ADDRESS_SPACE
	u32 tile_base;
#endif
	u32 ctl_reg;
	u32 io_reg;
	u32 intr_cfg_reg;
	u32 intr_status_reg;
	u32 intr_target_reg;
	u32 dir_conn_reg;

	unsigned mux_bit:5;

	unsigned pull_bit:5;
	unsigned drv_bit:5;

	unsigned egpio_enable:5;
	unsigned egpio_present:5;
	unsigned oe_bit:5;
	unsigned in_bit:5;
	unsigned out_bit:5;

	unsigned intr_enable_bit:5;
	unsigned intr_status_bit:5;
	unsigned intr_ack_high:1;

	unsigned intr_target_bit:5;
	unsigned intr_target_kpss_val:5;
	unsigned intr_raw_status_bit:5;
	unsigned intr_polarity_bit:5;
	unsigned intr_detection_bit:5;
	unsigned intr_detection_width:5;
	unsigned dir_conn_en_bit:8;
};

/**
 * struct msm_gpio_mux_input - Map GPIO to Mux pin
 * @mux::	The mux pin to which the GPIO is connected to
 * @gpio:	GPIO pin number
 * @init:	Setup PDC connection at probe
 */
struct msm_gpio_mux_input {
	unsigned int mux;
	unsigned int gpio;
	bool init;
};

/**
 * struct msm_pdc_mux_output - GPIO mux pin to PDC port
 * @mux:	GPIO mux pin number
 * @hwirq:	The PDC port (hwirq) that GPIO is connected to
 */
struct msm_pdc_mux_output {
	unsigned int mux;
	irq_hw_number_t hwirq;
};

/**
 * struct msm_dir_conn - Direct GPIO connect configuration
 * @gpio:	GPIO pin number
 * @hwirq:	The GIC interrupt that the pin is connected to
 * @tlmm_dc:	indicates if the GPIO is routed to GIC directly
 */
struct msm_dir_conn {
	int gpio;
	irq_hw_number_t hwirq;
	bool tlmm_dc;
};

/**
 * struct msm_pinctrl_soc_data - Qualcomm pin controller driver configuration
 * @pins:       An array describing all pins the pin controller affects.
 * @npins:      The number of entries in @pins.
 * @functions:  An array describing all mux functions the SoC supports.
 * @nfunctions: The number of entries in @functions.
 * @groups:     An array describing all pin groups the pin SoC supports.
 * @ngroups:    The numbmer of entries in @groups.
 * @ngpio:      The number of pingroups the driver should expose as GPIOs.
 * @pull_no_keeper: The SoC does not support keeper bias.
 * @dir_conn:   An array describing all the pins directly connected to GIC.
 * @ndirconns:  The number of pins directly connected to GIC
 * @dir_conn_irq_base:  Direct connect interrupt base register for kpss.
 * @gpio_mux_in:	Map of GPIO pin to the hwirq.
 * @n_gpioc_mux_in:	The number of entries in @pdc_mux_in.
 * @pdc_mux_out:	Map of GPIO mux to PDC port.
 * @n_pdc_mux_out:	The number of entries in @pdc_mux_out.
 * @n_pdc_offset:	The offset for the PDC mux pins
 */
struct msm_pinctrl_soc_data {
	const struct pinctrl_pin_desc *pins;
	unsigned npins;
	const struct msm_function *functions;
	unsigned nfunctions;
	const struct msm_pingroup *groups;
	unsigned ngroups;
	unsigned ngpios;
	bool pull_no_keeper;
	const struct msm_dir_conn *dir_conn;
	unsigned int n_dir_conns;
	unsigned int dir_conn_irq_base;
	struct msm_pdc_mux_output *pdc_mux_out;
	unsigned int n_pdc_mux_out;
	struct msm_gpio_mux_input *gpio_mux_in;
	unsigned int n_gpio_mux_in;
	unsigned int n_pdc_mux_offset;
#ifdef CONFIG_FRAGMENTED_GPIO_ADDRESS_SPACE
	const u32 *tile_start;
	const u32 *tile_offsets;
	unsigned int n_tile;
	void __iomem **pin_base;
	const u32 *tile_end;
#endif
};

int msm_pinctrl_probe(struct platform_device *pdev,
		      const struct msm_pinctrl_soc_data *soc_data);
int msm_pinctrl_remove(struct platform_device *pdev);

#endif
