
//D4 GPIO2 LED_BUILTIN 会引起不停闪烁
//D8 GPIO15 会引起连接不上nodemcu

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

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
#define IR_LED 4  // D2 GPIO 4
IRCoolixAC coolix(IR_LED);

#define DHT_PIN 12  // D6 GPIO12 
SimpleDHT11 dht11(DHT_PIN);

// 网络和MQTT 设置
WiFiClient espClient;
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_user = "jonas";
const char* mqtt_password = "abc123";
PubSubClient client(espClient);
const char* mqttSubscribeTopic = "switch";
const char* mqttSubscribeTopic1 = "weather";

// 定时器回调设置
Ticker publishDataTicker;
Ticker subscribeDataTicker;

// 定义定时器回调函数
void publishDataTask();
void subscribeDataTask();

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived:");
  Serial.println(topic);
  if (strcmp(topic, mqttSubscribeTopic) == 0) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
      message += (char)payload[i];
    }
    Serial.println(message);

    // 将json 字符串解析为json 对象并取出信息
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, message);
    JsonObject obj = doc.as<JsonObject>();
    String state = obj["state"];

    Serial.println(state);

    if (state.equals("on")) {
      coolix.on();
      coolix.send();
    } else if (state.equals("off")) {
      coolix.off();
      coolix.send();
    }
  }
}

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  coolix.begin();

  WiFiManager wifiManager;
  // wifiManager.resetSettings();  //清除上一次保存的WIFI账号和密码
  wifiManager.autoConnect("esp8266-auto");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // client.setKeepAlive(20); // 默认是15s 心跳

  while (!client.connected()) {
    Serial.println("连接MQTT服务器中...");
    // mqttClient.connect(clientId.c_str(), mqttId, mqttPwd, willTopic, willQoS, willRetain, willMsg)；
    if (client.connect("完全新的的clientId", mqtt_user, mqtt_password)) {
      Serial.println("连接MQTT成功");
    } else {
      Serial.print("连接失败： ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  // 发布 监听
  publishDataTicker.attach_ms(20000, publishDataTask); // 温度湿度 20s 发布一次

  // 订阅 MQTT 主题
  client.subscribe(mqttSubscribeTopic);
  // 这个会关联 callback => 影响IR 发射器的响应时间
  subscribeDataTicker.attach_ms(200, subscribeDataTask);  
}

// 定时器回调函数，发布温湿度数据到 MQTT
void publishDataTask() {
  // 读取温湿度数据
  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
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
  client.publish(mqttSubscribeTopic1, sendJson, false);
}

// 定时器回调函数，检查 MQTT 订阅的消息
void subscribeDataTask() {
  // 检查 MQTT 订阅的消息
  client.loop();
}

void loop() {
}
