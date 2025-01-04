#include <SPI.h>
#include <mcp2515.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128  // Độ rộng của OLED
#define SCREEN_HEIGHT 64  // Chiều cao của OLED
#define OLED_RESET -1     // Không cần sử dụng reset với ESP32

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

struct can_frame canMsg;

MCP2515 CAN(5);
int led1 = 27;
int led2 = 26;

String cardRFIDs[1];
int recentCard[5];

int temp_i = 0, temp_d = 0, hum_i = 0, hum_d = 0, doorVal = 0, disVal = 0, trunkLightVal = 0;

const char* ssidList[] = {  "VKU-SinhVien" };
const char* passwordList[] = { "" };
const int numWifi = sizeof(ssidList) / sizeof(ssidList[0]);
const char* host = "canbus.onlinewebshop.net";

unsigned long start_t = millis();
unsigned long previousMillis = 0;
const unsigned long interval = 60000;

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  Serial.begin(115200);
  while (!Serial)
    ;

  connectToWiFi();

  SPI.begin();
  CAN.reset();
  CAN.setBitrate(CAN_250KBPS, MCP_8MHZ);
  CAN.setNormalMode();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Không tìm thấy màn hình OLED"));
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  getCardRFID();
  convertStringToIntArray(cardRFIDs[0], recentCard, 5, 0);
}

void loop() {
  send();
  receive();
  displayOled();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendDataToServer();
  }
}


void receive() {
  if (CAN.readMessage(&canMsg) == MCP2515::ERROR_OK) {
    if (canMsg.can_id == 1) {
      digitalWrite(led1, !digitalRead(led1));
      Serial.print("Node 1:");
      Serial.print(" ");
      Serial.print(canMsg.data[0]);
      Serial.print(" ");
      Serial.print(canMsg.data[1]);
      Serial.print(" ");
      Serial.print(canMsg.data[2]);
      Serial.println(" ");

      doorVal = canMsg.data[0];
      disVal = canMsg.data[1];
      trunkLightVal = canMsg.data[2];
    } else if (canMsg.can_id == 2) {
      digitalWrite(led2, !digitalRead(led2));
      Serial.print("Node 2:");
      Serial.print(" ");
      Serial.print(canMsg.data[0]);
      Serial.print(" ");
      Serial.print(canMsg.data[1]);
      Serial.print(" ");
      Serial.print(canMsg.data[2]);
      Serial.print(" ");
      Serial.print(canMsg.data[3]);
      Serial.println(" ");

      temp_i = canMsg.data[0];
      temp_d = canMsg.data[1];
      hum_i = canMsg.data[2];
      hum_d = canMsg.data[3];
    }
  }
}


void send() {
  if (millis() - start_t > 100) {
    canMsg.can_id = 0x00;
    canMsg.can_dlc = 5;
    canMsg.data[0] = recentCard[0];
    canMsg.data[1] = recentCard[1];
    canMsg.data[2] = recentCard[2];
    canMsg.data[3] = recentCard[3];
    canMsg.data[4] = recentCard[4];

    Serial.print("Node 0: ");
    Serial.print(canMsg.data[0]);
    Serial.print(" ");
    Serial.print(canMsg.data[1]);
    Serial.print(" ");
    Serial.print(canMsg.data[2]);
    Serial.print(" ");
    Serial.print(canMsg.data[3]);
    Serial.print(" ");
    Serial.print(canMsg.data[4]);
    Serial.println("");
    CAN.sendMessage(&canMsg);
    start_t = millis();
  }
}

void displayOled() {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.print("Tempeature: ");
  oled.print(temp_i);
  oled.print(".");
  oled.print(temp_d);
  oled.print(" C");
  oled.setCursor(0, 10);
  oled.print("Humidity: ");
  oled.print(hum_i);
  oled.print(".");
  oled.print(hum_d);
  oled.print(" %");
  oled.display();
  oled.setCursor(0, 20);
  oled.print("DOOR: ");
  if (doorVal == 1) {
    oled.print("OPEN");
  } else {
    oled.print("CLOSE");
  }
  oled.setCursor(0, 40);
  oled.print("DISTANCE: ");
  oled.print(disVal);
  oled.print(" cm");
  oled.setCursor(0, 50);
  oled.print("LIGHT: ");
  if (trunkLightVal == 1) {
    oled.print("ON");
  } else {
    oled.print("OFF");
  }
  oled.display();
  delay(100);
}

void getCardRFID() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String serverPath = "http://" + String(host) + "/get_data.php";
    http.begin(serverPath.c_str());  // Kết nối đến server

    int httpResponseCode = http.GET();  // Gửi yêu cầu GET

    if (httpResponseCode == HTTP_CODE_OK) {  // Nếu nhận phản hồi thành công
      String response = http.getString();    // Lấy chuỗi JSON từ phản hồi
      Serial.println("Response: " + response);

      // Xử lý JSON
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, response);

      if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
      }
      // Truy xuất mảng JSON và lưu vào `cardRFIDs`
      JsonArray array = doc.as<JsonArray>();
      cardRFIDs[0] = array[0]["id_card"].as<String>();
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }

    http.end();  // Kết thúc kết nối
  } else {
    Serial.println("WiFi not connected");
  }

  delay(1000);  // Kiểm tra lại sau mỗi 1 giây
}

void sendDataToServer() {
  WiFiClient client;
  const int httpPort = 80;

  // Kết nối đến server
  if (!client.connect(host, httpPort)) {
    Serial.println("Connection failed");
    return;
  }

  // Tạo URL với các biến id_card và status
  String url = "/insert_data.php?temp=" + String(temp_i) + "." + String(temp_d) + "&hum=" + String(hum_i) + "." + String(hum_d);

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // Gửi yêu cầu HTTP GET đến server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");

  delay(500);  // Chờ để server xử lý và phản hồi

  // Đọc và hiển thị phản hồi từ server
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  client.stop();  // Đóng kết nối sau khi hoàn thành
}

void convertStringToIntArray(String str, int arr[], int arrSize, int startIndex) {
  int index = 0;
  int lastSpaceIndex = 0;

  for (int i = 0; i < str.length() && index < arrSize; i++) {
    if (str[i] == ' ') {
      arr[startIndex + index] = str.substring(lastSpaceIndex, i).toInt();
      index++;
      lastSpaceIndex = i + 1;
    }
  }
  // Lấy phần tử cuối cùng sau dấu cách
  arr[startIndex + index] = str.substring(lastSpaceIndex).toInt();
}
void connectToWiFi() {
  for (int i = 0; i < numWifi; i++) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssidList[i]);

    WiFi.begin(ssidList[i], passwordList[i]);
    unsigned long startAttemptTime = millis();

    // Chờ kết nối tối đa 10 giây
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(500);
      Serial.print(".");
    }

    // Nếu kết nối thành công, thoát vòng lặp
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.println("\nFailed to connect to WiFi");
  }

  Serial.println("Could not connect to any WiFi network. Restarting...");
  ESP.restart();  // Khởi động lại nếu không kết nối được bất kỳ WiFi nào
}