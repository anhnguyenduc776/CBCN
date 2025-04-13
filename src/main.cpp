#include "wifiConfig.h"
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <ESP_Mail_Client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Định nghĩa chân cảm biến
#define MQ2_PIN 34           // Chân analog để đọc giá trị MQ2
#define MQ2_INTERRUPT_PIN 15 // Chân D15 (GPIO 15) làm ngắt từ MQ2 (digital)
#define DS18B20_PIN 4

// Định nghĩa chân nút nhấn
#define BUTTON_AUTO 33
#define BUTTON_MAN 25
#define BUTTON_FAN 26
#define BUTTON_BUZZ 27
#define BUTTON_PUMP 14
#define BUTTON_WAKEUP 32     // Nút nhấn để đánh thức ngay lập tức

// Định nghĩa chân relay
#define RELAY_FAN 5
#define RELAY_BUZZER 18
#define RELAY_PUMP 19

// Định nghĩa chân LED
#define LED_STATUS 13

// Ngưỡng an toàn
#define MQ2_THRESHOLD 50.0
#define TEMP_THRESHOLD 35.0

// Thời gian (ms)
#define HOLD_TIME 300       // Thời gian giữ nút để kích hoạt
#define MQTT_WAIT_TIME 1000  // Thời gian chờ web cập nhật trước khi ngủ
#define DEBOUNCE_DELAY 50    // Thời gian debounce cho nút nhấn
#define EMAIL_STATUS_TIMEOUT 5000 // Thời gian hiển thị "Gmail: OK" (5 giây)

// Thông tin MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_pass = "";
const char* mqtt_topic_sensors = "ducan_esp32/test/sensors";
const char* mqtt_topic_auto = "ducan_esp32/test/auto";
const char* mqtt_topic_manual = "ducan_esp32/test/manual";
const char* mqtt_topic_fan = "ducan_esp32/test/fan";
const char* mqtt_topic_buzzer = "ducan_esp32/test/buzzer";
const char* mqtt_topic_pump = "ducan_esp32/test/pump";
const char* mqtt_topic_sleep = "ducan_esp32/test/sleep";

// Thông tin Gmail
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "testabc151004@gmail.com" // Email gửi
#define AUTHOR_PASSWORD "vjbnixcuyykncfna"     // App Password
#define RECIPIENT_EMAIL "ducanh20102004@gmail.com" // Email nhận

// Khởi tạo OLED (SSD1306 128x64 qua I2C) với U8g2
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // SDA: GPIO 21, SCL: GPIO 22

// Khai báo hàm
bool sendMQTTData();
void enterLightSleep();
void displayOLED();
void displayResetCountdown();
void checkButtonReset(); // Đã được định nghĩa trong wifiConfig.h
void sendEmailAlert(const char* sensor, float value, float threshold);
void emailTask(void *pvParameters);

// Khởi tạo đối tượng cảm biến
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SMTPSession smtp;

Config wifiConfig;
unsigned long lastMsg = 0;
unsigned long lastMqttAttempt = 0;
const unsigned long mqttRetryInterval = 5000;
unsigned long lastNetworkAttempt = 0;
const unsigned long networkRetryInterval = 10000;
unsigned long lastOledUpdate = 0;
const unsigned long oledUpdateInterval = 500; // Cập nhật OLED mỗi 500ms
unsigned long lastEmailSent = 0;
const unsigned long emailInterval = 60000; // Gửi email tối đa mỗi 60 giây

// Biến trạng thái
RTC_DATA_ATTR bool autoMode = false; // Lưu vào RTC memory
RTC_DATA_ATTR bool justReset = false; // Biến theo dõi trạng thái reset bằng nút BOOT
RTC_DATA_ATTR bool oledInitialized = false; // Theo dõi trạng thái OLED
bool manualFan = false;
bool manualBuzzer = false;
bool manualPump = false;
bool fanActive = false;
bool buzzerActive = false;
bool pumpActive = false;
uint64_t sleepTime = 0;
bool manualOverride = false;
bool sleepPending = false;
bool sleepCanceled = false;
bool receivedOff = false; // Theo dõi lệnh OFF từ web

// Biến theo dõi trạng thái gửi email (toàn cục, chia sẻ với task)
volatile bool gasAlertTriggered = false; // Cờ báo khí gas vượt ngưỡng
volatile bool tempAlertTriggered = false; // Cờ báo nhiệt độ vượt ngưỡng
volatile float lastGasValue = 0.0; // Giá trị khí gas để gửi email
volatile float lastTempValue = 0.0; // Giá trị nhiệt độ để gửi email
volatile int emailStatus = 0; // 0: Không gửi, 1: Đang gửi, 2: Thành công
volatile unsigned long lastEmailStatusChange = 0; // Thời gian trạng thái email thay đổi

// Biến thời gian nhấn nút
unsigned long buttonAutoPressTime = 0;
unsigned long buttonManPressTime = 0;
unsigned long buttonFanPressTime = 0;
unsigned long buttonBuzzPressTime = 0;
unsigned long buttonPumpPressTime = 0;

// Biến debounce
unsigned long lastButtonAutoDebounce = 0;
unsigned long lastButtonManDebounce = 0;
unsigned long lastButtonFanDebounce = 0;
unsigned long lastButtonBuzzDebounce = 0;
unsigned long lastButtonPumpDebounce = 0;

// Biến cho LED nháy
unsigned long previousLedMillis = 0;
bool ledState = false;

float getMQ2Value() {
    int analogValue = analogRead(MQ2_PIN);
    float mq2Value = map(analogValue, 0, 4095, 0, 1000) / 10.0;
    if (mq2Value < 0 || mq2Value > 100) {
        Serial.println("Lỗi: Giá trị MQ2 không hợp lệ - " + String(mq2Value));
        return -1.0;
    }
    return mq2Value;
}

float getDS18B20Value() {
    ds18b20.requestTemperatures();
    float temp = ds18b20.getTempCByIndex(0);
    if (temp == DEVICE_DISCONNECTED_C || temp < -50 || temp > 150) {
        Serial.println("Lỗi: DS18B20 ngắt kết nối hoặc không hợp lệ");
        return -1.0;
    }
    return temp;
}

void displayResetCountdown() {
    if (!oledInitialized) {
        Serial.println("OLED chưa khởi tạo, bỏ qua hiển thị đếm ngược");
        return;
    }

    if (esp_reset_reason() == ESP_RST_SW) {
        justReset = true;
        for (int i = 5; i >= 1; i--) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setCursor(0, 20);
            u8g2.print("Resetting by BOOT...");
            u8g2.setCursor(0, 40);
            u8g2.print("Countdown: ");
            u8g2.print(i);
            u8g2.print(" s");
            u8g2.sendBuffer();
            delay(1000);
        }

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(0, 30);
        u8g2.print("Setup Oke");
        u8g2.sendBuffer();
        delay(1000);
    } else {
        for (int i = 5; i >= 1; i--) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setCursor(0, 20);
            u8g2.print("Setup Mode...");
            u8g2.sendBuffer();
            delay(1000);
        }

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(0, 30);
        u8g2.print("RESET OK");
        u8g2.sendBuffer();
        delay(1000);
    }
}

void displayOLED() {
    if (!oledInitialized) {
        Serial.println("OLED chưa khởi tạo, bỏ qua hiển thị");
        return;
    }

    unsigned long now = millis();
    if (now - lastOledUpdate < oledUpdateInterval) {
        return;
    }
    lastOledUpdate = now;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    if (justReset) {
        u8g2.setCursor(0, 20);
        u8g2.print("WiFi: ");
        u8g2.print(wifiMode == 0 ? WiFi.softAPSSID() : ssid.c_str());
        u8g2.setCursor(0, 40);
        u8g2.print("IP: ");
        u8g2.print(wifiMode == 0 ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
        u8g2.sendBuffer();
        delay(2000);
        justReset = false;
        return;
    }

    if (WiFi.status() == WL_DISCONNECTED && wifiMode == 2 && !is4GConnected) {
        u8g2.setCursor(0, 30);
        u8g2.print("WiFi: Connecting...");
        u8g2.sendBuffer();
        return;
    }

    u8g2.setCursor(0, 10);
    u8g2.print("Mode: ");
    u8g2.print(autoMode ? "Auto" : "Manual");

    float temp = getDS18B20Value();
    u8g2.setCursor(0, 20);
    u8g2.print("Temp: ");
    if (temp != -1.0) u8g2.print(temp);
    else u8g2.print("Error");
    u8g2.print(" C");

    float mq2 = getMQ2Value();
    u8g2.setCursor(0, 30);
    u8g2.print("Gas: ");
    if (mq2 != -1.0) u8g2.print(mq2);
    else u8g2.print("Error");
    u8g2.print(" %");

    // Hiển thị trạng thái email
    u8g2.setCursor(0, 40);
    if (emailStatus == 1) {
        u8g2.print("Gmail...");
    } else if (emailStatus == 2 && (millis() - lastEmailStatusChange < EMAIL_STATUS_TIMEOUT)) {
        u8g2.print("Gmail: OK");
        // Hiển thị thông số khi gửi thành công
        u8g2.setCursor(0, 50);
        u8g2.print("Gas: ");
        u8g2.print(lastGasValue);
        u8g2.print(" %");
        u8g2.setCursor(0, 60);
        u8g2.print("Temp: ");
        u8g2.print(lastTempValue);
        u8g2.print(" C");
    } else {
        u8g2.print("Gmail: No");
        emailStatus = 0; // Reset sau timeout
        // Hiển thị trạng thái mạng
        u8g2.setCursor(0, 50);
        if (WiFi.status() == WL_CONNECTED) {
            u8g2.print("WiFi: Connected");
        } else if (is4GConnected) {
            u8g2.print("4G: Connected");
        } else if (wifiMode == 0) {
            u8g2.print("AP: ");
            u8g2.print(WiFi.softAPSSID());
        } else {
            u8g2.print("Network: Disconnected");
        }
    }

    u8g2.sendBuffer();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Tin nhắn đến [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    char message[256];
    if (length >= sizeof(message)) length = sizeof(message) - 1;
    memcpy(message, payload, length);
    message[length] = '\0';

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("deserializeJson() thất bại: ");
        Serial.println(error.c_str());
        return;
    }

    if (strcmp(topic, mqtt_topic_sleep) == 0) {
        if (doc.containsKey("type") && doc["type"] == "sleep") {
            if (doc.containsKey("time")) {
                int timeValue = doc["time"];
                if (timeValue > 0) {
                    sleepTime = (uint64_t)timeValue * 1000000ULL;
                    sleepPending = true;
                    sleepCanceled = false;
                    receivedOff = false;
                    Serial.println("Nhận thời gian ngủ: " + String(timeValue) + " giây");
                    sendMQTTData();
                    enterLightSleep();
                }
            } else if (doc.containsKey("enable") && doc["enable"] == true) {
                sleepTime = 10 * 1000000ULL; // Mặc định 10 giây
                sleepPending = true;
                sleepCanceled = false;
                receivedOff = false;
                Serial.println("Nhận lệnh kích hoạt ngủ, thời gian mặc định: 10 giây");
                sendMQTTData();
                enterLightSleep();
            }
        } else if (strcmp(message, "OFF") == 0) {
            receivedOff = true;
            Serial.println("Nhận lệnh TẮT, sẽ xử lý sau khi thức dậy");
            sendMQTTData();
        }
    } else if (strcmp(topic, mqtt_topic_fan) == 0) {
        int state = doc["state"] | atoi(message);
        if (autoMode) {
            fanActive = state;
            digitalWrite(RELAY_FAN, fanActive);
            Serial.println("Trạng thái quạt cập nhật (Tự động): " + String(fanActive));
        } else {
            manualFan = state;
            digitalWrite(RELAY_FAN, manualFan);
            Serial.println("Trạng thái quạt cập nhật (Thủ công): " + String(manualFan));
        }
        sendMQTTData();
    } else if (strcmp(topic, mqtt_topic_pump) == 0) {
        int state = doc["state"] | atoi(message);
        if (autoMode) {
            pumpActive = state;
            digitalWrite(RELAY_PUMP, pumpActive);
            Serial.println("Trạng thái bơm cập nhật (Tự động): " + String(pumpActive));
        } else {
            manualPump = state;
            digitalWrite(RELAY_PUMP, manualPump);
            Serial.println("Trạng thái bơm cập nhật (Thủ công): " + String(manualPump));
        }
        sendMQTTData();
    } else if (strcmp(topic, mqtt_topic_buzzer) == 0) {
        int state = doc["state"] | atoi(message);
        if (autoMode) {
            buzzerActive = state;
            digitalWrite(RELAY_BUZZER, buzzerActive);
            Serial.println("Trạng thái còi cập nhật (Tự động): " + String(buzzerActive));
        } else {
            manualBuzzer = state;
            digitalWrite(RELAY_BUZZER, manualBuzzer);
            Serial.println("Trạng thái còi cập nhật (Thủ công): " + String(manualBuzzer));
        }
        sendMQTTData();
    } else if (strcmp(topic, mqtt_topic_auto) == 0) {
        autoMode = doc["state"] | atoi(message);
        Serial.println("Chế độ tự động cập nhật: " + String(autoMode));
        sendMQTTData();
    } else if (strcmp(topic, mqtt_topic_manual) == 0) {
        autoMode = !(doc["state"] | atoi(message));
        Serial.println("Chế độ thủ công cập nhật: " + String(!autoMode));
        sendMQTTData();
    }
}

void setupMQTT() {
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(callback);
}

void reconnectMQTT() {
    if (millis() - lastMqttAttempt >= mqttRetryInterval) {
        lastMqttAttempt = millis();
        Serial.print("Đang kết nối MQTT...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("đã kết nối");
            mqttClient.subscribe(mqtt_topic_auto);
            mqttClient.subscribe(mqtt_topic_manual);
            mqttClient.subscribe(mqtt_topic_fan);
            mqttClient.subscribe(mqtt_topic_buzzer);
            mqttClient.subscribe(mqtt_topic_pump);
            mqttClient.subscribe(mqtt_topic_sleep);
        } else {
            Serial.print("thất bại, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" thử lại sau 5 giây");
        }
    }
}

bool sendMQTTData() {
    float mq2Value = getMQ2Value();
    float ds18b20Value = getDS18B20Value();
    char msg[150];

    int fanState = autoMode ? fanActive : manualFan;
    int buzzerState = autoMode ? buzzerActive : manualBuzzer;
    int pumpState = autoMode ? pumpActive : manualPump;

    snprintf(msg, 150, "{\"mq2\":%.1f,\"ds18b20\":%.1f,\"wifiMode\":%d,\"autoMode\":%d,\"fan\":%d,\"buzzer\":%d,\"pump\":%d,\"sleepPending\":%d,\"sleepCanceled\":%d}", 
             mq2Value, ds18b20Value, wifiMode, autoMode, fanState, buzzerState, pumpState, sleepPending, sleepCanceled);
    Serial.print("Gửi dữ liệu: ");
    Serial.println(msg);

    bool success = false;
    if (mqttClient.connected()) {
        success = mqttClient.publish(mqtt_topic_sensors, msg);
    }

    if (!success) {
        Serial.println("Gửi dữ liệu thất bại, đang thử kết nối lại...");
        reconnectMQTT();
        if (mqttClient.connected()) {
            success = mqttClient.publish(mqtt_topic_sensors, msg);
        }
    }

    if (success) {
        Serial.println("Gửi dữ liệu thành công");
    } else {
        Serial.println("Gửi dữ liệu thất bại sau khi thử lại");
    }
    return success;
}

void tryNetwork() {
    if (millis() - lastNetworkAttempt >= networkRetryInterval) {
        lastNetworkAttempt = millis();
        if (wifiMode == 2 && wifiEverConnected) {
            Serial.println("Đang kết nối lại WiFi...");
            WiFi.begin(ssid.c_str(), password.c_str());
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            Serial.println();
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("WiFi đã kết nối lại!");
                wifiMode = 1;
            } else if (!is4GConnected) {
                Serial.println("Đang thử 4G...");
                connect4G();
            }
        } else if (wifiMode == 2) {
            Serial.println("Đang kết nối lại WiFi...");
            WiFi.begin(ssid.c_str(), password.c_str());
        }
    }
}

void sendEmailAlert(const char* sensor, float value, float threshold) {
    unsigned long startTime = millis();
    Serial.println("EmailTask: Bắt đầu gửi email...");
    emailStatus = 1; // Đang gửi
    lastEmailStatusChange = millis();

    if (millis() - lastEmailSent < emailInterval) {
        Serial.println("EmailTask: Chưa đến thời gian gửi email tiếp theo");
        emailStatus = 0; // Reset nếu không gửi
        lastEmailStatusChange = millis();
        return;
    }

    if (WiFi.status() != WL_CONNECTED && !is4GConnected) {
        Serial.println("EmailTask: Không có kết nối mạng, không thể gửi email");
        emailStatus = 0;
        lastEmailStatusChange = millis();
        return;
    }

    smtp.debug(1);
    ESP_Mail_Session session;
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = AUTHOR_EMAIL;
    session.login.password = AUTHOR_PASSWORD;
    session.login.user_domain = "";

    SMTP_Message message;
    message.sender.name = "ESP32 Alert";
    message.sender.email = AUTHOR_EMAIL;
    message.subject = "Cảnh báo cảm biến vượt ngưỡng";
    message.addRecipient("Recipient", RECIPIENT_EMAIL);

    String textMsg = "Cảnh báo: ";
    textMsg += sensor;
    textMsg += " vượt ngưỡng!\nGiá trị hiện tại: ";
    textMsg += String(value, 1);
    textMsg += "\nNgưỡng an toàn: ";
    textMsg += String(threshold, 1);
    message.text.content = textMsg.c_str();
    message.text.charSet = "utf-8";
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    if (!smtp.connect(&session)) {
        Serial.println("EmailTask: Kết nối SMTP thất bại");
        Serial.println("EmailTask: Thời gian gửi email (thất bại): " + String(millis() - startTime) + " ms");
        emailStatus = 0;
        lastEmailStatusChange = millis();
        return;
    }

    if (!MailClient.sendMail(&smtp, &message)) {
        Serial.println("EmailTask: Gửi email thất bại: " + smtp.errorReason());
        emailStatus = 0;
    } else {
        Serial.println("EmailTask: Email cảnh báo đã được gửi thành công");
        emailStatus = 2; // Thành công
        lastEmailSent = millis();
    }
    lastEmailStatusChange = millis();
    smtp.closeSession();
    Serial.println("EmailTask: Thời gian gửi email: " + String(millis() - startTime) + " ms");
}

void emailTask(void *pvParameters) {
    while (true) {
        if (gasAlertTriggered && lastGasValue > MQ2_THRESHOLD) {
            sendEmailAlert("Khí gas", lastGasValue, MQ2_THRESHOLD);
            gasAlertTriggered = false;
        }
        if (tempAlertTriggered && lastTempValue > TEMP_THRESHOLD) {
            sendEmailAlert("Nhiệt độ", lastTempValue, TEMP_THRESHOLD);
            tempAlertTriggered = false;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void handleButtons() {
    static bool lastButtonAutoState = HIGH;
    static bool lastButtonManState = HIGH;
    static bool lastButtonFanState = HIGH;
    static bool lastButtonBuzzState = HIGH;
    static bool lastButtonPumpState = HIGH;

    bool buttonAutoState = digitalRead(BUTTON_AUTO);
    bool buttonManState = digitalRead(BUTTON_MAN);
    bool buttonFanState = digitalRead(BUTTON_FAN);
    bool buttonBuzzState = digitalRead(BUTTON_BUZZ);
    bool buttonPumpState = digitalRead(BUTTON_PUMP);

    if (buttonAutoState != lastButtonAutoState) {
        lastButtonAutoDebounce = millis();
    }
    if (millis() - lastButtonAutoDebounce >= DEBOUNCE_DELAY) {
        if (buttonAutoState == LOW) {
            if (buttonAutoPressTime == 0) buttonAutoPressTime = millis();
            if (millis() - buttonAutoPressTime >= HOLD_TIME) {
                if (!autoMode) {
                    digitalWrite(RELAY_FAN, LOW);
                    digitalWrite(RELAY_BUZZER, LOW);
                    digitalWrite(RELAY_PUMP, LOW);
                    manualFan = false;
                    manualBuzzer = false;
                    manualPump = false;
                    autoMode = true;
                    manualOverride = false;
                    Serial.println("Kích hoạt chế độ tự động từ nút nhấn");
                    sendMQTTData();
                }
                buttonAutoPressTime = 0;
            }
        } else {
            buttonAutoPressTime = 0;
        }
    }
    lastButtonAutoState = buttonAutoState;

    if (buttonManState != lastButtonManState) {
        lastButtonManDebounce = millis();
    }
    if (millis() - lastButtonManDebounce >= DEBOUNCE_DELAY) {
        if (buttonManState == LOW) {
            if (buttonManPressTime == 0) buttonManPressTime = millis();
            if (millis() - buttonManPressTime >= HOLD_TIME) {
                if (autoMode) {
                    digitalWrite(RELAY_FAN, LOW);
                    digitalWrite(RELAY_BUZZER, LOW);
                    digitalWrite(RELAY_PUMP, LOW);
                    fanActive = false;
                    buzzerActive = false;
                    pumpActive = false;
                    manualFan = false;
                    manualBuzzer = false;
                    manualPump = false;
                    autoMode = false;
                    manualOverride = false;
                    Serial.println("Kích hoạt chế độ thủ công từ nút nhấn");
                    sendMQTTData();
                }
                buttonManPressTime = 0;
            }
        } else {
            buttonManPressTime = 0;
        }
    }
    lastButtonManState = buttonManState;

    if (buttonFanState != lastButtonFanState) {
        lastButtonFanDebounce = millis();
    }
    if (millis() - lastButtonFanDebounce >= DEBOUNCE_DELAY) {
        if (buttonFanState == LOW) {
            if (buttonFanPressTime == 0) buttonFanPressTime = millis();
            if (millis() - buttonFanPressTime >= HOLD_TIME) {
                if (autoMode) {
                    fanActive = !fanActive;
                    digitalWrite(RELAY_FAN, fanActive);
                    manualOverride = true;
                    Serial.println(fanActive ? "Quạt BẬT từ nút nhấn (Tự động)" : "Quạt TẮT từ nút nhấn (Tự động)");
                } else {
                    manualFan = !manualFan;
                    digitalWrite(RELAY_FAN, manualFan);
                    Serial.println(manualFan ? "Quạt BẬT từ nút nhấn (Thủ công)" : "Quạt TẮT từ nút nhấn (Thủ công)");
                }
                buttonFanPressTime = 0;
                sendMQTTData();
            }
        } else {
            buttonFanPressTime = 0;
        }
    }
    lastButtonFanState = buttonFanState;

    if (buttonBuzzState != lastButtonBuzzState) {
        lastButtonBuzzDebounce = millis();
    }
    if (millis() - lastButtonBuzzDebounce >= DEBOUNCE_DELAY) {
        if (buttonBuzzState == LOW) {
            if (buttonBuzzPressTime == 0) buttonBuzzPressTime = millis();
            if (millis() - buttonBuzzPressTime >= HOLD_TIME) {
                if (autoMode) {
                    buzzerActive = !buzzerActive;
                    digitalWrite(RELAY_BUZZER, buzzerActive);
                    manualOverride = true;
                    Serial.println(buzzerActive ? "Còi BẬT từ nút nhấn (Tự động)" : "Còi TẮT từ nút nhấn (Tự động)");
                } else {
                    manualBuzzer = !manualBuzzer;
                    digitalWrite(RELAY_BUZZER, manualBuzzer);
                    Serial.println(manualBuzzer ? "Còi BẬT từ nút nhấn (Thủ công)" : "Còi TẮT từ nút nhấn (Thủ công)");
                }
                buttonBuzzPressTime = 0;
                sendMQTTData();
            }
        } else {
            buttonBuzzPressTime = 0;
        }
    }
    lastButtonBuzzState = buttonBuzzState;

    if (buttonPumpState != lastButtonPumpState) {
        lastButtonPumpDebounce = millis();
    }
    if (millis() - lastButtonPumpDebounce >= DEBOUNCE_DELAY) {
        if (buttonPumpState == LOW) {
            if (buttonPumpPressTime == 0) buttonPumpPressTime = millis();
            if (millis() - buttonPumpPressTime >= HOLD_TIME) {
                if (autoMode) {
                    pumpActive = !pumpActive;
                    digitalWrite(RELAY_PUMP, pumpActive);
                    manualOverride = true;
                    Serial.println(pumpActive ? "Bơm BẬT từ nút nhấn (Tự động)" : "Bơm TẮT từ nút nhấn (Tự động)");
                } else {
                    manualPump = !manualPump;
                    digitalWrite(RELAY_PUMP, manualPump);
                    Serial.println(manualPump ? "Bơm BẬT từ nút nhấn (Thủ công)" : "Bơm TẮT từ nút nhấn (Thủ công)");
                }
                buttonPumpPressTime = 0;
                sendMQTTData();
            }
        } else {
            buttonPumpPressTime = 0;
        }
    }
    lastButtonPumpState = buttonPumpState;
}

void handleAutoMode() {
    if (!autoMode || manualOverride) return;

    float mq2Value = getMQ2Value();
    float tempValue = getDS18B20Value();

    Serial.print("Debug AutoMode - MQ2: ");
    Serial.print(mq2Value);
    Serial.print(", Temp: ");
    Serial.println(tempValue);

    if (tempValue > TEMP_THRESHOLD && !tempAlertTriggered) {
        fanActive = true;
        digitalWrite(RELAY_FAN, HIGH);
        pumpActive = true;
        digitalWrite(RELAY_PUMP, HIGH);
        Serial.println("Nhiệt độ vượt ngưỡng: Quạt và Bơm BẬT");
        lastTempValue = tempValue;
        tempAlertTriggered = true;
    } else if (tempValue <= TEMP_THRESHOLD && tempAlertTriggered) {
        fanActive = false;
        digitalWrite(RELAY_FAN, LOW);
        pumpActive = false;
        digitalWrite(RELAY_PUMP, LOW);
        Serial.println("Nhiệt độ ổn định: Quạt và Bơm TẮT");
        tempAlertTriggered = false;
    }

    if (mq2Value > MQ2_THRESHOLD && !gasAlertTriggered) {
        buzzerActive = true;
        digitalWrite(RELAY_BUZZER, HIGH);
        Serial.println("Khí gas vượt ngưỡng: Còi BẬT");
        lastGasValue = mq2Value;
        gasAlertTriggered = true;
    } else if (mq2Value <= MQ2_THRESHOLD && gasAlertTriggered) {
        buzzerActive = false;
        digitalWrite(RELAY_BUZZER, LOW);
        Serial.println("Khí gas ổn định: Còi TẮT");
        gasAlertTriggered = false;
    }
}

void handleLED() {
    if (autoMode) {
        digitalWrite(LED_STATUS, HIGH);
    } else {
        unsigned long currentMillis = millis();
        if (currentMillis - previousLedMillis >= 500) {
            previousLedMillis = currentMillis;
            ledState = !ledState;
            digitalWrite(LED_STATUS, ledState);
        }
    }
}

void enterLightSleep() {
    if (sleepTime == 0) return;

    esp_sleep_enable_timer_wakeup(sleepTime);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_WAKEUP, 0);
    esp_sleep_enable_ext1_wakeup(BIT(MQ2_INTERRUPT_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);

    digitalWrite(RELAY_FAN, LOW);
    digitalWrite(RELAY_BUZZER, LOW);
    digitalWrite(RELAY_PUMP, LOW);
    fanActive = false;
    buzzerActive = false;
    pumpActive = false;
    if (!autoMode) {
        manualFan = false;
        manualBuzzer = false;
        manualPump = false;
    }

    if (!sendMQTTData()) {
        Serial.println("Gửi lần đầu thất bại, thử lại...");
        delay(1000);
        sendMQTTData();
    }

    if (oledInitialized) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(0, 20);
        u8g2.print("Sleep");
        u8g2.setCursor(0, 40);
        u8g2.print("Time: ");
        u8g2.print(sleepTime / 1000000);
        u8g2.print(" s");
        u8g2.sendBuffer();
    } else {
        Serial.println("OLED chưa khởi tạo, không hiển thị trạng thái ngủ");
    }

    Serial.print("Vào chế độ ngủ nhẹ trong ");
    Serial.print(sleepTime / 1000000);
    Serial.println(" giây...");
    
    esp_light_sleep_start();

    Serial.println("Thức dậy từ chế độ ngủ nhẹ!");
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
        mqttClient.loop();
    }

    if (receivedOff) {
        Serial.println("Xử lý lệnh TẮT sau khi thức dậy");
        sleepPending = false;
        sleepTime = 0;
        sleepCanceled = true;
        receivedOff = false;
        sendMQTTData();
        displayOLED(); // Cập nhật OLED sau khi xử lý lệnh OFF
    }

    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("Thức dậy bởi nút D32");
            sleepPending = false;
            displayOLED(); // Cập nhật OLED
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            Serial.println("Thức dậy bởi ngưỡng MQ2 (D15)");
            digitalWrite(RELAY_BUZZER, HIGH);
            buzzerActive = true;
            if (!autoMode) manualBuzzer = true;
            lastGasValue = getMQ2Value();
            gasAlertTriggered = true;
            if (!sendMQTTData()) {
                delay(1000);
                sendMQTTData();
            }
            sleepPending = false;
            displayOLED(); // Cập nhật OLED
            break;
        case ESP_SLEEP_WAKEUP_TIMER: {
            Serial.println("Thức dậy bởi bộ đếm thời gian");
            float mq2Value = getMQ2Value();
            float tempValue = getDS18B20Value();
            if (!sendMQTTData()) {
                delay(1000);
                sendMQTTData();
            }

            if (mq2Value > MQ2_THRESHOLD || tempValue > TEMP_THRESHOLD) {
                Serial.println("Cảm biến vượt ngưỡng, đang xử lý...");
                while (mq2Value > MQ2_THRESHOLD || tempValue > TEMP_THRESHOLD) {
                    if (mq2Value > MQ2_THRESHOLD && tempValue > TEMP_THRESHOLD) {
                        digitalWrite(RELAY_FAN, HIGH);
                        digitalWrite(RELAY_BUZZER, HIGH);
                        digitalWrite(RELAY_PUMP, HIGH);
                        fanActive = true;
                        buzzerActive = true;
                        pumpActive = true;
                        if (!autoMode) {
                            manualFan = true;
                            manualBuzzer = true;
                            manualPump = true;
                        }
                        Serial.println("Nhiệt độ và khí gas vượt ngưỡng: Quạt, Còi, Bơm BẬT");
                        if (!tempAlertTriggered && !gasAlertTriggered) {
                            lastTempValue = tempValue;
                            lastGasValue = mq2Value;
                            tempAlertTriggered = true;
                            gasAlertTriggered = true;
                        }
                    } else if (tempValue > TEMP_THRESHOLD) {
                        digitalWrite(RELAY_FAN, HIGH);
                        digitalWrite(RELAY_PUMP, HIGH);
                        fanActive = true;
                        pumpActive = true;
                        if (!autoMode) {
                            manualFan = true;
                            manualPump = true;
                        }
                        Serial.println("Nhiệt độ vượt ngưỡng: Quạt và Bơm BẬT");
                        if (!tempAlertTriggered) {
                            lastTempValue = tempValue;
                            tempAlertTriggered = true;
                        }
                    } else if (mq2Value > MQ2_THRESHOLD) {
                        digitalWrite(RELAY_BUZZER, HIGH);
                        buzzerActive = true;
                        if (!autoMode) manualBuzzer = true;
                        Serial.println("Khí gas vượt ngưỡng: Còi BẬT");
                        if (!gasAlertTriggered) {
                            lastGasValue = mq2Value;
                            gasAlertTriggered = true;
                        }
                    }
                    if (!sendMQTTData()) {
                        Serial.println("Gửi thất bại khi xử lý ngưỡng, thử lại...");
                        delay(1000);
                        sendMQTTData();
                    }
                    displayOLED(); // Cập nhật OLED trong vòng lặp
                    delay(1000);
                    mq2Value = getMQ2Value();
                    tempValue = getDS18B20Value();
                }
                Serial.println("Cảm biến ổn định, tắt các thiết bị");
                digitalWrite(RELAY_FAN, LOW);
                digitalWrite(RELAY_BUZZER, LOW);
                digitalWrite(RELAY_PUMP, LOW);
                fanActive = false;
                buzzerActive = false;
                pumpActive = false;
                if (!autoMode) {
                    manualFan = false;
                    manualBuzzer = false;
                    manualPump = false;
                }
                tempAlertTriggered = false;
                gasAlertTriggered = false;
                if (!sendMQTTData()) {
                    Serial.println("Gửi thất bại sau khi ổn định, thử lại...");
                    delay(1000);
                    sendMQTTData();
                }
                displayOLED(); // Cập nhật OLED sau khi ổn định
            } else {
                Serial.println("Cảm biến bình thường, không cần hành động");
                displayOLED(); // Cập nhật OLED ngay cả khi không vượt ngưỡng
            }
            if (sleepPending && !sleepCanceled && sleepTime > 0) {
                Serial.println("Cảm biến ổn định, quay lại ngủ...");
                enterLightSleep();
            }
            break;
        }
        default:
            Serial.println("Thức dậy bởi lý do khác");
            sleepPending = false;
            displayOLED(); // Cập nhật OLED
            break;
    }
    sleepPending = false;
    if (!sendMQTTData()) {
        delay(1000);
        sendMQTTData();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Bắt đầu thiết lập...");

    pinMode(MQ2_PIN, INPUT);
    pinMode(MQ2_INTERRUPT_PIN, INPUT);
    ds18b20.begin();

    pinMode(BUTTON_AUTO, INPUT_PULLUP);
    pinMode(BUTTON_MAN, INPUT_PULLUP);
    pinMode(BUTTON_FAN, INPUT_PULLUP);
    pinMode(BUTTON_BUZZ, INPUT_PULLUP);
    pinMode(BUTTON_PUMP, INPUT_PULLUP);
    pinMode(BUTTON_WAKEUP, INPUT_PULLUP);

    pinMode(RELAY_FAN, OUTPUT);
    pinMode(RELAY_BUZZER, OUTPUT);
    pinMode(RELAY_PUMP, OUTPUT);
    pinMode(LED_STATUS, OUTPUT);

    digitalWrite(RELAY_FAN, LOW);
    digitalWrite(RELAY_BUZZER, LOW);
    digitalWrite(RELAY_PUMP, LOW);
    digitalWrite(LED_STATUS, LOW);

    if (!u8g2.begin()) {
        oledInitialized = false;
        Serial.println("Khởi tạo OLED thất bại");
    } else {
        oledInitialized = true;
        displayResetCountdown();
    }

    WiFi.onEvent(WiFiEvent);
    wifiConfig.begin();
    setupMQTT();

    xTaskCreate(emailTask, "EmailTask", 8192, NULL, 1, NULL);
    Serial.println("Task EmailTask đã được tạo");

    Serial.println("Thiết lập hoàn tất, chế độ tự động ban đầu: " + String(autoMode));
}

void loop() {
    checkButtonReset();
    wifiConfig.run();
    tryNetwork();
    handleButtons();

    float mq2Value = getMQ2Value();
    float tempValue = getDS18B20Value();

    if (autoMode) {
        handleAutoMode();
    } else {
        if (mq2Value > MQ2_THRESHOLD && !gasAlertTriggered) {
            manualBuzzer = true;
            digitalWrite(RELAY_BUZZER, HIGH);
            Serial.println("Khí gas vượt ngưỡng (Thủ công): Còi BẬT");
            lastGasValue = mq2Value;
            gasAlertTriggered = true;
        } else if (mq2Value <= MQ2_THRESHOLD && gasAlertTriggered) {
            manualBuzzer = false;
            digitalWrite(RELAY_BUZZER, LOW);
            Serial.println("Khí gas ổn định (Thủ công): Còi TẮT");
            gasAlertTriggered = false;
        }

        if (tempValue > TEMP_THRESHOLD && !tempAlertTriggered) {
            manualFan = true;
            manualPump = true;
            digitalWrite(RELAY_FAN, HIGH);
            digitalWrite(RELAY_PUMP, HIGH);
            Serial.println("Nhiệt độ vượt ngưỡng (Thủ công): Quạt và Bơm BẬT");
            lastTempValue = tempValue;
            tempAlertTriggered = true;
        } else if (tempValue <= TEMP_THRESHOLD && tempAlertTriggered) {
            manualFan = false;
            manualPump = false;
            digitalWrite(RELAY_FAN, LOW);
            digitalWrite(RELAY_PUMP, LOW);
            Serial.println("Nhiệt độ ổn định (Thủ công): Quạt và Bơm TẮT");
            tempAlertTriggered = false;
        }
    }

    handleLED();

    if (WiFi.status() == WL_CONNECTED || is4GConnected) {
        if (!mqttClient.connected()) {
            reconnectMQTT();
        } else {
            mqttClient.loop();
        }
    } else {
        Serial.print(".");
        delay(1000);
    }

    unsigned long now = millis();
    if (now - lastMsg > 5000 && mqttClient.connected()) {
        lastMsg = now;
        sendMQTTData();
    }
    displayOLED();
}