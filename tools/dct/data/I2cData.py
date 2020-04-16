#! /usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

class BusData:
    def __init__(self):
        self.__speed = ''
        self.__enable = False

    def set_speed(self, speed):
        self.__speed = speed

    def set_enable(self, flag):
        self.__enable = flag

    def get_speed(self):
        return self.__speed

    def get_enable(self):
        return self.__enable

class I2cData:
    _i2c_count = 0
    _channel_count = 0
    _busList = []

    def __init__(self):
        self.__varname = ''
        self.__channel = ''
        self.__address = ''

    def set_varName(self, name):
        self.__varname = name

    def set_channel(self, channel):
        self.__channel = channel

    def set_address(self, addr):
        self.__address = addr

    def get_varName(self):
        return self.__varname

    def get_channel(self):
        return self.__channel

    def get_address(self):
        return self.__address



