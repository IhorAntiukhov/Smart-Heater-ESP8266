#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Firebase_ESP_Client.h>

#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <FS.h>
#include <LittleFS.h>

#include "Helper.h"

/* Предупреждение: ниже указаны GPIO пины, номера которых не соответствуют номерам пинов на плате ESP8266.
  Смотрите схему: https://i0.wp.com/randomnerdtutorials.com/wp-content/uploads/2019/05/ESP8266-NodeMCU-kit-12-E-pinout-gpio-pin.png?quality=100&strip=all&ssl=1 */

#define RELAY_PIN 12                    // пин, к которому подключено реле (на плате D6)
#define DS18B20_PIN 4                   // пин, к которому подключен датчик температуры DS18B20 (на плате D2)
#define SET_SETTINGS_MODE_BUTTON_PIN 5  // пин, к которому подключена кнопка для перехода в режим настройки (на плате D1)

#define MAX_GET_TEMPERATURE_ATTEMPTS 5  // максимальное количество попыток получить температуру
#define MAX_WIFI_CONNECTION_TIME 30     // максимальное время подключения к WiFi сети (в секундах)
#define RECONNECT_INTERVAL 2            // интервал попыток переподключения к WiFi сети (в минутах)

String settings;    // параметры работы из SPIFFS
bool settingsMode;  // запущен ли режим настройки

String networkName;
String networkPass;
String userEmail;
String userPass;
short timezone = 0;

bool isHeaterStarted;

bool isTimeModeStarted;
String heaterOnTime;   // время включения обогревателя в режиме по времени
String heaterOffTime;  // время выключения обогревателя в режиме по времени

short temperature = 0;

String temperatureRange;
bool isTemperatureModeStarted;
bool isTemperatureModeStartedNow;
short getTemperatureInterval = 0;
short minTemperature = 0;
short maxTemperature = 0;
bool temperatureModePhase = false;  // фаза работы в режиме по температуре
bool temperatureSavedInFirebase;    // переменная нужная для того, чтобы при сохранении температуры в Firebase, слушатель изменения данных не срабатывал

// переменные, нужные для того, чтобы при получении данных из Firebase, они записывались в разные переменные
bool changeIsHeaterStarted;
bool changeHeaterOnOffTime;
bool changeTemperatureRange;

/* массивы для хранения предыдущих и текущих данных из Firebase (это нужно потому, что иногда слушатель изменения данных перезапускается и из-за этого команды прочитываются дважды.
  поэтому, для того, чтобы не было ложных срабатываний слушателя, мы сравниваем предыдущее и текущее значение) */
bool isHeaterStarted1;
bool isHeaterStarted2;
String heaterOnOffTimeArray[2];
String temperatureRangeArray[2];

unsigned long getTemperatureMillis = 0;   // время последнего получения температуры в millis
unsigned long getTimeMillis = 0;          // время последнего получения времени в millis
unsigned long wifiReconnectedMillis = 0;  // время последней попытки подключиться к WiFi сети в millis

bool wifiNotConnected;
short hours = 0;
short minutes = 0;

String formattedHours;
String formattedMinutes;

Helper helper;  // объект "помощник" с дополнительными методами

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensor(&oneWire);  // датчик температуры DS18B20

ESP8266WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 1000);  // настраиваем NTP клиент (UDP клиент, NTP сервер, часовой пояс по UTC, интервал получения времени)

FirebaseData firebaseData;
FirebaseData firebaseStream;  // слушатель изменения данных в базе данных Firebase

FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;  // объект для настройки подключения к Firebase

// функция для прослушивания изменений данных в базе данных Firebase, или, проще говоря, получения команд со смартфона
void firebaseStreamCallback(MultiPathStream firebaseStream) {
  if (!temperatureSavedInFirebase) {
    if (firebaseStream.get("isHeaterStarted")) {  // если получена команда на запуск/остановку обогревателя
      if (helper.checkVariableChange(changeIsHeaterStarted, isHeaterStarted1, isHeaterStarted2, String(firebaseStream.value.c_str()))) {
        if (!isTemperatureModeStarted) {
          if (String(firebaseStream.value.c_str()).indexOf("true") != -1) {
            isHeaterStarted = true;
            digitalWrite(RELAY_PIN, HIGH);
            Serial.println("Обогреватель запущен");
          } else {
            isHeaterStarted = false;
            digitalWrite(RELAY_PIN, LOW);
            Serial.println("Обогреватель остановлен");
          }
          helper.saveToSPIFFS("/ModeSettings.txt", String(isHeaterStarted) + String(isTimeModeStarted) + String(heaterOnTime) + "#"
                                                     + String(heaterOffTime) + "0#" + String(isTemperatureModeStarted) + String(temperatureRange));
        }
      }
    }

    if (firebaseStream.get("heaterOnOffTime")) {  // если получено время включения/выключения обогревателя в режиме по времени
      String heaterOnOffTime = String(firebaseStream.value.c_str());
      if (helper.checkVariableChange(changeHeaterOnOffTime, heaterOnOffTimeArray, heaterOnOffTime)) {
        heaterOnTime = "";
        heaterOffTime = "";
        if (heaterOnOffTime != " ") {  // если получена команда на запуск режима по времени
          isTimeModeStarted = true;
          bool heaterOnOff = false;
          Serial.println("Время включения/выключения обогревателя: " + String(heaterOnOffTime));
          // разделяем время включения и время выключения обогревателя
          for (int i = 0; i < heaterOnOffTime.length(); i = i + 4) {
            heaterOnOff = !heaterOnOff;
            if (heaterOnOff) {
              heaterOnTime = heaterOnTime + heaterOnOffTime.substring(i, i + 4) + " ";
            } else {
              heaterOffTime = heaterOffTime + heaterOnOffTime.substring(i, i + 4) + " ";
            }
          }
          Serial.println("Время включения: " + String(heaterOnTime));
          Serial.println("Время выключения: " + String(heaterOffTime));
          Serial.println("Режим работы по времени запущен");
        } else {
          isTimeModeStarted = false;
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("Режим работы по времени остановлен");
        }
        helper.saveToSPIFFS("/ModeSettings.txt", String(isHeaterStarted) + String(isTimeModeStarted) + String(heaterOnTime) + "#"
                                                   + String(heaterOffTime) + "0#" + String(isTemperatureModeStarted) + String(temperatureRange));
      }
    }

    if (firebaseStream.get("temperatureRange")) {  // если получен диапазон температуры, который нужно поддерживать в режиме по температуре
      temperatureRange = String(firebaseStream.value.c_str());
      if (helper.checkVariableChange(changeTemperatureRange, temperatureRangeArray, temperatureRange)) {
        if (temperatureRange != " ") {
          isTemperatureModeStarted = true;
          isTemperatureModeStartedNow = true;
          minTemperature = (temperatureRange.substring(0, temperatureRange.indexOf(" "))).toInt();
          maxTemperature = (temperatureRange.substring(temperatureRange.indexOf(" ") + 1, temperatureRange.length())).toInt();
          Serial.println("Минимальная температура: " + String(minTemperature) + "\nМаксимальная температура: " + String(maxTemperature) + "\nРежим работы по температуре запущен");
        } else {
          isTemperatureModeStarted = false;
          isTemperatureModeStartedNow = false;
          temperatureModePhase = false;
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("Режим работы по температуре остановлен");
        }
        helper.saveToSPIFFS("/ModeSettings.txt", String(isHeaterStarted) + String(isTimeModeStarted) + String(heaterOnTime) + "#"
                                                   + String(heaterOffTime) + "0#" + String(isTemperatureModeStarted) + String(temperatureRange));
      }
    }

    if (firebaseStream.get("workParameters")) {  // если получены параметры работы
      String newSettings = String(firebaseStream.value.c_str());
      if (newSettings != " " && newSettings.indexOf(settings) == -1) {
        Serial.println("Параметры работы: " + String(newSettings));
        if (helper.saveToSPIFFS("/Settings.txt", newSettings)) {
          if (!Firebase.RTDB.setString(&firebaseData, ("/" + firebaseAuth.token.uid + "/workParameters").c_str(), " "))
            Serial.println("Не удалось удалить параметры работы из Firebase :( Причина: " + String(firebaseData.errorReason().c_str()));
          ESP.restart();
        }
      }
    }
  }
}

// функция для отслеживания ошибок слушателя изменения данных в Firebase
void firebaseStreamTimeoutCallback(bool timeout) {
  if (timeout)
    Serial.println("Таймаут слушателя изменения данных в Firebase истёк");

  if (!firebaseStream.httpConnected())
    Serial.printf("Ошибка слушателя изменения данных в Firebase :( Код ошибки: %d, причина ошибки: %s\n", firebaseStream.httpCode(), firebaseStream.errorReason().c_str());
}

// функция для получения параметров работы по WiFi
void handleRoot() {
  server.send(200, "text/plain", "Ok\r\n");
  delay(500);  // делаем задержку для того, чтобы пользователь успел получить ответ
  if (server.hasArg("network_name") && server.hasArg("network_pass") && server.hasArg("user_email")
      && server.hasArg("user_pass") && server.hasArg("timezone") && server.hasArg("interval")) {
    settings = server.arg("network_name") + "#" + server.arg("network_pass") + "#" + server.arg("user_email")
               + "#" + server.arg("user_pass") + "#" + server.arg("timezone") + "#" + server.arg("interval");
    Serial.println("Параметры работы: " + String(settings));

    if (helper.saveToSPIFFS("/Settings.txt", settings)) ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("\nНе удалось монтировать SPIFFS :(");
    ESP.restart();
  }

  if (LittleFS.exists("/Settings.txt")) {  // если параметры работы настроены
    File settingsFile = LittleFS.open("/Settings.txt", "r");
    if (!settingsFile) {
      Serial.println("\nНе удалось открыть файл с параметрами работы :(");
      ESP.restart();
    }

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(SET_SETTINGS_MODE_BUTTON_PIN, INPUT);

    while (settingsFile.available()) settings = settingsFile.readString();
    settingsFile.close();

    Serial.println("\nПараметры работы: " + String(settings));

    helper.getSettings(settings, networkName, networkPass, userEmail, userPass, timezone, getTemperatureInterval);  // получаем параметры работы из SPIFFS

    unsigned int settingsModeButtonMillis = millis();
    bool settingsModeButtonFlag;

    if (!digitalRead(SET_SETTINGS_MODE_BUTTON_PIN)) {
      Serial.println("Кнопка для перехода в режим настройки нажата");
      for (int i = 0; i <= 3010; i++) {
        bool settingsModeButtonState = !digitalRead(SET_SETTINGS_MODE_BUTTON_PIN);
        if (settingsModeButtonState && !settingsModeButtonFlag && millis() - settingsModeButtonMillis >= 100) {
          settingsModeButtonMillis = millis();
          settingsModeButtonFlag = true;
          Serial.println("Кнопка для перехода в режим настройки нажата");
        }

        if (!settingsModeButtonState && settingsModeButtonFlag && millis() - settingsModeButtonMillis >= 100) {
          Serial.println("Кнопка для перехода в режим настройки отпущена");
          break;
        }

        if (settingsModeButtonState && settingsModeButtonFlag && millis() - settingsModeButtonMillis >= 3000) {
          settingsMode = true;
          WiFi.mode(WIFI_AP);
          WiFi.softAP("Умный Обогрев", "12345678");

          server.on("/", handleRoot);
          server.begin();

          Serial.println("Перешли в режим настройки параметров работы");
          break;
        }
        delay(1);
      }
    }

    if (!settingsMode) {
      if (LittleFS.exists("/ModeSettings.txt")) {  // если файл с параметрами режимов работы существует в SPIFFS
        File modeSettingsFile = LittleFS.open("/ModeSettings.txt", "r");
        if (modeSettingsFile) {
          String modeSettings;
          while (modeSettingsFile.available()) modeSettings = modeSettingsFile.readString();
          modeSettingsFile.close();

          // получаем параметры режимов работы из SPIFFS
          helper.getModeSettings(modeSettings, isTimeModeStarted, heaterOnTime, heaterOffTime, temperatureRange,
                                 isTemperatureModeStarted, isTemperatureModeStartedNow, minTemperature, maxTemperature, RELAY_PIN);
        } else {
          Serial.println("Не удалось открыть файл с параметрами режима :(");
        }
      }

      connectToWiFi();
    }
  } else {
    settingsMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Умный Обогрев", "12345678");

    server.on("/", handleRoot);
    server.begin();

    Serial.println("\nWiFi точка \"Умный Обогрев\" запущена");
  }
}

void loop() {
  if (settingsMode) {
    server.handleClient();
  } else {
    // прочитываем и отправляем температуру
    if ((millis() - getTemperatureMillis >= getTemperatureInterval * 60000) || getTemperatureMillis == 0 || isTemperatureModeStartedNow) {
      getTemperatureMillis = millis();

      short currentTemperature = helper.getTemperatureC(sensor, MAX_GET_TEMPERATURE_ATTEMPTS);
      if (currentTemperature >= -10) {
        temperature = currentTemperature;
        Serial.println("Температура: " + String(temperature) + "°C");
      }

      if (isTemperatureModeStarted) {
        // если текущая температура меньше минимальной, или если режим по температуре только запустился и температура меньше максимальной
        if ((!temperatureModePhase && temperature < minTemperature) || (isTemperatureModeStartedNow && temperature < maxTemperature)) {
          temperatureModePhase = true;
          isTemperatureModeStartedNow = false;
          digitalWrite(RELAY_PIN, HIGH);
          Serial.println("Обогреватель запущен по температуре");
        } else if (temperatureModePhase && temperature >= maxTemperature) {  // если текущая температура выше максимальной
          temperatureModePhase = false;
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("Обогреватель остановлен по температуре");
        }

        if (Firebase.ready()) {
          if (!Firebase.RTDB.setBool(&firebaseData, ("/" + firebaseAuth.token.uid + "/isHeaterStarted").c_str(), temperatureModePhase))
            Serial.println("Не удалось записать в Firebase то, что обогреватель запущен/остановлен :( Причина: " + String(firebaseData.errorReason().c_str()));
        }
      }

      if (Firebase.ready()) {
        temperatureSavedInFirebase = true;
        if (!Firebase.RTDB.setInt(&firebaseData, ("/" + firebaseAuth.token.uid + "/temperature").c_str(), temperature))
          Serial.println("Не удалось записать температуру в Firebase :( Причина: " + String(firebaseData.errorReason().c_str()));
        temperatureSavedInFirebase = false;
      }
    }

    if (millis() - getTimeMillis > 60000) {
      getTimeMillis = millis();

      helper.getFormatTime(hours, minutes, formattedHours, formattedMinutes);  // вычисляем время

      if (isTimeModeStarted) {
        if (heaterOnTime.indexOf(formattedHours + formattedMinutes) != -1) {  // если в переменной с временем включения есть текущее время
          digitalWrite(RELAY_PIN, HIGH);
          helper.saveToSPIFFS("/ModeSettings.txt", String(isHeaterStarted) + String(isTimeModeStarted) + String(heaterOnTime) + "#"
                                                     + String(heaterOffTime) + "1#" + String(isTemperatureModeStarted) + String(temperatureRange));
          Serial.println("Обогреватель запущен по времени");
        } else if (heaterOffTime.indexOf(formattedHours + formattedMinutes) != -1) {  // если в переменной с временем выключения есть текущее время
          digitalWrite(RELAY_PIN, LOW);
          helper.saveToSPIFFS("/ModeSettings.txt", String(isHeaterStarted) + String(isTimeModeStarted) + String(heaterOnTime) + "#"
                                                     + String(heaterOffTime) + "0#" + String(isTemperatureModeStarted) + String(temperatureRange));
          Serial.println("Обогреватель остановлен по времени");
        }
      }
    }

    if (wifiNotConnected && (millis() - wifiReconnectedMillis >= RECONNECT_INTERVAL * 60000)) {
      connectToWiFi();
    }
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(networkName, networkPass);

  Serial.println("Подключаемся к WiFi сети");

  int reconnectingTime = 0;
  // подключаемся к WiFi сети 30 секунд
  while (WiFi.status() != WL_CONNECTED && reconnectingTime <= MAX_WIFI_CONNECTION_TIME * 2) {
    delay(500);
    reconnectingTime++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nПодключились к WiFi сети. Локальный IP: ");
    Serial.println(WiFi.localIP());

    timeClient.setTimeOffset(timezone * 3600);  // устанавливаем часовой пояс по UTC
    timeClient.begin();

    timeClient.update();
    String formattedTime = timeClient.getFormattedTime(); // получаем время от NTP сервера в формате ЧЧ:ММ:СС
    getTimeMillis = millis();

    // выделяем и записываем часы и минуты
    String hoursStr = formattedTime.substring(0, 2);
    String minutesStr = formattedTime.substring(3, 5);

    // удаляем ноль перед часами, или минутами
    if (hoursStr.startsWith("0")) {
      hoursStr.remove(0, 1);
    }
    if (minutesStr.startsWith("0")) {
      minutesStr.remove(0, 1);
    }

    hours = hoursStr.toInt();
    minutes = minutesStr.toInt();
    Serial.println("Текущее время: " + String(hours) + ":" + String(minutes));

    timeClient.end(); // останавливаем NTP клиент

    // подключаемся к проекту на платформе Firebase
    helper.connectToFirebase(wifiNotConnected, userEmail, userPass, firebaseData, firebaseStream,
                             firebaseAuth, firebaseConfig, firebaseStreamCallback, firebaseStreamTimeoutCallback);

    sensor.begin();  // запускаем датчик DS18B20
  } else {
    wifiNotConnected = true;
    wifiReconnectedMillis = millis();
  }
}