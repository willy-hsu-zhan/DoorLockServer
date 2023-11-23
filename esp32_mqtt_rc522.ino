#include <PubSubClient.h> 
#include <WiFi.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <MFRC522.h>
#include <Ultrasonic.h>

///////////////////////

// 接腳設定
#define PIN_RELAY   5
#define PIN_TRIGGER 12
#define PIN_ECHO    13
#define PIN_SPEAKER 15
#define PIN_SS      21
#define PIN_RST     22

///////////////////////

// 參數設定
// Wi-Fi configuration
const char * ssid = "CSIE_405";
const char * password = "61226122";

// MQTT configuration
const char *server = "nuucsie.ddns.net";
const char *machine_id = "esp32-doorlock";
const char *topicList[10] = { "doorlock_login", "doorlock_auth" };
String apiKey = "CSIE61226122";
String AuthToken = "";

// State
bool relayState = false;
String lastCardID = "";
int lastDistance = -1;
bool distanceDetectAvailable = false;
int distanceDetectTimes = 0;
int distanceDetectTimesMax = 10;
int distanceCheckTimes = 0;
int distanceCheckTimesMax = 3;

///////////////////////

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// RC522
MFRC522 mfrc522(PIN_SS, PIN_RST);  // Create MFRC522 instance

// HC-SR04
Ultrasonic ultrasonic(PIN_TRIGGER, PIN_ECHO);

///////////////////////

void beep(int delayTime=200) {
  digitalWrite(PIN_SPEAKER, HIGH), delay(delayTime);
  digitalWrite(PIN_SPEAKER, LOW);
}

///////////////////////

void init_WIFI() {
  Serial.println("---");
  Serial.println("[init_WIFI]");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void init_SPEAKER() {
  Serial.println("---");
  Serial.println("[init_SPEAKER]");
  pinMode(PIN_SPEAKER, OUTPUT);
}

void init_RELAY() {
  Serial.println("---");
  Serial.println("[init_RELAY]");
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
  relayState = false;
}

void init_RC522() {
  Serial.println("---");
  Serial.println("[init_RC522]");
  
  mfrc522.PCD_Init();   // Init MFRC522 初始化MFRC522卡
  // mfrc522.PCD_Init(PIN_SS, PIN_RST); // 初始化MFRC522卡
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  Serial.print(F("Reader "));
  Serial.print(F(": "));
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details 顯示讀卡設備的版本
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}

void init_ULTRASONIC() {
  Serial.println("---");
  Serial.println("[init_ULTRASONIC]");
  Serial.print("Distance:");
  Serial.print(ultrasonic.distanceRead()); // 讀取距離
  Serial.println(" CM");
}

///////////////////////

void mqttReConnect() {
  Serial.println("---");
  Serial.println("[mqttReConnect]");
  mqttClient.setServer(server, 1883);
  mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(machine_id)) {
      Serial.println("connected");
      mqttDeviceLogin(); // MQTT裝置登入
      mqttSubscribe(); // MQTT訂閱主題
    // mqttPublish(); // MQTT推播訊息
      beep(1000);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("---");
  Serial.println("[mqttCallback]");

  Serial.println("[mqttCallback][topic]: " + (String)topic);

  // 解析JSON data
  String jsonData = "";
  for (int i=0;i<length;i++) {
    jsonData += (char)payload[i];
  }
  Serial.println();
  Serial.println("[mqttCallback][jsonData]: " + jsonData);

  //////////////////////////////////

  StaticJsonDocument<200> doc;
  deserializeJson(doc, jsonData);
  bool state = (bool)doc["state"];
  Serial.print("[mqttCallback][state]: ");
  Serial.println(state);

  if ( (String)topic=="doorlock_auth"  ) {
    
    if(state==true){
      Serial.println("[mqttCallback][doorlock_auth]: success");
      beep(500);
      relay_switch();
    }else{
      Serial.println("[mqttCallback][doorlock_auth]: failed");
      beep(500);
      beep(500);
      beep(500);
    }
    
    String message = doc["data"]["message"];
    String Time = doc["data"]["time"];
    Serial.println("[mqttCallback][message]: " + message);
    Serial.println("[mqttCallback][Time]: " + Time);
  }

  //////////////////////////////////
  
//  String getData;
//  serializeJson(doc, getData);
//  Serial.println("[mqttCallback][getData]: " + getData);
}

void mqttSubscribe() {
  Serial.println("---");
  Serial.println("[mqttSubscribe]");
  int topicListLength = sizeof(topicList) / sizeof(String);
  Serial.printf("[mqttSubscribe][topicListLength]: %d\n", topicListLength);
  
  for ( byte i=0; i<topicListLength; ++i ) {
    bool state = mqttClient.subscribe(topicList[i]);
    Serial.println("[mqttSubscribe][topic]: " + (String)topicList[i] + ", " + (state?"success":"failed"));
  }
  
}

void mqttPublish() {
  Serial.println("---");
  Serial.println("[mqttPublish]");
  
  StaticJsonDocument<200> doc;
  
  doc["apiKey"] = apiKey;
  doc["data"]["text"] = "hello";

  String topic = "test";
  String payload;
  
  serializeJson(doc, payload);
  Serial.println("[payload]: " + payload);
  
  mqttEmit(topic, payload);
}

void mqttEmit(String topic, String value) {
  Serial.println("[mqttEmit] topic:" + topic + ", value:" + value);
  mqttClient.publish((char * ) topic.c_str(), (char * ) value.c_str());
}

void mqttDeviceLogin() {
  Serial.println("---");
  Serial.println("[mqttDoorlockLogin]");
  
  StaticJsonDocument<200> doc;
  
  doc["apiKey"] = apiKey;
  doc["data"]["machine_id"] = machine_id;

  String topic = "api_login";
  String payload;
  
  serializeJson(doc, payload);
  Serial.println("[payload]: " + payload);
  
  mqttEmit(topic, payload);
}

///////////////////////

void relay_switch(){
  Serial.println("---");
  Serial.println("[relay_switch]");
  digitalWrite(PIN_RELAY, HIGH), relayState = true; // 觸發繼電器ON
  Serial.println("[relay_switch][relayState]:" + (String)relayState);
  delay(1000);
  digitalWrite(PIN_RELAY, LOW), relayState = false; // 觸發繼電器OFF
  Serial.println("[relay_switch][relayState]:" + (String)relayState);
}

bool rc522ReadCard() {

  bool cardState = false; //卡片偵測狀態
  
  // 檢查是不是一張新的卡
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    cardState = true; // 偵測到卡片
    beep();
      
    // 顯示卡片內容
    byte *id = mfrc522.uid.uidByte;   // 取得卡片的UID
    byte idSize = mfrc522.uid.size;   // 取得UID的長度

    Serial.print("PICC type: ");      // 顯示卡片類型
    // 根據卡片回應的SAK值（mfrc522.uid.sak）判斷卡片類型
    MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
    Serial.println(mfrc522.PICC_GetTypeName(piccType));

    Serial.print("UID Size: ");       // 顯示卡片的UID長度值
    Serial.println(idSize);

    lastCardID = "";
    for (byte i = 0; i < idSize; i++) {  // 逐一顯示UID碼
      if (id[i] < 0x10) {
        Serial.println("0");
        lastCardID += "0";
      }
      Serial.println(id[i], HEX);       // 以16進位顯示UID值
      lastCardID += String(id[i], HEX);
    }
    Serial.println();

    lastCardID.toUpperCase(); // 強制轉換大寫
    
    Serial.println();
    Serial.println("[lastCardID]: " + lastCardID);

    mfrc522.PICC_HaltA();  // 讓卡片進入停止模式

    // mqtt push
    StaticJsonDocument<200> doc;
  
    doc["apiKey"] = apiKey;
    doc["data"]["machine_id"] = machine_id;
    doc["data"]["cardID"] = lastCardID;
    
    String payload;
    serializeJson(doc, payload);
    Serial.println("[payload]: " + payload);
    
    mqttEmit("api_update_doorlock", payload);
  }

  return cardState; // 回傳卡片狀態
}

bool ultrasonicRead() {
  int distance = ultrasonic.read();
    
  if ( ++distanceDetectTimes>=distanceDetectTimesMax ) {
    distanceDetectTimes = 0;
    return false;
  }
  
  if ( 3<=distance && distance<=10 ) { //距離介於3~5cm的距離
    Serial.print("[ULTRASONIC][distance]: " + (String)distance + " CM");
    Serial.print(" | [DetectTimes]: " + (String)distanceDetectTimes);
    Serial.println(" | [CheckTimes]: " + (String)distanceCheckTimes);
    if ( distance == lastDistance ) {
      if ( ++distanceCheckTimes>=distanceCheckTimesMax ) {
        distanceCheckTimes = 0;
        return true;
      }
    } else {
      lastDistance = distance;
    }
    delay(100);
    bool state = ultrasonicRead();
    return state;
  } else{
    distanceDetectTimes = 0;
    return false;
  }
}

// 偵測距離
bool detectDistance() {
  // 超音波
  bool distanceState = false;   // 距離確認
  int distanceOld = 0;          // 感測距離 - 舊
  int distanceNow = 100;        // 感測距離 - 現在
  int checkTime = 0;            // 確認次數
  int delayTime = 100;          // 延遲時間
  int detectTimes = 0;          // 感測次數
  int detectTimesMax = 100; // 感測次數最大值
  float distanceMin = 5;
  float distanceMax = 10;
  while ( !distanceState ) {

    // 檢查次數
    if ( checkTime == 2 ) {
      distanceState = true;
      break;
    }

    // 獲得最新距離
    distanceNow = ultrasonic.distanceRead();

    // 距離範圍檢查
    if ( distanceMin <= distanceNow && distanceNow <= distanceMax ) {
      // 檢查距離相同
      if ( distanceOld == distanceNow ) {
        ++checkTime; // 檢查次數遞增
      } else {
        distanceOld = distanceNow; // 更新距離值
        checkTime = 0; // 檢查次數歸零
      }
    }

    // 檢查次數 大於 最大偵測次數
    if ( ++detectTimes >= detectTimesMax ) break;
    delay(delayTime); // 延遲
  }
  if(distanceState == true){
    beep(300);
    beep(300);
   }
  return distanceState;
}

///////////////////////

void setup() {
  SPI.begin();  // Init SPI bus 初始化SPI介面
  Serial.begin(115200);
  Serial.println("---");
  Serial.println("[mqtt Setup]");
  init_WIFI(); // WIFI初始化
  init_SPEAKER(); // 蜂鳴器初始化
  init_RELAY(); // 繼電器初始化
  init_RC522(); // RFID初始化
  init_ULTRASONIC(); // 超音波初始化
  mqttReConnect(); // MQTT自動連線
  
  
  delay(500);
  Serial.println("---");
  Serial.println("[mqtt Ready]");
  beep();
}

void loop() {
  
  if (!mqttClient.connected()) { // 偵測MQTT是否斷線
    mqttReConnect(); // MQTT自動重新連線
  }
  
  mqttClient.loop(); // MQTT 循環監聽

  bool cardState = rc522ReadCard(); // 讀取卡片狀態
  
  distanceDetectAvailable = true; // 啟動超音波測距
  bool distanceState = ultrasonicRead(); // 偵測超音波測距
  if( distanceState ) {
    Serial.println("[distanceState]" + (String)distanceState);
    relay_switch();
    mqttEmit("api_distance", (String)lastDistance);
  }

}
