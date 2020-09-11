/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef TEE_SMC_XFER_H
#define TEE_SMC_XFER_H

struct smc_param;

void smc_xfer(struct smc_param *p);
void __call_tee(struct smc_param *p);

int tee_init_smc_xfer(void);
void tee_exit_smc_xfer(void);

#endif
