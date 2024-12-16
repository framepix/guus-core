#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace abi_utils {

std::string string_to_hex(const std::string& input);
template<typename T>
std::string pad_to_32_bytes(const T& hex);
std::string encode_dynamic(const std::string& value);
std::string encode_int(const std::string& value, bool is_signed);
std::string compute_function_selector(const std::string& signature);
std::string encode_function_call(const std::string& function_signature, const std::vector<std::pair<std::string, std::string>>& args);

} // namespace abi_utils
