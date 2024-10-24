#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Khởi tạo LCD với địa chỉ I2C

const byte ROWS = 4;  
const byte COLS = 4;  
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {19, 18, 5, 4};  
byte colPins[COLS] = {26, 14, 12, 13};  

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

Servo myServo;

String inputPassword;  // Chuỗi lưu mật khẩu nhập vào

#define WIFI_SSID "ae38"  // SSID WiFi
#define WIFI_PASSWORD "19216801"  // Mật khẩu WiFi
#define FIREBASE_HOST "smart-home-52e19-default-rtdb.firebaseio.com"  // Địa chỉ Firebase
#define FIREBASE_AUTH "Nphho9OXtu9dMyqppI7Jb0aj7Oe8TWYLpXn7yLpE"  // Mã xác thực Firebase

const int relayPin = 25;   // Chân GPIO cho relay
const int sensorPin = 27;  // Chân GPIO cho cảm biến TTP223

bool fanState = LOW;  // Biến lưu trạng thái quạt
bool doorState = false;  // Biến lưu trạng thái cửa
FirebaseData firebaseDataDoor;  // Đối tượng Firebase cho cửa
FirebaseData firebaseDataFan;   // Đối tượng Firebase cho quạt
FirebaseConfig config;
FirebaseAuth auth;

void setup() {
  Serial.begin(115200);  // Khởi động Serial monitor
  lcd.init();  // Khởi tạo LCD
  lcd.backlight();  // Bật đèn nền LCD
  myServo.attach(2);  // Gán servo vào chân GPIO 2
  myServo.write(0);  // Đặt góc ban đầu cho servo (cửa đóng)
  pinMode(relayPin, OUTPUT);  // Đặt chân relay là đầu ra
  pinMode(sensorPin, INPUT);  // Đặt chân cảm biến là đầu vào
  digitalWrite(relayPin, LOW);  // Tắt quạt ban đầu

  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Đang kết nối với WiFi...");
  }
  Serial.println("Đã kết nối với WiFi!");

  // Cấu hình Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Khởi tạo luồng Firebase cho trạng thái cửa
  if (Firebase.beginStream(firebaseDataDoor, "/door/isOpened")) {
    Serial.println("Bắt đầu luồng cho cửa thành công!");
  } else {
    Serial.print("Lỗi khi bắt đầu luồng cho cửa: ");
    Serial.println(firebaseDataDoor.errorReason());
  }

  // Khởi tạo luồng Firebase cho trạng thái quạt
  if (Firebase.beginStream(firebaseDataFan, "/fan/isOn")) {
    Serial.println("Bắt đầu luồng cho quạt thành công!");
  } else {
    Serial.print("Lỗi khi bắt đầu luồng cho quạt: ");
    Serial.println(firebaseDataFan.errorReason());
  }
}

void loop() {
  // Đọc phím từ keypad
  char key = keypad.getKey();

  if (key) {  
    if (key == '#') {  // Nhấn '#' để xác nhận mật khẩu
      if (inputPassword == "66") {
        doorState = true;  // Đặt trạng thái cửa là mở
        myServo.write(90);  // Mở cửa (90 độ)
        lcd.setCursor(0, 1);
        lcd.print("Door Open      ");
        Firebase.setBool(firebaseDataDoor, "/door/isOpened", true);  // Cập nhật trạng thái cửa lên Firebase
      } else if (inputPassword == "33") {
        doorState = false;  // Đặt trạng thái cửa là đóng
        myServo.write(0);  // Đóng cửa (0 độ)
        lcd.setCursor(0, 1);
        lcd.print("Door Closed    ");
        Firebase.setBool(firebaseDataDoor, "/door/isOpened", false);  // Cập nhật trạng thái cửa lên Firebase
      } else {
        lcd.setCursor(0, 1);
        lcd.print("Wrong Pass     ");  // Hiển thị thông báo mật khẩu sai
      }
      inputPassword = "";  // Xóa chuỗi nhập vào
      delay(2000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Password:");
    } else if (key == '*') {  // Nhấn '*' để xóa mật khẩu
      inputPassword = "";
      lcd.setCursor(0, 1);
      lcd.print("Cleared        ");
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Password:");
    } else {
      inputPassword += key;  // Thêm ký tự vào chuỗi mật khẩu
      lcd.setCursor(0, 1);
      lcd.print("Pass: " + inputPassword);  // Hiển thị mật khẩu hiện tại
    }
  }

  // Kiểm tra cập nhật từ luồng Firebase cho trạng thái cửa
  if (Firebase.readStream(firebaseDataDoor)) {
    if (firebaseDataDoor.dataType() == "boolean") {
      bool doorIsOpened = firebaseDataDoor.boolData();
      if (doorIsOpened != doorState) {  // Chỉ hành động khi trạng thái thay đổi
        doorState = doorIsOpened;  // Cập nhật trạng thái cửa
        myServo.write(doorState ? 90 : 0);  // Mở hoặc đóng cửa
      }
    }
  }

  // Kiểm tra trạng thái từ cảm biến TTP223
  int sensorState = digitalRead(sensorPin);
  static int lastSensorState = LOW;

  if (sensorState == HIGH && lastSensorState == LOW) {
    fanState = !fanState;  // Đổi trạng thái quạt
    digitalWrite(relayPin, fanState);  // Cập nhật trạng thái quạt
    Firebase.setBool(firebaseDataFan, "/fan/isOn", fanState);  // Cập nhật trạng thái quạt lên Firebase
    delay(200);
  }

  // Kiểm tra cập nhật từ luồng Firebase cho trạng thái quạt
  if (Firebase.readStream(firebaseDataFan)) {
    if (firebaseDataFan.dataType() == "boolean") {
      bool firebaseFanIsOn = firebaseDataFan.boolData();
      if (firebaseFanIsOn != fanState) {  // Chỉ hành động khi trạng thái thay đổi
        fanState = firebaseFanIsOn;  // Cập nhật trạng thái quạt
        digitalWrite(relayPin, fanState ? HIGH : LOW);  // Bật hoặc tắt quạt
      }
    }
  }

  // Kiểm tra kết nối WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Mất kết nối WiFi! Đang thử kết nối lại...");
    WiFi.reconnect();
  }

  lastSensorState = sensorState;  // Cập nhật trạng thái cảm biến trước đó
  delay(100);
}
