#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "structs.h"
#include "config.h"
#include "wifi_utils.h"

String getLocation();
void uploadScanReport();

#endif