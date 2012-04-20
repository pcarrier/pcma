#!/usr/bin/env python
from json import dumps
from msgpack import packb, unpackb
from sys import argv
from zmq import Context, REQ

ctx = Context()
sock = ctx.socket(REQ)
sock.connect("ipc:///var/run/pcma.socket")

sock.send(packb(argv[1:]))
ret, out = unpackb(sock.recv(), encoding='utf-8')

if ret:
    print(dumps(out))
else:
    raise Exception(out)

sock.close()
ctx.term()
