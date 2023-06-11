

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
#include <time.h>

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
volatile msg my_own_msg;
const char* ssid = "Ziggo5578958";
const char* password = "tgMsqtwd2spp";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

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
    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    if(!getLocalTime(&timeinfo)){
      display.println("Failed to obtain time");
    }
    else{
    char time_info[10];
    strftime(time_info,10,"%H:%M:%S",&timeinfo);
    display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println(time_info);
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
    }
   
    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:

  
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
  xTaskCreate(biometric_data_collect,"bio metric collector",4056,NULL,1,NULL);
  xTaskCreate(display_task,"display task",6045,NULL,1,NULL);
  
  vTaskStartScheduler();
}

void loop()
{
  
}

