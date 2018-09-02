#!/usr/bin/env python3

# https://github.com/paulfurley/pub-sub-python-zeromq-protobuf/blob/master/zpubsub/subscriber/subscriber.py

import zmq
import time
import dune_pb2

PA_TOPIC = b'P'

ADDR = "127.0.0.1"
PORT = 9876

context = zmq.Context()
socket = context.socket(zmq.SUB)
#socket.connect("tcp://{addr}:{port}".format(addr=ADDR, port=PORT))
socket.bind("tcp://{addr}:{port}".format(addr=ADDR, port=PORT))
socket.setsockopt(zmq.SUBSCRIBE, PA_TOPIC)


t0 = time.time()
count = 0
last_count = None
last_tick = None
last_time = None
while True:
    dat = socket.recv()
    pa = dune_pb2.PrimitiveActivity()
    pa.ParseFromString(dat[1:])
    count += 1
    if count % 10000 == 1:
        t = time.time()
        d_count = 0
        d_tick = 0
        dt = 0
        if last_count:
            d_count = pa.count - last_count
            d_tick = pa.reference_tick - last_tick
            dt = t - last_time
        last_count = pa.count
        last_tick = pa.reference_tick
        last_time = t
        eps = int(dt / (0.5e-6)) - d_tick
        print ("%d %f d_count=%d count=%d d_tick=%d tick=%d dt=%f eps=%d"%\
               (count, count/(t-t0),
                d_count, pa.count,
                d_tick, pa.reference_tick,
                dt, eps))
    
