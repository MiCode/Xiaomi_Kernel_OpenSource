/**
 *
 * Synaptics Register Mapped Interface (RMI4) Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

#ifndef _RMI_H
#define _RMI_H

/*  RMI4 Protocol Support
 */

/* For each function present on the RMI device, we need to get the RMI4 Function
 * Descriptor info from the Page Descriptor Table. This will give us the
 * addresses for Query, Command, Control, Data and the Source Count (number
 * of sources for this function) and the function id.
 */
struct rmi_function_descriptor {
	unsigned char queryBaseAddr;
	unsigned char commandBaseAddr;
	unsigned char controlBaseAddr;
	unsigned char dataBaseAddr;
	unsigned char interruptSrcCnt;
	unsigned char functionNum;
};

/*  This encapsulates the information found using the RMI4 Function $01
 *  query registers. There is only one Function $01 per device.
 *
 *  Assuming appropriate endian-ness, you can populate most of this
 *  structure by reading query registers starting at the query base address
 *  that was obtained from RMI4 function 0x01 function descriptor info read
 *  from the Page Descriptor Table.
 *
 *  Specific register information is provided in the comments for each field.
 *  For further reference, please see the "Synaptics RMI 4 Interfacing
 *  Guide" document : go to http://www.synaptics.com/developers/manuals - and
 *  select "Synaptics RMI 4 Interfacting Guide".
 */
struct rmi_F01_query {
	/* The manufacturer identification byte.*/
	unsigned char mfgid;

	/* The Product Properties information.*/
	unsigned char properties;

	/* The product info bytes.*/
	unsigned char prod_info[2];

	/* Date Code - Year, Month, Day.*/
	unsigned char date_code[3];

	/* Tester ID (14 bits).*/
	unsigned short tester_id;

	/* Serial Number (14 bits).*/
	unsigned short serial_num;

	/* A null-terminated string that identifies this particular product.*/
	char prod_id[11];
};

/* This encapsulates the F01 Device Control control registers.
 * TODO: This isn't right.  The number of interrupt enables needs to be determined
 * dynamically as the sensor is initialized.  Fix this.
 */
struct rmi_F01_control {
    unsigned char deviceControl;
    unsigned char interruptEnable[1];
};

/** This encapsulates the F01 Device Control data registers.
 * TODO: This isn't right.  The number of irqs needs to be determined
 * dynamically as the sensor is initialized.  Fix this.
 */
struct rmi_F01_data {
    unsigned char deviceStatus;
    unsigned char irqs[1];
};


/**********************************************************/

/** This is the data read from the F11 query registers.
 */
struct rmi_F11_device_query {
    bool hasQuery9;
    unsigned char numberOfSensors;
};

struct rmi_F11_sensor_query {
    bool configurable;
    bool hasSensitivityAdjust;
    bool hasGestures;
    bool hasAbs;
    bool hasRel;
    unsigned char numberOfFingers;
    unsigned char numberOfXElectrodes;
    unsigned char numberOfYElectrodes;
    unsigned char maximumElectrodes;
    bool hasAnchoredFinger;
    unsigned char absDataSize;
};

struct rmi_F11_control {
    bool relativeBallistics;
    bool relativePositionFilter;
    bool absolutePositionFilter;
    unsigned char reportingMode;
    bool manuallyTrackedFinger;
    bool manuallyTrackedFingerEnable;
    unsigned char motionSensitivity;
    unsigned char palmDetectThreshold;
    unsigned char deltaXPosThreshold;
    unsigned char deltaYPosThreshold;
    unsigned char velocity;
    unsigned char acceleration;
    unsigned short sensorMaxXPos;
    unsigned short sensorMaxYPos;
};


/**********************************************************/

/** This is the data read from the F19 query registers.
 */
struct rmi_F19_query {
	bool hasHysteresisThreshold;
	bool hasSensitivityAdjust;
	bool configurable;
	unsigned char buttonCount;
};

struct rmi_F19_control {
	unsigned char buttonUsage;
	unsigned char filterMode;
	unsigned char *intEnableRegisters;
	unsigned char *singleButtonControl;
	unsigned char *sensorMap;
	unsigned char *singleButtonSensitivity;
	unsigned char globalSensitivityAdjustment;
	unsigned char globalHysteresisThreshold;
};

#endif
