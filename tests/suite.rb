#!/usr/bin/env ruby

begin; require 'rubygems'; rescue; end

%w[zmq msgpack].each {|m| require m}

ctx = ZMQ::Context.new(1)

$sock = ctx.socket(ZMQ::REQ)
$sock.connect("ipc:///tmp/pcma.socket")

def run f
  $sock.send MessagePack.pack f
  r, v = MessagePack.unpack $sock.recv
  puts "#{r and 'B)' or ':('} #{f.inspect} => #{v.inspect}"
end

puts "=== STUFF THAT FAILS ==="
run 42
run []
run %w[list hop]
run %w[lock]
run %w[unlock]
run ['lock', 1]
run ['unlock', false]
run %w[lock /bin/dog]

puts "=== SHOULD WORK ==="
run %w[ping]
run %w[list]

puts "=== LET'S PLAY ==="
run %w[lock /bin/cat]
run %w[list]
run %w[lock /bin/cat]
run %w[list]
run %w[lock /bin/echo]
run %w[list]
run %w[unlock /bin/echo]
run %w[list]
run %w[unlock /bin/cat]
run %w[list]
