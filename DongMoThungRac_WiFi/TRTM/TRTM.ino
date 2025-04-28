#include <Servo.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ====== Servo & Cảm biến ======
Servo myservo;

#define GOC_DONG 0
#define GOC_MO 130

int trigPin = D2;
int echoPin = D1;
int servoPin = D5;
#define BUZZER_PIN D7
#define LED_PIN D6 
#define DHTPIN D3    
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ====== RGB LED Pins ======
#define RED_PIN D0
#define GREEN_PIN D8
#define BLUE_PIN D4

int openCount = 0;  // Thêm dòng này
unsigned long previousMillisLED = 0;  // Lưu thời gian thay đổi LED
int ledState = 0;  // Trạng thái LED, sẽ thay đổi từ 0 -> 1 -> 2 -> 0 ->...
bool overheatTriggered = false;

unsigned long previousMillis = 0;
unsigned char autoTrigger = 0;
unsigned long autoMillis = 0;

// ====== WiFi & MQTT ======
const char* ssid = "Fablab 2.4G";
const char* password = "Fira@2024";
const char* mqtt_server = "192.168.69.87";

WiFiClient espClient;
PubSubClient client(espClient);

// ====== Kết nối WiFi ======
void setup_wifi() {
  delay(10);
  Serial.println("Đang kết nối WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bị mất kết nối. Đang kết nối lại...");
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Đã kết nối WiFi!");
  Serial.print("IP của ESP8266: ");
  Serial.println(WiFi.localIP());
}

// ====== Kết nối lại MQTT nếu bị mất ======
void reconnect() {
  while (!client.connected()) {
    Serial.print("Đang kết nối MQTT...");
    if (client.connect("ESP8266Client")) {
      Serial.println(" => Đã kết nối MQTT!");
      client.subscribe("trashbin/control");
    } else {
      Serial.println(" => Thất bại, thử lại sau 5s.");
      Serial.print("Mã lỗi MQTT: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

// ====== Callback khi nhận lệnh từ MQTT ======
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Nhận lệnh từ MQTT: ");
  Serial.println(message);

  if (String(topic) == "trashbin/control") {
    if (message == "open") {
      myservo.write(GOC_MO);
      digitalWrite(LED_PIN, HIGH);
      tone(BUZZER_PIN, 1500);
      delay(100);
      noTone(BUZZER_PIN);

      client.publish("trashbin/servo", "open");
      client.publish("trashbin/buzzer", "on");
      client.publish("trahbin/led", "on");
      openCount++;
      Serial.print("Số lần mở nắp (MQTT): ");
      Serial.println(openCount);
      // Gửi lên MQTT
      String countStr = String(openCount);
      client.publish("trashbin/opencount", countStr.c_str());
    } 
    else if (message == "close") {
      myservo.write(GOC_DONG);
      digitalWrite(LED_PIN, LOW);
      noTone(BUZZER_PIN); // tắt buzzer phòng trường hợp đang kêu
      client.publish("trashbin/servo", "close");
      client.publish("trashbin/buzzer", "off");
      client.publish("trashbin/led", "off");
    }
  }
}

// ====== Cảm biến khoảng cách ======
long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2; // đổi ra cm
}

// ====== Cài đặt ban đầu ======
void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  myservo.attach(servoPin);
  myservo.write(GOC_DONG);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  dht.begin();
  String json = "{\"temperature\":25.3}";
  client.publish("trashbin/temperature", json.c_str());
  pinMode(BLUE_PIN, OUTPUT);  // Đổi LED_BLUE_PIN thành BLUE_PIN
  pinMode(GREEN_PIN, OUTPUT); // Đổi LED_GREEN_PIN thành GREEN_PIN
  pinMode(RED_PIN, OUTPUT);   // Đổi LED_RED_PIN thành RED_PIN
}

// ====== Vòng lặp chính ======
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi bị mất kết nối. Đang kết nối lại...");
    setup_wifi();
  }

  if (millis() - previousMillis >= 200) {
    previousMillis = millis();
    int distance = getDistance();
    Serial.print("Khoảng cách đo được: ");
    Serial.println(distance);
    Serial.println(" cm");

    static int lastDistance = -1;
    if (abs(distance - lastDistance) > 2) {
      lastDistance = distance;
      String msg = String(distance);
      Serial.print("Gửi lên MQTT: ");
      Serial.println(msg);
      if (client.publish("trashbin/distance", msg.c_str())) {
        Serial.println("Đã gửi lên MQTT!");
      } else {
        Serial.println("Không thể gửi dữ liệu lên MQTT!");
      }
    }

    // Tự động mở nắp nếu có người lại gần
    if (distance < 15) {
      autoTrigger = 1;
      autoMillis = millis();
      myservo.write(GOC_MO);
      tone(BUZZER_PIN, 1500);
      delay(100);
      noTone(BUZZER_PIN);
      digitalWrite(LED_PIN, HIGH);
      client.publish("trashbin/servo", "open");
      client.publish("trashbin/buzzer", "on");
      client.publish("trashbin/led", "on");
      openCount++;
      Serial.print("Số lần mở nắp (MQTT): ");
      Serial.println(openCount);
      // Gửi lên MQTT
      String countStr = String(openCount);
      client.publish("trashbin/opencount", countStr.c_str());
    }
  }
  // Đóng nắp sau 2 giây nếu đã mở tự động
  if (millis() - autoMillis >= 2000 && autoTrigger == 1) {
    autoTrigger = 0;
    myservo.write(GOC_DONG);
    digitalWrite(LED_PIN, LOW);
    client.publish("trashbin/servo", "close");
    client.publish("trashbin/buzzer", "off");
    client.publish("trashbin/led", "off");
  }

  // Đọc nhiệt độ/độ ẩm
  unsigned long lastTempHumMillis = 0;
  const unsigned long tempHumInterval = 1000; 
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    String tempStr = String(temperature);
    String humStr = String(humidity);

    Serial.print("Nhiệt độ: ");
    Serial.print(tempStr);
    Serial.print(" °C, Độ ẩm: ");
    Serial.print(humStr);
    Serial.println(" %");

    client.publish("trashbin/temperature", tempStr.c_str());
    client.publish("trashbin/humidity", humStr.c_str());
  } else {
    Serial.println("Không đọc được dữ liệu từ DHT11");
  }

  // 🚨 Nếu nhiệt độ ≥ 40 độ C => cảnh báo
  if (temperature >= 40 && !overheatTriggered) {
    overheatTriggered = true;
    Serial.println("🔥 CẢNH BÁO: Nhiệt độ quá cao! MỞ NẮP!");
    myservo.write(GOC_MO);
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000); // kêu liên tục

    client.publish("trashbin/servo", "open");
    client.publish("trashbin/buzzer", "on");
    client.publish("trashbin/led", "on");
    openCount++;
    Serial.print("Số lần mở nắp (MQTT): ");
    Serial.println(openCount);
    // Gửi lên MQTT
    String countStr = String(openCount);
    client.publish("trashbin/opencount", countStr.c_str());
  }

  // Nếu nhiệt độ giảm < 40 => tắt cảnh báo (tùy bạn muốn giữ mở hay tự đóng lại)
  if (temperature < 40 && overheatTriggered) {
    overheatTriggered = false;
    Serial.println("✅ Nhiệt độ bình thường trở lại");

    noTone(BUZZER_PIN); // tắt còi
    digitalWrite(LED_PIN, LOW);
    myservo.write(GOC_DONG); // Đóng nắp nếu muốn
    client.publish("trashbin/servo", "close");
    client.publish("trashbin/buzzer", "off");
    client.publish("trashbin/led", "off");
  }
  // Thay đổi màu LED sau mỗi 2 giây
  if (millis() - previousMillisLED >= 2000) {
    previousMillisLED = millis(); // Cập nhật thời gian

  // Tắt tất cả các LED
    digitalWrite(BLUE_PIN, LOW);  // Đổi LED_BLUE_PIN thành BLUE_PIN
    digitalWrite(GREEN_PIN, LOW); // Đổi LED_GREEN_PIN thành GREEN_PIN
    digitalWrite(RED_PIN, LOW);   // Đổi LED_RED_PIN thành RED_PIN

  // Đổi trạng thái LED theo giá trị của ledState
    if (ledState == 0) {
      digitalWrite(BLUE_PIN, HIGH);  // Bật LED xanh dương
      digitalWrite(GREEN_PIN, LOW);
      digitalWrite(RED_PIN, LOW);
      client.publish("trashbin/3led", "BLUE");
      ledState = 1;
    } else if (ledState == 1) {
      digitalWrite(GREEN_PIN, HIGH);  // Bật LED xanh lá
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(RED_PIN, LOW);
      client.publish("trashbin/3led", "GREEN");
      ledState = 2;
    } else if (ledState == 2) {
      digitalWrite(RED_PIN, HIGH);  // Bật LED đỏ
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
      client.publish("trashbin/3led", "RED");
      ledState = 0;
    }
  }
}