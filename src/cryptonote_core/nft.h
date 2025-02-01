#pragma once

#include "cryptonote_config.h"

namespace nft {

    enum class nft_type {
        creation,
        transfer,
    };

    inline uint64_t nft_burn_needed(uint8_t hf_version, nft_type type) {
        switch (type) {
            case nft_type::creation:
                return NFT_CREATION_BURN_AMOUNT;
            case nft_type::transfer:
                return NFT_TRANSFER_BURN_AMOUNT;
            default:
                return NFT_CREATION_BURN_AMOUNT;
        }
    }

}
