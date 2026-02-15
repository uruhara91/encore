/*
 * Copyright (C) 2026 Rem01Gaming
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <DeviceMitigationStore.hpp>
#include <Dumpsys.hpp>
#include <Encore.hpp>
#include <EncoreCLI.hpp>
#include <EncoreConfig.hpp>
#include <EncoreConfigStore.hpp>
#include <EncoreLog.hpp>
#include <EncoreUtility.hpp>
#include <GameRegistry.hpp>
#include <InotifyWatcher.hpp>
#include <ModuleProperty.hpp>
#include <PIDTracker.hpp>
#include <ShellUtility.hpp>
#include <SignalHandler.hpp>

#include "../src/CustomLogic/BypassManager.hpp"
#include "../src/CustomLogic/ResolutionManager.hpp"

GameRegistry game_registry;

bool CheckBatterySaver() {
    try {
        DumpsysPower dumpsys_power;
        Dumpsys::Power(dumpsys_power);
        return dumpsys_power.battery_saver;
    } catch (...) {
        return false;
    }
}

void encore_main_daemon(void) {
    constexpr static auto INGAME_LOOP_INTERVAL = std::chrono::milliseconds(1000);
    constexpr static auto NORMAL_LOOP_INTERVAL = std::chrono::seconds(5);

    EncoreProfileMode cur_mode = PERFCOMMON;
    DumpsysWindowDisplays window_displays;

    std::string active_package;
    std::string last_game_package = "";

    auto last_full_check = std::chrono::steady_clock::now();

    bool in_game_session = false;
    bool battery_saver_state = false;
    bool game_requested_dnd = false;

    PIDTracker pid_tracker;
    
    auto GetActiveGame = [&](const std::vector<RecentAppList> &recent_applist) -> std::string {
        for (const auto &recent : recent_applist) {
            if (!recent.visible) continue;
            if (game_registry.is_game_registered(recent.package_name)) {
                return recent.package_name;
            }
        }
        return "";
    };

    auto IsGameStillActive = [&](const std::vector<RecentAppList> &recent_applist, const std::string &package_name) -> bool {
        for (const auto &recent : recent_applist) {
            if (recent.package_name == package_name) return true;
        }
        return false;
    };

    run_perfcommon();
    pthread_setname_np(pthread_self(), "EncoreLoop");

    while (true) {
        if (access(MODULE_UPDATE, F_OK) == 0) [[unlikely]] {
            LOGI("Module update detected, exiting");
            notify("Please reboot your device to complete module update.");
            break;
        }

        auto now = std::chrono::steady_clock::now();
        bool should_scan_window = !in_game_session || (now - last_full_check) >= INGAME_LOOP_INTERVAL;

        if (should_scan_window) {
            try {
                Dumpsys::WindowDisplays(window_displays);
                last_full_check = now;
            } catch (const std::runtime_error &e) {
                LOGE_TAG("Dumpsys", "Window scan failed: {}", e.what());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        // Logic Exit Game
        if (in_game_session && !active_package.empty()) {
             if (!IsGameStillActive(window_displays.recent_app, active_package)) {
                LOGI("Game %s exited (not in visible list)", active_package.c_str());
                goto handle_game_exit;
             }
             
             // PID check is fast (via kill 0 or parsing /proc)
             if (!pid_tracker.is_valid()) {
                LOGI("Game %s PID dead", active_package.c_str());
                goto handle_game_exit;
             }
        }
        
        // Logic Enter Game / Switch Game
        if (active_package.empty()) {
            active_package = GetActiveGame(window_displays.recent_app);
            if (!active_package.empty()) {
                in_game_session = true;
                battery_saver_state = CheckBatterySaver();
            }
        } else if (!in_game_session) {
             // Recovery state
             in_game_session = true;
        }

        // 1. STATE: GAMING
        if (!active_package.empty() && window_displays.screen_awake) {
            // Jika pindah game atau baru masuk
            if (active_package != last_game_package) {
                LOGI("[Encore] Entering Game Mode: %s", active_package.c_str());
                
                // APPLY FEATURES
                ResolutionManager::GetInstance().ApplyGameMode(active_package);
                BypassManager::GetInstance().SetBypass(true);
                // FreezeManager REMOVED

                last_game_package = active_package;
            }

            // Apply Profile (Performance)
            if (cur_mode != PERFORMANCE_PROFILE) {
                pid_t game_pid = Dumpsys::GetAppPID(active_package);
                
                if (game_pid > 0) {
                    LOGI("Applying Performance Profile -> PID: %d", game_pid);
                    cur_mode = PERFORMANCE_PROFILE;
                    
                    auto active_game = game_registry.find_game_ptr(active_package);
                    bool lite_mode = (active_game && active_game->lite_mode) || config_store.get_preferences().enforce_lite_mode;
                    
                    apply_performance_profile(lite_mode, active_package, game_pid);
                    pid_tracker.set_pid(game_pid);
                    
                    if (active_game && active_game->enable_dnd) {
                        game_requested_dnd = true;
                        set_do_not_disturb(true);
                    }
                }
            }
            
            // Sleep Game Mode
            std::this_thread::sleep_for(INGAME_LOOP_INTERVAL);
            continue;
        }

        // 2. STATE: NOT GAMING (Idle/Daily)
        
        if (!last_game_package.empty()) {
        handle_game_exit:
            LOGI("[Encore] Exiting Game Mode: %s", last_game_package.c_str());
            ResolutionManager::GetInstance().ResetGameMode(last_game_package);
            BypassManager::GetInstance().SetBypass(false);

            if (game_requested_dnd) {
                set_do_not_disturb(false);
                game_requested_dnd = false;
            }
            
            last_game_package = "";
            active_package.clear();
            pid_tracker.invalidate();
            in_game_session = false;
        }

        // Cek Battery Saver
        static int bs_check_counter = 0;
        if (bs_check_counter++ > 5) {
            battery_saver_state = CheckBatterySaver();
            bs_check_counter = 0;
        }

        if (battery_saver_state) {
            if (cur_mode != POWERSAVE_PROFILE) {
                LOGI("Switching to PowerSave Profile");
                cur_mode = POWERSAVE_PROFILE;
                apply_powersave_profile();
            }
        } else {
            if (cur_mode != BALANCE_PROFILE) {
                LOGI("Switching to Balance Profile");
                cur_mode = BALANCE_PROFILE;
                apply_balance_profile();
            }
        }

        std::this_thread::sleep_for(NORMAL_LOOP_INTERVAL);
    }
}

int run_daemon() {
    auto SetModule_DescriptionStatus = [](const std::string &status) {
        static const std::string description_base = "Special performance module for your Device.";
        std::string description_new = "[" + status + "] " + description_base;

        std::vector<ModuleProperties> module_properties{{"description", description_new}};

        try {
            ModuleProperty::Change(MODULE_PROP, module_properties);
        } catch (const std::runtime_error &e) {
            LOGE("Failed to apply module properties: {}", e.what());
        }
    };

    auto NotifyFatalError = [&SetModule_DescriptionStatus](const std::string &error_msg) {
        notify(("ERROR: " + error_msg).c_str());
        SetModule_DescriptionStatus("\xE2\x9D\x8C " + error_msg);
    };

    std::atexit([]() { SignalHandler::cleanup_before_exit(); });

    SignalHandler::setup_signal_handlers();

    if (!create_lock_file()) {
        std::cerr << "\033[31mERROR:\033[0m Another instance of Encore Daemon is already running!" << std::endl;
        return EXIT_FAILURE;
    }

    if (!check_dumpsys_sanity()) {
        std::cerr << "\033[31mERROR:\033[0m Dumpsys sanity check failed" << std::endl;
        NotifyFatalError("Dumpsys sanity check failed");
        LOGC("Dumpsys sanity check failed");
        return EXIT_FAILURE;
    }

    if (access(ENCORE_GAMELIST, F_OK) != 0) {
        std::cerr << "\033[31mERROR:\033[0m " << ENCORE_GAMELIST << " is missing" << std::endl;
        NotifyFatalError("gamelist.json is missing");
        LOGC("{} is missing", ENCORE_GAMELIST);
        return EXIT_FAILURE;
    }

    if (!game_registry.load_from_json(ENCORE_GAMELIST)) {
        std::cerr << "\033[31mERROR:\033[0m Failed to parse " << ENCORE_GAMELIST << std::endl;
        NotifyFatalError("Failed to parse gamelist.json");
        LOGC("Failed to parse {}", ENCORE_GAMELIST);
        return EXIT_FAILURE;
    }

    if (!device_mitigation_store.load_config()) {
        std::cerr << "\033[31mERROR:\033[0m Failed to parse " << DEVICE_MITIGATION_FILE << std::endl;
        NotifyFatalError("Failed to parse device_mitigation.json");
        LOGC("Failed to parse {}", DEVICE_MITIGATION_FILE);
        return EXIT_FAILURE;
    }

    if (daemon(0, 0) != 0) {
        LOGC("Failed to daemonize service");
        NotifyFatalError("Failed to daemonize service");
        return EXIT_FAILURE;
    }

    InotifyWatcher file_watcher;
    if (!init_file_watcher(file_watcher)) {
        LOGC("Failed to initialize file watcher");
        NotifyFatalError("Failed to initialize file watcher");
        return EXIT_FAILURE;
    }

    LOGI("Initializing Custom Logic Managers...");
    BypassManager::GetInstance().Init();
    ResolutionManager::GetInstance().LoadGameMap("/data/adb/.config/encore/games.txt");

    LOGI("Encore Tweaks daemon started");
    SetModule_DescriptionStatus("\xF0\x9F\x98\x8B Tweaks applied successfully");
    encore_main_daemon();

    // If we reach this, the daemon is dead
    LOGW("Encore Tweaks daemon exited");
    SignalHandler::cleanup_before_exit();
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (getuid() != 0) {
        std::cerr << "\033[31mERROR:\033[0m Please run this program as root" << std::endl;
        return EXIT_FAILURE;
    }

    // Handle args
    return encore_cli(argc, argv);
}
