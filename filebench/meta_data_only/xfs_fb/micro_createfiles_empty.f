#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

# Creates a fileset with 20,000 entries ($nfiles), but only preallocates
# 50% of the files.  Each file's size is set via a gamma distribution with
# a median size of 1KB ($filesize).
#
# The single thread then creates a new file and writes the whole file with
# 1MB I/Os.  The thread stops after 5000 files ($count/num of flowops) have
# been created and written to.

set $dir=/xfs_mount_point
set $count=16000000
set $filesize=0k
set $iosize=1m
set $meandirwidth=100
set $nfiles=1000000
set $nthreads=1

set mode quit alldone

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc=0,paralloc

define process name=filecreate,instances=1
{
  thread name=filecreatethread,memsize=10m,instances=$nthreads
  {
    flowop createfile name=createfile1,filesetname=bigfileset,fd=1
    flowop closefile name=closefile1,fd=1
    flowop finishoncount name=finish,value=$count
  }
}

run 100000000

echo  "FileMicro-Createfiles Version 2.2 personality successfully loaded"
