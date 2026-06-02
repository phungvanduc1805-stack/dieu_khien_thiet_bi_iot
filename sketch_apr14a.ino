  #define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
  #define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
  #define BLYNK_AUTH_TOKEN    "hEmZPxReKFVWnoXokouhldr_vz49x9iy"

  #include <WiFi.h>
  #include <BlynkSimpleEsp32.h>

  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #include <DHT.h>

  // ================= WIFI =================
  char ssid[] = "khu tự trị 41";
  char pass[] = "diencaothe";

  // ================= OLED =================
  #define SCREEN_WIDTH 128
  #define SCREEN_HEIGHT 64
  #define OLED_RESET -1

  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

  // ================= DHT11 =================
  #define DHTPIN 4
  #define DHTTYPE DHT11

  DHT dht(DHTPIN, DHTTYPE);

  // ================= MQ135 =================
  #define MQ135_PIN 34

  // ================= LED + BUZZER + Quat =================
  #define LED_PIN 18
  #define BUZZER_PIN 19
  #define SWITCH_PIN 27

  // Buzzer module 3 chân thường là active buzzer.
  // Nếu buzzer không kêu, đổi true thành false.
  #define ACTIVE_BUZZER false

  // ================= NGƯỠNG CẢNH BÁO =================
  #define TEMP_HIGH_THRESHOLD 40.0
  #define TEMP_RESET_THRESHOLD 33.0

  #define GAS_HIGH_THRESHOLD 800
  #define GAS_RESET_THRESHOLD 600

  #define TEMP_RISE_THRESHOLD 5.0
  #define GAS_RISE_THRESHOLD 300

  #define WARNING_DELAY_MS 10000

  // ================= BIẾN DỮ LIỆU CHUNG =================
  float currentTemp = 0;
  float currentHum = 0;
  int currentGas = 0;

  float lastTemp = 0;
  int lastGas = 0;

  bool sensorError = false;
  bool warningState = false;
  bool lastButtonState = HIGH;

  bool tempAlertSent = false;
  bool gasAlertSent = false;
  bool alarmMuted = false;
  bool testAlarm = false;

  unsigned long tempStartTime = 0;
  unsigned long gasStartTime = 0;

  // Mutex bảo vệ dữ liệu dùng chung
  SemaphoreHandle_t dataMutex;

  // ================= HÀM BUZZER =================
  void buzzerOn() {
  #if ACTIVE_BUZZER
    digitalWrite(BUZZER_PIN, HIGH);
  #else
    ledcWriteTone(BUZZER_PIN, 1000);
  #endif
  }

  void buzzerOff() {
  #if ACTIVE_BUZZER
    digitalWrite(BUZZER_PIN, LOW);
  #else
    ledcWriteTone(BUZZER_PIN, 0);
  #endif
  }

  // ================= HÀM ĐIỀU KHIỂN CẢNH BÁO =================
  void setAlertOutput(bool state) {
    if (state) {
      digitalWrite(LED_PIN, HIGH);
      buzzerOn();
    } else {
      digitalWrite(LED_PIN, LOW);
      buzzerOff();
    }
  }

  // ================= TASK 1: ĐỌC CẢM BIẾN =================
  void SensorTask(void *pvParameters) {
    while (true) {
      float temp = dht.readTemperature();
      float hum = dht.readHumidity();
      int gas = analogRead(MQ135_PIN);

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (isnan(temp) || isnan(hum)) {
          sensorError = true;
        } else {
          sensorError = false;
          currentTemp = temp;
          currentHum = hum;
          currentGas = gas;
        }

        xSemaphoreGive(dataMutex);
      }

      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }

  // ================= TASK 2: XỬ LÝ CẢNH BÁO =================
  void AlertTask(void *pvParameters) {
    while (true) {
      float temp = 0;
      int gas = 0;
      bool error = false;

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        temp = currentTemp;
        gas = currentGas;
        error = sensorError;
        xSemaphoreGive(dataMutex);
      }

      bool warning = false;

      if (error) {
        warning = true;
      } else {
        // ===== CẢNH BÁO NHIỆT ĐỘ CAO DUY TRÌ =====
        if (temp > TEMP_HIGH_THRESHOLD) {
          if (tempStartTime == 0) {
            tempStartTime = millis();
          }

          if (millis() - tempStartTime > WARNING_DELAY_MS) {
            warning = true;

            if (!tempAlertSent && Blynk.connected()) {
              Blynk.logEvent("warning", "Nhiet do qua cao!");
              tempAlertSent = true;
            }
          }
        } else {
          tempStartTime = 0;

          if (temp < TEMP_RESET_THRESHOLD) {
            tempAlertSent = false;
          }
        }

        // ===== CẢNH BÁO NHIỆT ĐỘ TĂNG ĐỘT BIẾN =====
        if (lastTemp != 0 && temp > TEMP_HIGH_THRESHOLD && (temp - lastTemp) > TEMP_RISE_THRESHOLD) {
          warning = true;

          if (!tempAlertSent && Blynk.connected()) {
            Blynk.logEvent("warning", "Nhiet do tang dot bien!");
            tempAlertSent = true;
          }
        }

        // ===== CẢNH BÁO KHÍ GAS CAO DUY TRÌ =====
        if (gas > GAS_HIGH_THRESHOLD) {
          if (gasStartTime == 0) {
            gasStartTime = millis();
          }

          if (millis() - gasStartTime > WARNING_DELAY_MS) {
            warning = true;

            if (!gasAlertSent && Blynk.connected()) {
              Blynk.logEvent("warning", "Khi gas cao!");
              gasAlertSent = true;
            }
          }
        } else {
          gasStartTime = 0;

          if (gas < GAS_RESET_THRESHOLD) {
            gasAlertSent = false;
          }
        }

        // ===== CẢNH BÁO KHÍ GAS TĂNG ĐỘT BIẾN =====
        if (lastGas != 0 && (gas - lastGas) > GAS_RISE_THRESHOLD) {
          warning = true;

          if (!gasAlertSent && Blynk.connected()) {
            Blynk.logEvent("warning", "Khi gas tang dot bien!");
            gasAlertSent = true;
          }
        }

        lastTemp = temp;
        lastGas = gas;
      }

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        warningState = warning;
        xSemaphoreGive(dataMutex);
      }
      // Nhấn công tắc để tắt cảnh báo hiện tại
      
      bool buttonState = digitalRead(SWITCH_PIN);

      if(lastButtonState == HIGH && buttonState == LOW){
        if(warning){
            // Đang có cảnh báo -> tắt cảnh báo
            alarmMuted = !alarmMuted;
        }
        else{
            // Không có cảnh báo -> bật/tắt chế độ test còi
            testAlarm = !testAlarm;
        }
      }

      lastButtonState = buttonState;

      if(!warning){
          alarmMuted = false;
      }

      if(warning && !alarmMuted){
        setAlertOutput(true);
      }
      else if(testAlarm){
        setAlertOutput(true);
      }
      else{
        setAlertOutput(false);
      }

      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  // ================= TASK 3: HIỂN THỊ OLED =================
  void DisplayTask(void *pvParameters) {
    while (true) {
      float temp = 0;
      float hum = 0;
      int gas = 0;
      bool warning = false;
      bool error = false;

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        temp = currentTemp;
        hum = currentHum;
        gas = currentGas;
        warning = warningState;
        error = sensorError;
        xSemaphoreGive(dataMutex);
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(0, 0);
      display.println("HE THONG CANH BAO");

      display.setCursor(0, 14);

      if (error) {
        display.println("LOI CAM BIEN!");
        display.println("KIEM TRA DHT11");
      } else {
        display.print("Nhiet do: ");
        display.print(temp);
        display.println(" C");

        display.print("Do am: ");
        display.print(hum);
        display.println(" %");

        display.print("Air: ");
        display.println(gas);

      }

      display.println();

      if (warning) {
        display.println("CANH BAO");
      }

      display.display();

      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // ================= TASK 4: GỬI DỮ LIỆU LÊN BLYNK =================
  void CloudTask(void *pvParameters) {
    unsigned long lastSendTime = 0;

    while (true) {
      Blynk.run();

      if (millis() - lastSendTime >= 2000) {
        lastSendTime = millis();

        float temp = 0;
        float hum = 0;
        int gas = 0;
        bool warning = false;
        bool error = false;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
          temp = currentTemp;
          hum = currentHum;
          gas = currentGas;
          warning = warningState;
          error = sensorError;
          xSemaphoreGive(dataMutex);
        }

        if (Blynk.connected() && !error) {
          Blynk.virtualWrite(V0, temp);
          Blynk.virtualWrite(V1, hum);
          Blynk.virtualWrite(V2, gas);
          Blynk.virtualWrite(V3, warning ? 1 : 0);
          
        }

        Serial.println("===== DATA =====");

        if (error) {
          Serial.println("Loi cam bien DHT11");
        } else {
          Serial.print("Nhiet do: ");
          Serial.println(temp);

          Serial.print("Do am: ");
          Serial.println(hum);

          Serial.print("Gas: ");
          Serial.println(gas);
        }

        Serial.print("Canh bao: ");
        Serial.println(warning ? "CO" : "KHONG");

        Serial.print("Blynk: ");
        Serial.println(Blynk.connected() ? "Online" : "Offline");
      }

      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  // ================= TASK 5: GIÁM SÁT WIFI / BLYNK =================
  void MonitorTask(void *pvParameters) {
    while (true) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Mat Wi-Fi, dang ket noi lai...");
        WiFi.disconnect();
        WiFi.begin(ssid, pass);
      }

      if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
        Serial.println("Dang ket noi lai Blynk...");
        Blynk.connect(3000);
      }

      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }

  // ================= SETUP =================
  void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(SWITCH_PIN, INPUT_PULLUP);

    digitalWrite(LED_PIN, LOW);
    buzzerOff();

  #if !ACTIVE_BUZZER
    ledcAttach(BUZZER_PIN, 1000, 8);
  #endif

    dht.begin();

    Wire.begin(21, 22);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println("OLED loi!");
      while (true);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Khoi dong...");
    display.println("ESP32 + FreeRTOS");
    display.display();

    dataMutex = xSemaphoreCreateMutex();

    if (dataMutex == NULL) {
      Serial.println("Loi tao Mutex!");
      while (true);
    }

    WiFi.begin(ssid, pass);

    Serial.println("Dang ket noi Wi-Fi...");

    unsigned long wifiStart = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
      delay(500);
      Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Da ket noi Wi-Fi");
    } else {
      Serial.println("Khong ket noi Wi-Fi, he thong van chay local");
    }

    Blynk.config(BLYNK_AUTH_TOKEN);

    if (WiFi.status() == WL_CONNECTED) {
      Blynk.connect(3000);
    }

    // ================= TẠO TASK FREERTOS =================
    xTaskCreatePinnedToCore(
      SensorTask,
      "SensorTask",
      4096,
      NULL,
      3,
      NULL,
      1
    );

    xTaskCreatePinnedToCore(
      AlertTask,
      "AlertTask",
      4096,
      NULL,
      4,
      NULL,
      1
    );

    xTaskCreatePinnedToCore(
      DisplayTask,
      "DisplayTask",
      4096,
      NULL,
      2,
      NULL,
      1
    );

    xTaskCreatePinnedToCore(
      CloudTask,
      "CloudTask",
      8192,
      NULL,
      1,
      NULL,
      0
    );

    xTaskCreatePinnedToCore(
      MonitorTask,
      "MonitorTask",
      4096,
      NULL,
      2,
      NULL,
      0
    );

    Serial.println("He thong da san sang");
  }

  // ================= LOOP =================
  void loop() {
    // Không xử lý trong loop.
    // Toàn bộ hệ thống chạy bằng FreeRTOS Task.
    vTaskDelay(pdMS_TO_TICKS(1000));
  }