/*
 * STMicroelectronics Flush Buffer Stack Class
 *
 * Copyright 2015-2016 STMicroelectronics Inc.
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#include "FlushBufferStack.h"

FlushBufferStack::FlushBufferStack()
{
	pthread_mutex_init(&data_mutex, NULL);

	elements_available = 0;
}

FlushBufferStack::~FlushBufferStack()
{

}

int FlushBufferStack::writeElement(int handle, int64_t timestamp)
{
	pthread_mutex_lock(&data_mutex);

	if (elements_available >= ST_FLUSH_BUFFER_STACK_MAX_ELEMENTS) {
		pthread_mutex_unlock(&data_mutex);
		return -ENOMEM;
	}

	flush_timestamps[elements_available] = timestamp;
	flush_handles[elements_available] = handle;
	elements_available++;

	pthread_mutex_unlock(&data_mutex);

	return 0;
}

int FlushBufferStack::readLastElement(int64_t *timestamp)
{
	int flush_handle;

	pthread_mutex_lock(&data_mutex);

	if (elements_available == 0) {
		pthread_mutex_unlock(&data_mutex);
		return -EIO;
	}

	*timestamp = flush_timestamps[0];
	flush_handle = flush_handles[0];

	pthread_mutex_unlock(&data_mutex);

	return flush_handle;
}

unsigned int FlushBufferStack::ElemetsOnStack()
{
	unsigned int data_available;

	pthread_mutex_lock(&data_mutex);
	data_available = elements_available;
	pthread_mutex_unlock(&data_mutex);

	return data_available;
}

void FlushBufferStack::removeLastElement()
{
	unsigned int i;

	pthread_mutex_lock(&data_mutex);

	if (elements_available == 0) {
		pthread_mutex_unlock(&data_mutex);
		return;
	}

	for (i = 0; i < elements_available - 1; i++) {
		flush_timestamps[i] = flush_timestamps[i + 1];
		flush_handles[i] = flush_handles[i + 1];
	}

	elements_available--;

	pthread_mutex_unlock(&data_mutex);
}

void FlushBufferStack::resetBuffer()
{
	pthread_mutex_lock(&data_mutex);

	elements_available = 0;

	pthread_mutex_unlock(&data_mutex);
}
