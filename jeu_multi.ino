#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- GESTION MATÉRIELLE ---
const uint8_t BROCHES[] = {D0, D1, D2, D6, D7, D8, D9, D10};
const int NB = sizeof(BROCHES);

struct Bouton { int8_t sortie; int8_t entree; };
Bouton btnTirer;
Bouton btnReset;

bool baseG[8];
bool base[8][8];

bool versGnd(int i) {
  pinMode(BROCHES[i], INPUT_PULLUP);
  delayMicroseconds(50);
  bool r = (digitalRead(BROCHES[i]) == LOW);
  pinMode(BROCHES[i], INPUT);
  return r;
}

bool relie(int sortie, int entree) {
  pinMode(BROCHES[sortie], OUTPUT);
  digitalWrite(BROCHES[sortie], LOW);
  pinMode(BROCHES[entree], INPUT_PULLUP);
  delayMicroseconds(50);
  bool r = (digitalRead(BROCHES[entree]) == LOW);
  pinMode(BROCHES[sortie], INPUT);
  pinMode(BROCHES[entree], INPUT);
  return r;
}

bool appuye(const Bouton &b) {
  if (b.sortie < 0) return versGnd(b.entree);
  return relie(b.sortie, b.entree);
}

bool cherche(Bouton &res) {
  for (int i = 0; i < NB; i++) {
    if (!baseG[i] && versGnd(i)) {
      res = {-1, (int8_t)i}; return true;
    }
  }
  for (int o = 0; o < NB; o++) {
    for (int i = 0; i < NB; i++) {
      if (o != i && !base[o][i] && relie(o, i)) {
        res = {(int8_t)o, (int8_t)i}; return true;
      }
    }
  }
  return false;
}

void calibrerBouton(const char* nom, Bouton &b) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10); display.print("Appuie sur :");
  display.setTextSize(2);
  display.setCursor(0, 30); display.print(nom);
  display.display();
  
  while (!cherche(b)) delay(10);
  
  display.clearDisplay();
  display.setCursor(0, 30); display.print("OK !");
  display.display();
  
  Bouton tmp;
  delay(200);
  while (cherche(tmp)) delay(10);
  delay(200);
}

// --- MULTIJOUEUR ---
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint32_t myId; 
uint16_t gameId = 0;

int myScore = 0;
int opponentScore = 0;
int scoreLimit = 25;
bool gameOver = false;

// Variables pour l'anti-rebond
bool lastBtnTirer = false;
bool lastBtnReset = false;
unsigned long lastTirerTime = 0;
unsigned long lastResetTime = 0;
const unsigned long debounceDelay = 50; // 50 ms

typedef struct struct_message {
  uint32_t id;
  uint16_t gameId;
  int clicks;
  bool reset;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  struct_message incomingMessage;
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
  
  // Rejet de ses propres messages
  if (incomingMessage.id == myId) {
    return;
  }
  
  if (incomingMessage.reset) {
    myScore = 0; 
    opponentScore = 0; 
    gameOver = false;
    gameId = incomingMessage.gameId; // Synchro sur le nouvel ID de partie
  } else {
    // Rejet des paquets retardataires de la partie précédente
    if (incomingMessage.gameId == gameId) {
      opponentScore = incomingMessage.clicks;
    }
  }
}

void setup() {
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  for (int i = 0; i < NB; i++) baseG[i] = versGnd(i);
  for (int o = 0; o < NB; o++) {
    for (int i = 0; i < NB; i++) base[o][i] = (o != i) && relie(o, i);
  }

  calibrerBouton("TIRER", btnTirer);
  calibrerBouton("RESET", btnReset);

  myId = esp_random();
  myData.id = myId;

  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }
}

void loop() {
  bool currentTirer = appuye(btnTirer);
  bool currentReset = appuye(btnReset);
  unsigned long currentTime = millis();

  if (currentTirer && !lastBtnTirer && (currentTime - lastTirerTime > debounceDelay)) {
    lastTirerTime = currentTime;
    if (!gameOver) {
      myScore++;
      myData.clicks = myScore;
      myData.reset = false;
      myData.gameId = gameId;
      esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    }
  }
  lastBtnTirer = currentTirer;

  if (currentReset && !lastBtnReset && (currentTime - lastResetTime > debounceDelay)) {
    lastResetTime = currentTime;
    
    gameId++; // Changement d'ID pour invalider les anciens paquets
    myScore = 0; 
    opponentScore = 0; 
    gameOver = false;
    
    myData.clicks = 0; 
    myData.reset = true;
    myData.gameId = gameId;
    esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  }
  lastBtnReset = currentReset;

  int diff = myScore - opponentScore;
  if (diff >= scoreLimit) gameOver = true;
  else if (diff <= -scoreLimit) gameOver = true;

  // --- INTERFACE GRAPHIQUE ---
  display.clearDisplay();
  
  if (gameOver) {
    display.setTextSize(2);
    int16_t x, y; uint16_t w, h;
    const char* msg = (diff >= scoreLimit) ? "VICTOIRE" : "DEFAITE";
    display.getTextBounds(msg, 0, 0, &x, &y, &w, &h);
    display.setCursor((128 - w) / 2, 20);
    display.print(msg);
    
    display.setTextSize(1);
    const char* subMsg = "RESET pour rejouer";
    display.getTextBounds(subMsg, 0, 0, &x, &y, &w, &h);
    display.setCursor((128 - w) / 2, 45);
    display.print(subMsg);
  } else {
    display.setTextSize(1);
    display.setCursor(14, 0); 
    display.print("--- MULTIJOUEUR ---");

    display.setCursor(5, 20); display.print("Adv");
    display.setCursor(105, 20); display.print("Toi");

    display.drawLine(15, 35, 113, 35, SSD1306_WHITE);
    display.drawLine(15, 36, 113, 36, SSD1306_WHITE);

    display.drawLine(64, 28, 64, 44, SSD1306_WHITE);

    int cursorX = map(diff, -scoreLimit, scoreLimit, 15, 113);
    cursorX = constrain(cursorX, 15, 113);
    
    display.fillRect(cursorX - 4, 31, 9, 12, SSD1306_WHITE);
    display.fillRect(cursorX - 2, 33, 5, 8, SSD1306_BLACK);

    display.setCursor(0, 56);
    display.print("Force: ");
    display.print(abs(diff));
  }
  
  display.display();
  delay(10); 
}