
#include "myheader.hpp"
#define FORMAT_LITTLEFS_IF_FAILED true
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


MAX30105 particleSensor;


GoogleHomeNotifier ghome;

static SemaphoreHandle_t mutex;


volatile msg my_own_msg;
volatile bool warning ; //For patient health
volatile bool wakeup ;
volatile bool alarmtime = false;
struct alarm my_signal[2]={{1,02,1},{0,0,0}};
String message = "This message was brought to you by Health Instruments!";
String firstName, lastName, room;
const TickType_t delaysleep = 3000, waitAlarm = 61000;
TickType_t lastTime = 0, alarmTime = 0;
// Interrupt for button to discard alert and warning
void discard(void*arg){
  while(1){
    while(digitalRead(button) == 0){
     
      my_signal[0].set = false;
      my_signal[1].set = false;
      
      warning = false;
      
      alarmTime = xTaskGetTickCount();
      
      if((xTaskGetTickCount() - lastTime) > delaysleep){
        if(digitalRead(button) == 0){
          wakeup = !wakeup;
        }
        
      }
    }
    lastTime = xTaskGetTickCount();
  }
  
}

bool checkalarm(struct tm timeinfo){
  if(xTaskGetTickCount() - alarmTime < waitAlarm){
    return false;
  }
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
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  
    xSemaphoreTake(mutex, portMAX_DELAY);
    
    display.clearDisplay();
    
    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    if(!getLocalTime(&timeinfo)){
      display.println("Failed to obtain time");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }
    else{
      if(checkalarm(timeinfo)){
        display.drawBitmap(34,0,alarm_icon,30,28,WHITE);
        
        tone(buzzer,120,500);
      }
      else if(warning){
       display.drawBitmap(34,0,warning_icon,30,30,WHITE);
       tone(180,1000);
      }
      else{
        char time_info[10];
        strftime(time_info,10,"%H:%M:%S",&timeinfo);
        display.setCursor(0,0);
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.println(time_info);
        tone(buzzer,0);
      }
    
    }
    
    
   
    display.drawBitmap(0,40,heart_icon,20,20,INVERSE);
    display.drawBitmap(80,40,spo2_icon,15,24,WHITE);
    display.setTextSize(1);
    if(my_own_msg.validHeartRate !=1 || my_own_msg.validSPO2 !=1 ){
        display.setCursor(23,40);
        display.printf(" ");
        display.setCursor(100,40);
        display.printf(" ");
    }
    else{
        display.setCursor(20,44);
        display.printf(":%d bpm",my_own_msg.heartRate);
        display.setCursor(90,45);
        display.printf(":%d %",my_own_msg.spo2);
    }
    if(!wakeup){
      display.clearDisplay();
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
     
      my_own_msg.heartRate = heartRate;
      my_own_msg.spo2 = spo2;
      my_own_msg.validHeartRate = validHeartRate;
      my_own_msg.validSPO2 = validSPO2;
      /*
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
      */
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
  if(WiFi.status() == WL_CONNECTED){
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
  else{
    return "None";
  }
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
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      char time_info[22];
      strftime(time_info,22,"%F%%20%H-%M-%S",&timeinfo);
      // Set the headers
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String payload = "api_key=" + apikey +"&patientID=" + id \
        + "&time=" + String(time_info) 
        + "&heartRate=" + String(my_own_msg.heartRate) + "&spO2=" + String(my_own_msg.spo2);
      Serial.println(payload);
      // Send HTTP POST request
    int httpResponseCode = http.POST(payload);
     
    
    
    if (httpResponseCode>0) {
      Serial.print("HTTP POST Response code data: ");
      Serial.println(httpResponseCode);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    if(warning){
      http_header = server + "/post-warning.php";
      http.begin(*client,http_header.c_str());

      // Set the headers
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      String payload = "api_key=" + apikey +"&patientID=" + id \
        + "&warning=1";
      Serial.println(payload);
      // Send HTTP POST request
    int httpResponseCode = http.POST(payload);
    if (httpResponseCode>0) {
      Serial.print("HTTP POST Response code warning: ");
      Serial.println(httpResponseCode);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    }
    // Free resources
    http.end();
    client->~WiFiClientSecure();
    }

    } 
  
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}
void write_read_file(){
  if(WiFi.status()!=WL_CONNECTED){
    if(my_own_msg.validHeartRate == 1 && my_own_msg.validSPO2 == 1){
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      char time_info[22];
      strftime(time_info,22,"%F%%20%H-%M-%S",&timeinfo);
      String data = String(time_info) + " heartRate: " + \
      String(my_own_msg.heartRate) + " spO2: " + String(my_own_msg.spo2) ;
      appendFile(LITTLEFS,"/data.txt",data.c_str());
      readFile(LITTLEFS,"/data.txt");
      return;
    }
  }
    
}

void signal(void*arg){
  while(1){
    String req = server + "/getalarm.php?patientID=" + id;
    String recv = httpGETRequest(req.c_str());
    Serial.println(recv);
    StaticJsonDocument<256> doc;
    if(recv != "None"){
      

    DeserializationError error = deserializeJson(doc, recv);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      
    }
    else{
      for (JsonObject item : doc.as<JsonArray>()) {

      const char* alarmID = item["alarmID"]; // "1", "2"
      const char* patientID = item["patientID"]; // "0001", "0001"
      const char* alarmTime = item["alarmTime"]; // "18:09:01", "05:13:55"
      const char* description = item["description"]; // "test", "test"
      char strhour[3], strmin[3];
      strncpy(strhour,&alarmTime[0],2);
      strncpy(strmin,&alarmTime[3],2);
      my_signal[atoi(alarmID)-1].hour = atoi(strhour);
      my_signal[atoi(alarmID)-1].minute = atoi(strmin);
      my_signal[atoi(alarmID)-1].set = true;

      } 
    }
    }
   
    req = server + "/get_name.php"+ "?api_key=" + apikey +"&patientID="+id;
    recv = httpGETRequest(req.c_str());
    if(recv != "None"){
      DeserializationError error = deserializeJson(doc, recv);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      
      }
      else{
        JsonObject root_0 = doc[0];
        const char* root_0_firstName = root_0["firstName"]; // "Henk"
        const char* root_0_lastName = root_0["lastName"]; // "Gerards"
        const char* root_0_sex = root_0["sex"]; // "M"
        const char* root_0_room = root_0["room"]; // "W34"
        firstName = root_0_firstName;
        lastName = root_0_lastName;
        room = root_0_room;
      }
    
    }
    
    Serial.printf("firstName: %s, lastName: %s, room: %s\n",firstName, lastName,room);
    HttpPostRequest();
    write_read_file();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

/* Send signal to Google Home
*/
void send_signalGH(void *arg){
  Serial.println("Finding Home...");
  if (ghome.device(homeName,"en") != true) {
    Serial.println(ghome.getLastError());
   
  }
  Serial.print("Home found!(");
  Serial.print(ghome.getIPAddress());
  Serial.println(")");
  
  if (ghome.notify(message.c_str()) != true) {
    Serial.println(ghome.getLastError());
  }
  struct tm timeinfo;
  while(1){
    if(my_own_msg.validSPO2 == 1 && my_own_msg.validHeartRate == 1){//
      if(my_own_msg.heartRate > 130){
        message = "Heart rate of " + firstName + lastName + "in room " + room +  " is too high";
      }
      else{
        message =  "Heart rate of " + firstName + lastName + "in room " + room +  "is too low";
      }
      if (my_own_msg.spo2 <80){
        message = message + "and pulse oxygen is too low";
      }

      if (ghome.notify(message.c_str()) != true) {
      Serial.println(ghome.getLastError());
      }
    }
    else{
      message = "";
       if (ghome.notify(message.c_str()) != true) {
      Serial.println(ghome.getLastError());
      }
    }
    getLocalTime(&timeinfo);
    if(checkalarm(timeinfo)){
      message = firstName + lastName + " you are now having alarm";
    }
    if (ghome.notify(message.c_str()) != true) {
      Serial.println(ghome.getLastError());
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  
}
void logo_start(){
  for(int i = 0; i < 2; i++){
    for(int j = 0; j < bitmap_allArray_LEN; j++){
    display.clearDisplay();
    display.drawBitmap(34,0,bitmap_allArray[j],60,60,WHITE);
    display.display();
  }
  vTaskDelay(pdMS_TO_TICKS(500));
  }
 
}

void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:
  wakeup = true;
  warning = false;
  Serial.setDebugOutput(true);

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
  logo_start();
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      Serial.println("LITTLEFS Mount Failed");
        
  }
  
  mutex = xSemaphoreCreateMutex();
  pinMode(buzzer,OUTPUT);
  pinMode(button,INPUT_PULLUP);
  //attachInterrupt(button,discard,RISING);
  xTaskCreate(send_signalGH,"Send signal to Google Home",8000,NULL,1,NULL);
  xTaskCreate(biometric_data_collect,"bio metric collector",8056,NULL,1,NULL);
  xTaskCreate(display_task,"display task",10045,NULL,1,NULL);
  xTaskCreate(signal,"Get signal",16048,NULL,1,NULL);
  xTaskCreate(discard,"Discard signal",2000,NULL,3,NULL);
  
  vTaskStartScheduler();
  
}

void loop()
{
  
}


