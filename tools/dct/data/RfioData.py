#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

class RfioData:
    def __init__(self):
        self.__padName = ''
        self.__slaveMode = ''
        self.__driving = ''
        self.__pupd = 'NONE'
        self.__dir = ''
        self.__outDef = ''
        self.__ies = False
        self.__smt = False
        self.__analogPad = False

    def set_padName(self, name):
        self.__padName = name

    def get_padName(self):
        return self.__padName

    def set_slaveMode(self, name):
        self.__slaveMode = name

    def get_slaveMode(self):
        return self.__slaveMode

    def set_driving(self, val):
        self.__driving = val

    def get_driving(self):
        return self.__driving

    def get_defEnable(self):
        return self.__driving

    def set_pupd(self, val):
        self.__pupd = val;

    def get_pupd(self):
        return self.__pupd

    def set_dir(self, val):
        self.__dir = val

    def get_dir(self):
        return self.__dir

    def set_outDef(self, val):
        self.__outDef = val

    def get_outDef(self):
        return self.__outDef

    def set_ies(self, flag):
        self.__ies = flag

    def get_ies(self):
        return self.__ies

    def set_smt(self, flag):
        self.__smt = flag

    def get_smt(self):
        return self.__smt;

    def set_analogPad(self, val):
        self.__analogPad = val

    def get_analogPad(self):
        return self.__analogPad
