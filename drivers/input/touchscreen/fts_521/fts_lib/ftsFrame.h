/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*                  FTS functions for getting frames						 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsFrame.h
* \brief Contains all the definitions and structs to work with frames
*/

#ifndef FTS_FRAME_H
#define FTS_FRAME_H

#include "ftsSoftware.h"


#define BYTES_PER_NODE							2	/*number of data bytes for each node*/

#define RETRY_FRAME_DATA_READ					2	/*max number of attempts to read a frame*/

/**
* Possible types of MS frames
*/
typedef enum{
	MS_RAW = 0,																	/*Mutual Sense Raw Frame*/
	MS_FILTER = 1,																/*Mutual Sense Filtered Frame*/
	MS_STRENGTH = 2,															/*Mutual Sense Strength Frame (Baseline-Raw)*/
	MS_BASELINE = 3,															/*Mutual Sense Baseline Frame*/
	MS_KEY_RAW = 4,																/*Mutual Sense Key Raw Frame*/
	MS_KEY_FILTER = 5,															/*Mutual Sense Key Filter Frame*/
	MS_KEY_STRENGTH = 6,														/*Mutual Sense Key Strength Frame (Baseline-Raw)*/
	MS_KEY_BASELINE = 7,														/*Mutual Sense Key Baseline Frame*/
	FRC_RAW = 8,																/*Force Raw Frame*/
	FRC_FILTER = 9,																/*Force Filtered Frame*/
	FRC_STRENGTH = 10,															/*Force Strength Frame (Baseline-Raw)*/
	FRC_BASELINE = 11															/*Force Baseline Frame*/
} MSFrameType;

/**
* Possible types of SS frames
*/
typedef enum{
	SS_RAW = 0,																	/*Self Sense Raw Frame*/
	SS_FILTER = 1,																/*Self Sense Filtered Frame*/
	SS_STRENGTH = 2,															/*Self Sense Strength Frame (Baseline-Raw)*/
	SS_BASELINE = 3,															/*Self Sense Baseline Frame*/
	SS_HVR_RAW = 4,																/*Self Sense Hover Raw Frame*/
	SS_HVR_FILTER = 5,															/*Self Sense Hover Filter Frame*/
	SS_HVR_STRENGTH = 6,														/*Self Sense Hover Strength Frame (Baseline-Raw)*/
	SS_HVR_BASELINE = 7,														/*Self Sense Hover Baseline Frame*/
	SS_PRX_RAW = 8,																/*Self Sense Proximity Raw Frame*/
	SS_PRX_FILTER = 9,															/*Self Sense Proximity Filtered Frame*/
	SS_PRX_STRENGTH = 10,														/*Self Sense Proximity Strength Frame (Baseline-Raw)*/
	SS_PRX_BASELINE = 11														/*Self Sense Proximity Baseline Frame*/
}SSFrameType;

/**
* Struct which contains the data of a MS Frame
*/
typedef struct {
	DataHeader header;	/*Header which contain basic info of the frame*/
	short *node_data;	/*Data of the frame*/
	int node_data_size;	/*Dimension of the data of the frame*/
} MutualSenseFrame;

/**
* Struct which contains the data of a SS Frame
*/
typedef struct {
	DataHeader header;	/*Header which contain basic info of the frame*/
	short *force_data;	/*Force Channels Data*/
	short *sense_data;	/*Sense Channels Data*/
} SelfSenseFrame;

int getChannelsLength(void);
int getFrameData(u16 address, int size, short *frame);
int getSenseLen(void);
int getForceLen(void);
int getMSFrame3(MSFrameType type, MutualSenseFrame *frame);
int getSSFrame3(SSFrameType type, SelfSenseFrame *frame);
#endif
