## 硬件准备
- Nodemcu v1 esp12e
- DHT11 温度湿度模块
- 38kHZ 红外发射和接收模块
- 杜邦线若干
- 面包板
- 锂电池充电宝 3.7v
- USB -> mini USB 线

## 驱动准备
- CP201X 取代 CH340

## IDE 准备
- vscode + platformio
- 之前用的arduino

## 安装库 
- 见代码注释

## 开发语言
- c++

## 作用
利用MQTT 协议吧温度湿度模块的数据传递给MQTT服务器，然后通过客户端订阅实时得到数据；并推送开关空调指令给MQTT服务器远程控制空调开关