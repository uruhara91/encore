#pragma once
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

class ResolutionManager {
public:
    static ResolutionManager& GetInstance() {
        static ResolutionManager instance;
        return instance;
    }

    void LoadGameMap(const std::string& configPath);
    void ApplyGameMode(const std::string& packageName);
    void ResetGameMode(const std::string& packageName);

private:
    ResolutionManager() = default;
    
    std::vector<std::pair<std::string, std::string>> gameRatios;
    
    // CACHE: Menyimpan status package yang sudah di-apply
    // Key: PackageName, Value: RatioApplied
    std::unordered_map<std::string, std::string> appliedCache; 

    std::string GetRatio(const std::string& pkg);
    void ExecuteCmdDirect(const std::vector<const char*>& args);
};