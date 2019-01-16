/*****************************************************************************
*
* Filename:
* ---------
*   bq27531.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq27531 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq27531_SW_H_
#define _bq27531_SW_H_

#define BQ27531_BUSNUM 2

#define bq27531_CMD_CONTROL      			0x00
#define bq27531_CMD_ATRATE					0x02
#define bq27531_CMD_ATTIMETOENMPTY			0x04
#define bq27531_CMD_TEMPERATURE     	 	0x06
#define bq27531_CMD_VOLTAGE      			0x08
#define bq27531_CMD_FLAG      				0x0A
#define bq27531_CMD_NOMICAP      			0x0C
#define bq27531_CMD_FULLCAP      			0x0E
#define bq27531_CMD_REMAINCAP      			0x10
#define bq27531_CMD_CHARGECAP      			0x12
#define bq27531_CMD_AVGCUR      			0x14
#define bq27531_CMD_TIMETOEMPTY      		0x16
#define bq27531_CMD_REMAINCAPUNFILTER   	0x18
#define bq27531_CMD_STANDBYCUR      		0x1A
#define bq27531_CMD_REMAINCAPFILTER     	0x1C
#define bq27531_CMD_PROGCARGINGCUR      	0x1E
#define bq27531_CMD_PROGCARGINGVOL      	0x20
#define bq27531_CMD_FULLCHARGECAPUNFILTER   0x22
#define bq27531_CMD_AVGPOWER      			0x24
#define bq27531_CMD_FULLCHARGECAPFILTER     0x26
#define bq27531_CMD_HEALTH      			0x28
#define bq27531_CMD_CYCLECOUNT      		0x2a
#define bq27531_CMD_STATECHARGE      		0x2c
#define bq27531_CMD_TRUESOC      			0x2e
#define bq27531_CMD_CURREADING      		0x30
#define bq27531_CMD_INTERNALTEMP      		0x32
#define bq27531_CMD_CHARGINGLEVEL      		0x34
#define bq27531_CMD_LEVELCUR      			0x6e
#define bq27531_CMD_CALCCUR      			0x70
#define bq27531_CMD_CALCVOL    				0x72

//control data
#define bq27531_CTRL_STATUS    			0x0000
#define bq27531_CTRL_DEVTYPE   			0x0001
#define bq27531_CTRL_FWVER    			0x0002
#define bq27531_CTRL_HWVER    			0x0003
#define bq27531_CTRL_MACWRITE 			0x0007
#define bq27531_CTRL_ID    				0x0008
#define bq27531_CTRL_BOARDOFFSET		0x0009
#define bq27531_CTRL_OFFSET    			0x000a
#define bq27531_CTRL_OFFSETSAVE    		0x000b
#define bq27531_CTRL_OCVCMD    			0x000c
#define bq27531_CTRL_BATINSERT    		0x000d
#define bq27531_CTRL_BATREMOVE    		0x000e
#define bq27531_CTRL_SETHIB    			0x0011
#define bq27531_CTRL_CLEARHIB    		0x0012
#define bq27531_CTRL_SETSLEEP    		0x0013
#define bq27531_CTRL_CLEARSLEEP    		0x0014
#define bq27531_CTRL_OTGENABLE  		0x0015
#define bq27531_CTRL_OTGDISABLE    		0x0016
#define bq27531_CTRL_DIVCURENABLE 		0x0017
#define bq27531_CTRL_CHGENABLE    		0x001a
#define bq27531_CTRL_CHGDISABLE    		0x001b
#define bq27531_CTRL_CHGCTLENABLE    	0x001c
#define bq27531_CTRL_CHGCTLDISABLE    	0x001d
#define bq27531_CTRL_DIVCURDISABLE   	0x001e
#define bq27531_CTRL_DFVER    			0x001f
#define bq27531_CTRL_SEALED    			0x0020
#define bq27531_CTRL_ITENABLE    		0x0021
#define bq27531_CTRL_RESET    			0x0041
#define bq27531_CTRL_SHIPENABLE    		0x0050
#define bq27531_CTRL_SHIPDISABLE    	0x0051

//charger data
#define bq27531_CHG_STATE    		0x74
#define bq27531_CHG_REG0    		0x75
#define bq27531_CHG_REG1    		0x76
#define bq27531_CHG_REG2    		0x77
#define bq27531_CHG_REG3    		0x78
#define bq27531_CHG_REG4    		0x79
#define bq27531_CHG_REG5    		0x7a
#define bq27531_CHG_REG6    		0x7b
#define bq27531_CHG_REG7    		0x7c
#define bq27531_CHG_REG8    		0x7d
#define bq27531_CHG_REG9    		0x7e
#define bq27531_CHG_REGA    		0x7f

#define bq27531_REG_NUM 30

#define BQ27531_CHIPID 0x531
#define BQ27531_FWVER 0x102
#define BQ27531_DFVER 0x0AA02

#define ERR_UPDATE (1<<0)
#define ERR_CHIPID (1<<1)
#define ERR_FWVER (1<<2)
#define ERR_DFVER (1<<3)



/**********************************************************
  *
  *   [MASK/SHIFT] 
  *
  *********************************************************/
//CON0


/**********************************************************
  *
  *   [Extern Function] 
  *
  *********************************************************/
//r&w----------------------------------------------------
extern int bq27531_read_byte(kal_uint8 cmd, kal_uint8 *returnData);
extern int bq27531_write_byte(kal_uint8 cmd, kal_uint8 writeData);
extern int bq27531_read_bytes(kal_uint8 cmd, kal_uint8 *returnData, kal_uint32 len);
extern int bq27531_write_bytes(kal_uint8 cmd, kal_uint8* writeData, kal_uint32 len);
extern int bq27531_write_single_bytes(kal_uint8 slave_addr, kal_uint8* writeData, kal_uint32 len);

//CON0----------------------------------------------------
extern unsigned int bq27531_get_ctrl_devicetype(void);
extern unsigned int bq27531_get_ctrl_fwver(void);
extern void bq27531_ctrl_enableotg(void);
extern void bq27531_ctrl_disableotg(void);
extern unsigned int bq27531_ctrl_ctrldisablecharge(void);
extern unsigned int bq27531_ctrl_ctrlenablecharge(void);
extern unsigned int bq27531_ctrl_enablecharge(void);
extern unsigned int bq27531_ctrl_disablecharge(void);
extern unsigned int bq27531_get_ctrl_dfver(void);
extern unsigned int bq27531_get_ocv(void);
extern short bq27531_get_instantaneouscurrent(void);
extern short bq27531_get_percengage_of_fullchargercapacity(void);
extern short bq27531_get_remaincap(void);
extern short bq27531_get_averagecurrent(void);
extern void bq27531_set_temperature(kal_int32  temp);
extern kal_int32 bq27531_get_temperature(void);

//other cmd----------------------------------------------------
extern void bq27531_set_charge_voltage(unsigned short vol);
extern short bq27531_get_charge_voltage(void);

extern unsigned int bq27531_enter_rommode(void);
extern unsigned int bq27531_exit_rommode(void);

extern void bq27531_dump_register(void);

extern int bq27531_check_fw_ver(void);

#endif // _bq27531_SW_H_

