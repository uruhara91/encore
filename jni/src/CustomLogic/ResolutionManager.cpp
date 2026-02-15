#include "ResolutionManager.hpp"
#include "EncoreLog.hpp"
#include <fstream>
#include <sstream>
#include <algorithm> // Wajib include ini untuk trimming
#include <stdlib.h>

void ResolutionManager::LoadGameMap(const std::string& configPath) {
    gameRatios.clear();
    std::ifstream file(configPath);
    std::string line;
    
    // Format: com.mobile.legends:0.7
    while (std::getline(file, line)) {
        // 1. Trim CR (\r) dan newline dari baris mentah (PENTING!)
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> seglist;
        
        while(std::getline(ss, segment, ':')) {
            seglist.push_back(segment);
        }

        if (seglist.size() == 2) {
            std::string pkg = seglist[0];
            std::string ratio = seglist[1];

            // Bersihkan spasi tidak sengaja (misal: "0.7 ")
            ratio.erase(std::remove(ratio.begin(), ratio.end(), ' '), ratio.end());

            gameRatios[pkg] = ratio;
        }
    }
    LOGI("ResolutionManager: Loaded %zu game configs", gameRatios.size());
}

void ResolutionManager::ApplyGameMode(const std::string& packageName) {
    if (gameRatios.find(packageName) != gameRatios.end()) {
        std::string ratio = gameRatios[packageName];
        // Gunakan full path /system/bin/cmd agar lebih reliable
        std::string cmd = "/system/bin/cmd game mode set --downscale " + ratio + " " + packageName;
        ExecuteCmd(cmd);
        LOGI("ResolutionManager: Applied Downscale %s for %s", ratio.c_str(), packageName.c_str());
    }
}

void ResolutionManager::ResetGameMode(const std::string& packageName) {
    // Reset ke standard saat keluar game
    std::string cmd = "/system/bin/cmd game mode set standard " + packageName;
    ExecuteCmd(cmd);
    LOGD("ResolutionManager: Reset to Standard for %s", packageName.c_str());
}

void ResolutionManager::ExecuteCmd(const std::string& cmd) {
    // Menggunakan system() kadang terblokir environment, tapi sebagai root daemon biasanya aman.
    // Opsi debug: redirect stderr ke logcat jika command gagal, tapi system() standar sudah cukup.
    int ret = system(cmd.c_str());
    if (ret != 0) {
        LOGE("ResolutionManager: Command failed with code %d: %s", ret, cmd.c_str());
    }
}