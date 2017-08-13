#############################################
#
# logparse.awk
#
# awk script that will read and parse messages.h 
# header file and then use it to parse the log stream
# from executive lmte debug output
#
# invoke awk with: --non-decimal-data
#
#############################################

BEGIN           {
                    messagesFile = "./include/messages.h"
                    while (( getline message < messagesFile) > 0)   # load 'messages.h' header and build messages hash table
                    {
                        if (match(message,"@@"))                    # look for parsing token
                        {
                            split(message,msgField)
                            messageId[msgField[3]] = msgField[2]    # if parsing token exists, save the message and its ID
                        }
                    }
                    messageId[255] = "DB_TRACE"                     # save debug trace special message
                    
                    taskName["db"] = "debug"                        # set task name and ID for debug messages
                    
                    debugType[0] = "DB_INFO"                        # special debug message type from 'lmte.h'
                    debugType[1] = "DB_BAD_TNAME"
                    debugType[2] = "DB_BAD_MSG"
                    debugType[3] = "DB_BAD_PARAM"
                    debugType[4] = "DB_TIME_OUT"
                    debugType[5] = "DB_USER"
                    debugType[6] = "{undefined}"                    # a marker in case I forget to update the list...
                    
                    prevClockTics = 0                               # setup variable
                    
                    printf("\n")
                    printf(" sys tics      delta tics     src         dest         message           payload    payload\n")
                    printf("------------ -------------- --------    -------- -------------------- ------------ ---------\n")
                }
        
/registered/    {                                                   # build task names and IDs table
                    taskName[$4] = $2                               # store task name for task id
                }

/mSec/          {                                                   # get correct mili-sec per time tic
                    msecPerTic = hex2dec($5)
                }

/TR/            {                                                   # pick trace line and interpret
                    currentClockTics = hex2dec($2)                  # get clock tics and calculate delta from last output
                    deltaClockTics = currentClockTics - prevClockTics
                    msgId = hex2dec($5)                             # get message ID
                    wPayload = $6
                    dwPayload = $7
                    
                    if (msgId == 255)                               # special handling for debug messages
                    {
                        wPayload = debugType[hex2dec($6)]           # get the debug message type
                        dwPayload = hex2dec($7)                     # this is usually the line number of the posting module
                    }
                                                                    # print out a converted format of the debug output
                    printf("%12s [%12s] %-8s -> %-8s %-20s %-12s %s\n",
                           tics2time(currentClockTics),
                           tics2time(deltaClockTics),
                           taskName[$3],
                           taskName[$4],
                           messageId[msgId],
                           wPayload,
                           dwPayload)
                    prevClockTics = currentClockTics
                }

END             {                                                   # nothing here.
                }

#--------------------------------------------
# hex2dec()
#
#   function converts hex string to decimal value
#   must invoke awk with --non-decimal-data option
#
function hex2dec(hexNum)
{
    hexNum = "0x"hexNum
    decNum = sprintf("%d",hexNum)
    return  decNum
}

#--------------------------------------------
# tics2time()
#
#   function converts clock tics to an 
#   hh:mm:ss.mmm formated string
#
function tics2time(clockTics)
{
    time = clockTics * msecPerTic                                   # conver tics to mili-sec
    hours = int(time/1000/60/60)
    min = int(time/1000/60) - hours*60
    sec = int(time/1000) - 60*(min + hours*60)
    msec = time - 1000*(sec + 60*(min + hours*60))
    timeFormat = sprintf("%02d:%02d:%02d.%03d",hours,min,sec,msec)
    return timeFormat
}

#############################################
#
# --- sample executive debug output ---
#
#  TR 0000000d 01 02 0a 0001 00000000
#  +  ----+--- +  +  +  -+-- ---+----
#  |      |    |  |  |   |      |
#  |      |    |  |  |   |      +------ dwPayload
#  |      |    |  |  |   +-------------  wPaylod
#  |      |    |  |  +-----------------  bPayload
#  |      |    |  +-------------------- message destination task ID
#  |      |    +----------------------- message source task ID, if eq to 'db' then debug message
#  |      +---------------------------- time ticks in mSec
#  +----------------------------------- 'TR' tag for trace strings
#
# B:\BIN>ws3
# Build: ws.exe Dec  1 2016 19:06:21
# Registering task (t_term)
#   task entry point 0x1046:0xe04
#   message queue allocated, slot count 1
#   stack allocated
#   registered t_term / 01
# Registering task (t_dummy)
#   task entry point 0x1046:0xe98
#   message queue allocated, slot count 1
#   stack allocated
#   registered t_dummy / 02
# Starting scheduler
# Registering task (idle)
#   task entry point 0x1046:0x1d2
#   message queue allocated, slot count 1
#   stack allocated
#   registered idle / 03
#   debug level DB_LEVEL(3)
#   scheduler starting mSec/tick = 01 ...
# in t_term()
# in t_dummy()
# TR 0000000d 01 02 0a 0000 00000000
# TR 0000003f 01 db ff 0004 00000000
# TR 00000427 01 02 0a 0000 00000000
# TR 00000459 01 db ff 0004 00000000
# TR 00000841 01 02 0a 0000 00000000
# TR 00000873 01 db ff 0004 00000000
# TR 00000c5b 01 02 0a 0000 00000000
# TR 00000c8d 01 db ff 0004 00000000
# TR 00001075 01 02 0a 0000 00000000
# TR 000010a7 01 db ff 0004 00000000
#
#
# JK Microsystems Flashlite Bios Version 1.5a
# DOS Version 3.3a for JK microsystems Flashlite
# (C) HBS Corp and JK microsystems 1991-1997
#
# B:\BIN>ws4
# in t_term()
# in t_dummy()
# TR 0000000d 01 02 0a 0001 00000000
# TR 0000001b 02 01 0b 0000 00000000
# TR 00000419 01 02 0a 0001 00000000
# TR 0000041a 02 01 0b 0000 00000000
# TR 00000815 01 02 0a 0001 00000000
# TR 00000816 02 01 0b 0000 00000000
#
#############################################
