#include <WiFi.h>
#include <Arduino.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>


// Configura tus credenciales Wi-Fi
const char* ssid = "motog_nico";
const char* password = "asd12345";
const char* usernameLogin = "admin";
const char* passwordLogin = "admin";

WebServer server(80);

// Pines para las luces
const int pinLiving = 2;
const int pinCocina = 4;
const int pinServo = 25;
const int pinLed = 22;
const int pinIrReceiver = 23;
const int pinIrSender = 21;

Servo servo;
IRsend irsend(pinIrSender);  // Set the GPIO to be used to sending the message.


void setup() {
  // Inicializa los pines como salidas
  pinMode(pinLiving, OUTPUT);
  pinMode(pinCocina, OUTPUT);
  pinMode(pinLed, OUTPUT);
  irsend.begin();

  // Inicializa el servo con el pin y las posiciones mínima y máxima
  servo.attach(pinServo, 500, 2500);


  digitalWrite(pinLiving, HIGH); // ESTA INVERTIDO
  digitalWrite(pinCocina, HIGH);

  
  // Inicia la comunicación serial
  Serial.begin(115200);
  
  // Conéctate a Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
  Serial.println(WiFi.localIP());

  // Configura el servidor para manejar la ruta de la solicitud POST y OPTIONS
  server.on("/controlar-luces", HTTP_POST, handleControlarLuces);
  server.on("/controlar-luces", HTTP_OPTIONS, handleCors);
  server.on("/controlar-servo", HTTP_POST, handleControlarServo);
  server.on("/controlar-servo", HTTP_OPTIONS, handleCors);
  server.on("/controlar-tv",HTTP_POST,handleControlarTV);
  server.on("/controlar-tv",HTTP_OPTIONS,handleCors);
  server.on("/login",HTTP_POST,handleLogin);
  server.on("/login",HTTP_OPTIONS,handleCors);


  // Inicia el servidor
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  // Maneja las solicitudes del cliente
digitalWrite(pinLiving,LOW);
delay
}

void handleControlarLuces() {
  // Lee el cuerpo de la solicitud
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  // Parsear el JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"JSON malformado\"}");
    return;
  }

  int ordenActual = 0;

  // Procesar cada objeto en el array
  for (JsonObject obj : doc.as<JsonArray>()) {
    const char* id = obj["id"];
    int duration = obj["duration"];
    int order = obj["order"];
    
    // Controlar las luces según el ID
    if (ordenActual == 0){
      if (strcmp(id, "living") == 0 && order == 1) {
      digitalWrite(pinLiving, LOW);
      delay(duration * 60000);
      digitalWrite(pinLiving, HIGH);
      ordenActual++;
      Serial.println(ordenActual);
    } else if (strcmp(id, "cocina") == 0 && order == 1 ) {
      Serial.println(ordenActual);
      digitalWrite(pinCocina, LOW);
      delay(duration * 60000);
      digitalWrite(pinCocina, HIGH);
      ordenActual++;
      Serial.println(ordenActual);
    }
    } else if(ordenActual == 1){
      Serial.println(ordenActual);
      Serial.println(id);
      if (strcmp(id, "living") == 0 && order == 2) {
      digitalWrite(pinLiving, LOW);
      delay(duration * 60000);
      digitalWrite(pinLiving, HIGH);
      ordenActual++;
      Serial.println(ordenActual);
    } else if (strcmp(id, "cocina") == 0 && order == 2) {
      Serial.println(id);
      Serial.println(ordenActual);
      digitalWrite(pinCocina, LOW);
      delay(duration * 60000);
      digitalWrite(pinCocina, HIGH);
      ordenActual++;
      Serial.println(ordenActual);
    }
    }
    
  }

  // Enviar respuesta de éxito con las cabeceras CORS
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", "{\"status\": \"success\"}");
}

void handleControlarServo() {
  // Lee el cuerpo de la solicitud
  String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  // Parsear el JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"JSON malformado\"}");
    return;
  }

  // Leer los datos del JSON
  int initialPosition = doc["initalPosition"];
  int finalPosition = doc["finalPosition"];
  int duration = doc["duration"];
  int pos = 0;

  // Mover el servo a las posiciones indicadas
  for (int pos = initialPosition; pos <= finalPosition; pos++) {
    digitalWrite(pinLed,HIGH);
    //Movemos el servo a los grados que le entreguemos
    servo.write(pos);
    //Espera de 15 milisegundos
    delay(15);
  }

  delay(duration*60000);
  digitalWrite(pinLed,LOW);


  //Ciclo que posicionara el servo de 180 hasta 0 grados
  for (int pos = finalPosition; pos >= initialPosition; pos--) {
    digitalWrite(pinLed,HIGH);
    //Movemos el servo a los grados que le entreguemos
    servo.write(pos);
    //Espera de 15 milisegundos
    delay(15);
  }
  digitalWrite(pinLed,LOW);


  // Enviar respuesta de éxito con las cabeceras CORS
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", "{\"status\": \"success\"}");
}

void handleControlarTV(){

 String json = server.arg("plain");
  Serial.println("JSON recibido: " + json);

  // Parsear el JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, json);

  int intervaloEncendido = doc["powerInterval"];
  int intervaloCambioCanal = doc["channelChangeInterval"];
  int intervaloSubidaBajadaVolumen = doc["volumeChangeInterval"];

  Serial.println("SAMSUNG");
  irsend.sendSAMSUNG(0xE0E06798);
  delay(intervaloEncendido*60000);
  irsend.sendSAMSUNG(0xE0E06798);


  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"JSON malformado\"}");
    return;
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(200, "application/json", "{\"status\": \"success\"}");
}

void handleLogin(){

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

   // Obtén el usuario y la contraseña del cuerpo de la solicitud
    const char* receivedUser = doc["username"];
    const char* receivedPassword = doc["password"];

    // Valida las credenciales
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

  if (error) {
    Serial.print("Error al parsear el JSON: ");
    Serial.println(error.c_str());
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    server.send(400, "application/json", "{\"status\": \"error\", \"message\": \"JSON malformado\"}");
    return;
  }
}

void handleCors() {
  // Manejar la solicitud OPTIONS para CORS
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.send(204);
}

