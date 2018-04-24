/*
 * STMicroelectronics Flush Requested Class
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#include "FlushRequested.h"

FlushRequested::FlushRequested()
{
	pthread_mutex_init(&data_mutex, NULL);

	elements_available = 0;
}

FlushRequested::~FlushRequested()
{

}

int FlushRequested::writeElement(int handle)
{
	pthread_mutex_lock(&data_mutex);

	if (elements_available >= ST_FLUSH_REQUESTED_STACK_MAX_ELEMENTS) {
		pthread_mutex_unlock(&data_mutex);
		return -ENOMEM;
	}

	handles[elements_available] = handle;
	elements_available++;

	pthread_mutex_unlock(&data_mutex);

	return 0;
}

int FlushRequested::readElement()
{
	int handle;
	unsigned int i;

	pthread_mutex_lock(&data_mutex);

	if (elements_available == 0) {
		pthread_mutex_unlock(&data_mutex);
		return -EIO;
	}

	handle = handles[0];

	for (i = 0; i < elements_available - 1; i++)
		handles[i] = handles[i + 1];

	elements_available--;

	pthread_mutex_unlock(&data_mutex);

	return handle;
}

void FlushRequested::resetBuffer()
{
	pthread_mutex_lock(&data_mutex);

	elements_available = 0;

	pthread_mutex_unlock(&data_mutex);
}
