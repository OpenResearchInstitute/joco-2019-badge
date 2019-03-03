#!/usr/bin/env python

import random

max_index = 104
pos = list(range(1,max_index+1))
random.shuffle(pos)
for i in range(1, max_index+1):
    print 'mv %04d.DAT renum/%04d.DAT' %  (i, pos[i-1])
    print 'mv %04d.RAW renum/%04d.RAW' %  (i, pos[i-1])

