#!/usr/bin/env python

import time
import socket

TCP_IP = '192.168.1.10'
TCP_PORT = 60002
BUFFER_SIZE = 1024
MESSAGE = "Hello, World!"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((TCP_IP, TCP_PORT))
s.listen(0)

print 'listening ...'
(conn, address) = s.accept()

print 'accepted connection from:',address

while True:
    data = conn.recv(BUFFER_SIZE)
    if not data: break
    print data
    conn.send(data)

#time.sleep(5)

print 'closing connection ...'

conn.shutdown(socket.SHUT_RDWR)
conn.close()

s.shutdown(socket.SHUT_RDWR)
s.close()

#print "received data:", data
