#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// Định nghĩa thông tin WiFi và Firebase
#define WIFI_SSID "ae38"
#define WIFI_PASSWORD "19216801"
#define FIREBASE_HOST "smart-home-52e19-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Nphho9OXtu9dMyqppI7Jb0aj7Oe8TWYLpXn7yLpE"

// Định nghĩa chân GPIO
const int ledPin = 5;   // Chân GPIO cho LED
const int sensorPin = 14; // Chân GPIO cho TTP223
#define DPIN 4        // Pin để kết nối cảm biến DHT (GPIO)
#define DTYPE DHT11   // Định nghĩa loại cảm biến DHT (DHT11 hoặc DHT22)

bool ledState = LOW; // Biến lưu trạng thái LED
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

DHT dht(DPIN, DTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Kiểm tra lại địa chỉ I2C nếu không phải 0x27


void setup() {
  // Khởi động Serial Monitor
  Serial.begin(115200);
  
  // Khởi tạo chân GPIO cho LED và cảm biến
  pinMode(ledPin, OUTPUT);
  pinMode(sensorPin, INPUT_PULLUP);

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("Đang kết nối với WiFi...");
  }
  Serial.println("Đã kết nối với WiFi!");

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Khởi tạo cảm biến DHT
  dht.begin();

  // Cấu hình Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Khởi tạo Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
 // Kiểm tra kết nối Firebase
  if (Firebase.beginStream(firebaseData, "/light/isOn")) {
    Serial.println("Kết nối đến Firebase thành công!");
  } else {
    Serial.print("Lỗi kết nối đến Firebase: ");
    Serial.println(firebaseData.errorReason());
  }
}

unsigned long lastUpdateTime = 0; // Biến để lưu thời gian cập nhật lần cuối
const unsigned long updateInterval = 60000; // 1 p (5000 ms)

void loop() {
  // Đọc trạng thái từ TTP223
  int sensorState = digitalRead(sensorPin);
  // In ra trạng thái cảm biến
    lcd.setCursor(0, 0); 
    float tc = dht.readTemperature(false);  // Đọc nhiệt độ bằng DHT (đơn vị Celsius)
    float hu = dht.readHumidity();          // Đọc độ ẩm bằng DHT

    // Hiển thị dữ liệu trên màn hình LCD
    lcd.print("Temp: ");
    lcd.print(tc);
    lcd.print(" C ");
    lcd.setCursor(0, 1); 
    lcd.print("Hum: ");
    lcd.print(hu);
    lcd.println(" %                ");
  // Lấy thời gian hiện tại
  unsigned long currentTime = millis();
  
  // Kiểm tra nếu đã đến thời điểm cập nhật dữ liệu (sau mỗi 5 giây)
  if (currentTime - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentTime; // Cập nhật thời gian lần cuối
    // Gửi dữ liệu lên Firebase
    FirebaseJson json;
    json.set("/environment/temperature", tc);
    json.set("/environment/humidity", hu);
    Firebase.updateNode(firebaseData, "/", json);
  }
  
  // Phần còn lại của loop không bị ảnh hưởng bởi thời gian cập nhật
  static int lastSensorState = LOW; // Lưu trạng thái cảm biến trước đó
  Serial.print(sensorState);
  Serial.println(lastSensorState);

  // Kiểm tra nếu cảm biến vừa được kích hoạt
  if (sensorState == HIGH && lastSensorState == LOW) {
    // Đổi trạng thái LED
    ledState = !ledState; // Đảo ngược trạng thái LED
    digitalWrite(ledPin, ledState); // Cập nhật trạng thái LED

    // Cập nhật trạng thái LED lên Firebase
    if (Firebase.setBool(firebaseData, "/light/isOn", !ledState)) {
      Serial.println("Trạng thái LED đã được cập nhật lên Firebase.");
    } else {
      Serial.print("Lỗi cập nhật Firebase: ");
      Serial.println(firebaseData.errorReason());
    }
  }

  // Kiểm tra cập nhật từ Firebase
  if (Firebase.readStream(firebaseData)) {
    if (firebaseData.dataType() == "boolean") {
      bool firebaseIsOn = firebaseData.boolData();
      digitalWrite(ledPin, !firebaseIsOn ? HIGH : LOW); // Cập nhật trạng thái LED từ Firebase
    }
  } else if (firebaseData.dataType() == "undefined") {
    Serial.print("Lỗi đọc Firebase: ");
    Serial.println(firebaseData.errorReason());
  }

  lastSensorState = sensorState; // Cập nhật trạng thái cảm biến trước đó

  // Kiểm tra lại kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Mất kết nối WiFi! Đang thử kết nối lại...");
    WiFi.reconnect(); // Tự động kết nối lại
  }
}
