#pragma once
#include <string>
#include <string_view>
#include "EncoreLog.hpp"

class BypassManager {
public:
    static BypassManager& GetInstance() {
        static BypassManager instance;
        return instance;
    }

    void Init();
    void SetBypass(bool enable);
    [[nodiscard]] bool IsSupported() const { return !targetPath.empty(); }

private:
    BypassManager() = default;
    std::string targetPath;
    int mode = 0;

    static constexpr std::string_view PATH_CMD = "/proc/mtk_battery_cmd/current_cmd";
    static constexpr std::string_view PATH_EN = "/proc/mtk_battery_cmd/en_power_path";
};