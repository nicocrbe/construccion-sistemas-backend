#include <WiFi.h>
#include <Arduino.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>
#include <vector>
#include "config.h"  // Incluir el archivo de configuración generado

WebServer server(80);

const int pinLiving = 2;
const int pinCocina = 4;
const int pinServo = 25;
const int pinLed = 22;
const int pinIrReceiver = 23;
const int pinIrSender = 21;
const int pinDFPlayerRx = 16; // RX pin del DFPlayer Mini
const int pinDFPlayerTx = 17; // TX pin del DFPlayer Mini

Servo servo;
IRsend irsend(pinIrSender);

SoftwareSerial mySoftwareSerial(pinDFPlayerRx, pinDFPlayerTx); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

struct Orden {
  String id;
  unsigned long duration;
  int order;
  bool isLight;
  unsigned long startTime;
  bool isOn;
  unsigned long repeatInterval;
  String soundName;
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

  Serial.begin(115200);
  mySoftwareSerial.begin(9600);
  myDFPlayer.begin(mySoftwareSerial);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
  Serial.println(WiFi.localIP());

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
      int trackNumber = getTrackNumber(orden.soundName);
      myDFPlayer.play(trackNumber); // Reproduce el archivo de sonido correspondiente
    } else {
      myDFPlayer.stop();
    }
  }
}

int getTrackNumber(String soundName) {
  if (soundName == "Ladridos") {
    return 1;
  }
  if (soundName == "Conversaciones") {
    return 2;
  }
  if (soundName == "Lavarropas") {
    return 3;
  }
  // Añade más casos según los archivos de sonido disponibles
  return 1; // Valor por defecto
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
  String soundName = doc["sound"].as<String>();

  // Limpiar órdenes existentes de sonidos
  ordenesSonidos.clear();

  Orden orden;
  orden.id = "sound";
  orden.duration = 60000; // Duración para la reproducción del sonido, puede ajustarse según sea necesario
  orden.isLight = false;
  orden.startTime = millis();
  orden.repeatInterval = repeatInterval;
  orden.soundName = soundName;

  ordenesSonidos.push_back(orden);

  sendSuccessResponse();
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
  myDFPlayer.stop(); // Detener la reproducción de sonido

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