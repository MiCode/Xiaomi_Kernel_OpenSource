/*
 * STMicroelectronics Circular Buffer Class
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"

CircularBuffer::CircularBuffer(unsigned int num_elements)
{
	data_sensor = (SensorBaseData *)malloc(num_elements * sizeof(SensorBaseData));

	pthread_mutex_init(&data_mutex, NULL);

	length = num_elements;
	elements_available = 0;
	first_free_element = &data_sensor[0];
	first_available_element = &data_sensor[0];
}

CircularBuffer::~CircularBuffer()
{
	delete data_sensor;
}

int CircularBuffer::writeElement(SensorBaseData *data)
{
	pthread_mutex_lock(&data_mutex);

	if (elements_available == length) {
		first_available_element++;
		if (first_available_element == (&data_sensor[0] + length))
			first_available_element = &data_sensor[0];
	}

	pthread_mutex_unlock(&data_mutex);

	memcpy(first_free_element, data, sizeof(SensorBaseData));
	first_free_element++;

	if (first_free_element == (&data_sensor[0] + length))
		first_free_element = &data_sensor[0];

	pthread_mutex_lock(&data_mutex);

	if (elements_available < length)
		elements_available++;
	else {
		pthread_mutex_unlock(&data_mutex);
		return -ENOMEM;
	}

	pthread_mutex_unlock(&data_mutex);

	return 0;
}

int CircularBuffer::readElement(SensorBaseData *data)
{
	unsigned int num_remaining_elements;

	pthread_mutex_lock(&data_mutex);

	if (elements_available == 0) {
		pthread_mutex_unlock(&data_mutex);
		return -EFAULT;
	}

	memcpy(data, first_available_element, sizeof(SensorBaseData));
	first_available_element++;

	if (first_available_element == (&data_sensor[0] + length))
		first_available_element = &data_sensor[0];

	elements_available--;
	num_remaining_elements = elements_available;

	pthread_mutex_unlock(&data_mutex);

	return num_remaining_elements;
}

int CircularBuffer::readSyncElement(SensorBaseData *data, int64_t timestamp_sync)
{
	int i = 0;
	int64_t timediff1, timediff2;
	unsigned int num_remaining_elements;
	SensorBaseData *next2_available_element;
	SensorBaseData *next1_available_element;

	pthread_mutex_lock(&data_mutex);

	if ((elements_available == 0) || (timestamp_sync <= 0)) {
		pthread_mutex_unlock(&data_mutex);
		return -EFAULT;
	}

	if (elements_available == 1) {
		memcpy(data, first_available_element, sizeof(SensorBaseData));
		pthread_mutex_unlock(&data_mutex);
		return 1;
	}

	next1_available_element = first_available_element;

	do {
		i++;

		if (i > 1) {
			next1_available_element++;
			if (next1_available_element == (&data_sensor[0] + length))
				next1_available_element = &data_sensor[0];
		}

		timediff1 = next1_available_element->timestamp - timestamp_sync;
		if (timediff1 < 0)
			timediff1 = -timediff1;

		next2_available_element = next1_available_element + 1;
		if (next2_available_element == (&data_sensor[0] + length))
			next2_available_element = &data_sensor[0];

		timediff2 = next2_available_element->timestamp - timestamp_sync;
		if (timediff2 < 0)
			timediff2 = -timediff2;

	} while ((timediff2 < timediff1) && (i < ((int)elements_available - 1)));

	if (timediff2 < timediff1) {
		memcpy(data, next2_available_element, sizeof(SensorBaseData));
		first_available_element = next2_available_element;

		elements_available -= i;
		num_remaining_elements = elements_available;
	} else {
		memcpy(data, next1_available_element, sizeof(SensorBaseData));
		first_available_element = next1_available_element;

		elements_available -= (i - 1);
		num_remaining_elements = elements_available;
	}

	pthread_mutex_unlock(&data_mutex);

	return num_remaining_elements;
}

void CircularBuffer::resetBuffer()
{
	pthread_mutex_lock(&data_mutex);

	elements_available = 0;
	first_free_element = &data_sensor[0];
	first_available_element = &data_sensor[0];

	pthread_mutex_unlock(&data_mutex);
}
