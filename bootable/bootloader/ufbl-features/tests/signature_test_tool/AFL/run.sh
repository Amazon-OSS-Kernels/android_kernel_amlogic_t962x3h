#!/bin/sh
# Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.

#Execute this script from the AFL directory
# by calling ./run.sh <Device Type> <Run time>
#This script will execute 5 instances of AFL at a time which will
# eventually fuzz every signature header.
#Each instance of AFL is run in the background and
# then killed after $RUNTIME seconds
#"AFL_SKIP_CPUFREQ=1" causes AFL to skip checking your CPU frequency settings
#"AFL_NO_AFFINITY=1" prevents an instance of AFL to binding to a core,
# for running only one or 2 instances of AFL, it may be best to remove that.
#"../../../../../../fireos/external/AFL/afl-fuzz" is the absolute path
# to the afl-fuzz executable, which is located in the fireos/external directory
#The address after -i indicates the location of the test cases
#The address after -o indicates the location of the directory
# where all the information AFL gathers will be dumped
#The "@@" causes AFL to pass the name of the input file
#  as a parameter to main (argv[5] in this case)

DEVICE=$1
#How long each instance of AFL will run in seconds
RUNTIME=$2
if [ $DEVICE == "help" ]
	then
		echo "To run this script:"
		echo "./run.sh <Device_Type> <Run Time>"
		echo
		echo "This script will run 5 instances of AFL, fuzzing the"
		echo "specified device's signature headers each for however"
		echo "long the run time is set to be"
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
if [ -z "$2" ]
	then
		echo "Please provide a device type and run time after \"./run.sh\""
		echo "ex. ./run.sh ABC 10"
		exit
	fi
if [ ! $RUNTIME -gt 0 ]
	then
		echo "Please provide a run time that is greater than 0"
		exit
	fi
if [ ! -d ../include/$DEVICE ]
	then
		echo "There is no ../include/$DEVICE directory"
		exit
	fi
if [ ! -f ../include/$DEVICE/boot.img ]
	then
		echo "There is no boot.img in the ../include/$DEVICE directory"
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
if [ ! -d output/$DEVICE ]
	then
		mkdir -p output/$DEVICE
	fi
#If AFL isn't already made, go ahead and make it
if [ ! -f ../../../../../../fireos/external/AFL/afl-fuzz ]
	then
		cd ../../../../../../fireos/external/AFL
		make clean
		make
		cd ../../../bootable/bootloader/ufbl-features/tests/signature_test_tool/AFL
	fi
# Get the page size of the image
PAGE_SIZE=`od -j 36 -t x4 -N 4 -An ../include/$DEVICE/boot.img`
PAGE_SIZE=`echo "ibase=16; $PAGE_SIZE" | bc`
counter=0
#This line takes the signature of boot.img and outputs meta data in the format:
#    0:d=0  hl=4 l=1069 cons: SEQUENCE
#By removing all letters, =, and :, we're left with just the numbers
#The first number is the byte offset, and the third is the length of the header
#These are the only two relevant numbers for this program
mkdir -p out/$DEVICE
tail -c $PAGE_SIZE ../include/$DEVICE/boot.img | openssl asn1parse -inform der \
	| tr 'a-z' ' ' | tr '=' ' ' | tr ':' ' ' \
	| tr 'A-Z' ' '> out/$DEVICE/parse.txt
#Go through the newly generated parse.txt and extract the relevant values
#This is repeated for every line of parse.txt
#'x' and 'y' hold useless numbers in the file
while IFS=' '  read -r offset x hlength y
do
	#Increment the counter
	counter=$((counter+1))
	#A testcase is generated for every header
	#with the folder named the offset
	if [ ! -d testcases/$DEVICE/$offset ]
		then
			mkdir -p testcases/$DEVICE/$offset
		fi
	#The signature is extracted from boot.img and
	#trimmed to the header length at its offset
	tail -c $(($PAGE_SIZE-$offset)) ../include/$DEVICE/boot.img \
		> testcases/$DEVICE/$offset/signature.bin
	truncate -s $hlength testcases/$DEVICE/$offset/signature.bin

	#Start AFL: pass offset, header length, page size,
	# device type, and file name of testcase as parameters
	#Output is redirected to stdout and sent to background
	# so multiple instances of it can run in the same terminal
	AFL_SKIP_CPUFREQ=1 AFL_NO_AFFINITY=1 \
		../../../../../../fireos/external/AFL/afl-fuzz \
		-i testcases/$DEVICE/$offset -o output/$DEVICE/$offset \
		 out/signature_test_tool \
		$offset $hlength $PAGE_SIZE $DEVICE @@  > out/stdout &
	printf 'Running AFL on bytes %s-%s\n' \
		"$offset" "$((offset+hlength-1))"
	#Assign process ID by which iteration in the loop it is
	case $counter in
		1)
			PID1=$!
		;;
		2)
			PID2=$!
		;;
		3)
			PID3=$!
		;;
		4)
			PID4=$!
		;;
		5)
			PID5=$!
		;;
		*)
		;;
	esac
	#Once 5 applications are running wait for $RUNTIME seconds,
	# then kill the processes and continue
	if [ $counter -gt 4 ]
		then
			sleep $RUNTIME
			counter=0
			kill $PID1
			kill $PID2
			kill $PID3
			kill $PID4
			kill $PID5
		fi
done <"out/$DEVICE/parse.txt"
#Number of processes likely won't be divisible by 5, clean up any remaining
sleep $RUNTIME
while [ $counter -gt 0 ]
do
	case $counter in
		1)
			kill $PID1
		;;
		2)
			kill $PID2
		;;
		3)
			kill $PID3
		;;
		4)
			kill $PID4
		;;
		*)
		;;
	esac
	counter=$((counter-1))
done

cd output/$DEVICE
echo "These are the header offsets that have crashes:" >crash_report.txt
grep -r "unique_crashes " | sed 's/[^0-9 || ^ ]*//g' > ../../out/$DEVICE/crashes.txt
sed -i '/ 0/d' ../../out/$DEVICE/crashes.txt
awk '{print $DEVICE}' ../../out/$DEVICE/crashes.txt | sort -n >> crash_report.txt
cd ../..
