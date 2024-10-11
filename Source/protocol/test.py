#!/usr/bin/env python3
import serial
import sys
import struct

MSG_LEN         = 19
MSG_HEADER_LEN  = 7
MSG_HEADER      = '>HBBBBB'
MSG_BODY_ENABLE = '>B'
MSG_BODY_STATUS = '>BBB'
MSG_BODY_XMETER = '>BBHH'
MSG_BODY_STEPS  = '>BBHHBBHH'
MSG_BODY_PARAM  = '>BBBHH'
MSG_BODY_PRESET = '>BBBHH'

def verify_msg_crc(msg_array):
	crc = 0
	for i in range(len(msg_array)):
		crc += msg_array[i]
	return crc == 0
	
def gen_msg_crc(msg_array):
	crc = 0
	for i in range(len(msg_array)):
		crc += msg_array[i]
	crc = ~crc + 1
	msg_array[5] = crc

def pack_msg(msg_header, msg_body):
	msg_array = bytearray()
	msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
	if msg_body_len != 0 and msg_body_len != len(msg_body):
		print('pack_msg: msg_body_len mis-match')
		return None
	
	msg_header = struct.pack(MSG_HEADER, msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len)
	msg_array.append(msg_header)

	if msg_body_len:
		msg_array.append(msg_body)
		
	if len(msg_array) > MSG_LEN:
		print('pack_msg: msg too long')
		return None
		
	if msg_body_len:
		gen_msg_crc(msg_array)	
	
	pad_len = MSG_LEN - len(msg_array)
	for i in range(pad_len):
		msg_array.append(0)
	return bytes(msg_array)

def unpack_msg(msg_binary):
	msg_header = None
	msg_body   = None
	msg_array = bytearray()
	if len(msg_binary) < MSG_HEADER_LEN:
		print('unpack_msg: msg short than header')
		return (None, None)
	msg_header = msg_binary[0:MSG_HEADER_LEN]
	msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
	if len(msg_binary) < MSG_HEADER_LEN + msg_body_len
		print('unpack_msg: msg short than MSG_HEADER_LEN + msg_body_len')
		return (None, None)
	if msg_body_len:
		msg_body = msg_binary[MSG_HEADER_LEN:MSG_HEADER_LEN + msg_body_len]
	msg_array.append(msg_header)
	if msg_body:
		msg_array.append(msg_body)
	if not verify_msg_crc(msg_array):
		print('unpack_msg: msg crc not ok')
		return (None, None)
	return (msg_header, msg_body)

def dump_msg_header(msg_header):
	msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
	print('msg_magic     %04x'   % msg_magic)
	print('msg_code      %02x'   % msg_code)	
	print('msg_status    %02x'   % msg_status)
	print('msg_channel   %02x'   % msg_channel)
	print('msg_crc       %02x'   % msg_crc)
	print('msg_body_len  %02x'   % msg_body_len)

def dump_enable(msg_body):
	enable = struct.unpack(MSG_BODY_ENABLE, msg_body)
	print('msg_body_enable: %d' % enable)
	
def do_heartbeat(serial, args):
	cmd_header = struct.pack(MSG_HEADER, 0x1234, 1, 0, 0, 0, 0)
	cmd_msg = pack_msg(cmd_header, None)
	if cmd_msg:
		serial.write(cmd_msg)
		res_msg = serial.read(MSG_LEN)
		res_header, res_body = unpack_msg(res_msg)
		if res_header:
			dump_msg_header(res_header)
		
def do_start_stop_fan(serial, args):
	cmd_header = struct.pack(MSG_HEADER, 0x1234, 2, 0, 0, 0, 1)
	cmd_body   = struct.pack(MSG_BODY_ENABLE, args[0] == 'on' ? 1 : 0)
	cmd_msg = pack_msg(cmd_header, cmd_body)
	if cmd_msg:
		serial.write(cmd_msg)
		res_msg = serial.read(MSG_LEN)
		res_header, res_body = unpack_msg(res_msg)
		if res_header:
			dump_msg_header(res_header)		
		dump_enable(res_body)
#--------------------------------------------------------------------
tests = {
	'heartbeat': do_heartbeat,
	'fan' : do_start_stop_fan
}

if len(sys.argv) < 3:
	print("test.py [COM1] [cmd] <args>")
	sys.exit(1)

serialname = sys.argv[1]
testname   = sys.argv[2]

if testname in tests.keys():
	serialport = serial.Serial(
    port=serialname, baudrate=9600, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)
	tests[testname](serialname, sys.argv[2:-1])
	serialname.close()
else:
	print("no cmd name %s" % testname);
	sys.exit(1)

