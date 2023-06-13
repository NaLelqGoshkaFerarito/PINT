

#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const unsigned char heart_icon [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x1f, 
0x9f, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x1f, 0xff, 0x80, 0x0f, 0xff, 
0x00, 0x07, 0xfe, 0x00, 0x03, 0xfc, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char spo2_icon [] PROGMEM = {
0xff, 0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfc, 0x7e, 0xfc, 0x7e, 0xf8, 0x7e, 0xf8, 0x3e, 
	0xf0, 0x3e, 0xf0, 0x1e, 0xe0, 0x0e, 0xc0, 0x0e, 0xc6, 0x06, 0x9f, 0x06, 0x9b, 0xe2, 0x99, 0xf2, 
	0x99, 0xf2, 0x9f, 0x62, 0x8e, 0x76, 0xc0, 0x06, 0xc0, 0x0e, 0xe0, 0x1e, 0xf8, 0x3e, 0xff, 0xfe
};
const unsigned char alarm_icon [] PROGMEM = {
	0xc0, 0xff, 0xf8, 0x0c, 0x80, 0x3f, 0xf0, 0x04, 0x80, 0x60, 0x18, 0x04, 0x01, 0x80, 0x06, 0x00, 
	0x02, 0x07, 0x81, 0x00, 0x04, 0x3c, 0xf0, 0x80, 0x08, 0xdc, 0xe8, 0x40, 0x11, 0xcf, 0xce, 0x20, 
	0x11, 0xcf, 0xce, 0x20, 0x23, 0xfc, 0xff, 0x10, 0xe4, 0x78, 0x78, 0x9c, 0xe6, 0x7c, 0xf9, 0x9c, 
	0xc7, 0xfc, 0xff, 0x8c, 0xcf, 0xfc, 0xdf, 0xcc, 0xc9, 0xf8, 0x06, 0x4c, 0xc8, 0xf8, 0x04, 0x4c, 
	0xcf, 0xfc, 0xcf, 0xcc, 0xc7, 0xff, 0xff, 0x8c, 0xc6, 0xff, 0xfd, 0x8c, 0xe4, 0x7f, 0xf8, 0x9c, 
	0xe0, 0xff, 0xfc, 0x1c, 0xf3, 0xff, 0xff, 0x3c, 0xf1, 0xcf, 0xce, 0x3c, 0xf0, 0x9c, 0xe4, 0x3c, 
	0xe0, 0x3c, 0xf0, 0x1c, 0xe0, 0x0f, 0xc0, 0x1c, 0xe3, 0x00, 0x03, 0x1c, 0xf7, 0xe0, 0x1f, 0xbc
};
// 'warning', 30x30px
const unsigned char warning_icon [] PROGMEM = {
	0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xf8, 0x7f, 0xfc, 
	0xff, 0xf0, 0x3f, 0xfc, 0xff, 0xf0, 0x3f, 0xfc, 0xff, 0xe0, 0x1f, 0xfc, 0xff, 0xe0, 0x1f, 0xfc, 
	0xff, 0xc0, 0x0f, 0xfc, 0xff, 0x83, 0x07, 0xfc, 0xff, 0x87, 0x87, 0xfc, 0xff, 0x07, 0x83, 0xfc, 
	0xff, 0x07, 0x83, 0xfc, 0xfe, 0x07, 0x81, 0xfc, 0xfc, 0x03, 0x00, 0xfc, 0xfc, 0x03, 0x00, 0xfc, 
	0xf8, 0x03, 0x00, 0x7c, 0xf8, 0x03, 0x00, 0x7c, 0xf0, 0x03, 0x00, 0x3c, 0xf0, 0x00, 0x00, 0x3c, 
	0xe0, 0x00, 0x00, 0x1c, 0xc0, 0x03, 0x00, 0x0c, 0xc0, 0x07, 0x80, 0x0c, 0x80, 0x07, 0x80, 0x04, 
	0x80, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x04, 0xc0, 0x00, 0x00, 0x0c, 0xff, 0xff, 0xff, 0xfc, 
	0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc
};

const static char* root_mem_ca PROGMEM = \
"-----BEGIN CERTIFICATE-----\n"
"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\n"
"-----END CERTIFICATE-----\n";

#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif
#define buzzer 32
#define button 16
#define voltpin 33

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);




MAX30105 particleSensor;
static xQueueHandle msg_queue; //Queue for display

static SemaphoreHandle_t mutex;
typedef struct {
 
  int32_t heartRate;
  int32_t spo2;
  int8_t validSPO2;
  int8_t validHeartRate;
}msg;
struct alarm{
  uint8_t hour;
  uint8_t minute;
  bool set;
};

volatile msg my_own_msg;
volatile bool warning ; //For patient health
volatile bool wakeup ;
struct alarm my_signal[2]={{0,13,1},{0,0,0}};
const char* ssid = "Ziggo5578958";
const char* password = "tgMsqtwd2spp";
//const char* ssid = "Nokia 3.4";
//const char* password = "Minh1234";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const String id ="0001"; 
const String server ="https://jessedijksma.com";
const String apikey = "6fas6ABat7shfg14";
// Interrupt for button to discard alert and warning
void discard(void*arg){
  while(1){
    if(digitalRead(button) == 0){
      warning = false;
      my_signal[0].set = false;
      my_signal[1].set = false;
    }
  }
  
}

bool checkalarm(struct tm timeinfo){
  for(uint8_t i = 0; i < 2; i++){
    if(timeinfo.tm_hour == my_signal[i].hour && timeinfo.tm_min == my_signal[i].minute && my_signal[i].set){
      return true;
    }
  }
  return false;
}

void display_task(void*args){
  struct tm timeinfo;
  while(1){
    while(WiFi.status()!=WL_CONNECTED){
      xSemaphoreTake(mutex, portMAX_DELAY);
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(WHITE); // Draw white text
      display.setCursor(0, 0);  
      display.println("Fail to connect wifi. Reconnect");
      display.display();
      xSemaphoreGive(mutex);
      WiFi.disconnect();
      WiFi.reconnect();
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
   // msg recv_msg;
   // xQueueReceive(msg_queue,(void*)&recv_msg,0);
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    display.clearDisplay();

    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    if(!getLocalTime(&timeinfo)){
      display.println("Failed to obtain time");
    }
    else{
      if(checkalarm(timeinfo)){
        display.drawBitmap(0,0,alarm_icon,30,28,WHITE);
        tone(buzzer,120,500);
      }
      else if(warning){
       display.drawBitmap(0,0,warning_icon,30,30,WHITE);
       tone(180,1000);
      }
      else{
        char time_info[10];
        strftime(time_info,10,"%H:%M:%S",&timeinfo);
        display.setCursor(0,0);
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.println(time_info);
        tone(buzzer,0);
      }
    
    }
    
    
   
    display.drawBitmap(0,40,heart_icon,20,20,INVERSE);
    display.drawBitmap(80,40,spo2_icon,15,24,WHITE);
    if(my_own_msg.validHeartRate !=1 || my_own_msg.validSPO2 !=1 ){
        display.setCursor(23,40);
        display.printf(" ");
        display.setCursor(100,40);
        display.printf(" ");
    }
    else{
        display.setCursor(20,40);
        display.printf(":%d bpm",my_own_msg.heartRate);
        display.setCursor(90,40);
        display.printf(":%d %",my_own_msg.spo2);
    }
    
    display.display();
    xSemaphoreGive(mutex);   
    taskYIELD();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void biometric_data_collect(void*args){
  // bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples, and determine the signal range
  int32_t bufferLength = 100; //data length
  int32_t spo2; //SPO2 value
  int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
  int32_t heartRate; //heart rate value
  int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

  for (byte i = 0 ; i < bufferLength ; i++)
  {
    xSemaphoreTake(mutex, portMAX_DELAY);
   while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data
  
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample
    xSemaphoreGive(mutex);
    taskYIELD();
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  msg my_msg={0,0,0,0};
  while (1)
  {
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      xSemaphoreTake(mutex, portMAX_DELAY);
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data
  
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
      xSemaphoreGive(mutex); 
      taskYIELD();
      // my_msg.heartRate = heartRate;
      // my_msg.spo2 = spo2;
      // my_msg.validHeartRate = validHeartRate;
      // my_msg.validSPO2 = validSPO2;
      my_own_msg.heartRate = heartRate;
      my_own_msg.spo2 = spo2;
      my_own_msg.validHeartRate = validHeartRate;
      my_own_msg.validSPO2 = validSPO2;
       Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);

      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC);

      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC);

      Serial.print(F(", SPO2Valid="));
      Serial.println(validSPO2, DEC);
      //xQueueSend(msg_queue,(void*)&my_msg,(TickType_t)0);
      if(validHeartRate ==1 &&validSPO2 ==1 ){
        if(heartRate > 130 || heartRate < 60 || spo2 < 80){
            warning = true;
        }
        else{
          warning = false;
        }
      }
      else{
        warning = false;
      }
    
    }
   
    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
String httpGETRequest(const char* serverName) {
  HTTPClient http;
  
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);

  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");


  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}"; 

  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
void HttpPostRequest(){
  if(my_own_msg.validHeartRate == 1 && my_own_msg.validSPO2 == 1){
    if(WiFi.status() == WL_CONNECTED){
     WiFiClientSecure *client = new WiFiClientSecure;
      client->setCACert(root_mem_ca);
      // Send the data using HTTPS
      HTTPClient http;
      String http_header = server + "/post-biometric-data.php";
      // Set the target URL
      http.begin(*client,http_header.c_str());

      // Set the headers
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String payload = "api_key=" + apikey +"&patientID=" + id \
        + "&heartRate=" + String(my_own_msg.heartRate) + "&spO2=" + String(my_own_msg.spo2);
      Serial.println(payload);
      // Send HTTP POST request
    int httpResponseCode = http.POST(payload);
     
    
    
    if (httpResponseCode>0) {
      Serial.print("HTTP POST Response code: ");
      Serial.println(httpResponseCode);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();

    }

    } 
  
}
void signal(void*arg){
  while(1){
    String req = server + "/getalarm.php?patientID=" + id;
    String recv = httpGETRequest(req.c_str());
    Serial.println(recv);
    StaticJsonDocument<256> doc;

    DeserializationError error = deserializeJson(doc, recv);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      continue;
    }

    for (JsonObject item : doc.as<JsonArray>()) {

    const char* alarmID = item["alarmID"]; // "1", "2"
    const char* patientID = item["patientID"]; // "0001", "0001"
    const char* alarmTime = item["alarmTime"]; // "18:09:01", "05:13:55"
    char strhour[3], strmin[3];
    strncpy(strhour,&alarmTime[0],2);
    strncpy(strmin,&alarmTime[3],2);
    my_signal[atoi(alarmID)-1].hour = atoi(strhour);
    my_signal[atoi(alarmID)-1].minute = atoi(strmin);
    my_signal[atoi(alarmID)-1].set = true;

    } 
    
    HttpPostRequest();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:
  wakeup = false;
  warning = false;
  
  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
   // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  msg_queue=xQueueCreate(25,sizeof(msg));
  if(msg_queue == NULL){
    Serial.println("Queue initialization failed");
  }
  mutex = xSemaphoreCreateMutex();
  pinMode(buzzer,OUTPUT);
  pinMode(button,INPUT_PULLUP);
  //attachInterrupt(button,discard,RISING);
  xTaskCreate(biometric_data_collect,"bio metric collector",4056,NULL,2,NULL);
  xTaskCreate(display_task,"display task",10045,NULL,2,NULL);
  xTaskCreate(signal,"Get signal",8048,NULL,2,NULL);
  xTaskCreate(discard,"Discard signal",1000,NULL,3,NULL);
  vTaskStartScheduler();
}

void loop()
{
  
}


