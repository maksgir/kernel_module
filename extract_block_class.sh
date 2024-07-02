#!/bin/bash

# extract_block_class.sh

# Find the address of block_class in /proc/kallsyms
address=$(grep " block_class" /proc/kallsyms | awk '{print "0x"$1}')

# Write the address to the header file
echo "#define BLOCK_CLASS ((struct class *)$address)" > block_class_header.h

echo "Address of block_class: $address"
echo "Header file updated: block_class_header.h"

