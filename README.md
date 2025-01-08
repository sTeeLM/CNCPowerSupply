## 一个隔离四通道的数控电源 ##
### 介绍 ###
![完成图](<Doc/Design/assets/IMG_5843.jpeg>)
每通道最大输出电压30V，电流5A，波纹小于0.1mV，各通道独立可串联或者并联，可以通过USB远程用电脑程序化控制。
由于采用了原始的线性降压设计，带着200VA的变压器，所以笨重无比。外壳采用酚醛树脂制作，第一次折腾AutoCAD，整出来浓浓的苏联风格。
成本量产500RMB以内，单个制作1000RMB左右。

### 主要功能 ###
1. 4路直流隔离，每路最大输出30V+5A，可长时间工作
2. 预设4挡电压电流值，可修改
3. 自动风扇控制，上下阈值可设置
4. 高耗散功率自动断开防烧毁
5. USB电脑远程控制和编程扩展
6. 在线校正和上位机校正

### TODO ###
1. 前置开关电源而不是变压器，以波纹换重量优化。
2. 完成USB上位机的控制程序，内嵌Python解释器。
3. 更好的校正设计甚至是自动校正。

## A CNC DC Power Supply with 4 independent channel ##
The maximum output voltage of each channel is 30V, the current is 5A, and the ripple is less than 0.1mV. all channels can be connected in series or parallel.
It can be programmed and controlled remotely via USB.
Because it uses an original linear step-down design with a 200VA transformer, it is extremely bulky. The shell is made of phenolic resin and has a strong Soviet style.

### TODO ###
1. use switching power supply plus linear convertor instead of the transformer for weight optimization.
2. Complete the control program on the USB host computer and embed the Python interpreter.
3. Better calibration design or even automatic calibration.
