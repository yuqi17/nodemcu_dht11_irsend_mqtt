
// D4 GPIO2 LED_BUILTIN 会引起不停闪烁
// D8 GPIO15 会引起连接不上nodemcu
#include <ESP8266WiFi.h>

// 第三方库
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ir_Coolix.h>
#include <SimpleDHT.h>

// 红外发射引脚设置
#define IR_LED 4 // D2 GPIO 4
IRCoolixAC coolix(IR_LED);

// 温度湿度引脚设置
#define DHT_PIN 12 // D6 GPIO12
SimpleDHT11 dht11(DHT_PIN);

// wifi 配网
WiFiManager wifiManager;

// MQTT设置
WiFiClient espClient;
const char *mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char *mqtt_user = "jonas";
const char *mqtt_password = "abc123";
PubSubClient client(espClient);
const char *switchSubscribeTopic = "switch";
const char *weatherPublishTopic = "weather";
const char *switchPublishTopic = "switch/feedback";

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
  Serial.println("准备发布温度湿度...");
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
}

void connectMQTTServer()
{
  // 根据ESP8266的MAC地址生成客户端ID（避免与其他设备产生冲突）
  String clientId = "你的设备clientId";
  const char *willTopic = "willDead";
  const int willQoS = 2;
  const bool willRetain = true;
  const char *willMsg = "CLIENT-OFFLINE";

  // 连接MQTT服务器
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, willTopic, willQoS, willRetain, willMsg))
  {
    Serial.println("MQTT 连接成功.");
    client.subscribe(switchSubscribeTopic);
  }
  else
  {
    Serial.println("MQTT 连接失败,状态码: ");
    Serial.println(client.state());
    delay(2000);
  }
}

void setup()
{
  Serial.begin(115200);
  coolix.begin();

  // wifiManager.resetSettings();  //清除上一次保存的WIFI账号和密码
  wifiManager.autoConnect("esp8266-auto");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(50); // 默认是15s 心跳

  while (!client.connected())
  {
    connectMQTTServer();
  }

  for(int i = 0;i < 10; i++){
    publishDataTask();
    client.loop();    
    delay(3000);
  }

  Serial.println("准备120s的休眠");
  ESP.deepSleep(120e6);
}

void loop()
{
  Serial.println("loop...");
}
