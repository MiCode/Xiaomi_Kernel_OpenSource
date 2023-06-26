/*******************************************************************************
 * @file     fusb30X.c
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#include "fusb30X.h"
#include "platform.h"

FSC_BOOL DeviceWrite(FSC_U8 i2cAddr, FSC_U8 regAddr,
                     FSC_U8 length, FSC_U8* data)
{
    return platform_i2c_write(i2cAddr, FUSB300AddrLength, length,
                              length, FUSB300IncSize, regAddr, data);
}

FSC_BOOL DeviceRead(FSC_U8 i2cAddr, FSC_U8 regAddr,
                    FSC_U8 length, FSC_U8* data)
{
    return platform_i2c_read(i2cAddr, FUSB300AddrLength, length,
                             length, FUSB300IncSize, regAddr, data);
}
