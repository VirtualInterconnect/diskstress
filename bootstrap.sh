#!/bin/sh
#
# Copyright 2018 Virtual Interconnect, LLC
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

function check_for() {
    which $1 > /dev/null
    prog_found=$?
    
    if test $prog_found -ne 0  ; then
        echo "Please install $1"
        exit 1
    fi
    
}

check_for autoreconf
check_for aclocal
check_for g++

# This one's not quite so easy.
acdir=$(aclocal --print-ac-dir)
if [[ ! -e "${acdir}/ax_cxx_compile_stdcxx_11.m4" ]] ; then
    echo "ax_cxx_compile_stdcxx_11 m4 macro missing. Please install the autoconf-archive package."
    exit 1
fi

autoreconf --install
