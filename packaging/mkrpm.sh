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

which rpmbuild > /dev/null
rpmbuild_found=$?

if test $rpmbuild_found -ne 0  ; then
    echo "Please install rpmbuild"
    exit 1
fi

set -e

# Peel the name and version number out of configure.ac
name=$(grep AC_INIT configure.ac | sed -e 's/^AC_INIT(\[\(.*\)\], *\[\(.*\)\], *\[\(.*\)\])$/\1/')
version=$(grep AC_INIT configure.ac | sed -e 's/^AC_INIT(\[\(.*\)\], *\[\(.*\)\], *\[\(.*\)\])$/\2/')
dist_name="${name}-${version}"
dist_file="${dist_name}.tar.gz"
working_dir_parent=".."
working_dir="${dist_name}/"

mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
rm -rf -- "${working_dir_parent}/${working_dir}"
cp -a -- . "${working_dir_parent}/${working_dir}"
cd -- "${working_dir_parent}"

# Patch .gitignore, so we can have use it to exclude detritus from the tarball.
echo ".gitignore" >> "${working_dir}/.gitignore"
echo ".git" >> "${working_dir}/.gitignore"
tar -X "${working_dir}/.gitignore" -vczf "${HOME}/rpmbuild/SOURCES/${dist_file}" "${working_dir}"

cp "${working_dir}/packaging/${name}.spec" ~/rpmbuild/SPECS/
cd ~/rpmbuild/

rpmbuild -ba SPECS/diskstress.spec
