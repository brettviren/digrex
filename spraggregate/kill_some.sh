#!/bin/bash
kill $(ps -ef|grep 'python3 ./spraggregate.py'|grep -v grep | awk '{print $2}')
