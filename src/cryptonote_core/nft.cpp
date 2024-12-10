#include "nft.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "crypto/crypto.h"
#include "common/base58.h"
#include "common/util.h"
#include <sstream>
#include <string>
#include <stdexcept>
#include <json/json.h>

namespace nft {

// Utility to encode data into base58 for transaction extra
std::string encode_to_base58(const std::string& data) {
    std::string encoded;
    tools::base58::encode(data, encoded);
    return encoded;
}

bool create_nft(const std::string& name, const std::string& description, const std::string& image_url, uint64_t token_id, const std::string& owner_address, nft_data& nft) {
    // Basic validation
    if (name.empty() || description.empty() || image_url.empty() || token_id == 0 || owner_address.empty()) {
        return false;
    }

    cryptonote::account_public_address owner;
    if (!cryptonote::get_account_address_from_str(owner, mainnet, owner_address)) {
        throw std::runtime_error("Invalid Guus address for NFT owner");
    }

    nft.token_id = token_id;
    nft.owner = owner_address;
    nft.metadata.name = name;
    nft.metadata.description = description;
    nft.metadata.image_url = image_url;

    // Creating a unique NFT identifier using Guus's crypto functions
    crypto::hash nft_hash;
    std::string nft_data_string = name + description + image_url + std::to_string(token_id);
    crypto::cn_fast_hash(nft_data_string.data(), nft_data_string.size(), nft_hash);
    nft.unique_id = epee::string_tools::pod_to_hex(nft_hash);

    // Prepare NFT data for storage (using JSON for simplicity, ok i will look into this.)
    Json::Value nft_json;
    nft_json["token_id"] = static_cast<Json::UInt64>(token_id);
    nft_json["owner"] = owner_address;
    Json::Value metadata;
    metadata["name"] = name;
    metadata["description"] = description;
    metadata["image_url"] = image_url;
    metadata["unique_id"] = nft.unique_id;
    nft_json["metadata"] = metadata;

    Json::StreamWriterBuilder builder;
    std::string json_string = Json::writeString(builder, nft_json);

    // (TODO): Storing NFT in blockchain (transaction extra)
    // This is highly simplified. But might use any if its best for guus:
    // - Off-chain storage with on-chain pointers
    // - Encrypted data for privacy

    // Encode JSON to Base58 for transaction extra
    std::string encoded_nft_data = encode_to_base58(json_string);
    nft.transaction_extra = encoded_nft_data; // Store for use in transaction creation

    // (TODO): NFT authenticity (pseudo-signing via transaction)
    // This would involve:
    // - Creating a transaction where the NFT creation is part of the transaction data
    // - The owner signs this transaction, indirectly "signing" the NFT

    return true;
}

// Deserialize NFT data from transaction extra
bool deserialize_nft(const std::string& encoded_nft_data, nft_data& nft) {
    std::string decoded_nft_data;
    if (!tools::base58::decode(encoded_nft_data, decoded_nft_data)) {
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;
    if (!reader->parse(decoded_nft_data.c_str(), decoded_nft_data.c_str() + decoded_nft_data.size(), &root, &errors)) {
        return false;
    }

    // Populate nft_data
    nft.token_id = root["token_id"].asUInt64();
    nft.owner = root["owner"].asString();
    nft.metadata.name = root["metadata"]["name"].asString();
    nft.metadata.description = root["metadata"]["description"].asString();
    nft.metadata.image_url = root["metadata"]["image_url"].asString();
    nft.unique_id = root["metadata"]["unique_id"].asString();

    return true;
}

// (TODO): Additional functions for transfer, verification, etc.

} // namespace nft
