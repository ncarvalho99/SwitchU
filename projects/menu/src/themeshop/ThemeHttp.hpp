#pragma once

#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace themeshop::http {

bool initialize();
void shutdown();
bool isInitialized();

std::vector<std::uint8_t> getBytes(const std::string& url,
                                   const std::list<std::string>& headers = {});

std::string getText(const std::string& url,
                    const std::list<std::string>& headers = {});

} // namespace themeshop::http