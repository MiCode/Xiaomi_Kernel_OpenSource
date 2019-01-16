/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
 */

#include "RefCode.h"
#include "RefCode_PDTScan.h"

void FirmwareCheck( void )
{
        unsigned char buffer[10];
        int i;
    unsigned int packrat, bootpackrat;
    unsigned short packageID, packageIDRev;

    printk("\nCheck firmware information\n");

        //Check Config ID
        readRMI(F34_Ctrl_Base, &buffer[0], 4);
        printk("\nConfigID = %c%c%c%c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

    //Check Product family ID
    readRMI(F01_Query_Base+2, buffer, 1);
    printk("Product Family = %d\n", buffer[0]);
    readRMI(F01_Query_Base+3, buffer, 1);
    printk("Firmware Revision = %d\n", buffer[0]);

        //Check UI Product ID
        readRMI(F01_Query_Base+11, &buffer[0], 10);
        printk("Product ID = ");
    for(i=0; i<10; i++)
    {
        printk("%c", buffer[i]);
        if(buffer[i] == 0)    break;
    }
        printk("\n");

        //Check UI packrat #
        readRMI(F01_Query_Base+18, &buffer[0], 3);
        packrat = (int)(buffer[0] | (buffer[1] << 8) | (buffer[2] << 16));
    printk("Firmware = 0x%X%X%X\n", buffer[2], buffer[1], buffer[0]);

    readRMI(F01_Query_Base+17, &buffer[0], 4);
    packageID = (buffer[1] << 8) | buffer[0];
    packageIDRev = (buffer[3] << 8) | buffer[2];
    printk("Package ID = %d\n", packageID);
    printk("Package ID Rev = %d\n\n", packageIDRev);

        //Enter bootloader mode
        readRMI(F34_Query_Base, &buffer[0], 2);
        writeRMI(F34_Data_Base+2, &buffer[0], 2);
        buffer[0] = 0x0f;
        writeRMI(0x12, &buffer[0], 1);

    SYNA_PDTScan_BootloaderMode();

    //Check bootloader Product family ID
    readRMI(F01_Query_Base+2, buffer, 1);
    printk("Bootloader Product Family = %d\n", buffer[0]);
    readRMI(F01_Query_Base+3, buffer, 1);
    printk("Bootloader Firmware Revision = %d\n", buffer[0]);

        //Check Bootloader Product ID
        readRMI(F01_Query_Base+11, &buffer[0], 10);
        printk("Bootloader Product ID = ");
        for(i=0; i<10; i++)
    {
        printk("%c", buffer[i]);
        if(buffer[i] == 0)    break;
        }
        printk("\n");

        //Check Bootloader packrat #
        readRMI(F01_Query_Base+18, &buffer[0], 3);
        bootpackrat = (unsigned int)(buffer[0] | (buffer[1] << 8) | (buffer[2] << 16));
    printk("Bootloader Firmware = 0x%X%X%X\n", buffer[2], buffer[1], buffer[0]);

        //Reset
        buffer[0] = 0x01;
        writeRMI(F01_Cmd_Base, &buffer[0], 1);
        delayMS(200);
}

void FirmwareCheck_temp( void )
{
#ifdef F54_Porting
    unsigned char buffer[10];
    unsigned short partNumber1;
    unsigned char partNumber2;
#else
      unsigned char buffer[10];
    int i;
    unsigned int packrat, bootpackrat;
    unsigned short packageID, packageIDRev;
    unsigned short partNumber1;
    unsigned char partNumber2, FWVersion1, FWVersion2;
#endif

        //Check Config ID
        readRMI(F34_Ctrl_Base, &buffer[0], 4);
        printk("\nConfigID = %c%c%c%c\n", buffer[0], buffer[1], buffer[2], buffer[3]);

    partNumber1 = (buffer[0] << 4) | (buffer[1] >> 4);
    partNumber2 = buffer[1] & 0x0F;
    printk("ID = 0x%x 0x%x 0x%x 0x%x\n", partNumber1, partNumber2, buffer[2], buffer[3]);

    printk("Configuration ID : TM%d-%03d %c%03d\n", partNumber1, partNumber2, buffer[2], buffer[3]);
}

void AttentionTest( void )
{
    unsigned char command;

    printk("\nBin #: 23        Name: Attention Test\n");

    //Reset
    command = 0x01;
    writeRMI(F01_Cmd_Base, &command, 1);
    delayMS(100);

    if(waitATTN(1, 300) == 1)
        printk("Attention Test Pass\n");
    else
        printk("Attention Test Fail\n");
}

