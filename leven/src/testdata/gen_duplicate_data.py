import sys
import os
from random import randint

def gen_duplicate_data(input_filename, output_filename):
	lines = None
	with open(input_filename, 'r') as f:
		lines = f.readlines()
	
	output_file = open(output_filename, 'w')
	for ln in lines:
		ln = ln.strip().rstrip()

		ln = '%s\n' % ln
		if not ln.startswith('0x'):
			output_file.write(ln)
			continue
		
		ln = '\t%s' % ln
		for n in xrange(0, randint(1, 5)):
			output_file.write(ln)
	
	output_file.close()


if __name__=='__main__':
	sys.exit(gen_duplicate_data(sys.argv[1], sys.argv[2]))


