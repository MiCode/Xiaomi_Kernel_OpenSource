#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

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



