#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <BME280_MOD-1022.h>
#include <Arduino_JSON.h>
#include <MHZ19.h>

// ネットワーク設定
const char *ssid = "";
const char *password = "";
IPAddress ip(192, 168, 0, );
ESP8266WebServer server(80);
const String name = "living";

// BME280
float temperature = 0.0;
float humidity    = 0.0;
float pressure    = 0.0;

// MHZ19
MHZ19 mhz19;

void readBME280()
{
  BME280.writeMode(smForced);
  while (BME280.isMeasuring()) {
    delay(1);
  }

  BME280.readMeasurements();
  temperature = BME280.getTemperature();
  humidity = BME280.getHumidity();
  pressure = BME280.getPressure();
}

void setup() {
  // BME280のセットアップ
  Wire.begin(0, 2);
  BME280.readCompensationParams();
  BME280.writeOversamplingTemperature(os1x);
  BME280.writeOversamplingHumidity(os1x);
  BME280.writeOversamplingPressure(os1x);
  readBME280();

  // MHZ19のセットアップ
  Serial.begin(9600); 
  mhz19.begin(Serial);
  // 24時間毎にその期間のCO2濃度の最低値を400ppmとみなしてキャリブレーションする機能。いらないので切る。
  mhz19.autoCalibration(false); 

  // WiFiネットワーク接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1);
  }

  // 固定IPアドレスの設定
  WiFi.config(ip, WiFi.gatewayIP(), WiFi.subnetMask());

  // WEBサーバー開始
  server.on("/", handle_OnConnect);
  server.on("/calibrate", co2Calibrate);
  server.on("/metrics", metric);
  server.begin();
}

// 雑関数
void add_metric(String *p, String metricName, String help, String value, String label = "") {
  *p += "# HELP " + metricName + " " + help + "\n";
  *p += "# TYPE " + metricName + " gauge\n";
  *p += metricName + label + " " + value + "\n";
}

void handle_OnConnect() {
  readBME280();

  JSONVar doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["pressure"] = pressure;
  doc["co2"] = mhz19.getCO2();
  server.send(200, "application/json", JSON.stringify(doc));
}

void metric() {
  readBME280();
  String body = "";
  
  String label = "{device_name=\"" + name + "\"}";

  add_metric(&body, "environment_temperature", "temperature(celsius)", String(temperature), label);
  add_metric(&body, "environment_humidity", "humidity(percent)", String(humidity), label);
  add_metric(&body, "environment_pressure", "pressure(hPa)", String(pressure), label);
  add_metric(&body, "environment_co2", "co2(ppm)", String(mhz19.getCO2()), label);
  server.send(200, "text/plain", body);
}

void co2Calibrate() {
  mhz19.calibrate();
  server.send(200, "text/plain", "Calibrate complete!");
}

void loop() {
  server.handleClient();
}
