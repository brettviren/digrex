#!/usr/bin/env python3
'''
Make a memmap numpy file
'''
import sys
import numpy

shape = (int(sys.argv[1]), int(sys.argv[2]))
size = shape[0] * shape[1]
sample_type = numpy.uint16
resolution = 1<<12

mmdat = numpy.memmap("test_mmap.dat", dtype=sample_type,
                     mode='w+', offset=0, shape=shape)


mmdat[:] = numpy.random.randint(0, resolution, shape, dtype=sample_type)
mmdat.flush()
