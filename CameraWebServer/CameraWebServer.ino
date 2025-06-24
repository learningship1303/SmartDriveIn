#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// I2C and OLED
#define I2C_SDA 15
#define I2C_SCL 14
TwoWire I2Cbus = TwoWire(0);
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2Cbus, OLED_RESET);

// WiFi and server info
const char* ssid = "Divyanshu";         // Replace with your WiFi SSID
const char* password = "abcd@1234";     // Replace with your WiFi Password
String serverName = "www.circuitdigest.cloud";
String serverPath = "/readnumberplate";
const int serverPort = 443;
String apiKey = "jou6vSukhW2y";            // Replace with your API key

#define triggerButton 13
#define flashLight 4
int count = 0;
WiFiClientSecure client;

// Camera pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Utility: Extract a string from JSON-like response
String extractJsonStringValue(const String& jsonString, const String& key) {
  int keyIndex = jsonString.indexOf(key);
  if (keyIndex == -1) return "";
  int startIndex = jsonString.indexOf(':', keyIndex) + 2;
  int endIndex = jsonString.indexOf('"', startIndex);
  if (startIndex == -1 || endIndex == -1) return "";
  return jsonString.substring(startIndex, endIndex);
}

// Display helper
void displayText(String text) {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(text);
  display.display();
}

// Send parsed response to local server
void sendResponseToServer(String response) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("http://192.168.127.56:3000/update-entry");
    http.addHeader("Content-Type", "application/json");

    String numberPlate = extractJsonStringValue(response, "\"number_plate\"");
    // float  plateX = extractJsonStringValue(response, "\"plate_Xcenter\"");
    // float  plateY = extractJsonStringValue(response, "\"plate_Ycenter\"");
    String viewImage = extractJsonStringValue(response, "\"view_image\"");

    String jsonData = "{\"data\":{";
    jsonData += "\"message\":\"ANPR successful\",";
    jsonData += "\"number_plate\":\"" + numberPlate + "\",";
    // jsonData += "\"plate_Xcenter\":" + String(plateX) + ",";
    // jsonData += "\"plate_Ycenter\":" + String(plateY) + ",";
    jsonData += "\"view_image\":\"" + viewImage + "\"";
    jsonData += "},";
    jsonData += "\"error\":null,";
    jsonData += "\"status\":\"success\"";
    jsonData += "}";

    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode > 0) {
      Serial.println("HTTP Response code: " + String(httpResponseCode));
      Serial.println("Server Response: " + http.getString());
    } else {
      Serial.println("Error in sending POST request: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  pinMode(flashLight, OUTPUT);
  pinMode(triggerButton, INPUT);
  digitalWrite(flashLight, LOW);

  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 5;
    config.fb_count = 2;
    Serial.println("PSRAM found");
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  I2Cbus.begin(I2C_SDA, I2C_SCL, 100000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init failed");
    while (true);
  }

  displayText("System Initialization Successful");
  delay(1000);
  displayText("Press Trigger Button \n\nto Start Capturing");
}

void loop() {
  if (digitalRead(triggerButton) == HIGH) {
    int status = sendPhoto();
    if (status == -1) displayText("Image Capture Failed");
    else if (status == -2) displayText("Server Connection Failed");
  }
}

int sendPhoto() {
  camera_fb_t* fb = NULL;
  delay(100);
  fb = esp_camera_fb_get();
  delay(100);
  if (!fb) {
    Serial.println("Camera capture failed");
    return -1;
  }

  displayText("Image Capture Success");
  delay(300);

  Serial.println("Connecting to server:" + serverName);
  displayText("Connecting to server:\n\n" + serverName);
  client.setInsecure();

  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    displayText("Connection successful!");
    delay(300);
    displayText("Data Uploading !");

    count++;
    String filename = apiKey + ".jpeg";
    String head = "--CircuitDigest\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + filename + "\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--CircuitDigest--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t totalLen = head.length() + tail.length() + imageLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=CircuitDigest");
    client.println("Authorization:" + apiKey);
    client.println();
    client.print(head);

    uint8_t* fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      } else {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);
    esp_camera_fb_return(fb);

    displayText("Waiting For Response!");

    String response;
    long startTime = millis();
    while (client.connected() && millis() - startTime < 5000) {
      if (client.available()) {
        char c = client.read();
        response += c;
      }
    }

    Serial.print("Response: ");
    Serial.println(response);
    client.stop();

    String NPRData = extractJsonStringValue(response, "\"number_plate\"");
    displayText("NPR Data:\n\n" + NPRData);

    sendResponseToServer(response);
    return 0;
  } else {
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    return -2;
  }
}