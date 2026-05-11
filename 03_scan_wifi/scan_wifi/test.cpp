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

// ===== Configuration =====
const char* AP_SSID = "ESP32-WiFi-Scanner";
const char* AP_PASSWORD = "12345678";
const int AP_CHANNEL = 1;
const int AP_MAX_CONNECTIONS = 4;

// Serial configuration
#define SERIAL_BAUD 115200
#define SERIAL_BEGIN_DELAY 2000

// ===== Global Variables =====
WebServer server(80);
int numNetworks = 0;

// Function prototypes
void setupAccessPoint();
void setupWebServer();
void scanNetworks();
String getNetworkListHTML();
void handleRoot();
void handleScan();
void handleConnect();
void handleNotFound();

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
  
  // Setup WiFi Access Point
  setupAccessPoint();
  
  // Setup Web Server
  setupWebServer();
  server.begin();
  
  Serial.println("\n✓ Setup complete!");
  Serial.printf("Connect to WiFi: %s\n", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.println("Open browser: http://192.168.4.1");
  Serial.println("========================================\n");
}

// ===== Main Loop =====
void loop() {
  server.handleClient();
  delay(10);
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
  
  // API endpoint to get WiFi networks
  server.on("/api/networks", HTTP_GET, handleScan);
  
  // API endpoint to connect to a network
  server.on("/api/connect", HTTP_POST, handleConnect);
  
  // Handle 404 errors
  server.onNotFound(handleNotFound);
  
  Serial.println("✓ Web Server routes configured");
}

// ===== HTTP Request Handlers =====

void handleRoot() {
  Serial.println("Client requested root page");
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Scanner</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    
    .container {
      background: white;
      border-radius: 10px;
      box-shadow: 0 10px 40px rgba(0, 0, 0, 0.3);
      max-width: 500px;
      width: 100%;
      overflow: hidden;
    }
    
    .header {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 30px 20px;
      text-align: center;
    }
    
    .header h1 {
      font-size: 28px;
      margin-bottom: 5px;
    }
    
    .header p {
      font-size: 14px;
      opacity: 0.9;
    }
    
    .content {
      padding: 30px 20px;
    }
    
    .button-group {
      display: flex;
      gap: 10px;
      margin-bottom: 20px;
    }
    
    button {
      flex: 1;
      padding: 12px;
      border: none;
      border-radius: 5px;
      font-size: 16px;
      cursor: pointer;
      font-weight: 600;
      transition: all 0.3s ease;
    }
    
    .scan-btn {
      background: #667eea;
      color: white;
    }
    
    .scan-btn:hover {
      background: #5568d3;
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
    }
    
    .scan-btn:active {
      transform: translateY(0);
    }
    
    .scan-btn:disabled {
      background: #ccc;
      cursor: not-allowed;
      transform: none;
    }
    
    .refresh-btn {
      background: #f0f0f0;
      color: #333;
      border: 2px solid #667eea;
    }
    
    .refresh-btn:hover {
      background: #e8e8e8;
    }
    
    .loading {
      display: none;
      text-align: center;
      padding: 20px;
      color: #667eea;
    }
    
    .spinner {
      border: 3px solid #f3f3f3;
      border-top: 3px solid #667eea;
      border-radius: 50%;
      width: 30px;
      height: 30px;
      animation: spin 1s linear infinite;
      margin: 0 auto 10px;
    }
    
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    
    .network-list {
      display: flex;
      flex-direction: column;
      gap: 10px;
      max-height: 400px;
      overflow-y: auto;
    }
    
    .network-item {
      padding: 15px;
      border: 2px solid #e0e0e0;
      border-radius: 8px;
      cursor: pointer;
      transition: all 0.3s ease;
      background: #f9f9f9;
    }
    
    .network-item:hover {
      border-color: #667eea;
      background: #f0f4ff;
      transform: translateX(5px);
    }
    
    .network-item.selected {
      background: #e8f0ff;
      border-color: #667eea;
      box-shadow: 0 0 10px rgba(102, 126, 234, 0.2);
    }
    
    .network-name {
      font-weight: 600;
      color: #333;
      margin-bottom: 5px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    
    .signal-strength {
      display: inline-block;
      font-size: 12px;
      background: #667eea;
      color: white;
      padding: 2px 8px;
      border-radius: 3px;
    }
    
    .network-info {
      display: flex;
      justify-content: space-between;
      font-size: 12px;
      color: #666;
      gap: 10px;
    }
    
    .info-item {
      flex: 1;
    }
    
    .encryption-badge {
      display: inline-block;
      background: #764ba2;
      color: white;
      padding: 3px 8px;
      border-radius: 3px;
      font-size: 11px;
      font-weight: 600;
    }
    
    .status-message {
      padding: 15px;
      border-radius: 5px;
      margin-bottom: 15px;
      display: none;
    }
    
    .status-message.success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
      display: block;
    }
    
    .status-message.error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
      display: block;
    }
    
    .status-message.info {
      background: #d1ecf1;
      color: #0c5460;
      border: 1px solid #bee5eb;
      display: block;
    }
    
    .empty-state {
      text-align: center;
      padding: 40px 20px;
      color: #999;
    }
    
    .empty-state svg {
      width: 60px;
      height: 60px;
      margin-bottom: 15px;
      opacity: 0.5;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📡 WiFi Scanner</h1>
      <p>Select an available network</p>
    </div>
    
    <div class="content">
      <div id="statusMessage" class="status-message"></div>
      
      <div class="button-group">
        <button class="scan-btn" id="scanBtn" onclick="scanNetworks()">🔍 Scan Networks</button>
        <button class="refresh-btn" onclick="location.reload()">⟳</button>
      </div>
      
      <div class="loading" id="loading">
        <div class="spinner"></div>
        <p>Scanning for networks...</p>
      </div>
      
      <div id="networkListContainer" style="display: none;">
        <div class="network-list" id="networkList"></div>
        <div style="margin-top: 20px;">
          <button class="scan-btn" style="width: 100%;" id="connectBtn" onclick="connectToSelected()" disabled>
            Connect to Selected Network
          </button>
        </div>
      </div>
      
      <div class="empty-state" id="emptyState">
        <p>Click "Scan Networks" to find available WiFi networks</p>
      </div>
    </div>
  </div>

  <script>
    let selectedNetwork = null;
    let allNetworks = [];
    
    async function scanNetworks() {
      const scanBtn = document.getElementById('scanBtn');
      const loading = document.getElementById('loading');
      const container = document.getElementById('networkListContainer');
      const emptyState = document.getElementById('emptyState');
      const statusMsg = document.getElementById('statusMessage');
      
      scanBtn.disabled = true;
      loading.style.display = 'block';
      container.style.display = 'none';
      emptyState.style.display = 'none';
      statusMsg.className = 'status-message';
      
      try {
        const response = await fetch('/api/networks');
        if (!response.ok) throw new Error('Failed to fetch networks');
        
        allNetworks = await response.json();
        
        if (allNetworks.length === 0) {
          statusMsg.textContent = '⚠️ No networks found. Try scanning again.';
          statusMsg.className = 'status-message info';
          emptyState.style.display = 'block';
        } else {
          displayNetworks(allNetworks);
          container.style.display = 'block';
        }
      } catch (error) {
        console.error('Scan error:', error);
        statusMsg.textContent = '❌ Error scanning networks: ' + error.message;
        statusMsg.className = 'status-message error';
      } finally {
        loading.style.display = 'none';
        scanBtn.disabled = false;
      }
    }
    
    function displayNetworks(networks) {
      const listContainer = document.getElementById('networkList');
      listContainer.innerHTML = '';
      
      networks.forEach((network, index) => {
        const item = document.createElement('div');
        item.className = 'network-item';
        item.onclick = () => selectNetwork(index, network);
        
        const signalStrength = getSignalBars(network.rssi);
        const encryption = network.encryption || 'Unknown';
        
        item.innerHTML = `
          <div class="network-name">
            📶 ${escapeHtml(network.ssid)}
            <span class="signal-strength">${signalStrength}</span>
          </div>
          <div class="network-info">
            <div class="info-item">
              <strong>Channel:</strong> ${network.channel}
            </div>
            <div class="info-item">
              <span class="encryption-badge">${encryption}</span>
            </div>
            <div class="info-item" style="text-align: right;">
              <strong>RSSI:</strong> ${network.rssi} dBm
            </div>
          </div>
        `;
        
        listContainer.appendChild(item);
      });
    }
    
    function selectNetwork(index, network) {
      // Remove previous selection
      document.querySelectorAll('.network-item').forEach(item => {
        item.classList.remove('selected');
      });
      
      // Select new network
      event.currentTarget.classList.add('selected');
      selectedNetwork = network;
      document.getElementById('connectBtn').disabled = false;
    }
    
    function getSignalBars(rssi) {
      if (rssi >= -50) return '⚡ Excellent';
      if (rssi >= -70) return '▓▓▓ Good';
      if (rssi >= -80) return '▓▓░ Fair';
      return '▓░░ Weak';
    }
    
    function escapeHtml(text) {
      const div = document.createElement('div');
      div.textContent = text;
      return div.innerHTML;
    }
    
    async function connectToSelected() {
      if (!selectedNetwork) {
        alert('Please select a network first');
        return;
      }
      
      const connectBtn = document.getElementById('connectBtn');
      const statusMsg = document.getElementById('statusMessage');
      
      connectBtn.disabled = true;
      statusMsg.textContent = '⏳ Attempting to connect...';
      statusMsg.className = 'status-message info';
      
      try {
        const response = await fetch('/api/connect', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            ssid: selectedNetwork.ssid
          })
        });
        
        if (response.ok) {
          statusMsg.textContent = '✅ Connection request sent! The ESP32 will now connect to: ' + selectedNetwork.ssid;
          statusMsg.className = 'status-message success';
          setTimeout(() => {
            statusMsg.textContent = '📡 Connection in progress... You may lose connection to the access point.';
            statusMsg.className = 'status-message info';
          }, 2000);
        } else {
          throw new Error('Connection failed');
        }
      } catch (error) {
        console.error('Connection error:', error);
        statusMsg.textContent = '❌ Connection error: ' + error.message;
        statusMsg.className = 'status-message error';
        connectBtn.disabled = false;
      }
    }
    
    // Auto-scan on page load
    window.onload = () => {
      scanNetworks();
    };
  </script>
</body>
</html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void handleScan() {
  Serial.println("Client requested WiFi scan");
  
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
}

void handleConnect() {
  Serial.println("Client requested WiFi connection");
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    return;
  }
  
  String body = server.arg("plain");
  Serial.printf("  Request body: %s\n", body.c_str());
  
  // Simple JSON parsing (could use ArduinoJson library for production)
  int startIdx = body.indexOf("\"ssid\":\"") + 8;
  int endIdx = body.indexOf("\"", startIdx);
  
  if (startIdx > 7 && endIdx > startIdx) {
    String ssid = body.substring(startIdx, endIdx);
    Serial.printf("  Connecting to: %s\n", ssid.c_str());
    
    // Send success response
    server.send(200, "application/json", "{\"status\":\"connecting\",\"ssid\":\"" + ssid + "\"}");
    
    // Switch to STA mode and connect to the selected network
    // Note: You may need to add password handling depending on your requirements
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str());
    
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
    } else {
      Serial.printf("✗ Failed to connect to: %s\n", ssid.c_str());
    }
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid SSID\"}");
  }
}

void handleNotFound() {
  Serial.printf("Not found: %s %s\n", server.method() == HTTP_GET ? "GET" : "POST", server.uri().c_str());
  server.send(404, "text/plain", "404: Not Found");
}
