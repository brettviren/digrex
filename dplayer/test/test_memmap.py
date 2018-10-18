#!/usr/bin/env python3
'''
Make a memmap numpy file
'''
import numpy


arr = numpy.asarray([[1,2,3],[4,5,6]], dtype=numpy.int16)
mmoutput = numpy.memmap("test_mmap.dat", dtype=numpy.int16,
                        mode='w+', offset=0, shape=arr.shape)

mmoutput[:] = arr[:]
mmoutput.flush()
