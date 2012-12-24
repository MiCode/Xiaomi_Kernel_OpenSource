/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Copyright © 2011, 2012 Synaptics Incorporated. All rights reserved.
 *
 * The information in this file is confidential under the terms
 * of a non-disclosure agreement with Synaptics and is provided
 * AS IS without warranties or guarantees of any kind.
 *
 * The information in this file shall remain the exclusive property
 * of Synaptics and may be the subject of Synaptics patents, in
 * whole or part. Synaptics intellectual property rights in the
 * information in this file are not expressly or implicitly licensed
 * or otherwise transferred to you as a result of such information
 * being made available to you.
 *
 * File: synaptics_fw_updater.c
 *
 * Description: command line reflash implimentation using command
 * line args. This file should not be OS dependant and should build and
 * run under any Linux based OS that utilizes the Synaptice rmi driver
 * built into the kernel (kernel/drivers/input/rmi4).
 *
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#define DEFAULT_SENSOR "/sys/class/input/input1"

#define MAX_STRING_LEN 256
#define MAX_INT_LEN 33

#define DATA_FILENAME "data"
#define IMAGESIZE_FILENAME "imagesize"
#define DOREFLASH_FILENAME "doreflash"
#define CONFIGAREA_FILENAME "configarea"
#define READCONFIG_FILENAME "readconfig"
#define WRITECONFIG_FILENAME "writeconfig"
#define BLOCKSIZE_FILENAME "blocksize"
#define IMAGEBLOCKCOUNT_FILENAME "fwblockcount"
#define CONFIGBLOCKCOUNT_FILENAME "configblockcount"
#define PMCONFIGBLOCKCOUNT_FILENAME "permconfigblockcount"
#define BUILDID_FILENAME "buildid"
#define FLASHPROG_FILENAME "flashprog"

#define UI_CONFIG_AREA 0
#define PERM_CONFIG_AREA 1
#define BL_CONFIG_AREA 2
#define DISP_CONFIG_AREA 3

#define IMAGE_FILE_CHECKSUM_SIZE 4

unsigned char *firmware = NULL;
int fileSize;
int firmwareBlockSize;
int firmwareBlockCount;
int firmwareImgSize;
int configBlockSize;
int configBlockCount;
int configImgSize;
int totalBlockCount;
int readConfig = 0;
int writeConfig = 0;
int uiConfig = 0;
int pmConfig = 0;
int blConfig = 0;
int dpConfig = 0;
int force = 0;
int verbose = 0;

char mySensor[MAX_STRING_LEN];
char imageFileName[MAX_STRING_LEN];

static void usage(char *name)
{
	printf("Usage: %s [-b {image_file}] [-d {sysfs_entry}] [-r] [-ui] [-pm] [-bl] [-dp] [-f] [-v]\n", name);
	printf("\t[-b {image_file}] - Name of image file\n");
	printf("\t[-d {sysfs_entry}] - Path to sysfs entry of sensor\n");
	printf("\t[-r] - Read config area\n");
	printf("\t[-ui] - UI config area\n");
	printf("\t[-pm] - Permanent config area\n");
	printf("\t[-bl] - BL config area\n");
	printf("\t[-dp] - Display config area\n");
	printf("\t[-f] - Force reflash\n");
	printf("\t[-v] - Verbose output\n");

	return;
}

static void TimeSubtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	if (x->tv_usec < y->tv_usec) {
		result->tv_sec = x->tv_sec - y->tv_sec - 1;
		result->tv_usec = y->tv_usec - x->tv_usec;
	} else {
		result->tv_sec = x->tv_sec - y->tv_sec;
		result->tv_usec = x->tv_usec - y->tv_usec;
	}

	return;
}

static int CheckSysfsEntry(char *sensorName)
{
	int retval;
	struct stat st;

	retval = stat(sensorName, &st);
	if (retval)
		printf("ERROR: sensor sysfs entry %s not found\n", sensorName);

	return retval;
}

static void WriteBinData(char *fname, unsigned char *buf, int len)
{
	int numBytesWritten;
	FILE *fp;

	fp = fopen(fname, "wb");
	if (!fp) {
		printf("ERROR: failed to open %s for writing data\n", fname);
		exit(EIO);
	}

	numBytesWritten = fwrite(buf, 1, len, fp);

	if (numBytesWritten != len) {
		printf("ERROR: failed to write all data to bin file\n");
		fclose(fp);
		exit(EIO);
	}

	fclose(fp);

	return;
}

static void ReadBinData(char *fname, unsigned char *buf, int len)
{
	int numBytesRead;
	FILE *fp;

	fp = fopen(fname, "rb");
	if (!fp) {
		printf("ERROR: failed to open %s for reading data\n", fname);
		exit(EIO);
	}

	numBytesRead = fread(buf, 1, len, fp);

	if (numBytesRead != len) {
		printf("ERROR: failed to read all data from bin file\n");
		fclose(fp);
		exit(EIO);
	}

	fclose(fp);

	return;
}

static void WriteValueToFp(FILE *fp, unsigned int value)
{
	int numBytesWritten;
	char buf[MAX_INT_LEN];

	snprintf(buf, MAX_INT_LEN, "%u", value);

	fseek(fp, 0, 0);

	numBytesWritten = fwrite(buf, 1, strlen(buf) + 1, fp);
	if (numBytesWritten != ((int)(strlen(buf) + 1))) {
		printf("ERROR: failed to write value to file pointer\n");
		fclose(fp);
		exit(EIO);
	}

	return;
}

static void WriteValueToSysfsFile(char *fname, unsigned int value)
{
	FILE *fp;

	fp = fopen(fname, "w");
	if (!fp) {
		printf("ERROR: failed to open %s for writing value\n", fname);
		exit(EIO);
	}

	WriteValueToFp(fp, value);

	fclose(fp);

	return;
}

static void ReadValueFromFp(FILE *fp, unsigned int *value)
{
	int retVal;
	char buf[MAX_INT_LEN];

	fseek(fp, 0, 0);

	retVal = fread(buf, 1, sizeof(buf), fp);
	if (retVal == -1) {
		printf("ERROR: failed to read value from file pointer\n");
		exit(EIO);
	}

	*value = strtoul(buf, NULL, 0);

	return;
}

static void ReadValueFromSysfsFile(char *fname, unsigned int *value)
{
	FILE *fp;

	fp = fopen(fname, "r");
	if (!fp) {
		printf("ERROR: failed to open %s for reading value\n", fname);
		exit(EIO);
	}

	ReadValueFromFp(fp, value);

	fclose(fp);

	return;
}

static void WriteBlockData(char *buf, int len)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, DATA_FILENAME);

	WriteBinData(tmpfname, (unsigned char *)buf, len);

	return;
}

static void ReadBlockData(char *buf, int len)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, DATA_FILENAME);

	ReadBinData(tmpfname, (unsigned char *)buf, len);

	return;
}

static void SetImageSize(int value)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, IMAGESIZE_FILENAME);

	WriteValueToSysfsFile(tmpfname, value);

	return;
}

static void StartReflash(int value)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, DOREFLASH_FILENAME);

	WriteValueToSysfsFile(tmpfname, value);

	return;
}

static void SetConfigArea(int value)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, CONFIGAREA_FILENAME);

	WriteValueToSysfsFile(tmpfname, value);

	return;
}

static void StartWriteConfig(int value)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, WRITECONFIG_FILENAME);

	WriteValueToSysfsFile(tmpfname, value);

	return;
}

static void StartReadConfig(int value)
{
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, READCONFIG_FILENAME);

	WriteValueToSysfsFile(tmpfname, value);

	return;
}

static int ReadBlockSize(void)
{
	unsigned int blockSize;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, BLOCKSIZE_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &blockSize);

	return blockSize;
}

static int ReadFirmwareBlockCount(void)
{
	unsigned int imageBlockCount;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, IMAGEBLOCKCOUNT_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &imageBlockCount);

	return imageBlockCount;
}

static int ReadConfigBlockCount(void)
{
	unsigned int configBlockCount;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, CONFIGBLOCKCOUNT_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &configBlockCount);

	return configBlockCount;
}

static int ReadPmConfigBlockCount(void)
{
	unsigned int configBlockCount;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, PMCONFIGBLOCKCOUNT_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &configBlockCount);

	return configBlockCount;
}

static int ReadBuildID(void)
{
	unsigned int buildID;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, BUILDID_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &buildID);

	return buildID;
}

static int ReadFlashProg(void)
{
	unsigned int flashProg;
	char tmpfname[MAX_STRING_LEN];

	snprintf(tmpfname, MAX_STRING_LEN, "%s/%s", mySensor, FLASHPROG_FILENAME);

	ReadValueFromSysfsFile(tmpfname, &flashProg);

	return flashProg;
}

static void ReadFirmwareInfo(void)
{
	firmwareBlockSize = ReadBlockSize();
	firmwareBlockCount = ReadFirmwareBlockCount();
	firmwareImgSize = firmwareBlockCount * firmwareBlockSize;

	return;
}

static void ReadConfigInfo(void)
{
	configBlockSize = ReadBlockSize();
	configBlockCount = ReadConfigBlockCount();
	configImgSize = configBlockSize * configBlockCount;

	return;
}

static void CalculateChecksum(unsigned short *data, unsigned short len, unsigned long *result)
{
	unsigned long temp;
	unsigned long sum1 = 0xffff;
	unsigned long sum2 = 0xffff;

	*result = 0xffffffff;

	while (len--) {
		temp = *data;
		sum1 += temp;
		sum2 += sum1;
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		data++;
	}

	*result = sum2 << 16 | sum1;

	return;
}

static int CompareChecksum(void)
{
	unsigned long headerChecksum;
	unsigned long computedChecksum;

	headerChecksum = (unsigned long)firmware[0] +
			(unsigned long)firmware[1] * 0x100 +
			(unsigned long)firmware[2] * 0x10000 +
			(unsigned long)firmware[3] * 0x1000000;

	CalculateChecksum((unsigned short *)&firmware[IMAGE_FILE_CHECKSUM_SIZE],
			((fileSize - IMAGE_FILE_CHECKSUM_SIZE) / 2), &computedChecksum);

	if (verbose) {
		printf("Checksum in image file header = 0x%08x\n", (unsigned int)headerChecksum);
		printf("Checksum computed from image file = 0x%08x\n", (unsigned int)computedChecksum);
	}

	if (headerChecksum == computedChecksum)
		return 1;
	else
		return 0;
}

static int ProceedWithReflash(void)
{
	int index = 0;
	int deviceBuildID;
	int imageBuildID;
	char imagePR[MAX_STRING_LEN];
	char *strptr;

	if (force) {
		printf("Force reflash...\n");
		return 1;
	}

	if (ReadFlashProg()) {
		printf("Force reflash (device in flash prog mode)...\n");
		return 1;
	}

	strptr = strstr(imageFileName, "PR");
	if (!strptr) {
		printf("No valid PR number (PRxxxxxxx) found in image file name...\n");
		return 0;
	}

	strptr += 2;
	while (strptr[index] >= '0' && strptr[index] <= '9') {
		imagePR[index] = strptr[index];
		index++;
	}
	imagePR[index] = 0;

	imageBuildID = strtoul(imagePR, NULL, 0);
	deviceBuildID = ReadBuildID();
	printf("Image file PR = %d\n", imageBuildID);
	printf("Device PR = %d\n", deviceBuildID);

	if (imageBuildID > deviceBuildID) {
		printf("Proceed with reflash...\n");
		return 1;
	} else {
		printf("No need to do reflash...\n");
		return 0;
	}
}

static void DoReadConfig(void)
{
	int ii;
	int jj;
	int index = 0;
	int configSize;
	int blockCount;
	unsigned char *buffer;

	if (uiConfig) {
		SetConfigArea(UI_CONFIG_AREA);
		StartReadConfig(1);
		blockCount = configBlockCount;
		configSize = configImgSize;
		buffer = malloc(configSize);
		if (!buffer)
			exit(ENOMEM);
		ReadBlockData((char *)&buffer[0], configSize);
	} else if (pmConfig) {
		SetConfigArea(PERM_CONFIG_AREA);
		StartReadConfig(1);
		blockCount = ReadPmConfigBlockCount();
		configSize = configBlockSize * blockCount;
		buffer = malloc(configSize);
		if (!buffer)
			exit(ENOMEM);
		ReadBlockData((char *)&buffer[0], configSize);
	} else {
		return;
	}

	for (ii = 0; ii < blockCount; ii++) {
		for (jj = 0; jj < configBlockSize; jj++) {
			printf("0x%02x ", buffer[index]);
			index++;
		}
		printf("\n");
	}

	free(buffer);

	return;
}

static void DoWriteConfig(void)
{
	printf("Starting config programming...\n");

	if (uiConfig)
		SetConfigArea(UI_CONFIG_AREA);
	else if (pmConfig)
		SetConfigArea(PERM_CONFIG_AREA);
	else if (blConfig)
		SetConfigArea(BL_CONFIG_AREA);
	else if (dpConfig)
		SetConfigArea(DISP_CONFIG_AREA);
	else
		return;

	SetImageSize(fileSize);
	WriteBlockData((char *)&firmware[0], fileSize);
	StartWriteConfig(1);

	printf("Config programming completed...\n");

	return;
}

static void DoReflash(void)
{
	if (verbose)
		printf("Blocks: %d (firmware: %d, config: %d)\n", totalBlockCount, firmwareBlockCount, configBlockCount);

	if (!ProceedWithReflash())
		return;

	printf("Starting reflash...\n");

	SetImageSize(fileSize);
	WriteBlockData((char *)&firmware[0], fileSize);
	StartReflash(1);

	printf("Reflash completed...\n");

	return;
}

static int InitFirmwareImage(void)
{
	int numBytesRead;
	FILE *fp;

	if (!readConfig) {
		fp = fopen(imageFileName, "rb");

		if (!fp) {
			printf("ERROR: image file %s not found\n", imageFileName);
			exit(ENODEV);
		}

		fseek(fp, 0L, SEEK_END);
		fileSize = ftell(fp);
		if (fileSize == -1) {
			printf("ERROR: failed to determine size of %s\n", imageFileName);
			exit(EIO);
		}

		fseek(fp, 0L, SEEK_SET);

		firmware = malloc(fileSize + 1);
		if (!firmware) {
			exit(ENOMEM);
		} else {
			numBytesRead = fread(firmware, 1, fileSize, fp);
			if (numBytesRead != fileSize) {
				printf("ERROR: failed to read entire content of image file\n");
				exit(EIO);
			}
		}

		fclose(fp);

		if (!(pmConfig || blConfig || dpConfig)) {
			if (!CompareChecksum()) {
				printf("ERROR: failed to validate checksum of image file\n");
				exit(EINVAL);
			}
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	int retVal;
	int this_arg = 1;
	struct stat st;
	struct timeval start_time;
	struct timeval end_time;
	struct timeval elapsed_time;

	if (argc == 1) {
		usage(argv[0]);
		exit(EINVAL);
	}

	while (this_arg < argc) {
		if (!strcmp((const char *)argv[this_arg], "-b")) {
			/* Image file */
			FILE *file;

			this_arg++;
			if (this_arg >= argc) {
				printf("ERROR: image file missing\n");
				exit(EINVAL);
			}

			/* check for presence of image file */
			file = fopen(argv[this_arg], "rb");
			if (file == 0) {
				printf("ERROR: image file %s not found\n", argv[this_arg]);
				exit(EINVAL);
			}
			fclose(file);

			strncpy(imageFileName, argv[this_arg], MAX_STRING_LEN);
		} else if (!strcmp((const char *)argv[this_arg], "-d")) {
			/* path to sensor sysfs entry */
			this_arg++;

			if (stat(argv[this_arg], &st) == 0) {
				strncpy(mySensor, argv[this_arg], MAX_STRING_LEN);
			} else {
				printf("ERROR: sensor sysfs entry %s not found\n", argv[this_arg]);
				exit(EINVAL);
			}
		} else if (!strcmp((const char *)argv[this_arg], "-r")) {
			readConfig = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-ui")) {
			uiConfig = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-pm")) {
			pmConfig = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-bl")) {
			blConfig = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-dp")) {
			dpConfig = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-f")) {
			force = 1;
		} else if (!strcmp((const char *)argv[this_arg], "-v")) {
			verbose = 1;
		} else {
			usage(argv[0]);
			printf("ERROR: invalid parameter %s supplied\n", argv[this_arg]);
			exit(EINVAL);
		}
		this_arg++;
	}

	if ((uiConfig + pmConfig + blConfig + dpConfig) > 1) {
		printf("ERROR: too many parameters\n");
		exit(EINVAL);
	}

	if (uiConfig || pmConfig || blConfig || dpConfig)
		writeConfig = 1;

	if (!readConfig && !strlen(imageFileName)) {
		printf("ERROR: no image file specified\n");
		exit(EINVAL);
	}

	if (!strlen(mySensor))
		strncpy(mySensor, DEFAULT_SENSOR, MAX_STRING_LEN);

	if (CheckSysfsEntry(mySensor))
		exit(ENODEV);

	InitFirmwareImage();

	ReadFirmwareInfo();
	ReadConfigInfo();
	totalBlockCount = configBlockCount + firmwareBlockCount;

	retVal = gettimeofday(&start_time, NULL);
	if (retVal)
		printf("WARNING: failed to get start time\n");

	if (verbose) {
		if (!readConfig)
			printf("Image file: %s\n", imageFileName);
		printf("Sensor sysfs entry: %s\n", mySensor);
	}

	if (readConfig)
		DoReadConfig();
	else if (writeConfig)
		DoWriteConfig();
	else
		DoReflash();

	retVal = gettimeofday(&end_time, NULL);
	if (retVal)
		printf("WARNING: failed to get end time\n");

	TimeSubtract(&elapsed_time, &end_time, &start_time);

	if (verbose) {
		printf("Elapsed time = %ld.%06ld seconds\n",
				(long)elapsed_time.tv_sec,
				(long)elapsed_time.tv_usec);
	}

	return 0;
}
