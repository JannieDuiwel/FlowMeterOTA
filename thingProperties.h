#pragma once

#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "arduino_secrets.h"

const char DEVICE_LOGIN_NAME[]  = "b22cce1e-1c52-4338-b513-d06abf7a940f";

// Match secrets file:
const char SSID[] = SECRET_SSID;
const char PASS[] = SECRET_PASS;     // <<< CHANGED — the Arduino template used SECRET_OPTIONAL_PASS
const char DEVICE_KEY[] = SECRET_DEVICE_KEY;

void onLitersPerPulseChange();
void onResetTotalChange();

// Cloud variables
extern float flowRate;
extern float flowRateM3H;
extern float litersPerPulse;
extern float totalLiters;
extern bool resetTotal;

void initProperties();

// WiFi handler
extern WiFiConnectionHandler ArduinoIoTPreferredConnection;
