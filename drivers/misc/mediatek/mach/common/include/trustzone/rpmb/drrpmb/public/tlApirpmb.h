/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All rights reserved
 *
 * The present software is the confidential and proprietary information of
 * TRUSTONIC LIMITED. You shall not disclose the present software and shall
 * use it only in accordance with the terms of the license agreement you
 * entered into with TRUSTONIC LIMITED. This software may be subject to
 * export or import laws in certain countries.
 */
 
#ifndef __TLAPIROT13_H__
#define __TLAPIROT13_H__

#include "TlApi/TlApiCommon.h"
#include "TlApi/TlApiError.h"

/* Marshaled function parameters.
 * structs and union of marshaling parameters via TlApi.
 *
 * @note The structs can NEVER be packed !
 * @note The structs can NOT used via sizeof(..) !
 */
 

/** Encode cleartext with rot13.
 *
 * @param encodeData Pointer to the tlApiEncodeRot13_t structure 
 * @return TLAPI_OK if data has successfully been encrypted.
 */

//_TLAPI_EXTERN_C tlApiResult_t tlApiAddRpmb(tlApiRpmb_ptr RpmbData);

#endif // __TLAPIROT13_H__

