// Cấu hình MQTT
const mqttConfig = {
    broker: 'test.mosquitto.org',
    port: 8080,  // WebSocket port không bảo mật
    clientId: 'web_client_' + Math.random().toString(16).substr(2, 8),
    topic: 'ducan_esp32/test/sensors',  // Topic nhận dữ liệu
    topicAuto: 'ducan_esp32/test/auto',    // Topic cho nút Auto
    topicManual: 'ducan_esp32/test/manual', // Topic cho nút Manual
    topicFan: 'ducan_esp32/test/fan',       // Topic cho nút Fan
    topicBuzzer: 'ducan_esp32/test/buzzer', // Topic cho nút Buzzer
    topicPump: 'ducan_esp32/test/pump',      // Topic cho nút Pump
    topicSleep: 'ducan_esp32/test/sleep'    // Topic cho chế độ ngủ
};

let client;
let isConnected = false;

// Biến lưu trữ dữ liệu cho biểu đồ
const maxDataPoints = 20;
let temperatureData = [];
let gasData = [];
let temperatureChart;
let gasChart;

// Thêm biến để theo dõi trạng thái điều khiển
let isManualControl = false;
let lastManualState = {};
let lastUpdateTime = 0;

// Biến lưu trạng thái ngủ
let isSleeping = false;

// Khởi tạo
document.addEventListener('DOMContentLoaded', () => {
    console.log('Khởi tạo dashboard...');
    initCharts();
    initButtons();
    createSleepInput();
    connectMQTT();
    updateConnectionStatus('Đang kết nối...');
});

// Khởi tạo biểu đồ
function initCharts() {
    const commonOptions = {
        responsive: true,
        animation: false,
        scales: {
            x: {
                type: 'time',
                time: {
                    unit: 'second',
                    displayFormats: {
                        second: 'HH:mm:ss'
                    }
                }
            },
            y: {
                beginAtZero: true,
                suggestedMax: 150
            }
        }
    };

    temperatureChart = new Chart(document.getElementById('temperatureChart'), {
        type: 'line',
        data: {
            datasets: [{
                label: 'Nhiệt độ (°C)',
                borderColor: 'rgb(255, 99, 132)',
                data: []
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    ...commonOptions.scales.y,
                    suggestedMax: 150
                }
            }
        }
    });

    gasChart = new Chart(document.getElementById('gasChart'), {
        type: 'line',
        data: {
            datasets: [{
                label: 'Nồng độ khí (ppm)',
                borderColor: 'rgb(54, 162, 235)',
                data: []
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    ...commonOptions.scales.y,
                    suggestedMax: 150
                }
            }
        }
    });
}

// Khởi tạo các nút điều khiển
function initButtons() {
    // Mode buttons
    const autoBtn = document.getElementById('btnAuto');
    const manualBtn = document.getElementById('btnManual');
    
    if (autoBtn && manualBtn) {
        // Xóa event listener cũ nếu có
        autoBtn.removeEventListener('click', handleAutoClick);
        manualBtn.removeEventListener('click', handleManualClick);
        
        // Thêm event listener mới
        autoBtn.addEventListener('click', handleAutoClick);
        manualBtn.addEventListener('click', handleManualClick);
        
        console.log('Đã gắn event listener cho nút Auto/Manual');
    } else {
        console.error('Không tìm thấy nút Auto/Manual');
    }
    
    // Device buttons
    const devices = ['fan', 'pump', 'buzzer'];
    devices.forEach(device => {
        const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
        if (button) {
            // Xóa event listener cũ nếu có
            button.removeEventListener('click', () => handleDeviceClick(device));
            // Thêm event listener mới
            button.addEventListener('click', () => handleDeviceClick(device));
            // Thêm class mặc định
            button.classList.add('btn-secondary');
            console.log(`Đã gắn event listener cho nút ${device}`);
        } else {
            console.error(`Không tìm thấy nút ${device}`);
        }
    });

    // Sleep mode
    const sleepBtn = document.getElementById('btnSleep');
    if (sleepBtn) {
        sleepBtn.removeEventListener('click', handleSleepClick);
        sleepBtn.addEventListener('click', handleSleepClick);
        console.log('Đã gắn event listener cho nút Sleep');
    } else {
        console.error('Không tìm thấy nút Sleep');
    }
}

// Kết nối MQTT
function connectMQTT() {
    console.log('Đang kết nối MQTT...');
    console.log('Broker:', mqttConfig.broker);
    console.log('Port:', mqttConfig.port);
    console.log('Topic:', mqttConfig.topic);
    
    client = new Paho.MQTT.Client(mqttConfig.broker, mqttConfig.port, mqttConfig.clientId);
    
    client.onConnectionLost = onConnectionLost;
    client.onMessageArrived = onMessageArrived;
    
    const options = {
        onSuccess: onConnect,
        onFailure: onFailure,
        useSSL: false,  // Không sử dụng SSL cho port 8080
        timeout: 3,
        keepAliveInterval: 60,
        cleanSession: true
    };
    
    client.connect(options);
}

// Cập nhật trạng thái kết nối
function updateConnectionStatus(status, isError = false) {
    const statusElement = document.createElement('div');
    statusElement.className = `alert alert-${isError ? 'danger' : 'info'} alert-dismissible fade show`;
    statusElement.innerHTML = `
        ${status}
        <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
    `;
    document.querySelector('.dashboard-content').insertBefore(statusElement, document.querySelector('.dashboard-content').firstChild);
}

// Xử lý kết nối thành công
function onConnect() {
    console.log('Đã kết nối MQTT broker');
    isConnected = true;
    updateConnectionStatus('Đã kết nối thành công đến MQTT broker');
    client.subscribe(mqttConfig.topic);
    console.log('Đã đăng ký topic:', mqttConfig.topic);
}

// Xử lý kết nối thất bại
function onFailure(error) {
    console.error('Lỗi kết nối MQTT:', error);
    console.error('Mã lỗi:', error.errorCode);
    console.error('Thông báo lỗi:', error.errorMessage);
    isConnected = false;
    updateConnectionStatus('Lỗi kết nối MQTT: ' + error.errorMessage, true);
    setTimeout(connectMQTT, 5000);
}

// Xử lý mất kết nối
function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.error('Mất kết nối MQTT:', responseObject.errorMessage);
        isConnected = false;
        updateConnectionStatus('Mất kết nối MQTT: ' + responseObject.errorMessage, true);
        setTimeout(connectMQTT, 5000);
    }
}

// Cập nhật biểu đồ
function updateCharts(temperature, gas) {
    const now = new Date();
    
    if (temperature !== undefined) {
        temperatureData.push({x: now, y: temperature});
        if (temperatureData.length > maxDataPoints) {
            temperatureData.shift();
        }
        temperatureChart.data.datasets[0].data = temperatureData;
        temperatureChart.update('none');
    }
    
    if (gas !== undefined) {
        gasData.push({x: now, y: gas});
        if (gasData.length > maxDataPoints) {
            gasData.shift();
        }
        gasChart.data.datasets[0].data = gasData;
        gasChart.update('none');
    }
}

// Cập nhật trạng thái thiết bị
function updateDeviceStatus(data) {
    // Cập nhật nhiệt độ và khí
    if (data.ds18b20 !== undefined) {
        document.getElementById('temperature').textContent = data.ds18b20.toFixed(1) + '°C';
    }
    if (data.mq2 !== undefined) {
        document.getElementById('gas').textContent = data.mq2.toFixed(1) + ' ppm';
    }

    // Kiểm tra chế độ auto
    const autoBtn = document.getElementById('btnAuto');
    const isAutoMode = autoBtn && autoBtn.classList.contains('active');

    // Nếu đang ở chế độ auto hoặc chưa có điều khiển thủ công
    if (isAutoMode || !isManualControl) {
        const devices = ['fan', 'pump', 'buzzer'];
        devices.forEach(device => {
            const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
            const statusElement = document.getElementById(`${device}Status`);
            
            if (data[device] !== undefined) {
                const isActive = data[device] === 1;
                if (button) {
                    if (isActive) {
                        button.classList.add('active');
                        button.classList.add('btn-success');
                        button.classList.remove('btn-secondary');
                    } else {
                        button.classList.remove('active');
                        button.classList.remove('btn-success');
                        button.classList.add('btn-secondary');
                    }
                }
                if (statusElement) {
                    statusElement.textContent = isActive ? 'BẬT' : 'TẮT';
                }
            }
        });
    }
}

// Gửi lệnh điều khiển
function sendControlCommand(device, state) {
    if (!isConnected) {
        console.error('Chưa kết nối đến MQTT broker');
        return;
    }
    
    const topic = `ducan_esp32/test/${device}`;
    // Gửi trực tiếp giá trị state (1 hoặc 0)
    const message = state.toString();
    const mqttMessage = new Paho.MQTT.Message(message);
    mqttMessage.destinationName = topic;
    client.send(mqttMessage);
    console.log(`Gửi lệnh: ${topic} - ${message}`);
}

// Xử lý tin nhắn MQTT nhận được
function onMessageArrived(message) {
    console.log('Nhận dữ liệu từ MQTT:', message.payloadString);
    try {
        const data = JSON.parse(message.payloadString);
        console.log('Dữ liệu đã parse:', data);
        
        // Cập nhật giá trị cảm biến
        if (data.ds18b20 !== undefined) {
            document.getElementById('temperature').textContent = data.ds18b20.toFixed(1) + '°C';
            updateCharts(data.ds18b20, undefined);
        }
        if (data.mq2 !== undefined) {
            document.getElementById('gas').textContent = data.mq2.toFixed(1) + ' ppm';
            updateCharts(undefined, data.mq2);
        }

        // Cập nhật trạng thái thiết bị
        const devices = ['fan', 'pump', 'buzzer'];
        devices.forEach(device => {
            const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
            const statusElement = document.getElementById(`${device}Status`);
            
            if (data[device] !== undefined) {
                const isActive = data[device] === 1;
                
                // Cập nhật giao diện
                if (button) {
                    if (isActive) {
                        button.classList.add('active', 'btn-success');
                        button.classList.remove('btn-secondary');
                    } else {
                        button.classList.remove('active', 'btn-success');
                        button.classList.add('btn-secondary');
                    }
                }
                
                // Cập nhật trạng thái hiển thị
                if (statusElement) {
                    statusElement.textContent = isActive ? 'BẬT' : 'TẮT';
                }
            }
        });

        // Cập nhật chế độ auto/manual
        if (data.autoMode !== undefined) {
            const autoBtn = document.getElementById('btnAuto');
            const manualBtn = document.getElementById('btnManual');
            
            if (data.autoMode === 1) {
                autoBtn.classList.add('active', 'btn-success');
                autoBtn.classList.remove('btn-secondary');
                manualBtn.classList.remove('active', 'btn-success');
                manualBtn.classList.add('btn-secondary');
                
                // Vô hiệu hóa các nút điều khiển khi ở chế độ auto
                devices.forEach(device => {
                    const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
                    if (button) {
                        button.disabled = true;
                        button.classList.add('disabled');
                    }
                });
            } else {
                autoBtn.classList.remove('active', 'btn-success');
                autoBtn.classList.add('btn-secondary');
                manualBtn.classList.add('active', 'btn-success');
                manualBtn.classList.remove('btn-secondary');
                
                // Kích hoạt các nút điều khiển khi ở chế độ manual
                devices.forEach(device => {
                    const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
                    if (button) {
                        button.disabled = false;
                        button.classList.remove('disabled');
                    }
                });
            }
        }

    } catch (error) {
        console.error('Lỗi khi xử lý dữ liệu MQTT:', error);
    }
}

// Xử lý sự kiện click cho nút Auto
function handleAutoClick() {
    const autoBtn = document.getElementById('btnAuto');
    const manualBtn = document.getElementById('btnManual');
    
    // Kiểm tra nếu đã ở chế độ auto
    if (autoBtn.classList.contains('active')) {
        return;
    }
    
    // Cập nhật giao diện
    autoBtn.classList.add('active', 'btn-success');
    autoBtn.classList.remove('btn-secondary');
    manualBtn.classList.remove('active', 'btn-success');
    manualBtn.classList.add('btn-secondary');
    
    // Vô hiệu hóa các nút điều khiển
    const devices = ['fan', 'pump', 'buzzer'];
    devices.forEach(device => {
        const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
        if (button) {
            button.disabled = true;
            button.classList.add('disabled');
        }
    });
    
    // Gửi lệnh
    sendControlCommand('auto', 1);
}

// Xử lý sự kiện click cho nút Manual
function handleManualClick() {
    const autoBtn = document.getElementById('btnAuto');
    const manualBtn = document.getElementById('btnManual');
    
    // Kiểm tra nếu đã ở chế độ manual
    if (manualBtn.classList.contains('active')) {
        return;
    }
    
    // Cập nhật giao diện
    autoBtn.classList.remove('active', 'btn-success');
    autoBtn.classList.add('btn-secondary');
    manualBtn.classList.add('active', 'btn-success');
    manualBtn.classList.remove('btn-secondary');
    
    // Kích hoạt các nút điều khiển
    const devices = ['fan', 'pump', 'buzzer'];
    devices.forEach(device => {
        const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
        if (button) {
            button.disabled = false;
            button.classList.remove('disabled');
        }
    });
    
    // Gửi lệnh
    sendControlCommand('manual', 1);
}

// Xử lý sự kiện click cho các nút thiết bị
function handleDeviceClick(device) {
    const button = document.getElementById(`btn${device.charAt(0).toUpperCase() + device.slice(1)}`);
    if (!button) {
        console.error(`Không tìm thấy nút ${device}`);
        return;
    }
    
    // Lấy trạng thái hiện tại từ nút
    const isActive = button.classList.contains('active');
    
    // Đảo ngược trạng thái
    const newState = !isActive;
    
    // Cập nhật giao diện ngay lập tức
    if (newState) {
        button.classList.add('active', 'btn-success');
        button.classList.remove('btn-secondary');
    } else {
        button.classList.remove('active', 'btn-success');
        button.classList.add('btn-secondary');
    }
    
    // Cập nhật trạng thái hiển thị ngay lập tức
    const statusElement = document.getElementById(`${device}Status`);
    if (statusElement) {
        statusElement.textContent = newState ? 'BẬT' : 'TẮT';
    }
    
    // Gửi lệnh điều khiển ngay lập tức
    sendControlCommand(device, newState ? 1 : 0);
}

// Xử lý sự kiện click cho nút Sleep
function handleSleepClick() {
    const btnSleep = document.getElementById('btnSleep');
    isSleeping = !isSleeping;

    // Gửi lệnh bật/tắt ngủ đến ESP32
    const message = new Paho.MQTT.Message(JSON.stringify({
        type: 'sleep',
        enable: isSleeping
    }));
    message.destinationName = mqttConfig.topicSleep;
    client.send(message);

    // Cập nhật giao diện
    if (isSleeping) {
        btnSleep.classList.add('btn-success');
        btnSleep.classList.remove('btn-secondary');
        btnSleep.innerHTML = '<i class="fas fa-moon"></i><span class="d-block mt-1">Tắt ngủ</span>';
    } else {
        btnSleep.classList.add('btn-secondary');
        btnSleep.classList.remove('btn-success');
        btnSleep.innerHTML = '<i class="fas fa-moon"></i><span class="d-block mt-1">Bật ngủ</span>';
    }

    console.log('Chế độ ngủ:', isSleeping ? 'BẬT' : 'TẮT');
}

// Hàm thiết lập thời gian ngủ
function setSleepTime() {
    const hour = document.getElementById('sleepHour').value || 0;
    const minute = document.getElementById('sleepMinute').value || 0;
    const second = document.getElementById('sleepSecond').value || 0;

    // Kiểm tra giá trị hợp lệ
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        alert('Vui lòng nhập thời gian hợp lệ!');
        return;
    }

    // Chuyển đổi thời gian thành giây
    const totalSeconds = (parseInt(hour) * 3600) + (parseInt(minute) * 60) + parseInt(second);

    // Gửi thời gian ngủ (tính bằng giây) đến ESP32
    const message = new Paho.MQTT.Message(JSON.stringify({
        type: 'sleep',
        time: totalSeconds
    }));
    message.destinationName = mqttConfig.topicSleep;
    client.send(message);

    console.log('Đã gửi thời gian ngủ:', totalSeconds, 'giây');
}

// Thêm hàm để tạo input thời gian ngủ
function createSleepInput() {
    const sleepContainer = document.getElementById('sleepContainer');
    if (!sleepContainer) {
        console.error('Không tìm thấy container cho chế độ ngủ');
        return;
    }
    
    // Tạo input thời gian
    const timeInput = document.createElement('input');
    timeInput.type = 'number';
    timeInput.id = 'sleepTime';
    timeInput.min = '0';
    timeInput.value = '0';
    timeInput.className = 'form-control';
    timeInput.placeholder = 'Nhập thời gian ngủ (giây)';
    
    // Thêm input vào container
    sleepContainer.appendChild(timeInput);
    console.log('Đã tạo input thời gian ngủ');
} 