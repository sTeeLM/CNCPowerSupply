#!/usr/bin/env python3
import serial
import sys
import struct
import itertools

####CONSTANTS BEGIN####
MSG_LEN         = 19
MSG_HEADER      = '>HBBBBB'
MSG_HEADER_LEN  = 7
MSG_BODY_ENABLE = '>B'
MSG_BODY_ENABLE_LEN = 1
MSG_BODY_STATUS = '>BBBB'
MSG_BODY_STATUS_LEN = 4
MSG_BODY_XMETER = '>BBHH'
MSG_BODY_XMETER_LEN = 6
MSG_BODY_STEPS  = '>BBHHBBHH'
MSG_BODY_STEPS_LEN = 12
MSG_BODY_LIMITS  = '>BBHHBBHH'
MSG_BODY_LIMITS_LEN = 12
MSG_BODY_PARAM  = '>BBBHH'
MSG_BODY_PARAM_LEN = 7
MSG_BODY_PRESET = '>BBBHH'
MSG_BODY_PRESET_LEN = 7

CONTROL_MSG_CODE_NONE                = 0
CONTROL_MSG_CODE_HEATBEAT            = 1
CONTROL_MSG_CODE_REMOTE_OVERRIDE     = 2
CONTROL_MSG_CODE_START_STOP_FAN      = 3
CONTROL_MSG_CODE_START_STOP_SWITCH   = 4
CONTROL_MSG_CODE_GET_XMETER_STATUS   = 5
CONTROL_MSG_CODE_GET_ADC_CURRENT     = 6
CONTROL_MSG_CODE_GET_ADC_VOLTAGE_DISS= 7
CONTROL_MSG_CODE_GET_ADC_VOLTAGE_OUT = 8
CONTROL_MSG_CODE_GET_ADC_TEMP        = 9
CONTROL_MSG_CODE_GET_POWER_OUT       = 10
CONTROL_MSG_CODE_GET_POWER_DISS      = 11
CONTROL_MSG_CODE_GET_DAC_CURRENT     = 12
CONTROL_MSG_CODE_SET_DAC_CURRENT     = 13
CONTROL_MSG_CODE_GET_DAC_VOLTAGE     = 14
CONTROL_MSG_CODE_SET_DAC_VOLTAGE     = 15
CONTROL_MSG_CODE_GET_STEPS_VOLTAGE   = 16
CONTROL_MSG_CODE_GET_STEPS_CURRENT   = 17
CONTROL_MSG_CODE_GET_STEPS_TEMP      = 18
CONTROL_MSG_CODE_GET_STEPS_POWER     = 19
CONTROL_MSG_CODE_GET_LIMITS_CURRENT       = 20
CONTROL_MSG_CODE_GET_LIMITS_VOLTAGE_OUT   = 21
CONTROL_MSG_CODE_GET_LIMITS_TEMP          = 22
CONTROL_MSG_CODE_GET_LIMITS_POWER_DISS    = 23
CONTROL_MSG_CODE_GET_PARAM_TEMP_HI   = 24
CONTROL_MSG_CODE_SET_PARAM_TEMP_HI   = 25
CONTROL_MSG_CODE_GET_PARAM_TEMP_LO   = 26
CONTROL_MSG_CODE_SET_PARAM_TEMP_LO   = 27
CONTROL_MSG_CODE_GET_PARAM_OVER_HEAT = 28
CONTROL_MSG_CODE_SET_PARAM_OVER_HEAT = 29
CONTROL_MSG_CODE_GET_PARAM_MAX_POWER_DISS  = 30
CONTROL_MSG_CODE_SET_PARAM_MAX_POWER_DISS  = 31
CONTROL_MSG_CODE_GET_PARAM_BEEP      = 32
CONTROL_MSG_CODE_SET_PARAM_BEEP      = 33
CONTROL_MSG_CODE_GET_PRESET_CURRENT  = 34
CONTROL_MSG_CODE_SET_PRESET_CURRENT  = 35
CONTROL_MSG_CODE_GET_PRESET_VOLTAGE  = 36
CONTROL_MSG_CODE_SET_PRESET_VOLTAGE  = 37
CONTROL_MSG_CODE_CNT                 = 38
####CONSTANTS END####

def verify_msg_crc(msg_array):
    crc = 0
    for i in range(len(msg_array)):
        crc += msg_array[i]
    return (crc & 0xFF) == 0
    
def gen_msg_crc(msg_array):
    crc = 0
    for i in range(len(msg_array)):
        crc += msg_array[i]
    crc = ~crc + 1
    msg_array[5] = (crc & 0xFF)

def pack_msg(msg_header, msg_body):
    msg_array = bytearray()
    msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
    if msg_body_len != 0 and msg_body_len != len(msg_body):
        print('pack_msg: msg_body_len mis-match')
        return None
    
    msg_array.extend(msg_header)

    if msg_body_len:
        msg_array.extend(msg_body)
        
    if len(msg_array) > MSG_LEN:
        print('pack_msg: msg too long')
        return None
    
    gen_msg_crc(msg_array)    
    
    pad_len = MSG_LEN - len(msg_array)
    msg_array.extend(itertools.repeat(0, pad_len))
    return bytes(msg_array)

def unpack_msg(msg_binary):
    msg_header = None
    msg_body   = None
    msg_array = bytearray()
    if len(msg_binary) < MSG_HEADER_LEN:
        print('unpack_msg: msg short than header %d < %d' % (len(msg_binary), MSG_HEADER_LEN))
        return (None, None)
    msg_header = msg_binary[0:MSG_HEADER_LEN]
    msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
    if len(msg_binary) < MSG_HEADER_LEN + msg_body_len :
        print('unpack_msg: msg short than MSG_HEADER_LEN + msg_body_len')
        return (None, None)
    if msg_body_len:
        msg_body = msg_binary[MSG_HEADER_LEN:MSG_HEADER_LEN + msg_body_len]
    msg_array.extend(msg_header)
    if msg_body:
        msg_array.extend(msg_body)
    if not verify_msg_crc(msg_array):
        print('unpack_msg: msg crc not ok')
        return (None, None)
    return (msg_header, msg_body)

def dump_msg_header(msg_header):
    msg_magic, msg_code, msg_status, msg_channel, msg_crc, msg_body_len =  struct.unpack(MSG_HEADER, msg_header)
    print('msg_magic     0x%04X'   % msg_magic)
    print('msg_code      0x%02X'   % msg_code)    
    print('msg_status    0x%02X'   % msg_status)
    print('msg_channel   0x%02X'   % msg_channel)
    print('msg_crc       0x%02X'   % msg_crc)
    print('msg_body_len  0x%02X'   % msg_body_len)

def dump_enable(msg_body):
    enable = struct.unpack(MSG_BODY_ENABLE, msg_body)
    print('msg_body_enable: %d' % enable)
    
def do_heartbeat(serial, args):
    if len(args) != 1:
        print('heartbeat [channel no]')
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, CONTROL_MSG_CODE_HEATBEAT, 0, int(args[0]), 0, 0)
    cmd_msg = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)

def do_on_off(serial, cmd_str, cmd_code, args):
    if len(args) != 2:
        print('%s [channel no] [on|off]' % cmd_str)
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_ENABLE_LEN)
    cmd_body   = struct.pack(MSG_BODY_ENABLE, 1 if args[1] == 'on' else 0)
    cmd_msg    = pack_msg(cmd_header, cmd_body)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)        
        dump_enable(res_body)    

def do_remote_override(serial, args):
    do_on_off(serial, 'override', CONTROL_MSG_CODE_REMOTE_OVERRIDE, args)
        
def do_start_stop_fan(serial, args):
    do_on_off(serial, 'fan', CONTROL_MSG_CODE_START_STOP_FAN, args)
    
def do_start_stop_switch(serial, args):
    do_on_off(serial, 'switch', CONTROL_MSG_CODE_START_STOP_SWITCH, args)
    
def do_get_status(serial, args):
    if len(args) != 1:
        print('status [channel no]')
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, CONTROL_MSG_CODE_GET_XMETER_STATUS, 0, int(args[0]), 0, 0)
    cmd_msg = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            override_on, fan_on, switch_on, cc_on = struct.unpack(MSG_BODY_STATUS, res_body)
            print('override_on : %d' % override_on)
            print('fan_on : %d' % fan_on)
            print('switch_on : %d' % switch_on)
            print('cc_on : %d' % cc_on)
            
def dump_xmeter_val(msg_body):            
    (b_neg, b_res, h_integer, h_decimal) = struct.unpack(MSG_BODY_XMETER, msg_body)
    print('%d/%s%d.%03d' % (b_res, '-' if b_neg else '+', h_integer, h_decimal))
    
def do_get_xmeter_value(serial, cmd_str, cmd_code, args):
    if len(args) != 1:
        print('%s [channel no]' % cmd_str)
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, 0)
    cmd_msg = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_xmeter_val(res_body)

# res/<->integer.decimal
def parse_xmeter_value(val_str):
    b_res        = 0
    h_integer    = 0
    h_decimal    = 0
    b_neg        = 0
    try:
        b_res,remain = val_str.split('/')
        b_res    = int(b_res)
        f_value  = float(remain)
        h_integer = int(f_value)
        h_decimal  = int((f_value - int(f_value)) * 1000)
        if h_integer < 0:
            h_integer = 0 - h_integer
            h_decimal = 0 - h_decimal
            b_neg = 1
    except (ValueError, TypeError):
        return None
    return (b_neg, b_res, h_integer, h_decimal)

def pack_xmeter_value(xmeter_value):
    return struct.pack(MSG_BODY_XMETER, xmeter_value[0], xmeter_value[1], xmeter_value[2], xmeter_value[3])
    
def do_set_xmeter_value(serial, cmd_str, cmd_code, args):
    if len(args) != 2:
        print('%s [channel no] [xmeter value]' % cmd_str)
        return
    xmeter_value = parse_xmeter_value(args[1])
    if not xmeter_value :
        print('invalid xmeter value' % args[1])
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_XMETER_LEN)
    cmd_body   = pack_xmeter_value(xmeter_value)
    cmd_msg    = pack_msg(cmd_header, cmd_body)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_xmeter_val(res_body)

def do_get_adc_current(serial, args):
    do_get_xmeter_value(serial, 'get_adc_current', CONTROL_MSG_CODE_GET_ADC_CURRENT, args)

def do_get_adc_voltage_diss(serial, args):
    do_get_xmeter_value(serial, 'get_adc_voltage_diss', CONTROL_MSG_CODE_GET_ADC_VOLTAGE_DISS, args)

def do_get_adc_voltage_out(serial, args):
    do_get_xmeter_value(serial, 'get_adc_voltage_out', CONTROL_MSG_CODE_GET_ADC_VOLTAGE_OUT, args)

def do_get_adc_temp(serial, args):
    do_get_xmeter_value(serial, 'get_adc_temp', CONTROL_MSG_CODE_GET_ADC_TEMP, args)

def do_get_power_out(serial, args):
    do_get_xmeter_value(serial, 'get_power_out', CONTROL_MSG_CODE_GET_POWER_OUT, args)

def do_get_power_diss(serial, args):
    do_get_xmeter_value(serial, 'get_power_diss', CONTROL_MSG_CODE_GET_POWER_DISS, args)    
    
def do_get_dac_voltage(serial, args):
    do_get_xmeter_value(serial, 'get_dac_voltage', CONTROL_MSG_CODE_GET_DAC_VOLTAGE, args)
    
def do_set_dac_voltage(serial, args):
    do_set_xmeter_value(serial, 'set_dac_voltage', CONTROL_MSG_CODE_SET_DAC_VOLTAGE, args)
    
def do_get_dac_current(serial, args):
    do_get_xmeter_value(serial, 'get_dac_current', CONTROL_MSG_CODE_GET_DAC_CURRENT, args)
    
def do_set_dac_current(serial, args):
    do_set_xmeter_value(serial, 'set_dac_current', CONTROL_MSG_CODE_SET_DAC_CURRENT, args)    


def dump_steps(res_body):
    (b_neg_c, b_res_c, h_integer_c, h_decimal_c, b_neg_f, b_res_f, h_integer_f, h_decimal_f) = \
        struct.unpack(MSG_BODY_STEPS, res_body)
    print('coarse: %d/%s%d.%03d' % (b_res_c, '-' if b_neg_c else '+', h_integer_c, h_decimal_c))
    print('fine:   %d/%s%d.%03d' % (b_res_f, '-' if b_neg_f else '+', h_integer_f, h_decimal_f))

def do_get_steps(serial, cmd_str, cmd_code, args):
    if len(args) != 1:
        print('%s [channel no]' % cmd_str)
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, 0)
    cmd_msg    = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_steps(res_body)

#def pack_steps(coarse, fine):
#    return struct.pack(MSG_BODY_STEPS, 
#        coarse[0], coarse[1], coarse[2], coarse[3],
#        fine[0], fine[1], fine[2], fine[3])
#    
#def do_set_steps(serial, cmd_str, cmd_code, args):
#    if len(args) != 3:
#        print('%s [channel no] [coarse] [fine]' % cmd_str)
#        return
#    xmeter_value_coarse = parse_xmeter_value(args[1])
#    xmeter_value_fine   = parse_xmeter_value(args[2])
#    if not xmeter_value_coarse:
#        print('invalid xmeter value' % args[1])
#        return
#    if not xmeter_value_fine:
#        print('invalid xmeter value' % args[2])
#        return        
#    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_STEPS_LEN)
#    cmd_body   = pack_steps(xmeter_value_coarse, xmeter_value_fine)
#    cmd_msg    = pack_msg(cmd_header, cmd_body)
#    if cmd_msg:
#        serial.write(cmd_msg)
#        res_msg = serial.read(MSG_LEN)
#        res_header, res_body = unpack_msg(res_msg)
#        if res_header:
#            dump_msg_header(res_header)
#        if res_body:
#            dump_steps(res_body)            
            
def do_get_steps_voltage(serial, args):
    do_get_steps(serial, 'get_steps_voltage', CONTROL_MSG_CODE_GET_STEPS_VOLTAGE, args)    
    
def do_get_steps_current(serial, args):
    do_get_steps(serial, 'get_steps_current', CONTROL_MSG_CODE_GET_STEPS_CURRENT, args)    
    
def do_get_steps_temp(serial, args):
    do_get_steps(serial, 'get_steps_temp', CONTROL_MSG_CODE_GET_STEPS_TEMP, args)
    
def do_get_steps_power(serial, args):
    do_get_steps(serial, 'get_steps_power', CONTROL_MSG_CODE_GET_STEPS_POWER, args)    


def dump_limits(res_body):
    (b_neg_c, b_res_c, h_integer_c, h_decimal_c, b_neg_f, b_res_f, h_integer_f, h_decimal_f) = \
        struct.unpack(MSG_BODY_STEPS, res_body)
    print('min: %d/%s%d.%03d' % (b_res_c, '-' if b_neg_c else '+', h_integer_c, h_decimal_c))
    print('max: %d/%s%d.%03d' % (b_res_f, '-' if b_neg_f else '+', h_integer_f, h_decimal_f))

def do_get_limits(serial, cmd_str, cmd_code, args):
    if len(args) != 1:
        print('%s [channel no]' % cmd_str)
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, 0)
    cmd_msg    = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_limits(res_body)

def do_get_limits_current(serial, args):
    do_get_limits(serial, 'get_limits_current', CONTROL_MSG_CODE_GET_LIMITS_CURRENT, args) 
    
def do_get_limits_voltage_out(serial, args):
    do_get_limits(serial, 'get_limits_voltage_outt', CONTROL_MSG_CODE_GET_LIMITS_VOLTAGE_OUT, args) 
    
def do_get_limits_temp(serial, args):
    do_get_limits(serial, 'get_limits_temp', CONTROL_MSG_CODE_GET_LIMITS_TEMP, args) 
    
def do_get_limits_power_diss(serial, args):
    do_get_limits(serial, 'get_limits_power_diss', CONTROL_MSG_CODE_GET_LIMITS_POWER_DISS, args) 


def dump_param(res_body):
    (uint8_val, b_neg, b_res, h_integer, h_decimal) = struct.unpack(MSG_BODY_PARAM, res_body)
    print('uint8_val: %x' % uint8_val)
    print('xmeter_value: %d/%s%d.%03d' % (b_res, '-' if b_neg else '+', h_integer, h_decimal))

def do_get_param_xmeter_value(serial, cmd_str, cmd_code, args):
    if len(args) != 1:
        print('%s [channel no]' % cmd_str)
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, 0)
    cmd_msg    = pack_msg(cmd_header, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_param(res_body)

def pack_param(uint8_val, xmeter_value):
    if not xmeter_value:
        xmeter_value =  (0, 0, 0, 0)
    return struct.pack(MSG_BODY_PRESET, uint8_val, *xmeter_value)

def do_set_param_xmeter_value(serial, cmd_str, cmd_code, args):
    if len(args) != 2:
        print('%s [channel no] [xmeter value]' % cmd_str)
        return
    xmeter_value = parse_xmeter_value(args[1])
    if not xmeter_value :
        print('invalid xmeter value' % args[1])
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_PARAM_LEN)
    cmd_body   = pack_param(0, xmeter_value)
    cmd_msg    = pack_msg(cmd_header, cmd_body)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_param(res_body)
 
def do_get_param_temp_hi(serial, args):
    do_get_param_xmeter_value(serial, 'get_param_temp_hi', CONTROL_MSG_CODE_GET_PARAM_TEMP_HI, args) 
    
def do_set_param_temp_hi(serial, args):
    do_set_param_xmeter_value(serial, 'set_param_temp_hi', CONTROL_MSG_CODE_SET_PARAM_TEMP_HI, args)     

def do_get_param_temp_lo(serial, args):
    do_get_param_xmeter_value(serial, 'get_param_temp_lo', CONTROL_MSG_CODE_GET_PARAM_TEMP_LO, args) 
    
def do_set_param_temp_lo(serial, args):
    do_set_param_xmeter_value(serial, 'set_param_temp_lo', CONTROL_MSG_CODE_SET_PARAM_TEMP_LO, args)   
    
    
def do_get_param_over_heat(serial, args):
    do_get_param_xmeter_value(serial, 'get_param_over_heat', CONTROL_MSG_CODE_GET_PARAM_OVER_HEAT, args) 
    
def do_set_param_over_heat(serial, args):
    do_set_param_xmeter_value(serial, 'set_param_over_heat', CONTROL_MSG_CODE_SET_PARAM_OVER_HEAT, args)     

def do_get_param_max_pd(serial, args):
    do_get_param_xmeter_value(serial, 'get_param_max_pd', CONTROL_MSG_CODE_GET_PARAM_MAX_POWER_DISS, args) 
    
def do_set_param_max_pd(serial, args):
    do_set_param_xmeter_value(serial, 'set_param_max_pd', CONTROL_MSG_CODE_SET_PARAM_MAX_POWER_DISS, args) 
    
def do_get_param_beep(serial, args):
    do_get_param_xmeter_value(serial, 'set_param_max_pd', CONTROL_MSG_CODE_SET_PARAM_BEEP, args)
    
def do_set_param_beep(serial, args):
    if len(args) != 1:
        print('set_param_beep [channel no] [on|off]')
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, CONTROL_MSG_CODE_HEATBEAT, 0, int(args[0]), 0, MSG_BODY_PARAM_LEN)
    cmd_msg = pack_msg(cmd_header, None)
    cmd_body   = pack_param(MSG_BODY_PARAM, 1 if args[1] == 'on' else 0, None)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_enable(res_body)
    
    
def dump_preset(res_body):
    (index, b_neg, b_res, h_integer, h_decimal) = struct.unpack(MSG_BODY_PRESET, res_body)
    print('index: %d' % index)
    print('xmeter_val: %d/%s%d.%03d' % (b_res, '-' if b_neg else '+', h_integer, h_decimal))

def pack_preset(index, xmeter_value):
    if not xmeter_value:
        xmeter_value = (0, 0, 0, 0)
    return struct.pack(MSG_BODY_PRESET, index, xmeter_value[0], xmeter_value[1], xmeter_value[2], xmeter_value[3])

def do_get_preset(serial, cmd_str, cmd_code, args):
    if len(args) != 2:
        print('%s [channel no] [index]' % cmd_str)
        return
    index = int(args[1])
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_PRESET_LEN)
    cmd_body   = pack_preset(index, None)
    cmd_msg    = pack_msg(cmd_header, cmd_body)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_preset(res_body)

def do_set_preset(serial, cmd_str, cmd_code, args):
    if len(args) != 3:
        print('%s [channel no] [index] [xmeter value]' % cmd_str)
        return
    index = int(args[1])
    xmeter_value = parse_xmeter_value(args[2])
    if not xmeter_value :
        print('invalid xmeter value' % args[2])
        return
    cmd_header = struct.pack(MSG_HEADER, 0x1234, cmd_code, 0, int(args[0]), 0, MSG_BODY_PRESET_LEN)
    cmd_body   = pack_preset(index, xmeter_value)
    cmd_msg    = pack_msg(cmd_header, cmd_body)
    if cmd_msg:
        serial.write(cmd_msg)
        res_msg = serial.read(MSG_LEN)
        res_header, res_body = unpack_msg(res_msg)
        if res_header:
            dump_msg_header(res_header)
        if res_body:
            dump_preset(res_body)

def do_get_preset_current(serial, args):
    do_get_preset(serial, 'get_preset_current', CONTROL_MSG_CODE_GET_PRESET_CURRENT, args) 
    
def do_set_preset_current(serial, args):
    do_set_preset(serial, 'set_preset_current', CONTROL_MSG_CODE_SET_PRESET_CURRENT, args)
    
def do_get_preset_voltage(serial, args):
    do_get_preset(serial, 'get_preset_voltage', CONTROL_MSG_CODE_GET_PRESET_VOLTAGE, args) 
    
def do_set_preset_voltage(serial, args):
    do_set_preset(serial, 'set_preset_voltage', CONTROL_MSG_CODE_SET_PRESET_VOLTAGE, args)     
#--------------------------------------------------------------------
tests = {
    'heartbeat': do_heartbeat,
    'override': do_remote_override,
    'fan' : do_start_stop_fan,
    'switch': do_start_stop_switch,
    'status' : do_get_status,
    'get_adc_current' : do_get_adc_current,
    'get_adc_voltage_diss' : do_get_adc_voltage_diss,
    'get_adc_voltage_out' : do_get_adc_voltage_out,    
    'get_adc_temp' : do_get_adc_temp,
    'get_power_out' : do_get_power_out,
    'get_power_diss' : do_get_power_diss,
    'get_dac_voltage' : do_get_dac_voltage,
    'set_dac_voltage' : do_set_dac_voltage,    
    'get_dac_current' : do_get_dac_current,
    'set_dac_current' : do_set_dac_current,    
    'get_steps_voltage' : do_get_steps_voltage,
    'get_steps_current' : do_get_steps_current,
    'get_steps_temp' : do_get_steps_temp,
    'get_steps_power' : do_get_steps_power,
    'get_limits_current' : do_get_limits_current,
    'get_limits_voltage_out' : do_get_limits_voltage_out,
    'get_limits_temp' : do_get_limits_temp,
    'get_limits_power_diss' : do_get_limits_power_diss,
    'get_param_temp_hi' : do_get_param_temp_hi,    
    'set_param_temp_hi' : do_set_param_temp_hi,    
    'get_param_temp_lo' : do_get_param_temp_lo,    
    'set_param_temp_lo' : do_set_param_temp_lo,    
    'get_param_overheat' : do_get_param_over_heat,    
    'set_param_overheat' : do_set_param_over_heat,
    'get_param_max_pd' : do_get_param_max_pd,    
    'set_param_max_pd' : do_set_param_max_pd,
    'get_param_beep' : do_get_param_beep,    
    'set_param_beep' : do_set_param_beep,
    'get_preset_current' : do_get_preset_current,
    'set_preset_current' : do_set_preset_current,    
    'get_preset_voltage' : do_get_preset_voltage,
    'set_preset_voltage' : do_set_preset_voltage,    
}

if len(sys.argv) < 3:
    print("test.py [COMx] [cmd] <args>")
    sys.exit(1)

serialname = sys.argv[1]
testname   = sys.argv[2]

if testname in tests.keys():
    serialport = serial.Serial(
        port=serialname, baudrate=115200, bytesize=8, timeout=10, stopbits=serial.STOPBITS_ONE)
    tests[testname](serialport, sys.argv[3:])
    serialport.close()
else:
    print("no cmd name %s" % testname);
    sys.exit(1)

