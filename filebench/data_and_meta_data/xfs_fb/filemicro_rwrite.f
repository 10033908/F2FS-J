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

# Single threaded asynchronous ($sync) random writes (2KB I/Os) on a 1GB file.
# Stops when 128MB ($bytes) has been written.

set $dir=/xfs_mount_point
set $bytes=64g
set $cached=false
set $filesize=64g
set $iosize=1m
set $iters=1
set $nthreads=1
set $sync=false

define file name=bigfile1,path=$dir,size=$filesize,prealloc,reuse,cached=$cached

define process name=filewriter,instances=1
{
  thread name=filewriterthread,memsize=10m,instances=$nthreads
  {
    flowop write name=write-file,filename=bigfile1,random,dsync=$sync,iosize=$iosize,iters=$iters
    flowop finishonbytes name=finish,value=$bytes
  }
}

echo  "FileMicro-WriteRand Version 2.1 personality successfully loaded"
