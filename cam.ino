#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Ticker.h>

// WiFi va Telegram
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* botToken = "YOUR_BOT_TOKEN";
const char* chatID = "YOUR_CHAT_ID";

// Kamera konfiguratsiyasi (AI-Thinker ESP32-CAM)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_GPIO_NUM    4   // chiroq pin

Ticker photoTicker;       
unsigned long lastSendTime = 0;
const int intervalSend = 60000; // 1 daqiqa
int photoCount = 0;
String photoFolder = "/photos";

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected!");

  // SD kartani ishga tushirish
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  if(!SD_MMC.exists(photoFolder)) SD_MMC.mkdir(photoFolder);

  // Kamera init
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if(esp_camera_init(&config) != ESP_OK){
    Serial.println("Camera init failed");
    return;
  }

  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW); // flash o‘chiriladi

  photoTicker.attach(1, takePhoto);
}

void loop() {
  if(millis() - lastSendTime > intervalSend){
    lastSendTime = millis();
    sendPhotosToTelegram();
  }
}

void takePhoto() {
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) return;

  String fileName = photoFolder + "/photo_" + String(photoCount) + ".jpg";
  File file = SD_MMC.open(fileName, FILE_WRITE);
  if(file){
    file.write(fb->buf, fb->len);
    file.close();
    Serial.println("Saved: " + fileName);
    photoCount++;
  } else {
    Serial.println("Failed to save photo");
  }

  esp_camera_fb_return(fb);
}

void sendPhotosToTelegram() {
  if(photoCount == 0) return;

  WiFiClientSecure client;
  client.setInsecure(); // sertifikatni tekshirmaydi

  for(int i = 0; i < photoCount; i++){
    String path = photoFolder + "/photo_" + String(i) + ".jpg";
    File file = SD_MMC.open(path);
    if(!file) continue;

    String boundary = "----ESP32CAMBoundary";
    String header = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String footer = "\r\n--" + boundary + "--\r\n";

    HTTPClient http;
    http.begin(client, "https://api.telegram.org/bot" + String(botToken) + "/sendPhoto?chat_id=" + chatID);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    int httpCode = http.sendRequest("POST", (uint8_t*)NULL, 0); // multipart bodyni yuborish uchun kutubxona kerak
    Serial.println("Sent: " + String(httpCode));

    http.end();
    file.close();
    SD_MMC.remove(path);
  }
  photoCount = 0;
}
