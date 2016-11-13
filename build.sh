#!/bin/bash
#
# build.sh
#
#  use to invoke TASM.EXE or BCC.EXE or TLIB.EXE or TLINK.EXE DOS 16 bit tools
#  the script is a work around for assembly/error reporting
#  into Eclipse. all errors piped to build.out file and parsed for Eclipse standard error parser
#
#  build.sh [ -tasm <.obj file> <.asm file>  ] |
#           [ -tlib <.lib file> <.obj file> ... ] |
#           [ -bcc  <.obj file> > <.c file> ] |
#           [ -tlink <.exe output> <.obj file1> <.obj file2 ...> <.lib file1> <.lib file2 ...> ]
#


#----------------------------------------
# project directory definitions
#----------------------------------------
DOSPRJBASE="l:\\webserve"
DOSSRCDIR=$DOSPRJBASE"\\src"
DOSBINDIR=$DOSPRJBASE"\\src"
DOSLIBDIR="c:\\bc5\\lib"
DOSLIBPATH=$DOSBINDIR";"$DOSLIBDIR
DOSOBJPATH=$DOSLIBPATH

OUTFILE1=$DOSBINDIR"\\build.out"
OUTFILE2="build.out"

#----------------------------------------
# project options
# change here if options or memory model change
#----------------------------------------
TASMOPT=""
TLIBOPT=""
#BCCOPT=" -P- -1- -a- -c -ml -f- -v- -H -H=L:\WEBSERVE\project.csm -IC:\BC5\INCLUDE;L:\WEBSERVE\SRC\INCLUDE -D__V25__"
BCCOPT="+"$DOSSRCDIR"\\BccDos.cfg"
TLINKOPT="/L"$DOSLIBPATH" /j"$DOSOBJPATH" /c /Tde /x /d"
DOSLARGEOBJ="c0l.obj"
DOSLARGELIB="cl.lib"
LINKRSP="tlink.rsp"

#----------------------------------------
# tool chian definitions
#----------------------------------------
TASM="c:\\tasm\\bin\\tasm.exe"
TLIB="c:\\bc5\\bin\\tlib.exe"
BCC="c:\\bc5\\bin\\bcc.exe"
TLINK="c:\\bc5\\bin\\tlink.exe"

if [[ $# -lt 3 ]]																# make sure we have at least 3 input parameters, build option and two more
    then
        echo "build.sh:44: Error build error, too few parameters"
        exit 1
fi


KEY="$1" ; shift																# KEY holds the build option

case $KEY in

    #
    # handle tasm.exe
    #
    -tasm)
    OBJFILE=$DOSBINDIR"\\"$1 ; shift											# get .obj file name
    SRCFILE=$1 ; shift															# get .asm source file
    ASMFILE=$DOSSRCDIR"\\"$SRCFILE
    LSTFILE=$DOSBINDIR"\\"${SRCFILE%.*}".lst"  									# create list file "*.lst" file name
    CMD=$TASM" "$TASMOPT" "$ASMFILE" "$OBJFILE" "$LSTFILE" > "$OUTFILE1
    CLEAN="del "$OBJFILE
    #echo $CMD
    #echo $CLEAN
    dosbox -c "$CLEAN" -c "$CMD" -c exit > /dev/null
    find . -name *.'[A-Z]*' -exec rename  's/(.*)\/([^\/]*)/$1\/\L$2/' {} \;
    awk -f tasm.awk $OUTFILE2
    ;;
    
    #
    # handle tlib.exe
    #
    -tlib)
    LIBNAME=$DOSBINDIR"\\"$1 ; shift											# get .lib name
    while [[ $# -gt 0 ]]														# loop through one or more .obj files
    do
        OBJNAME=$OBJNAME" -+"$DOSBINDIR"\\"$1 ; shift							# get an .obj file, assuming make transfers only .obj files
    done
    CMD=$TLIB" "$LIBNAME" "$TLIBOPT" "$OBJNAME" > "$OUTFILE1					# library add command
    #echo $CMD
    dosbox -c "$CMD" -c exit > /dev/null
    find . -name *.'[A-Z]*' -exec rename  's/(.*)\/([^\/]*)/$1\/\L$2/' {} \;
    cat $OUTFILE2
    ;;
    
    #
    # handle bcc.exe
    #
    -bcc)
    OBJNAME=$DOSBINDIR"\\"$1 ; shift											# get .obj file name
    SRCNAME=$DOSSRCDIR"\\"$1 ; shift											# get .c file name
    CMD=$BCC" "$BCCOPT" -o"$OBJNAME" "$SRCNAME" > "$OUTFILE1
    #echo $CMD
    dosbox -c "$CMD" -c exit > /dev/null
    find . -name *.'[A-Z]*' -exec rename  's/(.*)\/([^\/]*)/$1\/\L$2/' {} \;
    awk -f bcc.awk $OUTFILE2
    ;;
    
    #
    # handle tlink.exe
    #
    -tlink)
    OBJFILES=$DOSLARGEOBJ														# initialize .obj and .lib file list
    LIBFILES=$DOSLARGELIB
    EXENAME=$DOSBINDIR"\\"$1 ; shift											# get .exe file name, always first
    while [[ $# -gt 0 ]]														# loop through .obj and .lib files
    do
        if [[ $1 =~ .*obj$ ]]													# match files with .obj extension
        then
            OBJFILES=$OBJFILES" "$1												# create .obj file list
        else
            LIBFILES=$LIBFILES" "$1												# create .lib file list, assume make transfers only .lib and .obj
        fi
        shift
    done
    echo $TLINKOPT" "$OBJFILES", "$EXENAME", , "$LIBFILES", ," > $LINKRSP
    CMD=$TLINK" @"$DOSSRCDIR"\\"$LINKRSP" > "$OUTFILE1
    #echo $CMD
    dosbox -c "$CMD" -c exit > /dev/null
    find . -name *.'[A-Z]*' -exec rename  's/(.*)\/([^\/]*)/$1\/\L$2/' {} \;
    awk -f tlink.awk $OUTFILE2
    ;;
    
    # bad option
    *)
    echo "build.sh:104: Error unknown build option."
    exit 1
    ;;

esac

exit 0


