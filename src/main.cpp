
// D4 GPIO2 LED_BUILTIN 会引起不停闪烁
// D8 GPIO15 会引起连接不上nodemcu

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

// 第三方库
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <Ticker.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Coolix.h>
#include <SimpleDHT.h>

// 引脚设置
#define IR_LED 4 // D2 GPIO 4
IRCoolixAC coolix(IR_LED);

#define DHT_PIN 12 // D6 GPIO12
SimpleDHT11 dht11(DHT_PIN);

// 网络和MQTT 设置
WiFiClient espClient;
const char *mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char *mqtt_user = "jonas";
const char *mqtt_password = "abc123";
PubSubClient client(espClient);
const char *switchSubscribeTopic = "switch";
const char *weatherPublishTopic = "weather";
const char *switchPublishTopic = "switch/feedback";

WiFiUDP Udp;                       // 实例化WiFiUDP对象
unsigned int localUdpPort = 1234;  // 自定义本地监听端口
unsigned int remoteUdpPort = 4321; // 自定义远程监听端口
char incomingPacket[255];          // 保存Udp工具发过来的消息

// 定时器回调设置
Ticker publishDataTicker;
Ticker subscribeDataTicker;
Ticker udpTicker;

// 定义定时器回调函数 定义之后就可以把方法写在后面的位置
void publishDataTask();
void subscribeDataTask();
void callback(char *topic, byte *payload, unsigned int length);
void udpDataTask();

void setup()
{
  // pinMode(LED_BUILTIN, OUTPUT); 不设置默认的低电平会让led 一直亮
  Serial.begin(115200);
  coolix.begin();

  WiFiManager wifiManager;
  // wifiManager.resetSettings();  //清除上一次保存的WIFI账号和密码
  wifiManager.autoConnect("esp8266-auto");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // client.setKeepAlive(20); // 默认是15s 心跳

  while (!client.connected())
  {
    Serial.println("连接MQTT服务器中...");
    // mqttClient.connect(clientId.c_str(), mqttId, mqttPwd, willTopic, willQoS, willRetain, willMsg)；
    const char *willTopic = "willDead";
    const int willQoS = 2;
    const bool willRetain = true;
    const char *willMsg = "CLIENT-OFFLINE";

    if (client.connect("完全新的的clientId", mqtt_user, mqtt_password, willTopic, willQoS, willRetain, willMsg))
    {
      Serial.println("连接MQTT成功");
    }
    else
    {
      Serial.print("连接失败： ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  // 发布 监听
  publishDataTicker.attach_ms(5000, publishDataTask); // 温度湿度 2s 发布一次

  // 订阅 MQTT 主题
  client.subscribe(switchSubscribeTopic);
  // 这个会关联 callback => 影响IR 发射器的响应时间
  subscribeDataTicker.attach_ms(200, subscribeDataTask);

  if (Udp.begin(localUdpPort))
  { // 启动Udp监听服务
    Serial.println("UDP 监听成功");
    // 打印本地的ip地址，在UDP工具中会使用到
    // WiFi.localIP().toString().c_str()用于将获取的本地IP地址转化为字符串
    Serial.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), localUdpPort);
  }
  else
  {
    Serial.println("UDP 监听失败");
  }
  udpTicker.attach_ms(200, udpDataTask);
}

// 向udp工具发送消息
void sendCallBack(const char *buffer)
{
  Udp.beginPacket(Udp.remoteIP(), remoteUdpPort); // 配置远端ip地址和端口
  Udp.write(buffer);                              // 把数据写入发送缓冲区
  Udp.endPacket();                                // 发送数据
}

void udpDataTask()
{
  int packetSize = Udp.parsePacket(); // 获得解析包
  if (packetSize)                     // 解析包不为空
  {
    // 收到Udp数据包
    // Udp.remoteIP().toString().c_str()用于将获取的远端IP地址转化为字符串
    Serial.printf("收到来自远程IP：%s（远程端口：%d）的数据包字节数：%d\n", Udp.remoteIP().toString().c_str(), Udp.remotePort(), packetSize);

    // 读取Udp数据包并存放在incomingPacket
    int len = Udp.read(incomingPacket, 255); // 返回数据包字节数
    if (len > 0)
    {
      incomingPacket[len] = 0;                                // 清空缓存
      Serial.printf("UDP数据包内容为: %s\n", incomingPacket); // 向串口打印信息

      // strcmp函数是string compare(字符串比较)的缩写，用于比较两个字符串并根据比较结果返回整数。
      // 基本形式为strcmp(str1,str2)，若str1=str2，则返回零；若str1<str2，则返回负数；若str1>str2，则返回正数。
      if (strcmp(incomingPacket, "LED_OFF") == 0) // 命令LED_OFF
      {
        coolix.off();
        coolix.send();
        sendCallBack("LED has been turn off\n");
      }
      else if (strcmp(incomingPacket, "LED_ON") == 0) // 如果收到LED_ON
      {
        coolix.on();
        coolix.send();
        sendCallBack("LED has been turn on\n");
      }
      else // 如果指令错误，调用sendCallBack
      {
        sendCallBack("Command Error!");
      }
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.println(topic);
  if (strcmp(topic, switchSubscribeTopic) == 0)
  {
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
      message += (char)payload[i];
    }

    Serial.println(message);

    // 将json 字符串解析为json 对象并取出信息
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, message);
    JsonObject obj = doc.as<JsonObject>();
    String state = obj["state"];

    if (state.equals("on"))
    {
      coolix.on();
      coolix.send();
      const char *feedback = "openned";
      client.publish(switchPublishTopic, feedback, false);
    }
    else if (state.equals("off"))
    {
      coolix.off();
      coolix.send();
      const char *feedback = "shut down";
      client.publish(switchPublishTopic, feedback, false);
    }
  }
}

// 定时器回调函数，发布温湿度数据到 MQTT
void publishDataTask()
{

  // 读取温湿度数据
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess)
  {
    Serial.print("温湿度传感器异常:");
    Serial.print(SimpleDHTErrCode(err));
    Serial.print(",");
    Serial.println(SimpleDHTErrDuration(err));
    delay(1000);
    return;
  }

  // 将温湿度数据转换为json字符串
  DynamicJsonDocument data(256);
  data["temperature"] = String(temperature);
  data["humidity"] = String(humidity);

  char sendJson[256];
  serializeJson(data, sendJson);
  // 发布温湿度数据到 MQTT
  client.publish(weatherPublishTopic, sendJson, false);

  sendCallBack(sendJson);
}

// 定时器回调函数，检查 MQTT 订阅的消息
void subscribeDataTask()
{
  // 检查 MQTT 订阅的消息
  client.loop();
}

void loop()
{
}
