#define RXD2 26
#define TXD2 27
// FUNCIONAAA REPRODUCE CADA 30 SEGS LAS PISTAS Y EMPIEZA DESDE LA 1 EN BUCLE
int currentTrack = 1; // Variable para llevar el registro del número de pista actual
const int totalTracks = 3; // Total de pistas disponibles
unsigned long trackStartTime = 0; // Variable para llevar el registro del tiempo de inicio de la pista

// Esta función envía el comando real
// Recibe el byte de comando y ParData es información opcional, como el número de pista o el volumen, dependiendo del comando
void sendDFCommand(byte Command, int ParData) {
  byte commandData[10]; // Esto contiene todos los datos del comando que se enviarán
  byte q;
  int checkSum;
  Serial.print("Com: ");
  Serial.print(Command, HEX);
  // Cada valor de comando se envía en hexadecimal
  commandData[0] = 0x7E; // Inicio de un nuevo comando
  commandData[1] = 0xFF; // Información de versión
  commandData[2] = 0x06; // Longitud de datos (sin incluir la paridad) o el inicio y la versión
  commandData[3] = Command; // El comando que se envió
  commandData[4] = 0x01; // 1 = feedback
  commandData[5] = highByte(ParData); // Byte alto de los datos enviados
  commandData[6] = lowByte(ParData); // Byte bajo de los datos enviados
  checkSum = -(commandData[1] + commandData[2] + commandData[3] + commandData[4] + commandData[5] + commandData[6]);
  commandData[7] = highByte(checkSum); // Byte alto del checksum
  commandData[8] = lowByte(checkSum); // Byte bajo del checksum
  commandData[9] = 0xEF; // Byte de fin
  for (q = 0; q < 10; q++) {
    Serial2.write(commandData[q]);
  }
  Serial.println("Comando Enviado: ");
  for (q = 0; q < 10; q++) {
    Serial.println(commandData[q], HEX);
  }
  Serial.println("Fin del Comando: ");
  delay(100);
}

// Reproducir una pista específica
void playTrack(int tracknum) {
  Serial.print("Pista seleccionada: ");
  Serial.println(tracknum);
  sendDFCommand(0x03, tracknum);
  trackStartTime = millis(); // Registro del tiempo de inicio de la pista
}

// Reproducir la siguiente pista
void playNext() {
  Serial.println("Reproducir Siguiente Pista");
  currentTrack++; // Incrementar el número de pista actual
  if (currentTrack > totalTracks) { // Si se supera el total de pistas, reiniciar al inicio
    currentTrack = 1;
  }
  playTrack(currentTrack);
}

// Aumentar el volumen en 1
void volumeUp() {
  Serial.println("Volumen Arriba");
  sendDFCommand(0x04, 0);
}

// Disminuir el volumen en 1
void volumeDown() {
  Serial.println("Volumen Abajo");
  sendDFCommand(0x05, 0);
}

// Establecer el volumen a un valor específico
void changeVolume(int thevolume) {
  sendDFCommand(0x06, thevolume);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Script de Prueba DFPlayer");

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial2 iniciado...");

  delay(3500); // Dejar que todo se inicialice
  changeVolume(20); // Establecer el volumen a 15. 30 es muy alto incluso en un altavoz pequeño.
  delay(100);
  playTrack(currentTrack); // Reproducir la primera pista
}

int q = 5;
byte returnCodes[10];
byte returnByteCounter = 0;

void processReturnCode() {
  Serial.println("Cadena de Códigos");
  for (int w = 0; w < 10; w++) {
    Serial.print(returnCodes[w], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");
  if (returnCodes[3] == 0x3D) { // Pista terminada
    Serial.println("Reproducir Siguiente Pista");
    playNext(); // Reproducir la siguiente pista
  }
  returnByteCounter = 0;
}

void readSerial2() {
  byte readByte;
  while (Serial2.available()) {
    readByte = Serial2.read();
    Serial.print(readByte, HEX);
    Serial.print(" Contador: ");
    Serial.println(returnByteCounter);

    if (returnByteCounter == 0 && readByte == 0x7E) {
      returnCodes[returnByteCounter++] = readByte;
    } else if (returnByteCounter > 0) {
      returnCodes[returnByteCounter++] = readByte;
    }

    if (returnByteCounter > 9) {
      processReturnCode();
    }
  }
}

void loop() {
  readSerial2();
  
  // Verificar si han pasado 30 segundos desde el inicio de la pista actual
  if (millis() - trackStartTime >= 30000) {
    playNext(); // Reproducir la siguiente pista
  }
}
