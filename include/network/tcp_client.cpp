#include "tcp_client.h"

static WiFiClient tcpClient;

// ============================================
//  WIFI
// ============================================

bool wifi_connect(const char* ssid, const char* password, unsigned long timeoutMs) {
  Serial.printf("[WiFi] Connecting to %s", ssid);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("\n[WiFi] Connection timed out!");
      return false;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  // Override DNS to Google (8.8.8.8) - phone hotspots often don't provide
  // a working DNS server, which breaks hostname resolution (needed for ngrok)
  IPAddress dns(8, 8, 8, 8);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns);

  Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("[WiFi] DNS set to 8.8.8.8");
  return true;
}

bool wifi_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifi_disconnect() {
  WiFi.disconnect();
  Serial.println("[WiFi] Disconnected.");
}

// ============================================
//  TCP
// ============================================

bool tcp_connect(const char* host, uint16_t port) {
  Serial.printf("[TCP] Connecting to %s:%d...\n", host, port);

  if (tcpClient.connect(host, port)) {
    Serial.println("[TCP] Connected!");
    return true;
  }

  Serial.println("[TCP] Connection failed!");
  return false;
}

bool tcp_is_connected() {
  return tcpClient.connected();
}

bool tcp_send(const char* message) {
  if (!tcpClient.connected()) {
    Serial.println("[TCP] Not connected, can't send.");
    return false;
  }

  tcpClient.print(message);
  tcpClient.print('\n');
  return true;
}

bool tcp_send_join(uint8_t playerID) {
  char buf[16];
  snprintf(buf, sizeof(buf), "JOIN:%d", playerID);
  Serial.printf("[TCP] Sending: %s\n", buf);
  return tcp_send(buf);
}

bool tcp_send_direction(uint8_t playerID, const char* direction) {
  char buf[32];
  snprintf(buf, sizeof(buf), "DIR:%d:%s", playerID, direction);
  Serial.printf("[TCP] Sending: %s\n", buf);
  return tcp_send(buf);
}

void tcp_disconnect() {
  tcpClient.stop();
  Serial.println("[TCP] Disconnected.");
}

int tcp_available() {
  return (int)tcpClient.available();
}

int tcp_read() {
  return tcpClient.read();
}
