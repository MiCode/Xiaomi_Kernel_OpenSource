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

from data.EintData import EintData

class Md1EintData(EintData):
    def __init__(self):
        EintData.__init__(self)
        self.__dedicatedEn = False
        self.__srcPin = ''
        self.__socetType = ''

    def set_dedicatedEn(self, value):
        if value == 'Disable':
            self.__dedicatedEn = False
        else:
            self.__dedicatedEn = True

    def get_dedicatedEn(self):
        return self.__dedicatedEn

    def set_srcPin(self, pin):
        self.__srcPin = pin

    def get_srcPin(self):
        return self.__srcPin

    def set_socketType(self, type):
        self.__socetType = type

    def get_socketType(self):
        return self.__socetType

    def set_sensitiveLevel(self, level):
        EintData.set_sensitiveLevel(self, level)

    def get_sensitiveLevel(self):
        return EintData.get_sensitiveLevel(self)

    def set_debounceEnable(self, enable):
        EintData.set_debounceEnable(self, enable)

    def get_debounceEnable(self):
        return EintData.get_debounceEnable(self)

    def set_polarity(self, polarity):
        EintData.set_polarity(self, polarity)

    def get_polarity(self):
        return EintData.get_polarity(self)
