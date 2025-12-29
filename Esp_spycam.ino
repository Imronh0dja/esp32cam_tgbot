#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SD_MMC.h>
#include <Ticker.h>

// ===== WIFI & TELEGRAM =====
const char* ssid = "Innbox";
const char* password = "INNBOX";
const char* botToken = "BOT_TOKEN_BU_YERGA";
const char* chatID   = "CHAT_ID_BU_YERGA";

// ===== ESP32-CAM PINS =====
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

// ===== SETTINGS =====
const String photoFolder = "/photos";
int photoCount = 0;
const int maxPhotos = 10;
const char* zipName = "/photos.zip";

Ticker photoTicker;

// ===== Minimal ZIP struct =====
struct SimpleZip {
  File zipFile;
};
SimpleZip zip;

// ===== Setup =====
void setup() {
  Serial.begin(115200);

  // SD init
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("SD CARD XATO!");
  } else {
    if(!SD_MMC.exists(photoFolder)) SD_MMC.mkdir(photoFolder);
    Serial.println("SD karta ulandi ✅");
  }

  // Camera init
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if(esp_camera_init(&config) != ESP_OK){
    Serial.println("KAMERA XATO!");
    return;
  }

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("WiFi ulanmoqda");
  while (WiFi.status() != WL_CONNECTED){
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi ulandi");

  photoTicker.attach(1, takePhoto); // 1 soniya
}

// ===== Loop =====
void loop() {
  if(photoCount >= maxPhotos){
    createZip();
    sendZipToTelegram();
    clearPhotos();
    photoCount = 0;
  }
}

// ===== Take photo =====
void takePhoto() {
  camera_fb_t* fb = esp_camera_fb_get();
  if(!fb){
    Serial.println("Rasm olinmadi!");
    return;
  }

  String fileName = photoFolder + "/photo_" + String(photoCount) + ".jpg";
  File f = SD_MMC.open(fileName, FILE_WRITE);
  if(f){
    f.write(fb->buf, fb->len);
    f.close();
    Serial.println("Saved: " + fileName);
    photoCount++;
  }
  esp_camera_fb_return(fb);
}

// ===== Create ZIP =====
void createZip() {
  Serial.println("ZIP yaratilyapti...");
  zip.zipFile = SD_MMC.open(zipName, FILE_WRITE);
  if(!zip.zipFile){
    Serial.println("ZIP yaratib bo‘lmadi!");
    return;
  }

  for(int i=0; i<maxPhotos; i++){
    String path = photoFolder + "/photo_" + String(i) + ".jpg";
    File f = SD_MMC.open(path);
    if(f){
      while(f.available()){
        zip.zipFile.write(f.read());
      }
      f.close();
    }
  }
  zip.zipFile.close();
  Serial.println("ZIP tayyor: " + String(zipName));
}

// ===== Send ZIP to Telegram =====
void sendZipToTelegram(){
  Serial.println("Telegramga yuborilmoqda...");

  File zipFile = SD_MMC.open(zipName);
  if(!zipFile){
    Serial.println("ZIP topilmadi!");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();



  String boundary = "ESP32ZIP";
  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"document\"; filename=\"photos.zip\"\r\n"
    "Content-Type: application/zip\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  client.connect("api.telegram.org", 443);
  client.print(
    "POST /bot" + String(botToken) + "/sendDocument?chat_id=" + chatID + " HTTP/1.1\r\n"
    "Host: api.telegram.org\r\n"
    "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
    "Content-Length: " + String(head.length() + zipFile.size() + tail.length()) + "\r\n\r\n"
  );

  client.print(head);
  while(zipFile.available()) client.write(zipFile.read());
  client.print(tail);

  zipFile.close();
  SD_MMC.remove(zipName);

  Serial.println("Telegramga yuborildi 🌟");
}

// ===== Clear photos =====
void clearPhotos(){
  for(int i=0; i<maxPhotos; i++){
    String path = photoFolder + "/photo_" + String(i) + ".jpg";
    SD_MMC.remove(path);
  }
  Serial.println("SD ichi tozalandi");
}
