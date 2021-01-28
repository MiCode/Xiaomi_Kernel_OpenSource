/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TEE_FP_H
#define TEE_FP_H

int tee_spi_cfg_padsel(uint32_t padsel);

int tee_spi_transfer(void *conf, uint32_t conf_size, void *inbuf, void *outbuf,
		uint32_t size);

int tee_spi_transfer_disable(void);

#endif
