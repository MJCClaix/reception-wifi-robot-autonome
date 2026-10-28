/*
 * Serveur TCP — LOLIN ESP8266 D1 Mini Pro
 * Reçoit les données de position du programme Aruco_detection.py
 * Format : 3 entiers 32 bits signés big-endian = 12 octets
 *          [x_mm (4)] [y_mm (4)] [angle (4)]
 *
 * L'ESP8266 écoute sur le port 80 en TCP.
 * IP fixe : 192.168.1.3
 * quid de la MAC adresse ?
 */
/* Sur D1 mini :
 *  - SDA : GPIO ... -> D2
 *  - SCL : GPIO ... -> D1
 */
#include <ESP8266WiFi.h>
#include <Wire.h>
// ─── Configuration WiFi ───────────────────────────────────────────────────────
#define STASSID "NETGEAR00"
#define STAPSK  "manicteapot411"
// ─── Configuration réseau ─────────────────────────────────────────────────────
// IP fixe pour que le PC Aruco sache toujours où envoyer
IPAddress ip(192, 168, 1, 3);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);
const uint16_t TCP_PORT = 80;
WiFiServer server(TCP_PORT);
// ─── Variables de position ────────────────────────────────────────────────────
int16_t cmd_x       = 0;
int16_t cmd_y       = 0;
int16_t cmd_angle   = 0;
bool new_data   = false;
// ─── Taille d'un paquet position ─────────────────────────────────────────────
#define PACKET_SIZE  12   // 3 × int32 = 12 octets
// ─────────────────────────────────────────────────────────────────────────────

union IntToByte {
  // 'converts' incoming bytes to a float array
  // valueReading: [0] wheelSpeed; [1] wheelAngle; [2] wheelRotation
  byte valueBuffer[6];
  int16_t valueWriting[3];
} converter;
//*************************************************************************
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println("\n\n=== LOLIN D1 Mini Pro — Serveur position Aruco ===");
  // IP fixe
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  Wire.begin();
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connecté !");
  Serial.print("IP : ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.print("Serveur TCP démarré sur le port ");
  Serial.println(TCP_PORT);
}
// ─────────────────────────────────────────────────────────────────────────────
// Décode 4 octets big-endian en int32
int16_t decode_int16(uint8_t *buf) {
  return ((int16_t)buf[0] << 24) |
         ((int16_t)buf[1] << 16) |
         ((int16_t)buf[2] <<  8) |
         ((int16_t)buf[3]);
}
//********************************************
void send_to_robot() {
  Wire.beginTransmission(27);  // adresse i2c 27
  converter.valueWriting[0] = cmd_x;
  converter.valueWriting[1] = cmd_y;
  converter.valueWriting[2] = cmd_angle;
  Wire.write(converter.valueBuffer, sizeof(converter.valueBuffer));
  /*Wire.write(cmd_x);
  Serial.println(cmd_x);
  Wire.write(cmd_y);
  Serial.println(cmd_y);
  Wire.write(cmd_angle);
  Serial.println(cmd_angle);*/
  Wire.endTransmission();
}
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    digitalWrite(LED_BUILTIN, HIGH);  
  delay(50);                       
  digitalWrite(LED_BUILTIN, LOW);
  WiFiClient client = server.available();
  if (client) {  //(client || true)
    Serial.println("[TCP] Client connecté");
    uint8_t buf[PACKET_SIZE];
    uint8_t buf_idx = 0;
    // Timeout : si plus de données pendant 3s, on ferme
    unsigned long last_data = millis();
    while (client.connected()) { //(client.connected() || true)
      // Watchdog connexion
      if (millis() - last_data > 3000) {// 3 secondes de time out
        Serial.println("[TCP] Timeout — fermeture");
        break;
      }
      while (client.available() && buf_idx < PACKET_SIZE) {//(client.available() && buf_idx < PACKET_SIZE && false)
        buf[buf_idx++] = client.read();
        last_data = millis();
      }
      // Paquet complet reçu
      if (buf_idx == PACKET_SIZE) {//(buf_idx == PACKET_SIZE || true)
        cmd_x     = decode_int16(&buf[0]);
        cmd_y     = decode_int16(&buf[4]);
        cmd_angle = decode_int16(&buf[8]);
        new_data = true;
        buf_idx  = 0;   // prêt pour le paquet suivant
        // Affichage série
        Serial.print("[CMD] X=");
        Serial.print(cmd_x);
        Serial.print("     Y=");
        Serial.print(cmd_y);
        Serial.print("     Angle=");
        Serial.print(cmd_angle);
        Serial.println("°");
        // ── Ici : utilisez x_mm, y_mm, angle pour piloter le robot ──
        // Exemple : envoyer vers le contrôleur moteur via Serial / I2C / SPI
        send_to_robot();
      }
      yield();  // laisser le WiFi stack travailler
    }
    client.stop();
    Serial.println("[TCP] Client déconnecté");
  }
  // ── Traitement des nouvelles données hors connexion TCP ──
  if (new_data) {
    new_data = false;
    // Traitements supplémentaires si nécessaire
  }
}
