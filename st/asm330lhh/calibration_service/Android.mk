#
# Copyright (C) 2013-2016 STMicroelectronics
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

ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
include $(LOCAL_PATH)/../hal_config

ifdef CONFIG_ST_HAL_FACTORY_CALIBRATION

LOCAL_MODULE_OWNER := STMicroelectronics

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS += -DLOG_TAG=\"SensorHAL-daemon\" -Wall \
		-Wunused-parameter -Wunused-value -Wunused-function

ifeq ($(DEBUG),y)
LOCAL_CFLAGS += -g -O0
endif # DEBUG

LOCAL_SRC_FILES := \
		calibration_daemon.c

LOCAL_CPPFLAGS := \
		-std=gnu++11 \
		-W -Wall -Wextra

LOCAL_SHARED_LIBRARIES := libc liblog

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := STSensors_daemon

include $(BUILD_EXECUTABLE)

endif # CONFIG_ST_HAL_FACTORY_CALIBRATION

endif # !TARGET_SIMULATOR
