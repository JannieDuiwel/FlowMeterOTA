#include "thingProperties.h"

float flowRate;
float flowRateM3H;
float litersPerPulse;
float totalLiters;
bool resetTotal;

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);

void initProperties() {

  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);

  ArduinoCloud.addProperty(flowRate, READ, 1 * SECONDS, NULL);
  ArduinoCloud.addProperty(flowRateM3H, READ, 5 * SECONDS, NULL);
  ArduinoCloud.addProperty(litersPerPulse, READWRITE, ON_CHANGE, onLitersPerPulseChange);
  ArduinoCloud.addProperty(totalLiters, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(resetTotal, READWRITE, ON_CHANGE, onResetTotalChange);

  // Default safe value (100 liters per pulse)
  litersPerPulse = 100;
}
