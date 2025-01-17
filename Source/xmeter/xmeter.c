#include "xmeter.h"
#include "gpio.h"
#include "i2c.h"
#include "debug.h"
#include "delay.h"
#include "rom.h"
#include "task.h"
#include "lcd.h"

#include <stdint.h>
#include <string.h>
#include <limits.h>

#define XMETER_DAC_I2C_VOLTAGE   0x98 // 10011000
#define XMETER_DAC_I2C_CURRENT   0x9c // 10011100
#define XMETER_DAC_I2C_SUBADDR_CONFIG 0x11


#define XMETER_CONV_RDY_WAIT10US 100

#define XMETER_ADC_I2C_ADDR 0x90 // 1001 00 0
#define XMETER_ADC_I2C_SUBADDR_CONV 0x0  // Conversion register
#define XMETER_ADC_I2C_SUBADDR_CONFIG 0x1  // Config register
#define XMETER_ADC_I2C_SUBADDR_LOTH 0x2  // Lo_thresh register
#define XMETER_ADC_I2C_SUBADDR_HITH 0x3  // Hi_thresh register

/* 由于DAC8571不能可靠读出，所以读取的时候直接从这里读！ */
static uint16_t xmeter_dac_internal_bit_v;
static uint16_t xmeter_dac_internal_bit_c;

#define XMETER_ADC_MAX_WAIT_CNT 255

/*
100 : AINP = AIN0 and AINN = GND
101 : AINP = AIN1 and AINN = GND
110 : AINP = AIN2 and AINN = GND
111 : AINP = AIN3 and AINN = GND
*/
#define XMETER_ADC_MUX_CURRENT  (0x4 << 12)  
#define XMETER_ADC_MUX_VOLTAGE_OUT  (0x5 << 12)
#define XMETER_ADC_MUX_VOLTAGE_DISS (0x6 << 12)
#define XMETER_ADC_MUX_TEMP  (0x7 << 12)

xmeter_value_t xmeter_adc_current;         // 输出电压
xmeter_value_t xmeter_adc_voltage_out;     // 输出电流
xmeter_value_t xmeter_power_out;           // 输出功率
xmeter_value_t xmeter_power_diss;          // 耗散功率
xmeter_value_t xmeter_adc_temp;            // 温度
xmeter_value_t xmeter_adc_voltage_diss;    // 耗散电压

static double xmeter_adc_current_f;
static xmeter_grid_t xmeter_adc_current_g[XMETER_GRID_SIZE];

static double xmeter_adc_voltage_out_f;
static xmeter_grid_t xmeter_adc_voltage_out_g[XMETER_GRID_SIZE];

static double xmeter_adc_voltage_diss_f;
static xmeter_grid_t xmeter_adc_voltage_diss_g[XMETER_GRID_SIZE];

static double xmeter_adc_temp_f;
static double xmeter_power_out_f;
static double xmeter_power_diss_f;

xmeter_value_t xmeter_dac_current;
xmeter_value_t xmeter_dac_voltage;

static xmeter_grid_t xmeter_dac_current_g[XMETER_GRID_SIZE];

static xmeter_grid_t xmeter_dac_voltage_g[XMETER_GRID_SIZE];

static const xmeter_value_t code xmeter_dac_max_voltage = {0, XMETER_RES_VOLTAGE, 30, 0}; /* 30V */
static const xmeter_value_t code xmeter_dac_max_current = {0, XMETER_RES_CURRENT, 5, 0}; /* 5A */

xmeter_value_t xmeter_temp_hi;
xmeter_value_t xmeter_temp_lo;
xmeter_value_t xmeter_temp_overheat;

xmeter_value_t xmeter_max_power_diss;

static const xmeter_value_t code xmeter_max_temp_hi = {0, XMETER_RES_TEMP, 150, 0} /* 150 C */;
static const xmeter_value_t code xmeter_min_temp_lo = {0, XMETER_RES_TEMP, 0, 0};  /* 0 C */
static const xmeter_value_t code xmeter_max_power_diss_max = {0, XMETER_RES_POWER, 120, 0};  /* 80W */
static const xmeter_value_t code xmeter_max_power_diss_min = {0, XMETER_RES_POWER, 40, 0};  /* 40W */
static const xmeter_value_t code xmeter_zero2 = {0, XMETER_RES_TWO, 0, 0};
const xmeter_value_t code xmeter_zero3 = {0, XMETER_RES_THREE, 0, 0};

static const xmeter_value_t code xmeter_step_fine_voltage = {0, XMETER_RES_VOLTAGE, 0, 1};
static const xmeter_value_t code xmeter_step_coarse_voltage = {0, XMETER_RES_VOLTAGE, 0, 100};
static const xmeter_value_t code xmeter_step_fine_current = {0, XMETER_RES_CURRENT, 0, 1};
static const xmeter_value_t code xmeter_step_coarse_current = {0, XMETER_RES_CURRENT, 0, 100};
static const xmeter_value_t code xmeter_step_temp = {0, XMETER_RES_TEMP, 0, 500};
static const xmeter_value_t code xmeter_step_temp_gap = {0, XMETER_RES_TEMP, 0, 500}; /* gap between lo and high */
static const xmeter_value_t code xmeter_step_power = {0, XMETER_RES_POWER, 0, 500};


#define XMETER_PRESET_CNT 4

static xmeter_value_t xmeter_dac_preset_current[XMETER_PRESET_CNT];
static xmeter_value_t xmeter_dac_preset_voltage[XMETER_PRESET_CNT];

static uint8_t xmeter_dac_preset_current_index;
static uint8_t xmeter_dac_preset_voltage_index;

static bit xmeter_old_cc_status;

/*
  47k电阻串联，NTC 100K，B=3950K
  分压3V, ADC量程7FFF->4.096V
  中心温度在40度附近
*/
static const xmeter_grid_t code 
  xmeter_adc_temp_g[] = 
{
{-55.000 ,0X5D1F},
{-54.000 ,0X5D17},
{-53.000 ,0X5D0E},
{-52.000 ,0X5D05},
{-51.000 ,0X5CFA},
{-50.000 ,0X5CEF},
{-49.000 ,0X5CE3},
{-48.000 ,0X5CD6},
{-47.000 ,0X5CC8},
{-46.000 ,0X5CBA},
{-45.000 ,0X5CAA},
{-44.000 ,0X5C99},
{-43.000 ,0X5C87},
{-42.000 ,0X5C74},
{-41.000 ,0X5C5F},
{-40.000 ,0X5C49},
{-39.000 ,0X5C32},
{-38.000 ,0X5C19},
{-37.000 ,0X5BFF},
{-36.000 ,0X5BE3},
{-35.000 ,0X5BC5},
{-34.000 ,0X5BA6},
{-33.000 ,0X5B85},
{-32.000 ,0X5B62},
{-31.000 ,0X5B3E},
{-30.000 ,0X5B17},
{-29.000 ,0X5AEE},
{-28.000 ,0X5AC3},
{-27.000 ,0X5A96},
{-26.000 ,0X5A67},
{-25.000 ,0X5A35},
{-24.000 ,0X5A01},
{-23.000 ,0X59CA},
{-22.000 ,0X5991},
{-21.000 ,0X5955},
{-20.000 ,0X5916},
{-19.000 ,0X58D5},
{-18.000 ,0X5891},
{-17.000 ,0X5849},
{-16.000 ,0X57FF},
{-15.000 ,0X57B2},
{-14.000 ,0X5761},
{-13.000 ,0X570E},
{-12.000 ,0X56B7},
{-11.000 ,0X565D},
{-10.000 ,0X55FF},
{-9.000 ,0X559E	},
{-8.000 ,0X5539	},
{-7.000 ,0X54D1	},
{-6.000 ,0X5465	},
{-5.000 ,0X53F5	},
{-4.000 ,0X5381	},
{-3.000 ,0X530A	},
{-2.000 ,0X528F	},
{-1.000 ,0X5210	},
{0.000 ,0X5199	},
{1.000 ,0X5105	},
{2.000 ,0X507A	},
{3.000 ,0X4FEB	},
{4.000 ,0X4F58	},
{5.000 ,0X4EC1	},
{6.000 ,0X4E25	},
{7.000 ,0X4D86	},
{8.000 ,0X4CE2	},
{9.000 ,0X4C3B	},
{10.000 ,0X4B8F	},
{11.000 ,0X4AE0	},
{12.000 ,0X4A2C	},
{13.000 ,0X4975	},
{14.000 ,0X48B9	},
{15.000 ,0X47FA	},
{16.000 ,0X4737	},
{17.000 ,0X4671	},
{18.000 ,0X45A7	},
{19.000 ,0X44D9	},
{20.000 ,0X4408	},
{21.000 ,0X4334	},
{22.000 ,0X425D	},
{23.000 ,0X4182	},
{24.000 ,0X40A5	},
{25.000 ,0X3FC6	},
{26.000 ,0X3EE3	},
{27.000 ,0X3DFE	},
{28.000 ,0X3D17	},
{29.000 ,0X3C2E	},
{30.000 ,0X3B43	},
{31.000 ,0X3A57	},
{32.000 ,0X3969	},
{33.000 ,0X3879	},
{34.000 ,0X3789	},
{35.000 ,0X3697	},
{36.000 ,0X35A5	},
{37.000 ,0X34B3	},
{38.000 ,0X33C0	},
{39.000 ,0X32CD	},
{40.000 ,0X31DA	},
{41.000 ,0X30E7	},
{42.000 ,0X2FF5	},
{43.000 ,0X2F04	},
{44.000 ,0X2E14	},
{45.000 ,0X2D24	},
{46.000 ,0X2C36	},
{47.000 ,0X2B4A	},
{48.000 ,0X2A5F	},
{49.000 ,0X2976	},
{50.000 ,0X288F	},
{51.000 ,0X27A9	},
{52.000 ,0X26C7	},
{53.000 ,0X25E6	},
{54.000 ,0X2509	},
{55.000 ,0X242D	},
{56.000 ,0X2355	},
{57.000 ,0X227F	},
{58.000 ,0X21AD	},
{59.000 ,0X20DE	},
{60.000 ,0X2011	},
{61.000 ,0X1F48	},
{62.000 ,0X1E83	},
{63.000 ,0X1DC0	},
{64.000 ,0X1D01	},
{65.000 ,0X1C46	},
{66.000 ,0X1B8D	},
{67.000 ,0X1AD9	},
{68.000 ,0X1A28	},
{69.000 ,0X197A	},
{70.000 ,0X18D1	},
{71.000 ,0X182A	},
{72.000 ,0X1788	},
{73.000 ,0X16E8	},
{74.000 ,0X164D	},
{75.000 ,0X15B5	},
{76.000 ,0X1520	},
{77.000 ,0X148F	},
{78.000 ,0X1401	},
{79.000 ,0X1377	},
{80.000 ,0X12F0	},
{81.000 ,0X126C	},
{82.000 ,0X11EC	},
{83.000 ,0X116E	},
{84.000 ,0X10F5	},
{85.000 ,0X107E	},
{86.000 ,0X100A	},
{87.000 ,0X0F99	},
{88.000 ,0X0F2C	},
{89.000 ,0X0EC1	},
{90.000 ,0X0E59	},
{91.000 ,0X0DF4	},
{92.000 ,0X0D92	},
{93.000 ,0X0D32	},
{94.000 ,0X0CD5	},
{95.000 ,0X0C7B	},
{96.000 ,0X0C23	},
{97.000 ,0X0BCD	},
{98.000 ,0X0B7A	},
{99.000 ,0X0B2A	},
{100.000 ,0X0AE2},
{101.000 ,0X0A8F},
{102.000 ,0X0A45},
{103.000 ,0X09FD},
{104.000 ,0X09B7},
{105.000 ,0X0973},
{106.000 ,0X0931},
{107.000 ,0X08F1},
{108.000 ,0X08B3},
{109.000 ,0X0877},
{110.000 ,0X083C},
{111.000 ,0X0803},
{112.000 ,0X07CC},
{113.000 ,0X0796},
{114.000 ,0X0762},
{115.000 ,0X0730},
{116.000 ,0X06FE},
{117.000 ,0X06CF},
{118.000 ,0X06A0},
{119.000 ,0X0673},
{120.000 ,0X0648},
{121.000 ,0X061D},
{122.000 ,0X05F4},
{123.000 ,0X05CC},
{124.000 ,0X05A5},
{125.000 ,0X057F},
{126.000 ,0X055B},
{127.000 ,0X0537},
{128.000 ,0X0515},
{129.000 ,0X04F3},
{130.000 ,0X04D3},
{131.000 ,0X04B3},
{132.000 ,0X0494},
{133.000 ,0X0476},
{134.000 ,0X045A},
{135.000 ,0X043D},
{136.000 ,0X0422},
{137.000 ,0X0407},
{138.000 ,0X03ED},
{139.000 ,0X03D4},
{140.000 ,0X03BC},
{141.000 ,0X03A4},
{142.000 ,0X038D},
{143.000 ,0X0377},
{144.000 ,0X0361},
{145.000 ,0X034C},
{146.000 ,0X0337},
{147.000 ,0X0323},
{148.000 ,0X0310},
{149.000 ,0X02FD},
{150.000 ,0X02EB},
{151.000 ,0X02D8},
{152.000 ,0X02C7},
{153.000 ,0X02B6},
{154.000 ,0X02A6},
{155.000 ,0X0295},
{156.000 ,0X0286},
{157.000 ,0X0277},
{158.000 ,0X0268},
{159.000 ,0X025A},
{160.000 ,0X024C},
{161.000 ,0X023E},
{162.000 ,0X0231},
{163.000 ,0X0224},
{164.000 ,0X0217},
{165.000 ,0X020B},
{166.000 ,0X01FF},
{167.000 ,0X01F3},
{168.000 ,0X01E8},
{169.000 ,0X01DD},
{170.000 ,0X01D3},
{171.000 ,0X01C8},
{172.000 ,0X01BE},
{173.000 ,0X01B4},
{174.000 ,0X01AB},
{175.000 ,0X01A1},
{176.000 ,0X0198},
{177.000 ,0X018F},
{178.000 ,0X0186},
{179.000 ,0X017E},
{180.000 ,0X0175},
{181.000 ,0X016E},
{182.000 ,0X0166},
{183.000 ,0X015E},
{184.000 ,0X0157},
{185.000 ,0X014F},
{186.000 ,0X0148},
{187.000 ,0X0141},
{188.000 ,0X013A},
{189.000 ,0X0134},
{190.000 ,0X012E},
{191.000 ,0X0128},
{192.000 ,0X0121},
{193.000 ,0X011B},
{194.000 ,0X0116},
{195.000 ,0X0110},
{196.000 ,0X010A},
{197.000 ,0X0105},
{198.000 ,0X0100},
{199.000 ,0X00FB},
{200.000 ,0X00F6},
{201.000 ,0X00F1},
{202.000 ,0X00EC},
{203.000 ,0X00E7},
{204.000 ,0X00E3},
{205.000 ,0X00DE},
{206.000 ,0X00DA},
{207.000 ,0X00D6},
{208.000 ,0X00D1},
{209.000 ,0X00CD},
{210.000 ,0X00C9},
{211.000 ,0X00C6},
{212.000 ,0X00C1},
{213.000 ,0X00BE},
{214.000 ,0X00BA},
{215.000 ,0X00B6},
{216.000 ,0X00B3},
{217.000 ,0X00B0},
{218.000 ,0X00AC},
{219.000 ,0X00A9},
{220.000 ,0X00A6},
{221.000 ,0X00A3},
{222.000 ,0X00A0},
{223.000 ,0X009D},
{224.000 ,0X009A},
{225.000 ,0X0097},
{226.000 ,0X0095},
{227.000 ,0X0092},
{228.000 ,0X008F},
{229.000 ,0X008D},
{230.000 ,0X008A},
{231.000 ,0X0088},
{232.000 ,0X0086},
{233.000 ,0X0083},
{234.000 ,0X0081},
{235.000 ,0X007E},
{236.000 ,0X007C},
{237.000 ,0X007A},
{238.000 ,0X0078},
{239.000 ,0X0076},
{240.000 ,0X0074},
{241.000 ,0X0072},
{242.000 ,0X0070},
{243.000 ,0X006E},
{244.000 ,0X006C},
{245.000 ,0X006A},
{246.000 ,0X0069},
{247.000 ,0X0067},
{248.000 ,0X0065},
{249.000 ,0X0064},
{250.000 ,0X0062},
};

void xmeter_dump_value(const char * name, xmeter_value_t * pval, uint8_t cnt)
{
  uint8_t index;
  for(index = 0 ; index < cnt; index ++) {
    CDBG("value %s[%bu]: [%bu] %c%03u.%03u\n", name, index, pval[index].res,
      pval[index].neg ? '-':'+', pval[index].integer, pval[index].decimal);  
  }
}

void xmeter_cal_grid(
    uint16_t x1, uint16_t x2, 
    bit is_signed, 
    double y1, double y2, 
    xmeter_grid_t * grid, uint16_t cnt)
{
  uint32_t sx1, sx2, dx;
  double dy;
  uint16_t i;
  
  sx1 = is_signed ? (int16_t)x1 : x1;
  sx2 = is_signed ? (int16_t)x2 : x2; 
  
  dx = (sx2 - sx1) / (cnt - 1);
  dy = (y2 - y1) / (cnt - 1);
  
  for(i = 0 ; i < cnt - 1; i ++) {
    grid[i].val = y1 + dy * i;
    grid[i].bits = (uint16_t)(sx1 + dx * i);
  }
  
  grid[i].val = y2;
  grid[i].bits = (uint16_t)(sx2);
}

static void xmeter_dump_grid(const char * name, const xmeter_grid_t * grid, uint16_t cnt)
{
  uint16_t i;
  CDBG("%s:\n", name);
  for(i = 0 ; i < cnt; i ++) {
    CDBG("%04x -> %f:\n", grid[i].bits, grid[i].val);
  }
}

void xmeter_write_rom_adc_current_g(const xmeter_grid_t * grid, uint16_t cnt)
{
  rom_write_struct(ROM_XMETER_ADC_CURRENT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_write_rom_adc_voltage_out_g(const xmeter_grid_t * grid, uint16_t cnt)
{
  rom_write_struct(ROM_XMETER_ADC_VOLTAGE_OUT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_write_rom_adc_voltage_diss_g(const xmeter_grid_t * grid, uint16_t cnt)
{
  rom_write_struct(ROM_XMETER_ADC_VOLTAGE_DISS_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_write_rom_dac_current_g(const xmeter_grid_t * grid, uint16_t cnt)
{
  rom_write_struct(ROM_XMETER_DAC_CURRENT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_write_rom_dac_voltage_g(const xmeter_grid_t * grid, uint16_t cnt)
{
  rom_write_struct(ROM_XMETER_DAC_VOLTAGE_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_read_rom_adc_current_g(xmeter_grid_t * grid, uint16_t cnt)
{
  rom_read_struct(ROM_XMETER_ADC_CURRENT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_read_rom_adc_voltage_out_g(xmeter_grid_t * grid, uint16_t cnt)
{
  rom_read_struct(ROM_XMETER_ADC_VOLTAGE_OUT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_read_rom_adc_voltage_diss_g(xmeter_grid_t * grid, uint16_t cnt)
{
  rom_read_struct(ROM_XMETER_ADC_VOLTAGE_DISS_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_read_rom_dac_current_g(xmeter_grid_t * grid, uint16_t cnt)
{
  rom_read_struct(ROM_XMETER_DAC_CURRENT_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_read_rom_dac_voltage_g(xmeter_grid_t * grid, uint16_t cnt)
{
  rom_read_struct(ROM_XMETER_DAC_VOLTAGE_G, grid, cnt * sizeof(xmeter_grid_t));
}

void xmeter_reset_adc_current_config(void)
{
  /* xmeter_adc_current, fsr is 0~5.0
     0000 -> 0.0
     7fff -> 10.24 (ADC输入电流)
  */
  xmeter_cal_grid(0x0, 0x3fff, 1, 0.0, 5.0, xmeter_adc_current_g, XMETER_GRID_SIZE);
  xmeter_write_rom_adc_current_g(xmeter_adc_current_g, XMETER_GRID_SIZE);
}

void xmeter_reset_adc_voltage_out_config(void)
{
  /* xmeter_adc_voltage_out, fsr is 0~30
     0000 -> 0.0
     7fff -> 30 (ADC输入电压)
  */  
  xmeter_cal_grid(0x0, 0x7fff, 1, 0.0, 30, xmeter_adc_voltage_out_g, XMETER_GRID_SIZE); 
  xmeter_write_rom_adc_voltage_out_g(xmeter_adc_voltage_out_g, XMETER_GRID_SIZE);
}

void xmeter_reset_adc_voltage_diss_config(void)
{
  /* xmeter_adc_voltage_in, fsr is 0~40
     0000 -> 0.0
     7fff -> 40 (ADC耗散电压)
  */  
  xmeter_cal_grid(0x0, 0x7fff, 1, 0.0, 40.0, xmeter_adc_voltage_diss_g, XMETER_GRID_SIZE);
  xmeter_write_rom_adc_voltage_diss_g(xmeter_adc_voltage_diss_g, XMETER_GRID_SIZE);

}

void xmeter_reset_dac_current_config(void)
{
  /* xmeter_dac_current, fsr is 0~5.0
     0000 -> 0.0
     ffff -> 5.0 (DAC输出电流)
  */    
  xmeter_cal_grid(0x0, 0xffff, 0, 0.0, 5.0, xmeter_dac_current_g, XMETER_GRID_SIZE);
  xmeter_write_rom_dac_current_g(xmeter_dac_current_g, XMETER_GRID_SIZE);
}

void xmeter_reset_dac_voltage_config(void)
{
  /* xmeter_dac_voltage, fsr is 0~30.0
     0000 -> 0.0
     ffff -> 30 (DAC输出电压)
  */    
  xmeter_cal_grid(0x0, 0xffff, 0, 0.0, 30, xmeter_dac_voltage_g, XMETER_GRID_SIZE);
  xmeter_write_rom_dac_voltage_g(xmeter_dac_voltage_g, XMETER_GRID_SIZE);

}

void xmeter_rom_factory_reset(void)
{
  
  xmeter_value_t val[4];
  
  lcd_show_progress(1, 0);
  
  /*
  电路设计 Note:
    输入部分保障ADC最大测量30/45V、5A
    输出部分保障DAC最大输出30V、5A
  */
  xmeter_reset_adc_current_config();
  
  lcd_show_progress(1, 1);
  
  xmeter_reset_adc_voltage_out_config();
  
  lcd_show_progress(1, 2);
  
  xmeter_reset_adc_voltage_diss_config();
  
  lcd_show_progress(1, 3);
  
  xmeter_reset_dac_current_config();
  
  lcd_show_progress(1, 4);
  
  xmeter_reset_dac_voltage_config();
  
  lcd_show_progress(1, 5);
  
  /* preset current 100mA */
  val[0].neg = 0;
  val[0].res = XMETER_RES_CURRENT;  
  val[0].integer = 0;
  val[0].decimal = 100;

  /* 1 A */
  val[1].neg = 0;
  val[1].res = XMETER_RES_CURRENT;  
  val[1].integer = 1;
  val[1].decimal = 0;
  
  /* 1.5 A */
  val[2].neg = 0;
  val[2].res = XMETER_RES_CURRENT;  
  val[2].integer = 1;
  val[2].decimal = 500;

  /* 2.0 A */
  val[3].neg = 0;
  val[3].res = XMETER_RES_CURRENT;  
  val[3].integer = 2;
  val[3].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_PRESET_CURRENT, val, sizeof(val));
  
  lcd_show_progress(1, 6);
  
  /* preset voltage 3.3V */
  val[0].neg = 0;
  val[0].res = XMETER_RES_VOLTAGE;  
  val[0].integer = 3;
  val[0].decimal = 300;

  /* 5.0V */
  val[1].neg = 0;
  val[1].res = XMETER_RES_VOLTAGE;  
  val[1].integer = 5;
  val[1].decimal = 0;
  
  /* 9.0 V */
  val[2].neg = 0;
  val[2].res = XMETER_RES_VOLTAGE;  
  val[2].integer = 9;
  val[2].decimal = 0;

  /* 12.0 V */
  val[3].neg = 0;
  val[3].res = XMETER_RES_VOLTAGE;  
  val[3].integer = 12;
  val[3].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_PRESET_VOLTAGE, val, sizeof(val));
  
  lcd_show_progress(1, 8);
  
  /* last dac current 1A and voltage 5V*/
  val[0].neg = 0;
  val[0].res = XMETER_RES_CURRENT;  
  val[0].integer = 1;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_LAST_CURRENT, val, sizeof(xmeter_value_t));
  
  lcd_show_progress(1, 11);
  
  val[0].neg = 0;
  val[0].res = XMETER_RES_VOLTAGE;  
  val[0].integer = 5;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_LAST_VOLTAGE, val, sizeof(xmeter_value_t));  

  lcd_show_progress(1, 12);

  /* temp high is 45 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 45;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_HI, val, sizeof(xmeter_value_t));  
  
  lcd_show_progress(1, 13);
  
  /* temp low is 40 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 40;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_LO, val, sizeof(xmeter_value_t));  

  lcd_show_progress(1, 14);

  /* temp overheat is 80 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 80;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_OVERHEAT, val, sizeof(xmeter_value_t));  
  
  lcd_show_progress(1, 15);
  
  /* max diss power is 120 W */
  val[0].neg = 0;
  val[0].res = XMETER_RES_POWER;  
  val[0].integer = 120;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_MAX_POWER_DISS, val, sizeof(xmeter_value_t)); 

  lcd_show_progress(1, 16);
}

void xmeter_reload_adc_current_config(void)
{
  xmeter_read_rom_adc_current_g(xmeter_adc_current_g, XMETER_GRID_SIZE);
  xmeter_dump_grid("ADC_c", xmeter_adc_current_g, XMETER_GRID_SIZE);
}

void xmeter_reload_adc_voltage_out_config(void)
{
  xmeter_read_rom_adc_voltage_out_g(xmeter_adc_voltage_out_g, XMETER_GRID_SIZE);
  xmeter_dump_grid("ADC_vo", xmeter_adc_voltage_out_g, XMETER_GRID_SIZE);
}

void xmeter_reload_adc_voltage_diss_config(void)
{
  xmeter_read_rom_adc_voltage_diss_g(xmeter_adc_voltage_diss_g, XMETER_GRID_SIZE);
  xmeter_dump_grid("ADC_vd", xmeter_adc_voltage_diss_g, XMETER_GRID_SIZE);
}

void xmeter_reload_dac_current_config(void)
{
  xmeter_read_rom_dac_current_g(xmeter_dac_current_g, XMETER_GRID_SIZE);
  xmeter_dump_grid("DAC_c", xmeter_dac_current_g, XMETER_GRID_SIZE);
}

void xmeter_reload_dac_voltage_config(void)
{
  xmeter_read_rom_dac_voltage_g(xmeter_dac_voltage_g, XMETER_GRID_SIZE);
  xmeter_dump_grid("DAC_v", xmeter_dac_voltage_g, XMETER_GRID_SIZE);
}

void xmeter_load_config(void)
{
  lcd_show_progress(1, 3);
  
  xmeter_reload_adc_current_config();
  
  lcd_show_progress(1, 4);
  
  xmeter_reload_adc_voltage_out_config();
  
  lcd_show_progress(1, 5);
  
  xmeter_reload_adc_voltage_diss_config();
  
  lcd_show_progress(1, 6);
  
  xmeter_reload_dac_current_config();
  
  lcd_show_progress(1, 7);
  
  xmeter_reload_dac_voltage_config();
  
  lcd_show_progress(1, 8);
  
  rom_read_struct(ROM_XMETER_DAC_PRESET_CURRENT, xmeter_dac_preset_current, sizeof(xmeter_dac_preset_current));
  xmeter_dump_value("DAC_Preset_C", xmeter_dac_preset_current, 4); 

  lcd_show_progress(1, 9);

  rom_read_struct(ROM_XMETER_DAC_PRESET_VOLTAGE, xmeter_dac_preset_voltage, sizeof(xmeter_dac_preset_voltage));
  xmeter_dump_value("DAC_Preset_V", xmeter_dac_preset_voltage, 4); 
  
  xmeter_dac_preset_current_index = 0;
  xmeter_dac_preset_voltage_index = 0;
  
  lcd_show_progress(1, 10);
  
  rom_read_struct(ROM_XMETER_DAC_LAST_CURRENT, &xmeter_dac_current, sizeof(xmeter_dac_current));
  xmeter_dump_value("DAC_Last_C", &xmeter_dac_current, 1);   

  lcd_show_progress(1, 11);

  rom_read_struct(ROM_XMETER_DAC_LAST_VOLTAGE, &xmeter_dac_voltage, sizeof(xmeter_dac_voltage));
  xmeter_dump_value("DAC_Last_V", &xmeter_dac_voltage, 1);
  
  lcd_show_progress(1, 12);
  
  rom_read_struct(ROM_XMETER_TEMP_HI, &xmeter_temp_hi, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Hi", &xmeter_temp_hi, 1);
  
  lcd_show_progress(1, 13);
  
  rom_read_struct(ROM_XMETER_TEMP_LO, &xmeter_temp_lo, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Lo", &xmeter_temp_lo, 1);  
  
  lcd_show_progress(1, 14);
  
  rom_read_struct(ROM_XMETER_TEMP_OVERHEAT, &xmeter_temp_overheat, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Overheat", &xmeter_temp_overheat, 1);  

  lcd_show_progress(1, 15);

  rom_read_struct(ROM_XMETER_MAX_POWER_DISS, &xmeter_max_power_diss, sizeof(xmeter_value_t));  
  xmeter_dump_value("Max_Diss_Power", &xmeter_max_power_diss, 1); 

  lcd_show_progress(1, 16);
}  
  
void xmeter_init_adc(void)
{
  uint16_t config = 0;
  CDBG("xmeter_init_adc\n");
  
  I2C_Init();
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  CDBG("xmeter adc config reg before %04x \n", config); 
  
  config &= ~(0x1 << 15); /* remove OS bit */
  
  // set chanel0
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_CURRENT;
  
  // PGA 001: set to 4.096V
  /*
    000 : FSR = ±6.144 V(1)
    001 : FSR = ±4.096 V(1)
    010 : FSR = ±2.048 V (default)
    011 : FSR = ±1.024 V
    100 : FSR = ±0.512 V
    101 : FSR = ±0.256 V
    110 : FSR = ±0.256 V
    111 : FSR = ±0.256 V
  */
  config &= ~(0x7 << 9);
  config |= (0x1 << 9);
  
  // MODE 1: Device operating mode set to single short
  
  // DR 100: Data rate to 128 SPS
  
  // COMP_MODE 0: Traditional comparator
  
  // COMP_POL 0: ALERT/RDY pin active low
  
  // COMP_LAT 0: Nonlatching comparator
  
  // COMP_QUE 00: Assert after one conversion
  config &= ~(0x3);
  CDBG("xmeter adc config reg after %04x \n", config); 
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  /*
  The ALERT/RDY pin can also be configured as a conversion ready pin. 
  Set the most-significant bit of the Hi_thresh register to 1 and 
  the most-significant bit of Lo_thresh register to 0 to enable the 
  pin as a conversion ready pin.  
  */
  config = 0x0; // 8000h after reset
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_LOTH, 2, (uint8_t*)&config);
  
  config = 0xFFFF; // 0x7fff after reset
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_HITH, 2, (uint8_t*)&config);    
}

static bit xmeter_dac_write_and_wait_ack(uint8_t val)
{
  I2C_Write(val);
  if(I2C_GetAck()){
    CDBG("xmeter_dac_write_and_wait_ack failed!\n");
    I2C_Stop();
    return 0;
  }
  return 1;
}

static bit xmeter_dac_read_and_ack(uint8_t * val, bit last)
{
  *val = I2C_Read();
  I2C_PutAck(last);
  return 1;
}

void xmeter_init_dac(void)
{
  
  uint8_t control = 0;
  CDBG("xmeter_init_dac\n");
  /* Switch DAC8571 to fast settling mode */
  
  I2C_Init();
  
  I2C_Start();
  
  do {  
    /* write address  */
    if(!xmeter_dac_write_and_wait_ack(XMETER_DAC_I2C_VOLTAGE))
      break;
    /* write control code */
    control = 0x11;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;
    /* write MSB*/
    control = 0x20;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;
    /* write LSB*/
    control = 0x00;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;  
    I2C_Stop();
    
    I2C_Start();
    /* write address  */
    if(!xmeter_dac_write_and_wait_ack(XMETER_DAC_I2C_CURRENT))
      break;
    /* write control code */
    control = 0x11;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;
    /* write MSB*/
    control = 0x20;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;
    /* write LSB*/
    control = 0x00;
    if(!xmeter_dac_write_and_wait_ack(control))
      break;  
  }while(0);
  
  I2C_Stop();
}

void xmeter_initialize(void)
{
  
  xmeter_output_off();

  xmeter_init_adc();
  
  xmeter_init_dac();
  
  // load config from rom
  xmeter_load_config();
  
  xmeter_fan_off();
  
  XMETER_CONV_RDY = 1;
  
  XMETER_CC = 1;
  
  xmeter_old_cc_status = XMETER_CC;
  
  xmeter_write_dac_voltage();
  
  xmeter_write_dac_current();
}

static bit xmeter_wait_for_conv_ready(void)
{
  uint8_t wait_cnt = XMETER_ADC_MAX_WAIT_CNT;
  
  while(XMETER_CONV_RDY) {
    delay_10us(XMETER_CONV_RDY_WAIT10US); /* 100us */
    wait_cnt --;
    if(!wait_cnt)
      break;
  }
  
  return wait_cnt != 0;
}
/* from x3 -> y3 */
static double xmeter_interpolate(int32_t x1, int32_t x2, double y1, double y2, int32_t x3)
{
  double dy;
  int32_t dx;
  
  if(x2 == x1)
    return 0.0;
  
  dy = y2 - y1;
  dx = x2 - x1;
  
  // k = (y2 - y1) / (x2 - x1);
  // b = y2 - k * x2;
  // return k * x3 + b;
  return dy * x3 / dx + y1 - dy * x1 / dx;
}
/* from y3 -> x3 */
static int32_t xmeter_interpolate_r(int32_t x1, int32_t x2, double y1, double y2, double y3)
{
  double dy;
  int32_t dx;
  
  dy = y2 - y1;
  dx = x2 - x1;
  
  if(y2 == y1)
    return 0;
  
  //k = (y2 - y1) / (x2 - x1);
  
  //if(k == 0.0)
  //  return 0;
  
  //b = y2 - k * x2;
  
  //return (int32_t)((y3 - b)/k);
  return (int32_t)((y3 - y1) * dx / dy + x1);
}

static uint16_t xmeter_float2bits(double f, xmeter_grid_t * grid, uint16_t cnt, uint8_t is_signed)
{
  int32_t x1, x2;
  double y1, y2;
  uint16_t i;
  
  if(cnt < 2)
    return 0;
  
  if(f < grid[0].val) {
    x1 = is_signed ? (int32_t)(int16_t)grid[0].bits : grid[0].bits;
    x2 = is_signed ? (int32_t)(int16_t)grid[1].bits : grid[1].bits;
    y1 = grid[0].val;
    y2 = grid[1].val;
  } else if(f >= grid[cnt - 1].val) {
    x1 = is_signed ? (int32_t)(int16_t)grid[cnt - 2].bits : grid[cnt - 2].bits;
    x2 = is_signed ? (int32_t)(int16_t)grid[cnt - 1].bits : grid[cnt - 1].bits;
    y1 = grid[cnt - 2].val;
    y2 = grid[cnt - 1].val;
  } else {
    for(i = 0 ; i <  cnt - 1; i ++) {
      if(f >= grid[i].val && f < grid[i + 1].val) {
        break;
      }
    }
    x1 = is_signed ? (int32_t)(int16_t)grid[i].bits : grid[i].bits;
    x2 = is_signed ? (int32_t)(int16_t)grid[i+1].bits : grid[i+1].bits;
    y1 = grid[i].val;
    y2 = grid[i+1].val;  
  }
  return (uint16_t)xmeter_interpolate_r(x1, x2, y1, y2, f);
}

static double xmeter_bits2float(uint16_t bits, xmeter_grid_t * grid, uint16_t cnt, uint8_t is_signed)
{
  int32_t x1, x2, x3, xleft, xright;
  double y1, y2;
  uint16_t i;
  
  if(cnt < 2)
    return 0;
  
  x3 = is_signed ? (int32_t)(int16_t)bits : bits;
  
  xleft = is_signed ? (int32_t)(int16_t)grid[0].bits : grid[0].bits;
  xright = is_signed ? (int32_t)(int16_t)grid[cnt - 1].bits : grid[cnt - 1].bits;
  
  if((xleft < xright && x3 < xleft) || (xleft >= xright && x3 > xleft)) {
    x1 = is_signed ? (int32_t)(int16_t)grid[0].bits : grid[0].bits;
    x2 = is_signed ? (int32_t)(int16_t)grid[1].bits : grid[1].bits;
    y1 = grid[0].val;
    y2 = grid[1].val;
  } else if((xleft < xright && x3 >= xright) || (xleft >= xright && x3 <= xright)) {
    x1 = is_signed ? (int32_t)(int16_t)grid[cnt - 2].bits : grid[cnt - 2].bits;
    x2 = is_signed ? (int32_t)(int16_t)grid[cnt - 1].bits : grid[cnt - 1].bits;
    y1 = grid[cnt - 2].val;
    y2 = grid[cnt - 1].val;
  }else {
    for(i = 0 ; i <  cnt - 1; i ++) {
      x1 = is_signed ? (int32_t)(int16_t)grid[i].bits : grid[i].bits;
      x2 = is_signed ? (int32_t)(int16_t)grid[i+1].bits : grid[i+1].bits;
      if(xleft < xright && x3 >= x1 && x3 < x2) {
        break;
      }
      if(xleft >= xright && x3 <= x1 && x3 > x2) {
        break;
      }
    }
    y1 = grid[i].val;
    y2 = grid[i+1].val;  
  }
   
  return xmeter_interpolate(x1, x2, y1, y2, x3);
}


/*
is_signed: interpret return as a signed integer
*/

static uint16_t xmeter_value2bits(
  xmeter_value_t * val,
  double * f,
  xmeter_grid_t * grid, 
  uint16_t cnt, 
  uint8_t is_signed)
{
  *f = xmeter_val2float(val);
  return xmeter_float2bits(*f, grid, cnt, is_signed);

}

/*
is_signed: interpret bits as a signed integer
*/

static double xmeter_bits2value(
  uint16_t bits, 
  xmeter_value_t * val,
  xmeter_grid_t * grid, 
  uint16_t cnt,
  uint8_t res, 
  uint8_t is_signed)
{
  double ret;
  ret = xmeter_bits2float(bits, grid, cnt, is_signed);
  xmeter_float2val(ret, val, res);
  return ret;
}

uint16_t xmeter_get_adc_bits_current(void)
{
  uint16_t config = 0;
  uint16_t val = 0;
  
  I2C_Init();
  
  // set channel0 and trigger cont, full scale set to 4.096
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_CURRENT;
  config |= 0x8000; /* OS = 1*/
  
  config &= ~(0x7 << 9); 
  config |= (0x1 << 9); 
  
  //CDBG(("xmeter_update_current write %04x\n", config));
  
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  if(!xmeter_wait_for_conv_ready()) {
    CDBG(("xmeter adc get current timeout\n"));
    return 0;
  }  
  
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONV, 2, (uint8_t*)&val);
  
  return val;
  

}

void xmeter_read_adc_current(void)
{
  uint16_t val;
  
  val = xmeter_get_adc_bits_current();
  
  xmeter_adc_current_f = xmeter_bits2value(
    val, 
    &xmeter_adc_current, 
    xmeter_adc_current_g, 
    XMETER_GRID_SIZE, 
    XMETER_RES_CURRENT, 1);
  
  // xmeter_dump_value("ADC_c", &xmeter_adc_current, 1);
}

uint16_t xmeter_get_adc_bits_voltage_out(void)
{
  uint16_t config = 0;
  uint16_t val = 0;
  
  I2C_Init();

  // set channel1 and trigger cont, full scale set to 4.096
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_VOLTAGE_OUT;
  config |= 0x8000; /* OS = 1*/
  
  config &= ~(0x7 << 9); 
  config |= (0x1 << 9);
  
  //CDBG(("xmeter_update_voltage_out write %04x\n", config));
  
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  if(!xmeter_wait_for_conv_ready()) {
    CDBG("xmeter adc get voltage out timeout\n");
    return 0;
  }
  
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONV, 2, (uint8_t*)&val);
  
  return val;
}

void xmeter_read_adc_voltage_out(void)
{
  uint16_t val;
  
  val = xmeter_get_adc_bits_voltage_out();
  
  xmeter_adc_voltage_out_f = xmeter_bits2value(
    val, 
    &xmeter_adc_voltage_out, 
    xmeter_adc_voltage_out_g, 
    XMETER_GRID_SIZE, 
    XMETER_RES_VOLTAGE, 1); 
  
  //xmeter_dump_value("ADC_vo", &xmeter_adc_voltage_out, 1);
}

uint16_t xmeter_get_adc_bits_voltage_diss(void)
{
  uint16_t config = 0;
  uint16_t val = 0;
  
  I2C_Init();
  
  // set channel2 and trigger cont, full scale set to 4.096
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_VOLTAGE_DISS;
  config |= 0x8000; /* OS = 1*/
  
  config &= ~(0x7 << 9); 
  config &= ~(0x1 << 9);   
  
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  if(!xmeter_wait_for_conv_ready()) {
    CDBG("xmeter adc get voltage in timeout\n");
    return 0;
  }
  
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONV, 2, (uint8_t*)&val);
  
  return val;
}

void xmeter_read_adc_voltage_diss(void)
{
  
  uint16_t val;
  
  val = xmeter_get_adc_bits_voltage_diss();
  
  xmeter_adc_voltage_diss_f = xmeter_bits2value(
    val, 
    &xmeter_adc_voltage_diss, 
    xmeter_adc_voltage_diss_g, 
    XMETER_GRID_SIZE, 
    XMETER_RES_VOLTAGE, 1);   
  
  //xmeter_dump_value("ADC_vd", &xmeter_adc_voltage_diss, 1);
}

uint16_t xmeter_get_adc_bits_temp(void)
{
  uint16_t config = 0;
  uint16_t val = 0;
  
  I2C_Init();  
  
  // set channel3 and trigger cont, full scale set to 4.096
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_TEMP;
  config |= 0x8000; /* OS = 1*/
  
  config &= ~(0x7 << 9); 
  config |= (0x1 << 9);   
  
  //CDBG(("xmeter_update_voltage_temp write %04x\n", config));
  
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  if(!xmeter_wait_for_conv_ready()) {
    CDBG("xmeter adc get temp timeout\n");
    return 0;
  }
  
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONV, 2, (uint8_t*)&val);
  
  return val;
}

void xmeter_read_adc_temp(void)
{
  uint16_t val;
  
  val = xmeter_get_adc_bits_temp();
  
  xmeter_adc_temp_f = xmeter_bits2float(val, 
    xmeter_adc_temp_g, 
    sizeof(xmeter_adc_temp_g) / sizeof(xmeter_grid_t), 1);
  
  xmeter_float2val(xmeter_adc_temp_f, &xmeter_adc_temp, XMETER_RES_TEMP);

  //xmeter_dump_value("ADC_t", &xmeter_adc_temp, 1);
}


/*
根据输出电压输出电流
计算输出功率，只用于LCD实时显示！
*/

void xmeter_calculate_power_out(
  const xmeter_value_t * current, 
  const xmeter_value_t * voltage,
  xmeter_value_t * power_out)
{
  double current_f, voltage_f, power_f;
  current_f = xmeter_val2float(current);
  voltage_f = xmeter_val2float(voltage);
  power_f   = current_f * voltage_f;
  xmeter_float2val(power_f, power_out, XMETER_RES_POWER);
}


static int8_t xmeter_compare_value(const xmeter_value_t * val0, const xmeter_value_t * val1)
{
  int8_t ret = 0;
  if(val0->neg > val1->neg) {
    return -1;
  } else if(val0->neg < val1->neg) {
    return 1;
  } else {
    if(val0->integer > val1->integer)
      ret = 1;
    else if(val0->integer < val1->integer) {
      ret = -1;
    } else {
      if(val0->decimal > val1->decimal) {
        ret = 1;
      } else if (val0->decimal < val1->decimal){
        ret = -1;
      } else {
        ret = 0;
      }
    }
    if(val0->neg) {
      if(ret != 0) {
        ret = ret > 0 ? -1 : 1;
      }
    }
  }
  return ret;
}

bit xmeter_temp_safe(void)
{
  return xmeter_compare_value(&xmeter_adc_temp, &xmeter_temp_lo) <= 0;
}

void xmeter_read_adc(void)
{
  xmeter_read_adc_current();
  xmeter_read_adc_voltage_out();
  xmeter_read_adc_voltage_diss();
  xmeter_read_adc_temp();
  xmeter_power_out_f = xmeter_adc_current_f * xmeter_adc_voltage_out_f;
  xmeter_float2val(xmeter_power_out_f, &xmeter_power_out, XMETER_RES_POWER);
  //CDBG(("adc po float %f x %f = %f\n", xmeter_adc_current_f, xmeter_adc_voltage_out_f, xmeter_power_out_f));
  //xmeter_dump_value("adc po", &xmeter_power_out, 1);
    
  xmeter_power_diss_f = xmeter_adc_current_f * xmeter_adc_voltage_diss_f;
  xmeter_float2val(xmeter_power_diss_f, &xmeter_power_diss, XMETER_RES_POWER); 
  //CDBG(("adc pd float %f x %f = %f\n", xmeter_power_diss_f, xmeter_power_diss_f, xmeter_power_diss_f));
  //xmeter_dump_value("adc pd", &xmeter_power_diss, 1);
    
  /* see if temp is hi or low */
  if(xmeter_fan_status()) {
    if(xmeter_compare_value(&xmeter_adc_temp, &xmeter_temp_lo) <= 0) {
      task_set(EV_TEMP_LO);
    }
  } else {
    if(xmeter_compare_value(&xmeter_adc_temp, &xmeter_temp_hi) >= 0) {
      task_set(EV_TEMP_HI);
    }
  }
    
  if(xmeter_compare_value(&xmeter_adc_temp, &xmeter_temp_overheat) >= 0) {
    task_set(EV_OVER_HEAT);
  }
    
  if(xmeter_output_status()) {
    if(xmeter_compare_value(&xmeter_power_diss, &xmeter_max_power_diss) >= 0) {
      xmeter_dump_value("adc pd", &xmeter_power_diss, 1);
      xmeter_dump_value("max adc pd", &xmeter_max_power_diss, 1);      
      task_set(EV_OVER_PD);
    }
  }
  
  if(xmeter_cc_status() != xmeter_old_cc_status) {
    task_set(EV_CC_CHANGE);
    xmeter_old_cc_status = xmeter_cc_status();
  }
}

bit xmeter_output_status(void)
{
  return XMETER_OUTPUT_SWITCH == 0;
}

bit xmeter_cc_status(void)
{
  return XMETER_CC == 0;
}

bit xmeter_output_on(void)
{
  XMETER_OUTPUT_SWITCH = 0;
  return xmeter_output_status();
}
bit xmeter_output_off(void)
{
  XMETER_OUTPUT_SWITCH = 1;
  return xmeter_output_status();
}

bit xmeter_fan_on(void)
{
  XMETER_FAN_SWITCH = 0;
  
  return xmeter_fan_status();
}

bit xmeter_fan_off(void)
{
  XMETER_FAN_SWITCH = 1;
  
  return xmeter_fan_status();
}

bit xmeter_fan_status(void)
{
  return XMETER_FAN_SWITCH == 0;
}


void xmeter_assign_value(const xmeter_value_t * src, xmeter_value_t * dst)
{
  memcpy(dst, src, sizeof(xmeter_value_t));
}

/*
  加减数值，必须相同精度
*/
static void xmeter_add_sub_value(xmeter_value_t * val, const xmeter_value_t * x, bit is_add)
{
  int32_t v1,v2;
  
  if(val->res != x->res) {
    CDBG("[ERROR!] differ res\n");
    return;
  }
  
  v1 = val->integer;
  v1 = v1 * 1000 + val->decimal; /* max 999999 */
  if(val->neg)
    v1 = 0 - v1;
  v2 = x->integer;
  v2 = v2 * 1000 + x->decimal; /* max 999999 */
  if(x->neg)
    v2 = 0 - v2;
  
  if(is_add)
    v1 += v2;
  else
    v1 -= v2;
  
  val->neg = v1 < 0 ? 1 : 0;
  
  if(v1 < 0)
    v1 = 0 - v1;
  
  val->integer = v1 / 1000;
  val->decimal = v1 % 1000;
}

void xmeter_inc_voltage_value(xmeter_value_t * voltage, bit coarse)
{
  xmeter_add_sub_value(voltage, coarse ? &xmeter_step_coarse_voltage : &xmeter_step_fine_voltage, 1);
}

void xmeter_dec_voltage_value(xmeter_value_t * voltage, bit coarse)
{
  xmeter_add_sub_value(voltage, coarse ? &xmeter_step_coarse_voltage : &xmeter_step_fine_voltage, 0);
}
void xmeter_inc_current_value(xmeter_value_t * current, bit coarse)
{
  xmeter_add_sub_value(current, coarse ? &xmeter_step_coarse_current : &xmeter_step_fine_current, 1);
}

void xmeter_dec_current_value(xmeter_value_t * current, bit coarse)
{
  xmeter_add_sub_value(current, coarse ? &xmeter_step_coarse_current : &xmeter_step_fine_current, 0);
}


void xmeter_inc_value(xmeter_value_t * val, const xmeter_value_t * step)
{
  xmeter_add_sub_value(val, step, 1);
}

void xmeter_dec_value(xmeter_value_t * val, const xmeter_value_t * step)
{
  xmeter_add_sub_value(val, step, 0);
}

void xmeter_inc_dac_v(bit coarse)
{
  xmeter_inc_voltage_value(&xmeter_dac_voltage, coarse);
  if(xmeter_compare_value(&xmeter_dac_voltage, &xmeter_dac_max_voltage) > 0) {
    xmeter_assign_value(&xmeter_dac_max_voltage, &xmeter_dac_voltage);
  }
}

void xmeter_dec_dac_v(bit coarse)
{
  xmeter_dec_voltage_value(&xmeter_dac_voltage, coarse);
  if(xmeter_compare_value(&xmeter_dac_voltage, &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &xmeter_dac_voltage);
  }  
}

/*
  向0取整（正数变小，负数变大）
*/
static void xmeter_round_value(xmeter_value_t * val, const xmeter_value_t * step)
{
  uint32_t temp = val->integer;
  uint32_t step_temp = step->integer;
  uint32_t n;
  
  temp = temp * 1000 + val->decimal;
  step_temp = step_temp * 1000 + step->decimal;
  
  if(step_temp == 0)
    return;
  
  n = temp / step_temp;
  temp = step_temp * n;
  
  val->integer = temp / 1000;
  val->decimal = temp % 1000;
  
  if(val->integer == val->decimal && val->decimal == 0) {
    val->neg = 0;
  }
  return;
}

void xmeter_set_dac_v(const xmeter_value_t * val)
{
  xmeter_value_t temp;
  
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_fine_voltage);
  if(xmeter_compare_value(&temp, &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &temp);
  }else if(xmeter_compare_value(&temp, &xmeter_dac_max_voltage) > 0) {
    xmeter_assign_value(&xmeter_dac_max_voltage, &temp);
  }  
  xmeter_assign_value(&temp, &xmeter_dac_voltage);
}

void xmeter_set_dac_c(const xmeter_value_t * val)
{
  xmeter_value_t temp;
  
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_fine_current);
  if(xmeter_compare_value(&temp, &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &temp);
  }else if(xmeter_compare_value(&temp, &xmeter_dac_max_current) > 0) {
    xmeter_assign_value(&xmeter_dac_max_current, &temp);
  }  
  xmeter_assign_value(&temp, &xmeter_dac_current);
}

void xmeter_inc_dac_c(bit coarse)
{
  xmeter_inc_value(&xmeter_dac_current, coarse ? &xmeter_step_coarse_current : &xmeter_step_fine_current );
  if(xmeter_compare_value(&xmeter_dac_current, &xmeter_dac_max_current) > 0) {
    xmeter_assign_value(&xmeter_dac_max_current, &xmeter_dac_current);
  }  
}

void xmeter_dec_dac_c(bit coarse)
{
  xmeter_dec_value(&xmeter_dac_current, coarse ? &xmeter_step_coarse_current : &xmeter_step_fine_current );
  if(xmeter_compare_value(&xmeter_dac_current, &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &xmeter_dac_current);
  }    
}

void xmeter_next_preset_dac_v(void)
{
  
  xmeter_dac_preset_voltage_index 
    = (xmeter_dac_preset_voltage_index + 1) % XMETER_PRESET_CNT;
  
 // CDBG("xmeter_next_preset_dac_v %bu\n", xmeter_dac_preset_voltage_index);
 // xmeter_dump_value("preset v", &xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], 1);
  
  xmeter_assign_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index],
    &xmeter_dac_voltage);
}

void xmeter_prev_preset_dac_v(void)
{
  if(xmeter_dac_preset_voltage_index == 0) {
    xmeter_dac_preset_voltage_index = XMETER_PRESET_CNT - 1;
  } else {
    xmeter_dac_preset_voltage_index 
      = (xmeter_dac_preset_voltage_index - 1);
  }
  
 // CDBG("xmeter_prev_preset_dac_v %bu\n", xmeter_dac_preset_voltage_index);
 // xmeter_dump_value("preset v", &xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], 1);
  
  xmeter_assign_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index],
    &xmeter_dac_voltage);
}

void xmeter_next_preset_dac_c(void)
{
  xmeter_dac_preset_current_index 
    = (xmeter_dac_preset_current_index + 1) % XMETER_PRESET_CNT;
  
  xmeter_assign_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index],
    &xmeter_dac_current);
}

void xmeter_prev_preset_dac_c(void)
{
  if(xmeter_dac_preset_current_index == 0) {
    xmeter_dac_preset_current_index = XMETER_PRESET_CNT - 1;
  } else {
    xmeter_dac_preset_current_index 
      = (xmeter_dac_preset_current_index - 1);
  }
  
  xmeter_assign_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index],
    &xmeter_dac_current);
}

/* load xmeter_dac_ from preset slot */
void xmeter_load_preset_dac_v(void)
{
  xmeter_load_preset_dac_v_by_index(xmeter_dac_preset_voltage_index);
}

void xmeter_load_preset_dac_c(void)
{
  xmeter_load_preset_dac_c_by_index(xmeter_dac_preset_current_index);
}

void xmeter_load_preset_dac_v_by_index(uint8_t index)
{
  if(index > xmeter_get_max_preset_index_v()) {
    index = xmeter_get_max_preset_index_v();
  }
  
  xmeter_assign_value(
    &xmeter_dac_preset_voltage[index],
    &xmeter_dac_voltage);
}

void xmeter_load_preset_dac_c_by_index(uint8_t index)
{
  if(index > xmeter_get_max_preset_index_c()) {
    index = xmeter_get_max_preset_index_c();
  }
  
  xmeter_assign_value(
    &xmeter_dac_preset_current[index],
    &xmeter_dac_current);
}

uint8_t xmeter_get_preset_index_c(void)
{
  return xmeter_dac_preset_current_index;
}

uint8_t xmeter_get_preset_index_v(void)
{
  return xmeter_dac_preset_voltage_index;
}

void xmeter_reset_preset_index_c(void)
{
  xmeter_dac_preset_current_index = 0;
}

void xmeter_reset_preset_index_v(void)
{
  xmeter_dac_preset_voltage_index = 0;
}

void xmeter_inc_preset_dac_v(bit coarse)
{
  xmeter_inc_voltage_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], coarse);
  
  if(xmeter_compare_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], &xmeter_dac_max_voltage) > 0) {
    xmeter_assign_value(&xmeter_dac_max_voltage, &xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index]);
  }
  
  xmeter_load_preset_dac_v();
}

void xmeter_dec_preset_dac_v(bit coarse)
{
  xmeter_dec_voltage_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], coarse);
  
  if(xmeter_compare_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index]);
  }  
  
  xmeter_load_preset_dac_v();
}

void xmeter_inc_preset_dac_c(bit coarse)
{
  xmeter_inc_current_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index], coarse);
  
  if(xmeter_compare_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index], &xmeter_dac_max_current) > 0) {
    xmeter_assign_value(&xmeter_dac_max_current, &xmeter_dac_preset_current[xmeter_dac_preset_current_index]);
  }
  
  xmeter_load_preset_dac_c();
}

void xmeter_dec_preset_dac_c(bit coarse)
{
  xmeter_dec_current_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index], coarse);
  
  if(xmeter_compare_value(&xmeter_dac_preset_current[xmeter_dac_preset_current_index], &xmeter_zero3) < 0) {
    xmeter_assign_value(&xmeter_zero3, &xmeter_dac_preset_current[xmeter_dac_preset_current_index]);
  }  
  
  xmeter_load_preset_dac_c();
}

void xmeter_store_preset_dac_c(uint8_t index)
{
  if(index > xmeter_get_max_preset_index_c()) {
    index = xmeter_get_max_preset_index_c();
  }
  
  xmeter_assign_value(&xmeter_dac_current,
    &xmeter_dac_preset_current[index]);
}

void xmeter_store_preset_dac_v(uint8_t index)
{
  if(index > xmeter_get_max_preset_index_v()) {
    index = xmeter_get_max_preset_index_v();
  }
  
  xmeter_assign_value(&xmeter_dac_voltage,
    &xmeter_dac_preset_voltage[index]);
}

uint8_t xmeter_get_max_preset_index_c(void)
{
  return XMETER_PRESET_CNT - 1;
}

uint8_t xmeter_get_max_preset_index_v(void)
{
  return XMETER_PRESET_CNT - 1;
}

void xmeter_write_rom_preset_dac_v(void)
{
  rom_write_struct(ROM_XMETER_DAC_PRESET_VOLTAGE + sizeof(xmeter_value_t) * xmeter_dac_preset_voltage_index
  , &xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index], sizeof(xmeter_value_t));
}

void xmeter_write_rom_preset_dac_c(void)
{
  rom_write_struct(ROM_XMETER_DAC_PRESET_CURRENT + sizeof(xmeter_value_t) * xmeter_dac_preset_current_index
  , &xmeter_dac_preset_current[xmeter_dac_preset_current_index], sizeof(xmeter_value_t));
}

void xmeter_inc_temp_hi(void)
{
  CDBG("xmeter_inc_temp_hi\n");
  
  xmeter_add_sub_value(&xmeter_temp_hi, &xmeter_step_temp, 1);
  
  if(xmeter_compare_value(&xmeter_temp_hi, &xmeter_max_temp_hi) > 0) {  
    xmeter_assign_value(&xmeter_max_temp_hi, &xmeter_temp_hi);
  }
}

/*
  lo can not above hi - 0.5
*/
void xmeter_inc_temp_lo(void)
{
  xmeter_value_t diff;
  
  CDBG("xmeter_inc_temp_lo\n");
  
  xmeter_assign_value(&xmeter_temp_hi, &diff);
  xmeter_add_sub_value(&diff, &xmeter_temp_lo, 0);
  
  if(xmeter_compare_value(&diff, &xmeter_step_temp_gap ) <= 0)
    return;
  
  xmeter_add_sub_value(&xmeter_temp_lo, &xmeter_step_temp, 1);
  
}

/*
  lo can not above hi - 0.5
*/
void xmeter_dec_temp_hi(void)
{
  xmeter_value_t diff;
  
  CDBG("xmeter_dec_temp_hi\n");
  
  xmeter_assign_value(&xmeter_temp_hi, &diff);
  xmeter_add_sub_value(&diff, &xmeter_temp_lo, 0);
  
  if(xmeter_compare_value(&diff, &xmeter_step_temp_gap ) <= 0)
    return;
  
  xmeter_add_sub_value(&xmeter_temp_hi, &xmeter_step_temp, 0);
  
}

void xmeter_dec_temp_lo(void)
{
  CDBG("xmeter_dec_temp_lo\n");
  
  xmeter_add_sub_value(&xmeter_temp_lo, &xmeter_step_temp, 0);
  
  if(xmeter_compare_value(&xmeter_temp_lo, &xmeter_min_temp_lo) < 0) {
    xmeter_assign_value(&xmeter_min_temp_lo, &xmeter_temp_lo);
  }  
}

void xmeter_set_temp_hi(const xmeter_value_t * val)
{
  xmeter_value_t diff;
  xmeter_value_t temp;
  
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_temp);
  
  if(xmeter_compare_value(&temp, &xmeter_max_temp_hi) > 0) {  
    xmeter_assign_value(&xmeter_max_temp_hi, &xmeter_temp_hi);
    return;
  }  
  
  xmeter_assign_value(&xmeter_temp_lo, &diff);
  xmeter_add_sub_value(&diff, &xmeter_step_temp, 1);
  
  if(xmeter_compare_value(&temp, &diff) <= 0) {
    xmeter_assign_value(&diff, &xmeter_temp_hi);
    return;
  }
  
  xmeter_assign_value(&temp, &xmeter_temp_hi);
}

void xmeter_set_temp_lo(const xmeter_value_t * val)
{
  xmeter_value_t diff;
  xmeter_value_t temp;
  
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_temp);
  
  if(xmeter_compare_value(&temp, &xmeter_min_temp_lo) <= 0) {  
    xmeter_assign_value(&xmeter_min_temp_lo, &xmeter_temp_lo);
    return;
  }  
  
  xmeter_assign_value(&xmeter_temp_hi, &diff);
  xmeter_add_sub_value(&diff, &xmeter_step_temp, 0);
  
  if(xmeter_compare_value(&temp, &diff) >= 0) {
    xmeter_assign_value(&diff, &xmeter_temp_lo);
    return;
  }
  
  xmeter_assign_value(&temp, &xmeter_temp_lo);
}

void xmeter_write_rom_temp_lo()
{
  CDBG("xmeter_write_rom_temp_lo\n");
  rom_write_struct(ROM_XMETER_TEMP_LO, &xmeter_temp_lo, sizeof(xmeter_value_t));  
}

void xmeter_write_rom_temp_hi()
{
  CDBG("xmeter_write_rom_temp_hi\n");
  rom_write_struct(ROM_XMETER_TEMP_HI, &xmeter_temp_hi, sizeof(xmeter_value_t));  
}

void xmeter_inc_max_power_diss(void)
{
  xmeter_add_sub_value(&xmeter_max_power_diss, &xmeter_step_power, 1);
  
  if(xmeter_compare_value(&xmeter_max_power_diss, &xmeter_max_power_diss_max) > 0) {
    xmeter_assign_value(&xmeter_max_power_diss_max, &xmeter_max_power_diss);
  }
}

void xmeter_dec_max_power_diss(void)
{
  xmeter_add_sub_value(&xmeter_max_power_diss, &xmeter_step_power , 0);
  
  if(xmeter_compare_value(&xmeter_max_power_diss, &xmeter_max_power_diss_min) < 0) {
    xmeter_assign_value(&xmeter_max_power_diss_min, &xmeter_max_power_diss);
  }
}

void xmeter_set_max_power_diss(const xmeter_value_t * val)
{
  xmeter_value_t temp;
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_power);
  
  if(xmeter_compare_value(&temp, &xmeter_max_power_diss_min) < 0) {
    xmeter_assign_value(&xmeter_max_power_diss_min, &xmeter_max_power_diss);
    return;
  }
  
  if(xmeter_compare_value(&temp, &xmeter_max_power_diss_max) > 0) {
    xmeter_assign_value(&xmeter_max_power_diss_max, &xmeter_max_power_diss);
    return;
  }
  
  xmeter_assign_value(&temp, &xmeter_max_power_diss);
}

void xmeter_write_rom_max_power_diss(void)
{
  CDBG("xmeter_write_rom_max_power_diss\n");
  rom_write_struct(ROM_XMETER_MAX_POWER_DISS, 
    &xmeter_max_power_diss, sizeof(xmeter_max_power_diss));
}
  
void xmeter_inc_temp_overheat(void)
{
  xmeter_add_sub_value(&xmeter_temp_overheat, &xmeter_step_power, 1); 
  
  if(xmeter_compare_value(&xmeter_temp_overheat, &xmeter_max_temp_hi) > 0) {
    xmeter_assign_value(&xmeter_max_temp_hi, &xmeter_temp_overheat);
  }
}

void xmeter_dec_temp_overheat(void)
{
  xmeter_add_sub_value(&xmeter_temp_overheat, &xmeter_step_power, 0);
  
  if(xmeter_compare_value(&xmeter_temp_overheat, &xmeter_min_temp_lo) < 0) {
    xmeter_assign_value(&xmeter_min_temp_lo, &xmeter_temp_overheat);
  }
}

void xmeter_set_temp_overheat(const xmeter_value_t * val)
{

  xmeter_value_t temp;
  xmeter_assign_value(val, &temp);
  xmeter_round_value(&temp, &xmeter_step_temp);  
  
  if(xmeter_compare_value(&temp, &xmeter_max_temp_hi) > 0) {
    xmeter_assign_value(&xmeter_max_temp_hi, &xmeter_temp_overheat);
    return;
  }
  
  if(xmeter_compare_value(&temp, &xmeter_min_temp_lo) < 0) {
    xmeter_assign_value(&xmeter_min_temp_lo, &xmeter_temp_overheat);
    return;
  }
  
  xmeter_assign_value(&temp, &xmeter_temp_overheat);
}

void xmeter_write_rom_temp_overheat(void)
{
  CDBG(("xmeter_write_rom_temp_overheat\n"));
  rom_write_struct(ROM_XMETER_TEMP_OVERHEAT, &xmeter_temp_overheat, sizeof(xmeter_temp_overheat));
}  

/*
WARN: val from 0000 to FFFF (unsigned!)
*/
static uint16_t xmeter_inc_dec_bits(uint16_t val, bit is_inc, bit coarse)
{
  uint16_t delta;
  
  delta = coarse ? 0x100 : 1;
  
  if(is_inc) {
    if(val > UINT_MAX - delta) {
      val = UINT_MAX;
    } else {
      val += delta;
    }
  } else {
    if(val < delta) {
      val = 0;
    } else {
      val -= delta;
    }
  }
  
  return val;
}

/* directlly fuck up dac */
void xmeter_inc_dac_bits_v(bit coarse)
{
  uint16_t val = xmeter_get_dac_bits_v();
  val = xmeter_inc_dec_bits(val, 1, coarse);
  xmeter_set_dac_bits_v(val);
}

void xmeter_dec_dac_bits_v(bit coarse)
{
  uint16_t val = xmeter_get_dac_bits_v();
  val = xmeter_inc_dec_bits(val, 0, coarse);
  xmeter_set_dac_bits_v(val);
}

void xmeter_inc_dac_bits_c(bit coarse)
{
  uint16_t val = xmeter_get_dac_bits_c();
  val = xmeter_inc_dec_bits(val, 1, coarse);
  xmeter_set_dac_bits_c(val);
}

void xmeter_dec_dac_bits_c(bit coarse)
{
  uint16_t val = xmeter_get_dac_bits_c();
  val = xmeter_inc_dec_bits(val, 0, coarse);
  xmeter_set_dac_bits_c(val);
}

void xmeter_dac_raw_write_bits(uint8_t addr, uint16_t dat)
{
  uint8_t val = 0;
  
  I2C_Init();
  
  I2C_Start();
  
  do {
    /* write address  */
    if(!xmeter_dac_write_and_wait_ack(addr))
      break;
    
    /* write control */
    val = 0x10; /*0001 0000*/
    if(!xmeter_dac_write_and_wait_ack(val))
      break;
    
    /* write MSB */
    val = (uint8_t)(dat >> 8)& 0xFF;
    if(!xmeter_dac_write_and_wait_ack(val))
      break;
    
    /* write LSB*/
    val = (uint8_t)(dat & 0xFF);
    if(!xmeter_dac_write_and_wait_ack(val))
      break; 
 
  }while(0);
  
  I2C_Stop();
}

static uint16_t xmeter_dac_raw_read_bits(uint8_t addr)
{
  uint8_t val = 0;
  uint16_t ret = 0;
  
  I2C_Init();
  
  addr |= 1; /* is read */
  
  I2C_Start();
  
  do {
    
    /* write address  */
    if(!xmeter_dac_write_and_wait_ack(addr))
      break;
    
    /* read MSB */
    if(!xmeter_dac_read_and_ack(&val, 0))
      break;
    ret = val;
    ret = ret << 8;
    
    /* read LSB*/
    if(!xmeter_dac_read_and_ack(&val, 0))
      break;
    ret |= val;
    
    /* read control, drop.. */
    if(!xmeter_dac_read_and_ack(&val, 1))
      break;
    CDBG("xmeter_dac_raw_read_bits control = %bx, ret = %x\n", val, ret);
    
  }while(0);
  
  I2C_Stop();
  
  return ret;
}

uint16_t xmeter_get_dac_bits_v(void)
{
  
  uint16_t bits = 0;
  
  //bits = xmeter_dac_raw_read_bits(XMETER_DAC_I2C_VOLTAGE);
  
  bits = xmeter_dac_internal_bit_v;
  
  bits = 0xFFFF - bits;
  
  CDBG("xmeter_get_dac_bits_v return %04x\n", bits);
  
  return bits;
}

uint16_t xmeter_get_dac_bits_c(void)
{
  uint16_t bits = 0;

  //bits = xmeter_dac_raw_read_bits(XMETER_DAC_I2C_CURRENT);  

  bits = xmeter_dac_internal_bit_c;
  
  CDBG("xmeter_get_dac_bits_c return %04x\n", bits);
  return bits;
}

void xmeter_set_dac_bits_v(uint16_t bits)
{
  //uint16_t ret = 0;
  CDBG("xmeter_set_dac_bits_v set %04x\n", bits);
  bits = 0xFFFF - bits;
  xmeter_dac_raw_write_bits(XMETER_DAC_I2C_VOLTAGE, bits);
  //ret = xmeter_dac_raw_read_bits(XMETER_DAC_I2C_VOLTAGE);
  //if(bits != ret) {
  //  CDBG(("xmeter_set_dac_bits_v FAILED!\n"));
  //} else {
    xmeter_dac_internal_bit_v = bits;
  //}
}

void xmeter_set_dac_bits_c(uint16_t bits)
{
  //uint16_t ret = 0;
  CDBG("xmeter_set_dac_bits_c set %04x\n", bits);
  xmeter_dac_raw_write_bits(XMETER_DAC_I2C_CURRENT, bits);
  //ret = xmeter_dac_raw_read_bits(XMETER_DAC_I2C_CURRENT);
  //if(bits != ret) {
  //  CDBG(("xmeter_set_dac_bits_c FAILED!\n"));
  //} else {
    xmeter_dac_internal_bit_c = bits;
  //}  
}

/*
  read dac, and update xmeter_dac_**
*/
void xmeter_read_dac_voltage(void)
{
  /* read dac and translate bits to value */
  uint16_t bits = xmeter_get_dac_bits_v();
  
  xmeter_bits2value(bits, 
  &xmeter_dac_voltage, 
  xmeter_dac_voltage_g, XMETER_GRID_SIZE, XMETER_RES_VOLTAGE, 0);
}

void xmeter_read_dac_current(void)
{
  /* read dac and translate bits to value */
  uint16_t bits = xmeter_get_dac_bits_c();
  
  xmeter_bits2value(bits, &xmeter_dac_current, 
    xmeter_dac_current_g, XMETER_GRID_SIZE, XMETER_RES_CURRENT, 0);
}

/* make xmeter_dac_** work! */
void xmeter_write_dac_voltage(void)
{
  double f;
  /* translate value to bis and write to dac */
  uint16_t bits = xmeter_value2bits(&xmeter_dac_voltage,
    &f, xmeter_dac_voltage_g, XMETER_GRID_SIZE, 0);
  
  xmeter_set_dac_bits_v(bits);
}

void xmeter_write_rom_dac_voltage(void)
{
  rom_write_struct(ROM_XMETER_DAC_LAST_VOLTAGE, &xmeter_dac_voltage, sizeof(xmeter_dac_voltage));
}

void xmeter_write_dac_current(void)
{
  double f;
  
  /* translate value to bis and write to dac */
  uint16_t bits = xmeter_value2bits(&xmeter_dac_current,
    &f, xmeter_dac_current_g, XMETER_GRID_SIZE, 0);
  
  xmeter_set_dac_bits_c(bits);
}

void xmeter_write_rom_dac_current(void)
{
  rom_write_struct(ROM_XMETER_DAC_LAST_CURRENT, &xmeter_dac_current, sizeof(xmeter_dac_current));
}

void xmeter_get_voltage_steps(xmeter_value_t * coarse, xmeter_value_t * fine)
{
  xmeter_assign_value(&xmeter_step_coarse_voltage, coarse);
  xmeter_assign_value(&xmeter_step_fine_voltage, fine);
}

void xmeter_get_current_steps(xmeter_value_t * coarse, xmeter_value_t * fine)
{
  xmeter_assign_value(&xmeter_step_coarse_current, coarse);
  xmeter_assign_value(&xmeter_step_fine_current, fine);
}

void xmeter_get_temp_steps(xmeter_value_t * coarse, xmeter_value_t * fine)
{
  xmeter_assign_value(&xmeter_step_temp, coarse);
  xmeter_assign_value(&xmeter_step_temp, fine);
}

void xmeter_get_power_steps(xmeter_value_t * coarse, xmeter_value_t * fine)
{
  xmeter_assign_value(&xmeter_step_power, coarse);
  xmeter_assign_value(&xmeter_step_power, fine);
}

void xmeter_get_current_limits(xmeter_value_t * min, xmeter_value_t * max)
{
  xmeter_assign_value(&xmeter_zero3, min);
  xmeter_assign_value(&xmeter_dac_max_current, max);
}

void xmeter_get_voltage_out_limits(xmeter_value_t * min, xmeter_value_t * max)
{
  xmeter_assign_value(&xmeter_zero3, min);
  xmeter_assign_value(&xmeter_dac_max_voltage, max);
}

void xmeter_get_temp_limits(xmeter_value_t * min, xmeter_value_t * max)
{
  xmeter_assign_value(&xmeter_min_temp_lo, min);
  xmeter_assign_value(&xmeter_max_temp_hi, max);
}

void xmeter_get_power_diss_limits(xmeter_value_t * min, xmeter_value_t * max)
{
  xmeter_assign_value(&xmeter_max_power_diss_min, min);
  xmeter_assign_value(&xmeter_max_power_diss_max, max);
}