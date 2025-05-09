#include "wifi_utils.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
// #include <FS.h>
// #include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"
#include "structs.h"

const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64_encode(const uint8_t* input, size_t len) {
    String result = "";
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++) {
                result += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++) {
            result += base64_chars[char_array_4[j]];
        }

        while(i++ < 3) {
            result += '=';
        }
    }

    return result;
}

Preferences preferences;

static String _obf_pfx_gen(int dummy) {
    char p[4] = {0};
    p[0] = (char)(83 ^ (dummy % 7));
    p[1] = (char)(78 ^ (dummy % 3));
    p[2] = (char)(45 ^ (dummy % 5));
    p[3] = '\0';
    for (int i = 0; i < 3; i++) {
        p[i] ^= (dummy % 5);
    }
    return String(p);
}

static void _obf_transform(uint8_t* data, size_t len, bool forward) {
    const uint8_t key = 0xA7;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key ^ (i % 4);
        if (forward) {
            data[i] = (data[i] >> 2) | (data[i] << 6);
        } else {
            data[i] = (data[i] << 2) | (data[i] >> 6);
        }
    }
}

static String _obf_enc(uint8_t* input, size_t len, int dummy) {
    _obf_transform(input, len, true);
    String result = base64_encode(input, len);
    result.replace("=", "");
    return result;
}

bool tryConnectSavedWiFi(String ssid, String &password) {
    preferences.begin("neoscout_wifi", true);
    String savedPassword = preferences.getString(ssid.c_str(), "");
    preferences.end();
    if (savedPassword != "") {
        password = savedPassword;
        return true;
    }
    return false;
}

void saveWiFiCredentials(String ssid, String password) {
    preferences.begin("neoscout_wifi", false);
    preferences.putString(ssid.c_str(), password);
    preferences.end();
    if (debugMode) {
        Serial.println("[DEBUG] Saved Wi-Fi: SSID=" + ssid);
    }
}

void clearSavedWiFi() {
    preferences.begin("neoscout_wifi", false);
    preferences.clear();
    preferences.end();
    if (browserMode) {
        Serial.println("{\"status\":\"success\",\"message\":\"Saved Wi-Fi credentials cleared\"}");
    } else {
        Serial.println("Saved Wi-Fi credentials cleared");
    }
}

bool checkServerStatus() {
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    
    Serial.println("[DEBUG] Starting server status check...");
    
    IPAddress serverIP;
    if (!WiFi.hostByName("neoscout.ru", serverIP)) {
        Serial.println("[DEBUG] DNS resolution failed");
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"DNS resolution failed for neoscout.ru\"}");
        } else {
            Serial.println("DNS resolution failed for neoscout.ru");
        }
        return false;
    }
    Serial.println("[DEBUG] Resolved IP: " + serverIP.toString());
    
    String url = String(BASE_URL) + "/ping";
    Serial.println("[DEBUG] Trying URL: " + url);
    
    http.begin(client, url);
    int httpCode = http.GET();
    
    Serial.println("[DEBUG] HTTP code: " + String(httpCode));
    if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("[DEBUG] Response: " + payload);
    } else {
        Serial.println("[DEBUG] Error: " + http.errorToString(httpCode));
    }
    
    http.end();
    return (httpCode == HTTP_CODE_OK);
}

bool validateApiKey(String apiKey) {
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    IPAddress serverIP;
    if (!WiFi.hostByName("neoscout.ru", serverIP)) {
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"DNS resolution failed for neoscout.ru\"}");
        } else {
            Serial.println("DNS resolution failed for neoscout.ru");
        }
        return false;
    }
    http.begin(client, String(BASE_URL) + "/get_me_as_device");
    http.addHeader("X-API-Key", apiKey);
    int httpCode = http.GET();
    if (debugMode) {
        Serial.println("[DEBUG] API key validation: HTTP code=" + String(httpCode));
    }
    http.end();
    return (httpCode == HTTP_CODE_OK);
}

// void uploadWiFiCredentials(String ssid, String password, String apiKey) {
//     WiFiClientSecure client;
//     HTTPClient http;
//     client.setInsecure();
//     IPAddress serverIP;
//     if (!WiFi.hostByName("neoscout.ru", serverIP)) {
//         if (browserMode) {
//             Serial.println("{\"status\":\"error\",\"message\":\"DNS resolution failed for neoscout.ru\"}");
//         } else {
//             Serial.println("DNS resolution failed for neoscout.ru");
//         }
//         return;
//     }
//     http.begin(client, String(BASE_URL) + "/upload_wifi");
//     http.addHeader("Content-Type", "application/json");
//     http.addHeader("X-API-Key", apiKey);
//     StaticJsonDocument<256> doc;
//     doc["ssid"] = ssid;
//     doc["password"] = password;
//     String jsonString;
//     serializeJson(doc, jsonString);
//     int httpCode = http.POST(jsonString);
//     if (httpCode == HTTP_CODE_OK) {
//         if (browserMode) {
//             Serial.println("{\"status\":\"success\",\"message\":\"Wi-Fi credentials uploaded\"}");
//         } else {
//             Serial.println("Wi-Fi credentials uploaded successfully");
//         }
//     } else {
//         String response = http.getString();
//         if (browserMode) {
//             Serial.println("{\"status\":\"error\",\"message\":\"Failed to upload Wi-Fi credentials, HTTP code: " + String(httpCode) + ", Response: " + response + "\"}");
//         } else {
//             Serial.println("Failed to upload Wi-Fi credentials. HTTP code: " + String(httpCode));
//             Serial.println("Server response: " + response);
//         }
//     }
//     http.end();
// }

void scanWiFiNetworks() {
    WiFi.disconnect();
    delay(100);
    networkCount = WiFi.scanNetworks();
    if (networkCount == 0) {
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"No networks found, retrying in 5s\"}");
        } else {
            Serial.println("No networks found. Retrying in 5s...");
        }
        delay(5000);
        scanWiFiNetworks();
        return;
    }
    if (browserMode) {
        StaticJsonDocument<1024> doc;
        doc["type"] = "wifi_scan";
        JsonArray networks = doc.createNestedArray("networks");
        for (int i = 0; i < networkCount; i++) {
            String ssid = WiFi.SSID(i);
            String password;
            bool isSaved = tryConnectSavedWiFi(ssid, password);
            if (isSaved) {
                selectedSSID = ssid;
                selectedPassword = password;
                connectToWiFi();
                return;
            }
            JsonObject net = networks.createNestedObject();
            net["ssid"] = ssid;
            net["rssi"] = WiFi.RSSI(i);
        }
        String jsonString;
        serializeJson(doc, jsonString);
        Serial.println(jsonString);
        awaitingNetworkSelection = true;
    } else {
        Serial.printf("Found %d networks:\n", networkCount);
        for (int i = 0; i < networkCount; i++) {
            String ssid = WiFi.SSID(i);
            String password;
            if (tryConnectSavedWiFi(ssid, password)) {
                selectedSSID = ssid;
                selectedPassword = password;
                connectToWiFi();
                return;
            }
            Serial.printf("%d: %s (Signal: %d dBm)\n", i + 1, ssid.c_str(), WiFi.RSSI(i));
        }
        Serial.printf("Enter Wi-Fi network number (1-%d):\n", networkCount);
        awaitingNetworkSelection = true;
    }
}

void processWiFiSelection(String input) {
    if (browserMode) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, input);
        if (error || !doc.containsKey("ssid")) {
            Serial.println("{\"status\":\"error\",\"message\":\"Invalid JSON or missing ssid\"}");
            return;
        }
        selectedSSID = doc["ssid"].as<String>();
        if (tryConnectSavedWiFi(selectedSSID, selectedPassword)) {
            connectToWiFi();
        } else {
            Serial.println("{\"status\":\"success\",\"message\":\"SSID selected, enter password\"}");
            awaitingNetworkSelection = false;
            awaitingPassword = true;
        }
    } else {
        int networkNumber = input.toInt();
        if (networkNumber < 1 || networkNumber > networkCount) {
            Serial.printf("Invalid number. Enter 1 to %d\n", networkCount);
            awaitingNetworkSelection = false;
            scanWiFiNetworks();
            return;
        }
        selectedSSID = WiFi.SSID(networkNumber - 1);
        if (tryConnectSavedWiFi(selectedSSID, selectedPassword)) {
            connectToWiFi();
        } else {
            Serial.printf("Selected: %s\n", selectedSSID.c_str());
            Serial.printf("Enter password for %s:\n", selectedSSID.c_str());
            awaitingNetworkSelection = false;
            awaitingPassword = true;
        }
    }
}

void connectToWiFi() {
    WiFi.begin(selectedSSID.c_str(), selectedPassword.c_str());
    if (!browserMode) {
        Serial.printf("Connecting to %s...\n", selectedSSID.c_str());
    }
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        if (!browserMode) Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        saveWiFiCredentials(selectedSSID, selectedPassword);
        if (browserMode) {
            StaticJsonDocument<256> doc;
            doc["type"] = "wifi_connected";
            doc["ip"] = WiFi.localIP().toString();
            String jsonString;
            serializeJson(doc, jsonString);
            Serial.println(jsonString);
        } else {
            Serial.println();
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        }
        if (!hasInternet()) {
            if (browserMode) {
                Serial.println("{\"status\":\"error\",\"message\":\"No internet connection\"}");
            } else {
                Serial.println("No internet connection");
            }
            return;
        }
        if (!checkServerStatus()) {
            if (browserMode) {
                Serial.println("{\"status\":\"error\",\"message\":\"Cannot connect to neoscout.ru\"}");
            } else {
                Serial.println("Cannot connect to neoscout.ru");
            }
            return;
        }
        preferences.begin("neoscout", true);
        apiKey = preferences.getString("api_key", "");
        preferences.end();
        if (debugMode) {
            Serial.println("[DEBUG] Loaded API key: " + (apiKey == "" ? "None" : apiKey));
        }
        if (apiKey != "") {
            if (validateApiKey(apiKey)) {
                deviceConnected = true;
                uploadReport = true;
                // uploadWiFiCredentials(selectedSSID, selectedPassword, apiKey);
            } else {
                if (browserMode) {
                    Serial.println("{\"status\":\"error\",\"message\":\"Invalid API key, please reconnect device\"}");
                } else {
                    Serial.println("Invalid API key, please reconnect device");
                }
                preferences.begin("neoscout", false);
                preferences.remove("api_key");
                preferences.end();
                apiKey = "";
                deviceConnected = false;
                promptDeviceConnection();
            }
        } else {
            promptDeviceConnection();
        }
    } else {
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"Connection failed\"}");
        } else {
            Serial.println("Connection failed");
        }
        selectedSSID = "";
        selectedPassword = "";
        awaitingPassword = false;
        awaitingNetworkSelection = false;
        scanWiFiNetworks();
    }
}

bool isLocalIP(String ip) {
    IPAddress addr;
    if (!addr.fromString(ip)) return false;
    uint32_t ipAddr = (uint32_t)addr;
    return (ipAddr & 0xFFFF0000) == 0xC0A80000 ||
           (ipAddr & 0xFF000000) == 0x0A000000 ||
           (ipAddr & 0xFFF00000) == 0xAC100000;
}

bool isIPAddress(String str) {
    IPAddress addr;
    return addr.fromString(str);
}

bool hasInternet() {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://api.ipify.org");
    int httpCode = http.GET();
    bool result = (httpCode == HTTP_CODE_OK);
    if (debugMode) {
        Serial.println("[DEBUG] Internet check: HTTP code=" + String(httpCode));
    }
    http.end();
    return result;
}

void promptDeviceConnection() {
    preferences.begin("neoscout", true);
    apiKey = preferences.getString("api_key", "");
    preferences.end();
    if (apiKey != "") {
        if (validateApiKey(apiKey)) {
            deviceConnected = true;
            if (browserMode) {
                Serial.println("{\"status\":\"success\",\"message\":\"Device already connected\"}");
            } else {
                Serial.println("Device already connected to neoscout.ru");
            }
            // uploadWiFiCredentials(selectedSSID, selectedPassword, apiKey);
            return;
        } else {
            preferences.begin("neoscout", false);
            preferences.remove("api_key");
            preferences.end();
            apiKey = "";
        }
    }
    if (browserMode) {
        Serial.println("{\"type\":\"prompt\",\"message\":\"Connect device to neoscout.ru? (yes/no)\"}");
    } else {
        Serial.println("Internet detected. Would you like to connect this device to neoscout.ru? (yes/no)");
    }
    awaitingPassword = true;
}

void processDeviceCode(String input) {
    if (browserMode) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, input);
        if (error) {
            Serial.println("{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
            return;
        }
        if (doc.containsKey("connect") && doc["connect"].as<String>() == "yes") {
            Serial.println("{\"type\":\"prompt\",\"message\":\"Enter 6-digit code from neoscout.ru\"}");
            return;
        } else if (doc.containsKey("connect") && doc["connect"].as<String>() == "no") {
            uploadReport = false;
            Serial.println("{\"status\":\"success\",\"message\":\"Device connection skipped\"}");
            awaitingPassword = false;
            // uploadWiFiCredentials(selectedSSID, selectedPassword, apiKey);
            return;
        } else if (doc.containsKey("code")) {
            input = doc["code"].as<String>();
        } else {
            Serial.println("{\"status\":\"error\",\"message\":\"Invalid input, expected connect or code\"}");
            return;
        }
    } else {
        if (input.equalsIgnoreCase("yes")) {
            Serial.println("Please visit https://neoscout.ru/dashboard?menu=devices, click 'Connect New Device', and enter the provided code.");
            Serial.println("Enter the 6-digit code:");
            return;
        } else if (input.equalsIgnoreCase("no")) {
            uploadReport = false;
            Serial.println("Device connection skipped. Reports will not be uploaded.");
            awaitingPassword = false;
            // uploadWiFiCredentials(selectedSSID, selectedPassword, apiKey);
            return;
        }
    }
    if (input.length() != 6 || !input.toInt()) {
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"Invalid code, must be 6 digits\"}");
        } else {
            Serial.println("Invalid code. Please enter a 6-digit code:");
        }
        return;
    }
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    http.begin(client, String(BASE_URL) + "/connect_device");
    http.addHeader("Content-Type", "application/json");
    String mac = WiFi.macAddress();
    uint8_t macBytes[17];
    memcpy(macBytes, mac.c_str(), 17);
    _obf_transform(macBytes, 17, true);
    int dummy = macBytes[0] * macBytes[16] % 23;
    String base64Mac = _obf_enc(macBytes, 17, dummy);
    String prefix = _obf_pfx_gen(dummy + macBytes[2]);
    String serialNumber = prefix + base64Mac;
    _obf_transform(macBytes, 17, false);
    String originalBase64Mac = base64_encode(macBytes, 17);
    originalBase64Mac.replace("=", "");
    String originalSerialNumber = "SN-" + originalBase64Mac;
    StaticJsonDocument<1024> doc;
    doc["code"] = input;
    JsonObject deviceInfo = doc.createNestedObject("device_info");
    deviceInfo["name"] = "NeoScout DIY (ESP8266)";
    deviceInfo["serial_number"] = originalSerialNumber;
    String jsonString;
    serializeJson(doc, jsonString);
    int httpCode = http.POST(jsonString);
    if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        StaticJsonDocument<256> respDoc;
        DeserializationError error = deserializeJson(respDoc, response);
        if (!error && respDoc.containsKey("api_key")) {
            apiKey = respDoc["api_key"].as<String>();
            preferences.begin("neoscout", false);
            if (debugMode) {
                Serial.println("[DEBUG] Saving API key: " + apiKey);
            }
            if (!preferences.putString("api_key", apiKey)) {
                if (browserMode) {
                    Serial.println("{\"status\":\"error\",\"message\":\"Failed to save API key to NVS\"}");
                } else {
                    Serial.println("Failed to save API key to NVS");
                }
                http.end();
                return;
            }
            preferences.end();
            preferences.begin("neoscout", true);
            String savedApiKey = preferences.getString("api_key", "");
            preferences.end();
            if (savedApiKey != apiKey) {
                if (browserMode) {
                    Serial.println("{\"status\":\"error\",\"message\":\"API key verification failed\"}");
                } else {
                    Serial.println("API key verification failed. Saved: " + savedApiKey + ", Expected: " + apiKey);
                }
                http.end();
                return;
            }
            deviceConnected = true;
            uploadReport = true;
            if (browserMode) {
                Serial.println("{\"status\":\"success\",\"message\":\"Device connected successfully\"}");
            } else {
                Serial.println("Device connected successfully! API key saved in NVS.");
            }
            // uploadWiFiCredentials(selectedSSID, selectedPassword, apiKey);
            awaitingPassword = false;
        } else {
            if (browserMode) {
                Serial.println("{\"status\":\"error\",\"message\":\"Failed to parse response or no API key received\"}");
            } else {
                Serial.println("Failed to parse response or no API key received.");
            }
        }
    } else {
        if (browserMode) {
            Serial.println("{\"status\":\"error\",\"message\":\"Connection failed, HTTP code: " + String(httpCode) + "\"}");
        } else {
            Serial.println("Connection failed. HTTP code: " + String(httpCode));
        }
    }
    http.end();
    awaitingPassword = false;
}