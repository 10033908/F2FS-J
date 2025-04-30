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

# Single threaded reads (1MB I/Os) on a 1G file.

set $dir=/f2fs_mount_point
set $filesize=4k
set $iosize=512k
set $nthreads=1
set $iters=1
#set $nfiles=3000000
#set $count=12000000
#set $nfiles=2000000
#set $count=8000000
set $nfiles=1000000
set $count=4000000
#set $nfiles=3400000
#set $nfiles=2097152
#set $bytes=8g
#set $nfiles=3145728
#set $bytes=12g
set $meandirwidth=100


define fileset name=bigfileset,path=$dir,size=$filesize,entries=$nfiles,dirwidth=$meandirwidth,prealloc,reuse=100

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$nthreads
  {
    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$iosize
    flowop closefile name=closefile1,fd=1
    flowop finishoncount name=finish,value=$count
    
  }
}

run 100000000

echo  "FileMicro-SeqRead Version 2.1 personality successfully loaded"
