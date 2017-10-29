#!/usr/bin/env python

import time
import socket


TCP_IP = '192.168.1.19'
TCP_PORT = 60001
BUFFER_SIZE = 1024
MESSAGE1 = "1---5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---80"
MESSAGE2 = "1---5---10---15---20---25---30---35---40"
MESSAGE3 = "---45---50---55---60---65---70---75---80"



for x in range(0, 900):
    print "test number %d" % (x)
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    s.send(MESSAGE2)
    s.close()
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    s.send(MESSAGE1)
    s.close()
    #data = s.recv(BUFFER_SIZE)
    #print "received data:", data
    #s.shutdown(socket.SHUT_RDWR)
    #s.close()
    time.sleep(18)
    

#time.sleep(10)

