#include "nft.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "crypto/crypto.h"
#include "common/base58.h"
#include "common/util.h"
#include "epee/serialization/keyvalue_serialization.h"
#include <sstream>
#include <string>
#include <stdexcept>

namespace nft {

std::string encode_to_base58(const std::string& data) {
    std::string encoded;
    tools::base58::encode(data, encoded);
    return encoded;
}
//------------------------------------------------------------------------
bool decode_from_base58(const std::string& encoded, std::string& data) {
    return tools::base58::decode(encoded, data);
}
//-------------------------------------------------------------------------
bool validate_json(const std::string& json_data) {
    epee::serialization::portable_storage storage;
    return storage.load_from_json(json_data);
}
//-------------------------------------------------------------------------
bool create_nft(const std::string& name, const std::string& description, const std::string& image_url, 
                uint64_t token_id, const std::string& owner_address, nft_data& nft, 
                cryptonote::transaction& tx, const std::string& additional_data) {
    if (name.empty() || description.empty() || image_url.empty() || token_id == 0 || owner_address.empty()) {
        MERROR("Invalid input parameters for creating NFT");
        return false;
    }

    cryptonote::account_public_address owner;
    if (!cryptonote::get_account_address_from_str(owner, cryptonote::network_type::MAINNET, owner_address)) {
        MERROR("Invalid Guus address for NFT owner: " << owner_address);
        return false;
    }

    // Populate NFT data
    nft.token_id = token_id;
    nft.owner = owner_address;
    nft.metadata.name = name;
    nft.metadata.description = description;
    nft.metadata.image_url = image_url;
    nft.creation_timestamp = std::time(nullptr);

    // Validate and assign additional_data
    if (!additional_data.empty()) {
        if (!validate_json(additional_data)) {
            MERROR("Invalid JSON format in additional_data");
            return false;
        }
        nft.additional_data = additional_data;
    } else {
        nft.additional_data = "{}"; // Default to empty JSON object
    }

    // Create unique NFT identifier
    crypto::hash nft_hash;
    std::string nft_data_string = name + description + image_url + std::to_string(token_id);
    crypto::cn_fast_hash(nft_data_string.data(), nft_data_string.size(), nft_hash);
    nft.unique_id = epee::string_tools::pod_to_hex(nft_hash);

    // Serialize NFT data to JSON
    std::string json_string;
    if (!epee::serialization::store_t_to_json(nft, json_string)) {
        MERROR("Failed to serialize NFT data to JSON");
        return false;
    }

    // Encode JSON to Base58 for transaction extra
    std::string encoded_nft_data = encode_to_base58(json_string);
    nft.transaction_extra = encoded_nft_data;

    // Append encoded NFT data to tx_extra in the transaction
    tx.extra.clear(); // Clear existing extra data
    tx.extra.insert(tx.extra.end(), encoded_nft_data.begin(), encoded_nft_data.end());

    return true;
}
//------------------------------------------------------------------------------------------
bool extract_nft_from_transaction(const cryptonote::transaction& tx, nft_data& nft) {
    std::string encoded_nft_data(tx.extra.begin(), tx.extra.end());

    if (encoded_nft_data.empty()) {
        return false;
    }

    std::string decoded_nft_data;
    if (!decode_from_base58(encoded_nft_data, decoded_nft_data)) {
        return false;
    }

    nft_data parsed_nft;
    if (!epee::serialization::load_t_from_json(parsed_nft, decoded_nft_data)) {
        MERROR("Failed to deserialize NFT data from JSON");
        return false;
    }

    nft = parsed_nft;
    return true;
}

} // namespace nft
