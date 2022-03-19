#!/usr/bin/python3

'''human readable bits'''

import sys

if len(sys.argv) != 2:
	print("hrbs hex\n")
	print("  eg: hrbs 0x01\n")

	exit(-1)

bits = format(int(sys.argv[1], 16), '064b')

for i in range(64, 0, -1):
	print("{0}\t{1}".format(i - 1, bits[64 - i])) 
