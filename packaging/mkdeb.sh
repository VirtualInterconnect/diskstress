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

export DEBEMAIL="opensource@virtualinterconnect.com"
export DEBFULLNAME="Michael Mol"

which debmake > /dev/null
debmake_found=$?

if test $debmake_found -ne 0  ; then
    echo "Please install debmake"
    exit 1
fi

# Nothing more annoying than blowing away the devel .git by accident.
set -e

rm -rf ../diskstress-*
debmake -t -T -u 0.1 -o packaging/debmake_params
cd ../diskstress-0.1/

# Because debuild won't source the COPYING file properly anyway, and we want to
# support more than just Debian. So we write the Debian-compatible copyright
# notice, and copy it into place before letting debuild think about it.
cp -a COPYING debian/copyright

packaging/git-to-changelog > debian/changelog
packaging/git-to-changelog > CHANGELOG
# Needed because debmake ignores 'section' in options file.
sed -i -e 's/^Section: unknown/Section: utils/' debian/control

# Needed because debmake ignores 'homepage' in options file.
sed -i -e 's/^Homepage: .*/Homepage: https:\/\/github.com\/VirtualInterconnect\/diskstress/' debian/control

# No need to package VCS-related cruft
rm -rf .git/
rm -rf .gitignore

debuild -us -uc
