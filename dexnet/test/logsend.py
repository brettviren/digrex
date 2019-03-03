#!/usr/bin/env python3

import sys
import zmq
import time
import logging
from zmq.log.handlers import PUBHandler

context = zmq.Context()
pub = context.socket(zmq.PUB)
pub.bind('tcp://127.0.0.1:12345')
handler = PUBHandler(pub)
logger = logging.getLogger()
logger.setLevel(logging.INFO)
logger.addHandler(handler)

handler.root_topic = "logtest"

for n in range(1000):
    logger.info('hello world #%d' % n)
    print (n)
    time.sleep(1)
