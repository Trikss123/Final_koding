bool sendATCommandWithTimeout(String command, String expected_response, int timeout_ms) {
  GSM.println(command);
  unsigned long start_time = millis();

  while (true) {
    while (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      Serial.println(response);
      if (response.indexOf(expected_response) != -1) {
        return true; // Expected response received, return true
      }
    }

    // Check if the timeout has been reached
    if (millis() - start_time > timeout_ms) {
      return false; // Timeout reached, return false
    }
  }
}

void readSerial(unsigned int wait) {
  char karakter = -2;
  unsigned long mulai = millis();
  while (millis() - mulai <= wait)  {
    karakter = GSM.read();
    if (debug == 1) {
      if (isAscii(karakter)) Serial.write(karakter);
    }
  }
}

void initGSM() {
  sendATCommandWithTimeout("AT", "OK", 100);
  sendATCommandWithTimeout("AT+CPIN?", "OK", 500);
  regSIM();
}

void regSIM() {
  GSM.println("AT+CREG?");
  unsigned int previous_millis = millis();
  while (true) {
    if (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      if (debug == 1) Serial.println(response);

      if (response.indexOf("+CREG: ") != -1) {
        byte comma_index = response.indexOf(',', response.indexOf(':') + 1);
        result = response.substring(response.indexOf('\n') , comma_index + 1);
        break;
      }
    }

    if (millis() - previous_millis > 500) {
      Serial.println("Error: Timeout waiting for registration network");
      break;
    }
  }
  readSerial(10);

  GSMregister = result.toInt();
  if (GSMregister == 1) Serial.println("GSM registered to network");
  else Serial.println("GSM not registered to network");
}

void cekOperator() {
  GSM.println("AT+COPS?");

  unsigned int previous_millis = millis();
  while (true) {
    if (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      if (debug == 1) Serial.println(response);

      if (response.indexOf("+COPS:") != -1) {
        byte comma_index = response.indexOf(',', response.indexOf('"') + 1);
        operators = response.substring(response.indexOf('"') + 1, comma_index);
        break;
      }
    }

    if (millis() - previous_millis > 500) {
      Serial.println("Error: Timeout waiting for network operator response");
      break;
    }
  }
  readSerial(10);
}

void signalQuality() {
  GSM.println("AT+CSQ");
  unsigned int previous_millis = millis();
  byte kualitasSinyal = 0;

  while (true) {
    if (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      if (debug == 1) Serial.println(response);

      if (response.indexOf("+CSQ:") != -1) {
        byte comma_index = response.indexOf(',', response.indexOf(':') + 1);
        result = response.substring(response.indexOf(':') + 1, comma_index);
        break;
      }
    }

    // Check if the timer has expired
    if (millis() - previous_millis > 500) {
      Serial.println("Error: Timeout waiting for network signal quality response");
      break;
    }
  }
  kualitasSinyal = result.toInt();
  if (debug == 1) {
    Serial.print("Kualitas Sinyal = ");
    Serial.println(kualitasSinyal);
  }

  if (kualitasSinyal < 10)             sinyal = ("JELEK");
  if (kualitasSinyal > 9 && kualitasSinyal < 15)    sinyal = ("CUKUP");
  if (kualitasSinyal > 14 && kualitasSinyal < 20)   sinyal = ("BAGUS");
  if (kualitasSinyal > 19 && kualitasSinyal <= 31)  sinyal = ("SANGAT BAGUS");
  if (kualitasSinyal == 99)            sinyal = ("Tidak Diketahui");

  readSerial(10);
}

void waktuGSM() {
  GSM.println("AT+CCLK?");
  unsigned int previous_millis = millis();
  // Wait for and read the response from the module
  String response = "";
  while (true) {
    if (GSM.available()) {
      String response = GSM.readStringUntil('\n');
      if (debug == 1) Serial.println(response);

      // Parse the response to get the UTC time
      if (response.startsWith("+CCLK:")) {
        // Extract the time string and quarter time offset from the response
        int timeStartIndex = response.indexOf("\"") + 1;
        int timeEndIndex = response.indexOf("\"", timeStartIndex);
        String timeString = response.substring(timeStartIndex, timeEndIndex);
        timeStartIndex = timeString.indexOf("+") + 1;
        int quarterTimeOffset = timeString.substring(timeStartIndex, timeString.length()).toInt() / 4;
        // Parse the time string into a DateTime object
        tahun = timeString.substring(0, 2).toInt() + 2000;
        bulan = timeString.substring(3, 5).toInt();
        hari = timeString.substring(6, 8).toInt();
        jam = timeString.substring(9, 11).toInt();
        menit = timeString.substring(12, 14).toInt();
        detik = timeString.substring(15, 17).toInt();
        DateTime now = DateTime(tahun, bulan, hari, jam, menit, detik) - TimeSpan(0, quarterTimeOffset, 0, 0);

        // Set the RTC module with the parsed time
        if (tahun >= 2023 && tahun < 2080) {
          rtc.adjust(now);
        }

        Serial.print("Time set to ");
        Serial.println(now.timestamp());
        break;
      }
    }
  }
}

void initGPRS() {
  sendATCommandWithTimeout("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 500);
  sendATCommandWithTimeout("AT+SAPBR=3,1,\"APN\",\"internet\"", "OK", 500);
  sendATCommandWithTimeout("AT+SAPBR=1,1", "OK", 2000);
  sendATCommandWithTimeout("AT+HTTPINIT", "OK", 2000);
  sendATCommandWithTimeout("AT+HTTPPARA=\"CID\",1", "OK", 500);
  sendATCommandWithTimeout("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 500);

  result = "AT+HTTPPARA=\"URL\",\"";
  result += URL;
  result += "/api/v1.6/devices/";
  result += deviceLabel;
  result += "/?token=" + String(ubidotsToken) + "\"";
  sendATCommandWithTimeout(result, "OK", 500);

}

void httpSend() {
  sendATCommandWithTimeout("AT+HTTPDATA=" + String(json.length()) + ",10000", "DOWNLOAD", 500);
  sendATCommandWithTimeout(json, "OK", 500);
  sendATCommandWithTimeout("AT+HTTPACTION=1", "OK", 500);
  readResponse();
  sendATCommandWithTimeout("AT+HTTPREAD", "OK", 500);
  readResponse();
  sendATCommandWithTimeout("AT+HTTPTERM", "OK", 500);
  sendATCommandWithTimeout("AT+SAPBR=0,1", "OK", 500);
  
}

// Read response from GSM module
void readResponse() {
  String response = "";
  unsigned long waktu = millis();
  char karakter = ' ';

  while (millis() - waktu < 10000) {
    while (GSM.available()) {
      karakter = char(GSM.read());
      response += karakter;
      waktu = millis();  // Reset the timer
    }
  }

  Serial.print("Hasil response: ");
  Serial.println(response);

  int StartIndex = response.indexOf(",") + 1;
  int EndIndex = response.indexOf(",", StartIndex);
  int httpCode = response.substring(StartIndex, EndIndex).toInt();

  Serial.print("Hasil kode: ");
  Serial.println(httpCode);

  if (httpCode == 201) {
    // Data sent successfully
    // Add your code here to handle the successful sending of data
  } else {
    // Data sending failed
    // Add your code here to handle the failure
  }
}
void dataJSON() {
  json = "";
  json = "{\"suhu\":" + String(temperatureC, 2);
  json += ",\"Kedalaman air\":" + String(kedalamanAir, 0);
  json += "}";
  Serial.println(json);
}

void ambilSuhu(){
  sensorSuhu.requestTemperatures();
  temperatureC = sensorSuhu.getTempCByIndex(0);

  suhu_konversi= temperatureC * 0.901 + 3.272;
}

void ambilJarak() {
  //pakai while agar nol nya hilang
  jarak = 0; //Reset nilai jarak
  while (jarak == 0 || jarak >= 800) {
    //AMBIL JARAK
    digitalWrite(trig, LOW);
    delayMicroseconds(2);
    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);
    durasi = pulseIn(echo, HIGH);
    jarak = durasi / 58.2; //mengubah waktu menjadi jarak

    jarak_konversi = 0.9999 * jarak + 0.0918;

    // Menghitung kedalaman air
    float tinggiSensor_cm = 315; // tinggi  sensor dalam centimeter
    kedalamanAir = tinggiSensor_cm - jarak_konversi;

    // Lanjutkan dengan operasi lain yang diinginkan...
  }
}

void swap(float* xp, float* yp) {
  float temp = *xp;
  *xp = *yp;
  *yp = temp;
}

void bubbleSort(float arr[], int n) {
  int i, j;
  for (i = 0; i < n - 1; i++) {
    // Last i elements are already in place
    for (j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1])    swap(&arr[j], &arr[j + 1]);
    }
  }
}

void printArray(float arr[], int sizeN) {
  int i;
  for (i = 0; i < sizeN; i++)  {
    Serial.print(arr[i]);
    Serial.print(",");
  }
  Serial.println("\n");
}

void saveDataToSD() {
  // Dapatkan tanggal dan waktu saat ini
  DateTime now = rtc.now();
  String timestamp = String(now.year()) + "/" + String(now.month()) + "/" + String(now.day()) + "/" +
                     String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

  // Buat nama file
  String fileName = "/PASUT14062023.txt";

  // Buka file pada kartu SD dengan mode append
  File dataFile = SD.open(fileName, FILE_APPEND);

  // Jika file berhasil dibuka, tulis data
  if (dataFile) {
    // Format string data
    String dataString = timestamp +"Kedalaman Air: " + String(kedalamanAir)+ ", suhu: " + String(suhu_konversi);

    // Tulis string data ke dalam file
    dataFile.println(dataString);
    Serial.println("Data tersimpan");
    
    // Tutup file
    dataFile.close();
  } else {
    Serial.println("Terjadi kesalahan saat membuka file data");
  }
}
void updateSerial()
{
  delay(500);
  while (Serial.available())
  {
    GSM.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while (GSM.available())
  {
    Serial.write(GSM.read());//Forward what Software Serial received to Serial Port
  }
}

void sleepmode() {
  updateSerial();
  Serial.println("Entering Deep Sleep...");
  delay(100);   
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_sleep_enable_timer_wakeup(10* TIME_TO_SLEEP * uS_TO_S_FACTOR);  esp_deep_sleep_start();
  
}
