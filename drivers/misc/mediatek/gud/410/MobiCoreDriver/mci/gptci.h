/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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

#ifndef _GP_TCI_H_
#define _GP_TCI_H_

struct tee_value {
	u32 a;
	u32 b;
};

struct _teec_memory_reference_internal {
	u32 sva;
	u32 len;
	u32 output_size;
};

union _teec_parameter_internal {
	struct tee_value		       value;
	struct _teec_memory_reference_internal memref;
};

enum _teec_tci_type {
	_TA_OPERATION_OPEN_SESSION   = 1,
	_TA_OPERATION_INVOKE_COMMAND = 2,
	_TA_OPERATION_CLOSE_SESSION  = 3,
};

struct _teec_operation_internal {
	enum _teec_tci_type	       type;
	u32			       command_id;
	u32			       param_types;
	union _teec_parameter_internal params[4];
	bool			       is_cancelled;
	u8			       rfu_padding[3];
};

struct _teec_tci {
	char				header[8];
	struct teec_uuid		destination;
	struct _teec_operation_internal operation;
	u32				ready;
	u32				return_origin;
	u32				return_status;
};

/**
 * Termination codes
 */
#define TA_EXIT_CODE_PANIC	  300
#define TA_EXIT_CODE_TCI	  301
#define TA_EXIT_CODE_PARAMS	  302
#define TA_EXIT_CODE_FINISHED	  303
#define TA_EXIT_CODE_SESSIONSTATE 304
#define TA_EXIT_CODE_CREATEFAILED 305

#endif /* _GP_TCI_H_ */
