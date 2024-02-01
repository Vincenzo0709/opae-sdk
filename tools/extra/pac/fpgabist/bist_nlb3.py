#!/usr/bin/env python
# Copyright(c) 2017, Intel Corporation
#
# Redistribution  and  use  in source  and  binary  forms,  with  or  without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of  source code  must retain the  above copyright notice,
# this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# * Neither the name  of Intel Corporation  nor the names of its contributors
# may be used to  endorse or promote  products derived  from this  software
# without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
# IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
# LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
# CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
# SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
# INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
# CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import os
import subprocess

import bist_common as bc


class Nlb3Mode(bc.BistMode):
    name = "nlb_3"

    def __init__(self):
        modes = ['read', 'write', 'trput']
        params = ('--mode={} '
                  '--multi-cl=1 --begin=1024, --end=1024 --timeout-sec=5 '
                  '--cont')
        self.executables = {mode: params.format(mode) for mode in modes}

    def run(self, gbs_path, bus_num):
        bc.load_gbs(gbs_path, bus_num)
        for test, param in self.executables.items():
            print "Running fpgadiag __MODIFIED__ {} test...\n".format(test)
            cmd = "./fpgadiag -B {} {}".format(bus_num, param)
            try:
                subprocess.check_call(cmd, shell=True)
            except subprocess.CalledProcessError as e:
                print "Failed Test: {}".format(test)
                print e
        print "Finished Executing NLB (FPGA DIAG)Tests\n"
