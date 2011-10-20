#! /bin/sh
# Copyright (C) 2002 Red Hat, Inc.
# Written by Jakub Jelinek <jakub@redhat.com>, 2002.
#
# This program is Open Source software; you can redistribute it and/or
# modify it under the terms of the Open Software License version 1.0 as
# published by the Open Source Initiative.
#
# You should have received a copy of the Open Software License along
# with this program; if not, you may obtain a copy of the Open Software
# License version 1.0 from http://www.opensource.org/licenses/osl.php or
# by writing the Open Source Initiative c/o Lawrence Rosen, Esq.,
# 3001 King Ranch Road, Ukiah, CA 95482.
set -e

# Don't fail if we cannot decompress the file.
bunzip2 -c $srcdir/testfile2.bz2 > testfile2 2>/dev/null || exit 0

./ecp testfile2 testfile2.tmp

rm -f testfile2 testfile2.tmp

exit 0
