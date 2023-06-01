#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

#define FIREBASE_HOST "https://health-instruments--home-default-rtdb.europe-west1.firebasedatabase.app/"
#define FIREBASE_AUTH "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"

const char* ssid = "meh";
const char* password = "askyourmom";

FirebaseData fbdo;

void setup() {
  Serial.begin(115200);
  Serial.println("Hi");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.getBool(fbdo, "/assistant/talk")) {
    String message = Firebase.getString(fbdo);
    if (message.length() > 0) {
      Serial.println("Google Assistant said: " + message);
      sendBroadcast(message);
    }
    Firebase.setBool(fbdo, "/assistant/talk", false);
  }

  delay(1000);
}

void sendBroadcast(const String& message) {
  DynamicJsonDocument json(256);
  json["command"] = "com.google.cast.media.BroadcastCommand";
  json["params"]["broadcastId"] = "RAMP";
  json["params"]["payload"]["devices"] = "1123105FE0HNWV";
  json["params"]["payload"]["message"] = message;

  String body;
  serializeJson(json, body);

  WiFiClient client;
  if (client.connect("localhost", 8008)) {
    client.println("POST http://localhost:8008/setup/broadcast HTTP/1.1");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(body.length()));
    client.println();
    client.println(body);
    client.println();
    client.stop();
  }
}
