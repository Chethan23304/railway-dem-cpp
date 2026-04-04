#pragma once
#include <string>
#include "StdTypes.hpp"
#include "DemCore.hpp"

// NvmStorage: saves/restores DEM event memory to a binary file
class NvmStorage {
public:
    explicit NvmStorage(const std::string& filePath = "dem_nvram.bin");

    // Save all entries from DemCore to file
    Std_ReturnType store(const DemCore& dem);

    // Restore entries from file into DemCore
    Std_ReturnType restore(DemCore& dem);

private:
    std::string m_filePath;
};
