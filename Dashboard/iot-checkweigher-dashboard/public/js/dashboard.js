// dashboard.js - Logika client-side untuk dashboard real-time Checkweigher
document.addEventListener('DOMContentLoaded', function () {
    const configEl = document.getElementById('dashboardConfig');
    if (!configEl) return;

    // --- 1. AMBIL KONFIGURASI DAN DATA DARI DOM ---
    const minThreshold = parseFloat(configEl.getAttribute('data-min-threshold')) || 300;
    const maxThreshold = parseFloat(configEl.getAttribute('data-max-threshold')) || 1000;
    const defaultSetpoint = parseFloat(configEl.getAttribute('data-setpoint')) || 60;
    const csrfToken = configEl.getAttribute('data-csrf') || '';

    let initialWeighings = [];
    try {
        const rawW = configEl.getAttribute('data-weighings');
        initialWeighings = rawW ? JSON.parse(rawW) : [];
    } catch (e) {
        console.error('Gagal parse initialWeighings:', e);
        initialWeighings = [];
    }

    let initialConveyorLogs = [];
    try {
        const rawL = configEl.getAttribute('data-logs');
        initialConveyorLogs = rawL ? JSON.parse(rawL) : [];
    } catch (e) {
        console.error('Gagal parse initialConveyorLogs:', e);
        initialConveyorLogs = [];
    }

    // --- 2. SETUP CHART 1: GRAFIK BERAT TERHADAP WAKTU ---
    const canvasWeight = document.getElementById('weightChart');
    let weightChart = null;

    if (canvasWeight) {
        const ctxWeight = canvasWeight.getContext('2d');
        const sortedWeighings = initialWeighings.slice().reverse();
        const weightLabels = sortedWeighings.map(w => w.measured_at ? new Date(w.measured_at).toLocaleTimeString() : '');
        const weightValues = sortedWeighings.map(w => typeof w.weight === 'number' ? w.weight : parseFloat(w.weight) || 0);

        weightChart = new Chart(ctxWeight, {
            type: 'line',
            data: {
                labels: weightLabels,
                datasets: [
                    {
                        label: 'Berat (gram)',
                        data: weightValues,
                        borderColor: '#2563eb',
                        backgroundColor: 'rgba(37, 99, 235, 0.08)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.3,
                        pointRadius: 3
                    },
                    {
                        label: 'Batas Min (' + minThreshold + 'g)',
                        data: weightValues.map(() => minThreshold),
                        borderColor: '#f59e0b',
                        borderWidth: 1.5,
                        borderDash: [4, 4],
                        pointRadius: 0,
                        fill: false
                    },
                    {
                        label: 'Batas Max (' + maxThreshold + 'g)',
                        data: weightValues.map(() => maxThreshold),
                        borderColor: '#ef4444',
                        borderWidth: 1.5,
                        borderDash: [4, 4],
                        pointRadius: 0,
                        fill: false
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: { display: true, text: 'Gram (g)' }
                    }
                }
            }
        });
    }

    // --- 3. SETUP CHART 2: GRAFIK RPM vs SETPOINT & PWM ---
    const canvasRpm = document.getElementById('rpmChart');
    let rpmChart = null;

    if (canvasRpm) {
        const ctxRpm = canvasRpm.getContext('2d');
        const sortedLogs = initialConveyorLogs.slice().reverse();
        const rpmLabels = sortedLogs.map(l => l.recorded_at ? new Date(l.recorded_at).toLocaleTimeString() : '');
        const rpmActual = sortedLogs.map(l => typeof l.actual_rpm === 'number' ? l.actual_rpm : parseFloat(l.actual_rpm) || 0);
        const rpmSetpoint = sortedLogs.map(l => typeof l.setpoint_rpm === 'number' ? l.setpoint_rpm : parseFloat(l.setpoint_rpm) || defaultSetpoint);

        rpmChart = new Chart(ctxRpm, {
            type: 'line',
            data: {
                labels: rpmLabels,
                datasets: [
                    {
                        label: 'Actual RPM',
                        data: rpmActual,
                        borderColor: '#10b981',
                        borderWidth: 2,
                        tension: 0.2,
                        pointRadius: 2
                    },
                    {
                        label: 'Setpoint RPM',
                        data: rpmSetpoint,
                        borderColor: '#f43f5e',
                        borderWidth: 2,
                        borderDash: [5, 5],
                        pointRadius: 0
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    y: {
                        beginAtZero: true,
                        title: { display: true, text: 'RPM' }
                    }
                }
            }
        });
    }

    // --- 4. WEBSOCKET CLIENT UNTUK REAL-TIME TELEMETRI ---
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProtocol}//${window.location.host}/ws`;
    let socket = null;

    function connectWebSocket() {
        socket = new WebSocket(wsUrl);

        socket.onopen = function () {
            const b = document.getElementById('badgeBroker');
            const d = document.getElementById('dotBroker');
            const t = document.getElementById('textBroker');
            if (b && t && d) {
                b.className = 'p-4 rounded-2xl flex items-center justify-center gap-2 font-bold text-white shadow-sm transition-colors bg-blue-700';
                d.className = 'w-3 h-3 rounded-full bg-emerald-400';
                t.innerText = 'OK (Connected)';
            }
        };

        socket.onmessage = function (event) {
            try {
                const message = JSON.parse(event.data);

                // A. Data Penimbangan Baru Masuk
                if (message.type === 'weighing') {
                    const w = message.data;
                    const weightVal = typeof w.weight === 'number' ? w.weight : parseFloat(w.weight) || 0;
                    const speedVal = typeof w.conveyor_speed === 'number' ? w.conveyor_speed : parseFloat(w.conveyor_speed) || 0;

                    const valWeight = document.getElementById('valWeight');
                    const valSpeed = document.getElementById('valSpeed');
                    if (valWeight) valWeight.innerText = weightVal.toFixed(1) + ' g';
                    if (valSpeed) valSpeed.innerText = speedVal.toFixed(1) + ' RPM';

                    const itemCodeEl = document.getElementById('valItemCode');
                    const itemNameEl = document.getElementById('valItemName');
                    if (itemCodeEl && itemNameEl) {
                        if (w.item && w.item.item_code) {
                            itemCodeEl.innerText = w.item.item_code;
                            itemNameEl.innerText = w.item.name || '-';
                        } else if (w.rfid_uid) {
                            itemCodeEl.innerText = w.rfid_uid;
                            itemNameEl.innerText = 'Tag Belum Terdaftar';
                        }
                    }

                    const catEl = document.getElementById('valCategory');
                    const catDescEl = document.getElementById('valCategoryDesc');
                    if (catEl && catDescEl) {
                        catEl.innerText = w.category || '-';

                        if (w.category === 'RINGAN') {
                            catEl.className = 'inline-block px-3 py-1 rounded-full text-xs font-bold uppercase tracking-wider bg-amber-100 text-amber-800';
                            catDescEl.innerText = 'Disortir via Servo';
                            const el = document.getElementById('statRingan');
                            if (el) el.innerText = (parseInt(el.innerText || '0', 10) + 1);
                        } else if (w.category === 'BERAT') {
                            catEl.className = 'inline-block px-3 py-1 rounded-full text-xs font-bold uppercase tracking-wider bg-emerald-100 text-emerald-800';
                            catDescEl.innerText = 'Jalur Lurus';
                            const el = document.getElementById('statBerat');
                            if (el) el.innerText = (parseInt(el.innerText || '0', 10) + 1);
                        } else {
                            catEl.className = 'inline-block px-3 py-1 rounded-full text-xs font-bold uppercase tracking-wider bg-rose-100 text-rose-800';
                            catDescEl.innerText = 'Konveyor Berhenti';
                            const el = document.getElementById('statError');
                            if (el) el.innerText = (parseInt(el.innerText || '0', 10) + 1);
                        }
                    }

                    const totEl = document.getElementById('statTotal');
                    if (totEl) totEl.innerText = (parseInt(totEl.innerText || '0', 10) + 1);

                    // Tambah data ke grafik berat
                    if (weightChart) {
                        const timeStr = new Date().toLocaleTimeString();
                        weightChart.data.labels.push(timeStr);
                        weightChart.data.datasets[0].data.push(weightVal);
                        weightChart.data.datasets[1].data.push(minThreshold);
                        weightChart.data.datasets[2].data.push(maxThreshold);
                        if (weightChart.data.labels.length > 25) {
                            weightChart.data.labels.shift();
                            weightChart.data.datasets[0].data.shift();
                            weightChart.data.datasets[1].data.shift();
                            weightChart.data.datasets[2].data.shift();
                        }
                        weightChart.update('none');
                    }
                }

                // B. Telemetri RPM Baru
                else if (message.type === 'conveyor_rpm') {
                    const l = message.data;
                    const actualRpm = typeof l.actual_rpm === 'number' ? l.actual_rpm : parseFloat(l.actual_rpm) || 0;
                    const setpointRpm = typeof l.setpoint_rpm === 'number' ? l.setpoint_rpm : parseFloat(l.setpoint_rpm) || defaultSetpoint;

                    const valSpeed = document.getElementById('valSpeed');
                    if (valSpeed) valSpeed.innerText = actualRpm.toFixed(1) + ' RPM';

                    if (rpmChart) {
                        const timeStr = new Date().toLocaleTimeString();
                        rpmChart.data.labels.push(timeStr);
                        rpmChart.data.datasets[0].data.push(actualRpm);
                        rpmChart.data.datasets[1].data.push(setpointRpm);
                        if (rpmChart.data.labels.length > 30) {
                            rpmChart.data.labels.shift();
                            rpmChart.data.datasets[0].data.shift();
                            rpmChart.data.datasets[1].data.shift();
                        }
                        rpmChart.update('none');
                    }
                }

                // C. Status Perangkat Baru (Heartbeat)
                else if (message.type === 'device_status') {
                    const devices = Array.isArray(message.data) ? message.data : [message.data];
                    let allOk = true;
                    let espOnline = false;
                    devices.forEach(d => {
                        if (d.status !== 'ONLINE') allOk = false;
                        if (d.device_type === 'loadcell' && d.status === 'ONLINE') espOnline = true;
                    });

                    const bStatus = document.getElementById('badgeStatus');
                    const tStatus = document.getElementById('textStatus');
                    if (bStatus && tStatus) {
                        bStatus.className = 'p-4 rounded-2xl flex items-center justify-center gap-2 font-bold text-white shadow-sm transition-colors ' + (allOk ? 'bg-emerald-600' : 'bg-amber-600');
                        tStatus.innerText = allOk ? 'AKTIF' : 'STANDBY';
                    }

                    const bEsp = document.getElementById('badgeEsp32');
                    const tEsp = document.getElementById('textEsp32');
                    if (bEsp && tEsp) {
                        bEsp.className = 'p-4 rounded-2xl flex items-center justify-center gap-2 font-bold text-white shadow-sm transition-colors ' + (espOnline ? 'bg-blue-600' : 'bg-slate-500');
                        tEsp.innerText = espOnline ? 'Online' : 'Offline';
                    }
                }
            } catch (err) {
                console.error('Error parse WebSocket message:', err);
            }
        };

        socket.onclose = function () {
            const b = document.getElementById('badgeBroker');
            const d = document.getElementById('dotBroker');
            const t = document.getElementById('textBroker');
            if (b && t && d) {
                b.className = 'p-4 rounded-2xl flex items-center justify-center gap-2 font-bold text-white shadow-sm transition-colors bg-rose-600';
                d.className = 'w-3 h-3 rounded-full bg-rose-300';
                t.innerText = 'Disconnected';
            }
            setTimeout(connectWebSocket, 3000);
        };
    }
    connectWebSocket();

    // --- 5. FUNGSI KIRIM PERINTAH (START / STOP / TARE) ---
    window.sendCommand = function (cmd) {
        const feedback = document.getElementById('commandFeedback');
        if (feedback) {
            feedback.innerText = 'Mengirim perintah ' + cmd + '...';
            feedback.className = 'mt-4 text-xs font-semibold text-center text-blue-600 animate-pulse';
        }

        fetch('/api/command', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-Token': csrfToken
            },
            body: JSON.stringify({ command: cmd })
        })
        .then(res => res.json())
        .then(data => {
            if (!feedback) return;
            if (data.success) {
                feedback.innerText = data.message;
                feedback.className = 'mt-4 text-xs font-semibold text-center text-emerald-600';
            } else {
                feedback.innerText = data.error || 'Gagal mengirim perintah.';
                feedback.className = 'mt-4 text-xs font-semibold text-center text-rose-600';
            }
        })
        .catch(err => {
            if (!feedback) return;
            feedback.innerText = 'Error koneksi ke server.';
            feedback.className = 'mt-4 text-xs font-semibold text-center text-rose-600';
        });
    };
});

