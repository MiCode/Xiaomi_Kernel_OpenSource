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
  
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#include "RefCode.h"
#include "RefCode_PDTScan.h"

#ifdef _F54_TEST_
unsigned char F54_TxToTxReport(void)
{
   unsigned char ImageBuffer[CFG_F54_TXCOUNT];
   unsigned char ImageArray[CFG_F54_TXCOUNT];
   unsigned char Result = 0; 
   //unsigned char Result[CFG_F54_TXCOUNT];
   
   int i, k;
   int shift;

   unsigned char command;

#ifdef F54_Porting
	char buf[256] = {0};
	int ret = 0;

	ret = sprintf(buf, "\nBin #: 5		Name: Transmitter To Transmitter Short Test\n");
#else   
   printk("\nBin #: 5		Name: Transmitter To Transmitter Short Test\n");
#endif
   for (i = 0; i < CFG_F54_TXCOUNT; i++)
		ImageArray[i] = 1;

   // Set report mode to run Tx-to-Tx
   command = 0x05;
   writeRMI(F54_Data_Base, &command, 1);

   command = 0x00;
   writeRMI(F54_Data_LowIndex, &command, 1);
   writeRMI(F54_Data_HighIndex, &command, 1);
   
   // Set the GetReport bit to run Tx-to-Tx
   command = 0x01;
   writeRMI(F54_Command_Base, &command, 1);

   // Wait until the command is completed
   do {
   	delayMS(1); //wait 1ms
      readRMI(F54_Command_Base, &command, 1);
   } while (command != 0x00);

   readRMI(F54_Data_Buffer, &ImageBuffer[0], 4);

	// One bit per transmitter channel
	k = 0;		
	for (i = 0; i < CFG_F54_TXCOUNT; i++)
	{
		 k = i / 8;
		 shift = i % 8;
		 if(!(ImageBuffer[k] & (1 << shift))) ImageArray[i] = 0;
	}

#ifdef F54_Porting
	   ret += sprintf(buf+ret, "Column:\t");
#else
   printk("Column:\t");
#endif
   for (i = 0; i < numberOfTx; i++)
   {   
#ifdef F54_Porting
		ret += sprintf(buf+ret, "Tx%d,\t", TxChannelUsed[i]);
#else   
	   printk("Tx%d,\t", TxChannelUsed[i]);
#endif
   }
#ifdef F54_Porting
	ret += sprintf(buf+ret, "\n");
	ret += sprintf(buf+ret, "0:\t");
#else   
   printk("\n");
   
   printk("0:\t");
#endif   
   for (i = 0; i < numberOfTx; i++)
   {   
	   if(!ImageArray[TxChannelUsed[i]])
	   {
		   Result++;
#ifdef F54_Porting
			ret += sprintf(buf+ret, "%d,\t", ImageArray[TxChannelUsed[i]]);
#else
		   printk("%d,\t", ImageArray[TxChannelUsed[i]]);
#endif
	   }
	   else
	   {
#ifdef F54_Porting
			ret += sprintf(buf+ret, "%d(*),\t", ImageArray[TxChannelUsed[i]]);
#else
		   printk("%d(*),\t", ImageArray[TxChannelUsed[i]]);
#endif
	   }
   }
#ifdef F54_Porting
	ret += sprintf(buf+ret, "\n");
#else   
   printk("\n");
#endif

   /*
   // Check against test limits
   printk("\nTx-Tx short test result:\n");
   for (i = 0; i < numberOfTx; i++)
   {   
	    if (ImageArray[i]== TxTxLimit)
   			Result[i] = 'P'; //Pass
		else
		    Result[i] = 'F'; //Fail
		printk("Tx[%d] = %c\n", TxChannelUsed[i], Result[i]);
   }
   */
   
   //enable all the interrupts
//	SetPage(0x00);
   //Reset
	command= 0x01;
	writeRMI(F01_Cmd_Base, &command, 1);
	delayMS(200);
	readRMI(F01_Data_Base+1, &command, 1); //Read Interrupt status register to Interrupt line goes to high

	if(Result == numberOfTx)
	{
#ifdef F54_Porting
		ret += sprintf(buf+ret, "Test Result: Pass\n");
		write_log(buf);
#else	
		printk("Test Result: Pass\n");
#endif
		return 1; //Pass
	}
	else
	 {
#ifdef F54_Porting
		ret += sprintf(buf+ret, "Test Result: Fail\n");
		write_log(buf);
#else	 
		 printk("Test Result: Fail\n");
#endif
		 return 0; //Fail
	 }
}

int F54_GetTxToTxReport(char *buf)
{
	unsigned char ImageBuffer[CFG_F54_TXCOUNT];
	unsigned char ImageArray[CFG_F54_TXCOUNT];
	unsigned char Result = 0; 

	int i, k;
	int shift;

	unsigned char command;

	int ret = 0;
	int waitcount;

	for (i = 0; i < CFG_F54_TXCOUNT; i++)
		ImageArray[i] = 1;

	// Set report mode to run Tx-to-Tx
	command = 0x05;
	writeRMI(F54_Data_Base, &command, 1);

	command = 0x00;
	writeRMI(F54_Data_LowIndex, &command, 1);
	writeRMI(F54_Data_HighIndex, &command, 1);

	// Set the GetReport bit to run Tx-to-Tx
	command = 0x01;
	writeRMI(F54_Command_Base, &command, 1);

	// Wait until the command is completed
	waitcount = 0;
	do {
		if(++waitcount > 500)
		{
			pr_info("%s[%d], command = %d\n", __func__, __LINE__, command);
			return ret;
		}
		delayMS(1); //wait 1ms
		readRMI(F54_Command_Base, &command, 1);
	} while (command != 0x00);

	readRMI(F54_Data_Buffer, &ImageBuffer[0], 4);

	// One bit per transmitter channel
	k = 0;		
	for (i = 0; i < CFG_F54_TXCOUNT; i++)
	{
		k = i / 8;
		shift = i % 8;
		if(!(ImageBuffer[k] & (1 << shift))) ImageArray[i] = 0;
	}

	ret += sprintf(buf+ret, "Info: Tx=%d\n", numberOfTx);
	ret += sprintf(buf+ret, "UsedTx: ");
	for (i = 0; i < numberOfTx; i++)
	{   
		ret += sprintf(buf+ret, "%d", TxChannelUsed[i]);

		if(i < (numberOfTx-1))
			ret += sprintf(buf+ret, " ");
	}
	ret += sprintf(buf+ret, "\n");
	ret += sprintf(buf+ret, "        ");
	for (i = 0; i < numberOfTx; i++)
	{   
		if(!ImageArray[TxChannelUsed[i]])
		{
			Result++;
			ret += sprintf(buf+ret, "%d", ImageArray[TxChannelUsed[i]]);
		}
		else
		{
			ret += sprintf(buf+ret, "%d(*)", ImageArray[TxChannelUsed[i]]);
		}

		if(i < (numberOfTx-1))
			ret += sprintf(buf+ret, " ");
	}
	ret += sprintf(buf+ret, "\n");

	/*
	// Check against test limits
	printk("\nTx-Tx short test result:\n");
	for (i = 0; i < numberOfTx; i++)
	{   
	if (ImageArray[i]== TxTxLimit)
	Result[i] = 'P'; //Pass
	else
	Result[i] = 'F'; //Fail
	printk("Tx[%d] = %c\n", TxChannelUsed[i], Result[i]);
	}
	 */


	if (Result == numberOfTx)
	{
		ret += sprintf(buf+ret, "RESULT: Pass\n");
	}
	else
	{
		ret += sprintf(buf+ret, "RESULT: Fail\n");
	}

	//enable all the interrupts
	//	SetPage(0x00);
	//Reset
	command= 0x01;
	writeRMI(F01_Cmd_Base, &command, 1);
	delayMS(200);
	readRMI(F01_Data_Base+1, &command, 1); //Read Interrupt status register to Interrupt line goes to high

	return ret;
}
#endif

