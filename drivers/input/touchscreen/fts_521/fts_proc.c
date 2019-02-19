/*

 **************************************************************************
 **                        STMicroelectronics							 **
 **************************************************************************
 **                        marco.cali@st.com							 **
 **************************************************************************
 *                                                                        *
 *                     Utilities published in /proc/fts					  *
 *                                                                        *
 **************************************************************************
 **************************************************************************

 */

/*!
* \file fts_proc.c
* \brief contains the function and variables needed to publish a file node in the file system which allow to communicate with the IC from userspace
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "fts.h"
#include "fts_lib/ftsCompensation.h"
#include "fts_lib/ftsCore.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsFlash.h"
#include "fts_lib/ftsTest.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"

#define DRIVER_TEST_FILE_NODE								"driver_test"
#define CHUNK_PROC											1024
#define DIAGNOSTIC_NUM_FRAME								10

/** @defgroup proc_file_code	 Proc File Node
* @ingroup file_nodes
* The /proc/fts/driver_test file node provide expose the most important API implemented into the driver to execute any possible operation into the IC \n
* Thanks to a series of Operation Codes, each of them, with a different set of parameter, it is possible to select a function to execute\n
* The result of the function is usually returned into the shell as an ASCII hex string where each byte is encoded in two chars.\n
* @{
*/

/*Bus operations*/
#define CMD_READ											0x00
#define CMD_WRITE											0x01
#define CMD_WRITEREAD										0x02
#define CMD_WRITETHENWRITEREAD								0x03
#define CMD_WRITEU8UX										0x04
#define CMD_WRITEREADU8UX									0x05
#define CMD_WRITEU8UXTHENWRITEU8UX							0x06
#define CMD_WRITEU8UXTHENWRITEREADU8UX						0x07
#define CMD_GETLIMITSFILE									0x08
#define CMD_GETFWFILE										0x09
#define CMD_VERSION											0x0A
#define CMD_READCONFIG										0x0B

/*GUI utils byte ver*/
#define CMD_READ_BYTE										0xF0
#define CMD_WRITE_BYTE										0xF1
#define CMD_WRITEREAD_BYTE									0xF2
#define CMD_WRITETHENWRITEREAD_BYTE							0xF3
#define CMD_WRITEU8UX_BYTE									0xF4
#define CMD_WRITEREADU8UX_BYTE								0xF5
#define CMD_WRITEU8UXTHENWRITEU8UX_BYTE						0xF6
#define CMD_WRITEU8UXTHENWRITEREADU8UX_BYTE					0xF7
#define CMD_GETLIMITSFILE_BYTE								0xF8
#define CMD_GETFWFILE_BYTE									0xF9
#define CMD_VERSION_BYTE									0xFA
#define CMD_CHANGE_OUTPUT_MODE								0xFF

/*Core/Tools*/
#define CMD_POLLFOREVENT									0x11
#define CMD_SYSTEMRESET										0x12
#define CMD_CLEANUP											0x13
#define CMD_POWERCYCLE										0x14
#define CMD_READSYSINFO										0x15
#define CMD_FWWRITE											0x16
#define CMD_INTERRUPT										0x17

/*Frame*/
#define CMD_GETFORCELEN										0x20
#define CMD_GETSENSELEN										0x21
#define CMD_GETMSFRAME										0x23
#define CMD_GETSSFRAME										0x24

/*Compensation*/
#define CMD_REQCOMPDATA										0x30
#define CMD_READCOMPDATAHEAD								0x31
#define CMD_READMSCOMPDATA									0x32
#define CMD_READSSCOMPDATA									0x33
#define CMD_READTOTMSCOMPDATA								0x35
#define CMD_READTOTSSCOMPDATA								0x36

/*FW Update*/
#define CMD_GETFWVER										0x40
#define CMD_FLASHUNLOCK										0x42
#define CMD_READFWFILE										0x43
#define CMD_FLASHPROCEDURE									0x44
#define CMD_FLASHERASEUNLOCK								0x45
#define CMD_FLASHERASEPAGE									0x46

/*MP test*/
#define CMD_ITOTEST											0x50
#define CMD_INITTEST										0x51
#define CMD_MSRAWTEST										0x52
#define CMD_MSINITDATATEST									0x53
#define CMD_SSRAWTEST										0x54
#define CMD_SSINITDATATEST									0x55
#define CMD_MAINTEST										0x56
#define CMD_FREELIMIT										0x57

/*Diagnostic*/
#define CMD_DIAGNOSTIC										0x60


#define CMD_CHANGE_SAD										0x70

static u8 bin_output;
/** @}*/

/** @defgroup scriptless Scriptless Protocol
 * @ingroup proc_file_code
 * Scriptless Protocol allows ST Software (such as FingerTip Studio etc) to communicate with the IC from an user space.
 * This mode gives access to common bus operations (write, read etc) and support additional functionalities. \n
 * The protocol is based on exchange of binary messages included between a start and an end byte
 * @{
 */

#define MESSAGE_START_BYTE									0x7B
#define MESSAGE_END_BYTE									0x7D
#define MESSAGE_MIN_HEADER_SIZE								8

/**
 * Possible actions that can be requested by an host
 */
typedef enum {
	ACTION_WRITE = (u16) 0x0001,
	ACTION_READ = (u16) 0x0002,
	ACTION_WRITE_READ = (u16) 0x0003,
	ACTION_GET_VERSION = (u16) 0x0004,
	ACTION_WRITEU8UX = (u16) 0x0011,
	ACTION_WRITEREADU8UX = (u16) 0x0012,
	ACTION_WRITETHENWRITEREAD = (u16) 0x0013,
	ACTION_WRITEU8XTHENWRITEREADU8UX = (u16) 0x0014,
	ACTION_WRITEU8UXTHENWRITEU8UX = (u16) 0x0015,
	ACTION_GET_FW = (u16) 0x1000,
	ACTION_GET_LIMIT = (u16) 0x1001
} Actions;

/**
 * Struct used to contain info of the message received by the host in Scriptless mode
 */
typedef struct {
	u16 msg_size;
	u16 counter;
	Actions action;
	u8 dummy;
} Message;

/** @}*/

extern TestToDo tests;
extern SysInfo systemInfo;

static int limit;
static int chunk;
static int printed;
static struct proc_dir_entry *fts_dir;
static u8 *driver_test_buff;
char buf_chunk[CHUNK_PROC];
static Message mess;

/************************ SEQUENTIAL FILE UTILITIES **************************/
/**
* This function is called at the beginning of the stream to a sequential file or every time into the sequential were already written PAGE_SIZE bytes and the stream need to restart
* @param s pointer to the sequential file on which print the data
* @param pos pointer to the offset where write the data
* @return NULL if there is no data to print or the pointer to the beginning of the data that need to be printed
*/
static void *fts_seq_start(struct seq_file *s, loff_t *pos)
{
	logError(0,
		 "%s %s: Entering start(), pos = %Ld limit = %d printed = %d \n",
		 tag, __func__, *pos, limit, printed);

	if (driver_test_buff == NULL && *pos == 0) {
		logError(1, "%s %s: No data to print!\n", tag, __func__);
		driver_test_buff = (u8 *) kmalloc(13 * sizeof(u8), GFP_KERNEL);

		snprintf(driver_test_buff, PAGE_SIZE, "{ %08X }\n", ERROR_OP_NOT_ALLOW);

		limit = strlen(driver_test_buff);
	} else {
		if (*pos != 0)
			*pos += chunk - 1;

		if (*pos >= limit) {
			return NULL;
		}
	}

	chunk = CHUNK_PROC;
	if (limit - *pos < CHUNK_PROC)
		chunk = limit - *pos;
	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &driver_test_buff[(int)*pos], chunk);

	return buf_chunk;
}

/**
* This function actually print a chunk amount of data in the sequential file
* @param s pointer to the sequential file where to print the data
* @param v pointer to the data to print
* @return 0
*/
static int fts_seq_show(struct seq_file *s, void *v)
{
	seq_write(s, (u8 *) v, chunk);
	printed += chunk;
	return 0;
}

/**
* This function update the pointer and the counters to the next data to be printed
* @param s pointer to the sequential file where to print the data
* @param v pointer to the data to print
* @param pos pointer to the offset where write the next data
* @return NULL if there is no data to print or the pointer to the beginning of the next data that need to be printed
*/
static void *fts_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos) += chunk;
	chunk = CHUNK_PROC;

	if (*pos >= limit)
		return NULL;
	else {
		if (limit - *pos < CHUNK_PROC)
			chunk = limit - *pos;
	}

	memset(buf_chunk, 0, CHUNK_PROC);
	memcpy(buf_chunk, &driver_test_buff[(int)*pos], chunk);
	return buf_chunk;
}

/**
* This function is called when there are no more data to print  the stream need to be terminated or when PAGE_SIZE data were already written into the sequential file
* @param s pointer to the sequential file where to print the data
* @param v pointer returned by fts_seq_next
*/
static void fts_seq_stop(struct seq_file *s, void *v)
{

	if (v) {
		/*logError(0, "%s %s: v is %X.\n", tag, __func__, v);*/
	} else {
		limit = 0;
		chunk = 0;
		printed = 0;
		if (driver_test_buff != NULL) {
			kfree(driver_test_buff);
			driver_test_buff = NULL;

		} else {
			/*logError(0, "%s %s: driver_test_buff is already null.\n", tag, __func__);*/
		}
	}

}

/**
* Struct where define and specify the functions which implements the flow for writing on a sequential file
*/
static struct seq_operations fts_seq_ops = {
	.start = fts_seq_start,
	.next = fts_seq_next,
	.stop = fts_seq_stop,
	.show = fts_seq_show
};

/**
* This function open a sequential file
* @param inode Inode in the file system that was called and triggered this function
* @param file file associated to the file node
* @return error code, 0 if success
*/
static int fts_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &fts_seq_ops);
};

/*****************************************************************************/

/**************************** DRIVER TEST ************************************/

/** @addtogroup proc_file_code
 * @{
 */

/**
 * Receive the OP code and the inputs from shell when the file node is called, parse it and then execute the corresponding function
 * echo cmd+parameters > /proc/fts/driver_test to execute the select command
 * cat /proc/fts/driver_test			to obtain the result into the shell \n
 * the string returned in the shell is made up as follow: \n
 * { = start byte \n
 * the answer content and format strictly depend on the cmd executed. In general can be: an HEX string or a byte array (e.g in case of 0xF- commands) \n
 * } = end byte \n
 */
static ssize_t fts_driver_test_write(struct file *file, const char __user *buf,
				     size_t count, loff_t *pos)
{
	int numberParam = 0;
	struct fts_ts_info *info = dev_get_drvdata(getDev());
	char *p = NULL;
	char pbuf[count];
	char path[100] = { 0 };
	int res = -1, j, index = 0;
	int size = 6;
	int temp, byte_call = 0;
	u16 byteToRead = 0;
	u32 fileSize = 0;
	u8 *readData = NULL;
	u8 cmd[count];
	u32 funcToTest[((count + 1) / 3)];
	u64 addr = 0;
	MutualSenseFrame frameMS;
	SelfSenseFrame frameSS;

	DataHeader dataHead;
	MutualSenseData compData;
	SelfSenseData comData;
	TotMutualSenseData totCompData;
	TotSelfSenseData totComData;

	u64 address;
	u16 fw_version;
	u16 config_id;

	Firmware fw;
	LimitFile lim;

	mess.dummy = 0;
	mess.action = 0;
	mess.msg_size = 0;

	if (access_ok(VERIFY_READ, buf, count) < OK
	    || copy_from_user(pbuf, buf, count) != 0) {
		res = ERROR_ALLOC;
		goto END;
	}

	p = pbuf;
	if (count > MESSAGE_MIN_HEADER_SIZE - 1 && p[0] == MESSAGE_START_BYTE) {
		logError(0, "%s Enter in Byte Mode! \n", tag);
		byte_call = 1;
		mess.msg_size = (p[1] << 8) | p[2];
		mess.counter = (p[3] << 8) | p[4];
		mess.action = (p[5] << 8) | p[6];
		logError(0,
			 "%s Message received: size = %d, counter_id = %d, action = %04X \n",
			 tag, mess.msg_size, mess.counter, mess.action);
		size = MESSAGE_MIN_HEADER_SIZE + 2;
		if (count < mess.msg_size || p[count - 2] != MESSAGE_END_BYTE) {
			logError(1,
				 "%s number of byte received or end byte wrong! msg_size = %d != %d, last_byte = %02X != %02X ... ERROR %08X\n",
				 tag, mess.msg_size, count, p[count - 1],
				 MESSAGE_END_BYTE, ERROR_OP_NOT_ALLOW);
			res = ERROR_OP_NOT_ALLOW;
			goto END;

		} else {
			numberParam = mess.msg_size - MESSAGE_MIN_HEADER_SIZE + 1;
			size = MESSAGE_MIN_HEADER_SIZE + 2;
			switch (mess.action) {
			case ACTION_READ:
				cmd[0] = funcToTest[0] = CMD_READ_BYTE;
				break;

			case ACTION_WRITE:
				cmd[0] = funcToTest[0] = CMD_WRITE_BYTE;
				break;

			case ACTION_WRITE_READ:
				cmd[0] = funcToTest[0] = CMD_WRITEREAD_BYTE;
				break;

			case ACTION_GET_VERSION:
				cmd[0] = funcToTest[0] = CMD_VERSION_BYTE;
				break;

			case ACTION_WRITETHENWRITEREAD:
				cmd[0] = funcToTest[0] =
				    CMD_WRITETHENWRITEREAD_BYTE;
				break;

			case ACTION_WRITEU8UX:
				cmd[0] = funcToTest[0] = CMD_WRITEU8UX_BYTE;
				break;

			case ACTION_WRITEREADU8UX:
				cmd[0] = funcToTest[0] = CMD_WRITEREADU8UX_BYTE;
				break;

			case ACTION_WRITEU8UXTHENWRITEU8UX:
				cmd[0] = funcToTest[0] =
				    CMD_WRITEU8UXTHENWRITEU8UX_BYTE;
				break;

			case ACTION_WRITEU8XTHENWRITEREADU8UX:
				cmd[0] = funcToTest[0] =
				    CMD_WRITEU8UXTHENWRITEREADU8UX_BYTE;
				break;

			case ACTION_GET_FW:
				cmd[0] = funcToTest[0] = CMD_GETFWFILE_BYTE;
				break;

			case ACTION_GET_LIMIT:
				cmd[0] = funcToTest[0] = CMD_GETLIMITSFILE_BYTE;
				break;

			default:
				logError(1,
					 "%s Invalid Action = %d ... ERROR %08X\n",
					 tag, mess.action, ERROR_OP_NOT_ALLOW);
				res = ERROR_OP_NOT_ALLOW;
				goto END;
			}

			if (numberParam - 1 != 0)
				memcpy(&cmd[1], &p[7], numberParam - 1);
		}
	} else {
		if (((count + 1) / 3) >= 1) {
			sscanf(p, "%02X ", &funcToTest[0]);
			p += 3;
			cmd[0] = (u8) funcToTest[0];
			numberParam = 1;
		} else {
			res = ERROR_OP_NOT_ALLOW;
			goto END;
		}

		logError(1, "%s functionToTest[0] = %02X cmd[0]= %02X\n", tag,
			 funcToTest[0], cmd[0]);
		switch (funcToTest[0]) {
		case CMD_GETFWFILE:
		case CMD_GETLIMITSFILE:
			if (count - 2 - 1 > 1) {
				numberParam = 2;
				sscanf(p, "%100s", path);
			}
			break;

		default:
			for (; numberParam < (count + 1) / 3; numberParam++) {
				sscanf(p, "%02X ", &funcToTest[numberParam]);
				p += 3;
				cmd[numberParam] = (u8) funcToTest[numberParam];
				logError(1,
					 "%s functionToTest[%d] = %02X cmd[%d]= %02X\n",
					 tag, numberParam,
					 funcToTest[numberParam], numberParam,
					 cmd[numberParam]);
			}
		}

	}

	fw.data = NULL;
	lim.data = NULL;

	logError(1, "%s Number of Parameters = %d \n", tag, numberParam);

	if (numberParam >= 1) {
		switch (funcToTest[0]) {
		case CMD_VERSION_BYTE:
			logError(0, "%s %s: Get Version Byte \n", tag, __func__,
				 res);
			byteToRead = 2;
			mess.dummy = 0;
			readData =
			    (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
			size += byteToRead;
			if (readData != NULL) {
				readData[0] = (u8) (FTS_TS_DRV_VER >> 24);
				readData[1] = (u8) (FTS_TS_DRV_VER >> 16);
				res = OK;
				logError(0, "%s %s: Version = %02X%02X \n", tag,
					 __func__, readData[0], readData[1]);
			} else {
				res = ERROR_ALLOC;
				logError(1,
					 "%s %s: Impossible allocate memory... ERROR %08X \n",
					 tag, __func__, res);
			}
			break;

		case CMD_VERSION:
			byteToRead = 2 * sizeof(u32);
			mess.dummy = 0;
			readData =
			    (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
			u32ToU8_be(FTS_TS_DRV_VER, readData);
			fileSize = 0;
#ifdef FW_H_FILE
			fileSize |= 0x00010000;
#endif

#ifdef LIMITS_H_FILE
			fileSize |= 0x00020000;
#endif

#ifdef USE_ONE_FILE_NODE
			fileSize |= 0x00040000;
#endif

#ifdef FW_UPDATE_ON_PROBE
			fileSize |= 0x00080000;
#endif

#ifdef PRE_SAVED_METHOD
			fileSize |= 0x00100000;
#endif

#ifdef USE_GESTURE_MASK
			fileSize |= 0x00100000;
#endif

#ifdef I2C_INTERFACE
			fileSize |= 0x00200000;
#endif

#ifdef PHONE_KEY
			fileSize |= 0x00000100;
#endif

#ifdef GESTURE_MODE
			fromIDtoMask(FEAT_SEL_GESTURE, (u8 *)&fileSize, 4);
#endif

#ifdef GRIP_MODE
			fromIDtoMask(FEAT_SEL_GRIP, (u8 *)&fileSize, 4);
#endif

#ifdef CHARGER_MODE
			fromIDtoMask(FEAT_SEL_CHARGER, (u8 *)&fileSize, 4);
#endif

#ifdef GLOVE_MODE
			fromIDtoMask(FEAT_SEL_GLOVE, (u8 *)&fileSize, 4);
#endif

#ifdef COVER_MODE
			fromIDtoMask(FEAT_SEL_COVER, (u8 *)&fileSize, 4);
#endif

#ifdef STYLUS_MODE
			fromIDtoMask(FEAT_SEL_STYLUS, (u8 *)&fileSize, 4);
#endif

			u32ToU8_be(fileSize, &readData[4]);
			res = OK;
			size += (byteToRead * sizeof(u8));
			break;

		case CMD_WRITEREAD:
		case CMD_WRITEREAD_BYTE:
			if (numberParam >= 5) {
				temp = numberParam - 4;
				if (cmd[numberParam - 1] == 0) {
					mess.dummy = 0;
				} else
					mess.dummy = 1;

				u8ToU16_be(&cmd[numberParam - 3], &byteToRead);
				logError(0, "%s bytesToRead = %d \n", tag,
					 byteToRead + mess.dummy);

				readData =
				    (u8 *) kmalloc((byteToRead + mess.dummy) *
						   sizeof(u8), GFP_KERNEL);
				res =
				    fts_writeRead(&cmd[1], temp, readData,
						  byteToRead + mess.dummy);
				size += (byteToRead * sizeof(u8));

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITE:
		case CMD_WRITE_BYTE:
			if (numberParam >= 2) {
				temp = numberParam - 1;
				res = fts_write(&cmd[1], temp);

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READ:
		case CMD_READ_BYTE:
			if (numberParam >= 3) {
				if (numberParam == 3
				    || (numberParam == 4
					&& cmd[numberParam - 1] == 0)) {
					mess.dummy = 0;
				} else
					mess.dummy = 1;
				u8ToU16_be(&cmd[1], &byteToRead);
				readData =
				    (u8 *) kmalloc((byteToRead + mess.dummy) *
						   sizeof(u8), GFP_KERNEL);
				res =
				    fts_read(readData, byteToRead + mess.dummy);
				size += (byteToRead * sizeof(u8));

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITETHENWRITEREAD:
		case CMD_WRITETHENWRITEREAD_BYTE:
			if (numberParam >= 6) {
				u8ToU16_be(&cmd[numberParam - 2], &byteToRead);
				readData =
				    (u8 *) kmalloc(byteToRead * sizeof(u8),
						   GFP_KERNEL);
				res =
				    fts_writeThenWriteRead(&cmd[3], cmd[1],
							   &cmd[3 +
								(int)cmd[1]],
							   cmd[2], readData,
							   byteToRead);
				size += (byteToRead * sizeof(u8));

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITEU8UX:
		case CMD_WRITEU8UX_BYTE:
			if (numberParam >= 4) {
				if (cmd[2] <= sizeof(u64)) {
					u8ToU64_be(&cmd[3], &addr, cmd[2]);
					logError(0, "%s addr = %016X %ld \n",
						 tag, addr, addr);
					res =
					    fts_writeU8UX(cmd[1], cmd[2], addr,
							  &cmd[3 + cmd[2]],
							  (numberParam -
							   cmd[2] - 3));
				} else {
					logError(1, "%s Wrong address size!\n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITEREADU8UX:
		case CMD_WRITEREADU8UX_BYTE:
			if (numberParam >= 6) {

				if (cmd[2] <= sizeof(u64)) {

					u8ToU64_be(&cmd[3], &addr, cmd[2]);
					u8ToU16_be(&cmd[numberParam - 3],
						   &byteToRead);
					readData =
					    (u8 *) kmalloc(byteToRead *
							   sizeof(u8),
							   GFP_KERNEL);
					logError(0,
						 "%s addr = %016X byteToRead = %d \n",
						 tag, addr, byteToRead);
					res =
					    fts_writeReadU8UX(cmd[1], cmd[2],
							      addr, readData,
							      byteToRead,
							      cmd[numberParam -
								  1]);
					size += (byteToRead * sizeof(u8));

				} else {
					logError(1, "%s Wrong address size!\n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITEU8UXTHENWRITEU8UX:
		case CMD_WRITEU8UXTHENWRITEU8UX_BYTE:
			if (numberParam >= 6) {
				if ((cmd[2] + cmd[4]) <= sizeof(u64)) {
					u8ToU64_be(&cmd[5], &addr,
						   cmd[2] + cmd[4]);
					logError(0, "%s addr = %016X %ld \n",
						 tag, addr, addr);
					res =
					    fts_writeU8UXthenWriteU8UX(cmd[1],
								       cmd[2],
								       cmd[3],
								       cmd[4],
								       addr,
								       &cmd[5 +
									    cmd
									    [2]
									    +
									    cmd
									    [4]],
								       (numberParam
									-
									cmd[2] -
									cmd[4] -
									5));
				} else {
					logError(1, "%s Wrong address size! \n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_WRITEU8UXTHENWRITEREADU8UX:
		case CMD_WRITEU8UXTHENWRITEREADU8UX_BYTE:
			if (numberParam >= 8) {
				if ((cmd[2] + cmd[4]) <= sizeof(u64)) {
					u8ToU64_be(&cmd[5], &addr,
						   cmd[2] + cmd[4]);
					logError(1,
						 "%s %s: cmd[5] = %02X, addr =  %016X \n",
						 tag, __func__, cmd[5], addr);
					u8ToU16_be(&cmd[numberParam - 3],
						   &byteToRead);
					readData =
					    (u8 *) kmalloc(byteToRead *
							   sizeof(u8),
							   GFP_KERNEL);
					res =
					    fts_writeU8UXthenWriteReadU8UX(cmd
									   [1],
									   cmd
									   [2],
									   cmd
									   [3],
									   cmd
									   [4],
									   addr,
									   readData,
									   byteToRead,
									   cmd
									   [numberParam
									    -
									    1]);
					size += (byteToRead * sizeof(u8));
				} else {
					logError(1,
						 "%s Wrong total address size! \n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_CHANGE_OUTPUT_MODE:
			if (numberParam >= 2) {
				bin_output = cmd[1];
				logError(0,
					 "%s Setting Scriptless output mode: %d \n",
					 tag, bin_output);
				res = OK;
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_FWWRITE:
			if (numberParam >= 3) {
				if (numberParam >= 2) {
					temp = numberParam - 1;
					res = fts_writeFwCmd(&cmd[1], temp);
				} else {
					logError(1, "%s Wrong parameters! \n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_INTERRUPT:
			if (numberParam >= 2) {
				if (cmd[1] == 1)
					res = fts_enableInterrupt();
				else
					res = fts_disableInterrupt();
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READCONFIG:
			if (numberParam == 5) {
				byteToRead =
				    ((funcToTest[3] << 8) | funcToTest[4]);
				readData =
				    (u8 *) kmalloc(byteToRead * sizeof(u8),
						   GFP_KERNEL);
				res =
				    readConfig((u16)
					       ((((u8) funcToTest[1] & 0x00FF)
						 << 8) +
						((u8) funcToTest[2] & 0x00FF)),
					       readData, byteToRead);
				size += (byteToRead * sizeof(u8));
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_POLLFOREVENT:
			if (numberParam >= 5) {
				temp = (int)funcToTest[1];
				if (numberParam == 5 + (temp - 1) && temp != 0) {
					readData =
					    (u8 *) kmalloc(FIFO_EVENT_SIZE *
							   sizeof(u8),
							   GFP_KERNEL);
					res =
					    pollForEvent((int *)&funcToTest[2],
							 temp, readData,
							 ((funcToTest[temp + 2]
							   & 0x00FF) << 8) +
							 (funcToTest[temp + 3] &
							  0x00FF));
					if (res >= OK)
						res = OK;
					size += (FIFO_EVENT_SIZE * sizeof(u8));
					byteToRead = FIFO_EVENT_SIZE;
				} else {
					logError(1, "%s Wrong parameters! \n",
						 tag);
					res = ERROR_OP_NOT_ALLOW;
				}

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_SYSTEMRESET:
			res = fts_system_reset();

			break;

		case CMD_READSYSINFO:
			if (numberParam == 2) {
				res = readSysInfo(funcToTest[1]);
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_CLEANUP:
			if (numberParam == 2) {
				res = cleanUp(funcToTest[1]);
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_GETFORCELEN:
			temp = getForceLen();
			if (temp < OK)
				res = temp;
			else {
				size += (1 * sizeof(u8));
				res = OK;
			}
			break;

		case CMD_GETSENSELEN:
			temp = getSenseLen();
			if (temp < OK)
				res = temp;
			else {
				size += (1 * sizeof(u8));
				res = OK;
			}
			break;

		case CMD_GETMSFRAME:
			if (numberParam == 2) {
				logError(0, "%s Get 1 MS Frame \n", tag);
				setScanMode(SCAN_MODE_ACTIVE, 0x01);
				mdelay(WAIT_FOR_FRESH_FRAMES);
				setScanMode(SCAN_MODE_ACTIVE, 0x00);
				mdelay(WAIT_AFTER_SENSEOFF);
				flushFIFO();
				res =
				    getMSFrame3((MSFrameType) cmd[1], &frameMS);
				if (res < 0) {
					logError(0,
						 "%s Error while taking the MS frame... ERROR %08X \n",
						 tag, res);

				} else {
					logError(0,
						 "%s The frame size is %d words\n",
						 tag, res);
					size += (res * sizeof(short) + 2);
					/* set res to OK because if getMSFrame is
					   successful res = number of words read
					 */
					res = OK;
					print_frame_short("MS frame =",
							  array1dTo2d_short
							  (frameMS.node_data,
							   frameMS.
							   node_data_size,
							   frameMS.header.
							   sense_node),
							  frameMS.header.
							  force_node,
							  frameMS.header.
							  sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

			/*read self raw */
		case CMD_GETSSFRAME:
			if (numberParam == 2) {
				logError(0, "%s Get 1 SS Frame \n", tag);
				flushFIFO();
				setScanMode(SCAN_MODE_ACTIVE, 0x01);
				mdelay(WAIT_FOR_FRESH_FRAMES);
				setScanMode(SCAN_MODE_ACTIVE, 0x00);
				mdelay(WAIT_AFTER_SENSEOFF);
				res =
				    getSSFrame3((SSFrameType) cmd[1], &frameSS);

				if (res < OK) {
					logError(0,
						 "%s Error while taking the SS frame... ERROR %08X \n",
						 tag, res);

				} else {
					logError(0,
						 "%s The frame size is %d words\n",
						 tag, res);
					size += (res * sizeof(short) + 2);
					/* set res to OK because if getMSFrame is
					   successful res = number of words read
					 */
					res = OK;
					print_frame_short("SS force frame =",
							  array1dTo2d_short
							  (frameSS.force_data,
							   frameSS.header.
							   force_node, 1),
							  frameSS.header.
							  force_node, 1);
					print_frame_short("SS sense frame =",
							  array1dTo2d_short
							  (frameSS.sense_data,
							   frameSS.header.
							   sense_node,
							   frameSS.header.
							   sense_node), 1,
							  frameSS.header.
							  sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_REQCOMPDATA:
			if (numberParam == 2) {
				logError(0,
					 "%s Requesting Compensation Data \n",
					 tag);
				res = requestCompensationData(cmd[1]);

				if (res < OK) {
					logError(0,
						 "%s Error requesting compensation data ERROR %08X \n",
						 tag, res);
				} else {
					logError(0,
						 "%s Requesting Compensation Data Finished! \n",
						 tag);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READCOMPDATAHEAD:
			if (numberParam == 2) {
				logError(0,
					 "%s Requesting Compensation Data \n",
					 tag);
				res = requestCompensationData(cmd[1]);
				if (res < OK) {
					logError(0,
						 "%s Error requesting compensation data ERROR %08X \n",
						 tag, res);
				} else {
					logError(0,
						 "%s Requesting Compensation Data Finished! \n",
						 tag);
					res =
					    readCompensationDataHeader((u8)
								       funcToTest
								       [1],
								       &dataHead,
								       &address);
					if (res < OK) {
						logError(0,
							 "%s Read Compensation Data Header ERROR %08X\n",
							 tag, res);
					} else {
						logError(0,
							 "%s Read Compensation Data Header OK!\n",
							 tag);
						size += (1 * sizeof(u8));
					}
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READMSCOMPDATA:
			if (numberParam == 2) {
				logError(0, "%s Get MS Compensation Data \n",
					 tag);
				res =
				    readMutualSenseCompensationData(cmd[1],
								    &compData);

				if (res < OK) {
					logError(0,
						 "%s Error reading MS compensation data ERROR %08X \n",
						 tag, res);
				} else {
					logError(0,
						 "%s MS Compensation Data Reading Finished! \n",
						 tag);
					size =
					    ((compData.node_data_size +
					      10) * sizeof(i8));
					print_frame_i8("MS Data (Cx2) =",
						       array1dTo2d_i8(compData.
								      node_data,
								      compData.
								      node_data_size,
								      compData.
								      header.
								      sense_node),
						       compData.header.
						       force_node,
						       compData.header.
						       sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READSSCOMPDATA:
			if (numberParam == 2) {
				logError(0, "%s Get SS Compensation Data... \n",
					 tag);
				res =
				    readSelfSenseCompensationData(cmd[1],
								  &comData);
				if (res < OK) {
					logError(0,
						 "%s Error reading SS compensation data ERROR %08X\n",
						 tag, res);
				} else {
					logError(0,
						 "%s SS Compensation Data Reading Finished! \n",
						 tag);
					size =
					    ((comData.header.force_node +
					      comData.header.sense_node) * 2 +
					     13) * sizeof(i8);
					print_frame_i8("SS Data Ix2_fm = ",
						       array1dTo2d_i8(comData.
								      ix2_fm,
								      comData.
								      header.
								      force_node,
								      comData.
								      header.
								      force_node),
						       1,
						       comData.header.
						       force_node);
					print_frame_i8("SS Data Cx2_fm = ",
						       array1dTo2d_i8(comData.
								      cx2_fm,
								      comData.
								      header.
								      force_node,
								      comData.
								      header.
								      force_node),
						       1,
						       comData.header.
						       force_node);
					print_frame_i8("SS Data Ix2_sn = ",
						       array1dTo2d_i8(comData.
								      ix2_sn,
								      comData.
								      header.
								      sense_node,
								      comData.
								      header.
								      sense_node),
						       1,
						       comData.header.
						       sense_node);
					print_frame_i8("SS Data Cx2_sn = ",
						       array1dTo2d_i8(comData.
								      cx2_sn,
								      comData.
								      header.
								      sense_node,
								      comData.
								      header.
								      sense_node),
						       1,
						       comData.header.
						       sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READTOTMSCOMPDATA:
			if (numberParam == 2) {
				logError(0,
					 "%s Get TOT MS Compensation Data \n",
					 tag);
				res =
				    readTotMutualSenseCompensationData(cmd[1],
								       &totCompData);

				if (res < OK) {
					logError(0,
						 "%s Error reading TOT MS compensation data ERROR %08X \n",
						 tag, res);
				} else {
					logError(0,
						 "%s TOT MS Compensation Data Reading Finished! \n",
						 tag);
					size =
					    (totCompData.node_data_size *
					     sizeof(short) + 9);
					print_frame_short("MS Data (TOT Cx) =",
							  array1dTo2d_short
							  (totCompData.
							   node_data,
							   totCompData.
							   node_data_size,
							   totCompData.header.
							   sense_node),
							  totCompData.header.
							  force_node,
							  totCompData.header.
							  sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_READTOTSSCOMPDATA:
			if (numberParam == 2) {
				logError(0,
					 "%s Get TOT SS Compensation Data... \n",
					 tag);
				res =
				    readTotSelfSenseCompensationData(cmd[1],
								     &totComData);
				if (res < OK) {
					logError(0,
						 "%s Error reading TOT SS compensation data ERROR %08X\n",
						 tag, res);
				} else {
					logError(0,
						 "%s TOT SS Compensation Data Reading Finished! \n",
						 tag);
					size =
					    ((totComData.header.force_node +
					      totComData.header.sense_node) *
					     2 * sizeof(short) + 9);
					print_frame_u16("SS Data TOT Ix_fm = ",
							array1dTo2d_u16
							(totComData.ix_fm,
							 totComData.header.
							 force_node,
							 totComData.header.
							 force_node), 1,
							totComData.header.
							force_node);
					print_frame_short
					    ("SS Data TOT Cx_fm = ",
					     array1dTo2d_short(totComData.cx_fm,
							       totComData.
							       header.
							       force_node,
							       totComData.
							       header.
							       force_node), 1,
					     totComData.header.force_node);
					print_frame_u16("SS Data TOT Ix_sn = ",
							array1dTo2d_u16
							(totComData.ix_sn,
							 totComData.header.
							 sense_node,
							 totComData.header.
							 sense_node), 1,
							totComData.header.
							sense_node);
					print_frame_short
					    ("SS Data TOT Cx_sn = ",
					     array1dTo2d_short(totComData.cx_sn,
							       totComData.
							       header.
							       sense_node,
							       totComData.
							       header.
							       sense_node), 1,
					     totComData.header.sense_node);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_GETFWVER:
			res = getFirmwareVersion(&fw_version, &config_id);
			if (res < OK) {
				logError(1,
					 "%s Error reading firmware version and config id ERROR %02X\n",
					 tag, res);
			} else {
				logError(0,
					 "%s getFirmwareVersion Finished! \n",
					 tag);
				size += (4) * sizeof(u8);
			}
			break;

		case CMD_FLASHUNLOCK:
			res = flash_unlock();
			if (res < OK) {
				logError(1,
					 "%s Impossible Unlock Flash ERROR %08X\n",
					 tag, res);
			} else {
				logError(0, "%s Flash Unlock OK!\n", tag);
			}
			break;

		case CMD_READFWFILE:
			if (numberParam == 2) {
				logError(0, "%s Reading FW File... \n", tag);
				res =
				    readFwFile(PATH_FILE_FW, &fw,
					       funcToTest[1]);
				if (res < OK) {
					logError(0,
						 "%s Error reading FW File ERROR %08X\n",
						 tag, res);
				} else {
					logError(0,
						 "%s Read FW File Finished! \n",
						 tag);
				}
				kfree(fw.data);
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_FLASHPROCEDURE:
			if (numberParam == 3) {
				logError(0,
					 "%s Starting Flashing Procedure... \n",
					 tag);
				res =
				    flashProcedure(PATH_FILE_FW, cmd[1],
						   cmd[2]);
				if (res < OK) {
					logError(0,
						 "%s Error during flash procedure ERROR %08X\n",
						 tag, res);
				} else {
					logError(0,
						 "%s Flash Procedure Finished! \n",
						 tag);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_FLASHERASEUNLOCK:
			res = flash_erase_unlock();
			if (res < OK) {
				logError(0,
					 "%s Error during flash erase unlock... ERROR %08X\n",
					 tag, res);
			} else {
				logError(0,
					 "%s Flash Erase Unlock Finished! \n",
					 tag);
			}
			break;

		case CMD_FLASHERASEPAGE:
			if (numberParam == 2) {
				logError(0,
					 "%s Starting Flashing Page Erase... \n",
					 tag);
				res = flash_erase_page_by_page(cmd[1]);
				if (res < OK) {
					logError(0,
						 "%s Error during flash page erase... ERROR %08X\n",
						 tag, res);
				} else {
					logError(0,
						 "%s Flash Page Erase Finished! \n",
						 tag);
				}
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

			/*ITO TEST */
		case CMD_ITOTEST:
			res = production_test_ito(LIMITS_FILE, &tests);
			break;

			/*Initialization */
		case CMD_INITTEST:
			if (numberParam == 2) {
				res = production_test_initialization(cmd[1]);

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_MSRAWTEST:
			if (numberParam == 2)
				res =
				    production_test_ms_raw(LIMITS_FILE, cmd[1],
							   &tests);
			else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_MSINITDATATEST:
			if (numberParam == 2)
				res =
				    production_test_ms_cx(LIMITS_FILE, cmd[1],
							  &tests);
			else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_SSRAWTEST:
			if (numberParam == 2)
				res =
				    production_test_ss_raw(LIMITS_FILE, cmd[1],
							   &tests);
			else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_SSINITDATATEST:
			if (numberParam == 2)
				res =
				    production_test_ss_ix_cx(LIMITS_FILE,
							     cmd[1], &tests);
			else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

			/*PRODUCTION TEST */
		case CMD_MAINTEST:
			if (numberParam == 3)
				res =
				    production_test_main(LIMITS_FILE, cmd[1],
							 cmd[2], &tests);
			else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_FREELIMIT:
			res = freeCurrentLimitsFile();
			break;

		case CMD_POWERCYCLE:
			res = fts_chip_powercycle(info);
			break;

		case CMD_GETLIMITSFILE:
			if (numberParam >= 1) {
				lim.data = NULL;
				lim.size = 0;
				if (numberParam == 1)
					res = getLimitsFile(LIMITS_FILE, &lim);
				else
					res = getLimitsFile(path, &lim);
				readData = lim.data;
				fileSize = lim.size;
				size += (fileSize * sizeof(u8));
				if (byte_call == 1)
					size += sizeof(u32);
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_GETLIMITSFILE_BYTE:
			if (numberParam >= 3) {
				lim.data = NULL;
				lim.size = 0;

				u8ToU16_be(&cmd[1], &byteToRead);
				addr = ((u64) byteToRead) * 4;

				res = getLimitsFile(LIMITS_FILE, &lim);

				readData = lim.data;
				fileSize = lim.size;

				if (fileSize > addr) {
					logError(1,
						 "%s Limits dimension expected by Host is less than actual size: expected = %d, real = %d \n",
						 tag, byteToRead, fileSize);
					res = ERROR_OP_NOT_ALLOW;
				}

				size += (addr * sizeof(u8));

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_GETFWFILE:
			if (numberParam >= 1) {

				if (numberParam == 1)
					res =
					    getFWdata(PATH_FILE_FW, &readData,
						      &fileSize);
				else
					res =
					    getFWdata(path, &readData,
						      &fileSize);

				size += (fileSize * sizeof(u8));
				if (byte_call == 1)
					size += sizeof(u32);
			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;

		case CMD_GETFWFILE_BYTE:
			if (numberParam == 3) {

				u8ToU16_be(&cmd[1], &byteToRead);
				addr = ((u64) byteToRead) * 4;

				res =
				    getFWdata(PATH_FILE_FW, &readData,
					      &fileSize);
				if (fileSize > addr) {
					logError(1,
						 "%s FW dimension expected by Host is less than actual size: expected = %d, real = %d \n",
						 tag, byteToRead, fileSize);
					res = ERROR_OP_NOT_ALLOW;
				}

				size += (addr * sizeof(u8));

			} else {
				logError(1, "%s Wrong number of parameters! \n",
					 tag);
				res = ERROR_OP_NOT_ALLOW;
			}
			break;
/*
* finish all the diagnostic command with a goto ERROR in order to skip the modification on driver_test_buff
* remember to set properly the limit and printed variables in order to make the seq_file logic to work
*/
		case CMD_DIAGNOSTIC:
			index = 0;
			size = 0;
			fileSize = 256 * 1024 * sizeof(char);
			driver_test_buff = (u8 *) kzalloc(fileSize, GFP_KERNEL);
			readData =
			    (u8 *)
			    kmalloc((ERROR_DUMP_ROW_SIZE *
				     ERROR_DUMP_COL_SIZE) * sizeof(u8),
				    GFP_KERNEL);
			if (driver_test_buff == NULL || readData == NULL) {
				res = ERROR_ALLOC;
				logError(1,
					 "%s Impossible allocate memory for buffers! ERROR %08X! \n",
					 tag, res);
				goto END;
			}
			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "DIAGNOSTIC TEST:\n1) I2C Test: ");
			index += j;

			res =
			    fts_writeReadU8UX(FTS_CMD_HW_REG_R,
					      ADDR_SIZE_HW_REG, ADDR_DCHIP_ID,
					      (u8 *)&temp, 2, DUMMY_HW_REG);
			if (res < OK) {
				logError(1,
					 "%s Error during I2C test: ERROR %08X! \n",
					 tag, res);
				j = snprintf(&driver_test_buff[index],
					     fileSize - index, "ERROR %08X \n",
					     res);
				index += j;
				res = ERROR_OP_NOT_ALLOW;
				goto END_DIAGNOSTIC;
			}

			temp &= 0xFFFF;
			logError(1, "%s Chip ID = %04X! \n", tag, temp);
			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "DATA = %04X, expected = %02X%02X \n",
				     temp, DCHIP_ID_1, DCHIP_ID_0);
			index += j;
			if (temp != ((DCHIP_ID_1 << 8) | DCHIP_ID_0)) {
				logError(1,
					 "%s Wrong CHIP ID, Diagnostic failed! \n",
					 tag, res);
				res = ERROR_OP_NOT_ALLOW;
				goto END_DIAGNOSTIC;
			}

			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "Present Driver Mode: %08X \n",
				     info->mode);
			index += j;

			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "2) FW running: Sensing On...");
			index += j;
			logError(1, "%s Sensing On! \n", tag, temp);
			readData[0] = FTS_CMD_SCAN_MODE;
			readData[1] = SCAN_MODE_ACTIVE;
			readData[2] = 0x1;
			fts_write(readData, 3);
			res = checkEcho(readData, 3);
			if (res < OK) {
				logError(1,
					 "%s No Echo received.. ERROR %08X !\n",
					 tag, res);
				j = snprintf(&driver_test_buff[index],
					     fileSize - index,
					     "No echo found... ERROR %08X!\n",
					     res);
				index += j;
				goto END_DIAGNOSTIC;
			} else {
				logError(1, "%s Echo FOUND... OK!\n", tag, res);
				j = snprintf(&driver_test_buff[index],
					     fileSize - index,
					     "Echo FOUND... OK!\n");
				index += j;
			}

			logError(1, "%s Reading Frames...! \n", tag, temp);
			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "3) Read Frames: \n");
			index += j;
			for (temp = 0; temp < DIAGNOSTIC_NUM_FRAME; temp++) {
				logError(1, "%s Iteration n. %d...\n", tag,
					 temp + 1);
				j = snprintf(&driver_test_buff[index],
					     fileSize - index,
					     "Iteration n. %d...\n", temp + 1);
				index += j;
				for (addr = 0; addr < 3; addr++) {
					switch (addr) {
					case 0:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "MS RAW FRAME =");
						index += j;
						res |=
						    getMSFrame3(MS_RAW,
								&frameMS);
						break;
					case 2:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "MS STRENGTH FRAME =");
						index += j;
						res |=
						    getMSFrame3(MS_STRENGTH,
								&frameMS);
						break;
					case 1:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "MS BASELINE FRAME =");
						index += j;
						res |=
						    getMSFrame3(MS_BASELINE,
								&frameMS);
						break;
					}
					if (res < OK) {
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "No data! ERROR %08X \n",
							     res);
						index += j;
					} else {
						for (address = 0;
						     address <
						     frameMS.node_data_size;
						     address++) {
							if (address %
							    frameMS.header.
							    sense_node == 0) {
								j = snprintf
								    (&driver_test_buff
								     [index],
								     fileSize -
								     index,
								     "\n");
								index += j;
							}
							j = snprintf
							    (&driver_test_buff
							     [index],
							     fileSize - index,
							     "%5d, ",
							     frameMS.
							     node_data
							     [address]);
							index += j;
						}
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "\n");
						index += j;
					}
					if (frameMS.node_data != NULL)
						kfree(frameMS.node_data);
				}
				for (addr = 0; addr < 3; addr++) {
					switch (addr) {
					case 0:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "SS RAW FRAME = \n");
						index += j;
						res |=
						    getSSFrame3(SS_RAW,
								&frameSS);
						break;
					case 2:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "SS STRENGTH FRAME = \n");
						index += j;
						res |=
						    getSSFrame3(SS_STRENGTH,
								&frameSS);
						break;
					case 1:
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "SS BASELINE FRAME = \n");
						index += j;
						res |=
						    getSSFrame3(SS_BASELINE,
								&frameSS);
						break;
					}
					if (res < OK) {
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "No data! ERROR %08X \n",
							     res);
						index += j;
					} else {
						for (address = 0;
						     address <
						     frameSS.header.force_node;
						     address++) {
							j = snprintf
							    (&driver_test_buff
							     [index],
							     fileSize - index,
							     "%d\n",
							     frameSS.
							     force_data
							     [address]);

							index += j;
						}
						for (address = 0;
						     address <
						     frameSS.header.sense_node;
						     address++) {
							j = snprintf
							    (&driver_test_buff
							     [index],
							     fileSize - index,
							     "%d, ",
							     frameSS.
							     sense_data
							     [address]);

							index += j;
						}
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "\n");
						index += j;
					}
					if (frameSS.force_data != NULL)
						kfree(frameSS.force_data);
					if (frameSS.sense_data != NULL)
						kfree(frameSS.sense_data);
				}
			}

			logError(1, "%s Reading error info... \n", tag, temp);
			j = snprintf(&driver_test_buff[index], fileSize - index,
				     "4) FW INFO DUMP: ");
			index += j;
			temp = dumpErrorInfo(readData, ERROR_DUMP_ROW_SIZE * ERROR_DUMP_COL_SIZE);
			if (temp < OK) {
				logError(1,
					 "%s Error during dump: ERROR %08X! \n",
					 tag, res);
				j = snprintf(&driver_test_buff[index],
					     fileSize - index, "ERROR %08X \n",
					     temp);
				index += j;
			} else {
				logError(1, "%s DUMP OK! \n", tag, res);
				for (temp = 0;
				     temp <
				     ERROR_DUMP_ROW_SIZE * ERROR_DUMP_COL_SIZE;
				     temp++) {
					if (temp % ERROR_DUMP_COL_SIZE == 0) {
						j = snprintf(&driver_test_buff
							     [index],
							     fileSize - index,
							     "\n%2d - ",
							     temp /
							     ERROR_DUMP_COL_SIZE);
						index += j;
					}
					j = snprintf(&driver_test_buff[index],
						     fileSize - index, "%02X ",
						     readData[temp]);
					index += j;
				}
			}
			res |= temp;

END_DIAGNOSTIC:
			if (res < OK) {
				j = snprintf(&driver_test_buff[index],
					     fileSize - index,
					     "\nRESULT = FAIL \n");
				index += j;
			} else {
				j = snprintf(&driver_test_buff[index],
					     fileSize - index,
					     "\nRESULT = FINISHED \n");
				index += j;
			}
			limit = index;
			printed = 0;
			goto ERROR;
			break;
		case CMD_CHANGE_SAD:
			res = changeSAD(cmd[1]);
			break;

		default:
			logError(1, "%s COMMAND ID NOT VALID!!! \n", tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}

	} else {
		logError(1,
			 "%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n",
			 tag);
		res = ERROR_OP_NOT_ALLOW;

	}

END:
	if (driver_test_buff != NULL) {
		logError(1,
			 "%s Consecutive echo on the file node, free the buffer with the previous result\n",
			 tag);
		kfree(driver_test_buff);
	}

	if (byte_call == 0) {
		size *= 2;
		size += 2;
	} else {
		if (bin_output != 1) {
			size *= 2;
			size -= 1;
		} else
			size += 1;
	}

	logError(0, "%s Size = %d\n", tag, size);
	driver_test_buff = (u8 *) kzalloc(size, GFP_KERNEL);
	logError(0, "%s Finish to allocate memory! \n", tag);
	if (driver_test_buff == NULL) {
		logError(0,
			 "%s Unable to allocate driver_test_buff! ERROR %08X\n",
			 tag, ERROR_ALLOC);
		goto ERROR;
	}

	if (byte_call == 0) {
		index = 0;
		snprintf(&driver_test_buff[index], 3, "{ ");
		index += 2;
		snprintf(&driver_test_buff[index], 9, "%08X", res);

		index += 8;
		if (res >= OK) {
			/*all the other cases are already fine printing only the res. */
			switch (funcToTest[0]) {
			case CMD_VERSION:
			case CMD_READ:
			case CMD_WRITEREAD:
			case CMD_WRITETHENWRITEREAD:
			case CMD_WRITEREADU8UX:
			case CMD_WRITEU8UXTHENWRITEREADU8UX:
			case CMD_READCONFIG:
			case CMD_POLLFOREVENT:
				if (mess.dummy == 1)
					j = 1;
				else
					j = 0;
				for (; j < byteToRead + mess.dummy; j++) {
					snprintf(&driver_test_buff[index], 3, "%02X", readData[j]);
					index += 2;
				}
				break;
			case CMD_GETFWFILE:
			case CMD_GETLIMITSFILE:
				logError(0, "%s Start To parse! \n", tag);
				for (j = 0; j < fileSize; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X", readData[j]);
					index += 2;
				}
				logError(0, "%s Finish to parse! \n", tag);
				break;
			case CMD_GETFORCELEN:
			case CMD_GETSENSELEN:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) temp);
				index += 2;

				break;

			case CMD_GETMSFRAME:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) frameMS.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) frameMS.header.sense_node);
				index += 2;

				for (j = 0; j < frameMS.node_data_size; j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (frameMS.
						  node_data[j] & 0xFF00) >> 8,
						 frameMS.node_data[j] & 0xFF);
					index += 4;
				}

				kfree(frameMS.node_data);
				break;

			case CMD_GETSSFRAME:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) frameSS.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) frameSS.header.sense_node);
				index += 2;
				for (j = 0; j < frameSS.header.force_node; j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (frameSS.
						  force_data[j] & 0xFF00) >> 8,
						 frameSS.force_data[j] & 0xFF);
					index += 4;
				}

				for (j = 0; j < frameSS.header.sense_node; j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (frameSS.
						  sense_data[j] & 0xFF00) >> 8,
						 frameSS.sense_data[j] & 0xFF);
					index += 4;
				}

				kfree(frameSS.force_data);
				kfree(frameSS.sense_data);
				break;

			case CMD_READMSCOMPDATA:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) compData.header.type);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) compData.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) compData.header.sense_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 compData.cx1 & 0xFF);
				index += 2;

				for (j = 0; j < compData.node_data_size; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X",
						 compData.node_data[j] & 0xFF);
					index += 2;
				}

				kfree(compData.node_data);
				break;

			case CMD_READSSCOMPDATA:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) comData.header.type);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.header.sense_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.f_ix1 & 0xFF);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.s_ix1 & 0xFF);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.f_cx1 & 0xFF);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 comData.s_cx1 & 0xFF);
				index += 2;

				for (j = 0; j < comData.header.force_node; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X",
						 comData.ix2_fm[j] & 0xFF);
					index += 2;

				}

				for (j = 0; j < comData.header.sense_node; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X",
						 comData.ix2_sn[j] & 0xFF);
					index += 2;

				}

				for (j = 0; j < comData.header.force_node; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X",
						 comData.cx2_fm[j] & 0xFF);

					index += 2;
				}

				for (j = 0; j < comData.header.sense_node; j++) {
					snprintf(&driver_test_buff[index], 3,
						 "%02X",
						 comData.cx2_sn[j] & 0xFF);
					index += 2;
				}

				kfree(comData.ix2_fm);
				kfree(comData.ix2_sn);
				kfree(comData.cx2_fm);
				kfree(comData.cx2_sn);
				break;

			case CMD_READTOTMSCOMPDATA:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) totCompData.header.type);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) totCompData.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) totCompData.header.sense_node);

				index += 2;

				for (j = 0; j < totCompData.node_data_size; j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (totCompData.
						  node_data[j] & 0xFF00) >> 8,
						 totCompData.
						 node_data[j] & 0xFF);
					index += 4;
				}

				kfree(totCompData.node_data);
				break;

			case CMD_READTOTSSCOMPDATA:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 (u8) totComData.header.type);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 totComData.header.force_node);
				index += 2;

				snprintf(&driver_test_buff[index], 3, "%02X",
					 totComData.header.sense_node);
				index += 2;

				for (j = 0; j < totComData.header.force_node;
				     j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (totComData.
						  ix_fm[j] & 0xFF00) >> 8,
						 totComData.ix_fm[j] & 0xFF);
					index += 4;
				}

				for (j = 0; j < totComData.header.sense_node;
				     j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (totComData.
						  ix_sn[j] & 0xFF00) >> 8,
						 totComData.ix_sn[j] & 0xFF);
					index += 4;
				}

				for (j = 0; j < totComData.header.force_node;
				     j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (totComData.
						  cx_fm[j] & 0xFF00) >> 8,
						 totComData.cx_fm[j] & 0xFF);

					index += 4;
				}

				for (j = 0; j < totComData.header.sense_node;
				     j++) {
					snprintf(&driver_test_buff[index], 5,
						 "%02X%02X",
						 (totComData.
						  cx_sn[j] & 0xFF00) >> 8,
						 totComData.cx_sn[j] & 0xFF);
					index += 4;
				}

				kfree(totComData.ix_fm);
				kfree(totComData.ix_sn);
				kfree(totComData.cx_fm);
				kfree(totComData.cx_sn);
				break;

			case CMD_GETFWVER:
				snprintf(&driver_test_buff[index], 5, "%04X",
					 fw_version);
				index += 4;

				snprintf(&driver_test_buff[index], 5, "%04X",
					 config_id);
				index += 4;
				break;

			case CMD_READCOMPDATAHEAD:
				snprintf(&driver_test_buff[index], 3, "%02X",
					 dataHead.type);
				index += 2;
				break;

			default:
				break;
			}
		}

		snprintf(&driver_test_buff[index], 4, " }\n");
		limit = size - 1;
		printed = 0;
	} else {

		driver_test_buff[index++] = MESSAGE_START_BYTE;
		if (bin_output == 1) {

			driver_test_buff[index++] = (size & 0xFF00) >> 8;
			driver_test_buff[index++] = (size & 0x00FF);

			driver_test_buff[index++] =
			    (mess.counter & 0xFF00) >> 8;
			driver_test_buff[index++] = (mess.counter & 0x00FF);

			driver_test_buff[index++] = (mess.action & 0xFF00) >> 8;
			driver_test_buff[index++] = (mess.action & 0x00FF);

			driver_test_buff[index++] = (res & 0xFF00) >> 8;
			driver_test_buff[index++] = (res & 0x00FF);

		} else {
			if (funcToTest[0] == CMD_GETLIMITSFILE_BYTE
			    || funcToTest[0] == CMD_GETFWFILE_BYTE)
				snprintf(&driver_test_buff[index], 5,
					 "%02X%02X",
					 (((fileSize + 3) / 4) & 0xFF00) >> 8,
					 ((fileSize + 3) / 4) & 0x00FF);
			else
				snprintf(&driver_test_buff[index], 5,
					 "%02X%02X", (size & 0xFF00) >> 8,
					 size & 0xFF);
			index += 4;
			index +=
			    snprintf(&driver_test_buff[index], 5, "%04X",
				     (u16) mess.counter);
			index +=
			    snprintf(&driver_test_buff[index], 5, "%04X",
				     (u16) mess.action);
			index +=
			    snprintf(&driver_test_buff[index], 5, "%02X%02X",
				     (res & 0xFF00) >> 8, res & 0xFF);
		}

		switch (funcToTest[0]) {
		case CMD_VERSION_BYTE:
		case CMD_READ_BYTE:
		case CMD_WRITEREAD_BYTE:
		case CMD_WRITETHENWRITEREAD_BYTE:
		case CMD_WRITEREADU8UX_BYTE:
		case CMD_WRITEU8UXTHENWRITEREADU8UX_BYTE:
			if (bin_output == 1) {
				if (mess.dummy == 1)
					memcpy(&driver_test_buff[index],
					       &readData[1], byteToRead);
				else
					memcpy(&driver_test_buff[index],
					       readData, byteToRead);
				index += byteToRead;
			} else {
				j = mess.dummy;
				for (; j < byteToRead + mess.dummy; j++)
					index +=
					    snprintf(&driver_test_buff[index],
						     3, "%02X",
						     (u8) readData[j]);
			}
			break;

		case CMD_GETLIMITSFILE_BYTE:
		case CMD_GETFWFILE_BYTE:
			if (bin_output == 1) {
				driver_test_buff[1] =
				    (((fileSize + 3) / 4) & 0xFF00) >> 8;
				driver_test_buff[2] =
				    (((fileSize + 3) / 4) & 0x00FF);

				if (readData != NULL) {
					memcpy(&driver_test_buff[index],
					       readData, fileSize);
				} else {
					logError(0,
						 "%s readData = NULL... returning junk data!",
						 tag);
				}
				index += addr;
			} else {
				for (j = 0; j < fileSize; j++) {
					index +=
					    snprintf(&driver_test_buff[index],
						     3, "%02X",
						     (u8) readData[j]);
				}
				for (; j < addr; j++)
					index += snprintf(&driver_test_buff[index], 3, "%02X", 0);
			}
			break;
		default:
			break;
		}

		driver_test_buff[index++] = MESSAGE_END_BYTE;
		driver_test_buff[index] = '\n';
		limit = size;
		printed = 0;
	}
ERROR:
	numberParam = 0;
	if (readData != NULL)
		kfree(readData);

	return count;
}

/** @}*/

/**
 * file_operations struct which define the functions for the canonical operation on a device file node (open. read, write etc.)
 */
static struct file_operations fts_driver_test_ops = {
	.open = fts_open,
	.read = seq_read,
	.write = fts_driver_test_write,
	.llseek = seq_lseek,
	.release = seq_release
};

/*****************************************************************************/

/**
* This function is called in the probe to initialize and create the directory /proc/fts and the driver test file node DRIVER_TEST_FILE_NODE into the /proc file system
* @return OK if success or an error code which specify the type of error encountered
*/
int fts_proc_init(void)
{
	struct proc_dir_entry *entry;

	int retval = 0;

	fts_dir = proc_mkdir_data("fts", 0777, NULL, NULL);
	if (fts_dir == NULL) {
		retval = -ENOMEM;
		goto out;
	}

	entry = proc_create(DRIVER_TEST_FILE_NODE, 0644, fts_dir,
			&fts_driver_test_ops);

	if (entry) {
		logError(1, "%s %s: proc entry CREATED! \n", tag, __func__);
	} else {
		logError(1, "%s %s: error creating proc entry! \n", tag,
			 __func__);
		retval = -ENOMEM;
		goto badfile;
	}
	return OK;
badfile:
	remove_proc_entry("fts", NULL);
out:
	return retval;
}

/**
* Delete and Clean from the file system, all the references to the driver test file node
* @return OK
*/
int fts_proc_remove(void)
{
	remove_proc_entry(DRIVER_TEST_FILE_NODE, fts_dir);
	remove_proc_entry("fts", NULL);
	return OK;
}
