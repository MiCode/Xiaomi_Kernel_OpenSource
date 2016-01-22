/*
*  stc3117_battery.c 
*  STC3117 fuel-gauge systems for lithium-ion (Li+) batteries
*
*  Copyright (C) 2011 STMicroelectronics.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/i2c/stc3117_battery.h>

#define GG_VERSION "2.03"

/*Function declaration*/
int STC31xx_SetPowerSavingMode(void);
int STC31xx_StopPowerSavingMode(void);
int STC31xx_AlarmSet(void);
int STC31xx_AlarmStop(void);
int STC31xx_AlarmGet(void);
int STC31xx_AlarmClear(void);
int STC31xx_AlarmSetVoltageThreshold(int VoltThresh);
int STC31xx_AlarmSetSOCThreshold(int SOCThresh);
int STC31xx_RelaxTmrSet(int CurrentThreshold);



/* ******************************************************************************** */
/*        STC311x DEVICE SELECTION                                                  */
/* STC3117 version only                                                             */
/* -------------------------------------------------------------------------------- */
//#define STC3115
#define STC3117

#define BATD_UC8
/* ******************************************************************************** */




/* Private define ------------------------------------------------------------*/


/* ******************************************************************************** */
/*        SPECIAL FUNCTIONS                                                         */
/* -------------------------------------------------------------------------------- */
/*                                                                                  */
/* define TEMPCOMP_SOC to enable SOC temperature compensation */
#define TEMPCOMP_SOC

/* ******************************************************************************** */


/* ******************************************************************************** */
/*        INTERNAL PARAMETERS                                                       */
/*   TO BE ADJUSTED ACCORDING TO BATTERY/APPLICATION CHARACTERISTICS                */
/* -------------------------------------------------------------------------------- */
/*                                                                                  */
#define BATT_CHG_VOLTAGE   4250   /* min voltage at the end of the charge (mV)      */
#define BATT_MIN_VOLTAGE   3300   /* nearly empty battery detection level (mV)      */
#define MAX_HRSOC          51200  /* 100% in 1/512% units*/
#define MAX_SOC            1000   /* 100% in 0.1% units */
/*                                                                                  */
#define CHG_MIN_CURRENT     200   /* min charge current in mA                       */
#define CHG_END_CURRENT      20   /* end charge current in mA                       */
#define APP_MIN_CURRENT     (-5)  /* minimum application current consumption in mA ( <0 !) */
#define APP_MIN_VOLTAGE	    3000  /* application cut-off voltage                    */
#define TEMP_MIN_ADJ		(-5)  /* minimum temperature for gain adjustment */

#define VMTEMPTABLE        { 85, 90, 100, 160, 320, 440, 840 }  /* normalized VM_CNF at 60, 40, 25, 10, 0, -10°C, -20°C */

#define AVGFILTER           4  /* average filter constant */

/* ******************************************************************************** */



/* Private define ------------------------------------------------------------*/

#define STC31xx_SLAVE_ADDRESS            0xE0   /* STC31xx 8-bit address byte */

/*Address of the STC311x register --------------------------------------------*/
#define STC311x_REG_MODE                 0x00    /* Mode Register             */
#define STC311x_REG_CTRL                 0x01    /* Control and Status Register */
#define STC311x_REG_SOC                  0x02    /* SOC Data (2 bytes) */
#define STC311x REG_COUNTER              0x04    /* Number of Conversion (2 bytes) */
#define STC311x_REG_CURRENT              0x06    /* Battery Current (2 bytes) */
#define STC311x_REG_VOLTAGE              0x08    /* Battery Voltage (2 bytes) */
#define STC311x_REG_TEMPERATURE          0x0A    /* Temperature               */
#ifdef STC3117
#define STC311x_REG_AVG_CURRENT          0x0B    /* Battery Average Current (2 bytes)   */
#endif
#define STC311x_REG_OCV                  0x0D    /* Battery OCV (2 bytes) */
#define STC311x_REG_CC_CNF               0x0F    /* CC configuration (2 bytes)    */
#define STC311x_REG_VM_CNF               0x11    /* VM configuration (2 bytes)    */
#define STC311x_REG_ALARM_SOC            0x13    /* SOC alarm level         */
#define STC311x_REG_ALARM_VOLTAGE        0x14    /* Low voltage alarm level */
#define STC311x_REG_CURRENT_THRES        0x15    /* Current threshold for relaxation */
#define STC311x_REG_CMONIT_COUNT         0x16    /* Current monitoring counter   */
#define STC311x_REG_CMONIT_MAX           0x17    /* Current monitoring max count */

#define STC311x_REG_CC_ADJ               0x1B    /* CC adjustement (2 bytes)    */
#define STC311x_REG_VM_ADJ               0x1D    /* VM adjustement (2 bytes)    */

/*Bit mask definition*/
#define STC311x_VMODE   		 0x01	 /* Voltage mode bit mask     */
#define STC311x_ALM_ENA			 0x08	 /* Alarm enable bit mask     */
#define STC311x_GG_RUN			 0x10	 /* Alarm enable bit mask     */
#define STC311x_FORCE_CC		 0x20	 /* Force CC bit mask     */
#define STC311x_FORCE_VM		 0x40	 /* Force VM bit mask     */
#define STC311x_SOFTPOR 		 0x11	 /* soft reset     */

#define STC311x_BATD_PU                  0x02  /* Enable internal Pull-Up on BATD bit mask */
#define STC311x_FORCE_CD                 0x04  /* Force CD high bit mask */

#define STC311x_REG_ID                   0x18    /* Chip ID (1 byte)       */
#define STC3117_ID                       0x16    /* STC3117 ID */

#define STC311x_REG_RAM                  0x20    /* General Purpose RAM Registers */
#define RAM_SIZE                         16      /* Total RAM size of STC311x in bytes */

#define STC311x_REG_OCVTAB               0x30
#define OCVTAB_SIZE                      16      /* OCVTAB size of STC311x */

#define VCOUNT				 0       /* counter value for 1st current/temp measurements */


#define M_STAT 0x1010       /* GG_RUN & PORDET mask in STC311x_BattDataTypeDef status word */
#define M_RST  0x1800       /* BATFAIL & PORDET mask */
#define M_RUN  0x0010       /* GG_RUN mask in STC311x_BattDataTypeDef status word */
#define M_GGVM 0x0400       /* GG_VM mask */
#define M_BATFAIL 0x0800    /* BATFAIL mask*/
#define M_UVLOD   0x8000    /* UVLOD mask (STC3117 only) */

#define M_VMOD 0x0001       /* VMODE mask */

#define OK 0

/* Battery charge state definition for BattState */
#define  BATT_CHARGING  3
#define  BATT_ENDCHARG  2
#define  BATT_FULCHARG  1
#define  BATT_IDLE      0
#define  BATT_DISCHARG (-1)
#define  BATT_LOWBATT  (-2)

/* STC311x RAM test word */
#define RAM_TSTWORD 0x53A9

/* Gas gauge states */
#define GG_INIT     'I'
#define GG_RUNNING  'R'
#define GG_POWERDN  'D'

#define VM_MODE 1
#define CC_MODE 0



/* gas gauge structure definition ------------------------------------*/

/* Private constants ---------------------------------------------------------*/

#define NTEMP 7
static const int TempTable[NTEMP] = {60, 40, 25, 10, 0, -10, -20} ;   /* temperature table from 60°C to -20°C (descending order!) */
static const int DefVMTempTable[NTEMP] = VMTEMPTABLE;

/* Private variables ---------------------------------------------------------*/

/* structure of the STC311x battery monitoring parameters */
typedef struct  {
	int Voltage;        /* battery voltage in mV */
	int Current;        /* battery current in mA */
	int Temperature;    /* battery temperature in 0.1°C */
	int SOC;            /* battery relative SOC (%) in 0.1% */
	int OCV;
	int AvgSOC;
	int AvgCurrent;
	int AvgVoltage;
	int AvgTemperature;
	int ChargeValue;    /* remaining capacity in mAh */
	int RemTime;        /* battery remaining operating time during discharge (min) */
	int State;          /* charge (>0)/discharge(<0) state */
	int CalStat;        /* Internal status */
	/* -- parameters -- */
	int Vmode;       /* 1=Voltage mode, 0=mixed mode */
	int Alm_SOC;     /* SOC alm level */
	int Alm_Vbat;    /* Vbat alm level */
	int CC_cnf;      /* nominal CC_cnf */
	int VM_cnf;      /* nominal VM cnf */
	int Cnom;        /* nominal capacity in mAh */
	int Rsense;      /* sense resistor */
	int Rint;         /* battery internal resistance */
	int RelaxCurrent; /* current for relaxation (< C/20) */
	int Adaptive;     /* adaptive mode */
	int CapDerating[7];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10, -20 °C */
	int OCVValue[16];    /* OCV curve values */
	int SOCValue[16];    /* SOC curve values */
	int ExternalTemperature;
	int ForceExternalTemperature;
	int Ropt;  
	int Var1;
} GasGauge_DataTypeDef;

/* structure of the STC311x battery monitoring data */
typedef struct  {
	/* STC311x data */
	int STC_Status;  /* status word  */
	int Vmode;       /* 1=Voltage mode, 0=mixed mode */
	int Voltage;     /* voltage in mV            */
	int Current;     /* current in mA            */
	int Temperature; /* temperature in 0.1°C     */
	int HRSOC;       /* uncompensated SOC in 1/512%   */
	int OCV;         /* OCV in mV*/
	int ConvCounter; /* convertion counter       */
	int RelaxTimer;  /* current relax timer value */
	int CC_adj;      /* CC adj */
	int VM_adj;      /* VM adj */
	/* results & internals */
	int SOC;         /* compensated SOC in 0.1% */
	int AvgSOC;      /* in 0.1% */
	int AvgVoltage;
	int AvgCurrent;
	int AvgTemperature;
	int AccSOC;
	int AccVoltage;
	int AccCurrent;
	int AccTemperature;
	int BattState;
	int GG_Mode;     /* 1=VM active, 0=CC active */
	int LastTemperature;
	int BattOnline;	// BATD
	int IDCode;
	/* parameters */
	int Alm_SOC;     /* SOC alm level in % */
	int Alm_Vbat;    /* Vbat alm level in mV */
	int CC_cnf;      /* nominal CC_cnf */
	int VM_cnf;      /* nominal VM cnf */
	int Cnom;        /* nominal capacity is mAh */
	int Rsense;      /* sense resistor in milliOhms */
	int Rint;        /* internal resistance in milliOhms */
	int CurrentFactor;
	int CRateFactor;
	int RelaxThreshold;   /* current threshold for VM (mA)  */
	int VM_TempTable[NTEMP];
	int CapacityDerating[NTEMP];
	int  OCVValue[OCVTAB_SIZE];
	unsigned char SOCValue[OCVTAB_SIZE];
	int  Ropt;
	int  Nropt;

} STC311x_BattDataTypeDef;

static STC311x_BattDataTypeDef BattData;   /* STC311x data */

/* structure of the STC311x RAM registers for the Gas Gauge algorithm data */
static union {
	unsigned char db[RAM_SIZE];  /* last byte holds the CRC */
	struct {
		short int TstWord;     /* 0-1 */
		short int HRSOC;       /* 2-3 SOC backup */
		short int CC_cnf;      /* 4-5 current CC_cnf */
		short int VM_cnf;      /* 6-7 current VM_cnf */
		char SOC;              /* 8 SOC for trace (in %) */
		char GG_Status;        /* 9  */
		/* bytes ..RAM_SIZE-2 are free, last byte RAM_SIZE-1 is the CRC */
	} reg;
} GG_Ram;


int Capacity_Adjust;


/* -------------------------------------------------------------------------------- */
/*        INTERNAL ANDROID DRIVER PARAMETERS                                        */
/*   TO BE ADJUSTED ACCORDING TO BATTERY/APPLICATION CHARACTERISTICS                */
/* -------------------------------------------------------------------------------- */

#define STC3100_BATTERY_FULL 95
#define STC311x_DELAY	1000  

/* ******************************************************************************** */

static struct i2c_client *sav_client;

struct stc311x_chip 
{
	struct i2c_client		*client;
	struct delayed_work		work;  
	struct power_supply		battery;
	struct stc311x_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* Current */
	int batt_current;
	/* State Of Charge */
	int status;

};

static int stc311x_get_property(
	struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct stc311x_chip *chip = container_of(psy,
	struct stc311x_chip, battery);

	/* from power_supply.h:
	* All voltages, currents, charges, energies, time and temperatures in uV,
	* ÂµA, ÂµAh, ÂµWh, seconds and tenths of degree Celsius unless otherwise
	* stated. It's driver's job to convert its raw values to units in which
	* this class operates.
	*/

	switch (psp) 
	{
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = chip->status;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = chip->online;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = chip->batt_voltage * 1000;  /* in uV */
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			val->intval = chip->batt_current * 1000;  /* in uA */
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			val->intval = chip->batt_soc;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static void stc311x_get_version(struct i2c_client *client)
{
	dev_info(&client->dev, "STC3117 Fuel-Gauge Ver %s\n", GG_VERSION);
}

static void stc311x_get_online(struct i2c_client *client)
{
	struct stc311x_chip *chip = i2c_get_clientdata(client);


	if (chip->pdata && chip->pdata->battery_online)
		chip->online = chip->pdata->battery_online();
	else
		chip->online = BattData.BattOnline;
}

static void stc311x_get_status(struct i2c_client *client)
{
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	if (!chip->pdata || !chip->pdata->charger_online ||
		!chip->pdata->charger_enable) 
	{
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) 
	{
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} 
	else 
	{
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	
	if (chip->batt_soc > STC3100_BATTERY_FULL)
		chip->status = POWER_SUPPLY_STATUS_FULL;
}

/* -------------------------------------------------------------------------- */
/* I2C interface */

/* -----------------------------------------------------------------
The following routines interface with the I2C primitives 
I2C_Read(u8_I2C_address, u8_NumberOfBytes, u8_RegAddress, pu8_RxBuffer);
I2C_Write(u8_I2C_address, u8_NumberOfBytes, u8_RegAddress, pu8_TxBuffer);
note: here I2C_Address is the 8-bit address byte
----------------------------------------------------------------- */

//#define NBRETRY 5 


/*******************************************************************************
* Function Name  : STC31xx_Write
* Description    : utility function to write several bytes to STC311x registers
* Input          : NumberOfBytes, RegAddress, TxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block write. If not available,
* STC311x registers can be written by 2-byte words (unless NumberOfBytes=1)
* or byte per byte.
*******************************************************************************/
static int STC31xx_Write(int length, int reg , unsigned char *values)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(sav_client, reg, length, values);
	if (ret < 0)
		dev_err(&sav_client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

/*******************************************************************************
* Function Name  : STC31xx_Read
* Description    : utility function to read several bytes from STC311x registers
* Input          : NumberOfBytes, RegAddress, , RxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block read. If not available,
* STC311x registers can be read by 2-byte words (unless NumberOfBytes=1)
* Using byte per byte read is not recommended since it doesn't ensure register data integrity
*******************************************************************************/
static int STC31xx_Read(int length, int reg , unsigned char *values)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(sav_client, reg, length, values);
	if (ret < 0)
		dev_err(&sav_client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}



/* ---- end of I2C primitive interface --------------------------------------------- */


/*******************************************************************************
* Function Name  : STC31xx_ReadByte
* Description    : utility function to read the value stored in one register
* Input          : RegAddress: STC311x register,
* Return         : 8-bit value, or 0 if error
*******************************************************************************/
static int STC31xx_ReadByte(int RegAddress)
{
	int value;
	unsigned char data[2];
	int res;

	res=STC31xx_Read(1, RegAddress, data);

	if (res >= 0)
	{
		/* no error */
		value = data[0];
	}
	else
		value=0;

	return(value);
}



/*******************************************************************************
* Function Name  : STC31xx_WriteByte
* Description    : utility function to write a 8-bit value into a register
* Input          : RegAddress: STC311x register, Value: 8-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC31xx_WriteByte(int RegAddress, unsigned char Value)
{
	int res;
	unsigned char data[2];

	data[0]= Value; 
	res = STC31xx_Write(1, RegAddress, data);

	return(res);

}


/*******************************************************************************
* Function Name  : STC31xx_ReadWord
* Description    : utility function to read the value stored in one register pair
* Input          : RegAddress: STC311x register,
* Return         : 16-bit value, or 0 if error
*******************************************************************************/
static int STC31xx_ReadWord(int RegAddress)
{
	int value;
	unsigned char data[2];
	int res;

	res=STC31xx_Read(2, RegAddress, data);

	if (res >= 0)
	{
		/* no error */
		value = data[1];
		value = (value <<8) + data[0];
	}
	else
		value=0;

	return(value);
}


/*******************************************************************************
* Function Name  : STC31xx_WriteWord
* Description    : utility function to write a 16-bit value into a register pair
* Input          : RegAddress: STC311x register, Value: 16-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC31xx_WriteWord(int RegAddress, int Value)
{
	int res;
	unsigned char data[2];

	data[0]= Value & 0xff; 
	data[1]= (Value>>8) & 0xff; 
	res = STC31xx_Write(2, RegAddress, data);

	return(res);

}



/* ---- end of I2C R/W interface --------------------------------------------- */


/* -------------------------------------------------------------------------- */

/* #define CurrentFactor  (24084/SENSERESISTOR)         LSB=5.88uV/R= ~24084/R/4096 - convert to mA  */
#define VoltageFactor  9011                          /* LSB=2.20mV ~9011/4096 - convert to mV         */



/*******************************************************************************
* Function Name  : STC311x_Status
* Description    :  Read the STC311x status
* Input          : None
* Return         : status word (REG_MODE / REG_CTRL), -1 if error
*******************************************************************************/
static int STC311x_Status(void)
{
	int value;

	/* first, check the presence of the STC311x by reading first byte of dev. ID */
	BattData.IDCode = STC31xx_ReadByte(STC311x_REG_ID);
	if (BattData.IDCode!= STC3117_ID) return (-1);

	/* read REG_MODE and REG_CTRL */
	value = STC31xx_ReadWord(STC311x_REG_MODE);

	return (value);
}


/*******************************************************************************
* Function Name  : STC311x_SetParam
* Description    :  initialize the STC311x parameters
* Input          : rst: init algo param
* Return         : 0
*******************************************************************************/
static void STC311x_SetParam(void)
{
	int value;
	int ii;

	STC31xx_WriteByte(STC311x_REG_MODE,0x01);  /*   set GG_RUN=0 before changing algo parameters */

	/* init OCV curve */
	for (ii=0;ii<OCVTAB_SIZE;ii++)
		if (BattData.OCVValue[ii]!=0) STC31xx_WriteWord(STC311x_REG_OCVTAB+ii*2, BattData.OCVValue[ii]*100/55);
	if (BattData.SOCValue[1]!=0) STC31xx_Write(OCVTAB_SIZE, STC311x_REG_OCVTAB+OCVTAB_SIZE*2, (unsigned char *) BattData.SOCValue);

	/* set alm level if different from default */
	if (BattData.Alm_SOC !=0 )   
		STC31xx_WriteByte(STC311x_REG_ALARM_SOC,BattData.Alm_SOC*2); 
	if (BattData.Alm_Vbat !=0 ) 
	{
		value= ((BattData.Alm_Vbat << 9) / VoltageFactor); /* LSB=8*2.44mV */
		STC31xx_WriteByte(STC311x_REG_ALARM_VOLTAGE, value);
	}

	/* relaxation timer */
	if (BattData.RelaxThreshold !=0 )  
	{
		value= ((BattData.RelaxThreshold << 9) / BattData.CurrentFactor);   /* LSB=8*5.88uV/Rsense */
		value = value & 0x7f;
		STC31xx_WriteByte(STC311x_REG_CURRENT_THRES,value); 
	}

	/* set parameters if different from default, only if a restart is done (battery change) */
	if (GG_Ram.reg.CC_cnf !=0 ) STC31xx_WriteWord(STC311x_REG_CC_CNF,GG_Ram.reg.CC_cnf); 
	if (GG_Ram.reg.VM_cnf !=0 ) STC31xx_WriteWord(STC311x_REG_VM_CNF,GG_Ram.reg.VM_cnf); 

	STC31xx_WriteByte(STC311x_REG_CTRL,0x03);  /*   clear PORDET, BATFAIL, free ALM pin, reset conv counter */
	if (BattData.Vmode)
		STC31xx_WriteByte(STC311x_REG_MODE,0x19);  /*   set GG_RUN=1, voltage mode, alm enabled */
	else
		STC31xx_WriteByte(STC311x_REG_MODE,0x18);  /*   set GG_RUN=1, mixed mode, alm enabled */

	return;
}  




/*******************************************************************************
* Function Name  : STC311x_Startup
* Description    :  initialize and start the STC311x at application startup
* Input          : None
* Return         : 0 if ok, -1 if error
*******************************************************************************/
static int STC311x_Startup(void)
{
	int res;
	int ocv, curr;

	/* check STC310x status */
	res = STC311x_Status();
	if (res<0) return(res);

	/* read OCV */
	ocv=STC31xx_ReadWord(STC311x_REG_OCV);

	STC311x_SetParam();  /* set parameters  */

	/* with STC3117, it is possible here to read the current and compensate OCV: */
	curr=STC31xx_ReadWord(STC311x_REG_CURRENT);  
	curr &= 0x3fff;   /* mask unused bits */
	if (curr>=0x2000) curr -= 0x4000;  /* convert to signed value */  
	ocv = ocv - BattData.Rint * curr * 588 / BattData.Rsense / 55000 ;

	/* rewrite ocv to start SOC with updated OCV curve */
	STC31xx_WriteWord(STC311x_REG_OCV,ocv);

	return(0);
}


/*******************************************************************************
* Function Name  : STC311x_Restore
* Description    :  Restore STC311x state
* Input          : None
* Return         : 
*******************************************************************************/
static int STC311x_Restore(void)
{
	int res;
	int ocv;

	/* check STC310x status */
	res = STC311x_Status();
	if (res<0) return(res);

	/* read OCV */
	ocv=STC31xx_ReadWord(STC311x_REG_OCV);

	STC311x_SetParam();  /* set parameters  */

#if 1
	/* if restore from unexpected reset, restore SOC (system dependent) */
	if (GG_Ram.reg.GG_Status == GG_RUNNING)
		if (GG_Ram.reg.SOC != 0)
			STC31xx_WriteWord(STC311x_REG_SOC,GG_Ram.reg.HRSOC);  /*   restore SOC */
#else  
	/* rewrite ocv to start SOC with updated OCV curve */
	STC31xx_WriteWord(STC311x_REG_OCV,ocv);
#endif  
	return(0);
}




/*******************************************************************************
* Function Name  : STC311x_Powerdown
* Description    :  stop the STC311x at application power down
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_Powerdown(void)
{
	int res;

	/* write 0x01 into the REG_CTRL to release IO0 pin open, */
	STC31xx_WriteByte(STC311x_REG_CTRL, 0x01);

	/* write 0 into the REG_MODE register to put the STC311x in standby mode */
	res = STC31xx_WriteByte(STC311x_REG_MODE, 0);
	if (res!= OK) return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STC311x_xxxx
* Description    :  misc STC311x utility functions
* Input          : None
* Return         : None
*******************************************************************************/
static void STC311x_Reset(void)
{
	STC31xx_WriteByte(STC311x_REG_CTRL, STC311x_SOFTPOR);  /*   set soft POR */
}

static void STC311x_SetSOC(int SOC)
{
	STC31xx_WriteWord(STC311x_REG_SOC,SOC);   
}

static void STC311x_ForceVM(void)
{
	int value;

	value=STC31xx_ReadByte(STC311x_REG_MODE);
	STC31xx_WriteByte(STC311x_REG_MODE,value | STC311x_FORCE_VM);   /*   force VM mode */
}

static void STC311x_ForceCC(void)
{
	int value;

	value=STC31xx_ReadByte(STC311x_REG_MODE);
	STC31xx_WriteByte(STC311x_REG_MODE,value | STC311x_FORCE_CC);  /*   force CC mode */   
}



static int STC311x_SaveCnf(void)
{
	int reg_mode,value;

	/* mode register*/
	reg_mode = BattData.STC_Status & 0xff;

	reg_mode &= ~STC311x_GG_RUN;  /*   set GG_RUN=0 before changing algo parameters */
	STC31xx_WriteByte(STC311x_REG_MODE, reg_mode);  

	STC31xx_ReadByte(STC311x_REG_ID);

	STC31xx_WriteWord(STC311x_REG_VM_CNF,GG_Ram.reg.VM_cnf); 
	value = STC31xx_ReadWord(STC311x_REG_SOC); 
	STC31xx_WriteWord(STC311x_REG_SOC,value); 
	STC31xx_WriteWord(STC311x_REG_CC_CNF,GG_Ram.reg.CC_cnf); 

	if (BattData.Vmode)
	{
		STC31xx_WriteByte(STC311x_REG_MODE,0x19);  /*   set GG_RUN=1, voltage mode, alm enabled */
	}
	else
	{
		STC31xx_WriteByte(STC311x_REG_MODE,0x18);  /*   set GG_RUN=1, mixed mode, alm enabled */
		if (BattData.GG_Mode == CC_MODE)
			STC31xx_WriteByte(STC311x_REG_MODE,0x38);  /*   force CC mode */   
		else
			STC31xx_WriteByte(STC311x_REG_MODE,0x58);  /*   force VM mode */
	}

	return(0);
}

static int STC311x_SaveVMCnf(void)
{
	int reg_mode;

	/* mode register*/
	reg_mode = BattData.STC_Status & 0xff;

	reg_mode &= ~STC311x_GG_RUN;  /*   set GG_RUN=0 before changing algo parameters */
	STC31xx_WriteByte(STC311x_REG_MODE, reg_mode);  

	STC31xx_ReadByte(STC311x_REG_ID);

	STC31xx_WriteWord(STC311x_REG_VM_CNF,GG_Ram.reg.VM_cnf); 

	if (BattData.Vmode)
	{
		STC31xx_WriteByte(STC311x_REG_MODE,0x19);  /*   set GG_RUN=1, voltage mode, alm enabled */
	}
	else
	{
		STC31xx_WriteByte(STC311x_REG_MODE,0x18);  /*   set GG_RUN=1, mixed mode, alm enabled */
		if (BattData.GG_Mode == CC_MODE)
			STC31xx_WriteByte(STC311x_REG_MODE,0x38);  /*   force CC mode */   
		else
			STC31xx_WriteByte(STC311x_REG_MODE,0x58);  /*   force VM mode */
	}

	return(0);
}





/*******************************************************************************
* Function Name  : conv
* Description    : conversion utility 
*  convert a raw 16-bit value from STC311x registers into user units (mA, mAh, mV, °C)
*  (optimized routine for efficient operation on 8-bit processors such as STM8)
* Input          : value, factor
* Return         : result = value * factor / 4096
*******************************************************************************/
static int conv(short value, unsigned short factor)
{
	int v;

	v= ( (long) value * factor ) >> 11;
	v= (v+1)/2;

	return (v);
}

/*******************************************************************************
* Function Name  : STC311x_ReadBatteryData
* Description    :  utility function to read the battery data from STC311x
*                  to be called every 5s or so
* Input          : ref to BattData structure
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_ReadBatteryData(STC311x_BattDataTypeDef *BattData)
{
	unsigned char data[16];
	int res;
	int value;

	res=STC311x_Status();
	if (res<0) return(res);  /* return if I2C error or STC3117 not responding */

	/* STC311x status */
	BattData->STC_Status = res;
	if (BattData->STC_Status & M_GGVM)
		BattData->GG_Mode = VM_MODE;   /* VM active */
	else 
		BattData->GG_Mode = CC_MODE;   /* CC active */

	/* read STC3117 registers 0 to 14 */
	res=STC31xx_Read(15, 0, data);

	if (res<0) return(res);  /* read failed */

	/* fill the battery status data */
	/* SOC */
	value=data[3]; value = (value<<8) + data[2];
	BattData->HRSOC = value;     /* result in 1/512% */

	/* conversion counter */
	value=data[5]; value = (value<<8) + data[4];
	BattData->ConvCounter = value;

	/* current */
	value=data[7]; value = (value<<8) + data[6];
	value &= 0x3fff;   /* mask unused bits */
	if (value>=0x2000) value -= 0x4000;  /* convert to signed value */
	BattData->Current = conv(value, BattData->CurrentFactor);  /* result in mA */

	/* voltage */
	value=data[9]; value = (value<<8) + data[8];
	value &= 0x0fff; /* mask unused bits */
	if (value>=0x0800) value -= 0x1000;  /* convert to signed value */
	value = conv(value,VoltageFactor);  /* result in mV */
	BattData->Voltage = value;  /* result in mV */

	/* temperature */
	value=data[10]; 
	if (value>=0x80) value -= 0x100;  /* convert to signed value */
	BattData->Temperature = value*10;  /* result in 0.1°C */

	/* Avg current */
	value=data[12]; value = (value<<8) + data[11];
	if (value>=0x8000) value -= 0x10000;  /* convert to signed value */
	if (BattData->Vmode==0)
	{
		value = conv(value, BattData->CurrentFactor);
		value = value / 4;  /* divide by 4  */
	}
	else
	{
		value = conv(value, BattData->CRateFactor);
	}
	BattData->AvgCurrent = value;  /* result in mA */

	/* OCV */
	value=data[14]; value = (value<<8) + data[13];
	value &= 0x3fff; /* mask unused bits */
	if (value>=0x02000) value -= 0x4000;  /* convert to signed value */
	value = conv(value,VoltageFactor);  
	value = (value+2) / 4;  /* divide by 4 with rounding */
	BattData->OCV = value;  /* result in mV */

	/* read STC3117 registers CC & VM adj */
	res=STC31xx_Read(4, STC311x_REG_CC_ADJ, data);
	if (res<0) return(res);  /* read failed */

	/* CC & VM adjustment counters */
	value=data[1]; value = (value<<8) + data[0];
	if (value>=0x8000) value -= 0x10000;  /* convert to signed value */
	BattData->CC_adj = value; /* in 1/512% */
	value=data[3]; value = (value<<8) + data[2];
	if (value>=0x8000) value -= 0x10000;  /* convert to signed value */
	BattData->VM_adj = value; /* in 1/512% */

	/* relax counter */
	res=STC31xx_Read(1, STC311x_REG_CMONIT_COUNT, data);
	if (res<0) return(res);  /* read failed */
	BattData->RelaxTimer = data[0];


	return(OK);
}


/*******************************************************************************
* Function Name  : STC311x_ReadRamData
* Description    : utility function to read the RAM data from STC311x
* Input          : ref to RAM data array
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_ReadRamData(unsigned char *RamData)
{
	return(STC31xx_Read(RAM_SIZE, STC311x_REG_RAM, RamData));
}


/*******************************************************************************
* Function Name  : STC311x_WriteRamData
* Description    : utility function to write the RAM data into STC311x
* Input          : ref to RAM data array
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_WriteRamData(unsigned char *RamData)
{
	return(STC31xx_Write(RAM_SIZE, STC311x_REG_RAM, RamData));
}

/******************************************************************************* 
* Function Name  : Interpolate
* Description    : interpolate a Y value from a X value and X, Y tables (n points)
* Input          : x
* Return         : y
*******************************************************************************/
static int interpolate(int x, int n, int const *tabx, int const *taby )
{  
	int index;
	int y;

	if (x >= tabx[0])
		y = taby[0];
	else if (x <= tabx[n-1])
		y = taby[n-1];
	else
	{
		/*  find interval */
		for (index= 1;index<n;index++)
			if (x > tabx[index]) break;
		/*  interpolate */
		y = (taby[index-1] - taby[index]) * (x - tabx[index]) * 2 / (tabx[index-1] - tabx[index]);
		y = (y+1) / 2;
		y += taby[index];
	}    
	return y;
}



/*******************************************************************************
* Function Name  : calcCRC8
* Description    : calculate the CRC8
* Input          : data: pointer to byte array, n: number of vytes
* Return         : CRC calue
*******************************************************************************/
static int calcCRC8(unsigned char *data, int n)
{
	int crc=0;   /* initial value */
	int i, j;

	for (i=0;i<n;i++)
	{
		crc ^= data[i];
		for (j=0;j<8;j++) 
		{
			crc <<= 1;
			if (crc & 0x100)  crc ^= 7;
		}
	}
	return(crc & 255);

}


/*******************************************************************************
* Function Name  : UpdateRamCrc
* Description    : calculate the RAM CRC
* Input          : none
* Return         : CRC value
*******************************************************************************/
static int UpdateRamCrc(void)
{
	int res;

	res=calcCRC8(GG_Ram.db,RAM_SIZE-1);
	GG_Ram.db[RAM_SIZE-1] = res;   /* last byte holds the CRC */
	return(res);
}

/*******************************************************************************
* Function Name  : Init_RAM
* Description    : Init the STC311x RAM registers with valid test word and CRC
* Input          : none
* Return         : none
*******************************************************************************/
static void Init_RAM(void)
{
	int index;

	for (index=0;index<RAM_SIZE;index++) 
		GG_Ram.db[index]=0;

	GG_Ram.reg.TstWord=RAM_TSTWORD;  /* id. to check RAM integrity */
	GG_Ram.reg.CC_cnf = BattData.CC_cnf;
	GG_Ram.reg.VM_cnf = BattData.VM_cnf;

	/* update the crc */
	UpdateRamCrc();
}




/* compensate SOC with temperature, SOC in 0.1% units */
static int CompensateSOC(int value, int temp)
{
	int r, v;

	r=0;    
#ifdef TEMPCOMP_SOC
	r=interpolate(temp/10,NTEMP,TempTable,BattData.CapacityDerating);  /* for APP_TYP_CURRENT */
#endif       
	v = (long) (value-r) * MAX_SOC * 2 / (MAX_SOC-r);   /* compensate */
	v = (v+1)/2;  /* rounding */
	if (v < 0) v = 0;
	if (v > MAX_SOC) v = MAX_SOC;

	return(v);
}






/*******************************************************************************
* Function Name  : MM_FSM
* Description    : process the Gas Gauge state machine in mixed mode
* Input          : BattData
* Return         : 
* Affect         : Global Gas Gauge data
*******************************************************************************/
static void MM_FSM(void)
{

	switch (BattData.BattState)
	{
	case BATT_CHARGING:
		if (BattData.AvgCurrent < CHG_MIN_CURRENT)
			BattData.BattState = BATT_ENDCHARG;        /* end of charge */
		break;

	case BATT_ENDCHARG:  /* end of charge state. check if fully charged or charge interrupted */
		if ( BattData.Current > CHG_MIN_CURRENT ) 
			BattData.BattState = BATT_CHARGING;
		else if (BattData.AvgCurrent < CHG_END_CURRENT )
			BattData.BattState = BATT_IDLE;     /* charge interrupted */
		else if ( (BattData.Current > CHG_END_CURRENT ) && ( BattData.Voltage > BATT_CHG_VOLTAGE ) )
			BattData.BattState = BATT_FULCHARG;  /* end of charge */
		break;

	case BATT_FULCHARG:  /* full charge state. wait for actual end of charge current */
		if ( (BattData.Current > CHG_MIN_CURRENT)) 
			BattData.BattState = BATT_CHARGING;  /* charge again */
		else if ( BattData.AvgCurrent < CHG_END_CURRENT ) 
		{
			if ( BattData.AvgVoltage > BATT_CHG_VOLTAGE )
			{
				/* end of charge detected */
				STC311x_SetSOC(MAX_HRSOC);
				BattData.SOC=MAX_SOC;  /* 100% */
			}
			BattData.BattState = BATT_IDLE;     /* end of charge cycle */
		}
		break;

	case BATT_IDLE:  /* no charging, no discharging */
		if (BattData.Current > CHG_END_CURRENT)
		{
			BattData.BattState = BATT_CHARGING; /* charging again */
		}
		else if (BattData.Current < APP_MIN_CURRENT) 
			BattData.BattState = BATT_DISCHARG; /* discharging again */
		break;

	case BATT_DISCHARG:
		if (BattData.Current > APP_MIN_CURRENT) 
			BattData.BattState = BATT_IDLE;
		else if (BattData.AvgVoltage < BATT_MIN_VOLTAGE) 
			BattData.BattState = BATT_LOWBATT;
		break;

	case BATT_LOWBATT:  /* battery nearly empty... */
		if ( BattData.AvgVoltage > (BATT_MIN_VOLTAGE+50) )
			BattData.BattState = BATT_IDLE;   /* idle */
		else
			break;

	default:
		BattData.BattState = BATT_IDLE;   /* idle */

	} /* end switch */


}


static void CompensateVM(int temp)
{
	int r;

#ifdef TEMPCOMP_SOC
	r=interpolate(temp/10,NTEMP,TempTable,BattData.VM_TempTable);
	GG_Ram.reg.VM_cnf = (BattData.VM_cnf * r) / 100;
	STC311x_SaveVMCnf();  /* save new VM cnf values to STC311x */
#endif    
}


/*******************************************************************************
* Function Name  : VM_FSM
* Description    : process the Gas Gauge machine in voltage mode
* Input          : BattData
* Return         : 
* Affect         : Global Gas Gauge data
*******************************************************************************/
static void VM_FSM(void)
{

#define DELTA_TEMP 30   /* 3 °C */

	/* in voltage mode, monitor temperature to compensate voltage mode gain */

	if ( ( BattData.AvgTemperature > (BattData.LastTemperature+DELTA_TEMP)) || 
		( BattData.AvgTemperature < (BattData.LastTemperature-DELTA_TEMP)) )
	{
		BattData.LastTemperature = BattData.AvgTemperature;
		CompensateVM(BattData.AvgTemperature);
	}

}




/*******************************************************************************
* Function Name  : Reset_FSM_GG
* Description    : reset the gas gauge state machine and flags
* Input          : None
* Return         : None
*******************************************************************************/
static void Reset_FSM_GG(void)
{
	BattData.BattState = BATT_IDLE;
}




/* -------------------- Algo functions ------------------------------------------- */


#define OG2

void SOC_correction (GasGauge_DataTypeDef *GG)
{
	int Var1=0;
	int Var2,Var3,Var4;
	int SOCopt;

#ifdef OG2

#define CURRENT_TH  (GG->Cnom/10)  
#define GAIN 10        
#define A_Var3 500    
#define VAR1MAX 64
#define VAR2MAX 128
#define VAR4MAX 128

	if (BattData.SOC>800)  Var3=600;
	else if (BattData.SOC>500) Var3=400;
	else if (BattData.SOC>250) Var3=200;
	else if (BattData.SOC>100) Var3=300;
	else Var3=400;

	Var1= 256*BattData.AvgCurrent*A_Var3/Var3/CURRENT_TH;
	Var1= 32768 * GAIN / (256+Var1*Var1/256) / 10;
	Var1 = (Var1+1)/2;
	if (Var1==0) Var1=1;
	if (Var1>=VAR1MAX) Var1=VAR1MAX-1;
	GG->Var1 = Var1;

	Var4=BattData.CC_adj-BattData.VM_adj;
	if (BattData.GG_Mode == CC_MODE)  
		SOCopt = BattData.HRSOC + Var1 * Var4 / 64;
	else
		SOCopt = BattData.HRSOC - BattData.CC_adj + Var1 * Var4 / 64;

	Var2 = BattData.Nropt;
	if ( (BattData.AvgCurrent < -CURRENT_TH) || (BattData.AvgCurrent > CURRENT_TH) ) 
	{
		if (Var2<VAR2MAX)  Var2++;
		BattData.Ropt = BattData.Ropt + ( 1000 * (BattData.Voltage-BattData.OCV) / BattData.AvgCurrent - BattData.Ropt / Var2);
		BattData.Nropt = Var2;
	}
	if (Var2>0)
		GG->Ropt = BattData.Ropt / Var2;
	else
		GG->Ropt = 0;  // not available

	if (SOCopt <= 0 )
		SOCopt = 0;
	if (SOCopt >= MAX_HRSOC)
		SOCopt = MAX_HRSOC;
	BattData.SOC = (SOCopt*10+256)/512;
	if ( (Var4<(-VAR4MAX)) || (Var4>=VAR4MAX) )
	{
		// rewrite SOCopt into STC311x and clear acc registers
		STC311x_SetSOC(SOCopt); 

	}

#endif

}

/* --------------------------------------------------------------------------------------------- */



/* -------------------- firmware interface functions ------------------------------------------- */




/*******************************************************************************
* Function Name  : GasGauge_Start
* Description    : Start the Gas Gauge system
* Input          : algo parameters in GG structure
* Return         : 0 is ok, -1 if STC310x not found or I2C error
* Affect         : global STC310x data and gas gauge variables
*******************************************************************************/
int GasGauge_Start(GasGauge_DataTypeDef *GG)
{
	int res, i;

	BattData.Cnom = GG->Cnom;
	BattData.Rsense = GG->Rsense;
	BattData.Rint = GG->Rint;
	BattData.Vmode = GG->Vmode;
	BattData.CC_cnf = GG->CC_cnf; 
	BattData.VM_cnf = GG->VM_cnf; 
	BattData.Alm_SOC = GG-> Alm_SOC; 
	BattData.Alm_Vbat = GG->Alm_Vbat; 
	BattData.RelaxThreshold = GG->RelaxCurrent;

	/* Init averaging */
	BattData.AvgVoltage = 0;
	BattData.AvgCurrent = 0;
	BattData.AvgTemperature = 0;
	BattData.AvgSOC = 0;  /* in 0.1% unit  */
	BattData.AccVoltage = 0;
	BattData.AccCurrent = 0;
	BattData.AccTemperature = 0;
	BattData.AccSOC = 0;

	// BATD
	BattData.BattOnline = 1;

	if (BattData.Rsense==0) BattData.Rsense=10;  /* default value in case, to avoid divide by 0 */
	BattData.CurrentFactor=24084/BattData.Rsense;    /* LSB=5.88uV/R= ~24084/R/4096 - convert to mA  */
	BattData.CRateFactor=36*BattData.Cnom;        /* LSB=0.008789.Cnom= 36*Cnom/4096 - convert to mA  */

	if (BattData.CC_cnf==0) BattData.CC_cnf=395;  /* default values */
	if (BattData.VM_cnf==0) BattData.VM_cnf=321;

	for (i=0;i<NTEMP;i++)
		BattData.CapacityDerating[i] = GG->CapDerating[i]; 
	for (i=0;i<OCVTAB_SIZE;i++)
	{
		BattData.OCVValue[i] = GG->OCVValue[i]; 
		BattData.SOCValue[i] = GG->SOCValue[i]; 
	}	
	for (i=0;i<NTEMP;i++)
		BattData.VM_TempTable[i] = DefVMTempTable[i];    

	BattData.Ropt = 0;
	BattData.Nropt = 0;

	/* check RAM valid */
	STC311x_ReadRamData(GG_Ram.db);

	if ( (GG_Ram.reg.TstWord != RAM_TSTWORD) || (calcCRC8(GG_Ram.db,RAM_SIZE)!=0) )
	{
		/* RAM invalid */
		Init_RAM();
		res=STC311x_Startup();  /* return -1 if I2C error or STC3117 not present */
	}
	else
	{
		/* check STC3117 status */
		if ((STC311x_Status() & M_RST) != 0 )
		{
			res=STC311x_Startup();  /* return -1 if I2C error or STC3117 not present */
		}
		else
		{
			res=STC311x_Restore(); /* recover from last SOC */
		}
	}


	GG_Ram.reg.GG_Status = GG_INIT;
	/* update the crc */
	UpdateRamCrc();
	STC311x_WriteRamData(GG_Ram.db);

	Reset_FSM_GG();

	return(res);    /* return -1 if I2C error or STC3117 not present */
}





/*******************************************************************************
Restart sequence:
Usage: 
call GasGaugeReset()
powerdown everything
wait 500ms
call GasGaugeStart(GG)
continue 
*******************************************************************************/


/*******************************************************************************
* Function Name  : GasGauge_Reset
* Description    : Reset the Gas Gauge system
* Input          : None
* Return         : 0 is ok, -1 if I2C error
*******************************************************************************/
void GasGauge_Reset(void)  
{
	GG_Ram.reg.TstWord=0;  /* reset RAM */
	GG_Ram.reg.GG_Status = 0;
	STC311x_WriteRamData(GG_Ram.db);

	STC311x_Reset();
}



/*******************************************************************************
* Function Name  : GasGauge_Stop
* Description    : Stop the Gas Gauge system
* Input          : None
* Return         : 0 is ok, -1 if I2C error
*******************************************************************************/
int GasGauge_Stop(void)
{
	int res;

	STC311x_ReadRamData(GG_Ram.db);
	GG_Ram.reg.GG_Status= GG_POWERDN;
	/* update the crc */
	UpdateRamCrc();
	STC311x_WriteRamData(GG_Ram.db);

	res=STC311x_Powerdown();
	if (res!=0) return (-1);  /* error */

	return(0);  
}



/*******************************************************************************
* Function Name  : GasGauge_Task
* Description    : Periodic Gas Gauge task, to be called e.g. every 5 sec.
* Input          : pointer to gas gauge data structure
* Return         : 1 if data available, 0 if no data, -1 if error
* Affect         : global STC310x data and gas gauge variables
*******************************************************************************/
int GasGauge_Task(GasGauge_DataTypeDef *GG)
{
	int res, value;
	BattData.Cnom = GG->Cnom;
	BattData.Rsense = GG->Rsense;
	BattData.Vmode = GG->Vmode;
	BattData.Rint = GG->Rint;
	BattData.CC_cnf = GG->CC_cnf;
	BattData.VM_cnf = GG->VM_cnf;
	BattData.Alm_SOC = GG-> Alm_SOC; 
	BattData.Alm_Vbat = GG->Alm_Vbat; 
	BattData.RelaxThreshold = GG->RelaxCurrent;

	res=STC311x_ReadBatteryData(&BattData);  /* read battery data into global variables */
	if (res!=0) return(-1); /* abort in case of I2C failure */

	/* check if RAM data is ok (battery has not been changed) */
	STC311x_ReadRamData(GG_Ram.db);
	if ( (GG_Ram.reg.TstWord!= RAM_TSTWORD) || (calcCRC8(GG_Ram.db,RAM_SIZE)!=0) )
	{
		/* if RAM non ok, reset it and set init state */
		Init_RAM(); 
		GG_Ram.reg.GG_Status = GG_INIT;
	}    

	//Check battery presence
	if ((BattData.STC_Status & M_BATFAIL) != 0)
	{
		BattData.BattOnline = 0; 
	}
	/* check STC3117 status */
#ifdef BATD_UC8
	/* check STC3117 status */
	if ((BattData.STC_Status & (M_BATFAIL | M_UVLOD) != 0)
	{
		/* BATD or UVLO detected */
		if(BattData.ConvCounter > 0)
		{
			GG->Voltage=BattData.Voltage;
			GG->SOC=(BattData.HRSOC*10+256)/512;
		}

		/* BATD or UVLO detected */
		GasGauge_Reset();

		return (-1);
	}
#endif

	if ((BattData.STC_Status & M_RUN) == 0)
	{
		/* if not running, restore STC3117 */
		STC311x_Restore();  
		GG_Ram.reg.GG_Status = GG_INIT;
	}

	BattData.SOC = (BattData.HRSOC*10+256)/512;  /* in 0.1% unit  */

	//Force an external temperature
	if(GG->ForceExternalTemperature == 1)
		BattData.Temperature = GG->ExternalTemperature;

	/* check INIT state */
	if (GG_Ram.reg.GG_Status == GG_INIT)
	{
		/* INIT state, wait for current & temperature value available: */
		if (BattData.ConvCounter>VCOUNT) 
		{
			/* update VM_cnf */
			CompensateVM(BattData.Temperature);
			BattData.LastTemperature=BattData.Temperature;

			/* Init averaging */
			BattData.AvgVoltage = BattData.Voltage;
			BattData.AvgCurrent = BattData.Current;
			BattData.AvgTemperature = BattData.Temperature;
			BattData.AvgSOC = CompensateSOC(BattData.SOC,BattData.Temperature);  /* in 0.1% unit  */
			BattData.AccVoltage = BattData.AvgVoltage*AVGFILTER;
			BattData.AccCurrent = BattData.AvgCurrent*AVGFILTER;
			BattData.AccTemperature = BattData.AvgTemperature*AVGFILTER;
			BattData.AccSOC = BattData.AvgSOC*AVGFILTER;

			GG_Ram.reg.GG_Status = GG_RUNNING;
		}
	}


	if (GG_Ram.reg.GG_Status != GG_RUNNING)
	{
		GG->SOC = CompensateSOC(BattData.SOC,250);
		GG->Voltage=BattData.Voltage;
		GG->OCV = BattData.OCV;
		GG->Current=0;
		GG->RemTime = -1;   /* means no estimated time available */
		GG->Temperature=250;
	}
	else
	{
		//Check battery presence
		if ((BattData.STC_Status & M_BATFAIL) == 0)
		{
			BattData.BattOnline = 1; 
		}

		SOC_correction (GG);
		/* SOC derating with temperature */
		BattData.SOC = CompensateSOC(BattData.SOC,BattData.Temperature);

		//early empty compensation
		value=BattData.AvgVoltage;
		if (BattData.Voltage < value) value = BattData.Voltage;
		if (value<(APP_MIN_VOLTAGE+200) && value>(APP_MIN_VOLTAGE-500))
		{
			if (value<APP_MIN_VOLTAGE) 
				BattData.SOC=0;
			else
				BattData.SOC = BattData.SOC * (value - APP_MIN_VOLTAGE) / 200;
		}

		BattData.AccVoltage += (BattData.Voltage - BattData.AvgVoltage);
		BattData.AccCurrent += (BattData.Current - BattData.AvgCurrent);
		BattData.AccTemperature += (BattData.Temperature - BattData.AvgTemperature);
		BattData.AccSOC +=  (BattData.SOC - BattData.AvgSOC);

		BattData.AvgVoltage = (BattData.AccVoltage+AVGFILTER/2)/AVGFILTER;
		BattData.AvgTemperature = (BattData.AccTemperature+AVGFILTER/2)/AVGFILTER;
		BattData.AvgSOC = (BattData.AccSOC+AVGFILTER/2)/AVGFILTER;

		/* ---------- process the Gas Gauge algorithm -------- */

		if (BattData.Vmode) 
			VM_FSM();  /* in voltage mode */
		else
			MM_FSM();  /* in mixed mode */

		if (BattData.Vmode==0) 
		{
			// Lately fully compensation
			if(BattData.AvgCurrent > 0 && BattData.SOC >= 990 && BattData.SOC < 995 && BattData.AvgCurrent > 100)
			{
				BattData.SOC = 990;
				STC311x_SetSOC(99*512);
			}
			// Lately empty compensation
			if(BattData.AvgCurrent < 0 && BattData.SOC >= 15 && BattData.SOC < 20 && BattData.Voltage > (APP_MIN_VOLTAGE+50))
			{
				BattData.SOC = 20;
				STC311x_SetSOC(2*512);
			}
		}


		/* -------- APPLICATION RESULTS ------------ */

		/* fill gas gauge data with battery data */
		GG->Voltage=BattData.Voltage;
		GG->Current=BattData.Current;
		GG->Temperature=BattData.Temperature;
		GG->SOC = BattData.SOC;
		GG->OCV = BattData.OCV;

		GG->AvgVoltage = BattData.AvgVoltage;
		GG->AvgCurrent = BattData.AvgCurrent;
		GG->AvgTemperature = BattData.AvgTemperature;
		GG->AvgSOC = BattData.AvgSOC;

		if (BattData.Vmode) 
		{
			/* no current value in voltage mode */
			GG->Current = 0;
			GG->AvgCurrent = 0;
		}

		GG->ChargeValue = (long) BattData.Cnom * BattData.AvgSOC / MAX_SOC;
		if (GG->AvgCurrent<APP_MIN_CURRENT)
		{
			GG->State=BATT_DISCHARG;
			value = GG->ChargeValue * 60 / (-GG->AvgCurrent);  /* in minutes */
			if (value<0) value=0;
			GG->RemTime = value; 
		}
		else 
		{
			GG->RemTime = -1;   /* means no estimated time available */
			if (GG->AvgCurrent>CHG_END_CURRENT)
				GG->State=BATT_CHARGING;
			else
				GG->State=BATT_IDLE;
		}
	}

	/* save SOC */
	GG_Ram.reg.HRSOC = BattData.HRSOC;
	GG_Ram.reg.SOC = (GG->SOC+5)/10;    /* trace SOC in % */
	UpdateRamCrc();
	STC311x_WriteRamData(GG_Ram.db);

	if (GG_Ram.reg.GG_Status==GG_RUNNING)
		return(1);
	else
		return(0);  /* only SOC, OCV and voltage are valid */
}




/*******************************************************************************
* Function Name  : STC31xx_SetPowerSavingMode
* Description    :  Set the power saving mode
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_SetPowerSavingMode(void)
{
	int res;

	/* Read the mode register*/
	res = STC31xx_ReadByte(STC311x_REG_MODE);

	/* Set the VMODE bit to 1 */
	res = STC31xx_WriteByte(STC311x_REG_MODE, (res | STC311x_VMODE));
	if (res!= OK) return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STC31xx_StopPowerSavingMode
* Description    :  Stop the power saving mode
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_StopPowerSavingMode(void)
{
	int res;

	/* Read the mode register*/
	res = STC31xx_ReadByte(STC311x_REG_MODE);

	/* Set the VMODE bit to 0 */
	res = STC31xx_WriteByte(STC311x_REG_MODE, (res & ~STC311x_VMODE));
	if (res!= OK) return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STC31xx_AlarmSet
* Description    :  Set the alarm function and set the alarm threshold
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_AlarmSet(void)
{
	int res;

	/* Read the mode register*/
	res = STC31xx_ReadByte(STC311x_REG_MODE);

	/* Set the ALM_ENA bit to 1 */
	res = STC31xx_WriteByte(STC311x_REG_MODE, (res | STC311x_ALM_ENA));
	if (res!= OK) return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STC31xx_AlarmStop
* Description    :  Stop the alarm function
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_AlarmStop(void)
{
	int res;

	/* Read the mode register*/
	res = STC31xx_ReadByte(STC311x_REG_MODE);

	/* Set the ALM_ENA bit to 0 */
	res = STC31xx_WriteByte(STC311x_REG_MODE, (res & ~STC311x_ALM_ENA));
	if (res!= OK) return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STC31xx_AlarmGet
* Description    : Return the ALM status
* Input          : None
* Return         : ALM status 00 : no alarm 
*                             01 : SOC alarm
*                             10 : Voltage alarm
*                             11 : SOC and voltage alarm
*******************************************************************************/
int STC31xx_AlarmGet(void)
{
	int res;

	/* Read the mode register*/
	res = STC31xx_ReadByte(STC311x_REG_CTRL);
	res = res >> 5;

	return (res);
}


/*******************************************************************************
* Function Name  : STC31xx_AlarmClear
* Description    :  Clear the alarm signal
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_AlarmClear(void)
{
	int res;

	/* clear ALM bits*/
	res = STC31xx_WriteByte(STC311x_REG_CTRL, 0x01);
	if (res!= OK) return (res);

	return (res);
}


/*******************************************************************************
* Function Name  : STC31xx_AlarmSetVoltageThreshold
* Description    : Set the alarm threshold
* Input          : int voltage threshold
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_AlarmSetVoltageThreshold(int VoltThresh)
{
	int res;
	int value;

	BattData.Alm_Vbat =VoltThresh;

	value= ((BattData.Alm_Vbat << 9) / VoltageFactor); /* LSB=8*2.44mV */
	res = STC31xx_WriteByte(STC311x_REG_ALARM_VOLTAGE, value);
	if (res!= OK) return (res);

	return (OK);
}




/*******************************************************************************
* Function Name  : STC31xx_AlarmSetSOCThreshold
* Description    : Set the alarm threshold
* Input          : int voltage threshold
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_AlarmSetSOCThreshold(int SOCThresh)
{
	int res;

	BattData.Alm_SOC = SOCThresh;
	res = STC31xx_WriteByte(STC311x_REG_ALARM_SOC, BattData.Alm_SOC*2);
	if (res!= OK) return (res);

	return (OK);
}




/*******************************************************************************
* Function Name  : STC31xx_RelaxTmrSet
* Description    :  Set the current threshold register to the passed value in mA
* Input          : int current threshold
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_RelaxTmrSet(int CurrentThreshold)
{
	int res, value;

	BattData.RelaxThreshold = CurrentThreshold;
	if (BattData.CurrentFactor!=0) 
	{
		value= ((BattData.RelaxThreshold << 9) / BattData.CurrentFactor);   /* LSB=8*5.88uV/Rsense */
		value = value & 0x7f;
		res=STC31xx_WriteByte(STC311x_REG_CURRENT_THRES,value);     
		if (res!= OK) return (res);
	}

	return (OK);
}

/*******************************************************************************
* Function Name  : STC31xx_ForceCC
* Description    :  Force the CC mode for CC eval
* Input          : 
* Return         : error status (OK, !OK)
*******************************************************************************/
int STC31xx_ForceCC(void)
{
	STC311x_ForceCC();

	return (OK);
}

/*******************************************************************************
* Function Name  : STC311x_ForceCD_high
* Description    :  Force the CD pin to high level (charger disable)
* Input          : 
* Return         : 
*******************************************************************************/
static void STC311x_ForceCD_high(void)
{
	int value;

	value=STC31xx_ReadByte(STC311x_REG_MODE);
	STC31xx_WriteByte(STC311x_REG_MODE,value | STC311x_FORCE_CD);   /*   force CD high */
}

/*******************************************************************************
* Function Name  : STC311x_ForceCD_low
* Description    :  Force the CD pin to low level (charger enable)
* Input          : 
* Return         : 
*******************************************************************************/
static void STC311x_ForceCD_low(void)
{
	int value;

	value=STC31xx_ReadByte(STC311x_REG_MODE);
	STC31xx_WriteByte(STC311x_REG_MODE,value & (~STC311x_FORCE_CD));   /*   force CD low */
}


/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------- */



static void stc311x_work(struct work_struct *work)
{
	struct stc311x_chip *chip;
	GasGauge_DataTypeDef GasGaugeData;
	int res,Loop;

	chip = container_of(work, struct stc311x_chip, work.work);

	sav_client = chip->client;

	if (chip->pdata)
	{
		GasGaugeData.Vmode = chip->pdata->Vmode;       /* 1=Voltage mode, 0=mixed mode */
		GasGaugeData.Alm_SOC = chip->pdata->Alm_SOC;     /* SOC alm level %*/
		GasGaugeData.Alm_Vbat = chip->pdata->Alm_Vbat;    /* Vbat alm level mV*/
		GasGaugeData.CC_cnf = chip->pdata->CC_cnf;      /* nominal CC_cnf */
		GasGaugeData.VM_cnf = chip->pdata->VM_cnf;      /* nominal VM cnf */
		GasGaugeData.Rint = chip->pdata->Rint;			/* nominal Rint */
		GasGaugeData.Cnom = chip->pdata->Cnom;        /* nominal capacity in mAh */
		GasGaugeData.Rsense = chip->pdata->Rsense;      /* sense resistor mOhms*/
		GasGaugeData.RelaxCurrent = chip->pdata->RelaxCurrent; /* current for relaxation in mA (< C/20) */
		GasGaugeData.Adaptive = chip->pdata->Adaptive;     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
		/* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 °C */
		for(Loop=0;Loop<NTEMP;Loop++)
			GasGaugeData.CapDerating[Loop] = chip->pdata->CapDerating[Loop];   
		/* OCV curve adjustment */
		for(Loop=0;Loop<16;Loop++)
			GasGaugeData.OCVValue[Loop] = chip->pdata->OCVValue[Loop];    
		GasGaugeData.ExternalTemperature = chip->pdata->ExternalTemperature(); /*External temperature fonction, return °C*/
		GasGaugeData.ForceExternalTemperature = chip->pdata->ForceExternalTemperature; /* 1=External temperature, 0=STC3117 temperature */
	}

	res=GasGauge_Task(&GasGaugeData);  /* process gas gauge algorithm, returns results */
	if (res>0) 
	{
		/* results available */
		chip->batt_soc = (GasGaugeData.SOC+5)/10;
		chip->batt_voltage = GasGaugeData.Voltage;
		chip->batt_current = GasGaugeData.Current;
	}
	else if(res == -1)
	{
		chip->batt_voltage = GasGaugeData.Voltage;
		chip->batt_soc = (GasGaugeData.SOC+5)/10;
	}

	stc311x_get_status(sav_client);
	stc311x_get_online(sav_client);

	schedule_delayed_work(&chip->work, STC311x_DELAY);
}


static enum power_supply_property stc311x_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,  
	POWER_SUPPLY_PROP_CAPACITY,
};



static int __devinit stc311x_probe(struct i2c_client *client,
								   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct stc311x_chip *chip;
	int ret,res,Loop;

	GasGauge_DataTypeDef GasGaugeData;

	/*First check the functionality supported by the host*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_I2C_BLOCK))
		return -EIO;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_I2C_BLOCK))
		return -EIO;

	/*OK. For now, we presume we have a valid client. We now create the
	client structure*/
	chip = kzalloc(sizeof(struct stc311x_chip), GFP_KERNEL);
	if (!chip)
	{
		printk("Out of memory to create client structure for stc311x\n");
		return -ENOMEM;  /*Out of memory*/
	}


	printk("\n\nstc311x probe started\n\n");

	/* The common I2C client data is placed right specific data. */
	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= stc311x_get_property;
	chip->battery.properties	= stc311x_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(stc311x_battery_props);

	if (chip->pdata && chip->pdata->power_supply_register)
		ret = chip->pdata->power_supply_register(&client->dev, &chip->battery);
	else
		ret = power_supply_register(&client->dev, &chip->battery);
	if (ret)
	{
		//dev_err(&client->dev, "failed: power supply register\n");

		kfree(chip);
		return ret;
	}

	dev_info(&client->dev, "power supply register,%d\n",ret);


	stc311x_get_version(client);

	/* init gas gauge system */
	sav_client = chip->client;

	if (chip->pdata)
	{
		GasGaugeData.Vmode = chip->pdata->Vmode;       /* 1=Voltage mode, 0=mixed mode */
		GasGaugeData.Alm_SOC = chip->pdata->Alm_SOC;     /* SOC alm level %*/
		GasGaugeData.Alm_Vbat = chip->pdata->Alm_Vbat;    /* Vbat alm level mV*/
		GasGaugeData.CC_cnf = chip->pdata->CC_cnf;      /* nominal CC_cnf */
		GasGaugeData.VM_cnf = chip->pdata->VM_cnf;      /* nominal VM cnf */
		GasGaugeData.Rint = chip->pdata->Rint;			/* nominal Rint */
		GasGaugeData.Cnom = chip->pdata->Cnom;        /* nominal capacity in mAh */
		GasGaugeData.Rsense = chip->pdata->Rsense;      /* sense resistor mOhms*/
		GasGaugeData.RelaxCurrent = chip->pdata->RelaxCurrent; /* current for relaxation in mA (< C/20) */
		GasGaugeData.Adaptive = chip->pdata->Adaptive;     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
		/* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 °C */
		for(Loop=0;Loop<NTEMP;Loop++)
			GasGaugeData.CapDerating[Loop] = chip->pdata->CapDerating[Loop];   
		/* OCV curve adjustment */
		for(Loop=0;Loop<16;Loop++)
			GasGaugeData.OCVValue[Loop] = chip->pdata->OCVValue[Loop];    
		GasGaugeData.ExternalTemperature = chip->pdata->ExternalTemperature(); /*External temperature fonction, return °C*/
		GasGaugeData.ForceExternalTemperature = chip->pdata->ForceExternalTemperature; /* 1=External temperature, 0=STC3117 temperature */
	}

	GasGauge_Start(&GasGaugeData);
	msleep(200);
	res=GasGauge_Task(&GasGaugeData);  /* process gas gauge algorithm, returns results */
	if (res>0) 
	{
		/* results available */
		chip->batt_soc = (GasGaugeData.SOC+5)/10;
		chip->batt_voltage = GasGaugeData.Voltage;
		chip->batt_current = GasGaugeData.Current;
	}
	else if(res==0)
	{
		/* SOC and Voltage  available */
		chip->batt_soc = (GasGaugeData.SOC+5)/10;
		chip->batt_voltage = GasGaugeData.Voltage;
		chip->batt_current = 0;
	}
	else if(res == -1)
	{
		chip->batt_voltage = GasGaugeData.Voltage;
		chip->batt_soc = (GasGaugeData.SOC+5)/10;
	}


	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, stc311x_work);

	schedule_delayed_work(&chip->work, STC311x_DELAY); 
	//The specified delay depends of every platform and Linux kernel. It has to be checked physically during the driver integration
	//a delay of about 5 seconds is correct but 30 seconds is enough compare to the battery SOC evolution speed

	return 0;

}



static int __devexit stc311x_remove(struct i2c_client *client)
{
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	/* stop gas gauge system */
	sav_client = chip->client;
	GasGauge_Stop();

	if (chip->pdata && chip->pdata->power_supply_unregister)
		chip->pdata->power_supply_unregister(&chip->battery);
	else
		power_supply_unregister(&chip->battery);
	cancel_delayed_work(&chip->work);
	kfree(chip);

	return 0;
}



#ifdef CONFIG_PM

static int stc311x_suspend(struct i2c_client *client,
						   pm_message_t state)
{
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
	return 0;
}

static int stc311x_resume(struct i2c_client *client)
{
	struct stc311x_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, 0);
	return 0;
}

#else

#define stc311x_suspend NULL
#define stc311x_resume NULL

#endif /* CONFIG_PM */






/* Every chip have a unique id */
static const struct i2c_device_id stc311x_id[] = {
	{ "stc3117", 0 },
	{ }
};

/* Every chip have a unique id and we need to register this ID using MODULE_DEVICE_TABLE*/
MODULE_DEVICE_TABLE(i2c, stc311x_id);

static struct i2c_driver stc311x_i2c_driver = {
	.driver	= {
		.name	= "stc3117",
	},
	.probe		= stc311x_probe,
	.remove		= __devexit_p(stc311x_remove),
	.suspend	= stc311x_suspend,    
	.resume		= stc311x_resume,  
	.id_table	= stc311x_id,
};

/*To register this I2C chip driver, the function i2c_add_driver should be called 
with a pointer to the struct i2c_driver*/
static int __init stc311x_init(void)
{
	return i2c_add_driver(&stc311x_i2c_driver);
}
module_init(stc311x_init);

/*To unregister the I2C chip driver, the i2c_del_driver function should be called
with the same pointer to the struct i2c_driver*/
static void __exit stc311x_exit(void)
{
	i2c_del_driver(&stc311x_i2c_driver);
}
module_exit(stc311x_exit);

MODULE_AUTHOR("STMICROELECTRONICS");
MODULE_DESCRIPTION("STC311x Fuel Gauge");
MODULE_LICENSE("GPL");


