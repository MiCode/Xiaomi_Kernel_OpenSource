/*
 * Support for Medfield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _HIVE_ISP_CSS_IRQ_TYPES_HRT_H_
#define _HIVE_ISP_CSS_IRQ_TYPES_HRT_H_

/* these are the indices of each interrupt in the
 * interrupt controller's registers. these can be used
 * as the irq_id argument to the hrt functions irq_controller.h.
 */
enum hrt_isp_css_irq {
  hrt_isp_css_irq_gpio_0,
  hrt_isp_css_irq_gpio_1,
  hrt_isp_css_irq_gpio_2,
  hrt_isp_css_irq_gpio_3,
  hrt_isp_css_irq_gpio_4,
  hrt_isp_css_irq_gpio_5,
  hrt_isp_css_irq_gpio_6,
  hrt_isp_css_irq_gpio_7,
  hrt_isp_css_irq_gpio_8,
  hrt_isp_css_irq_gpio_9,
  hrt_isp_css_irq_sp,
  hrt_isp_css_irq_isp,
  hrt_isp_css_irq_mipi,
  hrt_isp_css_irq_ift_prim,
  hrt_isp_css_irq_ift_prim_b,
  hrt_isp_css_irq_ift_sec,
  hrt_isp_css_irq_ift_mem_cpy,
  hrt_isp_css_irq_mipi_fifo_full,
  hrt_isp_css_irq_mipi_sof,
  hrt_isp_css_irq_mipi_eof,
  hrt_isp_css_irq_mipi_sol,
  hrt_isp_css_irq_mipi_eol,
  hrt_isp_css_irq_syncgen_sof,
  hrt_isp_css_irq_syncgen_eof,
  hrt_isp_css_irq_syncgen_sol,
  hrt_isp_css_irq_syncgen_eol,
#ifdef CSS_RECEIVER
  hrt_isp_css_irq_css_gen_short_0,
  hrt_isp_css_irq_css_gen_short_1,
  hrt_isp_css_irq_sideband_changed,
#endif
  /* The ASIC system has only 3 SW interrupts, so the FPGA system is limited
   * by this to 3 as well. */
  hrt_isp_css_irq_sw_0,
  hrt_isp_css_irq_sw_1,
  hrt_isp_css_irq_sw_2,
  /* this must (obviously) be the last on in the enum */
  hrt_isp_css_irq_num_irqs
};

enum hrt_isp_css_irq_status {
  hrt_isp_css_irq_status_error,
  hrt_isp_css_irq_status_more_irqs,
  hrt_isp_css_irq_status_success
};

#endif /* _HIVE_ISP_CSS_IRQ_TYPES_HRT_H_ */
