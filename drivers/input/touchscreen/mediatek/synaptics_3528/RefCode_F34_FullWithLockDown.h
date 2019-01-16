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

//#include "ImportedFunctions.h"  //FIXME

enum AttnState
{
  ASSERT,
  DEASSERT
};

#ifdef SYNA_F34_SAMPLE_CODE
  #define EXTERN
  #define FW_REVISION               "DS4 R3.0"
#else
  #define EXTERN extern
#endif

#if 0
EXTERN CCdciApi *  g_ulHandle;

EXTERN unsigned short cdciTarget;
EXTERN unsigned short i2cAddress;
EXTERN ERmiAddress    addressSize;
EXTERN unsigned long  m_uTimeout;
EXTERN bool           m_LineState;
EXTERN void ConfigCommunication();
#endif //0

void CompleteReflash(struct i2c_client *client);
void SynaScanPDT(struct i2c_client *client);
void SynaInitialize(struct i2c_client *client);
void SynaReadFirmwareInfo(struct i2c_client *client);
unsigned int SynaWaitForATTN(struct i2c_client *client, int timeout);
void SynaFinalizeReflash(struct i2c_client *client);
void SynaFlashFirmwareWrite(struct i2c_client *client);
void SynaFlashConfigWrite(struct i2c_client *client);
void SynaProgramFirmware(struct i2c_client *client);
void ConfigBlockReflash(struct i2c_client *client);
void EraseConfigBlock(struct i2c_client *client);
bool CheckTouchControllerType(struct i2c_client *client);
bool CheckFimrwareRevision(struct i2c_client *client);
void SynaEnableFlashing(struct i2c_client *client, bool force);
void eraseAllBlock(struct i2c_client *client);

#if 0
EError readRMI(unsigned short  uRmiAddress,
                               unsigned char * data,
                               unsigned int    length);
EError writeRMI(unsigned short   uRmiAddress, 
                unsigned char  * data,
                unsigned int     length);

#ifdef SYNA_F34_SAMPLE_CODE
EError readRMI( unsigned short  uRmiAddress, //register address
                unsigned char * data,
                unsigned int    length)
{
  EError ret;
  unsigned int lengthRead;
  ret = ReadRegister8(g_ulHandle, 
                      cdciTarget, 
                      i2cAddress, 
                      addressSize, 
                      uRmiAddress, 
                      data, 
                      length, 
                      lengthRead, 
                      m_uTimeout);
  return ret;
}


EError writeRMI(unsigned short   uRmiAddress, 
                unsigned char  * data,
                unsigned int     length)
{
  EError ret;
  unsigned int lengthWritten;
  ret = WriteRegister8( g_ulHandle, 
                        cdciTarget, 
                        i2cAddress, 
                        addressSize, 
                        uRmiAddress, 
                        data, 
                        length, 
                        lengthWritten, 
                        m_uTimeout);
  return ret;
}

#endif
#endif //0
