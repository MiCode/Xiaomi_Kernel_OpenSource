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


#define BYTES_PER_NODE							2

#define RETRY_FRAME_DATA_READ					2

/**
* Possible types of MS frames
*/
typedef enum {
	MS_RAW = 0,
	MS_FILTER = 1,
	MS_STRENGHT = 2,
	MS_BASELINE = 3,
	MS_KEY_RAW = 4,
	MS_KEY_FILTER = 5,
	MS_KEY_STRENGHT = 6,
	MS_KEY_BASELINE = 7,
	FRC_RAW = 8,
	FRC_FILTER = 9,
	FRC_STRENGHT = 10,
	FRC_BASELINE = 11
} MSFrameType;

/**
* Possible types of SS frames
*/
typedef enum {
	SS_RAW = 0,
	SS_FILTER = 1,
	SS_STRENGHT = 2,
	SS_BASELINE = 3,
	SS_HVR_RAW = 4,
	SS_HVR_FILTER = 5,
	SS_HVR_STRENGHT = 6,
	SS_HVR_BASELINE = 7,
	SS_PRX_RAW = 8,
	SS_PRX_FILTER = 9,
	SS_PRX_STRENGHT = 10,
	SS_PRX_BASELINE = 11
} SSFrameType;

/**
* Struct which contains the data of a MS Frame
*/
typedef struct {
	DataHeader header;
	short *node_data;
	int node_data_size;
} MutualSenseFrame;

/**
* Struct which contains the data of a SS Frame
*/
typedef struct {
	DataHeader header;
	short *force_data;
	short *sense_data;
} SelfSenseFrame;

int getChannelsLength(void);
int getFrameData(u16 address, int size, short *frame);
int getSenseLen(void);
int getForceLen(void);
int getMSFrame3(MSFrameType type, MutualSenseFrame *frame);
int getSSFrame3(SSFrameType type, SelfSenseFrame *frame);
#endif
