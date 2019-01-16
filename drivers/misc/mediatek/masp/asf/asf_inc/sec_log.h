#ifndef LOGGING_H
#define LOGGING_H

/**************************************************************************
*  DEBUG CONTROL
**************************************************************************/
#include <mach/sec_osal.h>  
#include "sec_osal_light.h"
#define NEED_TO_PRINT(flag)         ((flag) == true)
#define SMSG(debug_level, ...) do \
                                    { \
                                        if(NEED_TO_PRINT(debug_level)) \
                                            printk(__VA_ARGS__); \
                                    } while(0);

/**************************************************************************
 *  EXTERNAL VARIABLE
 **************************************************************************/
extern bool                         bMsg;

#endif /* LOGGING_H */

