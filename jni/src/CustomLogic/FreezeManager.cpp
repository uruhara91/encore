#include "FreezeManager.hpp"
#include "EncoreLog.hpp"
#include <fstream>
#include <dirent.h>
#include <signal.h>
#include <algorithm>
#include <cstring>

void FreezeManager::LoadConfig(const std::string& configPath) {
    freezeList.clear();
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE("FreezeManager: Failed to open config at %s", configPath.c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // C++23 Refactor opportunity: Bisa pakai ranges::remove, tapi erase-remove idiom klasik lebih safe untuk ndk saat ini.
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

        if (!line.empty() && line[0] != '#') {
            freezeList.push_back(line);
        }
    }
    LOGI("FreezeManager: Loaded %zu apps to freeze", freezeList.size());
}

void FreezeManager::ApplyFreeze(bool freeze) {
    if (freezeList.empty()) return;

    int signal = freeze ? SIGSTOP : SIGCONT;
    LOGI("FreezeManager: Starting %s sequence...", freeze ? "FREEZE" : "UNFREEZE");

    for (const auto& pkg : freezeList) {
        // Kita panggil fungsi helper yang sudah diperbaiki
        SendSignalToPkg(pkg, signal);
    }
}

std::vector<int> FreezeManager::GetPidsByPackageName(const std::string& packageName) {
    std::vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        // ERROR FIX: Hapus tanda bintang (*). ent->d_name[0] sudah berupa char.
        if (!isdigit(ent->d_name[0])) continue;

        int pid = atoi(ent->d_name);
        
        // Optimasi: Baca cmdline
        std::string cmdPath = "/proc/" + std::string(ent->d_name) + "/cmdline";
        std::ifstream cmdFile(cmdPath);
        std::string cmdline;
        
        if (std::getline(cmdFile, cmdline, '\0')) {
            // C++20/23: starts_with
            // Jika NDK support C++20, bisa: if (cmdline == packageName || cmdline.starts_with(packageName + ":"))
            // Untuk kompatibilitas aman pakai find == 0
            if (cmdline == packageName || (cmdline.find(packageName + ":") == 0)) {
                pids.push_back(pid);
            }
        }
    }
    closedir(dir);
    return pids;
}

// ERROR FIX: Implementasi fungsi ini agar parameter 'pkg' dan 'signal' terpakai
void FreezeManager::SendSignalToPkg(const std::string& pkg, int signal) {
    std::vector<int> pids = GetPidsByPackageName(pkg);
    
    if (pids.empty()) {
        // LOGD("FreezeManager: No process found for %s", pkg.c_str());
        return;
    }

    for (int pid : pids) {
        if (kill(pid, signal) == 0) {
            LOGD("FreezeManager: Sent signal %d to %s (PID: %d)", signal, pkg.c_str(), pid);
        } else {
            LOGE("FreezeManager: Failed to send signal to PID %d", pid);
        }
    }
}