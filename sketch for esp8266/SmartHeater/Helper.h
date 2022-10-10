#include "wl_definitions.h"
class Helper {
  public:
    // метод для того, чтобы определить, есть ли изменение данных, полученных из Firebase и записать текущее и предыдущее значение в разные переменные
    bool checkVariableChange(bool &changeVariable, bool &firstVariable, bool &secondVariable, String streamData) {
      changeVariable = !changeVariable;
      if (changeVariable) {
        if (streamData.indexOf("true") != -1) {
          firstVariable = true;
        } else {
          firstVariable = false;
        }
      } else {
        if (streamData.indexOf("true") != -1) {
          secondVariable = true;
        } else {
          secondVariable = false;
        }
      }
      if (firstVariable != secondVariable) {
        return true;
      } else {
        return false;
      }
      return false;
    }

    // метод для того, чтобы определить, есть ли изменение данных, полученных из Firebase и записать текущее и предыдущее значение в разные элементы массива
    bool checkVariableChange(bool &changeVariable, String *lastNewValuesArray, String streamData) {
      changeVariable = !changeVariable;
      if (changeVariable) {
        lastNewValuesArray[0] = streamData;
      } else {
        lastNewValuesArray[1] = streamData;
      }
      if (lastNewValuesArray[0] != lastNewValuesArray[1]) {
        return true;
      } else {
        return false;
      }
      return false;
    }

    // метод для записи файла в SPIFFS
    bool saveToSPIFFS(String fileName, String fileContent) {
      File file = LittleFS.open(fileName, "w");
      if (file) {
        if (file.print(fileContent)) {
          file.close();
          return true;
        } else {
          Serial.println("Не удалось записать в SPIFFS :(");
        }
      } else {
        Serial.println("Не удалось открыть файл из SPIFFS :(");
      }
      return false;
    }

    // метод для получения параметров из SPIFFS и записи их в отдельные переменные
    void getSettings(String settings, String &networkName, String &networkPass, String &userEmail, String &userPass, short &timezone, short &getTemperatureInterval) {
      // записываем индексы специальных символов, которые разделяют отдельные параметры
      int secondHashIndex = settings.indexOf("#", settings.indexOf("#") + 1);
      int thirdHashIndex = settings.indexOf("#", secondHashIndex + 1);
      int fourthHashIndex = settings.indexOf("#", thirdHashIndex + 1);
      int fifthHashIndex = settings.indexOf("#", fourthHashIndex + 1);

      // выделяем и записываем отдельные параметры из всех параметров
      networkName = settings.substring(0, settings.indexOf("#"));
      networkPass = settings.substring(settings.indexOf("#") + 1, secondHashIndex);
      userEmail = settings.substring(secondHashIndex + 1, thirdHashIndex);
      userPass = settings.substring(thirdHashIndex + 1, fourthHashIndex);
      timezone = (settings.substring(fourthHashIndex + 1, fifthHashIndex)).toInt();
      getTemperatureInterval = (settings.substring(fifthHashIndex + 1, settings.length())).toInt();

      Serial.println("Название WiFi сети: " + String(networkName));
      Serial.println("Пароль WiFi сети: " + String(networkPass));
      Serial.println("Почта пользователя: " + String(userEmail));
      Serial.println("Пароль пользователя: " + String(userPass));
      Serial.println("Часовой пояс: " + String(timezone));
      Serial.println("Интервал датчика: " + String(getTemperatureInterval));
    }

    // метод для получения параметров режима по температуре и по времени из SPIFFS
    void getModeSettings(String modeSettings, bool &isTimeModeStarted, String &heaterOnTime, String &heaterOffTime, String &temperatureRange,
                         bool &isTemperatureModeStarted, bool &isTemperatureModeStartedNow, short &minTemperature, short &maxTemperature, short relayPin) {
      Serial.println("Параметры режима: " + String(modeSettings));

      int secondHash = modeSettings.indexOf("#", modeSettings.indexOf("#") + 1);
      if (modeSettings.charAt(0) == '1') {
        digitalWrite(relayPin, HIGH);
        Serial.println("Обогреватель запущен");
      }
      if (modeSettings.charAt(1) == '1') { // если режим по времени запущен
        isTimeModeStarted = true;
        heaterOnTime = modeSettings.substring(2, modeSettings.indexOf("#"));
        heaterOffTime = modeSettings.substring(modeSettings.indexOf("#") + 1, secondHash - 1);
        if (modeSettings.charAt(secondHash - 1) == '1') {
          digitalWrite(relayPin, HIGH);
          Serial.println("Обогреватель запущен по времени");
        }
      }
      if (modeSettings.charAt(secondHash + 1) == '1') { // если режим по температуре запущен
        temperatureRange = modeSettings.substring(secondHash + 2, modeSettings.length());
        isTemperatureModeStarted = true;
        isTemperatureModeStartedNow = true;
        minTemperature = (temperatureRange.substring(0, temperatureRange.indexOf(" "))).toInt();
        maxTemperature = (temperatureRange.substring(temperatureRange.indexOf(" ") + 1, temperatureRange.length())).toInt();
        Serial.println("Минимальная температура: " + String(minTemperature) + "\nМаксимальная температура: " + String(maxTemperature) + "\nРежим работы по температуре запущен");
      }
    }

    // метод для подключения к проекту на платформе Firebase
    void connectToFirebase(bool &wifiNotConnected, String &userEmail, String &userPass, FirebaseData &firebaseData, FirebaseData &firebaseStream, FirebaseAuth &firebaseAuth,
                           FirebaseConfig &firebaseConfig, FirebaseData::MultiPathStreamEventCallback firebaseStreamCallback,
                           FirebaseData::StreamTimeoutCallback firebaseStreamTimeoutCallback) {
      wifiNotConnected = false;
      firebaseConfig.api_key = "AIzaSyCJh2823kCFJ0K5gbQ6R1U7pJ0P4W2rRx8"; // устанавливаем API ключ проекта на платформе Firebase
      firebaseAuth.user.email = userEmail;
      firebaseAuth.user.password = userPass;

      firebaseConfig.database_url = "https://smartwifiheater-default-rtdb.europe-west1.firebasedatabase.app/"; // устанавливаем URL базы данных Realtime Database (компонент Firebase)
      firebaseConfig.token_status_callback = tokenStatusCallback;

      Firebase.begin(&firebaseConfig, &firebaseAuth); // подключаемся к Firebase
      Firebase.reconnectWiFi(true);

      firebaseStream.setBSSLBufferSize(2048, 512);

      // устанавливаем слушатель изменения данных в Firebase
      if (Firebase.RTDB.beginMultiPathStream(&firebaseStream, (firebaseAuth.token.uid).c_str())) {
        Firebase.RTDB.setMultiPathStreamCallback(&firebaseStream, firebaseStreamCallback, firebaseStreamTimeoutCallback);
      } else {
        Serial.printf("Не удалось установить слушатель изменения данных в Firebase, %s\n", firebaseStream.errorReason().c_str());
        ESP.restart();
      }
    }

    // метод для получения температуры
    short getTemperatureC(DallasTemperature &sensor, short maxGetTemperatureAttempts) {
      short temperature = 0;
      sensor.requestTemperatures();
      temperature = round(sensor.getTempCByIndex(0));
      short getTemperatureAttempts = 0;
      if (temperature < -10) { // если температура получена не правильно
        while (temperature < -10 && getTemperatureAttempts < maxGetTemperatureAttempts - 1) {
          getTemperatureAttempts++;
          delay(750);
          sensor.requestTemperatures();
          temperature = round(sensor.getTempCByIndex(0));
          Serial.println("Температура: " + String(temperature) + "°C");
        }
      }
      return temperature;
    }

    // метод для вычисления времени
    void getFormatTime(short &hours, short &minutes, String &formattedHours, String &formattedMinutes) {
      minutes++;
      if (minutes == 60) {
        hours++;
        minutes = 0;
      }
      if (hours == 24) {
        hours = 0;
        minutes = 0;
      }

      formattedHours = String(hours);
      formattedMinutes = String(minutes);

      // форматируем часы и минуты (формат ММ:ЧЧ)
      if (formattedHours.toInt() < 10) {
        formattedHours = "0" + formattedHours;
      }
      if (formattedMinutes.toInt() < 10) {
        formattedMinutes = "0" + formattedMinutes;
      }

      Serial.println("Текущее время: " + String(formattedHours) + ":" + String(formattedMinutes));
    }
};
