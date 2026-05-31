let selectedNetwork = null;
let allNetworks = [];

async function getDeviceName() {
  try {
    const response = await fetch('/api/device-name');
    if (!response.ok) throw new Error('Failed to fetch device name');
    const data = await response.json();
    document.getElementById('deviceNameText').textContent = data.name;
    document.getElementById('deviceNameInput').value = data.name;
  } catch (error) {
    console.error('Error getting device name:', error);
    document.getElementById('deviceNameText').textContent = 'Unknown Device';
  }
}

function toggleEditDeviceName() {
  const editContainer = document.getElementById('deviceNameEditContainer');
  if (editContainer.style.display === 'none') {
    editContainer.style.display = 'block';
    document.getElementById('deviceNameInput').focus();
  } else {
    editContainer.style.display = 'none';
  }
}

async function saveDeviceName() {
  const newName = document.getElementById('deviceNameInput').value.trim();
  if (!newName) {
    alert('Device name cannot be empty');
    return;
  }
  
  try {
    const response = await fetch('/api/device-name', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ name: newName })
    });
    
    if (response.ok) {
      const data = await response.json();
      document.getElementById('deviceNameText').textContent = data.name;
      toggleEditDeviceName();
    } else {
      throw new Error('Failed to save device name');
    }
  } catch (error) {
    console.error('Error saving device name:', error);
    alert('Failed to save device name: ' + error.message);
  }
}

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
    item.onclick = (event) => selectNetwork(event, network);
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
function selectNetwork(event, network) {
  document.querySelectorAll('.network-item').forEach(item => {
    item.classList.remove('selected');
  });
  event.currentTarget.classList.add('selected');
  selectedNetwork = network;
  document.getElementById('connectBtn').disabled = false;
  const passwordContainer = document.getElementById('passwordContainer');
  const passwordInput = document.getElementById('passwordInput');
  if (network.encryption && network.encryption !== 'Open') {
    passwordContainer.style.display = 'block';
    passwordInput.focus();
  } else {
    passwordContainer.style.display = 'none';
    passwordInput.value = '';
  }
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
  const passwordInput = document.getElementById('passwordInput');
  const password = passwordInput.value;
  if (selectedNetwork.encryption && selectedNetwork.encryption !== 'Open' && !password) {
    alert('Password is required for this network');
    passwordInput.focus();
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
        ssid: selectedNetwork.ssid,
        password: password || ''
      })
    });
    if (response.ok) {
      statusMsg.textContent = '✅ Connection request sent! The ESP32 will now connect to: ' + selectedNetwork.ssid;
      statusMsg.className = 'status-message success';
      setTimeout(() => {
        statusMsg.textContent = '📡 Connection in progress... You may lose connection to the access point.';
        statusMsg.className = 'status-message info';
        setTimeout(() => {
          location.reload();
        }, 3000);
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
async function resetPreferences() {
  if (!confirm('Are you sure you want to reset WiFi settings? This will disconnect from the current network and restart in AP mode.')) {
    return;
  }
  const statusMsg = document.getElementById('statusMessage');
  statusMsg.textContent = '🔄 Resetting WiFi preferences...';
  statusMsg.className = 'status-message info';
  try {
    const response = await fetch('/api/reset', {
      method: 'POST'
    });
    if (response.ok) {
      statusMsg.textContent = '✅ WiFi preferences reset! Restarting in AP mode...';
      statusMsg.className = 'status-message success';
      setTimeout(() => {
        location.reload();
      }, 3000);
    } else {
      throw new Error('Reset failed');
    }
  } catch (error) {
    console.error('Reset error:', error);
    statusMsg.textContent = '❌ Reset error: ' + error.message;
    statusMsg.className = 'status-message error';
  }
}

async function publishToMqtt() {
  const statusMsg = document.getElementById('statusMessage');
  statusMsg.textContent = '📡 Publishing device ID and time to MQTT...';
  statusMsg.className = 'status-message info';

  try {
    const response = await fetch('/api/publish', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({})
    });
    if (response.ok) {
      statusMsg.textContent = '✅ Published device ID and time to MQTT successfully.';
      statusMsg.className = 'status-message success';
    } else {
      const errorData = await response.json().catch(() => null);
      const errorMessage = errorData && errorData.error ? errorData.error : 'Publish failed';
      throw new Error(errorMessage);
    }
  } catch (error) {
    console.error('MQTT publish error:', error);
    statusMsg.textContent = '❌ MQTT publish error: ' + error.message;
    statusMsg.className = 'status-message error';
  }
}

async function enterDeepSleep() {
  if (!confirm('Put the device into deep sleep for 30 seconds?')) {
    return;
  }

  const statusMsg = document.getElementById('statusMessage');
  statusMsg.textContent = '😴 Entering deep sleep...';
  statusMsg.className = 'status-message info';

  try {
    const response = await fetch('/api/sleep', {
      method: 'POST'
    });
    if (response.ok) {
      statusMsg.textContent = '✅ Device is going to sleep now. It will wake in 30 seconds.';
      statusMsg.className = 'status-message success';
    } else {
      throw new Error('Sleep request failed');
    }
  } catch (error) {
    console.error('Sleep error:', error);
    statusMsg.textContent = '❌ Sleep error: ' + error.message;
    statusMsg.className = 'status-message error';
  }
}

window.onload = () => {
  getDeviceName();
  scanNetworks();
};