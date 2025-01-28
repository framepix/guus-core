#pragma once
#include "crypto/hash.h"
#include "serialization/keyvalue_serialization.h"
#include <vector>
#include <string>

namespace cryptonote {
    struct nft_metadata {
        // Metadata versioning
        static constexpr uint8_t CURRENT_VERSION = 1;
        uint8_t version = CURRENT_VERSION;

        // Core fields
        std::string nft_name;
        std::string nft_description;
        uint64_t nft_id;
        std::vector<uint8_t> encrypted_address;
        std::string utility_data;
        std::vector<uint8_t> image_data;
        crypto::hash image_hash;
        uint64_t block_height;

        // Serialization with version check
        BEGIN_SERIALIZE_OBJECT()
            FIELD(version)
            if (version > CURRENT_VERSION)
                return false;
            FIELD(nft_name)
            FIELD(nft_description)
            FIELD(nft_id)
            FIELD(encrypted_address)
            FIELD(utility_data)
            FIELD(image_data)
            FIELD(image_hash)
            FIELD(block_height)
        END_SERIALIZE()

        // Validation structure
        struct validation_result
        {
            bool valid;
            std::string error;
            crypto::hash computed_hash;
        };

        validation_result validate() const;
    };

    struct nft_info {
        nft_metadata metadata;
        crypto::hash tx_hash;
        uint64_t output_index;
        crypto::public_key owner;
        uint64_t creation_height;
        bool spent;

        // Full serialization including all parent fields
        BEGIN_SERIALIZE_OBJECT()
            FIELD(metadata)
            FIELD(tx_hash)
            FIELD(output_index)
            FIELD(owner)
            FIELD(creation_height)
            FIELD(spent)
        END_SERIALIZE()

        // Add comparison operators for blockchain operations
        bool operator==(const nft_info& other) const {
            return 
                metadata.image_hash == other.metadata.image_hash &&
                tx_hash == other.tx_hash &&
                output_index == other.output_index &&
                owner == other.owner;
        }

        bool operator!=(const nft_info& other) const {
            return !(*this == other);
        }
    };
}
