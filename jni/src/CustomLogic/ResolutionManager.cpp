#include "ResolutionManager.hpp"
#include "EncoreLog.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <EncoreUtility.hpp>

void ResolutionManager::ApplyGameMode(const std::string& packageName, const std::string& ratio) {
    if (ratio == "1.0" || ratio.empty()) {
        return; 
    }

    if (appliedCache.contains(packageName) && appliedCache[packageName] == ratio) return;

    std::vector<const char*> args = {
        "/system/bin/cmd", "game", "mode", "set", "--downscale", 
        ratio.c_str(), packageName.c_str(), NULL 
    };
    ExecuteCmdDirect(args);
    
    appliedCache[packageName] = ratio;
    LOGI("ResolutionManager: Applied %s to %s", ratio.c_str(), packageName.c_str());
}

void ResolutionManager::ResetGameMode(const std::string& packageName) {
    if (!appliedCache.contains(packageName)) return;

    std::vector<const char*> args = {
        "/system/bin/cmd", "game", "mode", "set", "standard", 
        packageName.c_str(), NULL 
    };
    ExecuteCmdDirect(args);
    
    appliedCache.erase(packageName);
    LOGD("ResolutionManager: Reset %s", packageName.c_str());
}