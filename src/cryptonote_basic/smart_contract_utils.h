#pragma once

#include "cryptonote_basic.h" // Assuming transaction is defined here
#include <vector>
#include <string>

namespace cryptonote {

// Define a new structure for smart contract data
struct smart_contract_data {
    std::string bytecode; // Smart contract bytecode
    std::vector<uint8_t> input_data; // Input data for contract execution
    std::vector<uint8_t> function_call;
};

struct transaction_with_smart_contract : public transaction
{
    smart_contract_data contract_data; // Contains bytecode and input data for smart contract
    bool is_smart_contract; // Flag to indicate if the transaction is for a smart contract

    // Default constructor initializing is_smart_contract to false
    transaction_with_smart_contract() : is_smart_contract(false) {}
    
    // Constructor from existing transaction
    transaction_with_smart_contract(const transaction& t) : transaction(t), is_smart_contract(false) {}

    // Custom serialization method
    BEGIN_SERIALIZE_OBJECT()
        BASE_FIELDS(transaction) // Serialize base class fields first
        FIELD(contract_data)     // Serialize the smart contract data
        FIELD(is_smart_contract) // Serialize the smart contract flag
    END_SERIALIZE()
};

/*std::vector<uint8_t> string_to_bytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}*/
/*
std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)));
    }
    return bytes;
}*/

} // namespace cryptonote
