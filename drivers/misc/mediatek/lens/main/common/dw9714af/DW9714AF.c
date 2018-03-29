/*
 * DW9714AF voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"


#define AF_DRVNAME "DW9714AF_DRV"
/* Because ID Pin is Low, so set 0x1C, otherwise, 0x18; Actually, the lens name is dw9763, now we use dw9714 for code reused*/
#define AF_I2C_SLAVE_ADDR        0x1C

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
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


static int s4AF_ReadReg(u8 addr, unsigned char *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[1] = {(char)addr};
	char value = 0xEF;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, pBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -EINVAL;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, &value, 1);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -EINVAL;
	}

	*a_pu2Result = value;

	return 0;
}

/* don't use this now */
#if 0
static int s4AF_WriteReg(u8 addr, u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = { (char)0x03, (char)(a_u2Data >> 8)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -EINVAL;
	}

	puSendCmd[0] = (char)0x04;
	puSendCmd[1] = (char)(a_u2Data & 0xFF);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -EINVAL;
	}

	return 0;
}
#endif

static int s4AF_SetPosition(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[2] = { (char)0x03, (char)(a_u2Data >> 8)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -EINVAL;
	}

	puSendCmd[0] = (char)0x04;
	puSendCmd[1] = (char)(a_u2Data & 0xFF);
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -EINVAL;
	}

	return 0;
}


static inline int getAFInfo(__user stAF_MotorInfo * pstMotorInfo)
{
	stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
	unsigned char value;
	/*static int first_moveAF = 0;*/
	static int first_moveAF;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		/*
		unsigned char InitPos;

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
		*/
		char puSendCmd1[2] = { 0xEC, 0xA3 };
		char puSendCmd2[2] = { 0xA1, 0x0D };
		char puSendCmd3[2] = { 0xF2, 0x00 };
		char puSendCmd4[2] = { 0xDC, 0x51 };
		g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;
		g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd1, 2);
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 2);
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd3, 2);
		i2c_master_send(g_pstAF_I2Cclient, puSendCmd4, 2);
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = 0;
		spin_unlock(g_pAF_SpinLock);

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	if (first_moveAF == 0) {
		value = 0xFF;
		ret = s4AF_ReadReg(0x00, &value);
		if (ret == 0) {
			LOG_INF("IcInfo: %4d\n", value);
		}
		first_moveAF = 1;
	}

	LOG_INF("move [curr] %lu [target] %lu\n", g_u4CurrPosition, g_u4TargetPosition);


	if (s4AF_SetPosition((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
	}

	return 0;
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
long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user stAF_MotorInfo *) (a_u4Param));
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

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
		s4AF_SetPosition(400);
		msleep(20);
		s4AF_SetPosition(350);
		msleep(20);
		s4AF_SetPosition(300);
		msleep(20);
		s4AF_SetPosition(250);
		msleep(20);
		s4AF_SetPosition(200);
		msleep(20);
		s4AF_SetPosition(100);
		msleep(20);
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

void DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
}
