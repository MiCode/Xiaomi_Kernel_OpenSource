/*****************************************************************************
*
* Filename:
* ---------
*   bq27541.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq27541 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq27541_SW_H_
#define _bq27541_SW_H_

/**********************************************************
  *
  *   [MASK/SHIFT] 
  *
  *********************************************************/
#define BQ27541_CMD_AtRate  					0x02
#define BQ27541_CMD_AtRateTimeToEmpty  			0x04
#define BQ27541_CMD_Temperature  				0x06
#define BQ27541_CMD_Voltage  					0x08
#define BQ27541_CMD_Flags  						0x0A
#define BQ27541_CMD_NominalAvailableCapacity  	0x0C
#define BQ27541_CMD_FullAvailableCapacity  		0x0E
#define BQ27541_CMD_RemainingCapacity  			0x10
#define BQ27541_CMD_FullChargeCapacity  		0x12
#define BQ27541_CMD_AverageCurrent				0x14
#define BQ27541_CMD_TimeToEmpty					0x16
#define BQ27541_CMD_TimeToFull 					0x18
#define BQ27541_CMD_StandbyCurrent				0x1A
#define BQ27541_CMD_StandbyTimeToEmpty			0x1C
#define BQ27541_CMD_MaxLoadCurrent				0x1E
#define BQ27541_CMD_MaxLoadTimeToEmpty			0x20
#define BQ27541_CMD_AvailableEnergy				0x22
#define BQ27541_CMD_AveragePower				0x24
#define BQ27541_CMD_TimeToEmptyAtConstantPower	0x26
#define BQ27541_CMD_Internal_Temp				0x28
#define BQ27541_CMD_CycleCount					0x2A
#define BQ27541_CMD_StateOfCharge				0x2C
#define BQ27541_CMD_StateOfHealth				0x2E
#define BQ27541_CMD_PassedCharge				0x34
#define BQ27541_CMD_DOD0						0x36

/**********************************************************
  *
  *   [Extern Function] 
  *
  *********************************************************/
extern int bq27541_set_cmd_read(kal_uint8 cmd, int *returnData);
extern int bq27541_set_cmd_write(kal_uint8 cmd, int WriteData);
extern void bq27541_parameter_dump(void);
#endif // _bq27541_SW_H_

