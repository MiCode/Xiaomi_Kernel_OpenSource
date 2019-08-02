/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TEE_SMC_XFER_H
#define TEE_SMC_XFER_H

struct smc_param;

void smc_xfer(struct smc_param *p);
void __call_tee(struct smc_param *p);

int tee_init_smc_xfer(void);
void tee_exit_smc_xfer(void);

#endif
