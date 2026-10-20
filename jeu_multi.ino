#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

#define BUTTON_A D10
#define BUTTON_SELECT D7
#define PIN_LED D0

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_NeoPixel rgb_led(1, PIN_LED, NEO_GRB + NEO_KHZ800);

// Adresse de diffusion pour envoyer à tous les ESP32 à portée
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool hasBall = false;
bool lastBtnA = HIGH;

// Structure des données transmises
typedef struct struct_message {
  bool ballPassed;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Fonction exécutée lors de la réception d'un message ESP-NOW
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  if (myData.ballPassed) {
    hasBall = true;
  }
}

void setup() {
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  rgb_led.begin();
  rgb_led.setBrightness(30);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  // Configuration du Wi-Fi en mode Station requis pour ESP-NOW
  WiFi.mode(WIFI_STA);
  
  if (esp_now_init() != ESP_OK) {
    return;
  }
  
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Enregistrement du pair (broadcast)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop() {
  bool currentBtnA = digitalRead(BUTTON_A);
  bool currentBtnSel = digitalRead(BUTTON_SELECT);

  // Initialisation de la balle sur la carte actuelle
  if (currentBtnSel == LOW) {
    hasBall = true;
  }

  // Envoi de la balle si le bouton A est pressé
  if (currentBtnA == LOW && lastBtnA == HIGH && hasBall) {
    hasBall = false;
    myData.ballPassed = true;
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  }
  lastBtnA = currentBtnA;

  // Mise à jour de l'affichage OLED et de la LED RGB
  display.clearDisplay();
  display.setCursor(0, 20);
  
  if (hasBall) {
    display.print(F("BALLE ICI"));
    rgb_led.setPixelColor(0, rgb_led.Color(0, 255, 0)); // Vert
  } else {
    display.print(F("ATTENTE..."));
    rgb_led.setPixelColor(0, rgb_led.Color(255, 0, 0)); // Rouge
  }
  
  rgb_led.show();
  display.display();
  
  delay(20); 
}