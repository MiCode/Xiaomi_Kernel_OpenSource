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

#ifndef TEE_FP_H
#define TEE_FP_H

int tee_spi_cfg_padsel(uint32_t padsel);

int tee_spi_transfer(void *conf, uint32_t conf_size, void *inbuf, void *outbuf,
		uint32_t size);

int tee_spi_transfer_disable(void);

#endif
