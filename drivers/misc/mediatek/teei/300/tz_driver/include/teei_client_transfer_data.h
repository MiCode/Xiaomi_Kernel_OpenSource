/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
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

#ifndef TEEI_CLIENT_TRANSFER_DATA_H
#define TEEI_CLIENT_TRANSFER_DATA_H

#include <tee_client_api.h>
#include <teei_secure_api.h>

int ut_pf_gp_initialize_context(struct TEEC_Context *context);
int ut_pf_gp_finalize_context(struct TEEC_Context *context);
int ut_pf_gp_transfer_data(struct TEEC_Context *context, struct TEEC_UUID *uuid,
		unsigned int command, void *buffer, unsigned long size);

#endif /* end of TEEI_CLIENT_TRANSFER_DATA_H */
