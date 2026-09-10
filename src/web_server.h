#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

class RoverWebServer {
public:
    RoverWebServer();

    void begin();
    void update(); // Handles periodic WebSocket broadcast

    void broadcastTelemetry();

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    uint32_t _lastBroadcastMs;
    uint16_t _learnBaseline[CRSF_NUM_CHANNELS];
    bool _learnActive;

    void setupWiFi();
    void setupRoutes();
    void setupWebSocket();

    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
};

extern RoverWebServer webServer;
