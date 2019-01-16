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

#ifndef __FPC_IRQ_PM_H__
#define __FPC_IRQ_PM_H__

#include "fpc_irq_common.h"

extern int fpc_irq_pm_suspend(struct platform_device *plat_dev, pm_message_t state);

extern int fpc_irq_pm_resume(struct platform_device *plat_dev);

extern int fpc_irq_pm_init(fpc_irq_data_t *fpc_irq_data);

extern int fpc_irq_pm_destroy(fpc_irq_data_t *fpc_irq_data);

extern int fpc_irq_pm_notify_enable(fpc_irq_data_t *fpc_irq_data,
				int req_state);

extern int fpc_irq_pm_wakeup_req(fpc_irq_data_t *fpc_irq_data);
extern int fpc_irq_click_event(fpc_irq_data_t *fpc_irq_data, int val);
extern int fpc_irq_pm_notify_ack(fpc_irq_data_t *fpc_irq_data, int val);

extern int fpc_irq_setup_spi_clk_enable(fpc_irq_data_t *fpc_irq_data,
				int spi_clk_state);


#endif /* __FPC_IRQ_PM_H__ */



