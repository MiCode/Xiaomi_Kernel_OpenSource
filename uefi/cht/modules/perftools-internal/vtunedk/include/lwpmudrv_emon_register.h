/*COPYRIGHT**
 * -------------------------------------------------------------------------
 *               INTEL CORPORATION PROPRIETARY INFORMATION
 *  This software is supplied under the terms of the accompanying license
 *  agreement or nondisclosure agreement with Intel Corporation and may not
 *  be copied or disclosed except in accordance with the terms of that
 *  agreement.
 *        Copyright (C) 2009-2014 Intel Corporation.  All Rights Reserved.
 * -------------------------------------------------------------------------
**COPYRIGHT*/

#ifndef _LWPMUDRV_EMON_H_
#define _LWPMUDRV_EMON_H_

#include <vector>
using namespace std;

typedef struct EMON_REGISTER_INFO_S     EMON_REGISTER_INFO_NODE;
typedef vector<EMON_REGISTER_INFO_NODE> EMON_REGISTER_INFO;
//a vector (i.e. array) of registers to read or write in the vector order

struct EMON_REGISTER_INFO_S
{
    U32     mode;               //read (0) or write (1)
    U32     reg;                //register    pcb = CONTROL_Free_Memory(pcb);
    U64     value;              //value returned from a read or value to use during a write 
};

#define EMON_REGISTER_INFO_mode(er)     (er)->mode
#define EMON_REGISTER_INFO_reg(er)      (er)->reg
#define EMON_REGISTER_INFO_value(er)    (er)->value

#endif

