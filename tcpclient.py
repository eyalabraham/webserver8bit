#!/usr/bin/env python

import time
import socket


TCP_IP = '192.168.1.19'
TCP_PORT = 7
BUFFER_SIZE = 1024
MESSAGE = "Hello, World!"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
#s.send(MESSAGE)
#data = s.recv(BUFFER_SIZE)
time.sleep(30)
#s.shutdown(socket.SHUT_RDWR)
s.close()

#print "received data:", data
