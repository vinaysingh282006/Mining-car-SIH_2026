/**
 * ESP32 MINING FOG-SAFETY ROVER — ADVANCED SENSOR FUSION HUD
 * Interactive 2D Radar Canvas, 3D Attitude Gyro, DEFCON Threat Matrix & Audio Engine
 */

// Global State
let ws = null;
let reconnectTimer = null;
let audioEnabled = true;
let audioCtx = null;
let radarAngle = 0;
let lastObstacleDist = 400;
let activeLearnTarget = null;
let learnPollInterval = null;
let learnTimeoutTimer = null;

// DOM Cache - Header & Status
const threatBar = document.getElementById('threat-bar');
const threatScoreVal = document.getElementById('threat-score-val');
const threatLevelBadge = document.getElementById('threat-level-badge');
const threatDialBox = document.getElementById('threat-dial-box');
const badgeLink = document.getElementById('badge-link');
const badgeLinkText = document.getElementById('badge-link-text');
const badgeArm = document.getElementById('badge-arm');
const badgeArmText = document.getElementById('badge-arm-text');
const btnMasterEstop = document.getElementById('btn-master-estop');
const btnAudioToggle = document.getElementById('btn-audio-toggle');
const audioIcon = document.getElementById('audio-icon');
const btnFullscreen = document.getElementById('btn-fullscreen');
const disconnectOverlay = document.getElementById('disconnect-overlay');
const btnReconnect = document.getElementById('btn-reconnect');

// Hazard Banners
const alarmFlame = document.getElementById('alarm-flame');
const alarmGas = document.getElementById('alarm-gas');
const alarmTilt = document.getElementById('alarm-tilt');
const alarmFailsafe = document.getElementById('alarm-failsafe');

// Radar & Ultrasonic
const radarCanvas = document.getElementById('radar-canvas');
const radarCtx = radarCanvas ? radarCanvas.getContext('2d') : null;
const valDistance = document.getElementById('val-distance');
const valProxZone = document.getElementById('val-prox-zone');
const valBrakeState = document.getElementById('val-brake-state');
const radarTag = document.getElementById('radar-tag');

// 3D Gyro / Inclinometer
const gyroSky = document.getElementById('gyro-sky');
const gyroGround = document.getElementById('gyro-ground');
const gyroHorizon = document.getElementById('gyro-horizon');
const gyroRollRing = document.getElementById('gyro-roll-ring');
const valPitch = document.getElementById('val-pitch');
const valRoll = document.getElementById('val-roll');
const valTilt = document.getElementById('val-tilt');
const valGforce = document.getElementById('val-gforce');
const imuTag = document.getElementById('imu-tag');

// Environment & Climate
const valTemp = document.getElementById('val-temp');
const barTemp = document.getElementById('bar-temp');
const valHumidity = document.getElementById('val-humidity');
const barHumidity = document.getElementById('bar-humidity');
const valDewPoint = document.getElementById('val-dew-point');
const valFogRisk = document.getElementById('val-fog-risk');
const valFlameStatus = document.getElementById('val-flame-status');
const valGasStatus = document.getElementById('val-gas-status');
const climateTag = document.getElementById('climate-tag');

// Motors & Mast Turret
const valMotorL = document.getElementById('val-motor-l');
const barMotorL = document.getElementById('bar-motor-l');
const dirMotorL = document.getElementById('dir-motor-l');
const pctMotorL = document.getElementById('pct-motor-l');
const valMotorR = document.getElementById('val-motor-r');
const barMotorR = document.getElementById('bar-motor-r');
const dirMotorR = document.getElementById('dir-motor-r');
const pctMotorR = document.getElementById('pct-motor-r');
const valMastAngle = document.getElementById('val-mast-angle');
const barMastAngle = document.getElementById('bar-mast-angle');
const cursorMast = document.getElementById('cursor-mast');

// Mission Timeline Log
const timelineLogBox = document.getElementById('timeline-log-box');
const btnClearLog = document.getElementById('btn-clear-log');

// Transmitter & Learn Flow
const channelsContainer = document.getElementById('channels-container');
const learnBanner = document.getElementById('learn-banner');
const learnTitle = document.getElementById('learn-title');
const learnSubtitle = document.getElementById('learn-subtitle');
const btnCancelLearn = document.getElementById('btn-cancel-learn');

const selThrottle = document.getElementById('sel-ch-throttle');
const invThrottle = document.getElementById('inv-throttle');
const selYaw = document.getElementById('sel-ch-yaw');
const invYaw = document.getElementById('inv-yaw');
const selTilt = document.getElementById('sel-ch-tilt');
const invTilt = document.getElementById('inv-tilt');
const selAux = document.getElementById('sel-ch-aux');
const selAuxMode = document.getElementById('sel-aux-mode');
const btnSaveMapping = document.getElementById('btn-save-mapping');
const saveStatus = document.getElementById('save-status');

// Calibration & WiFi Form
const cfgDeadband = document.getElementById('cfg-deadband');
const valCfgDeadband = document.getElementById('val-cfg-deadband');
const cfgTilt = document.getElementById('cfg-tilt');
const valCfgTilt = document.getElementById('val-cfg-tilt');
const cfgSpeed = document.getElementById('cfg-speed');
const valCfgSpeed = document.getElementById('val-cfg-speed');
const staSsid = document.getElementById('sta-ssid');
const staPass = document.getElementById('sta-pass');
const btnSaveWifi = document.getElementById('btn-save-wifi');
const wifiStatus = document.getElementById('wifi-status');

// -----------------------------------------------------------------------------
// INITIALIZATION
// -----------------------------------------------------------------------------
document.addEventListener('DOMContentLoaded', () => {
    initNavigationTabs();
    initChannelMatrix();
    populateChannelOptions();
    initRadarAnimation();
    loadPersistentConfig();
    initEventHandlers();
    connectWebSocket();

    logEvent('SYS', 'Mission Control initialized. Ready for telemetry stream.');
});

// Setup Navigation Tabs
function initNavigationTabs() {
    const tabs = document.querySelectorAll('.nav-tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            playTacticalSound('click');
            tabs.forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.hud-tab-pane').forEach(p => p.classList.remove('active'));

            tab.classList.add('active');
            const targetPane = document.getElementById(tab.getAttribute('data-tab'));
            if (targetPane) targetPane.classList.add('active');
        });
    });
}

// Build 16-Channel Matrix
function initChannelMatrix() {
    channelsContainer.innerHTML = '';
    for (let i = 0; i < 16; i++) {
        const cell = document.createElement('div');
        cell.className = 'ch-cell';
        cell.id = `ch-cell-${i}`;
        cell.innerHTML = `
            <div class="ch-top">
                <span class="ch-name">CH ${i + 1}</span>
                <span class="ch-val" id="ch-num-${i}">992</span>
            </div>
            <div class="ch-meter-track">
                <div class="ch-meter-fill" id="ch-bar-${i}" style="width: 50%;"></div>
            </div>
        `;
        channelsContainer.appendChild(cell);
    }
}

// Populate Channel Select dropdowns
function populateChannelOptions() {
    const selects = [selThrottle, selYaw, selTilt, selAux];
    selects.forEach(sel => {
        sel.innerHTML = '';
        for (let i = 0; i < 16; i++) {
            const opt = document.createElement('option');
            opt.value = i;
            opt.textContent = `Channel ${i + 1} (CRSF)`;
            sel.appendChild(opt);
        }
    });
}

// -----------------------------------------------------------------------------
// INTERACTIVE 2D SONAR RADAR CANVAS
// -----------------------------------------------------------------------------
function initRadarAnimation() {
    if (!radarCtx) return;

    function renderRadar() {
        const w = radarCanvas.width;
        const h = radarCanvas.height;
        const cx = w / 2;
        const cy = h / 2;
        const maxR = cx - 12;

        radarCtx.clearRect(0, 0, w, h);

        // 1. Draw Concentric Range Rings
        radarCtx.strokeStyle = 'rgba(0, 240, 255, 0.18)';
        radarCtx.lineWidth = 1;
        const rings = [0.25, 0.5, 0.75, 1.0];
        rings.forEach(fraction => {
            radarCtx.beginPath();
            radarCtx.arc(cx, cy, maxR * fraction, 0, Math.PI * 2);
            radarCtx.stroke();
        });

        // 2. Draw Crosshair Axes
        radarCtx.beginPath();
        radarCtx.moveTo(cx, 12);
        radarCtx.lineTo(cx, h - 12);
        radarCtx.moveTo(12, cy);
        radarCtx.lineTo(w - 12, cy);
        radarCtx.stroke();

        // 3. Draw Forward Sector Cone (±35° FOV)
        radarCtx.fillStyle = 'rgba(0, 240, 255, 0.04)';
        radarCtx.beginPath();
        radarCtx.moveTo(cx, cy);
        radarCtx.arc(cx, cy, maxR, -Math.PI / 2 - 0.6, -Math.PI / 2 + 0.6);
        radarCtx.closePath();
        radarCtx.fill();

        // 4. Draw Rotating Radar Sweep Beam
        radarAngle += 0.035;
        if (radarAngle > Math.PI * 2) radarAngle = 0;

        const sweepGradient = radarCtx.createRadialGradient(cx, cy, 5, cx, cy, maxR);
        sweepGradient.addColorStop(0, 'rgba(0, 240, 255, 0.4)');
        sweepGradient.addColorStop(1, 'rgba(0, 240, 255, 0.0)');

        radarCtx.save();
        radarCtx.translate(cx, cy);
        radarCtx.rotate(radarAngle);

        radarCtx.fillStyle = 'rgba(0, 240, 255, 0.15)';
        radarCtx.beginPath();
        radarCtx.moveTo(0, 0);
        radarCtx.arc(0, 0, maxR, 0, 0.45);
        radarCtx.closePath();
        radarCtx.fill();

        radarCtx.strokeStyle = '#00f0ff';
        radarCtx.lineWidth = 2;
        radarCtx.beginPath();
        radarCtx.moveTo(0, 0);
        radarCtx.lineTo(maxR, 0);
        radarCtx.stroke();
        radarCtx.restore();

        // 5. Plot Ultrasonic Obstacle Detection Blip
        if (lastObstacleDist > 0 && lastObstacleDist < 390) {
            const normalizedDist = Math.min(lastObstacleDist / 400.0, 1.0);
            const blipR = maxR * normalizedDist;
            const blipY = cy - blipR; // Forward is -Y (0° FWD)

            let blipColor = '#00f0ff';
            if (lastObstacleDist <= 35) blipColor = '#ff2a55';
            else if (lastObstacleDist <= 90) blipColor = '#ffb703';

            // Pulsing glow aura
            radarCtx.fillStyle = blipColor;
            radarCtx.shadowColor = blipColor;
            radarCtx.shadowBlur = 12;
            radarCtx.beginPath();
            radarCtx.arc(cx, blipY, 5, 0, Math.PI * 2);
            radarCtx.fill();
            radarCtx.shadowBlur = 0;

            // Distance ring marker
            radarCtx.strokeStyle = blipColor;
            radarCtx.lineWidth = 1.5;
            radarCtx.beginPath();
            radarCtx.arc(cx, cy, blipR, -Math.PI / 2 - 0.25, -Math.PI / 2 + 0.25);
            radarCtx.stroke();
        }

        // 6. Draw Center Rover Reticle
        radarCtx.fillStyle = '#00f5a0';
        radarCtx.beginPath();
        radarCtx.arc(cx, cy, 4, 0, Math.PI * 2);
        radarCtx.fill();

        requestAnimationFrame(renderRadar);
    }

    renderRadar();
}

// -----------------------------------------------------------------------------
// WEBSOCKET TELEMETRY PIPELINE
// -----------------------------------------------------------------------------
function connectWebSocket() {
    const host = window.location.host || '192.168.4.1';
    const wsUrl = `ws://${host}/ws`;

    if (ws) {
        try { ws.close(); } catch(e) {}
    }

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        disconnectOverlay.classList.add('hidden');
        logEvent('LINK', `Telemetry link established with rover [${host}]`);
        playTacticalSound('connect');
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
    };

    ws.onmessage = (event) => {
        try {
            const payload = JSON.parse(event.data);
            if (payload.type === 'telemetry') {
                processTelemetryData(payload);
            }
        } catch (err) {
            console.error('WS Parse Error:', err);
        }
    };

    ws.onclose = () => {
        handleDisconnect();
        scheduleAutoReconnect();
    };

    ws.onerror = () => {
        handleDisconnect();
    };
}

function handleDisconnect() {
    badgeLink.className = 'stat-badge stat-danger';
    badgeLinkText.textContent = 'LINK SEVERED';
    disconnectOverlay.classList.remove('hidden');
}

function scheduleAutoReconnect() {
    if (!reconnectTimer) {
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            connectWebSocket();
        }, 2500);
    }
}

// -----------------------------------------------------------------------------
// TELEMETRY & SENSOR FUSION RENDERING
// -----------------------------------------------------------------------------
function processTelemetryData(data) {
    const { link, sensors, rover, channels } = data;

    // 1. Link & Failsafe Watchdog
    if (link.failsafe || !link.connected) {
        badgeLink.className = 'stat-badge stat-danger';
        badgeLinkText.textContent = 'FAILSAFE ACTIVE';
        alarmFailsafe.classList.remove('hidden');
    } else {
        badgeLink.className = 'stat-badge stat-green';
        badgeLinkText.textContent = `CRSF 420K (${link.age_ms}ms)`;
        alarmFailsafe.classList.add('hidden');
    }

    // 2. Armed & E-Stop Status
    if (rover.e_stop) {
        badgeArm.className = 'stat-badge stat-danger';
        badgeArmText.textContent = 'E-STOPPED';
        btnMasterEstop.classList.add('active');
    } else if (rover.armed) {
        badgeArm.className = 'stat-badge stat-green';
        badgeArmText.textContent = 'ARMED';
        btnMasterEstop.classList.remove('active');
    } else {
        badgeArm.className = 'stat-badge stat-amber';
        badgeArmText.textContent = 'DISARMED';
        btnMasterEstop.classList.remove('active');
    }

    // 3. DEFCON Multi-Sensor Threat Matrix
    const score = sensors.threat_score || 0;
    threatScoreVal.textContent = score;

    // 264 is full circumference for r=42
    const strokeOffset = 264 - (264 * (score / 100.0));
    threatBar.style.strokeDashoffset = strokeOffset;

    if (score >= 65) {
        threatBar.style.stroke = 'var(--neon-red)';
        threatLevelBadge.className = 'pill-badge pill-critical';
        threatLevelBadge.textContent = 'DEFCON 1: CRITICAL';
        playTacticalSound('alarm');
    } else if (score >= 35) {
        threatBar.style.stroke = 'var(--neon-amber)';
        threatLevelBadge.className = 'pill-badge pill-warning';
        threatLevelBadge.textContent = 'DEFCON 2: WARNING';
    } else if (score >= 12) {
        threatBar.style.stroke = 'var(--neon-cyan)';
        threatLevelBadge.className = 'pill-badge pill-advisory';
        threatLevelBadge.textContent = 'DEFCON 3: ADVISORY';
    } else {
        threatBar.style.stroke = 'var(--neon-green)';
        threatLevelBadge.className = 'pill-badge pill-nominal';
        threatLevelBadge.textContent = 'DEFCON 5: NOMINAL';
    }

    // 4. Direct Hazard Strobe Banners
    if (sensors.flame) {
        alarmFlame.classList.remove('hidden');
        valFlameStatus.textContent = '🔥 DETECTED';
        valFlameStatus.style.color = 'var(--neon-red)';
    } else {
        alarmFlame.classList.add('hidden');
        valFlameStatus.textContent = 'CLEAR (NOMINAL)';
        valFlameStatus.style.color = 'var(--neon-green)';
    }

    if (sensors.gas) {
        alarmGas.classList.remove('hidden');
        valGasStatus.textContent = '☣️ TOXIC SPIKE';
        valGasStatus.style.color = 'var(--neon-red)';
    } else {
        alarmGas.classList.add('hidden');
        valGasStatus.textContent = 'CLEAN AIR';
        valGasStatus.style.color = 'var(--neon-green)';
    }

    if (sensors.tilt_hazard) {
        alarmTilt.classList.remove('hidden');
    } else {
        alarmTilt.classList.add('hidden');
    }

    // 5. Ultrasonic Proximity & Radar
    lastObstacleDist = sensors.distance;
    valDistance.textContent = sensors.distance < 400 ? sensors.distance.toFixed(1) : '400+';

    if (sensors.prox_zone === 2 || sensors.distance <= 35) {
        valProxZone.textContent = 'CRITICAL (<35cm)';
        valProxZone.style.color = 'var(--neon-red)';
        valBrakeState.textContent = 'BRAKE ENGAGED';
        valBrakeState.style.color = 'var(--neon-red)';
        radarTag.className = 'status-pill pill-red';
        radarTag.textContent = 'COLLISION HAZARD';
    } else if (sensors.prox_zone === 1 || sensors.distance <= 90) {
        valProxZone.textContent = 'CAUTION (<90cm)';
        valProxZone.style.color = 'var(--neon-amber)';
        valBrakeState.textContent = 'MONITORING';
        valBrakeState.style.color = 'var(--neon-amber)';
        radarTag.className = 'status-pill pill-amber';
        radarTag.textContent = 'PROXIMITY WARNING';
    } else {
        valProxZone.textContent = 'CLEAR (>100cm)';
        valProxZone.style.color = 'var(--neon-green)';
        valBrakeState.textContent = 'STANDBY';
        valBrakeState.style.color = 'var(--text-secondary)';
        radarTag.className = 'status-pill pill-cyan';
        radarTag.textContent = 'ACTIVE SWEEP';
    }

    // 6. 3D Gyro Attitude & Rollover
    valPitch.textContent = `${sensors.pitch >= 0 ? '+' : ''}${sensors.pitch.toFixed(1)}°`;
    valRoll.textContent = `${sensors.roll >= 0 ? '+' : ''}${sensors.roll.toFixed(1)}°`;
    valTilt.textContent = `${sensors.tilt.toFixed(1)}°`;
    valGforce.textContent = `${(sensors.g_force || 1.0).toFixed(2)} G`;

    // Pitch translates Y (-40px to +40px), Roll rotates
    const pitchOffset = Math.max(Math.min(sensors.pitch * 1.5, 45), -45);
    gyroHorizon.style.transform = `translateY(${pitchOffset}px) rotate(${-sensors.roll}deg)`;
    gyroRollRing.style.transform = `rotate(${-sensors.roll}deg)`;

    if (sensors.tilt_hazard) {
        imuTag.className = 'status-pill pill-red';
        imuTag.textContent = 'ROLLOVER DANGER';
    } else if (sensors.tilt > 20) {
        imuTag.className = 'status-pill pill-amber';
        imuTag.textContent = 'CAUTION TILT';
    } else {
        imuTag.className = 'status-pill pill-cyan';
        imuTag.textContent = 'MPU6050 LEVEL';
    }

    // 7. Subterranean Microclimate Fusion
    if (sensors.dht_ok) {
        valTemp.textContent = sensors.temp.toFixed(1);
        barTemp.style.width = `${Math.min(Math.max((sensors.temp + 10) / 70 * 100, 0), 100)}%`;

        valHumidity.textContent = sensors.humidity.toFixed(0);
        barHumidity.style.width = `${Math.min(Math.max(sensors.humidity, 0), 100)}%`;

        valDewPoint.textContent = `${(sensors.dew_point || (sensors.temp - (100 - sensors.humidity)/5)).toFixed(1)} °C`;

        const fogRisk = sensors.fog_risk !== undefined ? sensors.fogRisk : (sensors.humidity >= 85 ? 2 : (sensors.humidity >= 70 ? 1 : 0));
        if (fogRisk === 2) {
            valFogRisk.textContent = 'DENSE CONDENSATION';
            valFogRisk.style.color = 'var(--neon-red)';
        } else if (fogRisk === 1) {
            valFogRisk.textContent = 'MODERATE FOG';
            valFogRisk.style.color = 'var(--neon-amber)';
        } else {
            valFogRisk.textContent = 'LOW / NOMINAL';
            valFogRisk.style.color = 'var(--neon-green)';
        }

        climateTag.className = 'status-pill pill-cyan';
        climateTag.textContent = 'DHT11 ONLINE';
    }

    // 8. Actuators & Motor Power Matrix
    valMotorL.textContent = `${rover.left_pwm} PWM`;
    valMotorR.textContent = `${rover.right_pwm} PWM`;

    renderBipolarGauge(barMotorL, dirMotorL, pctMotorL, rover.left_pwm);
    renderBipolarGauge(barMotorR, dirMotorR, pctMotorR, rover.right_pwm);

    valMastAngle.textContent = `${rover.mast_angle}°`;
    const mastPct = (rover.mast_angle / 180.0) * 100;
    barMastAngle.style.width = `${mastPct}%`;
    cursorMast.style.left = `${mastPct}%`;

    // 9. 16-Channel CRSF Live Oscilloscope
    if (channels && channels.length === 16) {
        for (let i = 0; i < 16; i++) {
            const raw = channels[i];
            const numEl = document.getElementById(`ch-num-${i}`);
            const barEl = document.getElementById(`ch-bar-${i}`);

            if (numEl) numEl.textContent = raw;
            if (barEl) {
                const pct = Math.max(0, Math.min(100, ((raw - 172) / (1811 - 172)) * 100));
                barEl.style.width = `${pct}%`;
            }
        }
    }
}

function renderBipolarGauge(barEl, dirEl, pctEl, pwm) {
    const absPwm = Math.abs(pwm);
    const pct = Math.round((absPwm / 255.0) * 100);
    const halfWidth = (absPwm / 255.0) * 50.0;

    pctEl.textContent = `${pct}%`;

    if (pwm > 0) {
        barEl.style.left = '50%';
        barEl.style.width = `${halfWidth}%`;
        barEl.style.backgroundColor = 'var(--neon-green)';
        dirEl.textContent = 'FORWARD';
        dirEl.style.color = 'var(--neon-green)';
    } else if (pwm < 0) {
        barEl.style.left = `${50.0 - halfWidth}%`;
        barEl.style.width = `${halfWidth}%`;
        barEl.style.backgroundColor = 'var(--neon-amber)';
        dirEl.textContent = 'REVERSE';
        dirEl.style.color = 'var(--neon-amber)';
    } else {
        barEl.style.left = '50%';
        barEl.style.width = '0%';
        dirEl.textContent = 'IDLE';
        dirEl.style.color = 'var(--text-secondary)';
    }
}

// -----------------------------------------------------------------------------
// MISSION TIMELINE EVENT LOGGER
// -----------------------------------------------------------------------------
function logEvent(type, message) {
    if (!timelineLogBox) return;

    const entry = document.createElement('div');
    entry.className = `log-entry ${type === 'ALARM' ? 'log-alarm' : (type === 'WARN' ? 'log-warn' : 'log-info')}`;
    
    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0];

    entry.innerHTML = `
        <span class="log-time">[${timeStr}]</span>
        <span class="log-msg"><strong>[${type}]</strong> ${message}</span>
    `;

    timelineLogBox.prepend(entry);

    // Retain maximum 40 log lines
    while (timelineLogBox.children.length > 40) {
        timelineLogBox.removeChild(timelineLogBox.lastChild);
    }
}

// -----------------------------------------------------------------------------
// TACTICAL SOUND SYNTHESIZER (WEB AUDIO API)
// -----------------------------------------------------------------------------
function playTacticalSound(type) {
    if (!audioEnabled) return;

    try {
        if (!audioCtx) {
            audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        }

        if (audioCtx.state === 'suspended') {
            audioCtx.resume();
        }

        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.connect(gain);
        gain.connect(audioCtx.destination);

        const now = audioCtx.currentTime;

        if (type === 'click') {
            osc.frequency.setValueAtTime(800, now);
            osc.frequency.exponentialRampToValueAtTime(400, now + 0.05);
            gain.gain.setValueAtTime(0.12, now);
            gain.gain.exponentialRampToValueAtTime(0.001, now + 0.05);
            osc.start(now);
            osc.stop(now + 0.05);
        } else if (type === 'connect') {
            osc.frequency.setValueAtTime(440, now);
            osc.frequency.exponentialRampToValueAtTime(880, now + 0.15);
            gain.gain.setValueAtTime(0.15, now);
            gain.gain.exponentialRampToValueAtTime(0.001, now + 0.15);
            osc.start(now);
            osc.stop(now + 0.15);
        } else if (type === 'alarm') {
            osc.type = 'sawtooth';
            osc.frequency.setValueAtTime(950, now);
            osc.frequency.setValueAtTime(650, now + 0.1);
            gain.gain.setValueAtTime(0.18, now);
            gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);
            osc.start(now);
            osc.stop(now + 0.2);
        }
    } catch (e) {
        // Audio policy or unsupported
    }
}

// -----------------------------------------------------------------------------
// "LEARN MY TRANSMITTER" ENGINE
// -----------------------------------------------------------------------------
function startLearnMode(targetFunction) {
    activeLearnTarget = targetFunction;
    learnBanner.classList.remove('hidden');
    playTacticalSound('click');

    const funcLabels = {
        throttle: 'THROTTLE / DRIVE SPEED',
        yaw: 'YAW / STEERING TURN',
        tilt: 'MAST / CAMERA TILT SERVO',
        aux: 'AUXILIARY SAFETY SWITCH'
    };

    learnTitle.textContent = `LEARNING: ${funcLabels[targetFunction] || targetFunction}`;
    learnSubtitle.textContent = 'Move the stick or toggle the switch on your RadioMaster T8L transmitter now...';

    // Highlight slot
    document.querySelectorAll('.mapping-slot-card').forEach(c => c.classList.remove('slot-active-learn'));
    const targetSlot = document.getElementById(`slot-${targetFunction}`);
    if (targetSlot) targetSlot.classList.add('slot-active-learn');

    // 1. Capture baseline snapshot
    fetch('/api/learn/baseline', { method: 'POST' })
        .then(res => res.json())
        .then(() => {
            if (learnPollInterval) clearInterval(learnPollInterval);
            learnPollInterval = setInterval(pollLearnMovement, 120);

            if (learnTimeoutTimer) clearTimeout(learnTimeoutTimer);
            learnTimeoutTimer = setTimeout(() => {
                cancelLearnMode('Learn timed out — no significant movement detected.');
            }, 9000);
        })
        .catch(err => {
            cancelLearnMode(`Error: ${err.message}`);
        });
}

function pollLearnMovement() {
    fetch('/api/learn/detect', { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            if (data.detected && data.channel >= 0) {
                completeLearnMode(data.channel);
            }
        })
        .catch(err => console.error('Learn Poll Error:', err));
}

function completeLearnMode(channelIndex) {
    clearInterval(learnPollInterval);
    clearTimeout(learnTimeoutTimer);
    learnPollInterval = null;
    learnTimeoutTimer = null;

    if (activeLearnTarget === 'throttle') selThrottle.value = channelIndex;
    else if (activeLearnTarget === 'yaw') selYaw.value = channelIndex;
    else if (activeLearnTarget === 'tilt') selTilt.value = channelIndex;
    else if (activeLearnTarget === 'aux') selAux.value = channelIndex;

    const cell = document.getElementById(`ch-cell-${channelIndex}`);
    if (cell) {
        cell.classList.add('highlight-target');
        setTimeout(() => cell.classList.remove('highlight-target'), 3500);
    }

    playTacticalSound('connect');
    logEvent('CAL', `Auto-assigned Channel ${channelIndex + 1} to ${activeLearnTarget.toUpperCase()}`);

    learnTitle.textContent = `✅ ASSIGNED TO CHANNEL ${channelIndex + 1}`;
    learnSubtitle.textContent = 'Stick movement detected! Remember to save mapping below.';

    setTimeout(() => {
        learnBanner.classList.add('hidden');
        document.querySelectorAll('.mapping-slot-card').forEach(c => c.classList.remove('slot-active-learn'));
    }, 2000);
}

function cancelLearnMode(msg) {
    if (learnPollInterval) clearInterval(learnPollInterval);
    if (learnTimeoutTimer) clearTimeout(learnTimeoutTimer);
    learnPollInterval = null;
    learnTimeoutTimer = null;

    learnBanner.classList.add('hidden');
    document.querySelectorAll('.mapping-slot-card').forEach(c => c.classList.remove('slot-active-learn'));

    if (msg) {
        saveStatus.style.color = 'var(--neon-amber)';
        saveStatus.textContent = msg;
        setTimeout(() => { saveStatus.textContent = ''; }, 4000);
    }
}

// -----------------------------------------------------------------------------
// REST API & SETTINGS
// -----------------------------------------------------------------------------
function loadPersistentConfig() {
    fetch('/api/config')
        .then(res => res.json())
        .then(data => {
            const m = data.mapping;
            if (m) {
                selThrottle.value = m.throttle_channel;
                invThrottle.checked = m.invert_throttle;

                selYaw.value = m.yaw_channel;
                invYaw.checked = m.invert_yaw;

                selTilt.value = m.tilt_channel;
                invTilt.checked = m.invert_tilt;

                selAux.value = m.aux_channel;
                selAuxMode.value = m.aux_mode;

                cfgDeadband.value = m.deadband || 35;
                valCfgDeadband.textContent = m.deadband || 35;

                cfgTilt.value = m.tilt_limit_deg || 35;
                valCfgTilt.textContent = `${m.tilt_limit_deg || 35}°`;

                cfgSpeed.value = m.max_drive_speed || 255;
                valCfgSpeed.textContent = m.max_drive_speed || 255;

                const obsBrakeEl = document.getElementById('cfg-obs-brake');
                const valObsBrakeEl = document.getElementById('val-cfg-obs-brake');
                if (obsBrakeEl) {
                    obsBrakeEl.checked = m.enable_obstacle_brake || false;
                    if (valObsBrakeEl) valObsBrakeEl.textContent = obsBrakeEl.checked ? 'ENABLED' : 'DISABLED';
                }
            }

            if (data.wifi) {
                staSsid.value = data.wifi.sta_ssid || '';
            }
        })
        .catch(err => console.error('Config Load Error:', err));
}

function saveChannelMapping() {
    playTacticalSound('click');
    const obsBrakeEl = document.getElementById('cfg-obs-brake');
    const payload = {
        throttle_channel: parseInt(selThrottle.value),
        yaw_channel: parseInt(selYaw.value),
        tilt_channel: parseInt(selTilt.value),
        aux_channel: parseInt(selAux.value),
        invert_throttle: invThrottle.checked,
        invert_yaw: invYaw.checked,
        invert_tilt: invTilt.checked,
        aux_mode: parseInt(selAuxMode.value),
        deadband: parseInt(cfgDeadband.value),
        tilt_limit_deg: parseFloat(cfgTilt.value),
        max_drive_speed: parseInt(cfgSpeed.value),
        enable_obstacle_brake: obsBrakeEl ? obsBrakeEl.checked : false
    };

    saveStatus.style.color = 'var(--neon-cyan)';
    saveStatus.textContent = 'Flashing mapping parameters to NVS...';

    fetch('/api/config/mapping', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            saveStatus.style.color = 'var(--neon-green)';
            saveStatus.textContent = '✅ Mapping permanently stored in NVS Flash!';
            logEvent('CFG', 'Transmitter mapping saved to flash.');
        } else {
            saveStatus.style.color = 'var(--neon-red)';
            saveStatus.textContent = `❌ Error: ${data.error}`;
        }
        setTimeout(() => { saveStatus.textContent = ''; }, 4500);
    })
    .catch(err => {
        saveStatus.style.color = 'var(--neon-red)';
        saveStatus.textContent = `❌ Network Error: ${err.message}`;
    });
}

function saveWifiCredentials() {
    playTacticalSound('click');
    const payload = {
        sta_ssid: staSsid.value.trim(),
        sta_pass: staPass.value
    };

    wifiStatus.style.color = 'var(--neon-cyan)';
    wifiStatus.textContent = 'Transmitting WiFi credentials...';

    fetch('/api/config/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            wifiStatus.style.color = 'var(--neon-green)';
            wifiStatus.textContent = '✅ Site WiFi uplink saved! Reboot rover to connect.';
            logEvent('WIFI', `Uplink SSID saved: ${payload.sta_ssid}`);
        } else {
            wifiStatus.style.color = 'var(--neon-red)';
            wifiStatus.textContent = '❌ Failed to save WiFi config';
        }
        setTimeout(() => { wifiStatus.textContent = ''; }, 4500);
    })
    .catch(err => {
        wifiStatus.style.color = 'var(--neon-red)';
        wifiStatus.textContent = `❌ Error: ${err.message}`;
    });
}

// -----------------------------------------------------------------------------
// EVENT HANDLERS
// -----------------------------------------------------------------------------
function initEventHandlers() {
    // Learn Detect Buttons
    document.querySelectorAll('.btn-learn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const func = e.target.getAttribute('data-func');
            startLearnMode(func);
        });
    });

    btnCancelLearn.addEventListener('click', () => {
        cancelLearnMode('Learn mode aborted.');
    });

    // Save Buttons
    btnSaveMapping.addEventListener('click', saveChannelMapping);
    btnSaveWifi.addEventListener('click', saveWifiCredentials);

    // Emergency Stop
    btnMasterEstop.addEventListener('click', () => {
        playTacticalSound('click');
        fetch('/api/emergency_stop', { method: 'POST' })
            .then(res => res.json())
            .then(data => {
                if (data.emergency_stop) {
                    btnMasterEstop.classList.add('active');
                    logEvent('ALARM', '🛑 MANUAL EMERGENCY STOP ENGAGED!');
                } else {
                    btnMasterEstop.classList.remove('active');
                    logEvent('SYS', 'Emergency stop cleared. Motors active.');
                }
            });
    });

    // Audio Toggle
    btnAudioToggle.addEventListener('click', () => {
        audioEnabled = !audioEnabled;
        audioIcon.textContent = audioEnabled ? '🔊' : '🔇';
        logEvent('SYS', `Tactical audio ${audioEnabled ? 'enabled' : 'muted'}`);
        if (audioEnabled) playTacticalSound('click');
    });

    // Fullscreen Toggle
    if (btnFullscreen) {
        btnFullscreen.addEventListener('click', () => {
            if (!document.fullscreenElement) {
                document.documentElement.requestFullscreen().catch(() => {});
            } else {
                document.exitFullscreen().catch(() => {});
            }
        });
    }

    // Reconnect Button
    btnReconnect.addEventListener('click', () => {
        connectWebSocket();
    });

    // Clear Log
    btnClearLog.addEventListener('click', () => {
        timelineLogBox.innerHTML = '';
        logEvent('SYS', 'Timeline log buffer cleared.');
    });

    // Config Sliders Live Value Readouts
    cfgDeadband.addEventListener('input', () => { valCfgDeadband.textContent = cfgDeadband.value; });
    cfgTilt.addEventListener('input', () => { valCfgTilt.textContent = `${cfgTilt.value}°`; });
    cfgSpeed.addEventListener('input', () => { valCfgSpeed.textContent = cfgSpeed.value; });

    const obsBrakeEl = document.getElementById('cfg-obs-brake');
    const valObsBrakeEl = document.getElementById('val-cfg-obs-brake');
    if (obsBrakeEl) {
        obsBrakeEl.addEventListener('change', () => {
            if (valObsBrakeEl) valObsBrakeEl.textContent = obsBrakeEl.checked ? 'ENABLED' : 'DISABLED';
        });
    }
}

// Hardware Motor Test Runner
window.runMotorTest = function(motor) {
    const tag = document.getElementById('test-status-tag');
    if (tag) {
        tag.textContent = `TESTING ${motor.toUpperCase()}...`;
        tag.className = 'status-pill pill-amber';
    }
    logEvent('TEST', `Initiating hardware test: ${motor.toUpperCase()} motor (200 PWM for 2000ms)...`);

    fetch('/api/test_motor', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ motor: motor, speed: 200, duration_ms: 2000 })
    })
    .then(res => res.json())
    .then(data => {
        setTimeout(() => {
            if (tag) {
                tag.textContent = 'READY';
                tag.className = 'status-pill pill-cyan';
            }
            logEvent('TEST', `Hardware test for ${motor.toUpperCase()} motor completed.`);
        }, 2000);
    })
    .catch(err => {
        if (tag) {
            tag.textContent = 'ERROR';
            tag.className = 'status-pill pill-red';
        }
        logEvent('ALARM', `Failed to send motor test command: ${err}`);
    });
};
