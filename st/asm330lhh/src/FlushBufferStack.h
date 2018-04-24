/*
 * Copyright (C) 2015-2016 STMicroelectronics
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ST_FLUSH_BUFFER_STACK_H
#define ST_FLUSH_BUFFER_STACK_H

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#define ST_FLUSH_BUFFER_STACK_MAX_ELEMENTS		(300)

/*
 * class FlushBufferStack
 */
class FlushBufferStack {
private:
	pthread_mutex_t data_mutex;
	unsigned int elements_available;

	int64_t flush_timestamps[ST_FLUSH_BUFFER_STACK_MAX_ELEMENTS];
	int flush_handles[ST_FLUSH_BUFFER_STACK_MAX_ELEMENTS];

public:
	FlushBufferStack();
	~FlushBufferStack();

	int writeElement(int handle, int64_t timestamp);
	int readLastElement(int64_t *timestamp);
	unsigned int ElemetsOnStack();

	void removeLastElement(void);
	void resetBuffer();
};

#endif /* ST_FLUSH_BUFFER_STACK_H */
