#ifndef __IRQ_LOCAL_H_INCLUDED__
#define __IRQ_LOCAL_H_INCLUDED__

#include "irq_global.h"

#include <hive_isp_css_irq_types_hrt.h>
#include <irq_controller_defs.h>

typedef struct virq_info_s					virq_info_t;
typedef struct irq_controller_state_s		irq_controller_state_t;

typedef enum {
	virq_gpio_pin_0     = hrt_isp_css_irq_gpio_0,
	virq_gpio_pin_1     = hrt_isp_css_irq_gpio_1,
	virq_gpio_pin_2     = hrt_isp_css_irq_gpio_2,
	virq_gpio_pin_3     = hrt_isp_css_irq_gpio_3,
	virq_gpio_pin_4     = hrt_isp_css_irq_gpio_4,
	virq_gpio_pin_5     = hrt_isp_css_irq_gpio_5,
	virq_gpio_pin_6     = hrt_isp_css_irq_gpio_6,
	virq_gpio_pin_7     = hrt_isp_css_irq_gpio_7,
	virq_gpio_pin_8     = hrt_isp_css_irq_gpio_8,
	virq_gpio_pin_9     = hrt_isp_css_irq_gpio_9,
	virq_sp             = hrt_isp_css_irq_sp,
	virq_isp            = hrt_isp_css_irq_isp,
	virq_isys           = hrt_isp_css_irq_mipi,
	virq_ifmt0_id       = hrt_isp_css_irq_ift_prim,
	virq_ifmt1_id       = hrt_isp_css_irq_ift_prim_b,
	virq_ifmt2_id       = hrt_isp_css_irq_ift_sec,
	virq_ifmt3_id       = hrt_isp_css_irq_ift_mem_cpy,
	virq_isys_fifo_full = hrt_isp_css_irq_mipi_fifo_full,
	virq_isys_sof       = hrt_isp_css_irq_mipi_sof,
	virq_isys_eof       = hrt_isp_css_irq_mipi_eof,
	virq_isys_sol       = hrt_isp_css_irq_mipi_sol,
	virq_isys_eol       = hrt_isp_css_irq_mipi_eol,
	virq_isel_sof       = hrt_isp_css_irq_syncgen_sof,
	virq_isel_eof       = hrt_isp_css_irq_syncgen_eof,
	virq_isel_sol       = hrt_isp_css_irq_syncgen_sol,
	virq_isel_eol       = hrt_isp_css_irq_syncgen_eol,
	virq_gen_short_0    = hrt_isp_css_irq_css_gen_short_0,
	virq_gen_short_1    = hrt_isp_css_irq_css_gen_short_1,
	virq_ifmt_sideband_changed = hrt_isp_css_irq_sideband_changed,
	virq_sw_pin_0       = hrt_isp_css_irq_sw_0,
	virq_sw_pin_1       = hrt_isp_css_irq_sw_1,
	virq_sw_pin_2       = hrt_isp_css_irq_sw_2,
	N_virq_id           = hrt_isp_css_irq_num_irqs
} virq_id_t;

struct virq_info_s {
/*
 * If we want to auto-size to the minimum required
 * else guess what is the maximum over all systems
 *
	hrt_data		irq_status_reg[N_IRQ_ID];
 */
	hrt_data		irq_status_reg[4];
};

/*
 * This data structure is private to the host
 */
struct irq_controller_state_s {
	unsigned int	irq_edge;
	unsigned int	irq_mask;
	unsigned int	irq_status;
	unsigned int	irq_enable;
	unsigned int	irq_level_not_pulse;
};

#endif /* __IRQ_LOCAL_H_INCLUDED__ */
