#include "abi_utils.h"
#include <boost/algorithm/hex.hpp>
#include "crypto/crypto.h"
#include "common/util.h"
#include "string_tools.h"

namespace abi_utils {

// Convert string to hex
std::string string_to_hex(const std::string& input) {
    std::string hex;
    boost::algorithm::hex(input.begin(), input.end(), std::back_inserter(hex));
    return hex;
}

// Pad to 32 bytes (64 hex chars)
template<typename T>
std::string pad_to_32_bytes(const T& hex) {
    std::string result = hex;
    while (result.length() < 64) {
        result = "0" + result;
    }
    return result;
}

// Encode dynamic type (string or bytes)
std::string encode_dynamic(const std::string& value) {
    std::string hex_value = string_to_hex(value);
    std::string size = pad_to_32_bytes(string_to_hex(std::to_string(hex_value.length() / 2)));
    return size + hex_value + std::string((64 - (hex_value.length() % 64)) % 64, '0');
}

// Encode fixed-size integer (uint256, int256)
std::string encode_int(const std::string& value, bool is_signed) {
    if (is_signed) {
        int64_t num = std::stoll(value);
        std::stringstream ss;
        ss << std::hex << (num >= 0 ? num : (UINT64_MAX + 1 + num));
        return pad_to_32_bytes(ss.str());
    } else {
        uint64_t num = std::stoull(value);
        std::stringstream ss;
        ss << std::hex << num;
        return pad_to_32_bytes(ss.str());
    }
}

// Compute function selector
std::string compute_function_selector(const std::string& signature) {
    crypto::hash hash;
    crypto::cn_fast_hash(signature.data(), signature.size(), hash);
    
    // Convert the hash to hex and then take the first 8 characters
    std::string hex_hash = epee::string_tools::pod_to_hex(hash.data);
    return hex_hash.substr(0, 8); // Take only the first 8 hex characters (4 bytes)
}

std::string encode_function_call(const std::string& function_signature, const std::vector<std::pair<std::string, std::string>>& args) {
    std::string selector = compute_function_selector(function_signature);
    
    std::vector<std::string> static_args;
    std::vector<std::string> dynamic_args;
    size_t dynamic_offset = 32 * args.size(); // Offset for dynamic data

    for (const auto& arg : args) {
        if (arg.first == "string" || arg.first == "bytes") {
            dynamic_args.push_back(encode_dynamic(arg.second));
            static_args.push_back(pad_to_32_bytes(string_to_hex(std::to_string(dynamic_offset))));
            dynamic_offset += dynamic_args.back().length();
        } else if (arg.first == "uint256") {
            static_args.push_back(encode_int(arg.second, false));
        } else if (arg.first == "int256") {
            static_args.push_back(encode_int(arg.second, true));
        } else if (arg.first == "address") {
            std::string addr = arg.second.substr(2); // Remove '0x' if present
            static_args.push_back(pad_to_32_bytes(addr));
        } else if (arg.first == "bool") {
            static_args.push_back(pad_to_32_bytes(arg.second == "true" ? "1" : "0"));
        }
        // Add more types as needed...
    }

    std::string encoded;
    for (const auto& arg : static_args) 
        encoded += arg;
    for (const auto& arg : dynamic_args) 
        encoded += arg;

    return selector + encoded;
}

} // namespace abi_utils
