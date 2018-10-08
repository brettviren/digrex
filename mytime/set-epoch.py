#!/usr/bin/env python3
'''
Send a Set Epoch Request to the network.
'''
import sys
import time
import zmq
import pyre

import mytime_pb2 as mydata

now = time.time()
ecr = mydata.EpochChange(number=int(sys.argv[1]),
                         period=int(sys.argv[2]),
                         start=int(1000*(now + float(sys.argv[3]))))
payload = ecr.SerializeToString()
node = pyre.Pyre("digrex-control")


node.start()
time.sleep(1)
node.join("CHAT")
node.join("EPOCH")


english = "epoch change request: %d %d %d" % (ecr.number, ecr.period, ecr.start)
print (english)
node.shouts("CHAT", english)

print ("shouting to epoch")
node.actor.send_unicode("SHOUT", flags=zmq.SNDMORE)
node.actor.send_unicode("EPOCH", flags=zmq.SNDMORE)
node.actor.send(payload, flags=0)
node.stop()



