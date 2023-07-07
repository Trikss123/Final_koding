#include <SD.h> 
#include <Wire.h>
#include <RTClib.h>
#include <OneWire.h> 
#include <DallasTemperature.h> 
#include <esp_sleep.h>

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60

unsigned long startTime,endTime;

//JSN SR04T
#define trig 26 // pin trigger sensor JSN SR04T
#define echo 27 // pin echo sensor JSN SR04T
float jarak, durasi, kedalamanAir, jarak_konversi;
float distance[5]={0,0,0,0,0};

//DS18B20
#define ONE_WIRE_BUS 15 // pin data sensor DS18B20
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensorSuhu(&oneWire);  
float temperatureC, suhu_konversi;
float ultrasonic;

//RTC
RTC_DS3231 rtc;
String operators, sinyal;
int tahun;
byte bulan, hari, jam, menit, detik;
char karakter;
const unsigned long waktu = 1 * 120 * 1000UL;

//Micro SD

//sim 900A
HardwareSerial GSM(2); // deklarasi objek SerialAT
boolean GSMregister;

//UBIDOTS
String URL = "industrial.api.ubidots.com";
const char* ubidotsToken = "BBFF-P4VGNGRMMIaloLxoy6aYoDz6Loqvl7";
const char* deviceLabel = "pasut";
int nilai, i, httpCode, indeks;
bool testing =0; //Uncomment for testing
bool debug=1, deleteDownload=0;
String result, json;


//Variabel 
RTC_DATA_ATTR bool isInitializedRTC = false;
RTC_DATA_ATTR bool isInitializedSD = false;
RTC_DATA_ATTR bool isInitializedGSM = false;
bool isGSMInitialized = false;
bool isGPRSdone = false;
SemaphoreHandle_t xSemaphore = NULL;
volatile int status = 0;

void taskInit(void *parameter) {
  Serial.println("Task Init started on core " + String(xPortGetCoreID()));

  unsigned long startTime = millis(); // tambahkan fungsi millis()

  // Initialize RTC
  if (rtc.begin()) {
    isInitializedRTC = true;
    Serial.println("RTC initialized");
     // Serial.println(xPortGetCoreID());  
  }
  //Reset dari deepsleep
  isInitializedSD = false;
  
  // Initialize SD card
  if (!isInitializedSD) {
    if (SD.begin()) {
      isInitializedSD = true;
      Serial.println("SD card initialized");
       // Serial.println (xPortGetCoreID());  

      // Check for config file
      if (SD.exists("/config.txt")) {
        Serial.println("Config file exists");
      }
      else {
        Serial.println("Config file does not exist");
         // Serial.println(xPortGetCoreID());  
      }
    }
  }
  unsigned long endTime = millis(); // tambahkan fungsi millis()
  Serial.println("Task Init finished on core " + String(xPortGetCoreID()) + " in " + String(endTime - startTime) + " ms");
  
   // Tunggu hingga tugas Init GSM selesai
  while (!isGSMInitialized) {
    delay(100);
  }
  
  xSemaphoreGive(xSemaphore);
 vTaskDelete(NULL);
}

void taskGSM(void *parameter) {
  Serial.println("Task GSM started on core " + String(xPortGetCoreID()));

  unsigned long startTime = millis(); // tambahkan fungsi millis()

  GSM.begin(9600);
  
  //INIT GSM
  initGSM();
  
  //REG SIM
  regSIM();

  // Cek Operator
  cekOperator();

  // Cek Kualitas Sinyal
  signalQuality();
  
  // Set status menjadi 1 dan isInitializedGSM menjadi true
  isInitializedGSM = true;
  status = 1;
  Serial.println("GSM initialized on core " + String(xPortGetCoreID()));

  unsigned long endTime = millis(); // tambahkan fungsi millis()
  Serial.println("Task GSM finished on core " + String(xPortGetCoreID()) + " in " + String(endTime - startTime) + " ms");

  // Tandai bahwa tugas Init GSM selesai
  isGSMInitialized = true;
  Serial.println("Selesai");
  
vTaskDelete(NULL);
}

void taskSensor(void *parameter) { 
  Serial.println("Task sensor started on core " + String(xPortGetCoreID())); 

  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  
  unsigned long startTime = millis(); // tambahkan fungsi millis() 

  Serial.println ("Alat Pasang Surut");

  //INIT SENSOR SUHU DS18B20
  Serial.print("Initializing DS18B20...");
  sensorSuhu.begin();
  Serial.println(" ");

  ambilSuhu();
  for (i = 0; i <= 4; i++) {
    ambilJarak();
    distance[i]=jarak;
  }

  bubbleSort(distance,5);
  printArray(distance,5);
  jarak=distance[2];

  unsigned long endTime = millis(); // tambahkan fungsi millis() 
  Serial.println("Task sensor finished on core " + String(xPortGetCoreID()) + " in " + String(endTime - startTime) + " ms"); 

     // Tunggu hingga tugas Init GPRS selesai
  while (!isGPRSdone){
    delay(100);
  }

// JSON
  dataJSON();
  xSemaphoreGive(xSemaphore);
  vTaskDelete(NULL); 
}

void taskGprs(void *parameter){ 
  Serial.println("Task GPRS started on core " + String(xPortGetCoreID()));
  unsigned long startTime = millis(); // tambahkan fungsi millis()

  GSM.begin(9600);
  delay(1000);

  //Init GPRS
  initGPRS();
    

  isGPRSdone = true;
  status = 1;
  Serial.println("Selesai");
  
  unsigned long endTime = millis(); // tambahkan fungsi millis() 
  Serial.println("Task GPRS finished on core " + String(xPortGetCoreID()) + " in " + String(endTime - startTime) + " ms"); 
  vTaskDelete(NULL);
}

void setup() {
  unsigned long startTime = millis();  // Record the start time
  Serial.begin(9600);

  // Create semaphore
  xSemaphore = xSemaphoreCreateBinary();

  // Create tasks
  xTaskCreatePinnedToCore(taskInit, "Task Init", 10000, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(taskGSM, "Task GSM", 10000, NULL, 1, NULL, 1);

  // Wait for tasks to complete
  xSemaphoreTake(xSemaphore, portMAX_DELAY);
  if (isInitializedRTC && isInitializedSD) {
    Serial.println("Init completed successfully");
  }
  
  // Print "FINISH"
  if (status == 1) {
    Serial.println("FINISH");
  }
  // Create semaphore
  xSemaphore = xSemaphoreCreateBinary();
  status = 0;
  
  //creat task baru
  xTaskCreatePinnedToCore(taskGprs, "Task GPRS", 10000, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskSensor, "Task Sensor", 10000, NULL, 2, NULL, 0);

  // Wait for tasks to complete
  xSemaphoreTake(xSemaphore, portMAX_DELAY);
  if (isGPRSdone) {
    Serial.println("GPRS completed successfully");
  }
    // Print "FINISH"
  if (status == 1) {
    Serial.println("FINISH");
  }
  httpSend();
  saveDataToSD();
  Serial.print("SELESAI ");
  unsigned long endTime = millis();  // Record the end time
  Serial.println("kodingan berakhir :" + String(endTime - startTime) + " ms");
  sleepmode();

}
void loop() {
}
