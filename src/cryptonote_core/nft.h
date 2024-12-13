#ifndef NFT_H
#define NFT_H

#include <string>
#include <cstdint>
#include <ctime>
#include "epee/serialization/keyvalue_serialization.h"
#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"

/**
 * @brief Represents the data structure for an NFT.
 * @details Contains all necessary information for an NFT including token ID, owner, metadata, and additional data.
 */
namespace nft {

struct nft_data {
    uint64_t token_id;
    std::string owner;

    struct metadata_t {
        std::string name;
        std::string description;
        std::string image_url;
    } metadata;

    std::string unique_id;
    uint64_t creation_timestamp;
    std::string additional_data; // JSON-encoded field for extensibility
    std::string transaction_extra;

    BEGIN_SERIALIZE()
        FIELD(token_id)
        FIELD(owner)
        FIELD(metadata.name)
        FIELD(metadata.description)
        FIELD(metadata.image_url)
        FIELD(unique_id)
        FIELD(creation_timestamp)
        FIELD(additional_data)
        FIELD(transaction_extra)
    END_SERIALIZE()
};

/**
 * @brief Encodes a string to Base58.
 * @param data The string to encode.
 * @return The Base58 encoded string.
 */
std::string encode_to_base58(const std::string& data);

bool decode_from_base58(const std::string& encoded, std::string& data);
/**
 * @brief Validates if the given string is a valid JSON.
 * @param json_data The JSON string to validate.
 * @return true if the JSON is valid, false otherwise.
 */
bool validate_json(const std::string& json_data);

bool create_nft(const std::string& name, const std::string& description, const std::string& image_url, 
                uint64_t token_id, const std::string& owner_address, nft_data& nft, 
                cryptonote::transaction& tx, const std::string& additional_data);

bool extract_nft_from_transaction(const cryptonote::transaction& tx, nft_data& nft);

} // namespace nft

#endif // NFT_H
