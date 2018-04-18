/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*               FTS functions for getting Initialization Data			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsCompensation.h
* \brief Contains all the definitions and structs to work with Initialization Data
*/

#ifndef FTS_COMPENSATION_H
#define FTS_COMPENSATION_H

#include "ftsCore.h"
#include "ftsSoftware.h"

#define RETRY_COMP_DATA_READ			2	/



#define COMP_DATA_HEADER				DATA_HEADER	/
#define COMP_DATA_GLOBAL				16-COMP_DATA_HEADER	/

#define HEADER_SIGNATURE				0xA5	/

/**
* Struct which contains the general info about Frames and Initialization Data
*/
typedef struct {
	int force_node;		/
	int sense_node;		/
	int type;		/
} DataHeader;

/**
* Struct which contains the MS Initialization data
*/
typedef struct {
	DataHeader header;	/
	i8 cx1;			/
	i8 *node_data;		/
	int node_data_size;	/
} MutualSenseData;

/**
* Struct which contains the SS Initialization data
*/
typedef struct {
	DataHeader header;	/
	u8 f_ix1;		/
	u8 s_ix1;		/
	i8 f_cx1;		/
	i8 s_cx1;		/
	u8 f_max_n;		/
	u8 s_max_n;		/

	u8 *ix2_fm;		/
	u8 *ix2_sn;		/
	i8 *cx2_fm;		/
	i8 *cx2_sn;		/

} SelfSenseData;

/**
* Struct which contains the TOT MS Initialization data
*/
typedef struct {
	DataHeader header;	/
	short *node_data;	/
	int node_data_size;	/
} TotMutualSenseData;

/**
* Struct which contains the TOT SS Initialization data
*/
typedef struct {
	DataHeader header;	/

	u16 *ix_fm;		/
	u16 *ix_sn;		/
	short *cx_fm;		/
	short *cx_sn;		/

} TotSelfSenseData;

int requestCompensationData(u8 type);
int readCompensationDataHeader(u8 type, DataHeader *header, u64 *address);
int readMutualSenseGlobalData(u64 *address, MutualSenseData *global);
int readMutualSenseNodeData(u64 address, MutualSenseData *node);
int readMutualSenseCompensationData(u8 type, MutualSenseData *data);
int readSelfSenseGlobalData(u64 *address, SelfSenseData *global);
int readSelfSenseNodeData(u64 address, SelfSenseData *node);
int readSelfSenseCompensationData(u8 type, SelfSenseData *data);
int readToTMutualSenseGlobalData(u64 *address, TotMutualSenseData *global);
int readToTMutualSenseNodeData(u64 address, TotMutualSenseData *node);
int readTotMutualSenseCompensationData(u8 type, TotMutualSenseData *data);
int readTotSelfSenseGlobalData(u64 *address, TotSelfSenseData *global);
int readTotSelfSenseNodeData(u64 address, TotSelfSenseData *node);
int readTotSelfSenseCompensationData(u8 type, TotSelfSenseData *data);
#endif
