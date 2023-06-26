/*******************************************************************************
 * @file     vendor_info.c
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
#include "vendor_info.h"
#include "PD_Types.h"

void VIF_InitializeSrcCaps(doDataObject_t *src_caps)
{
    FSC_U8 i;
    doDataObject_t gSrc_caps[7] =
    {
        /* macro expects index starting at 1 and type */
        CREATE_SUPPLY_PDO_FIRST(1),
        CREATE_SUPPLY_PDO(2, Src_PDO_Supply_Type2),
        CREATE_SUPPLY_PDO(3, Src_PDO_Supply_Type3),
        CREATE_SUPPLY_PDO(4, Src_PDO_Supply_Type4),
        CREATE_SUPPLY_PDO(5, Src_PDO_Supply_Type5),
        CREATE_SUPPLY_PDO(6, Src_PDO_Supply_Type6),
        CREATE_SUPPLY_PDO(7, Src_PDO_Supply_Type7),
    };

    for(i = 0; i < 7; ++i) {src_caps[i].object = gSrc_caps[i].object;}
}
void VIF_InitializeSnkCaps(doDataObject_t *snk_caps)
{
    FSC_U8 i;
    doDataObject_t gSnk_caps[7] =
    {
        /* macro expects index start at 1 and type */
        CREATE_SINK_PDO(1, Snk_PDO_Supply_Type1),
        CREATE_SINK_PDO(2, Snk_PDO_Supply_Type2),
        CREATE_SINK_PDO(3, Snk_PDO_Supply_Type3),
        CREATE_SINK_PDO(4, Snk_PDO_Supply_Type4),
        CREATE_SINK_PDO(5, Snk_PDO_Supply_Type5),
        CREATE_SINK_PDO(6, Snk_PDO_Supply_Type6),
        CREATE_SINK_PDO(7, Snk_PDO_Supply_Type7),
    };

    gSnk_caps[1].object &= 0xC00FFFFF;
    gSnk_caps[2].object &= 0xC00FFFFF;
    gSnk_caps[3].object &= 0xC00FFFFF;
    gSnk_caps[4].object &= 0xC00FFFFF;
    gSnk_caps[5].object &= 0xC00FFFFF;
    gSnk_caps[6].object &= 0xC00FFFFF;

    for(i = 0; i < 7; ++i) {snk_caps[i].object = gSnk_caps[i].object;}

}

#ifdef FSC_HAVE_EXT_MSG
FSC_U8 gCountry_codes[6] =
{
        2, 0, /* 2-byte Number of country codes */

        /* country codes follow */
        'U','S','C','N'
};
#endif
