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

# Single threaded sequential reads (512K I/Os) on a 4M files, each is 16k.

set $dir=/xfs_mount_point
set $bytes=64g
set $filesize=4g
set $iosize=512k
set $iters=8192
set $nthreads=1
set $nfiles=16
set $count=20
set $meandirwidth=100

define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,prealloc,reuse

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop readwholefile name=random-read,fd=1,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop finishonbytes name=finish,value=$bytes
  }
}

run 100000000

echo  "FileMicro-ReadRand Version 2.2 personality successfully loaded"