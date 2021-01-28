/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef TEEI_CLIENT_TRANSFER_DATA_H
#define TEEI_CLIENT_TRANSFER_DATA_H

#include <tee_client_api.h>
#include <teei_secure_api.h>

int ut_pf_gp_initialize_context(struct TEEC_Context *context);
int ut_pf_gp_finalize_context(struct TEEC_Context *context);
int ut_pf_gp_transfer_data(struct TEEC_Context *context, struct TEEC_UUID *uuid,
		unsigned int command, void *buffer, unsigned long size);
int ut_pf_gp_transfer_user_data(struct TEEC_Context *context,
		struct TEEC_UUID *uuid,
		unsigned int command, void *buffer, unsigned long size);

#endif /* end of TEEI_CLIENT_TRANSFER_DATA_H */
