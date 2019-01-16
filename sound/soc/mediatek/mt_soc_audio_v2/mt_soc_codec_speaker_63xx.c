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
#include <mach/pmic_mt6325_sw.h>
#include "mt_soc_pcm_common.h"

//static DEFINE_MUTEX(Speaker_Ctrl_Mutex);
//static DEFINE_SPINLOCK(Speaker_lock);
//static int Speaker_Counter = 0;
//static bool  Speaker_Trim_init = false;

extern kal_uint32 mt6332_upmu_get_swcid(void);

void Speaker_ClassD_Open(void)
{
    kal_uint32 i, SPKTrimReg = 0;
    printk("%s\n", __func__);
    // spk trim
    Ana_Set_Reg(SPK_CON7, 0x4531, 0xffff); //TD1,TD2,TD3 for trim (related with trim waiting time)
    Ana_Set_Reg(SPK_CON0, 0x3008, 0xffff);//Set to class D mode, set 0dB amplifier gain,  enable trim function
    Ana_Set_Reg(SPK_CON13, 0x1800, 0xffff);//Offset trim RSV bit
    Ana_Set_Reg(SPK_CON2, 0x04A4, 0xffff); //Turn on OC function, set SPK PGA in DCC mod
    Ana_Set_Reg(SPK_ANA_CON0, 0x5000, 0xffff); //Set 12dB PGA gain
    Ana_Set_Reg(SPK_CON9, 0x2000, 0xffff); //Set Fast Vcm mode
    Ana_Set_Reg(SPK_CON0, 0x3009, 0xffff); //Turn on speaker
    Ana_Set_Reg(SPK_CON9, 0x0A00, 0xffff); //Set Vcm high PSRR mode
    for (i = 0; i < 10; i++)
    {
        udelay(1000); //wait 10ms for trimming
    }
    Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff); //Turn off trim,(set to class D mode)
    Ana_Set_Reg(SPK_CON13, 0x0000, 0xffff); //Clock from Saw-tooth to Triangular wave

    SPKTrimReg = Ana_Get_Reg(SPK_CON1);
    if ((SPKTrimReg & 0x8000) == 0)
    {
        printk("spk trim fail!\n");
    }
    else
    {
        printk("spk trim offset=%d\n", (SPKTrimReg & 0x1f));
    }

    // spk amp gain fixed at 0dB
    Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff); //set 0dB amplifier gain
}

void Speaker_ClassD_close(void)
{
    printk("%s\n", __func__);
    Ana_Set_Reg(SPK_CON9, 0x2A00, 0xffff); //Set Vcm high PSRR mode
    Ana_Set_Reg(SPK_ANA_CON0, 0x5000, 0xffff); //Set 12dB PGA gain(level when trimming)
    Ana_Set_Reg(SPK_CON0, 0x3001, 0xffff); //set 0dB amp gain(level when trimming)
    Ana_Set_Reg(SPK_CON0, 0x3000, 0xffff); //amp L-ch disable
}


void Speaker_ClassAB_Open(void)
{
    kal_uint32 i, SPKTrimReg = 0;
    printk("%s\n", __func__);
    // spk trim
    Ana_Set_Reg(SPK_CON7, 0x4531, 0xffff); //TD1,TD2,TD3 for trim (related with trim waiting time)
    Ana_Set_Reg(SPK_CON0, 0x1008, 0xffff);//Set to class D mode, set -6dB amplifier gain,  enable trim function
    Ana_Set_Reg(SPK_CON13, 0x1800, 0xffff);//Offset trim RSV bit
    Ana_Set_Reg(SPK_CON2, 0x02A4, 0xffff); //Turn on OC function, set SPK PGA in DCC mod
    Ana_Set_Reg(SPK_ANA_CON0, 0x0800, 0xffff); //Set 0dB PGA gain
    Ana_Set_Reg(SPK_CON9, 0x2000, 0xffff); //Set Fast Vcm mode
    Ana_Set_Reg(SPK_CON0, 0x1009, 0xffff); //Turn on speaker
    Ana_Set_Reg(SPK_CON9, 0x0100, 0xffff); //Set Vcm high PSRR mode
    for (i = 0; i < 10; i++)
    {
        udelay(1000); //wait 10ms for trimming
    }
    
    Ana_Set_Reg(SPK_CON0, 0x1001, 0xffff); //Turn off trim
    Ana_Set_Reg(SPK_CON0, 0x1000, 0xffff); //Turn off spk_en
    Ana_Set_Reg(SPK_CON0, 0x1004, 0xffff); // set to class AB mode
    Ana_Set_Reg(SPK_CON0, 0x1005, 0xffff); // Turn on spk_en
    
    Ana_Set_Reg(SPK_CON13, 0x0000, 0xffff); //Clock from Saw-tooth to Triangular wave

    SPKTrimReg = Ana_Get_Reg(SPK_CON1);
    if ((SPKTrimReg & 0x8000) == 0)
    {
        printk("spk trim fail!\n");
    }
    else
    {
        printk("spk trim offset=%d\n", (SPKTrimReg & 0x1f));
    }

    // spk amp gain fixed at 0dB
    Ana_Set_Reg(SPK_CON0, 0x3005, 0xffff); //set 0dB amplifier gain
}

void Speaker_ClassAB_close(void)
{
    printk("%s\n", __func__);
    Ana_Set_Reg(SPK_CON9, 0x2100, 0xffff); //Set Vcm high PSRR mode
    Ana_Set_Reg(SPK_ANA_CON0, 0x0800, 0xffff); //Set 0dB PGA gain(level when trimming)
    Ana_Set_Reg(SPK_CON0, 0x1005, 0xffff); //set -6dB amp gain(level when trimming)
    Ana_Set_Reg(SPK_CON0, 0x1004, 0xffff); //amp L-ch disable
}

void Speaker_ReveiverMode_Open(void)
{
    printk("%s\n", __func__);
    Ana_Set_Reg(SPK_CON0, 0x1304, 0xffff); //Enable thermal_shout_down, OC_shout_down. Set class AB mode, -6dB amp gain
    Ana_Set_Reg(SPK_CON2, 0x02A4, 0xffff); //Turn on OC function, set SPK PGA in DCC mode
    Ana_Set_Reg(SPK_CON9, 0x2100, 0xffff); //Set Fast Vcm mode
    Ana_Set_Reg(SPK_ANA_CON0, 0x2000, 0xffff); //Set 6dB PGA gain
    Ana_Set_Reg(SPK_CON0, 0x1305, 0xffff); //Turn on speaker
    udelay(2000);
    Ana_Set_Reg(SPK_CON9, 0x0100, 0xffff); //Set Vcm high PSRR mode
}

void Speaker_ReveiverMode_close(void)
{
    Ana_Set_Reg(SPK_CON9, 0x2000, 0xffff); //speaker L PGA gain control: mute
    Ana_Set_Reg(SPK_CON2, 0x0014, 0xffff); //Turn off OC function
    Ana_Set_Reg(SPK_ANA_CON0, 0x0, 0xffff); //Set mute PGA gain
    Ana_Set_Reg(SPK_CON0, 0x0, 0xffff); //set mute amp gain, amp L-ch disable
}

bool GetSpeakerOcFlag(void)
{
    unsigned int OCregister = 0;
    unsigned int bitmask = 1;
    bool DmodeFlag = false;
    bool ABmodeFlag = false;
    Ana_Set_Reg(TOP_CKPDN_CON2_CLR, 0x3, 0xffff);
    OCregister = Ana_Get_Reg(SPK_CON6);
    DmodeFlag = OCregister & (bitmask << 14); // ; no.14 bit is SPK_D_OC_L_DEG
    ABmodeFlag = OCregister & (bitmask << 15); // ; no.15 bit is SPK_AB_OC_L_DEG
    printk("OCregister = %d \n", OCregister);
    return (DmodeFlag | ABmodeFlag);
}


