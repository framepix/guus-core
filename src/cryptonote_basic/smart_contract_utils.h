#pragma once

#include "cryptonote_basic.h" // Assuming transaction is defined here
#include <vector>
#include <string>
#include "tx_extra.h"

// If BASE_FIELDS isn't defined in your include files, define it here:
#ifndef BASE_FIELDS
#define BASE_FIELDS(T) FIELDS(static_cast<T&>(*this))
#endif

// If FIELDS isn't defined, define it here:
#ifndef FIELDS
#define FIELDS(f)                                                       \
  do {                                                                  \
    bool r = ::do_serialize(ar, f);                                     \
    if (!r || !ar.stream().good()) return false;                        \
  } while(0);
#endif

namespace cryptonote {

// Updated smart_contract_data with a default constructor
struct smart_contract_data {
    std::string bytecode;            // Smart contract bytecode
    std::vector<uint8_t> input_data; // Input data for contract execution
    std::vector<uint8_t> function_call;
    uint64_t gas_limit;
    uint64_t gas_price;

    // Default constructor added
    smart_contract_data() 
        : bytecode(""), input_data(), function_call(), gas_limit(0), gas_price(0) {}

    // Constructor from tx_extra_smart_contract_data
    smart_contract_data(const tx_extra_smart_contract_data& sc_data)
        : bytecode(sc_data.bytecode.begin(), sc_data.bytecode.end()),  // Convert vector to string
          input_data(sc_data.input_data), function_call(sc_data.function_call), 
          gas_limit(sc_data.gas_limit), gas_price(sc_data.gas_price) {}
};

struct transaction_with_smart_contract : public transaction {
    smart_contract_data contract_data; // Contains bytecode and input data for smart contract
    bool is_smart_contract; // Flag to indicate if the transaction is for a smart contract

    // Default constructor initializing is_smart_contract to false
    transaction_with_smart_contract() 
        : transaction(), contract_data(), is_smart_contract(false) {}

    // Constructor from existing transaction
    transaction_with_smart_contract(const transaction& t) 
        : transaction(t), contract_data(), is_smart_contract(false) {}

    // Custom serialization method
    BEGIN_SERIALIZE_OBJECT()
        BASE_FIELDS(transaction) // Serialize base class fields first
        FIELD(contract_data)     // Serialize the smart contract data
        FIELD(is_smart_contract) // Serialize the smart contract flag
    END_SERIALIZE()
};

} // namespace cryptonote
