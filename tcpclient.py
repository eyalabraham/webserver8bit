#!/usr/bin/env python

import time
import socket


TCP_IP = '192.168.1.19'
TCP_PORT = 60001
BUFFER_SIZE = 1024
MESSAGE = "1---5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---80"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
s.send(MESSAGE)
#data = s.recv(BUFFER_SIZE)
#time.sleep(10)
#s.shutdown(socket.SHUT_RDWR)
s.close()

#print "received data:", data
