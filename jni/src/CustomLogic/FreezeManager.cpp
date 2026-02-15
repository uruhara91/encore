#include "FreezeManager.hpp"
#include "EncoreLog.hpp"
#include <fstream>
#include <dirent.h>
#include <signal.h>
#include <cstring>
#include <algorithm> // Untuk remove

void FreezeManager::LoadConfig(const std::string& configPath) {
    freezeList.clear();
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOGE("FreezeManager: Failed to open config at %s", configPath.c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Hapus \r dan \n (Robust trimming)
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
        std::vector<int> pids = GetPidsByPackageName(pkg);
        if (pids.empty()) {
            LOGD("FreezeManager: No running process found for %s", pkg.c_str());
            continue;
        }

        for (int pid : pids) {
            if (kill(pid, signal) == 0) {
                LOGD("FreezeManager: Sent signal %d to %s (PID: %d)", signal, pkg.c_str(), pid);
            } else {
                LOGE("FreezeManager: Failed to send signal to PID %d", pid);
            }
        }
    }
}

std::vector<int> FreezeManager::GetPidsByPackageName(const std::string& packageName) {
    std::vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!isdigit(*ent->d_name[0])) continue; // Cek karakter pertama saja cukup untuk speed

        int pid = atoi(ent->d_name);
        std::string cmdPath = "/proc/" + std::string(ent->d_name) + "/cmdline";
        std::ifstream cmdFile(cmdPath);
        std::string cmdline;
        
        // Baca cmdline. cmdline di linux dipisahkan null terminator.
        // Kita baca string pertama saja (argv[0]) sebagai nama proses.
        if (std::getline(cmdFile, cmdline, '\0')) {
            // Logic match: Sama persis ATAU starts with (untuk handle sub-proses)
            // Misal: packageName = "com.wa", process = "com.wa:push" -> Match
            if (cmdline == packageName || (cmdline.find(packageName + ":") == 0)) {
                pids.push_back(pid);
            }
        }
    }
    closedir(dir);
    return pids;
}

void FreezeManager::SendSignalToPkg(const std::string& pkg, int signal) {
    // Wrapper function if needed specifically, but logic moved to ApplyFreeze for better logging
}