#include "BypassManager.hpp"
#include "EncoreLog.hpp"
#include <fstream>
#include <unistd.h>

void BypassManager::Init() {
    // Gunakan access() gaya C klasik yang lebih reliable di Android NDK daripada std::filesystem
    if (access(PATH_CMD, F_OK) == 0) {
        targetPath = PATH_CMD;
        mode = 0;
        LOGI("BypassManager: Detected MTK current_cmd interface");
    } else if (access(PATH_EN, F_OK) == 0) {
        targetPath = PATH_EN;
        mode = 1;
        LOGI("BypassManager: Detected MTK en_power_path interface");
    } else {
        LOGE("BypassManager: No supported bypass interface found");
    }
}

void BypassManager::SetBypass(bool enable) {
    if (targetPath.empty()) return;

    std::ofstream file(targetPath);
    if (!file.is_open()) {
        LOGE("BypassManager: Failed to open %s", targetPath.c_str());
        return;
    }

    if (mode == 0) {
        file << (enable ? "0 1" : "0 0");
    } else {
        file << (enable ? "1" : "0"); 
    }
    
    LOGD("BypassManager: Set to %s", enable ? "ON" : "OFF");
}