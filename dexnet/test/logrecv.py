#!/usr/bin/env python3

import sys
import zmq
import time

ctx = zmq.Context()
sub = ctx.socket(zmq.SUB)
sub.setsockopt(zmq.SUBSCRIBE, b'logtest')
sub.connect('tcp://127.0.0.1:12345')


while True:
    print ("recv'ing")
    level, logmsg = sub.recv_multipart()
    print(level, logmsg.decode('utf-8'))
    
