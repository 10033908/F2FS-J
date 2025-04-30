#!/bin/bash
cat /proc/filesystems | tail -n 2

echo -e "\e[32m->>>>sudo -i to setup randomize_va_space\e[0m"

#randomize_va_space for filebench
sudo sh -c 'echo 0 > /proc/sys/kernel/randomize_va_space'

#https://www.cnblogs.com/wx170119/p/11459995.html
#perf setup
sudo sh -c 'echo "kernel.perf_event_paranoid=-1" >> /etc/sysctl.conf'
sudo sysctl -p

echo 0 > /proc/sys/kernel/kptr_restrict

sudo sysctl -w vm.dirty_background_ratio=5
sudo sysctl -w vm.dirty_ratio=50           
sudo sysctl -w vm.dirty_writeback_centisecs=1500

#perf record
# sudo perf record -F 99 -a -p PID --call-graph dwarf -- sleep 60

#generate graph
#   script file    #    perf.folded          #            svg file      #
# sudo perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > test.svg

#objdump -x