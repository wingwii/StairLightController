import sys
import time
import struct
import socket
from datetime import datetime

argv = sys.argv
argc = len(argv)

host = argv[1]

t = 0
if argc > 2:
    t = int(argv[2])
else:
    now = datetime.now()
    t = t + now.hour
    t = t * 60
    t = t + now.minute
    t = t * 60
    t = t + now.second
    
    print(str(t))

buf = struct.pack('<I', t)
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

i = 0
while i < 5:
    s.sendto(buf, (host, 8888))
    time.sleep(0.1)
    i = i + 1

