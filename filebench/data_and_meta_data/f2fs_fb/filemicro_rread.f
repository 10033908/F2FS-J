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

# Single threaded random reads (2KB I/Os) on a 1GB file.
# Stops after 128MB ($bytes) has been read.

set $dir=/f2fs_mount_point
set $bytes=64g
set $cached=false
set $filesize=4g
set $iosize=512k
set $iters=1
set $nthreads=16
set $nfiles=16

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,prealloc,reuse

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop read name=random-read,fd=1,random,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop finishonbytes name=finish,value=$bytes
  }
}

run 100000000

echo  "FileMicro-ReadRand Version 2.2 personality successfully loaded"
