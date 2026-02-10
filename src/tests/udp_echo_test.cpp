#include <WiFi.h>
#include <WiFiUdp.h>

/**************************************
 *   WIFI CONFIG
**************************************/
const char* ssid =      "cs179j_gamepod";
const char* password =  "86g43B!2";


WiFiUDP udp;
const unsigned int localUdpPort = 5005;  // Port to listen on (matches server_init.py)
char incomingPacket[255];  // Buffer for incoming packets

/**************************************
 *   WIFI INIT
**************************************/
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== ESP32 UDP Test ===");

  // Connect to WiFi
  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Listening on UDP port %d\n", localUdpPort);

  // Start UDP
  udp.begin(localUdpPort);
}

/**************************************
 *   WIFI Packet Receive & Return test
**************************************/
void loop() {

  // if data is available
  int packetSize = udp.parsePacket();

  if (packetSize) {
    // receive incoming packet
    int len = udp.read(incomingPacket, 255);
    if (len > 0) { // if packet was read
      incomingPacket[len] = 0;  // end of packet marker
    }

    Serial.printf("Received %d bytes from %s:%d\n",
                  packetSize,
                  udp.remoteIP().toString().c_str(),
                  udp.remotePort());
    Serial.printf("Message: %s\n", incomingPacket);

    // Check if it's a handshake message
    if (strncmp(incomingPacket, "HANDSHAKE:", 10) == 0) {
      // Extract player ID from message 
      int playerID = incomingPacket[10] - '0';

      Serial.printf("Handshake from Player %d - Sending ACK\n", playerID);

      // Send ACK response
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.printf("ACK:Player%d", playerID);
      udp.endPacket();
    } else {
      // For non-handshake messages, echo back
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.printf("Echo: %s", incomingPacket);
      udp.endPacket();
    }
  }

  delay(10);
}
