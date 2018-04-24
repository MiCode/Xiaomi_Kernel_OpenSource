#
# Copyright (C) 2017 STMicroelectronics
# Denis Ciocca - Motion MEMS Product Div.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#/

LOCAL_PATH := ${CURDIR}

CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
RM = rm -f
INCLUDES := -I$(LOCAL_PATH)/ -I$(LOCAL_PATH)/src -I$(LOCAL_PATH)/linux/
CFLAGS += $(INCLUDES) -DLOG_TAG=\"SensorHAL\" -DPLTF_LINUX_ENABLED -Wall \
		-Wunused-parameter -Wunused-value -Wunused-function -Wunused-label -fPIC

CPPFLAGS = $(INCLUDES) -g -DPLTF_LINUX_ENABLED -Wunused-label -Wall -fPIC \
		-D_DEFAULT_SOURCE
LDFLAGS = -g
LDLIBS = -lpthread

ifeq ($(DEBUG),y)
CFLAGS += -g -O0
endif # DEBUG

# Where to produce shared object
OUT = $(LOCAL_PATH)

LOCAL_SRC_FILES := \
		src/SensorHAL.cpp \
		src/CircularBuffer.cpp \
		src/FlushBufferStack.cpp \
		src/FlushRequested.cpp \
		src/ChangeODRTimestampStack.cpp \
		src/SensorBase.cpp \
		src/HWSensorBase.cpp \
		src/Accelerometer.cpp \
		src/Gyroscope.cpp

ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
LOCAL_SRC_FILES += src/SelfTest.cpp
endif # CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS

OBJS=$(subst .cpp,.o,$(LOCAL_SRC_FILES)) src/iio_utils.o

all: SensorHAL

SensorHAL: $(OBJS)
	$(CXX) $(LDFLAGS) -shared $(OBJS) $(LDLIBS) -o $(OUT)/SensorHAL.so

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) SensorHAL.so



