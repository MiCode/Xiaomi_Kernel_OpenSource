/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _GPS_DL_ISR_H
#define _GPS_DL_ISR_H

#include "gps_dl_config.h"
#include "gps_each_link.h"

enum gps_dl_irq_index_enum {
	GPS_DL_IRQ_LINK0_DATA,
	GPS_DL_IRQ_LINK0_NODATA,
	GPS_DL_IRQ_LINK0_MCUB,
	GPS_DL_IRQ_LINK1_DATA,
	GPS_DL_IRQ_LINK1_NODATA,
	GPS_DL_IRQ_LINK1_MCUB,
	GPS_DL_IRQ_DMA,
	GPS_DL_IRQ_NUM,
};

enum gps_dl_irq_trigger_type {
	GPS_DL_IRQ_TRIG_LEVEL_HIGH,
	GPS_DL_IRQ_TRIG_EDGE_RISING,
	GPS_DL_IRQ_TRIG_NUM,
};

struct gps_each_irq_cfg {
	int sens_type;
	enum gps_dl_irq_index_enum index;
	void *isr;
	const char *name;
	enum gps_dl_irq_trigger_type trig_type;
};

struct gps_each_irq {
	struct gps_each_irq_cfg cfg;
	bool register_done;
	int reg_irq_id;
};

#define IRQ_IDX_IS_VALID(irq_idx) \
	(((irq_idx) >= 0) && ((irq_idx) < GPS_DL_IRQ_NUM))

#define ASSERT_IRQ_IDX(irq_idx, ret) \
	GDL_ASSERT(IRQ_IDX_IS_VALID(irq_idx), ret, "invalid irq index: %d", irq_idx)

struct gps_each_irq *gps_dl_irq_get(enum gps_dl_irq_index_enum irq_idx);


enum gps_dl_each_link_irq_type {
	GPS_DL_IRQ_TYPE_HAS_DATA,
	GPS_DL_IRQ_TYPE_HAS_NODATA,
	GPS_DL_IRQ_TYPE_MCUB,
	GPS_DL_IRQ_TYPE_NUM,
};
#define IRQ_TYPE_IS_VALID(irq_type) \
	(((irq_type) >= 0) && ((irq_type) < GPS_DL_IRQ_TYPE_NUM))

#define ASSERT_IRQ_TYPE(irq_type, ret) \
	GDL_ASSERT(IRQ_TYPE_IS_VALID(irq_type), ret, "invalid irq type: %d", irq_type)


enum gps_dl_irq_ctrl_from {
	GPS_DL_IRQ_CTRL_FROM_ISR,
	GPS_DL_IRQ_CTRL_FROM_THREAD,
	GPS_DL_IRQ_CTRL_FROM_MAX,
};

#if GPS_DL_USE_THREADED_IRQ
/*
 * #define GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR GPS_DL_IRQ_CTRL_FROM_THREAD
 * threaded_irq still can't call disable_irq
 */
#define GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR GPS_DL_IRQ_CTRL_FROM_ISR
#else
#define GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR GPS_DL_IRQ_CTRL_FROM_ISR
#endif

#if GPS_DL_HAS_CTRLD
#define GPS_DL_IRQ_CTRL_FROM_HAL GPS_DL_IRQ_CTRL_FROM_THREAD
#else
#define GPS_DL_IRQ_CTRL_FROM_HAL GPS_DL_IRQ_CTRL_POSSIBLE_FROM_ISR
#endif


void gps_dl_irq_mask(int irq_id, enum gps_dl_irq_ctrl_from from);
void gps_dl_irq_unmask(int irq_id, enum gps_dl_irq_ctrl_from from);

#if GPS_DL_ON_LINUX
void gps_dl_irq_set_id(enum gps_dl_irq_index_enum irq_idx, int irq_id);
#endif

int gps_dl_irq_index_to_id(enum gps_dl_irq_index_enum irq_idx);
int gps_dl_irq_type_to_id(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type);
int gps_dl_irq_type_to_hwirq(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type);

void gps_dl_irq_each_link_control(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, bool mask_it, enum gps_dl_irq_ctrl_from from);
void gps_dl_irq_each_link_mask(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, enum gps_dl_irq_ctrl_from from);
void gps_dl_irq_each_link_unmask(enum gps_dl_link_id_enum link_id,
	enum gps_dl_each_link_irq_type irq_type, enum gps_dl_irq_ctrl_from from);

int gps_dl_irq_init(void);
int gps_dl_irq_deinit(void);
void gps_dl_irq_mask_dma_intr(enum gps_dl_irq_ctrl_from from);
void gps_dl_irq_unmask_dma_intr(enum gps_dl_irq_ctrl_from from);

void gps_dl_isr_usrt_has_data(enum gps_dl_link_id_enum link_id);
void gps_dl_isr_usrt_has_nodata(enum gps_dl_link_id_enum link_id);
void gps_dl_isr_a2d_tx_dma_done(enum gps_dl_link_id_enum link_id);
void gps_dl_isr_d2a_rx_dma_done(enum gps_dl_link_id_enum link_id);
void gps_dl_isr_mcub(enum gps_dl_link_id_enum link_id);
bool gps_dl_hal_mcub_flag_handler(enum gps_dl_link_id_enum link_id);

#if GPS_DL_ON_CTP
void gps_dl_isr_dl0_has_data(void);
void gps_dl_isr_dl0_has_nodata(void);
void gps_dl_isr_dl0_mcub(void);
void gps_dl_isr_dl1_has_data(void);
void gps_dl_isr_dl1_has_nodata(void);
void gps_dl_isr_dl1_mcub(void);
#endif

void gps_dl_isr_dma_done(void);

#endif /* _GPS_DL_ISR_H */

