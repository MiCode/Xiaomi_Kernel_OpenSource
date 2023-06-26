/*******************************************************************************
 * @file     Log.c
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
#include "Log.h"

#ifdef FSC_DEBUG

void InitializeStateLog(StateLog *log)
{
    log->Count = 0;
    log->End = 0;
    log->Start = 0;
}

FSC_BOOL WriteStateLog(StateLog *log, FSC_U16 state, FSC_U32 time)
{
    if(!IsStateLogFull(log))
    {
        FSC_U8 index = log->End;
        log->logQueue[index].state = state;
        log->logQueue[index].time_s = time >> 16;     /* Upper 16: seconds */
        log->logQueue[index].time_ms = time & 0xFFFF; /* Lower 16: 0.1ms */

        log->End += 1;
        if(log->End == LOG_SIZE)
        {
            log->End = 0;
        }

        log->Count += 1;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

FSC_BOOL ReadStateLog(StateLog *log, FSC_U16 * state,
                      FSC_U16 * time_ms, FSC_U16 * time_s)
{
    if(!IsStateLogEmpty(log))
    {
        FSC_U8 index = log->Start;
        *state = log->logQueue[index].state;
        *time_ms = log->logQueue[index].time_ms;
        *time_s = log->logQueue[index].time_s;

        log->Start += 1;
        if(log->Start == LOG_SIZE)
        {
            log->Start = 0;
        }

        log->Count -= 1;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

FSC_U32 GetStateLog(StateLog *log, FSC_U8 *data, FSC_U8 bufLen)
{
    FSC_S32 i;
    FSC_S32 entries = log->Count;
    FSC_U16 state_temp;
    FSC_U16 time_tms_temp;
    FSC_U16 time_s_temp;
    FSC_U32 len = 0;

    for (i = 0; i < entries; i++)
    {
        if (bufLen < 5 ) { break; }
        if (ReadStateLog(log, &state_temp, &time_tms_temp, &time_s_temp))
        {
            data[len++] = state_temp;
            data[len++] = (FSC_U8) time_tms_temp;
            data[len++] = (time_tms_temp >> 8);
            data[len++] = (FSC_U8) time_s_temp;
            data[len++] = (time_s_temp) >> 8;
            bufLen -= 5;
        }
    }

    return len;
}
FSC_BOOL IsStateLogFull(StateLog *log)
{
    return (log->Count == LOG_SIZE) ? TRUE : FALSE;
}

FSC_BOOL IsStateLogEmpty(StateLog *log)
{
    return (!log->Count) ? TRUE : FALSE;
}

#endif /* FSC_DEBUG */

