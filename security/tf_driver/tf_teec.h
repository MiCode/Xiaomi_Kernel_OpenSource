/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __TF_TEEC_H__
#define __TF_TEEC_H__

#ifdef CONFIG_TF_TEEC

#include "tf_defs.h"
#include "tee_client_api.h"

TEEC_Result TEEC_encode_error(int err);
int TEEC_decode_error(TEEC_Result ret);

#endif /* defined(CONFIG_TF_TEEC) */

#endif  /* !defined(__TF_TEEC_H__) */
