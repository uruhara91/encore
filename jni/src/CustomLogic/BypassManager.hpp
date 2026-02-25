#pragma once
#include <string>
#include "EncoreLog.hpp"

class BypassManager {
public:
    static BypassManager& GetInstance() {
        static BypassManager instance;
        return instance;
    }

    void Init();
    void SetBypass(bool enable);
    bool IsSupported() const { return !targetPath.empty(); }

private:
    BypassManager() = default;
    std::string targetPath;
    int mode = 0;

    const std::string PATH_CMD = "/proc/mtk_battery_cmd/current_cmd";
    const std::string PATH_EN = "/proc/mtk_battery_cmd/en_power_path";
};