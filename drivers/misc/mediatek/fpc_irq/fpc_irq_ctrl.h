/* Fingerprint Cards, Hybrid Touch sensor driver
 *
 * Copyright (c) 2014,2015 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 *
 * Software license : "Dual BSD/GPL"
 * see <linux/module.h> and ./Documentation
 * for  details.
 *
*/

#ifndef __FPC_IRQ_CTRL_H
#define __FPC_IRQ_CTRL_H

#include "fpc_irq_common.h"

extern int fpc_irq_ctrl_init(fpc_irq_data_t *fpc_irq_data,
				fpc_irq_pdata_t *pdata);

extern int fpc_irq_ctrl_destroy(fpc_irq_data_t *fpc_irq_data);

extern int fpc_irq_ctrl_hw_reset(fpc_irq_data_t *fpc_irq_data);

#endif /* __FPC_IRQ_CTRL_H */


