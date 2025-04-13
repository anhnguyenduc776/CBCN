#ifndef WIFICONFIG_H
#define WIFICONFIG_H

#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ticker.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsm.h>

#define SIM7600_RX 16
#define SIM7600_TX 17
#define SIM7600_PWR 4

const char apn[] = "v-internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

WebServer webServer(80);
Ticker blinker;
HardwareSerial SerialSIM7600(1);
TinyGsm modem(SerialSIM7600);

String ssid;
String password;
#define WIFI_LED_PIN 2
#define btnPin 0 // Nút BOOT trên ESP32
#define PUSHTIME 5000
volatile unsigned long buttonPressTime = 0;
volatile bool buttonPressed = false;
int wifiMode = 0;
unsigned long blinkTime = millis();
bool is4GConnected = false;
bool wifiEverConnected = false;

const char html[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Cấu Hình WiFi</title>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@400;500;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Roboto', sans-serif;
        }
        body {
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            background: linear-gradient(135deg, #00B4DB, #0083B0);
            padding: 20px;
            position: relative;
            overflow: hidden;
        }
        body::before {
            content: '';
            position: absolute;
            top: -50%;
            left: -50%;
            width: 200%;
            height: 200%;
            background: radial-gradient(circle, rgba(255,255,255,0.1) 0%, transparent 50%);
            animation: gradient 15s ease infinite;
        }
        @keyframes gradient {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .container {
            background: rgba(255, 255, 255, 0.95);
            padding: 35px;
            border-radius: 24px;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.2);
            width: 100%;
            max-width: 420px;
            backdrop-filter: blur(10px);
            position: relative;
            z-index: 1;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        .header {
            text-align: center;
            margin-bottom: 35px;
            position: relative;
        }
        .header::after {
            content: '';
            position: absolute;
            bottom: -15px;
            left: 50%;
            transform: translateX(-50%);
            width: 60px;
            height: 4px;
            background: linear-gradient(to right, #00B4DB, #0083B0);
            border-radius: 2px;
        }
        .header-icon {
            font-size: 52px;
            background: linear-gradient(45deg, #00B4DB, #0083B0);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 20px;
            animation: float 3s ease-in-out infinite;
        }
        @keyframes float {
            0% { transform: translateY(0px) rotate(0deg); }
            50% { transform: translateY(-10px) rotate(5deg); }
            100% { transform: translateY(0px) rotate(0deg); }
        }
        h3 {
            color: #1a1a1a;
            font-size: 26px;
            font-weight: 700;
            margin-bottom: 15px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        #info {
            color: #666;
            font-size: 15px;
            padding: 12px 15px;
            background: #f0f7ff;
            border-radius: 12px;
            margin-bottom: 30px;
            border: 1px solid #cce4ff;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            transition: all 0.3s ease;
        }
        .form-group {
            margin-bottom: 25px;
            position: relative;
        }
        .form-group i.field-icon {
            position: absolute;
            left: 15px;
            top: 50%;
            transform: translateY(-50%);
            color: #0083B0;
            font-size: 20px;
            transition: all 0.3s ease;
        }
        label {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #333;
            font-size: 16px;
            font-weight: 500;
            margin-bottom: 10px;
        }
        label i {
            color: #00B4DB;
        }
        select, input {
            width: 100%;
            padding: 14px 45px;
            border: 2px solid #e1e1e1;
            border-radius: 12px;
            font-size: 15px;
            transition: all 0.3s ease;
            background: white;
        }
        select:focus, input:focus {
            border-color: #00B4DB;
            outline: none;
            box-shadow: 0 0 0 4px rgba(0, 180, 219, 0.1);
        }
        select:focus + i.field-icon,
        input:focus + i.field-icon {
            color: #00B4DB;
        }
        .password-field {
            position: relative;
        }
        .password-toggle {
            position: absolute;
            right: 15px;
            top: 50%;
            transform: translateY(-50%);
            cursor: pointer;
            color: #666;
            font-size: 18px;
            padding: 5px;
            transition: all 0.3s ease;
        }
        .password-toggle:hover {
            color: #00B4DB;
        }
        .button-group {
            display: flex;
            gap: 15px;
            margin-top: 35px;
        }
        button {
            flex: 1;
            padding: 14px;
            border: none;
            border-radius: 12px;
            font-size: 15px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            position: relative;
            overflow: hidden;
        }
        button::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(
                120deg,
                transparent,
                rgba(255, 255, 255, 0.3),
                transparent
            );
            transition: 0.5s;
        }
        button:hover::before {
            left: 100%;
        }
        button:hover {
            transform: translateY(-2px);
        }
        #saveBtn {
            background: linear-gradient(45deg, #00B4DB, #0083B0);
            color: white;
            box-shadow: 0 4px 15px rgba(0, 180, 219, 0.3);
        }
        #restartBtn {
            background: linear-gradient(45deg, #FF416C, #FF4B2B);
            color: white;
            box-shadow: 0 4px 15px rgba(255, 65, 108, 0.3);
        }
        .loading {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 3px solid rgba(255,255,255,0.3);
            border-radius: 50%;
            border-top: 3px solid #fff;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        @media (max-width: 480px) {
            .container {
                padding: 25px;
            }
            button {
                padding: 12px;
                font-size: 14px;
            }
        }
        .network-strength {
            display: inline-flex;
            align-items: center;
            gap: 5px;
            margin-left: 10px;
            color: #666;
        }
        .strength-bar {
            width: 4px;
            height: 8px;
            background: #ddd;
            border-radius: 2px;
        }
        .strength-bar.active {
            background: #00B4DB;
        }
        .tooltip {
            position: relative;
            display: inline-block;
        }
        .tooltip .tooltiptext {
            visibility: hidden;
            width: 200px;
            background-color: rgba(0, 0, 0, 0.8);
            color: #fff;
            text-align: center;
            border-radius: 6px;
            padding: 8px;
            position: absolute;
            z-index: 1;
            bottom: 125%;
            left: 50%;
            transform: translateX(-50%);
            opacity: 0;
            transition: opacity 0.3s;
            font-size: 12px;
        }
        .tooltip:hover .tooltiptext {
            visibility: visible;
            opacity: 1;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="header-icon">
                <i class="fas fa-wifi"></i>
            </div>
            <h3>Cấu Hình WiFi</h3>
            <p id="info">
                <i class="fas fa-spinner fa-spin"></i>
                Đang quét mạng WiFi...
            </p>
        </div>
        
        <div class="form-group">
            <label>
                <i class="fas fa-broadcast-tower"></i>
                Tên Mạng WiFi
            </label>
            <select id="ssid">
                <option value="">Chọn mạng WiFi...</option>
            </select>
            <i class="fas fa-network-wired field-icon"></i>
        </div>

        <div class="form-group">
            <label>
                <i class="fas fa-key"></i>
                Mật Khẩu
                <div class="tooltip">
                    <i class="fas fa-info-circle"></i>
                    <span class="tooltiptext">Mật khẩu phải có ít nhất 8 ký tự</span>
                </div>
            </label>
            <div class="password-field">
                <input id="password" type="password" placeholder="Nhập mật khẩu WiFi">
                <i class="fas fa-lock field-icon"></i>
                <i class="fas fa-eye password-toggle" onclick="togglePassword()" title="Hiện/Ẩn mật khẩu"></i>
            </div>
        </div>

        <div class="button-group">
            <button id="saveBtn" onclick="saveWifi()">
                <i class="fas fa-save"></i>
                Lưu Cấu Hình
            </button>
            <button id="restartBtn" onclick="reStart()">
                <i class="fas fa-sync-alt"></i>
                Khởi Động Lại
            </button>
        </div>
    </div>

    <script>
        window.onload = function() {
            scanWifi();
        }
        
        var xhttp = new XMLHttpRequest();
        
        function scanWifi() {
            xhttp.onreadystatechange = function() {
                if(xhttp.readyState == 4 && xhttp.status == 200) {
                    var data = xhttp.responseText;
                    var info = document.getElementById("info");
                    info.innerHTML = '<i class="fas fa-check-circle"></i> Đã tìm thấy các mạng WiFi';
                    info.style.background = "#e8f5e9";
                    info.style.borderColor = "#c8e6c9";
                    info.style.color = "#2e7d32";
                    
                    var obj = JSON.parse(data);
                    var select = document.getElementById("ssid");
                    select.innerHTML = '<option value="">Chọn mạng WiFi...</option>';
                    
                    obj.forEach(function(network) {
                        var strength = Math.floor(Math.random() * 4) + 1; // Giả lập cường độ tín hiệu
                        var option = document.createElement('option');
                        option.value = network;
                        option.innerHTML = network + createSignalStrength(strength);
                        select.appendChild(option);
                    });
                }
            }
            xhttp.open("GET", "/scanWifi", true);
            xhttp.send();
        }

        function createSignalStrength(strength) {
            var bars = '';
            for(var i = 1; i <= 4; i++) {
                bars += `<span class="strength-bar${i <= strength ? ' active' : ''}"></span>`;
            }
            return `<span class="network-strength">${bars}</span>`;
        }

        function togglePassword() {
            var passwordInput = document.getElementById("password");
            var toggleIcon = document.querySelector(".password-toggle");
            
            if (passwordInput.type === "password") {
                passwordInput.type = "text";
                toggleIcon.classList.remove("fa-eye");
                toggleIcon.classList.add("fa-eye-slash");
                toggleIcon.title = "Ẩn mật khẩu";
            } else {
                passwordInput.type = "password";
                toggleIcon.classList.remove("fa-eye-slash");
                toggleIcon.classList.add("fa-eye");
                toggleIcon.title = "Hiện mật khẩu";
            }
        }

        function saveWifi() {
            var ssid = document.getElementById("ssid").value;
            var pass = document.getElementById("password").value;
            
            if(!ssid) {
                alert("Vui lòng chọn mạng WiFi!");
                return;
            }

            if(pass.length < 8) {
                alert("Mật khẩu phải có ít nhất 8 ký tự!");
                return;
            }
            
            var saveBtn = document.getElementById("saveBtn");
            saveBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Đang lưu...';
            saveBtn.disabled = true;
            
            xhttp.onreadystatechange = function() {
                if(xhttp.readyState == 4 && xhttp.status == 200) {
                    alert("Đã lưu thông tin WiFi thành công!");
                    saveBtn.innerHTML = '<i class="fas fa-save"></i> Lưu Cấu Hình';
                    saveBtn.disabled = false;
                }
            }
            xhttp.open("GET", "/saveWifi?ssid=" + encodeURIComponent(ssid) + "&pass=" + encodeURIComponent(pass), true);
            xhttp.send();
        }

        function reStart() {
            if(confirm("Bạn có chắc chắn muốn khởi động lại thiết bị?")) {
                var restartBtn = document.getElementById("restartBtn");
                restartBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Đang khởi động lại...';
                restartBtn.disabled = true;
                
                xhttp.onreadystatechange = function() {
                    if(xhttp.readyState == 4 && xhttp.status == 200) {
                        alert("Thiết bị đang khởi động lại!");
                    }
                }
                xhttp.open("GET", "/reStart", true);
                xhttp.send();
            }
        }
    </script>
</body>
</html>
)html";

void blinkLed(uint32_t t) {
  if (millis() - blinkTime > t) {
    digitalWrite(WIFI_LED_PIN, !digitalRead(WIFI_LED_PIN));
    blinkTime = millis();
  }
}

void ledControl() {
  if (wifiMode == 0) blinkLed(50);
  else if (wifiMode == 1) blinkLed(3000);
  else if (wifiMode == 2) blinkLed(300);
  else if (wifiMode == 3) blinkLed(100);
}

IRAM_ATTR void handleButtonInterrupt() {
  if (digitalRead(btnPin) == LOW) {
    buttonPressTime = millis();
    buttonPressed = true;
  }
}

void disconnect4G() {
  if (is4GConnected) {
    Serial.println("Ngắt kết nối 4G...");
    modem.gprsDisconnect();
    is4GConnected = false;
    Serial.println("Đã ngắt kết nối 4G");
  }
}

void checkButtonReset() {
  if (buttonPressed && digitalRead(btnPin) == LOW) {
    if (millis() - buttonPressTime >= PUSHTIME) {
      Serial.println("Nút được giữ trong 5 giây - Đặt lại về mặc định!");
      disconnect4G();
      wifiEverConnected = false;
      for (int i = 0; i < 100; i++) EEPROM.write(i, 0);
      EEPROM.commit();
      Serial.println("Đã xóa bộ nhớ EEPROM!");
      delay(1000);
      ESP.restart();
    }
  } else if (digitalRead(btnPin) == HIGH) {
    buttonPressed = false;
    buttonPressTime = 0;
  }
}

void connect4G() {
  if (!is4GConnected) {
    Serial.println("Chuyển sang 4G (Viettel)...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println("Không thể kết nối với Viettel 4G");
      wifiMode = 2;
      return;
    }
    Serial.println("Đã kết nối với Viettel 4G");
    Serial.print("Địa chỉ IP: ");
    Serial.println(modem.localIP());
    wifiMode = 3;
    is4GConnected = true;
  }
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("Đã kết nối với WiFi");
      Serial.print("Địa chỉ IP: ");
      Serial.println(WiFi.localIP());
      wifiMode = 1;
      wifiEverConnected = true;
      if (is4GConnected) disconnect4G();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Mất kết nối WiFi");
      wifiMode = 2;
      if (ssid != "") {
        WiFi.begin(ssid.c_str(), password.c_str());
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
          delay(500);
          Serial.print(".");
          attempts++;
        }
        Serial.println();
        if (WiFi.status() != WL_CONNECTED && !is4GConnected) {
          Serial.println("Không thể kết nối lại WiFi, thử 4G...");
          connect4G();
        }
      }
      break;
    default:
      break;
  }
}

void createAccessPoint() {
  Serial.println("Đã tạo mạng wifi ESP32!");
  WiFi.mode(WIFI_AP);
  uint8_t macAddr[6];
  WiFi.softAPmacAddress(macAddr);
  String ssid_ap = "ESP32-" + String(macAddr[4], HEX) + String(macAddr[5], HEX);
  ssid_ap.toUpperCase();
  WiFi.softAP(ssid_ap.c_str());
  Serial.println("Tên điểm truy cập: " + ssid_ap);
  Serial.println("Địa chỉ truy cập máy chủ web: " + WiFi.softAPIP().toString());
  wifiMode = 0;
}

void setupWifi() {
  if (ssid != "") {
    Serial.println("Đang kết nối với wifi...!");
    Serial.print("SSID từ EEPROM: '"); Serial.print(ssid); Serial.println("'");
    Serial.print("Password từ EEPROM: '"); Serial.print(password); Serial.println("'");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(1000);

    WiFi.begin(ssid.c_str(), password.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Đã kết nối với WiFi!");
      Serial.print("Địa chỉ IP: ");
      Serial.println(WiFi.localIP());
      wifiMode = 1;
      wifiEverConnected = true;
    } else {
      Serial.println("Không thể kết nối WiFi, trạng thái: " + String(WiFi.status()));
      createAccessPoint();
    }
  } else {
    Serial.println("SSID trống, tạo điểm truy cập...");
    createAccessPoint();
  }
}

void setupWebServer() {
  webServer.on("/", [] { webServer.send(200, "text/html", html); });
  webServer.on("/scanWifi", [] {
    Serial.println("Đang quét mạng wifi...!");
    int wifi_nets = WiFi.scanNetworks(true, true);
    const unsigned long t = millis();
    while (wifi_nets < 0 && millis() - t < 10000) {
      delay(20);
      wifi_nets = WiFi.scanComplete();
    }
    JsonDocument doc;
    for (int i = 0; i < wifi_nets; ++i) doc.add(WiFi.SSID(i));
    String wifiList = "";
    serializeJson(doc, wifiList);
    Serial.println("Danh sách wifi: " + wifiList);
    webServer.send(200, "application/json", wifiList);
  });
  webServer.on("/saveWifi", [] {
    String ssid_temp = webServer.arg("ssid");
    String password_temp = webServer.arg("pass");
    Serial.println("SSID: " + ssid_temp);
    Serial.println("PASS: " + password_temp);
    EEPROM.writeString(0, ssid_temp);
    EEPROM.writeString(32, password_temp);
    EEPROM.commit();
    webServer.send(200, "text/plain", "Đã lưu wifi!");
  });
  webServer.on("/reStart", [] {
    webServer.send(200, "text/plain", "Esp32 đang khởi động lại!");
    delay(3000);
    ESP.restart();
  });
  webServer.begin();
}

class Config {
public:
  void begin() {
    pinMode(WIFI_LED_PIN, OUTPUT);
    pinMode(btnPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(btnPin), handleButtonInterrupt, FALLING);
    blinker.attach_ms(50, ledControl);
    EEPROM.begin(100);
    char ssid_temp[32], password_temp[64];
    EEPROM.readString(0, ssid_temp, sizeof(ssid_temp));
    EEPROM.readString(32, password_temp, sizeof(password_temp));
    ssid = String(ssid_temp);
    password = String(password_temp);
    if (ssid != "") {
      Serial.println("Tên wifi: " + ssid);
      Serial.println("Mật khẩu: " + password);
    }
    SerialSIM7600.begin(115200, SERIAL_8N1, SIM7600_RX, SIM7600_TX);
    pinMode(SIM7600_PWR, OUTPUT);
    digitalWrite(SIM7600_PWR, HIGH);
    delay(1000);
    digitalWrite(SIM7600_PWR, LOW);
    if (!modem.restart()) Serial.println("Không thể khởi động lại SIM7600C");
    else Serial.println("Đã khởi tạo SIM7600C");
    setupWifi();
    if (wifiMode == 0) setupWebServer();
  }

  void run() {
    if (wifiMode == 0) webServer.handleClient();
  }
};

extern Config wifiConfig;

#endif // WIFICONFIG_H