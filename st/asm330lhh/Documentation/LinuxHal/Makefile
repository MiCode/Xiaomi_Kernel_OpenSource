#
# Copyright (C) 2018 STMicroelectronics
# Motion MEMS Product Div.
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

IDIR = -I. -I../../linux/

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
CFLAGS=$(IDIR) -DLOG_TAG=\"test_linux\"
LIB=-ldl -lpthread

.PHONY: test_linux

all: test_linux

OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

test_linux: test_linux.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIB)

clean:
	rm -f *.o test_linux
