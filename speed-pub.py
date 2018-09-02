#!/usr/bin/env python3

# https://github.com/paulfurley/pub-sub-python-zeromq-protobuf/blob/master/zpubsub/publisher/publisher.py
import sys
import zmq
import time
import dune_pb2

try:
    fragid = int(sys.argv[1])
except IndexError:
    fragid = 1

# The zmq prefix for PrimitiveActivity messages
PA_TOPIC = b'P'

ADDR = "127.0.0.1"
PORT = 9876

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.connect("tcp://{addr}:{port}".format(addr=ADDR, port=PORT))
#socket.bind("tcp://{addr}:{port}".format(addr=ADDR, port=PORT))

t0 = time.time()

count = 0
while True:
    count += 1
    t = time.time()
    ref_tick = int((t-t0)  / 0.5e-6)

    # make a fake activity
    pa = dune_pb2.PrimitiveActivity(fragid=fragid, count=count,
                                    reference_tick = ref_tick)
    for ind in range(10):
        pa.primitives.add(channel=ind+48, start_tick=40,
                          duration=abs(ind-5),
                          activity=abs(ind-5)**2)

    dat = pa.SerializeToString()

    socket.send(PA_TOPIC + dat)




