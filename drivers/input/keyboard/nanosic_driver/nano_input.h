#ifndef __NANO_INPUT_H__
#define __NANO_INPUT_H__


// 8 Byte Keyboard ID(0x05) + (Ctrl,shift,alt,gui) + rev + 6byte(Key)
// consumer:reportID(0x06) + 16*2 Bit(4 keys)
static uint8_t HID_KeyboardReportDescriptor[]=
{
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls) (61 Byte)
    0x09, 0x06,        //   Usage (Keyboard)
    0xA1, 0x01,        //   Collection (Application)
    0x85, 0x05,        //   Report ID (5)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)

    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

    0x95, 0x05,        //   Report Count (5)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)

    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xA4, 0x00,  //   Logical Maximum (164)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x2A, 0xA4, 0x00,  //   Usage Maximum (0xA4)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
};

// 8 Byte Keyboard ID(0x05) + (Ctrl,shift,alt,gui) + rev + 6byte(Key)
// consumer:reportID(0x06) + 16*2 Bit(4 keys)
static uint8_t HID_ConsumerReportDescriptor[]=
{
    0x05, 0x0C,       //    USAGE_PAGE (Consumer Devices) 25
    0x09, 0x01,       //    USAGE (Consumer Control)
    0xA1, 0x01,       //    COLLECTION (Application)
    0x85, 0x06,       //    REPORT_ID (0x06)
    0x15, 0x00,       //    LOGICAL_MINIMUM (0)
    0x26, 0x80,0x03,  //    LOGICAL_MAXIMUM (0X0380)
    0x19, 0x00,       //    USAGE_MINIMUM(0)
    0x2A, 0x80,0x03,  //    USAGE_MAXIMUM(0x380)
    0x75, 0x10,       //    REPORT_SIZE (16)
    0x95, 0x01,       //    REPORT_COUNT (1)
    0x81, 0x00,       //    INPUT (Cnst,Var,Abs)
    0xC0,             //    End Collection
};

// mouse:reportID(0x2)
static unsigned char HID_MouseReportDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x05,        //     Usage Maximum (0x05)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x05,        //     Report Count (5)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x03,        //     Report Size (3)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x16, 0x00, 0x80,  //     Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //    End Collection  
    0xC0,              //    End Collection
};

#define TOUCH_PAD_X_SIZE_DESC         2560
#define TOUCH_PAD_Y_SIZE_DESC         1600
#define MI_DISPLAY_X_SIZE_DESC        2560
#define MI_DISPLAY_Y_SIZE_DESC        1600

// touch:reportID(0x19)
static unsigned char HID_TouchReportDescriptor[] = 
{
	//-----------------------------------------------
	0x05, 0x0D,        // Usage Page (Digitizer)
	0x09, 0x05,        // Usage (0x05:Touch Pad; 0x04:Touch Screen)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x19,        //   Report ID (25)
	//----------------------
	// button
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x35, 0x00,        //   Physical Minimum (0)
	0x45, 0x01,        //   Physical Maximum (1)
	0x75, 0x01,        //   Report Size (1)
	0x95, 0x02,        //   Report Count (2)
	0x05, 0x09,        //   Usage Page (Button)
	0x09, 0x01,        //   Usage (Button 1)
	0x09, 0x02,        //   Usage (Button 2)
	0x81, 0x02,        //   Input (Data,Var,Abs)
	0x95, 0x06,        //   Report Count (6)
	0x81, 0x01,        //   Input (Cnst,Var,Abs)  
	//----------------------
	// touch Pad
	0x05, 0x0D,        //   Usage Page (Digitizer)
	0x09, 0x22,        //   Usage (Finger)
	0xA1, 0x02,        //   Collection (Logical)
	0x09, 0x42,        //     Usage (Tip Switch)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x32,        //     Usage (In Range)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x47,        //     Usage(Touch Valid)
	0x81, 0x02,        //     Input(Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x05,        //     Report Count (5)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x08,        //     Report Size (8)
	0x09, 0x51,        //     Usage (Contact identifier)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, (TOUCH_PAD_X_SIZE_DESC&0xff), ((TOUCH_PAD_X_SIZE_DESC>>8)&0xff),  //     Logical Maximum (3203)
	0x75, 0x10,        //     Report Size (16)
	0x55, 0x0D,        //     Uint Exponent(-3)
	0x65, 0x13,        //     Unit(Inch,Englinear)
	0x09, 0x30,        //     Usage (X)
	0x35, 0x00,        //     Physical Minimum (0)
	0x46, (MI_DISPLAY_X_SIZE_DESC&0xff), ((MI_DISPLAY_X_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (400)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x31,        //     Usage (Y)
	0x26, (TOUCH_PAD_Y_SIZE_DESC&0xff), ((TOUCH_PAD_Y_SIZE_DESC>>8)&0xff),  //     Logical Maximum (1884)
	0x46, (MI_DISPLAY_Y_SIZE_DESC&0xff), ((MI_DISPLAY_Y_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (235)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	//-----------------------------------------------
	0xA1, 0x02,        //   Collection (Logical)
	0x05, 0x0D,        //     Usage Page (Digitizer)
	0x09, 0x42,        //     Usage (Tip Switch)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x32,        //     Usage (In Range)  
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x47,        //     Usage(Touch Valid)
	0x81, 0x02,        //     Input(Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x05,        //     Report Count (5)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x08,        //     Report Size (8)
	0x09, 0x51,        //     Usage (Contact identifier)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, (TOUCH_PAD_X_SIZE_DESC&0xff), ((TOUCH_PAD_X_SIZE_DESC>>8)&0xff),  //     Logical Maximum (4095)
	0x75, 0x10,        //     Report Size (16)
	0x55, 0x0D,        //     Uint Exponent(-3)
	0x65, 0x13,        //     Unit(Inch,Englinear)  
	0x09, 0x30,        //     Usage (X)
	0x35, 0x00,        //     Physical Minimum (0)
	0x46, (MI_DISPLAY_X_SIZE_DESC&0xff), ((MI_DISPLAY_X_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (4095)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x31,        //     Usage (Y)
	0x26, (TOUCH_PAD_Y_SIZE_DESC&0xff), ((TOUCH_PAD_Y_SIZE_DESC>>8)&0xff),  //     Logical Maximum (1884)
	0x46, (MI_DISPLAY_Y_SIZE_DESC&0xff), ((MI_DISPLAY_Y_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (4095)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	//-----------------------------------------------
	0xA1, 0x02,        //   Collection (Logical)
	0x05, 0x0D,        //     Usage Page (Digitizer)
	0x09, 0x42,        //     Usage (Tip Switch)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x32,        //     Usage (In Range)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x47,        //     Usage(Touch Valid)
	0x81, 0x02,        //     Input(Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x05,        //     Report Count (5)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x08,        //     Report Size (8)
	0x09, 0x51,        //     Usage (Contact identifier)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x15, 0x00,        //     Logical Minimum (0)
	0x26, (TOUCH_PAD_X_SIZE_DESC&0xff), ((TOUCH_PAD_X_SIZE_DESC>>8)&0xff),  //     Logical Maximum (4095)
	0x75, 0x10,        //     Report Size (16)
	0x55, 0x0D,        //     Uint Exponent(-3)
	0x65, 0x13,        //     Unit(Inch,Englinear)  
	0x09, 0x30,        //     Usage (X)
	0x35, 0x00,        //     Physical Minimum (0)
	0x46, (MI_DISPLAY_X_SIZE_DESC&0xff), ((MI_DISPLAY_X_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (4095)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x31,        //     Usage (Y)
	0x26, (TOUCH_PAD_Y_SIZE_DESC&0xff), ((TOUCH_PAD_Y_SIZE_DESC>>8)&0xff),  //     Logical Maximum (1884)
	0x46, (MI_DISPLAY_Y_SIZE_DESC&0xff), ((MI_DISPLAY_Y_SIZE_DESC >> 8)&0xff),  //     Physical Maximum (4095)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection  
	//-----------------------------------------------
	0x05, 0x0D,        //   Usage Page (Digitizer)
	0x09, 0x54,        //   Usage (Contact Ouunt)
	0x95, 0x01,        //   Report Count (1)
	0x75, 0x08,        //   Report Size (8)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x08,        //   Logical Maximum (8)
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x55,        //   Usage (Contact Ouunt Maximum)
	0xb1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	//-----------------------------------------------
	// end 
	0xC0,              // End Collection
};

// Vendor:
// ReportId(0x24) + 63byte (Usb->Pc)
// ReportId(0x23) + 31byte (Usb->Pc)
// ReportId(0x4e) + 31byte (Pc->USB)
// ReportId(0x4f) + 63byte (Pc->USB)
static uint8_t HID_VendorReportDescriptor[]=
{ // 76 Byte
    0x06,0x01,0xFF, // USAGE_PAGE (Talon Specific) (42 byte)
    0x09,0x04, // USAGE (diagnostic)
    0xA1,0x01, // COLLECTION (Application)

    0xA1, 0x02,  //   Collection (Logical)
    0x85, 0x22,  //   Report ID (0x22)
    0x09, 0x14,  //   Usage (0x14)
    0x15, 0x80,  //   Logical Minimum (-128)
    0x25, 0x7F,  //   Logical Maximum (127)
    0x75, 0x08,  //   Report Size (8)
    0x95, 0x0F,  //   Report Count (15)
    0x81, 0x22,  //   Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,No Null Position)
    0xC0,        //   End Collection
    
    0xA1,0x02, // COLLECTION (Logical) // 17 byte
    0x85,0x23, // REPORT_ID (0x23)
    0x09,0x14, // USAGE (byte)
    0x15,0x80, // LOGICAL_MINIMUM (-128)
    0x25,0x7F, // LOGICAL_MAXIMUM (127)
    0x75,0x08, // REPORT_SIZE (8)
    0x95,0x1F, // REPORT_COUNT (63)
    0x81,0x22, // INPUT (Data,Var,Abs,NPrf)
    0xC0,      // END_COLLECTION 
    
    0xA1,0x02, // COLLECTION (Logical)
    0x85,0x24, // REPORT_ID (0x24) 
    0x09,0x14, // USAGE (byte)
    0x15,0x80, // LOGICAL_MINIMUM (-128)
    0x25,0x7F, // LOGICAL_MAXIMUM (127)
    0x75,0x08, // REPORT_SIZE (8)
    0x95,0x3F, // REPORT_COUNT (63)
    0x81,0x22, // INPUT (Data,Var,Abs,NPrf)
    0xC0,      // END_COLLECTION 
    
    0xA1,0x02, // COLLECTION (Logical)
    0x85,0x4E, // REPORT_ID (0x4F) 
    0x09,0x14, // USAGE (byte) 
    0x15,0x80, // LOGICAL_MINIMUM (-128)
    0x25,0x7F, // LOGICAL_MAXIMUM (127)
    0x75,0x08, // REPORT_SIZE (8)
    0x95,0x1F, // REPORT_COUNT (63)
    0x91,0x22, // OUTPUT (Data,Var,Abs,NPrf)
    0xC0,      // END_COLLECTION 
    
    0xA1,0x02, // COLLECTION (Logical)
    0x85,0x4F, //REPORT_ID (0x4F) 
    0x09,0x14, // USAGE (byte) 
    0x15,0x80, // LOGICAL_MINIMUM (-128)
    0x25,0x7F, // LOGICAL_MAXIMUM (127)
    0x75,0x08, // REPORT_SIZE (8)
    0x95,0x3F, // REPORT_COUNT (63)
    0x91,0x22, // OUTPUT (Data,Var,Abs,NPrf)
    0xC0,      // END_COLLECTION 
    0xC0,      // END_COLLECTION 
};
#endif
