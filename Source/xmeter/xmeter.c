#include "xmeter.h"
#include "gpio.h"
#include "i2c.h"
#include "debug.h"
#include "delay.h"
#include "rom.h"
#include "task.h"

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

#define XMETER_RES_ZERO     0
#define XMETER_RES_ONE      1
#define XMETER_RES_TWO      2
#define XMETER_RES_THREE    3

#define XMETER_RES_VOLTAGE  XMETER_RES_THREE
#define XMETER_RES_CURRENT  XMETER_RES_THREE
#define XMETER_RES_TEMP     XMETER_RES_TWO
#define XMETER_RES_POWER    XMETER_RES_THREE

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
static double xmeter_adc_current_k;
static double xmeter_adc_current_b;

static double xmeter_adc_voltage_out_f;
static double xmeter_adc_voltage_out_k;
static double xmeter_adc_voltage_out_b;

static double xmeter_adc_voltage_diss_f;
static double xmeter_adc_voltage_diss_k;
static double xmeter_adc_voltage_diss_b;

static double xmeter_adc_temp_f;
static double xmeter_power_out_f;
static double xmeter_power_diss_f;

xmeter_value_t xmeter_dac_current;
xmeter_value_t xmeter_dac_voltage;

static double xmeter_dac_current_k;
static double xmeter_dac_current_b;
static double xmeter_dac_voltage_k;
static double xmeter_dac_voltage_b;

static const xmeter_value_t code xmeter_dac_max_voltage = {0, XMETER_RES_VOLTAGE, 30, 0}; /* 30V */
static const xmeter_value_t code xmeter_dac_max_current = {0, XMETER_RES_CURRENT, 5, 0}; /* 5A */

xmeter_value_t xmeter_temp_hi;
xmeter_value_t xmeter_temp_lo;
xmeter_value_t xmeter_temp_overheat;

xmeter_value_t xmeter_max_power_diss;

static const xmeter_value_t code xmeter_max_temp_hi = {0, XMETER_RES_TEMP, 150, 0} /* 150 C */;
static const xmeter_value_t code xmeter_min_temp_lo = {0, XMETER_RES_TEMP, 0, 0};  /* 0 C */
static const xmeter_value_t code xmeter_max_power_diss_max = {0, XMETER_RES_POWER, 80, 0};  /* 80W */
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

struct xmeter_adc_temp_slot
{
  double temp;
  uint16_t adc_val;
};

/*
  47k电阻串联，NTC 100K，B=3950K
  分压3V, ADC量程7FFF->4.096V
  中心温度在40度附近
*/
static const struct xmeter_adc_temp_slot code 
  xmeter_adc_temp_table[] = 
{
{-55.000 ,0X4DBF},
{-54.000 ,0X4DB8},
{-53.000 ,0X4DB1},
{-52.000 ,0X4DA9},
{-51.000 ,0X4DA0},
{-50.000 ,0X4D97},
{-49.000 ,0X4D8D},
{-48.000 ,0X4D82},
{-47.000 ,0X4D77},
{-46.000 ,0X4D6A},
{-45.000 ,0X4D5D},
{-44.000 ,0X4D4F},
{-43.000 ,0X4D40},
{-42.000 ,0X4D30},
{-41.000 ,0X4D1F},
{-40.000 ,0X4D0C},
{-39.000 ,0X4CF9},
{-38.000 ,0X4CE4},
{-37.000 ,0X4CCE},
{-36.000 ,0X4CB7},
{-35.000 ,0X4C9E},
{-34.000 ,0X4C84},
{-33.000 ,0X4C69},
{-32.000 ,0X4C4C},
{-31.000 ,0X4C2D},
{-30.000 ,0X4C0D},
{-29.000 ,0X4BEB},
{-28.000 ,0X4BC7},
{-27.000 ,0X4BA1},
{-26.000 ,0X4B7A},
{-25.000 ,0X4B50},
{-24.000 ,0X4B24},
{-23.000 ,0X4AF7},
{-22.000 ,0X4AC7},
{-21.000 ,0X4A95},
{-20.000 ,0X4A61},
{-19.000 ,0X4A2A},
{-18.000 ,0X49F1},
{-17.000 ,0X49B6},
{-16.000 ,0X4978},
{-15.000 ,0X4937},
{-14.000 ,0X48F4},
{-13.000 ,0X48AE},
{-12.000 ,0X4866},
{-11.000 ,0X481A},
{-10.000 ,0X47CC},
{-9.000 ,0X477B	},
{-8.000 ,0X4727	},
{-7.000 ,0X46D0	},
{-6.000 ,0X4676	},
{-5.000 ,0X4618	},
{-4.000 ,0X45B8	},
{-3.000 ,0X4554	},
{-2.000 ,0X44ED	},
{-1.000 ,0X4483	},
{0.000 ,0X4420	},
{1.000 ,0X43A5	},
{2.000 ,0X4331	},
{3.000 ,0X42B9	},
{4.000 ,0X423E	},
{5.000 ,0X41C0	},
{6.000 ,0X413E	},
{7.000 ,0X40B9	},
{8.000 ,0X4031	},
{9.000 ,0X3FA5	},
{10.000 ,0X3F16	},
{11.000 ,0X3E83	},
{12.000 ,0X3DED	},
{13.000 ,0X3D54	},
{14.000 ,0X3CB7	},
{15.000 ,0X3C18	},
{16.000 ,0X3B75	},
{17.000 ,0X3ACF	},
{18.000 ,0X3A27	},
{19.000 ,0X397B	},
{20.000 ,0X38CD	},
{21.000 ,0X381C	},
{22.000 ,0X3768	},
{23.000 ,0X36B2	},
{24.000 ,0X35F9	},
{25.000 ,0X353E	},
{26.000 ,0X3481	},
{27.000 ,0X33C2	},
{28.000 ,0X3301	},
{29.000 ,0X323E	},
{30.000 ,0X317A	},
{31.000 ,0X30B5	},
{32.000 ,0X2FEE	},
{33.000 ,0X2F26	},
{34.000 ,0X2E5D	},
{35.000 ,0X2D94	},
{36.000 ,0X2CCA	},
{37.000 ,0X2BFF	},
{38.000 ,0X2B34	},
{39.000 ,0X2A69	},
{40.000 ,0X299F	},
{41.000 ,0X28D4	},
{42.000 ,0X280A	},
{43.000 ,0X2741	},
{44.000 ,0X2678	},
{45.000 ,0X25B0	},
{46.000 ,0X24E9	},
{47.000 ,0X2424	},
{48.000 ,0X2360	},
{49.000 ,0X229D	},
{50.000 ,0X21DC	},
{51.000 ,0X211D	},
{52.000 ,0X2060	},
{53.000 ,0X1FA4	},
{54.000 ,0X1EEB	},
{55.000 ,0X1E34	},
{56.000 ,0X1D80	},
{57.000 ,0X1CCD	},
{58.000 ,0X1C1D	},
{59.000 ,0X1B70	},
{60.000 ,0X1AC6	},
{61.000 ,0X1A1E	},
{62.000 ,0X1979	},
{63.000 ,0X18D7	},
{64.000 ,0X1837	},
{65.000 ,0X179A	},
{66.000 ,0X1701	},
{67.000 ,0X166A	},
{68.000 ,0X15D6	},
{69.000 ,0X1545	},
{70.000 ,0X14B8	},
{71.000 ,0X142D	},
{72.000 ,0X13A5	},
{73.000 ,0X1320	},
{74.000 ,0X129E	},
{75.000 ,0X121F	},
{76.000 ,0X11A3	},
{77.000 ,0X112A	},
{78.000 ,0X10B3	},
{79.000 ,0X1040	},
{80.000 ,0X0FCF	},
{81.000 ,0X0F61	},
{82.000 ,0X0EF6	},
{83.000 ,0X0E8E	},
{84.000 ,0X0E28	},
{85.000 ,0X0DC5	},
{86.000 ,0X0D64	},
{87.000 ,0X0D06	},
{88.000 ,0X0CAB	},
{89.000 ,0X0C51	},
{90.000 ,0X0BFB	},
{91.000 ,0X0BA6	},
{92.000 ,0X0B54	},
{93.000 ,0X0B04	},
{94.000 ,0X0AB7	},
{95.000 ,0X0A6B	},
{96.000 ,0X0A22	},
{97.000 ,0X09DA	},
{98.000 ,0X0995	},
{99.000 ,0X0952	},
{100.000 ,0X0916},
{101.000 ,0X08D0},
{102.000 ,0X0893},
{103.000 ,0X0857},
{104.000 ,0X081C},
{105.000 ,0X07E4},
{106.000 ,0X07AD},
{107.000 ,0X0777},
{108.000 ,0X0743},
{109.000 ,0X0711},
{110.000 ,0X06E0},
{111.000 ,0X06B1},
{112.000 ,0X0682},
{113.000 ,0X0656},
{114.000 ,0X062A},
{115.000 ,0X0600},
{116.000 ,0X05D7},
{117.000 ,0X05AF},
{118.000 ,0X0588},
{119.000 ,0X0563},
{120.000 ,0X053E},
{121.000 ,0X051B},
{122.000 ,0X04F8},
{123.000 ,0X04D7},
{124.000 ,0X04B7},
{125.000 ,0X0497},
{126.000 ,0X0479},
{127.000 ,0X045B},
{128.000 ,0X043E},
{129.000 ,0X0422},
{130.000 ,0X0407},
{131.000 ,0X03EC},
{132.000 ,0X03D3},
{133.000 ,0X03BA},
{134.000 ,0X03A2},
{135.000 ,0X038A},
{136.000 ,0X0373},
{137.000 ,0X035D},
{138.000 ,0X0347},
{139.000 ,0X0333},
{140.000 ,0X031E},
{141.000 ,0X030A},
{142.000 ,0X02F7},
{143.000 ,0X02E4},
{144.000 ,0X02D2},
{145.000 ,0X02C0},
{146.000 ,0X02AF},
{147.000 ,0X029E},
{148.000 ,0X028E},
{149.000 ,0X027E},
{150.000 ,0X026F},
{151.000 ,0X0260},
{152.000 ,0X0252},
{153.000 ,0X0243},
{154.000 ,0X0236},
{155.000 ,0X0228},
{156.000 ,0X021B},
{157.000 ,0X020F},
{158.000 ,0X0202},
{159.000 ,0X01F6},
{160.000 ,0X01EB},
{161.000 ,0X01DF},
{162.000 ,0X01D4},
{163.000 ,0X01C9},
{164.000 ,0X01BF},
{165.000 ,0X01B5},
{166.000 ,0X01AB},
{167.000 ,0X01A1},
{168.000 ,0X0198},
{169.000 ,0X018F},
{170.000 ,0X0186},
{171.000 ,0X017D},
{172.000 ,0X0174},
{173.000 ,0X016C},
{174.000 ,0X0164},
{175.000 ,0X015C},
{176.000 ,0X0155},
{177.000 ,0X014D},
{178.000 ,0X0146},
{179.000 ,0X013F},
{180.000 ,0X0138},
{181.000 ,0X0131},
{182.000 ,0X012B},
{183.000 ,0X0124},
{184.000 ,0X011E},
{185.000 ,0X0118},
{186.000 ,0X0112},
{187.000 ,0X010C},
{188.000 ,0X0106},
{189.000 ,0X0101},
{190.000 ,0X00FC},
{191.000 ,0X00F7},
{192.000 ,0X00F1},
{193.000 ,0X00EC},
{194.000 ,0X00E8},
{195.000 ,0X00E3},
{196.000 ,0X00DE},
{197.000 ,0X00DA},
{198.000 ,0X00D5},
{199.000 ,0X00D1},
{200.000 ,0X00CD},
{201.000 ,0X00C9},
{202.000 ,0X00C5},
{203.000 ,0X00C1},
{204.000 ,0X00BD},
{205.000 ,0X00B9},
{206.000 ,0X00B6},
{207.000 ,0X00B2},
{208.000 ,0X00AE},
{209.000 ,0X00AB},
{210.000 ,0X00A8},
{211.000 ,0X00A5},
{212.000 ,0X00A1},
{213.000 ,0X009F},
{214.000 ,0X009B},
{215.000 ,0X0098},
{216.000 ,0X0096},
{217.000 ,0X0093},
{218.000 ,0X0090},
{219.000 ,0X008D},
{220.000 ,0X008B},
{221.000 ,0X0088},
{222.000 ,0X0086},
{223.000 ,0X0083},
{224.000 ,0X0081},
{225.000 ,0X007E},
{226.000 ,0X007C},
{227.000 ,0X007A},
{228.000 ,0X0077},
{229.000 ,0X0075},
{230.000 ,0X0073},
{231.000 ,0X0071},
{232.000 ,0X006F},
{233.000 ,0X006D},
{234.000 ,0X006B},
{235.000 ,0X006A},
{236.000 ,0X0068},
{237.000 ,0X0066},
{238.000 ,0X0064},
{239.000 ,0X0062},
{240.000 ,0X0061},
{241.000 ,0X005F},
{242.000 ,0X005D},
{243.000 ,0X005C},
{244.000 ,0X005A},
{245.000 ,0X0059},
{246.000 ,0X0057},
{247.000 ,0X0056},
{248.000 ,0X0054},
{249.000 ,0X0053},
{250.000 ,0X0052}
};

void xmeter_dump_value(const char * name, xmeter_value_t * pval, uint8_t cnt)
{
  uint8_t index;
  for(index = 0 ; index < cnt; index ++) {
    CDBG("value %s[%bu]: [%bu] %c%03u.%03u\n", name, index, pval[index].res,
      pval[index].neg ? '-':'+', pval[index].integer, pval[index].decimal);  
  }
}


bit xmeter_cal(uint16_t x1, uint16_t x2, double y1, double y2, double * k, double * b)
{

  if(x2 <= x1)
    return 0;
  
  *k = (y2 - y1) / (x2 - x1);
  
  *b = y2 - x2 * (*k);
  
  return *k != 0.0;
}

void xmeter_write_rom_adc_voltage_diss_kb(double k, double b)
{
  rom_write32(ROM_XMETER_ADC_VOLTAGE_DISS_K, &k);
  rom_write32(ROM_XMETER_ADC_VOLTAGE_DISS_B, &b);
}

void xmeter_write_rom_adc_voltage_out_kb(double k, double b)
{
  rom_write32(ROM_XMETER_ADC_VOLTAGE_OUT_K, &k);
  rom_write32(ROM_XMETER_ADC_VOLTAGE_OUT_B, &b); 
}

void xmeter_write_rom_adc_current_kb(double k, double b)
{
  rom_write32(ROM_XMETER_ADC_CURRENT_K, &k);
  rom_write32(ROM_XMETER_ADC_CURRENT_B, &b);
}

void xmeter_write_rom_dac_current_kb(double k, double b)
{
  rom_write32(ROM_XMETER_DAC_CURRENT_K, &k);
  rom_write32(ROM_XMETER_DAC_CURRENT_B, &b);
}

void xmeter_write_rom_dac_voltage_kb(double k, double b)
{
  rom_write32(ROM_XMETER_DAC_VOLTAGE_K, &k);
  rom_write32(ROM_XMETER_DAC_VOLTAGE_B, &b); 
}

void xmeter_rom_factory_reset(void)
{
  double k, b;
  xmeter_value_t val[4];
  /*
  电路设计 Note:
    输入部分保障ADC最大测量30/45V、5A
    输出部分保障DAC最大输出30V、5A
  */
  
  /* xmeter_adc_current, fsr is 0~5.0
     0000 -> 0.0
     7fff -> 4.096 (ADC输入电压)
  */
  xmeter_cal(0x0, 0x7fff, 0.0, 5.0, &k, &b);
  xmeter_write_rom_adc_current_kb(k, b);
  
  /* xmeter_adc_voltage_out, fsr is 0~30.0
     0000 -> 0.0
     7fff -> 4.096 (ADC输入电压)
  */  
  xmeter_cal(0x0, 0x7fff, 0.0, 30.0, &k, &b); 
  xmeter_write_rom_adc_voltage_out_kb(k, b);
  
  /* xmeter_adc_voltage_in, fsr is 0~45.0
     0000 -> 0.0
     7fff -> 4.096 (ADC耗散电压)
  */  
  xmeter_cal(0x0, 0x7fff, 0.0, 45.0, &k, &b);
  xmeter_write_rom_adc_voltage_diss_kb(k, b);
  
  /* xmeter_dac_current, fsr is 0~5.0
     0000 -> 0.0
     ffff -> 2.5 (DAC输出电压)
  */    
  xmeter_cal(0x0, 0xffff, 0.0, 5.0, &k, &b);
  xmeter_write_rom_dac_current_kb(k, b);
  
  /* xmeter_dac_voltage, fsr is 0~30.0
     0000 -> 0.0
     ffff -> 2.5 (DAC输出电压)
  */    
  xmeter_cal(0x0, 0xffff, 0.0, 30, &k, &b);
  xmeter_write_rom_dac_voltage_kb(k, b);
  
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
  
  /* last dac current 1A and voltage 5V*/
  val[0].neg = 0;
  val[0].res = XMETER_RES_CURRENT;  
  val[0].integer = 1;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_LAST_CURRENT, val, sizeof(xmeter_value_t));
  
  val[0].neg = 0;
  val[0].res = XMETER_RES_VOLTAGE;  
  val[0].integer = 5;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_DAC_LAST_VOLTAGE, val, sizeof(xmeter_value_t));  

  /* temp high is 80 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 60;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_HI, val, sizeof(xmeter_value_t));  
  
  /* temp low is 40 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 40;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_LO, val, sizeof(xmeter_value_t));  

  /* temp overheat is 80 C */
  val[0].neg = 0;
  val[0].res = XMETER_RES_TEMP;  
  val[0].integer = 80;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_TEMP_OVERHEAT, val, sizeof(xmeter_value_t));  
  
  /* max diss power is 70 W */
  val[0].neg = 0;
  val[0].res = XMETER_RES_POWER;  
  val[0].integer = 80;
  val[0].decimal = 0;
  rom_write_struct(ROM_XMETER_MAX_POWER_DISS, val, sizeof(xmeter_value_t));    
}

void xmeter_load_config(void)
{
  rom_read32(ROM_XMETER_ADC_CURRENT_K, &xmeter_adc_current_k);
  CDBG("ADC_Ck = %f\n", xmeter_adc_current_k);
  
  rom_read32(ROM_XMETER_ADC_CURRENT_B, &xmeter_adc_current_b); 
  CDBG("ADC_Cb = %f\n", xmeter_adc_current_b);  
  
  rom_read32(ROM_XMETER_ADC_VOLTAGE_OUT_K, &xmeter_adc_voltage_out_k);
  CDBG("ADC_Vok = %f\n", xmeter_adc_voltage_out_k); 
  
  rom_read32(ROM_XMETER_ADC_VOLTAGE_OUT_B, &xmeter_adc_voltage_out_b);
  CDBG("ADC_Vob = %f\n", xmeter_adc_voltage_out_b); 

  rom_read32(ROM_XMETER_ADC_VOLTAGE_DISS_K, &xmeter_adc_voltage_diss_k);
  CDBG("ADC_Vdk = %f\n", xmeter_adc_voltage_diss_k); 
  
  rom_read32(ROM_XMETER_ADC_VOLTAGE_DISS_B, &xmeter_adc_voltage_diss_b);
  CDBG("ADC_Vdb = %f\n", xmeter_adc_voltage_diss_b); 
  
  rom_read32(ROM_XMETER_DAC_CURRENT_K, &xmeter_dac_current_k);
  CDBG("DAC_Ck = %f\n", xmeter_dac_current_k);
  
  rom_read32(ROM_XMETER_DAC_CURRENT_B, &xmeter_dac_current_b); 
  CDBG("DAC_Cb = %f\n", xmeter_dac_current_b);  
  
  rom_read32(ROM_XMETER_DAC_VOLTAGE_K, &xmeter_dac_voltage_k);
  CDBG("DAC_Vk = %f\n", xmeter_dac_voltage_k); 
  
  rom_read32(ROM_XMETER_DAC_VOLTAGE_B, &xmeter_dac_voltage_b);
  CDBG("DAC_Vb = %f\n", xmeter_dac_voltage_b); 

  rom_read_struct(ROM_XMETER_DAC_PRESET_CURRENT, xmeter_dac_preset_current, sizeof(xmeter_dac_preset_current));
  xmeter_dump_value("DAC_Preset_C", xmeter_dac_preset_current, 4); 

  rom_read_struct(ROM_XMETER_DAC_PRESET_VOLTAGE, xmeter_dac_preset_voltage, sizeof(xmeter_dac_preset_voltage));
  xmeter_dump_value("DAC_Preset_V", xmeter_dac_preset_voltage, 4); 
  
  xmeter_dac_preset_current_index = 0;
  xmeter_dac_preset_voltage_index = 0;
  
  rom_read_struct(ROM_XMETER_DAC_LAST_CURRENT, &xmeter_dac_current, sizeof(xmeter_dac_current));
  xmeter_dump_value("DAC_Last_C", &xmeter_dac_current, 1);   

  rom_read_struct(ROM_XMETER_DAC_LAST_VOLTAGE, &xmeter_dac_voltage, sizeof(xmeter_dac_voltage));
  xmeter_dump_value("DAC_Last_V", &xmeter_dac_voltage, 1);
  
  rom_read_struct(ROM_XMETER_TEMP_HI, &xmeter_temp_hi, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Hi", &xmeter_temp_hi, 1);
  
  rom_read_struct(ROM_XMETER_TEMP_LO, &xmeter_temp_lo, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Lo", &xmeter_temp_lo, 1);  
  
  rom_read_struct(ROM_XMETER_TEMP_OVERHEAT, &xmeter_temp_overheat, sizeof(xmeter_value_t));  
  xmeter_dump_value("Temp_Overheat", &xmeter_temp_overheat, 1);  

  rom_read_struct(ROM_XMETER_MAX_POWER_DISS, &xmeter_max_power_diss, sizeof(xmeter_value_t));  
  xmeter_dump_value("Max_Diss_Power", &xmeter_max_power_diss, 1);  
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

static double xmeter_adc_lookup_temp(uint16_t val)
{
  uint16_t cnt = sizeof(xmeter_adc_temp_table) / sizeof(struct xmeter_adc_temp_slot);
  uint16_t i, dx;
  double dy;
  if(val <= xmeter_adc_temp_table[0].adc_val) return xmeter_adc_temp_table[0].temp;
    
  if(val >= xmeter_adc_temp_table[cnt - 1].adc_val) return xmeter_adc_temp_table[cnt - 1].temp;
  
  /* lookup table */
  for(i = 0 ; i < cnt - 1; i ++) {
    if(xmeter_adc_temp_table[i].adc_val <= val &&
      xmeter_adc_temp_table[i + 1].adc_val > val) {
        break;
      }
  }
  
  /* 插值 */
  dx = xmeter_adc_temp_table[i + 1].adc_val - xmeter_adc_temp_table[i].adc_val;
  dy = xmeter_adc_temp_table[i + 1].temp - xmeter_adc_temp_table[i].temp;
  
  return xmeter_adc_temp_table[i].temp + (val - xmeter_adc_temp_table[i].adc_val) * dy / dx;
}

/*
is_signed: interpret return as a signed integer
*/

static uint16_t xmeter_value2bits(
  xmeter_value_t * val,
  double * f,
  double k, double b, uint8_t is_signed)
{
  int32_t temp_bits;
  *f = xmeter_val2float(val);
  if(k == 0) {
    CDBG(("[ERROR!] invalid k!\n"));
    k = 1;
  }
  temp_bits = (*f - b) / k;
  
  if(is_signed) {
    if(temp_bits > SHRT_MAX) {
      temp_bits = SHRT_MAX;
    } else if(temp_bits < SHRT_MIN) {
      temp_bits = SHRT_MIN;
    }
  } else {
    if(temp_bits > USHRT_MAX) {
      temp_bits = USHRT_MAX;
    } else if(temp_bits < 0) {
      temp_bits = 0;
    }
  }
  return (uint16_t)temp_bits;
}

/*
is_signed: interpret bits as a signed integer
*/

static double xmeter_bits2value(
  uint16_t bits, 
  xmeter_value_t * val,
  double k, double b, uint8_t res, uint8_t is_signed)
{
  double ret;
  int32_t temp_bits;
  
  if(is_signed) {
    temp_bits = (int16_t)bits;
  } else {
    temp_bits = bits;
  }
  
  ret = temp_bits * k + b;
  xmeter_float2val(ret, val, res);
  return ret;
}

uint16_t xmeter_get_adc_bits_current(void)
{
  uint16_t config = 0;
  uint16_t val = 0;
  
  I2C_Init();
  
  // set channel0 and trigger cont, full scale set to 2.048
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  config &= ~(0x7 << 12);
  config |= XMETER_ADC_MUX_CURRENT;
  config |= 0x8000; /* OS = 1*/
  
  config &= ~(0x7 << 9); 
  config |= (0x2 << 9); 
  
  //CDBG(("xmeter_update_current write %04x\n", config));
  
  I2C_Puts(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONFIG, 2, (uint8_t*)&config);
  
  if(!xmeter_wait_for_conv_ready()) {
    CDBG(("xmeter adc get current timeout\n"));
    return 0;
  }  
  
  I2C_Gets(XMETER_ADC_I2C_ADDR, XMETER_ADC_I2C_SUBADDR_CONV, 2, (uint8_t*)&val);
  
  return val;
  

}

static void xmeter_read_adc_current(void)
{
  uint16_t val;
  
  val = xmeter_get_adc_bits_current();
  
  xmeter_adc_current_f = xmeter_bits2value(
    val, 
    &xmeter_adc_current, 
    xmeter_adc_current_k, 
    xmeter_adc_current_b, XMETER_RES_CURRENT, 1);
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

static void xmeter_read_adc_voltage_out(void)
{
  uint16_t val;
  
  val = xmeter_get_adc_bits_voltage_out();
  
  xmeter_adc_voltage_out_f = xmeter_bits2value(
    val, 
    &xmeter_adc_voltage_out, 
    xmeter_adc_voltage_out_k, 
    xmeter_adc_voltage_out_b, XMETER_RES_VOLTAGE, 1); 
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

static void xmeter_read_adc_voltage_diss(void)
{
  
  uint16_t val;
  
  val = xmeter_get_adc_bits_voltage_diss();
  
  xmeter_adc_voltage_diss_f = xmeter_bits2value(
    val, 
    &xmeter_adc_voltage_diss, 
    xmeter_adc_voltage_diss_k, 
    xmeter_adc_voltage_diss_b, XMETER_RES_VOLTAGE, 1);   
  
  //CDBG(("adc vdis bits and float: %x %f\n", val, xmeter_adc_voltage_diss_f));
  //xmeter_dump_value("adc vdis", &xmeter_adc_voltage_diss, 1);
}

bit xmeter_read_adc_temp(void)
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
  
  xmeter_adc_temp_f = xmeter_adc_lookup_temp(val);
  
  xmeter_float2val(xmeter_adc_temp_f, &xmeter_adc_temp, XMETER_RES_TEMP);

  //CDBG(("adc temp bits and float: %x %f\n", val, xmeter_adc_temp_f));
  //xmeter_dump_value("adc temp", &xmeter_adc_temp, 1);
  
  return 1;
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
      task_set(EV_OVER_PD);
    }
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
  
  xmeter_assign_value(&xmeter_dac_preset_voltage[xmeter_dac_preset_voltage_index],
    &xmeter_dac_voltage);
}

void xmeter_prev_preset_dac_v(void)
{
  if(xmeter_dac_preset_voltage_index == 0) {
    xmeter_dac_preset_voltage_index = XMETER_PRESET_CNT;
  } else {
    xmeter_dac_preset_voltage_index 
      = (xmeter_dac_preset_voltage_index - 1);
  }
  
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
    xmeter_dac_preset_current_index = XMETER_PRESET_CNT;
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
  
  delta = coarse ? 0x400 : 1;
  
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
  
  xmeter_bits2value(bits, &xmeter_dac_voltage, 
    xmeter_dac_voltage_k, xmeter_dac_voltage_b, XMETER_RES_VOLTAGE, 0);
}

void xmeter_read_dac_current(void)
{
  /* read dac and translate bits to value */
  uint16_t bits = xmeter_get_dac_bits_c();
  
  xmeter_bits2value(bits, &xmeter_dac_current, 
    xmeter_dac_current_k, xmeter_dac_current_b, XMETER_RES_CURRENT, 0);
}

/* make xmeter_dac_** work! */
void xmeter_write_dac_voltage(void)
{
  double f;
  /* translate value to bis and write to dac */
  uint16_t bits = xmeter_value2bits(&xmeter_dac_voltage,
    &f, xmeter_dac_voltage_k, xmeter_dac_voltage_b, 0);
  
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
    &f, xmeter_dac_current_k, xmeter_dac_current_b, 0);
  
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