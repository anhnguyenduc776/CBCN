// MQTT Configuration
const mqttConfig = {
    host: 'test.mosquitto.org',
    port: 8080,
    topic: 'esp32/test/sensors'
};

// Connect to MQTT broker
const client = mqtt.connect(`ws://${mqttConfig.host}:${mqttConfig.port}`);

// Update sensor value display
function updateSensorValue(elementId, value, unit) {
    const element = document.getElementById(elementId);
    if (element) {
        const valueSpan = element.querySelector('.value');
        const unitSpan = element.querySelector('.unit');
        if (valueSpan) valueSpan.textContent = value;
        if (unitSpan) unitSpan.textContent = unit;
    }
}

// Update status indicator
function updateStatus(elementId, connected) {
    const statusElement = document.getElementById(elementId);
    if (statusElement) {
        const indicator = statusElement.querySelector('.status-indicator');
        const text = statusElement.querySelector('.status-text');
        if (indicator) {
            indicator.className = 'status-indicator ' + (connected ? 'connected' : 'disconnected');
        }
        if (text) {
            text.textContent = connected ? 'Đã kết nối' : 'Mất kết nối';
        }
    }
}

// Handle MQTT connection
client.on('connect', () => {
    console.log('Connected to MQTT broker');
    client.subscribe(mqttConfig.topic);
    updateStatus('temp-status', true);
    updateStatus('gas-status', true);
});

// Handle MQTT messages
client.on('message', (topic, message) => {
    try {
        const data = JSON.parse(message.toString());
        console.log('Received data:', data);

        // Update temperature
        if (data.ds18b20 !== undefined) {
            updateSensorValue('temperature', data.ds18b20.toFixed(1), '°C');
        }

        // Update gas concentration
        if (data.mq2 !== undefined) {
            updateSensorValue('gas', data.mq2.toFixed(1), 'ppm');
        }
    } catch (error) {
        console.error('Error parsing MQTT message:', error);
    }
});

// Handle connection errors
client.on('error', (error) => {
    console.error('MQTT Error:', error);
    updateStatus('temp-status', false);
    updateStatus('gas-status', false);
});

// Handle disconnection
client.on('close', () => {
    console.log('Disconnected from MQTT broker');
    updateStatus('temp-status', false);
    updateStatus('gas-status', false);
}); 