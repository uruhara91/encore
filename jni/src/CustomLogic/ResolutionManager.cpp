#include "ResolutionManager.hpp"
#include "EncoreLog.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdlib.h>

void ResolutionManager::LoadGameMap(const std::string& configPath) {
    gameRatios.clear();
    std::ifstream file(configPath);
    std::string line;
    
    while (std::getline(file, line)) {
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
            ratio.erase(std::remove(ratio.begin(), ratio.end(), ' '), ratio.end());
            gameRatios[pkg] = ratio;
        }
    }
    LOGI("ResolutionManager: Loaded %zu game configs", gameRatios.size());
}

void ResolutionManager::ApplyGameMode(const std::string& packageName) {
    // REFACTOR C++20/23: Menggunakan .contains()
    if (gameRatios.contains(packageName)) {
        std::string ratio = gameRatios[packageName];
        std::string cmd = "/system/bin/cmd game mode set --downscale " + ratio + " " + packageName;
        ExecuteCmd(cmd);
        LOGI("ResolutionManager: Applied Downscale %s for %s", ratio.c_str(), packageName.c_str());
    }
}

void ResolutionManager::ResetGameMode(const std::string& packageName) {
    std::string cmd = "/system/bin/cmd game mode set standard " + packageName;
    ExecuteCmd(cmd);
    LOGD("ResolutionManager: Reset to Standard for %s", packageName.c_str());
}

void ResolutionManager::ExecuteCmd(const std::string& cmd) {
    int ret = system(cmd.c_str());
    if (ret != 0) {
        LOGE("ResolutionManager: Command failed: %s", cmd.c_str());
    }
}