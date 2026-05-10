#pragma once
#include <string>

namespace Network {
    bool IsBanned(const std::string& username);
    void ReportUser(const std::string& username);
}
