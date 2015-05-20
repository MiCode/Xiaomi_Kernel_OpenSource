/*COPYRIGHT**
 * -------------------------------------------------------------------------
 *               INTEL CORPORATION PROPRIETARY INFORMATION
 *  This software is supplied under the terms of the accompanying license
 *  agreement or nondisclosure agreement with Intel Corporation and may not
 *  be copied or disclosed except in accordance with the terms of that
 *  agreement.
 *        Copyright (C) 2009-2014 Intel Corporation. All Rights Reserved.
 * -------------------------------------------------------------------------
**COPYRIGHT*/

#ifndef _VT_CPLUSPLUS_TYPES_INC_
#define _VT_CPLUSPLUS_TYPES_INC_

#include <string>
using namespace std;

#if defined (UNICODE)
typedef wstring STLSTRING;
#else
typedef string  STLSTRING;
#endif

#endif

