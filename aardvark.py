#!/usr/bin/python

#==========================================================================
# IMPORTS
#==========================================================================
import os
import sys
import time

from aardvark_py import *

USAGE = "Usage: aardvark <port> < -r | -n | -f >"
RELAY_ENV = "RELAY"

#==========================================================================
# MAIN PROGRAM
#==========================================================================
if (len(sys.argv) < 3):
    print USAGE
    sys.exit()

opt_set = set(['-r', '-n', '-f'])
port = int(sys.argv[1])
opt  = sys.argv[2]

# check options
if not (opt in opt_set):
    print "Undefined option %s" % opt
    print USAGE
    sys.exit()

# Open the device
(handle, aaext) = aa_open_ext(port)
if (handle <= 0):
    print "Unable to open Aardvark device on port %d" % port
    print "Error code = %d" % handle
    sys.exit()

# Configure the Aardvark adapter so all pins
# are now controlled by the GPIO subsystem and are outputs
aa_configure(handle, AA_CONFIG_GPIO_ONLY)
aa_gpio_direction(handle, 0x3f)

# Turn off the external I2C line pullups
aa_i2c_pullup(handle, AA_I2C_PULLUP_NONE)

# activate GPIO relay bits 1 and 2 according to options
if opt == '-r':
    if os.path.exists("on"):
        aa_gpio_set(handle, 0x03)   # only reset the system if it is 'on'
        time.sleep(0.2)
        aa_gpio_set(handle, 0x02)
elif opt == '-n':
    aa_gpio_set(handle, 0x02)       # power on
    if not os.path.exists("on"):
        os.mknod("on")
elif opt == '-f':
    aa_gpio_set(handle, 0x00)       # power off
    if os.path.exists("on"):
        os.remove("on")
else:
    print "oops!"                   # should not happen, checked options already!

# Close the device
aa_close(handle)
