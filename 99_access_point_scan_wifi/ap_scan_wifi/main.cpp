/*
 * ESP32 Access Point with Web Server WiFi Scanner
 * 
 * Creates an access point that serves a web page allowing users to:
 * 1. Connect to the access point
 * 2. View a list of available WiFi networks
 * 3. Select a network to connect to
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

// ===== Configuration =====
const char* AP_SSID = "IOT-Ratter-Setup";
const char* AP_PASSWORD = "12345678";
const int AP_CHANNEL = 1;
const int AP_MAX_CONNECTIONS = 4;

// Serial configuration
#define SERIAL_BAUD 115200
#define SERIAL_BEGIN_DELAY 2000

// ===== Global Variables =====
WebServer server(80);
Preferences preferences;
int numNetworks = 0;
bool isInSTAMode = false;
TaskHandle_t flashTask = NULL;

// Preference keys
const char* PREF_NAMESPACE = "wifi_config";
const char* PREF_SSID_KEY = "ssid";
const char* PREF_PASSWORD_KEY = "password";
const char* PREF_DEVICE_NAME_KEY = "device_name";

// WiFi connection timeout (milliseconds)
const int WIFI_CONNECT_TIMEOUT = 10000;

// Function prototypes
void setupAccessPoint();
void setupWebServer();
void scanNetworks();
String getNetworkListHTML();
void handleRoot();
void handleScan();
void handleConnect();
void handleReset();
void handleNotFound();
void handleGetDeviceName();
void handleSetDeviceName();
void handleScript();
void handleStyle();
bool tryConnectWithSavedCredentials();
void clearSavedCredentials();
//void webServerTask(void *pvParameters);

void webServerTask(void *pvParameters) {
  for (;;) {
    server.handleClient();
    // Yield to other tasks for 2 ticks (approx 2ms on ESP32)
    vTaskDelay(2 / portTICK_PERIOD_MS); 
  }
}

void flashLED(void *pvParameters) {
  for (;;) {
    int newState = !digitalRead(LED_BUILTIN);
    //Serial.printf("Flash!\n");
    //Serial.printf("Setting LED_BUILTIN to %s\n", newState == HIGH ? "HIGH" : "LOW");
    digitalWrite(LED_BUILTIN, newState);  // toggle LED state
    vTaskDelay(80 / portTICK_PERIOD_MS);  // On for 500ms
  }
}

// ===== Setup =====
void setup() {

  #if (ARDUINO_USB_CDC_ON_BOOT > 0)
    Serial.begin();
    delay(SERIAL_BEGIN_DELAY);
  #else
    Serial.begin(SERIAL_BAUD);
    delay(SERIAL_BEGIN_DELAY);
    Serial.println();
  #endif

  Serial.println("\n\n========================================");
  Serial.println("ESP32 WiFi Access Point + Web Server");
  Serial.println("========================================");
  Serial.printf("Board: XIAO ESP32C6\n");
  
  // Initialize onboard LED (active-low)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Start off
  
   preferences.begin(PREF_NAMESPACE, true);  // Read-only mode for initial check

  // Mount LittleFS storage
  if (!LittleFS.begin()) {
    Serial.println("✗ Failed to mount LittleFS");
  } else {
    Serial.println("✓ LittleFS mounted");
  }
  
  // Try to connect to saved WiFi credentials first
  bool connectedToSTA = tryConnectWithSavedCredentials();
  
  if (connectedToSTA) {
    Serial.println("\n✓ Successfully connected to saved WiFi network!");
    Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
    isInSTAMode = true;
    preferences.end();
    
    // Setup Web Server in STA mode (optional, for additional functionality)
    setupWebServer();
    server.begin();

    xTaskCreate(
      webServerTask,    // Function name
      "WebServerTask",  // Name for debugging
      4096,             // Stack size (WebServer.h requires a decent amount)
      NULL,             // Task parameters
      1,                // Priority (lower numbers = lower priority)
      NULL              // Task handle
    );
    
    Serial.println("\n✓ Setup complete!");
    Serial.println("Web server is accessible on the connected network");
    Serial.println("========================================\n");
  } else {
    // Failed to connect to saved network or no credentials - start AP mode
    Serial.println("\nNo saved WiFi or connection failed - Starting in AP mode");
    preferences.end();
    
    // Setup WiFi Access Point
    setupAccessPoint();
    
    // Setup Web Server
    setupWebServer();
    server.begin();

    xTaskCreate(
      webServerTask,    // Function name
      "WebServerTask",  // Name for debugging
      4096,             // Stack size (WebServer.h requires a decent amount)
      NULL,             // Task parameters
      1,                // Priority (lower numbers = lower priority)
      NULL              // Task handle
    );
    
    Serial.println("\n✓ Setup complete!");
    Serial.printf("Connect to WiFi: %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.println("Open browser: http://192.168.4.1");
    Serial.println("========================================\n");
  }

}

// ===== Main Loop =====
void loop() {
  //server.handleClient();
  //delay(10);
}

// ===== Setup Access Point =====
void setupAccessPoint() {
  Serial.println("\nSetting up Access Point...");
  
  // Stop any existing WiFi connections
  WiFi.mode(WIFI_AP);
  
  // Configure and start the access point
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
  
  // Get and display AP IP address
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("Access Point IP: ");
  Serial.println(IP);
  
  // Display MAC address
  Serial.printf("Access Point MAC: %s\n", WiFi.softAPmacAddress().c_str());
}

// ===== Setup Web Server Routes =====
void setupWebServer() {
  Serial.println("Setting up Web Server...");
  
  // Root page - displays WiFi network list
  server.on("/", HTTP_GET, handleRoot);
  
  // Style file
  server.on("/style.css", HTTP_GET, handleStyle);
  
  // Script file
  server.on("/script.js", HTTP_GET, handleScript);
  
  // Device name endpoints
  server.on("/api/device-name", HTTP_GET, handleGetDeviceName);
  server.on("/api/device-name", HTTP_POST, handleSetDeviceName);
  
  // API endpoint to get WiFi networks
  server.on("/api/networks", HTTP_GET, handleScan);
  
  // API endpoint to connect to a network
  server.on("/api/connect", HTTP_POST, handleConnect);
  
  // API endpoint to reset WiFi preferences
  server.on("/api/reset", HTTP_POST, handleReset);
  
  // Handle 404 errors
  server.onNotFound(handleNotFound);
  
  Serial.println("✓ Web Server routes configured");
}

// ===== WiFi Preferences Management =====

bool tryConnectWithSavedCredentials() {
  Serial.println("\nChecking for saved WiFi credentials...");
  
  // Get SSID and password from preferences
  String savedSSID = preferences.getString(PREF_SSID_KEY, "");
  String savedPassword = preferences.getString(PREF_PASSWORD_KEY, "");
  
  if (savedSSID.length() == 0) {
    Serial.println("  No saved WiFi credentials found");
    return false;
  }
  
  Serial.printf("  Found saved SSID: %s\n", savedSSID.c_str());
  Serial.println("  Attempting to connect...");
  
  // Switch to STA mode
  WiFi.mode(WIFI_STA);
  
  // Connect with saved credentials
  if (savedPassword.length() > 0) {
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
  } else {
    WiFi.begin(savedSSID.c_str());
  }
  
  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Check if connection was successful
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  ✓ Successfully connected to saved WiFi!");
    return true;
  } else {
    Serial.println("  ✗ Failed to connect to saved WiFi - will start AP mode");
    return false;
  }
}

void clearSavedCredentials() {
  preferences.begin(PREF_NAMESPACE, false);  // Read-write mode
  preferences.remove(PREF_SSID_KEY);
  preferences.remove(PREF_PASSWORD_KEY);
  preferences.end();
  Serial.println("✓ Saved WiFi credentials cleared");
}

void saveSavedCredentials(const String& ssid, const String& password) {
  preferences.begin(PREF_NAMESPACE, false);  // Read-write mode
  preferences.putString(PREF_SSID_KEY, ssid);
  preferences.putString(PREF_PASSWORD_KEY, password);
  preferences.end();
  Serial.printf("✓ Saved WiFi credentials: SSID=%s\n", ssid.c_str());
}

// ===== HTTP Request Handlers =====

void handleRoot() {
  Serial.println("Client requested root page");
  
  File file = LittleFS.open("/index.htm", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open /index.htm");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleScript() {
  Serial.println("Client requested script.js");
  
  File file = LittleFS.open("/script.js", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open /script.js");
    return;
  }
  server.streamFile(file, "application/javascript");
  file.close();
}

void handleStyle() {
  Serial.println("Client requested style.css");
  
  File file = LittleFS.open("/style.css", "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open /style.css");
    return;
  }
  server.streamFile(file, "text/css");
  file.close();
}

void handleScan() {
  Serial.println("Client requested WiFi scan");
  
  xTaskCreate(
    flashLED,      // Function name
    "FlashLED",    // Name for debugging
    256,           // Stack size
    NULL,          // Task parameters
    1,             // Priority
    &flashTask     // Task handle
  );


  // Perform WiFi scan
  int n = WiFi.scanNetworks();
  numNetworks = n;
  
  
  // Build JSON response
  String json = "[";
  
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
    
    // Get encryption type
    String encryption;
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN:            encryption = "Open"; break;
      case WIFI_AUTH_WEP:             encryption = "WEP"; break;
      case WIFI_AUTH_WPA_PSK:         encryption = "WPA"; break;
      case WIFI_AUTH_WPA2_PSK:        encryption = "WPA2"; break;
      case WIFI_AUTH_WPA_WPA2_PSK:    encryption = "WPA+WPA2"; break;
      case WIFI_AUTH_WPA2_ENTERPRISE: encryption = "WPA2-EAP"; break;
      case WIFI_AUTH_WPA3_PSK:        encryption = "WPA3"; break;
      case WIFI_AUTH_WPA2_WPA3_PSK:   encryption = "WPA2+WPA3"; break;
      case WIFI_AUTH_WAPI_PSK:        encryption = "WAPI"; break;
      default:                        encryption = "Unknown";
    }
    json += "\"encryption\":\"" + encryption + "\"";
    json += "}";
  }
  
  json += "]";
  
  // Delete scan result to free memory
  WiFi.scanDelete();
  
  server.send(200, "application/json", json);
  
  Serial.printf("  Found %d networks\n", n);

  if (flashTask != NULL) {
    vTaskDelete(flashTask);
    flashTask = NULL;
  }
  digitalWrite(LED_BUILTIN, HIGH);  // Ensure LED is off after flashing
}

void handleConnect() {
  Serial.println("Client requested WiFi connection");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  String body = server.arg("plain");
  Serial.printf("  Request body: %s\n", body.c_str());
  
  // Simple JSON parsing for SSID
  int ssidStartIdx = body.indexOf("\"ssid\":\"") + 8;
  int ssidEndIdx = body.indexOf("\"", ssidStartIdx);
  
  // Simple JSON parsing for password
  int pwdStartIdx = body.indexOf("\"password\":\"") + 12;
  int pwdEndIdx = body.indexOf("\"", pwdStartIdx);
  
  if (ssidStartIdx > 7 && ssidEndIdx > ssidStartIdx) {
    String ssid = body.substring(ssidStartIdx, ssidEndIdx);
    String password = "";
    
    // Extract password if present and not empty
    if (pwdStartIdx > 11 && pwdEndIdx > pwdStartIdx) {
      password = body.substring(pwdStartIdx, pwdEndIdx);
    }
    
    Serial.printf("  Connecting to: %s\n", ssid.c_str());
    if (password.length() > 0) {
      Serial.printf("  Using password: %s\n", password.c_str());
    } else {
      Serial.println("  No password provided (open network)");
    }
    
    // Send success response
    server.send(200, "application/json", "{\"status\":\"connecting\",\"ssid\":\"" + ssid + "\"}");
    
    // Switch to STA mode and connect to the selected network
    WiFi.mode(WIFI_STA);
    if (password.length() > 0) {
      WiFi.begin(ssid.c_str(), password.c_str());
    } else {
      WiFi.begin(ssid.c_str());
    }
    
    // Optionally wait a bit to see the connection status
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("✓ Connected to WiFi: %s\n", ssid.c_str());
      Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
      isInSTAMode = true;
      // Save credentials to preferences for next startup
      saveSavedCredentials(ssid, password);
    } else {
      Serial.printf("✗ Failed to connect to: %s\n", ssid.c_str());
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid SSID\"}");
  }
}

void handleReset() {
  Serial.println("Client requested WiFi preferences reset");
  
  // Clear saved credentials
  clearSavedCredentials();
  
  // Send success response
  server.send(200, "application/json", "{\"status\":\"reset\",\"message\":\"WiFi preferences cleared. Device will restart in AP mode.\"}");
  
  // Disconnect from current WiFi and restart in AP mode after a short delay
  Serial.println("WiFi preferences reset - restarting in AP mode");
  
  // Disconnect from current network
  WiFi.disconnect();
  
  // Reset the STA mode flag
  isInSTAMode = false;
  
  // Restart the Access Point
  setupAccessPoint();
  
  Serial.println("✓ Access Point restarted");
}

void handleNotFound() {
  Serial.printf("Not found: %s %s\n", server.method() == HTTP_GET ? "GET" : "POST", server.uri().c_str());
  
  server.send(404, "text/plain", "404: Not Found");
}

void handleGetDeviceName() {
  Serial.println("Client requested device name");
  
  preferences.begin(PREF_NAMESPACE, true);  // Read-only mode
  String deviceName = preferences.getString(PREF_DEVICE_NAME_KEY, "");
  preferences.end();
  
  // If no device name is set, generate one from MAC address
  if (deviceName.length() == 0) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "IOT-Ratter-%02X%02X", mac[4], mac[5]);
    deviceName = String(buffer);
  }
  
  String response = "{\"name\":\"" + deviceName + "\"}";
  server.send(200, "application/json", response);
}

void handleSetDeviceName() {
  Serial.println("Client requested to set device name");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  String body = server.arg("plain");
  Serial.printf("  Request body: %s\n", body.c_str());
  
  // Simple JSON parsing for device name
  int nameStartIdx = body.indexOf("\"name\":\"") + 8;
  int nameEndIdx = body.indexOf("\"", nameStartIdx);
  
  if (nameStartIdx > 7 && nameEndIdx > nameStartIdx) {
    String deviceName = body.substring(nameStartIdx, nameEndIdx);
    
    // Limit device name to 32 characters
    if (deviceName.length() > 32) {
      deviceName = deviceName.substring(0, 32);
    }
    
    // Save to preferences
    preferences.begin(PREF_NAMESPACE, false);  // Read-write mode
    preferences.putString(PREF_DEVICE_NAME_KEY, deviceName);
    preferences.end();
    
    Serial.printf("  Device name set to: %s\n", deviceName.c_str());
    server.send(200, "application/json", "{\"status\":\"success\",\"name\":\"" + deviceName + "\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid device name\"}");
  }
}
