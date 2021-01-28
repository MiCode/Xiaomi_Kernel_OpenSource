#! /usr/bin/python
# -*- coding: utf-8 -*-

# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

import sys,os
import re
import string
import ConfigParser
import xml.dom.minidom

import ChipObj
from data.PowerData import PowerData
from utility.util import log
from utility.util import LogLevel
from utility.util import sorted_key
from ModuleObj import ModuleObj

class PowerObj(ModuleObj):
    def __init__(self):
        ModuleObj.__init__(self, 'cust_power.h', 'cust_power.dtsi')
        self.__list = {}

    def getCfgInfo(self):
        cp = ConfigParser.ConfigParser(allow_no_value=True)
        cp.read(ModuleObj.get_figPath())

        self.__list = cp.options('POWER')
        self.__list = self.__list[1:]
        self.__list = sorted(self.__list)

    def read(self, node):
        nodes = node.childNodes
        for node in nodes:
            if node.nodeType == xml.dom.Node.ELEMENT_NODE:
                if node.nodeName == 'count':
                    continue

                varNode = node.getElementsByTagName('varName')

                data = PowerData()
                data.set_varName(varNode[0].childNodes[0].nodeValue)

                ModuleObj.set_data(self, node.nodeName, data)

        return True

    def parse(self, node):
        self.getCfgInfo()
        self.read(node)

    def gen_files(self):
        ModuleObj.gen_files(self)

    def gen_spec(self, para):
        if re.match(r'.*_h$', para):
            self.gen_hFile()

    # power module has no DTSI file
    def gen_dtsiFile(self):
        pass

    def fill_hFile(self):
        gen_str = ''
        for key in sorted_key(ModuleObj.get_data(self).keys()):
            value = ModuleObj.get_data(self)[key]
            if value.get_varName() == '':
                continue
            idx = string.atoi(key[5:])
            name = self.__list[idx]
            gen_str += '''#define GPIO_%s\t\tGPIO_%s\n''' %(name.upper(), value.get_varName())

        return gen_str

    def fill_dtsiFile(self):
        return ''
