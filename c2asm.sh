#!/bin/bash
#
# c2asm.sh
#
#  convert C source to ASM using wcc and wdis
#  step 1:  wcc test.c -0 -ml -d1 -i=/home/eyal/bin/watcom/h
#  step 2:  wdis test.o -l -e -p -s  ...or...
#           wdis test.o -l -a
#
#   c2asm.sh <.C file>
#

#
# setup some directories and file names
#
WCCOPT="-0 -ml -d1 -s -zu -oi -i=/home/eyal/bin/watcom/h -i=./include"
WDISOPT="-l -e -p -s"

FULLFILENAME=$1
FILENAME=${FULLFILENAME##*/}		# extract file name by removing directory and extension
FNAME=${FILENAME%.*}				# extract file name only
TEMPOBJ=$FNAME".o"					# setup temporary object file

#
# do the conversion
#
wcc $1 $WCCOPT -fo=$TEMPOBJ
wdis $TEMPOBJ $WDISOPT
rm $TEMPOBJ

