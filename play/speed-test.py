#!/usr/bin/env python3

import time
import dune_pb2

pa = dune_pb2.PrimitiveActivity(fragid=55, count=2, reference_tick=10001)


count = 10000000

t1 = time.time()
for n in range(count):
    one = pa.SerializeToString()
t2 = time.time()
for n in range(count):
    pa.ParseFromString(one)
t3 = time.time()
print ("%.f Hz serialize, %.f Hz parse" % \
       (count/(t2-t1), count/(t3-t2)))
        

## just serializing/parsing
# 846460 Hz serialize, 1241685 Hz parse

                              
                
