#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>

// ============================================
//  TCP CLIENT - Reusable networking functions
//  for connecting to the Tron game server.
//
//  Usage:
//    1. wifi_connect(ssid, password)
//    2. tcp_connect(host, port)    // host can be IP or hostname (e.g. ngrok)
//    3. tcp_send_join(playerID)
//    4. tcp_send_direction(playerID, "UP")
//    5. tcp_disconnect() when done
// ============================================

// Connect to a WiFi network. Blocks until connected or timeout.
// Returns true on success, false on timeout.
bool wifi_connect(const char* ssid, const char* password, unsigned long timeoutMs = 10000);

// Returns true if WiFi is currently connected.
bool wifi_is_connected();

// Connect to the Tron game server via TCP.
// host can be an IP address ("192.168.1.1") or a hostname ("x.tcp.ngrok.io").
// Returns true on success.
bool tcp_connect(const char* host, uint16_t port);

// Returns true if TCP socket is currently connected.
bool tcp_is_connected();

// Send JOIN:<playerID>\n to register with the server.
// Returns true if sent successfully.
bool tcp_send_join(uint8_t playerID);

// Send DIR:<playerID>:<direction>\n to the server.
// direction must be one of: "UP", "DOWN", "LEFT", "RIGHT"
// Returns true if sent successfully.
bool tcp_send_direction(uint8_t playerID, const char* direction);

// Send a raw string over TCP (adds newline automatically).
// Returns true if sent successfully.
bool tcp_send(const char* message);

// Disconnect the TCP socket.
void tcp_disconnect();

// Disconnect WiFi.
void wifi_disconnect();

// Returns the number of bytes available to read from the TCP socket.
int tcp_available();

// Reads and returns one byte from the TCP socket (-1 if none available).
int tcp_read();

#endif // TCP_CLIENT_H
