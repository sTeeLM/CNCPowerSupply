#!/usr/bin/env python3
import serial
import sys
import struct

MSG_LEN         = 19
MSG_HEADER      = 'HBBBBB'
MSG_BODY_ENABLE = 'B'
MSG_BODY_STATUS = 'BBB'
MSG_BODY_XMETER = 'BBHH'
MSG_BODY_STEPS  = 'BBHHBBHH'
MSG_BODY_PARAM  = 'BBBHH'
MSG_BODY_PRESET = 'BBBHH'

def gen_msg_crc(msg_body, body_len):
	crc = 0
	for i in range(body_len):
		crc += msg_body[i]
	crc = ~crc + 1
	return crc & 0xff

def pack_msg(msg_header, msg_body):
	msg_array = bytearray()
	msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
	if msg_body_len != 0 and msg_body_len != len(msg_body):
		print('pack_msg: msg_body_len mis-match')
		return None
		
	if msg_body_len:
		msg_crc = gen_msg_crc(msg_body, msg_body_len)
	
	msg_header = struct.pack(MSG_HEADER, msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len)
	msg_array.append(msg_header)

	if msg_body_len:
		msg_array.append(msg_body)
		
	if len(msg_array) > MSG_LEN:
		print('pack_msg: msg too long')
		return None
		
	pad_len = MSG_LEN - len(msg_array)
	for i in range(pad_len):
		msg_array.append(0)
	return bytes(msg_array)
	

def dump_msg(msg_binary):
	pass

def do_heartbeat(serial):
	cmd_header = struct.pack(MSG_HEADER, 0x1234, 1, 0, 0, 0, 0)
	cmd_msg = pack_msg(cmd_header, None)
	if cmd_msg:
		serial.write(cmd_msg)
		res_msg = serial.read(MSG_LEN)
		dump_msg(res_msg)

tests = {
	'heartbeat':do_heartbeat
}

if len(sys.argv) != 3:
	print("test.py COM1 heartbeat")
	sys.exit(1)

serialname = sys.argv[1]
testname   = sys.argv[2]

if testname in tests.keys():
	serialport = serial.Serial(
    port=serialname, baudrate=9600, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)
	tests[testname](serialname)
	serialname.close()
else:
	print("no test name %s" % testname);
	sys.exit(1)

