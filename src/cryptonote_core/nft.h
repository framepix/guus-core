#pragma once

#include <string>
#include <vector>
#include "cryptonote_basic/cryptonote_basic.h"

namespace nft {

// Struct to hold metadata for an NFT
struct nft_metadata {
    std::string name;
    std::string description;
    std::string image_url;
};

// Struct to represent NFT data
struct nft_data {
    uint64_t token_id;
    std::string owner;
    std::string unique_id;
    std::string transaction_extra;
    nft_metadata metadata;
};

// Create an NFT and embed its data into a transaction
bool create_nft(const std::string& name, 
                const std::string& description, 
                const std::string& image_url, 
                uint64_t token_id, 
                const std::string& owner_address, 
                nft_data& nft,
                cryptonote::transaction& tx);

// Extract NFT data from a transaction
bool extract_nft_from_transaction(const cryptonote::transaction& tx, nft_data& nft);

// Utility for encoding data to Base58 (for transaction extra)
std::string encode_to_base58(const std::string& data);

// Utility for decoding data from Base58
bool decode_from_base58(const std::string& encoded, std::string& data);

} // namespace nft
