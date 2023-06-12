#include <WiFi.h>
#include <esp8266-google-home-notifier.h>

const char* ssid     = "meh";
const char* password = "askyourmom";
const char* homeName = "Health Instruments";
const char* message = "This message was brought to you by Health Instruments!";

GoogleHomeNotifier ghome;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("connected.");

  Serial.println("Finding Home...");
  if (ghome.device(homeName,"en") != true) {
    Serial.println(ghome.getLastError());
    return;
  }
  Serial.print("Home found!(");
  Serial.print(ghome.getIPAddress());
  Serial.println(")");
  
  if (ghome.notify(message) != true) {
    Serial.println(ghome.getLastError());
    return;
  }
}

void loop() {
  // other code here
}
