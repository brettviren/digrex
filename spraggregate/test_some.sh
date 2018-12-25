#!/bin/bash

set -x

rate=10000.0                       # hz
jitter=0.0
tardy=2.0
dwell=0.001


main=./spraggregate.py
host="127.0.0.1"

llep="tcp://${host}:5678"
$main sink "sub:${llep}" > log.sink 2>&1 &
l0fanin=""
for port in {5671..5674} ; do
    l0fanin="${l0fanin} sub:tcp://${host}:${port}"
done
$main fanin -T $tardy -D $dwell -o "pub:${llep}" $l0fanin > log.fanin 2>&1 &

for port in {5671..5674} ; do
    $main source -j $jitter -r $rate "pub:tcp://${host}:${port}" > log.source.${port} 2>&1 &
done
