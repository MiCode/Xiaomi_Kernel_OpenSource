/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_codec_speaker_63xx
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio codec stub file
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "mt_soc_analog_type.h"
#include <mach/pmic_mt6331_6332_sw.h>
#include "mt_soc_pcm_common.h"

//static DEFINE_MUTEX(Speaker_Ctrl_Mutex);
//static DEFINE_SPINLOCK(Speaker_lock);

static int Speaker_Counter = 0;
static bool  Speaker_Trim_init = false;

#ifndef CONFIG_MTK_FPGA
extern kal_uint32 mt6332_upmu_get_swcid(void);
#endif

static bool EnableBoost(void)
{
    return true;
}

extern int mt6332_OpenBoost4Audio(void);
extern int mt6332_CloseBoost4Audio(void);
static void BoostE2Open(void)
{
    printk("BoostE2Open2\n");
    if (EnableBoost() == false)
    {
        return ;
    }
    //Ana_Set_Reg(0x809a,  0x6025, 0x002a); // clk enable
    ///Ana_Set_Reg(SPK_TOP_CKPDN_CON1_CLR,  0x0002, 0xffff); // clk enable
    mt6332_OpenBoost4Audio();
    Ana_Set_Reg(0x853a,  0x0000, 0xffff); // VSBST_CON8  , 1 enable
    Ana_Set_Reg(0x8536,  0x0000, 0xffff); // VSBST_CON6
    Ana_Set_Reg(0x8532,  0x01c0, 0xffff); // VSBST_CON4
    Ana_Set_Reg(0x80b2,  0x03ff, 0xffff);
    Ana_Set_Reg(0x854a,  0x0000, 0xffff);
    Ana_Set_Reg(0x854a,  0x0001, 0xffff);
    Ana_Set_Reg(0x854c,  0x0001, 0xffff);

    Ana_Set_Reg(0x8542,  0x0040, 0xffff); //  VSBST_CON12
    Ana_Set_Reg(0x854e,  0x0008, 0xffff);
    Ana_Set_Reg(0x853e,  0x0021, 0x0021); //  VSBST_CON10 , 1 enable
    // wiat for boost ready
    msleep(30);
    Ana_Set_Reg(0x854e,  0x0000, 0xffff);

}

static void BoostE2Close(void)
{
    printk("BoostE2Close2\n");
    //Ana_Set_Reg(0x853e,  0x0020, 0xffff); //  VSBST_CON10 , 1 enable
    //msleep(30);
    ///Ana_Set_Reg(SPK_TOP_CKPDN_CON1_SET,  0x0002, 0xffff); // clk enable
    mt6332_CloseBoost4Audio();
}

static void BoostE1Open(void)
{
    printk("BoostE1Open2\n");
    if (EnableBoost() == false)
    {
        return ;
    }
    //Ana_Set_Reg(0x809a,  0x6025, 0x002a); // clk enable
    ///Ana_Set_Reg(SPK_TOP_CKPDN_CON1_CLR,  0x0002, 0xffff); // clk enable
    mt6332_OpenBoost4Audio();

    Ana_Set_Reg(VSBST_CON21,  0x0012, 0xffff); // VSBST_CON21
    Ana_Set_Reg(VSBST_CON8,  0x0000, 0xffff); // VSBST_CON8  , 1 enable
    Ana_Set_Reg(VSBST_CON6,  0x0000, 0xffff); // VSBST_CON6
    Ana_Set_Reg(VSBST_CON4,  0x01c0, 0xffff); // VSBST_CON4

    Ana_Set_Reg(VSBST_CON18,  0x0008, 0xffff); // VSBST_CON18
    Ana_Set_Reg(VSBST_CON10,  0x0021, 0x0021); //  VSBST_CON10 , 1 enable
    // wiat for boost ready
    msleep(30);
    Ana_Set_Reg(VSBST_CON18,  0x0000, 0xffff); //  VSBST_CON18
    Ana_Set_Reg(VSBST_CON7,  0x0000, 0xffff); //  VSBST_CON7
    Ana_Set_Reg(VSBST_CON5,  0x0300, 0xffff); //  VSBST_CON5
    Ana_Set_Reg(VSBST_CON12,  0x003a, 0xffff); //  VSBST_CON12
}

static void BoostE1Close(void)
{
    printk("BoostE1Close2\n");
    Ana_Set_Reg(VSBST_CON10,  0x0000, 0x0001); //  VSBST_CON10 , 1 enable
    msleep(30);
    //Ana_Set_Reg(0x809a,  0x002a, 0x002a); // clk enable
    ///Ana_Set_Reg(SPK_TOP_CKPDN_CON1_SET,  0x0002, 0xffff); // clk enable
    mt6332_CloseBoost4Audio();
}

static void BoostOpen(void)
{
#ifndef CONFIG_MTK_FPGA
    if (mt6332_upmu_get_swcid() == PMIC6332_E1_CID_CODE)
    {
        BoostE1Open();
    }
    else
    {
        BoostE2Open();
    }
#else
    BoostE2Open();
#endif
}

static void BoostClose(void)
{
#ifndef CONFIG_MTK_FPGA
    if (mt6332_upmu_get_swcid() == PMIC6332_E1_CID_CODE)
    {
        BoostE1Close();
    }
    else
    {
        BoostE2Close();
    }
#else
    BoostE2Close();
#endif
}

static void Enable_Speaker_Clk(bool benable)
{
    if (benable)
    {
        Speaker_Counter++;
        Ana_Set_Reg(SPK_TOP_CKPDN_CON0_CLR, 0x000e, 0xffff);
#ifndef CONFIG_MTK_FPGA
        if (mt6332_upmu_get_swcid() == PMIC6332_E1_CID_CODE)
        {
            //Ana_Set_Reg(0x8552,  0x0005, 0xffff);
        }
#endif
    }
    else
    {
        Speaker_Counter--;
        Ana_Set_Reg(SPK_TOP_CKPDN_CON0_SET, 0x000e, 0xffff);
    }
}

void Speaker_ClassD_Open(void)
{
    printk("%s\n", __func__);
    BoostOpen();
    Enable_Speaker_Clk(true);
    Ana_Set_Reg(SPK_CON2,  0x0404, 0xffff);
    Ana_Set_Reg(SPK_CON9,  0x0a00, 0xffff);
    Ana_Set_Reg(SPK_CON13, 0x1900, 0xffff);
    if (Speaker_Trim_init == false)
    {
        Ana_Set_Reg(SPK_CON0,  0x3000, 0xffff);
        Ana_Set_Reg(SPK_CON0,  0x3408, 0xffff);
        Ana_Set_Reg(SPK_CON0,  0x3409, 0xffff);
        msleep(20);
        Ana_Set_Reg(SPK_CON13, 0x0100, 0xff00);
        Speaker_Trim_init = true;
    }
    Ana_Set_Reg(SPK_CON0,  0x3000, 0xffff);
    Ana_Set_Reg(SPK_CON0,  0x3001, 0xffff);
}

void Speaker_ClassD_close(void)
{
    printk("%s\n", __func__);
    Ana_Set_Reg(SPK_CON0,  0x0000, 0xffff);
    BoostClose();
    Enable_Speaker_Clk(false);
}

void Speaker_ClassAB_Open(void)
{
    printk("%s\n", __func__);
    BoostOpen();
    Enable_Speaker_Clk(true);
    Ana_Set_Reg(SPK_CON2,  0x0204, 0xffff);
    Ana_Set_Reg(SPK_CON9,  0x0a00, 0xffff);
    if (Speaker_Trim_init == false)
    {
        Ana_Set_Reg(SPK_CON13, 0x3B00, 0xffff);
        Ana_Set_Reg(SPK_CON0,  0x3000, 0xffff);
        Ana_Set_Reg(SPK_CON0,  0x3408, 0xffff);
        msleep(5);
        Ana_Set_Reg(SPK_CON0,  0x3409, 0xffff);
        msleep(20);
        Speaker_Trim_init = true;
    }
    Ana_Set_Reg(SPK_CON13, 0x2300, 0xff00);
    Ana_Set_Reg(SPK_CON0,  0x3000, 0xffff);
    Ana_Set_Reg(SPK_CON0,  0x3005, 0xffff);
}

void Speaker_ClassAB_close(void)
{
    printk("%s\n", __func__);
    BoostClose();
    Ana_Set_Reg(SPK_CON0,  0x0000, 0xffff);
    Enable_Speaker_Clk(false);
}

void Speaker_ReveiverMode_Open(void)
{
    printk("%s\n", __func__);
    BoostOpen();
    Enable_Speaker_Clk(true);
    Ana_Set_Reg(SPK_CON2,  0x0204, 0xffff);
    Ana_Set_Reg(SPK_CON9,  0x0a00, 0xffff);
    Ana_Set_Reg(SPK_CON13, 0x1B00, 0xffff);

    Ana_Set_Reg(SPK_CON0,  0x1000, 0xffff);
    Ana_Set_Reg(SPK_CON0,  0x1408, 0xffff);
    Ana_Set_Reg(SPK_CON0,  0x1409, 0xffff);
    msleep(20);
    Ana_Set_Reg(SPK_CON13, 0x0300, 0xff00);
    Ana_Set_Reg(SPK_CON0,  0x1000, 0xffff);
    Ana_Set_Reg(SPK_CON0,  0x1005, 0xffff);
}

void Speaker_ReveiverMode_close(void)
{
    printk("%s\n", __func__);
    Ana_Set_Reg(SPK_CON0,  0x0000, 0xffff);
    BoostClose();
    Enable_Speaker_Clk(false);
}

bool GetSpeakerOcFlag(void)
{
    unsigned int OCregister = 0;
    unsigned int bitmask = 1;
    bool DmodeFlag = false;
    bool ABmodeFlag = false;
#if 0 // workaround for co-clock screen off
    Ana_Set_Reg(SPK_INT_CON2_CLR, (1 << 6 || 1 << 7), (1 << 6 || 1 << 7));
#endif
    OCregister = Ana_Get_Reg(0x8CFE);
    DmodeFlag = OCregister & (bitmask << 14); // ; no.14 bit is SPK_D_OC_L_DEG
    ABmodeFlag = OCregister & (bitmask << 15); // ; no.15 bit is SPK_AB_OC_L_DEG
    printk("OCregister = %d \n", OCregister);
    return (DmodeFlag | ABmodeFlag);
}


