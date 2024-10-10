
## 交互总体原则 
1. 交互符合直觉，逻辑前后尽量一致（我也曾经是个产品经理。。）  
两个按钮的用法，尽量按照符合直觉操作习惯设计  
1. mod：短按是模式切换，旋转是电压调整、数值粗调  
1. set：短按是数值选定，旋转是电流调整、数值精调  
1. 长按是特殊功能  

## 事件
|名称|含义|
|--|--|
250ms| 时钟脉冲250ms
1s| 时钟脉冲1s
Mu|Mod Up  
Md| Mod Down  
Ms| Mod Short Press  
Ml| Mod Long Press  
Mc| Mod Clock wise  
Mcc| Mod Counter Clock wise  
Su| Set Up  
Sd| Set Down  
Ss| Set Short Press  
Sl| Set Long Press  
Sc| Set Clock wise  
Scc| Set Counter Clock wise  
MSs| Mod+Set Press  
MSl| Mod+Set Long Press  
OVER_HEAT|过热  
OVER_PD|过载  
TEMP_HI| 温度超过阈值  
TEMP_LO| 温度低于阈值  
EV_TIMEO| 超时  

## 功能1 XMeter
**主功能，电压、电流、功率、温度显示、设置、过载保护**
### Main
**显示V、C、P**
|事件|动作|
|--|--|
Ms|进入PickV  
Ss|进入PickC  
Ml|进入Aux0  
Sl|进入Param功能  
MSs|进入校准功能  
Mc/Mcc|进入SetV  
Sc/Scc|进入SetC  
250ms|从ADC更新一遍计数并刷新LCD  
OVER_HEAT|关闭电源，打开风扇，进入OverHeat  
OVER_PD|关闭电源，进入OverPd  
TEMP_HI|开风扇  
TEMP_LO|关风扇  

### Aux0
**显示Vi、Pd**
|事件|动作|
|---|---|
250ms|从ADC更新一遍计数并刷新LCD  
Ms| 回到Main  
EV_TIMEO| 回到Main  
Mc/Mcc| Aux0/Aux1切换  
Sc/Scc| Aux0/Aux1切换  
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat  
OVER_PD| 关闭电源，进入OverPd  
TEMP_HI| 开风扇  
TEMP_LO|关风扇  

### Aux1
**显示T、Fan**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Ms| 回到Main
EV_TIMEO| 回到Main
Mc/Mcc| Aux0/Aux1切换
Sc/Scc| Aux0/Aux1切换
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat
OVER_PD| 关闭电源，进入OverPd
TEMP_HI| 开风扇
TEMP_LO|关风扇

### PickPreV
**选择预设置电压**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Mc/Mcc| 切换预设电压
Ms| 设置预设电压并返回Main **特殊不符合约定的设计，但是更符合直觉**
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat
OVER_PD| 关闭电源，进入OverPd
TEMP_HI| 开风扇
TEMP_LO| 关风扇
EV_TIMEO| 放弃设置回到Main

### PickPreC
**选择预设值电流**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Sc/Scc| 切换预设电流
Ss| 设置预设电流并返回Main
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat
OVER_PD| 关闭电源，进入OverPd
TEMP_HI| 开风扇
TEMP_LO|关风扇
EV_TIMEO| 放弃设置回到Main

### SetV
**设置电压**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Mc/Mcc| 粗粒度设置电压（100mV）
Sc/Scc| 细粒度设置电压（小数最末有效数字，最小1mV）
Ms| 设置预设电压并返回Main
EV_TIMEO| 放弃设置回到Main
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat
OVER_PD| 关闭电源，进入OverPd
TEMP_HI| 开风扇
TEMP_LO|关风扇

### SetC
**设置电流**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Mc/Mcc| 粗粒度设置电流（100mA）
Sc/Scc| 细粒度设置电流（小数最末有效数字，最小1mA）
Ss| 设置预设电流并返回Main
EV_TIMEO| 放弃设置回到Main
OVER_HEAT| 关闭电源，打开风扇，进入OverHeat
OVER_PD| 关闭电源，进入OverPd
TEMP_HI| 开风扇
TEMP_LO| 关风扇

### OverHeat
**过热状态**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Ss| 开始一次检测，如果温度低于TEMP_LO发送EV_TIMEO消息
EV_TIMEO| 打开电源并回到Main
TEMP_HI| 开风扇
TEMP_LO| 关闭风扇

### OverPd
**过载状态**
|事件|动作|
|---|---|
250ms| 从ADC更新一遍计数并刷新LCD
Ss| 打开电源并回到Main
TEMP_HI| 开风扇
TEMP_LO| 关闭风扇

## 功能2 参数设置 Set Param
> 辅助功能，设置各种参数，交互逻辑是分为4个大功能块  
> 每个功能块入口都可以通过Sl回到XMeter主界面  
> 功能块入口Ms切换功能块，Ss数值设置  
> 这样设计本质是把一个大的平铺线性向导，分割成了几个子向导  
> 避免了一次设置必须要全部经过一遍的囧境  
1. 保护相关的参数设置
2. 预设电压
3. 预设电流
4. 其他杂项参数
### Fan（功能块入口1）
**风扇温度上下限**
|事件|动作|
|---|---|
Mc/Mcc| 开启温度上限（以0.5 C）
Sc/Scc| 关闭温度下限（以0.5 C）
Ss| 设置温度上下限，并进入OverProtect
Ms| 进入SelPreV
Sl| 回到Main

### 过载保护OverProtect
**过载保护温度和功率**
|事件|动作|
|---|---|
Mc/Mcc| 过载保护温度（以0.5 C）
Sc/Scc| 最大耗散功率（以0.5 w）
Ss|  设置参数，并回到Fan

### SelPreV（功能块入口2）
**切换预设电压**
|事件|动作|
|---|---|
Mc/Mcc| 切换预设电压index
Sc/Scc| 切换预设电压index
Ss| 选定index并进入SetPreV
Ms| 进入SelPreC

### SetPreV
**设置预设电压**
|事件|动作|
|---|---|
Mc/Mcc| 粗调电压（100mV）
Sc/Scc| 精调电压（小数最末有效数字，最小1mV）
Ss| 设置并回到SelPreV

### SelPreC （功能块入口3）
**切换预设电流**
|事件|动作|
|---|---|
Mc/Mcc| 切换预设电流index
Sc/Scc| 切换预设电流index
Ss| 选定index进入SetPreC
Ms| 进入Beeper

### SetPrevC
**设置预设电流**
|事件|动作|
|---|---|
Mc/Mcc| 粗调电流（100mV）
Sc/Scc| 精调电流（小数最末有效数字，最小1mA）
Ss| 设置并回到SelPreC

### Beeper （功能块入口4）
**开关Beeper**
|事件|动作|
|---|---|
Mc/Mcc| Beeper On/Off切换
Mc/Mcc| Beeper On/Off切换
Ss| 设置Beeper并进入Fan设置
Ms| 回到Fan，但是不保存设置

## 功能3 Calibration
**辅助功能，校准功率平面的零值和最大值**
**过载保护功能完全关闭**
**风扇完全关闭**
三个阶段：  
1. 物理调整（手工）
2. 参数标定（手工）
3. 参数计算（自动）
阶段1可调节的几个物理电位器:  
- A. 电压DAC参考电压电位器（控制电压零点）
- B. 电压反馈分压电位器(控制电压最大值)
- C. 输出电压测量分压电位器（测量输出电压最大值）
- D. 输入电压测量分压电位器（测量输入电压最大值）
- E. 电流DAC参考电压电位器（控制电流最大值）
- F. 输出电流测两分压电位器（测量输出电流最大值）
- G. 两个三段电压继电器对应的电位器（控制2个切换输入电压）
### CalPhyVoltage
**电压物理调整，调整目标**
- 三段电压继电器正常工作（调节G）
- 电压DAC在0点对应输出0V，0xFFFF对应输出30V（调节A、B）
- 输出电压ADC在30V读数07FFF（调节C）
- 输入电压ADC在最小输入电压读数（需要记录此时的输入电压数值）
- 输入电压ADC在07FFF对应最大输入电压读数（需要记录此时的输入电压数值）（调节D）

|事件|动作|
|---|---|
Mc/Mcc| 粗调电压电位器读数（16LSB）
Sc/Scc| 精调电压电位器读数（1LSB）
Ss| 切换到CalPhyCurrent
### CalPhyCurrent
**电流物理调整，调整目标（注意一定把输出电压设置位5V左右，然后短路输出！）**
- 最大电流输出5A，电流DAC对应0xFFFF（调节E）
- 输出电流ADC在5A读数刚好是0x7FFF（调节F）

|事件|动作|
|---|---|
Mc/Mcc| 粗调电压电位器读数（16LSB）
Sc/Scc| 精调电压电位器读数（1LSB）
Ss| 切换到CalPhyVoltage
Ms| 切换到CalVoltageZero
### CalVoltageZero
**标定输出电压零点**
- 调整到电压输出0V，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调电压电位器读数（16LSB）
Sc/Scc| 精调电压电位器读数（1LSB）
Ss| 记录0点，切换到CalPhyVoltageZeroIn
### CalVoltageZeroIn
**标定输入电压最小值**
- 输入对应输出电压零点的输入电压数值，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调输入电压（100mv）
Sc/Scc| 精调输入电压（1mv）
Ss| 记录0点，切换到CalPhyVoltageMax
### CalVoltageMax
**标定输出电压最大值**
- 调整到电压输出30V，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调电压电位器读数（16LSB）
Sc/Scc| 精调电压电位器读数（1LSB）
Ss| 记录最大点，切换到CalPhyVoltageMaxIn
### CalVoltageZeroIn
**标定输入电压最大值**
- 输入对应输出电压最大值的输入电压数值，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调输入电压（100mv）
Sc/Scc| 精调输入电压（1mv）
Ss| 记录最大点，切换到CalPhyCurrentZero
### CalCurrentZero
**标定输入、输出电流零点**
- 调整到电流输出0A，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调电流电位器读数（16LSB）
Sc/Scc| 精调电流电位器读数（1LSB）
Ss| 切换到CalPhyCurrentMax
### CalCurrentMax
**标定输入、输出电流最大值**
- 调整到电流输出5A，按下Ss

|事件|动作|
|---|---|
Mc/Mcc| 粗调电流电位器读数（16LSB）
Sc/Scc| 精调电流电位器读数（1LSB）
Ss | 记录读数并进入CalBK
### CalBK
**自动校准输入电压ADC和电流ADC，以及输出电压ADC**
|事件|动作|
|---|---|
1s |依次校准，如果完成, 写入rom
Ss | 如果校准完成或者失败发送EV_TIMEO
EV_TIMEO| 回到XMeter.main

