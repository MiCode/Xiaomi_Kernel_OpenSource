#ifndef __IRQ_GLOBAL_H_INCLUDED__
#define __IRQ_GLOBAL_H_INCLUDED__

#define IS_IRQ_VERSION_1
#define IS_IRQ_MAP_VERSION_1

/* We cannot include the file defining the "CSS_RECEIVER" property without side effects */
#ifndef HAS_NO_RX
#define CSS_RECEIVER testbench_isp_css_receiver
#endif
#include "hive_isp_css_irq_types_hrt.h"	/* enum	hrt_isp_css_irq */

/* The IRQ is not mapped uniformly on its related interfaces */
#define	IRQ_SW_CHANNEL_OFFSET	hrt_isp_css_irq_sw_0

typedef enum {
	IRQ_SW_CHANNEL0_ID = hrt_isp_css_irq_sw_0 - IRQ_SW_CHANNEL_OFFSET,
	IRQ_SW_CHANNEL1_ID = hrt_isp_css_irq_sw_1 - IRQ_SW_CHANNEL_OFFSET,
/* deprecated; Do not use this channel */
	IRQ_SW_CHANNEL2_ID = hrt_isp_css_irq_sw_2 - IRQ_SW_CHANNEL_OFFSET,
	N_IRQ_SW_CHANNEL_ID
} irq_sw_channel_id_t;

#endif /* __IRQ_GLOBAL_H_INCLUDED__ */
