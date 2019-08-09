/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
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

/*
 * CEREUS_DW9714AF_OFILM voice coil motor driver vendor V2
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#define AF_DRVNAME "CEREUS_DW9714AF_OFILM_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;
#define MAX_NOSIE_POSITION (250) //need test 
#define MAX_SMOOTH_NOSIE_STEP  (50)    //need test
static int g_AF_InitStated = 0;
static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuffAddress=0x03;
	char pBuff[2];

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, &pBuffAddress, 1);
	if (i4RetValue < 0) {
		LOG_INF("I2C read failed 1!!\n");
		return -1;
	}
	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16)pBuff[0]) << 8) + (pBuff[1]);

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[3] = {0x03,
						(char)(a_u2Data >> 8),
			     		(char)((a_u2Data & 0xFF))};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}
typedef struct {
	char cmd[2];
	int  delayMs;
}af_init_config;

static inline  int AF_setconfig1(void)
{
    int  Ret = 0;
	int i = 0;
	af_init_config puSendCmd[3] ={
		{{(char)(0xED) , (char)(0xAB)}, 0},
		{{(char)(0x02) , (char)(0x01)}, 0},
		{{(char)(0x02) , (char)(0x00)}, 1},
	} ;
	if(g_AF_InitStated == 0){
	    LOG_INF("AF_setconfig1 \n");
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
		for(i = 0 ;i < 3 ; i++){
		    	Ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd[i].cmd, 2);
				LOG_INF("puSendCmd[%d]=(0x%x,0x%x)\n",i,puSendCmd[i].cmd[0],puSendCmd[i].cmd[1]);
			    if (Ret < 0)
			    {
			        LOG_INF("puSendCmd2 send failed.\n");
					break;
			    }
				if(puSendCmd[i].delayMs)
			    	msleep(puSendCmd[i].delayMs);	
		}
		spin_lock(g_pAF_SpinLock);
		g_AF_InitStated = 1;
		spin_unlock(g_pAF_SpinLock);
    }
    return 0;
}
static inline int AF_setconfig2(void)
{
    int  Ret = 0;
	int i = 0;
	af_init_config puSendCmd[3] ={		
		{{(char)(0x06) , (char)(0x84)}, 0},
		{{(char)(0x07) , (char)(0x01)}, 0},
		{{(char)(0x08) , (char)(0x55)}, 0},	
	} ;
    if(g_AF_InitStated == 1){
	    LOG_INF("AF_setconfig2 \n");
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
		for(i = 0 ;i < 3 ; i++){
		    	Ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd[i].cmd, 2);
				LOG_INF("puSendCmd[%d]=(0x%x,0x%x)\n",i,puSendCmd[i].cmd[0],puSendCmd[i].cmd[1]);
			    if (Ret < 0)
			    {
			        LOG_INF("puSendCmd2 send failed.\n");
					break;
			    }
				if(puSendCmd[i].delayMs)
			    	msleep(puSendCmd[i].delayMs);	
		}
		spin_lock(g_pAF_SpinLock);
		g_AF_InitStated = 2;
		spin_unlock(g_pAF_SpinLock);
    }
    return 0;
}
static inline int AFExitConfig(void){
	int  Ret = 0;
	char puSendCmd2[2] = {(char)(0x06) , (char)(0x83)};
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;	
    Ret = i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 2);
    if (Ret < 0)
    {
        LOG_INF("puSendCmd2 send failed.\n");
    }	
	return Ret;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;
		AF_setconfig1();
		ret = s4AF_ReadReg(&InitPos);

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	if(g_AF_InitStated == 1){
        AF_setconfig2();
	}
	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long CEREUS_DW9714AF_OFILM_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}
static  inline  void release_af_smooth(unsigned long curPositon){
    int tarCurposition = (int)curPositon;
	if(tarCurposition > MAX_NOSIE_POSITION){
		tarCurposition = MAX_NOSIE_POSITION;
		s4AF_WriteReg((unsigned short)tarCurposition);
	}
	AFExitConfig();
	while((tarCurposition > 0)){
		  tarCurposition = tarCurposition - MAX_SMOOTH_NOSIE_STEP; 	 
		  LOG_INF("move to target_positon = %d\n",tarCurposition);
		  if((tarCurposition < MAX_SMOOTH_NOSIE_STEP))
		      tarCurposition = 0;
		  s4AF_WriteReg((unsigned short)tarCurposition); 
		  msleep(13);
	}
	if(g_AF_InitStated == 2){
	    spin_lock(g_pAF_SpinLock);
	    g_AF_InitStated = 0;
	    spin_unlock(g_pAF_SpinLock);
	}
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int CEREUS_DW9714AF_OFILM_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
        LOG_INF("Wait\n");
		//s4AF_WriteReg(0x80); /* Power down mode */
		release_af_smooth(g_u4CurrPosition);
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int CEREUS_DW9714AF_OFILM_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}

int CEREUS_DW9714AF_OFILM_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}

