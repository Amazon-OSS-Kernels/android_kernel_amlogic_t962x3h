#!/bin/sh
# Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.

if [ -z "$1" ]
	then
		echo "Please provide a device type after \"./run.sh\""
		echo "ex. ./run.sh ABC"
		exit
	fi
if [ $1 = "help" ]
	then
		echo "To run this script:"
		echo "./run.sh <Device_Type>"
		echo
		echo "To get a valid boot.img:"
		echo "If the device type you want to test is not in the"
		echo "/include directory, download the latest userdebug build"
		echo "from KBITS, extract it, copy the 'boot.img' located in"
		echo "it, and make a new directory in"
		echo "signature_test_tools_libtomcrypt/include/<Device_type>"
		echo "named after the device and insert the boot.img"
		echo "file in to it."
		exit
	fi

if [ ! -d ../include/$1 ]
	then
		echo "There is no ../include/$1 directory"
		exit
	fi
if [ ! -f ../include/$1/boot.img ]
	then
		echo "There is no boot.img in the ../include/$1 directory"
		exit
	fi
if [ ! -d output/$1 ]
	then
		mkdir -p output/$1
	fi

# Get the page size of the image
PAGE_SIZE=`od -j 36 -t x4 -N 4 -An ../include/$1/boot.img`
PAGE_SIZE=`echo "ibase=16; $PAGE_SIZE" | bc`

out/test $1 $PAGE_SIZE
