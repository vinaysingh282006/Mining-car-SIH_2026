#include "config_store.h"

ConfigStore configStore;

ConfigStore::ConfigStore() {
    resetDefaults();
}

void ConfigStore::resetDefaults() {
    _config.throttleChannel = DEFAULT_CH_THROTTLE;
    _config.yawChannel = DEFAULT_CH_YAW;
    _config.tiltChannel = DEFAULT_CH_TILT;
    _config.auxChannel = DEFAULT_CH_AUX;

    _config.invertThrottle = false;
    _config.invertYaw = false;
    _config.invertTilt = false;

    // Default to Auto-Arm so car is ready to drive upon ELRS link acquisition
    _config.auxMode = AUX_MODE_ALWAYS_ARMED;

    _config.deadband = 35;
    _config.tiltLimitDeg = 35.0f;
    _config.obstacleStopDistanceCm = 15.0f;
    _config.enableObstacleBrake = false;
    _config.maxDriveSpeed = 255;

    strncpy(_config.apSSID, DEFAULT_AP_SSID, sizeof(_config.apSSID) - 1);
    _config.apSSID[sizeof(_config.apSSID) - 1] = '\0';

    strncpy(_config.apPassword, DEFAULT_AP_PASS, sizeof(_config.apPassword) - 1);
    _config.apPassword[sizeof(_config.apPassword) - 1] = '\0';

    _config.staSSID[0] = '\0';
    _config.staPassword[0] = '\0';
}

void ConfigStore::begin() {
    _prefs.begin("rover_cfg", false);
    load();
}

void ConfigStore::load() {
    if (!_prefs.isKey("init_v2")) {
        // First run or upgrade to v2, save defaults
        save();
        return;
    }

    _config.throttleChannel = _prefs.getUChar("ch_thr", DEFAULT_CH_THROTTLE);
    _config.yawChannel = _prefs.getUChar("ch_yaw", DEFAULT_CH_YAW);
    _config.tiltChannel = _prefs.getUChar("ch_tilt", DEFAULT_CH_TILT);
    _config.auxChannel = _prefs.getUChar("ch_aux", DEFAULT_CH_AUX);

    _config.invertThrottle = _prefs.getBool("inv_thr", false);
    _config.invertYaw = _prefs.getBool("inv_yaw", false);
    _config.invertTilt = _prefs.getBool("inv_tilt", false);

    _config.auxMode = _prefs.getUChar("aux_mode", AUX_MODE_ALWAYS_ARMED);

    _config.deadband = _prefs.getUShort("deadband", 35);
    _config.tiltLimitDeg = _prefs.getFloat("tilt_lim", 35.0f);
    _config.obstacleStopDistanceCm = _prefs.getFloat("stop_dist", 15.0f);
    _config.enableObstacleBrake = _prefs.getBool("obs_brake", false);
    _config.maxDriveSpeed = _prefs.getUChar("max_spd", 255);

    String apS = _prefs.getString("ap_ssid", DEFAULT_AP_SSID);
    String apP = _prefs.getString("ap_pass", DEFAULT_AP_PASS);
    String staS = _prefs.getString("sta_ssid", "");
    String staP = _prefs.getString("sta_pass", "");

    strncpy(_config.apSSID, apS.c_str(), sizeof(_config.apSSID) - 1);
    strncpy(_config.apPassword, apP.c_str(), sizeof(_config.apPassword) - 1);
    strncpy(_config.staSSID, staS.c_str(), sizeof(_config.staSSID) - 1);
    strncpy(_config.staPassword, staP.c_str(), sizeof(_config.staPassword) - 1);
}

void ConfigStore::save() {
    _prefs.putBool("init_v2", true);
    _prefs.putUChar("ch_thr", _config.throttleChannel);
    _prefs.putUChar("ch_yaw", _config.yawChannel);
    _prefs.putUChar("ch_tilt", _config.tiltChannel);
    _prefs.putUChar("ch_aux", _config.auxChannel);

    _prefs.putBool("inv_thr", _config.invertThrottle);
    _prefs.putBool("inv_yaw", _config.invertYaw);
    _prefs.putBool("inv_tilt", _config.invertTilt);

    _prefs.putUChar("aux_mode", _config.auxMode);

    _prefs.putUShort("deadband", _config.deadband);
    _prefs.putFloat("tilt_lim", _config.tiltLimitDeg);
    _prefs.putFloat("stop_dist", _config.obstacleStopDistanceCm);
    _prefs.putBool("obs_brake", _config.enableObstacleBrake);
    _prefs.putUChar("max_spd", _config.maxDriveSpeed);

    _prefs.putString("ap_ssid", _config.apSSID);
    _prefs.putString("ap_pass", _config.apPassword);
    _prefs.putString("sta_ssid", _config.staSSID);
    _prefs.putString("sta_pass", _config.staPassword);
}

void ConfigStore::serializeMapping(JsonObject &root) const {
    root["throttle_channel"] = _config.throttleChannel;
    root["yaw_channel"] = _config.yawChannel;
    root["tilt_channel"] = _config.tiltChannel;
    root["aux_channel"] = _config.auxChannel;

    root["invert_throttle"] = _config.invertThrottle;
    root["invert_yaw"] = _config.invertYaw;
    root["invert_tilt"] = _config.invertTilt;

    root["aux_mode"] = _config.auxMode;
    root["deadband"] = _config.deadband;
    root["tilt_limit_deg"] = _config.tiltLimitDeg;
    root["obstacle_stop_distance_cm"] = _config.obstacleStopDistanceCm;
    root["enable_obstacle_brake"] = _config.enableObstacleBrake;
    root["max_drive_speed"] = _config.maxDriveSpeed;
}

bool ConfigStore::deserializeMapping(const JsonObjectConst &root) {
    if (root.containsKey("throttle_channel")) _config.throttleChannel = root["throttle_channel"].as<uint8_t>() % CRSF_NUM_CHANNELS;
    if (root.containsKey("yaw_channel")) _config.yawChannel = root["yaw_channel"].as<uint8_t>() % CRSF_NUM_CHANNELS;
    if (root.containsKey("tilt_channel")) _config.tiltChannel = root["tilt_channel"].as<uint8_t>() % CRSF_NUM_CHANNELS;
    if (root.containsKey("aux_channel")) _config.auxChannel = root["aux_channel"].as<uint8_t>() % CRSF_NUM_CHANNELS;

    if (root.containsKey("invert_throttle")) _config.invertThrottle = root["invert_throttle"].as<bool>();
    if (root.containsKey("invert_yaw")) _config.invertYaw = root["invert_yaw"].as<bool>();
    if (root.containsKey("invert_tilt")) _config.invertTilt = root["invert_tilt"].as<bool>();

    if (root.containsKey("aux_mode")) _config.auxMode = root["aux_mode"].as<uint8_t>();
    if (root.containsKey("deadband")) _config.deadband = root["deadband"].as<uint16_t>();
    if (root.containsKey("tilt_limit_deg")) _config.tiltLimitDeg = root["tilt_limit_deg"].as<float>();
    if (root.containsKey("obstacle_stop_distance_cm")) _config.obstacleStopDistanceCm = root["obstacle_stop_distance_cm"].as<float>();
    if (root.containsKey("enable_obstacle_brake")) _config.enableObstacleBrake = root["enable_obstacle_brake"].as<bool>();
    if (root.containsKey("max_drive_speed")) _config.maxDriveSpeed = root["max_drive_speed"].as<uint8_t>();

    save();
    return true;
}

void ConfigStore::serializeWiFi(JsonObject &root) const {
    root["ap_ssid"] = _config.apSSID;
    root["sta_ssid"] = _config.staSSID;
    root["has_sta_pass"] = (strlen(_config.staPassword) > 0);
}

bool ConfigStore::deserializeWiFi(const JsonObjectConst &root) {
    if (root.containsKey("sta_ssid")) {
        const char *s = root["sta_ssid"];
        strncpy(_config.staSSID, s ? s : "", sizeof(_config.staSSID) - 1);
        _config.staSSID[sizeof(_config.staSSID) - 1] = '\0';
    }
    if (root.containsKey("sta_pass")) {
        const char *p = root["sta_pass"];
        strncpy(_config.staPassword, p ? p : "", sizeof(_config.staPassword) - 1);
        _config.staPassword[sizeof(_config.staPassword) - 1] = '\0';
    }
    save();
    return true;
}

void ConfigStore::serializeFull(JsonObject &root) const {
    JsonObject mapping = root.createNestedObject("mapping");
    serializeMapping(mapping);

    JsonObject wifi = root.createNestedObject("wifi");
    serializeWiFi(wifi);
}
