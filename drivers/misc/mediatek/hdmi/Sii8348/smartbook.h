/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SMARTBOOK_H__
#define __SMARTBOOK_H__

//#define HID_CRYPTO			// enable scratchpad cryptiion (deprecated)
//#define SBK_FAKE_BATTERY		// faking 2nd battery data
#define HID_BUF   64
#define HID_SIZE  8 // 16		// HID_SIZE = Protocol used Registers count.
#define KB_MODLEN 8
#define KB_LEN    250
#define HID_COMMAND_PAYLOAD_LEN 6


//CBUS command define
#define SCRATCHPAD_OFFSET     0x40
#define WRITEBURST_MAX_LEN    0x8
#define CA_PMU_LEN

//Category
#define CA_INPUT_DEV    0x1
#define CA_MISC         0x2
#define CA_PMU          0x80

//COMMAND
#define INPUT_MOUSE       0x2
#define INPUT_KEYBOARD    0x3
#define INPUT_TOUCHPAD    0x4
#define MISC_MOUSE        0x2
#define MISC_KEYBOARD     0x3
#define MISC_TOUCHPAD     0x4
#define MISC_UNSUPPORT    0xff
#define MISC_HANDSHAKE    0x3c
#define MISC_LATENCY      0xaa
#define PMU_BATTERY       0x3
#define PMU_SCREEN        0x55

#define ID_LEN            6
//ID
#define SMB_SOURCE_ID_0   0x66
#define SMB_SOURCE_ID_1   0x19
#define SMB_SOURCE_ID_2   0x5a
#define SMB_SOURCE_ID_3   0x22
#define SMB_SOURCE_ID_4   0xba
#define SMB_SOURCE_ID_5   0x51
#define SMB_SINK_ID_0     0x22
#define SMB_SINK_ID_1     0x45
#define SMB_SINK_ID_2     0x43
#define SMB_SINK_ID_3     0x69
#define SMB_SINK_ID_4     0x77
#define SMB_SINK_ID_5     0x26


typedef struct{
    unsigned char category;
    unsigned char command;
    unsigned char payload[HID_COMMAND_PAYLOAD_LEN];
}HIDCommand;

typedef enum{
    ImmediateOff = 0,
    DownCount = 1,
    CancelDownCount = 2
}ScreenOffType;

typedef enum{
    Init = 0,
    Ack = 1
}HandshakeType;

typedef enum{
    NotConnect = 0,
    SmartBook = 1,
    MonitorTV = 2,
    Unknown = 3
}SinkType;

#define DEBUG_LOG

#ifdef DEBUG_LOG
#define smb_print(fmt, args...) pr_debug(fmt, ##args)
#else
#define smb_print(fmt, args...) 
#endif

#ifdef DEBUG_LOG
#define smb_mmp_print(event, type, data1, data2, str) MMProfileLogMetaStringEx(event, type, data1, data2, str)
#else
#define smb_mmp_print(event, type, data1, data2, str) 
#endif
//export function
extern SinkType SMBGetSinkStatus(void);
extern int SiiHandshakeCommand(HandshakeType ComType);
extern void SiiHidSuspend(int flag);
extern int SiiHidWrite(uint8_t *scratchpad_data);

// others function
extern void update_battery_2nd_info(int status_2nd, int capacity_2nd, int present_2nd);
extern void RecordStamp(bool dump, char tag);

#endif
