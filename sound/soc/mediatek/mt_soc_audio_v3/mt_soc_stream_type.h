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
 *  mt_sco_stream_type.h
 *
 * Project:
 * --------
 *   Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio stream type define
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

#ifndef _AUDIO_STREAM_ATTRIBUTE_H_
#define _AUDIO_STREAM_ATTRIBUTE_H_


/*****************************************************************************
 *                ENUM DEFINITION
 *****************************************************************************/
enum STREAMSTATUS
{
    STREAMSTATUS_STATE_FREE = -1,     // memory is not allocate
    STREAMSTATUS_STATE_STANDBY,  // memory allocate and ready
    STREAMSTATUS_STATE_EXECUTING, // stream is running
};

// use in modem pcm and DAI
enum MEMIFDUPWRITE
{
    MEMIFDUPWRITE_DUP_WR_DISABLE = 0x0,
    MEMIFDUPWRITE_DUP_WR_ENABLE  = 0x1
};

//Used when AWB and VUL and data is mono
enum MEMIFMONOSEL
{
    MEMIFMONOSEL_AFE_MONO_USE_L = 0x0,
    MEMIFMONOSEL_AFE_MONO_USE_R = 0x1
};

enum SAMPLINGRATE
{
    SAMPLINGRATE_AFE_8000HZ   = 0x0,
    SAMPLINGRATE_AFE_11025HZ = 0x1,
    SAMPLINGRATE_AFE_12000HZ = 0x2,
    SAMPLINGRATE_AFE_16000HZ = 0x4,
    SAMPLINGRATE_AFE_22050HZ = 0x5,
    SAMPLINGRATE_AFE_24000HZ = 0x6,
    SAMPLINGRATE_AFE_32000HZ = 0x8,
    SAMPLINGRATE_AFE_44100HZ = 0x9,
    SAMPLINGRATE_AFE_48000HZ = 0xa
};

enum FETCHFORMATPERSAMPLE
{
    FETCHFORMATPERSAMPLEb_AFE_WLEN_16_BIT = 0,
    FETCHFORMATPERSAMPLE_AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA = 1,
    FETCHFORMATPERSAMPLE_AFE_WLEN_32_BIT_ALIGN_24BIT_DATA_8BIT_0 = 3,
};


#endif
