#pragma once

#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include <evmc/evmc.hpp>

namespace cryptonote {

// Context structure for the Guus EVMC host
struct MoneroHostContext {
    Blockchain* blockchain;       // Pointer to the blockchain instance
    const transaction* current_tx; // Pointer to the current transaction being executed

    MoneroHostContext(Blockchain* bc, const transaction* tx)
        : blockchain(bc), current_tx(tx) {}
};

// Declarations for EVMC host interface
bool monero_account_exists(evmc_host_context* context, const evmc_address* address);
evmc_uint256be monero_get_balance(evmc_host_context* context, const evmc_address* address);
evmc_storage_status monero_set_storage(evmc_host_context* context, const evmc_address* address,
                                       const evmc_bytes32* key, const evmc_bytes32* value);

} // namespace cryptonote
