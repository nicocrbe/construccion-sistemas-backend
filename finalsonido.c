#include <WiFi.h>
#include <Arduino.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <vector>
#include "config.h"  // Incluir el archivo de configuración generado

WebServer server(80);

const int pinLiving = 2;
const int pinCocina = 4;
const int pinServo = 25;
const int pinLed = 22;
const int pinIrReceiver = 23;
const int pinIrSender = 21;

static const uint8_t PIN_MP3_TX = 26; // Connects to module's RX 
static const uint8_t PIN_MP3_RX = 27; // Connects to module's TX 
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);

// Create the Player object
DFRobotDFPlayerMini player; 

Servo servo;
IRsend irsend(pinIrSender);


struct Orden {
  String id;
  unsigned long duration;
  int order;
  bool isLight;
  unsigned long startTime;
  bool isOn;
  unsigned long repeatInterval;
  int soundName;
};

std::vector<Orden> ordenesLuces;
std::vector<Orden> ordenesGenerales;
std::vector<Orden> ordenesSonidos;

// Variables para controlar la ejecución secuencial de las órdenes de luces
static int currentOrderIndex = 0;
static unsigned long lastExecutionTime = 0;
static bool isOn = false;

void setup() {
  pinMode(pinLiving, OUTPUT);
  pinMode(pinCocina, OUTPUT);
  pinMode(pinLed, OUTPUT);
  irsend.begin();

  servo.attach(pinServo, 0, 1280);

  digitalWrite(pinLiving, HIGH);
  digitalWrite(pinCocina, HIGH);

  Serial.begin(9600);
  // Init serial port for DFPlayer Mini
  softwareSerial.begin(9600);

  // Start communication with DFPlayer Mini
  if (player.begin(softwareSerial)) {
   Serial.println("Sonido OK");

    // Set volume to maximum (0 to 30).
    player.volume(20);
    // Play the first MP3 file on the SD card
  } else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  server.on("/controlar-luces", HTTP_POST, handleControlarLuces);
  server.on("/controlar-luces", HTTP_OPTIONS, handleCors);
  server.on("/controlar-servo", HTTP_POST, handleControlarServo);
  server.on("/controlar-servo", HTTP_OPTIONS, handleCors);
  server.on("/controlar-tv", HTTP_POST, handleControlarTV);
  server.on("/controlar-tv", HTTP_OPTIONS, handleCors);
  server.on("/sounds/settings", HTTP_POST, handleSoundSettings);
  server.on("/sounds/settings", HTTP_OPTIONS, handleCors);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/login", HTTP_OPTIONS, handleCors);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reset", HTTP_OPTIONS, handleCors);
  server.on("/lights/status", HTTP_GET, handleLightsStatus);
  server.on("/lights/status", HTTP_OPTIONS, handleCors);
  server.on("/servo/status", HTTP_GET, handleServoStatus);
  server.on("/servo/status", HTTP_OPTIONS, handleCors);

  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
  checkOrdenesLuces();
  checkOrdenesGenerales();
  checkOrdenesSonidos();
}

void checkOrdenesLuces() {
  unsigned long currentMillis = millis();

  if (currentOrderIndex < ordenesLuces.size()) {
    Orden& currentOrder = ordenesLuces[currentOrderIndex];
    if (!isOn && currentMillis >= lastExecutionTime) {
      executeOrden(currentOrder, true);
      currentOrder.startTime = currentMillis;
      isOn = true;
    } else if (isOn && currentMillis - currentOrder.startTime >= currentOrder.duration) {
      executeOrden(currentOrder, false);
      lastExecutionTime = currentMillis + (currentOrderIndex == ordenesLuces.size() - 1 ? currentOrder.repeatInterval : 0);
      isOn = false;
      currentOrderIndex = (currentOrderIndex + 1) % ordenesLuces.size();
    }
  }
}

void checkOrdenesGenerales() {
  unsigned long currentMillis = millis();
  for (auto it = ordenesGenerales.begin(); it != ordenesGenerales.end(); ) {
    if (it->isOn && currentMillis - it->startTime >= it->duration) {
      executeOrden(*it, false);
      if (it->repeatInterval > 0) {
        it->startTime = currentMillis + it->repeatInterval;
        it->isOn = false;
      } else {
        it = ordenesGenerales.erase(it);
        continue;
      }
    } else if (!it->isOn && currentMillis >= it->startTime) {
      executeOrden(*it, true);
      it->isOn = true;
    }
    ++it;
  }
}

void checkOrdenesSonidos() {
  unsigned long currentMillis = millis();
  for (auto it = ordenesSonidos.begin(); it != ordenesSonidos.end(); ) {
    if (it->isOn && currentMillis - it->startTime >= it->duration) {
      executeOrden(*it, false);
      if (it->repeatInterval > 0) {
        it->startTime = currentMillis + it->repeatInterval;
        it->isOn = false;
      } else {
        it = ordenesSonidos.erase(it);
        continue;
      }
    } else if (!it->isOn && currentMillis >= it->startTime) {
      executeOrden(*it, true);
      it->isOn = true;
    }
    ++it;
  }
}

void executeOrden(Orden& orden, bool action) {
  if (orden.isLight) {
    if (orden.id == "living") {
      Serial.println(action ? "Encendiendo living" : "Apagando living");
      digitalWrite(pinLiving, action ? LOW : HIGH);
    } else if (orden.id == "cocina") {
      Serial.println(action ? "Encendiendo cocina" : "Apagando cocina");
      digitalWrite(pinCocina, action ? LOW : HIGH);
    }
  } else if (orden.id == "servo") {
    Serial.println(action ? "Moviendo servo" : "Parando servo");
    if (action) {
      digitalWrite(pinLed, HIGH);
      for (int pos = 0; pos <= 180; pos++) {
        servo.write(pos);
        delay(15);
      }
      digitalWrite(pinLed, LOW);
    } else {
      digitalWrite(pinLed, HIGH);
      for (int pos = 180; pos >= 0; pos--) {
        servo.write(pos);
        delay(15);
      }
      digitalWrite(pinLed, LOW);
    }
  } else if (orden.id == "tv") {
    Serial.println(action ? "Encendiendo TV" : "Apagando TV");
    if (action) {
      irsend.sendSAMSUNG(tvPowerOnCommand);
    } else {
      irsend.sendSAMSUNG(tvPowerOffCommand);
    }
  } else if (orden.id == "sound") {
    Serial.println(action ? "Reproduciendo sonido: " + orden.soundName : "Parando sonido");
    if (action) {
      player.play(orden.soundName); // Reproduce el archivo de sonido correspondiente
    } else {
      player.stop();
    }
  }
}

void handleControlarLuces() {
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    sendErrorResponse();
    return;
  }

  unsigned long repeatInterval = 0;
  if (doc.containsKey("repeatInterval")) {
    repeatInterval = doc["repeatInterval"].as<unsigned long>() * 60000UL;
  }

  // Limpiar órdenes existentes de luces
  ordenesLuces.clear();

  for (JsonObject obj : doc["lightConfig"].as<JsonArray>()) {
    Orden orden;
    orden.id = obj["id"].as<String>();
    orden.duration = obj["duration"].as<unsigned long>() * 60000UL;
    orden.order = obj["order"].as<int>();
    orden.isLight = true;
    orden.isOn = false;
    orden.startTime = millis();
    orden.repeatInterval = repeatInterval;

    ordenesLuces.push_back(orden);
  }

  std::sort(ordenesLuces.begin(), ordenesLuces.end(), [](const Orden& a, const Orden& b) {
    return a.order < b.order;
  });

  // Reiniciar el índice de la orden actual y el tiempo de la última ejecución
  lastExecutionTime = 0;
  currentOrderIndex = 0;
  isOn = false;

  sendSuccessResponse();
}

void handleSoundSettings() {
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    sendErrorResponse();
    return;
  }

  unsigned long repeatInterval = doc["interval"].as<unsigned long>() * 60000UL;
  int soundName = doc["sound"].as<int>();

  // Limpiar órdenes existentes de sonidos
  ordenesSonidos.clear();

  Orden orden;
  orden.id = "sound";
  orden.duration = 60000; // Duración para la reproducción del sonido, puede ajustarse según sea necesario
  orden.isLight = false;
  orden.startTime = millis();
  orden.repeatInterval = repeatInterval;
  orden.soundName = soundName;

  executeOrden(orden,true);
  ordenesSonidos.push_back(orden);

  sendSuccessResponse();
}

void handleControlarServo() {
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    sendErrorResponse();
    return;
  }

  unsigned long repeatInterval = 0;
  if (doc.containsKey("repeatInterval")) {
    repeatInterval = doc["repeatInterval"].as<unsigned long>() * 60000UL;
  }

  // Limpiar órdenes existentes del servo
  ordenesGenerales.erase(std::remove_if(ordenesGenerales.begin(), ordenesGenerales.end(), [](const Orden& o) {
    return o.id == "servo";
  }), ordenesGenerales.end());

  Orden orden;
  orden.id = "servo";
  orden.duration = doc["duration"].as<unsigned long>() * 60000UL;
  orden.isLight = false;
  orden.startTime = millis();
  orden.repeatInterval = repeatInterval;

  executeOrden(orden, true);
  ordenesGenerales.push_back(orden);

  sendSuccessResponse();
}

void handleControlarTV() {
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    sendErrorResponse();
    return;
  }

  unsigned long repeatInterval = 0;
  if (doc.containsKey("repeatInterval")) {
    repeatInterval = doc["repeatInterval"].as<unsigned long>() * 60000UL;
  }

  // Limpiar órdenes existentes de la TV
  ordenesGenerales.erase(std::remove_if(ordenesGenerales.begin(), ordenesGenerales.end(), [](const Orden& o) {
    return o.id == "tv";
  }), ordenesGenerales.end());

  Orden orden;
  orden.id = "tv";
  orden.duration = doc["powerInterval"].as<unsigned long>() * 60000UL;
  orden.isLight = false;
  orden.startTime = millis();
  orden.repeatInterval = repeatInterval;

  executeOrden(orden, true);
  ordenesGenerales.push_back(orden);

  sendSuccessResponse();
}

void handleLogin() {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    sendErrorResponse();
    return;
  }

  const char* receivedUser = doc["username"];
  const char* receivedPassword = doc["password"];

  if (strcmp(receivedUser, usernameLogin) == 0 && strcmp(receivedPassword, passwordLogin) == 0) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(200, "application/json", "{\"message\":\"success\", \"token\":\"logged\"}");
  } else {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(401, "application/json", "{\"message\":\"Invalid credentials\"}");
  }
}

void handleHealth() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", "{\"status\": \"ok\"}");
}

void handleLightsStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");

  String status = (digitalRead(pinLiving) == LOW || digitalRead(pinCocina) == LOW) ? "ENCENDIDO" : "APAGADO";
  server.send(200, "application/json", "{\"status\": \"" + status + "\"}");
}

void handleServoStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");

  String status = digitalRead(pinServo) == HIGH ? "ENCENDIDO" : "APAGADO";
  server.send(200, "application/json", "{\"status\": \"" + status + "\"}");
}

void handleReset() {
  // Borrar todas las órdenes
  ordenesLuces.clear();
  ordenesGenerales.clear();
  ordenesSonidos.clear();

  // Apagar todos los dispositivos
  digitalWrite(pinLiving, HIGH);
  digitalWrite(pinCocina, HIGH);
  servo.detach();
  irsend.sendSAMSUNG(tvPowerOffCommand); // Enviar comando de apagado de la TV
  player.stop(); // Detener la reproducción de sonido

  // Reiniciar el índice de la orden actual y el tiempo de la última ejecución para las luces
  lastExecutionTime = 0;
  currentOrderIndex = 0;
  isOn = false;

  // Re-attach the servo after reset
  servo.attach(pinServo, 0, 1280);

  // Enviar respuesta de éxito
  sendSuccessResponse();
}

void handleCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
}

void sendErrorResponse() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"JSON malformado\"}");
}

void sendSuccessResponse() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", "{\"status\": \"success\"}");
}
