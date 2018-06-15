/*

**************************************************************************
**                        STMicroelectronics                            **
**************************************************************************
**                        marco.cali@st.com                             **
**************************************************************************
*                                                                        *
*                     API used by Driver Test Apk                        *
*                                                                        *
**************************************************************************
**************************************************************************

*/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/completion.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include "fts.h"
#include "fts_lib/ftsCompensation.h"
#include "fts_lib/ftsIO.h"
#include "fts_lib/ftsError.h"
#include "fts_lib/ftsFrame.h"
#include "fts_lib/ftsFlash.h"
#include "fts_lib/ftsTest.h"
#include "fts_lib/ftsTime.h"
#include "fts_lib/ftsTool.h"

#ifdef DRIVER_TEST

#define MAX_PARAMS 10


#define CMD_READ   0x00
#define CMD_WRITE   0x01
#define CMD_READU16   0x02
#define CMD_READB2   0x03
#define CMD_READB2U16   0x04
#define CMD_POLLFOREVENT  0x05
#define CMD_SYSTEMRESET   0x06
#define CMD_CLEANUP   0x07
#define CMD_GETFORCELEN   0x08
#define CMD_GETSENSELEN   0x09
#define CMD_GETMSFRAME   0x0A
#define CMD_GETSSFRAME   0x0C
#define CMD_REQCOMPDATA   0x0D
#define CMD_READCOMPDATAHEAD  0x0E
#define CMD_READMSCOMPDATA  0x0F
#define CMD_READSSCOMPDATA  0x10
#define CMD_READGNCOMPDATA  0x11
#define CMD_GETFWVER   0x12
#define CMD_FLASHSTATUS   0x13
#define CMD_FLASHUNLOCK   0x14
#define CMD_READFWFILE   0x15
#define CMD_FLASHPROCEDURE  0x16
#define CMD_ITOTEST   0x17
#define CMD_INITTEST   0x18
#define CMD_MSRAWTEST   0x19
#define CMD_MSINITDATATEST  0x1A
#define CMD_SSRAWTEST   0x1B
#define CMD_SSINITDATATEST  0x1C
#define CMD_MAINTEST   0x1D
#define CMD_POWERCYCLE   0x1E
#define CMD_FWWRITE   0x1F
#define CMD_READCHIPINFO  0x20
#define CMD_REQFRAME	0x21


u32 *functionToTest;
int numberParam;

static ssize_t stm_driver_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int n = 0;
	char *p = (char *) buf;
	functionToTest = (u32 *) kmalloc(MAX_PARAMS * sizeof(u32), GFP_KERNEL);

	if (functionToTest == NULL) {
		log_error("%s impossible to allocate functionToTest!\n", tag);
		return count;
	}

	memset(functionToTest, 0, MAX_PARAMS * sizeof(u32));

	for (n = 0; n < (count + 1) / 3 && n < MAX_PARAMS; n++) {
		sscanf(p, "%02X ", &functionToTest[n]);
		p += 3;
		log_error("%s functionToTest[%d] = %02X\n", tag, n, functionToTest[n]);
	}

	numberParam = n;
	log_error("%s Number of Parameters = %d\n", tag, numberParam);
	return count;
}

static ssize_t stm_driver_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char buff[CMD_STR_LEN] = {0};
	int res = -1, j = 0, count = 0;
	int size = 6 * 2;
	int temp = 0, i = 0, byteToRead = 0;
	u8 *readData = NULL;
	u8 *all_strbuff = NULL;
	u8 *cmd;
	MutualSenseFrame frameMS;
	SelfSenseFrame frameSS;
	DataHeader dataHead;
	MutualSenseData compData;
	SelfSenseData comData;
	GeneralData gnData;
	u16 address;
	u16 fw_version;
	u16 config_id;
	Firmware fw;
	TestToDo todoDefault;
	struct i2c_client *client = to_i2c_client(dev);
	struct fts_ts_info *info = i2c_get_clientdata(client);
	fw.data = NULL;
	todoDefault.MutualRaw = 1;
	todoDefault.MutualRawGap = 0;
	todoDefault.MutualCx1 = 0;
	todoDefault.MutualCx2 = 1;
	todoDefault.MutualCx2Adj = 0;
	todoDefault.MutualCxTotal = 0;
	todoDefault.MutualCxTotalAdj = 0;
	todoDefault.MutualKeyRaw = 1;
	todoDefault.MutualKeyCx1 = 0;
	todoDefault.MutualKeyCx2 = 0;
	todoDefault.MutualKeyCxTotal = 1;
	todoDefault.SelfForceRaw = 1;
	todoDefault.SelfForceRawGap = 0;
	todoDefault.SelfForceIx1 = 0;
	todoDefault.SelfForceIx2 = 0;
	todoDefault.SelfForceIx2Adj = 0;
	todoDefault.SelfForceIxTotal = 1;
	todoDefault.SelfForceIxTotalAdj = 0;
	todoDefault.SelfForceCx1 = 0;
	todoDefault.SelfForceCx2 = 0;
	todoDefault.SelfForceCx2Adj = 0;
	todoDefault.SelfForceCxTotal = 0;
	todoDefault.SelfForceCxTotalAdj = 0;
	todoDefault.SelfSenseRaw = 1;
	todoDefault.SelfSenseRawGap = 0;
	todoDefault.SelfSenseIx1 = 0;
	todoDefault.SelfSenseIx2 = 0;
	todoDefault.SelfSenseIx2Adj = 0;
	todoDefault.SelfSenseIxTotal = 1;
	todoDefault.SelfSenseIxTotalAdj = 0;
	todoDefault.SelfSenseCx1 = 0;
	todoDefault.SelfSenseCx2 = 0;
	todoDefault.SelfSenseCx2Adj = 0;
	todoDefault.SelfSenseCxTotal = 0;
	todoDefault.SelfSenseCxTotalAdj = 0;

	if (numberParam >= 1 && functionToTest != NULL) {
		res = fts_disableInterrupt();

		if (res < 0) {
			log_debug("%s stm_driver_test_show: ERROR %08X\n", tag, res);
			res = (res | ERROR_DISABLE_INTER);
			goto END;
		}

		switch (functionToTest[0]) {
		case CMD_READ:
			if (numberParam >= 4) {
				temp = (int) functionToTest[1];

				if (numberParam == 4 + (temp - 1) && temp != 0) {
					cmd = (u8 *) kmalloc(temp * sizeof(u8), GFP_KERNEL);

					for (i = 0; i < temp; i++)
						cmd[i] = functionToTest[i + 2];

					byteToRead = functionToTest[i + 2];
					readData = (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
					res = fts_readCmd(cmd, temp, readData, byteToRead);
					size += (byteToRead * sizeof(u8)) * 2;
				} else {
					log_error("%s Wrong parameters!\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_WRITE:
			if (numberParam >= 3) {
				temp = (int) functionToTest[1];

				if (numberParam == 3 + (temp - 1) && temp != 0) {
					cmd = (u8 *) kmalloc(temp * sizeof(u8), GFP_KERNEL);

					for (i = 0; i < temp; i++) {
						cmd[i] = functionToTest[i + 2];
					}

					res = fts_writeCmd(cmd, temp);
				} else {
					log_error("%s Wrong parameters!\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_FWWRITE:
			if (numberParam >= 3) {
				temp = (int) functionToTest[1];

				if (numberParam == 3 + (temp - 1) && temp != 0) {
					cmd = (u8 *) kmalloc(temp * sizeof(u8), GFP_KERNEL);

					for (i = 0; i < temp; i++)
						cmd[i] = functionToTest[i + 2];

					res = fts_writeFwCmd(cmd, temp);
				} else {
					log_error("%s Wrong parameters!\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READU16:
			if (numberParam == 6) {
				byteToRead = functionToTest[4];
				readData = (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
				res = readCmdU16((u8) functionToTest[1], (u16)((((u8) functionToTest[2] & 0x00FF) << 8) + ((u8) functionToTest[3] & 0x00FF)), readData, byteToRead, functionToTest[5]);
				size += (byteToRead * sizeof(u8)) * 2;
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READB2:
			if (numberParam == 4) {
				byteToRead = functionToTest[3];
				readData = (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
				res = readB2((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), readData, byteToRead);
				size += (byteToRead * sizeof(u8)) * 2;
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READB2U16:
			if (numberParam == 4) {
				byteToRead = functionToTest[3];
				readData = (u8 *) kmalloc(byteToRead * sizeof(u8), GFP_KERNEL);
				res = readB2U16((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), readData, byteToRead);
				size += (byteToRead * sizeof(u8)) * 2;
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_POLLFOREVENT:
			if (numberParam >= 5) {
				temp = (int) functionToTest[1];

				if (numberParam == 5 + (temp - 1) && temp != 0) {
					readData = (u8 *) kmalloc(FIFO_EVENT_SIZE * sizeof(u8), GFP_KERNEL);
					res = pollForEvent((int *) &functionToTest[2], temp, readData, ((functionToTest[temp + 2] & 0x00FF) << 8) + (functionToTest[temp + 3] & 0x00FF));

					if (res >= OK)
						res = OK;

					size += (FIFO_EVENT_SIZE * sizeof(u8)) * 2;
					byteToRead = FIFO_EVENT_SIZE;
				} else {
					log_error("%s Wrong parameters!\n", tag);
					res = ERROR_OP_NOT_ALLOW;
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_SYSTEMRESET:
			res = fts_system_reset();
			break;

		case CMD_READCHIPINFO:
			if (numberParam == 2) {
				res = readChipInfo(functionToTest[1]);
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_CLEANUP:
			if (numberParam == 2) {
				res = cleanUp(functionToTest[1]);
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_GETFORCELEN:
			temp = getForceLen();

			if (temp < OK)
				res = temp;
			else {
				size += (1 * sizeof(u8)) * 2;
				res = OK;
			}

			break;

		case CMD_GETSENSELEN:
			temp = getSenseLen();

			if (temp < OK)
				res = temp;
			else {
				size += (1 * sizeof(u8)) * 2;
				res = OK;
			}

			break;

		case CMD_REQFRAME:
			if (numberParam == 3) {
				log_debug("%s Requesting Frame\n", tag);
				res = requestFrame((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)));

				if (res < OK) {
					log_debug("%s Error requesting frame ERROR %02X\n", tag, res);
				} else {
					log_debug("%s Requesting Frame Finished!\n", tag);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_GETMSFRAME:
			if (numberParam == 3) {
				log_debug("%s Get 1 MS Frame\n", tag);
				flushFIFO();
				res = getMSFrame2((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &frameMS);

				if (res < 0)
					log_debug("%s Error while taking the MS frame... ERROR %02X\n", tag, res);
				else {
					log_debug("%s The frame size is %d words\n", tag, res);
					size = (res * sizeof(short) + 8) * 2;
					res = OK;
					print_frame_short("MS frame =", array1dTo2d_short(frameMS.node_data, frameMS.node_data_size, frameMS.header.sense_node), frameMS.header.force_node, frameMS.header.sense_node);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_GETSSFRAME:
			if (numberParam == 3) {
				log_debug("%s Get 1 SS Frame\n", tag);
				flushFIFO();
				res = getSSFrame2((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &frameSS);

				if (res < OK) {
					log_debug("%s Error while taking the SS frame... ERROR %02X\n", tag, res);
				} else {
					log_debug("%s The frame size is %d words\n", tag, res);
					size = (res * sizeof(short) + 8) * 2 + 1;
					res = OK;
					print_frame_short("SS force frame =", array1dTo2d_short(frameSS.force_data, frameSS.header.force_node, 1), frameSS.header.force_node, 1);
					print_frame_short("SS sense frame =", array1dTo2d_short(frameSS.sense_data, frameSS.header.sense_node, frameSS.header.sense_node), 1, frameSS.header.sense_node);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_REQCOMPDATA:
			if (numberParam == 3) {
				log_debug("%s Requesting Compensation Data\n", tag);
				res = requestCompensationData((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)));

				if (res < OK) {
					log_debug("%s Error requesting compensation data ERROR %02X\n", tag, res);
				} else {
					log_debug("%s Requesting Compensation Data Finished!\n", tag);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READCOMPDATAHEAD:
			if (numberParam == 3) {
				log_debug("%s Requesting Compensation Data\n", tag);
				res = requestCompensationData((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)));

				if (res < OK) {
					log_debug("%s Error requesting compensation data ERROR %02X\n", tag, res);
				} else {
					log_debug("%s Requesting Compensation Data Finished!\n", tag);
					res = readCompensationDataHeader((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &dataHead, &address);

					if (res < OK) {
						log_debug("%s Read Compensation Data Header ERROR %02X\n", tag, res);
					} else {
						log_debug("%s Read Compensation Data Header OK!\n", tag);
						size += (2 * sizeof(u8)) * 2;
					}
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READMSCOMPDATA:
			if (numberParam == 3) {
				log_debug("%s Get MS Compensation Data\n", tag);
				res = readMutualSenseCompensationData((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &compData);

				if (res < OK) {
					log_debug("%s Error reading MS compensation data ERROR %02X\n", tag, res);
				} else {
					log_debug("%s MS Compensation Data Reading Finished!\n", tag);
					size = ((compData.node_data_size + 9) * sizeof(u8)) * 2;
					print_frame_u8("MS Data (Cx2) =", array1dTo2d_u8(compData.node_data, compData.node_data_size, compData.header.sense_node), compData.header.force_node, compData.header.sense_node);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READSSCOMPDATA:
			if (numberParam == 3) {
				log_debug("%s Get SS Compensation Data...\n", tag);
				res = readSelfSenseCompensationData((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &comData);

				if (res < OK) {
					log_debug("%s Error reading SS compensation data ERROR %02X\n", tag, res);
				} else {
					log_debug("%s SS Compensation Data Reading Finished!\n", tag);
					size = ((comData.header.force_node + comData.header.sense_node) * 2 + 12) * sizeof(u8) * 2;
					print_frame_u8("SS Data Ix2_fm = ", array1dTo2d_u8(comData.ix2_fm, comData.header.force_node, comData.header.force_node), 1, comData.header.force_node);
					print_frame_u8("SS Data Cx2_fm = ", array1dTo2d_u8(comData.cx2_fm, comData.header.force_node, comData.header.force_node), 1, comData.header.force_node);
					print_frame_u8("SS Data Ix2_sn = ", array1dTo2d_u8(comData.ix2_sn, comData.header.sense_node, comData.header.sense_node), 1, comData.header.sense_node);
					print_frame_u8("SS Data Cx2_sn = ", array1dTo2d_u8(comData.cx2_sn, comData.header.sense_node, comData.header.sense_node), 1, comData.header.sense_node);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_READGNCOMPDATA:
			if (numberParam == 3) {
				log_debug("%s Get General Compensation Data...\n", tag);
				res = readGeneralCompensationData((u16)((((u8) functionToTest[1] & 0x00FF) << 8) + ((u8) functionToTest[2] & 0x00FF)), &gnData);

				if (res < OK) {
					log_debug("%s Error reading General compensation data ERROR %02X\n", tag, res);
				} else {
					log_debug("%s General Compensation Data Reading Finished!\n", tag);
					size = (14) * sizeof(u8) * 2;
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_GETFWVER:
			res = getFirmwareVersion(&fw_version, &config_id);

			if (res < OK) {
				log_error("%s Error reading firmware version and config id ERROR %02X\n", tag, res);
			} else {
				log_debug("%s getFirmwareVersion Finished!\n", tag);
				size += (4) * sizeof(u8) * 2;
			}

			break;
#ifdef FTM3_CHIP

		case CMD_FLASHSTATUS:
			res = flash_status();

			if (res < OK) {
				log_error("%s Error reading flash status ERROR %02X\n", tag, res);
			} else {
				log_debug("%s Flash Status: %d\n", tag, res);
				size += (1 * sizeof(u8)) * 2;
				temp = res;
				res = OK;
			}

			break;
#endif

		case CMD_FLASHUNLOCK:
			res = flash_unlock();

			if (res < OK) {
				log_error("%s Impossible Unlock Flash ERROR %02X\n", tag, res);
			} else {
				log_debug("%s Flash Unlock OK!\n", tag);
			}

			break;

		case CMD_READFWFILE:
			if (numberParam == 2) {
				log_debug("%s Reading FW File...\n", tag);
				res = readFwFile(PATH_FILE_FW, &fw, functionToTest[1]);

				if (res < OK)
					log_debug("%s Error reading FW File ERROR %02X\n", tag, res);
				else
					log_debug("%s Read FW File Finished!\n", tag);

				kfree(fw.data);
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_FLASHPROCEDURE:
			if (numberParam == 3) {
				log_debug("%s Starting Flashing Procedure...\n", tag);
				res = flashProcedure(PATH_FILE_FW, functionToTest[1], functionToTest[2]);

				if (res < OK) {
					log_debug("%s Error during flash procedure ERROR %02X\n", tag, res);
				} else {
					log_debug("%s Flash Procedure Finished!\n", tag);
				}
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_ITOTEST:
			res = production_test_ito();
			break;

		case CMD_INITTEST:
			if (numberParam == 2) {
				if (functionToTest[1] == 0x01)
					res = production_test_initialization();
				else
					res = production_test_splitted_initialization(false);
			} else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_MSRAWTEST:
			if (numberParam == 2)
				res = production_test_ms_raw(LIMITS_FILE, functionToTest[1], &todoDefault);
			else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_MSINITDATATEST:
			if (numberParam == 2)
				res = production_test_ms_cx(LIMITS_FILE, functionToTest[1], &todoDefault);
			else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_SSRAWTEST:
			if (numberParam == 2)
				res = production_test_ss_raw(LIMITS_FILE, functionToTest[1], &todoDefault);
			else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_SSINITDATATEST:
			if (numberParam == 2)
				res = production_test_ss_ix_cx(LIMITS_FILE, functionToTest[1], &todoDefault);
			else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_MAINTEST:
			if (numberParam == 3)
				res = production_test_main(LIMITS_FILE, functionToTest[1], functionToTest[2], &todoDefault, INIT_FIELD);
			else {
				log_error("%s Wrong number of parameters!\n", tag);
				res = ERROR_OP_NOT_ALLOW;
			}

			break;

		case CMD_POWERCYCLE:
			res = fts_chip_powercycle(info);
			break;

		default:
			log_error("%s COMMAND ID NOT VALID!! Inset a value between 00 and 1E..\n", tag);
			res = ERROR_OP_NOT_ALLOW;
			break;
		}
	} else {
		log_error("%s NO COMMAND SPECIFIED!!! do: 'echo [cmd_code] [args] > stm_fts_cmd' before looking for result!\n", tag);
		res = ERROR_OP_NOT_ALLOW;
		functionToTest = NULL;
	}

END:
	all_strbuff = (u8 *) kmalloc(size, GFP_KERNEL);
	memset(all_strbuff, 0, size);
	snprintf(buff, sizeof(buff), "%02X", 0xAA);
	strlcat(all_strbuff, buff, size);
	snprintf(buff, sizeof(buff), "%08X", res);
	strlcat(all_strbuff, buff, size);

	if (res >= OK) {
		switch (functionToTest[0]) {
		case CMD_READ:
		case CMD_READU16:
		case CMD_READB2:
		case CMD_READB2U16:
		case CMD_POLLFOREVENT:
			for (j = 0; j < byteToRead; j++) {
				snprintf(buff, sizeof(buff), "%02X", readData[j]);
				strlcat(all_strbuff, buff, size);
			}

			break;

		case CMD_GETFORCELEN:
		case CMD_GETSENSELEN:
		case CMD_FLASHSTATUS:
			snprintf(buff, sizeof(buff), "%02X", (u8) temp);
			strlcat(all_strbuff, buff, size);
			break;

		case CMD_GETMSFRAME:
			snprintf(buff, sizeof(buff), "%02X", (u8) frameMS.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", (u8) frameMS.header.sense_node);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < frameMS.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%04X", frameMS.node_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(frameMS.node_data);
			break;

		case CMD_GETSSFRAME:
			snprintf(buff, sizeof(buff), "%02X", (u8) frameSS.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", (u8) frameSS.header.sense_node);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < frameSS.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%04X", frameSS.force_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < frameSS.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%04X", frameSS.sense_data[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(frameSS.force_data);
			kfree(frameSS.sense_data);
			break;

		case CMD_READMSCOMPDATA:
			snprintf(buff, sizeof(buff), "%02X", (u8) compData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", (u8) compData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", compData.cx1);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < compData.node_data_size; j++) {
				snprintf(buff, sizeof(buff), "%02X", *(compData.node_data + j));
				strlcat(all_strbuff, buff, size);
			}

			kfree(compData.node_data);
			break;

		case CMD_READSSCOMPDATA:
			snprintf(buff, sizeof(buff), "%02X", comData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.f_ix1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.s_ix1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.f_cx1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", comData.s_cx1);
			strlcat(all_strbuff, buff, size);

			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.ix2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.force_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.cx2_fm[j]);
				strlcat(all_strbuff, buff, size);
			}

			for (j = 0; j < comData.header.sense_node; j++) {
				snprintf(buff, sizeof(buff), "%02X", comData.cx2_sn[j]);
				strlcat(all_strbuff, buff, size);
			}

			kfree(comData.ix2_fm);
			kfree(comData.ix2_sn);
			kfree(comData.cx2_fm);
			kfree(comData.cx2_sn);
			break;

		case CMD_READGNCOMPDATA:
			snprintf(buff, sizeof(buff), "%02X", gnData.header.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.header.sense_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsd_lp_timer_cal0);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsd_lp_timer_cal1);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsd_lp_timer_cal2);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsd_lp_timer_cal3);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsa_lp_timer_cal0);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", gnData.ftsa_lp_timer_cal1);
			strlcat(all_strbuff, buff, size);
			break;

		case CMD_GETFWVER:
			snprintf(buff, sizeof(buff), "%04X", fw_version);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%04X", config_id);
			strlcat(all_strbuff, buff, size);
			break;

		case CMD_READCOMPDATAHEAD:
			snprintf(buff, sizeof(buff), "%02X", dataHead.force_node);
			strlcat(all_strbuff, buff, size);
			snprintf(buff, sizeof(buff), "%02X", dataHead.sense_node);
			strlcat(all_strbuff, buff, size);
			break;

		default:
			break;
		}
	}

	snprintf(buff, sizeof(buff), "%02X", 0xBB);
	strlcat(all_strbuff, buff, size);
	count = snprintf(buf, PAGE_SIZE, "%s\n", all_strbuff);
	numberParam = 0;
	kfree(functionToTest);
	return count;
}

static DEVICE_ATTR(stm_driver_test, S_IWUSR | S_IRUGO, stm_driver_test_show, stm_driver_test_store);

static struct attribute *test_cmd_attributes[] = {
	&dev_attr_stm_driver_test.attr,
	NULL,
};

struct attribute_group test_cmd_attr_group = {
	.attrs = test_cmd_attributes,
};

#endif
