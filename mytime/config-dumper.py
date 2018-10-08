#!/usr/bin/env python3
'''
Listen to Zyre including EPOCH CONFIG 
'''
import zmq
import uuid
import json
import pyre


node = pyre.Pyre("digrex-snoop")

node.join("CHAT")
node.join("EPOCH")
node.start()

poller = zmq.Poller()
poller.register(node.socket(), zmq.POLLIN)
while (True):
    items = dict(poller.poll())
    print ()
    cmds = node.recv()
    print(cmds)
    msg_type = cmds.pop(0)
    print("type:  %s" % msg_type)
    print("peer:  %s" % uuid.UUID(bytes=cmds.pop(0)))
    print("name:  %s" % cmds.pop(0))
    if msg_type.decode('utf-8') == "SHOUT":
        print("group: %s" % cmds.pop(0))
    elif msg_type.decode('utf-8') == "ENTER":
        headers = json.loads(cmds.pop(0).decode('utf-8'))
        print("headers: %s" % headers)
        for key in headers:
            print("key = {0}, value = {1}".format(key, headers[key]))
    print("cont: %s" % cmds)

node.stop()
