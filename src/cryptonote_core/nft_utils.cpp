#include "nft_utils.h"
#include "crypto/crypto.h"
#include "serialization/serialization.h"
#include "common/util.h"
#include "string_tools.h"

#undef LOG_DEFAULT_CATEGORY
#define LOG_DEFAULT_CATEGORY "nft"

namespace cryptonote {

bool parse_nft_metadata_from_tx_extra(const std::vector<uint8_t>& extra, nft_metadata& metadata) {
    try {
        // Convert vector to span for hex conversion
        epee::span<const uint8_t> extra_span(extra.data(), extra.size());
        std::string extra_hex = epee::to_hex::string(extra_span);
        
        std::string extra_str;
        if(!epee::string_tools::parse_hexstr_to_binbuff(extra_hex, extra_str)) {
            MERROR("Failed to parse TX extra");
            return false;
        }

        // Use existing serialization framework
        std::istringstream iss(extra_str);
        binary_archive<false> ar(iss);
        
        if (!::serialization::serialize(ar, metadata)) {
            MERROR("Failed to deserialize NFT metadata");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        MERROR("Exception parsing NFT metadata: " << e.what());
        return false;
    }
}

crypto::hash get_image_hash(const std::vector<uint8_t>& image_data) {
    crypto::hash hash = crypto::null_hash;
    if (!image_data.empty()) {
        crypto::cn_fast_hash(image_data.data(), image_data.size(), hash);
    }
    return hash;
}

bool validate_against_schema(const nft_metadata& metadata) {
    if (metadata.nft_id == 0) {
        MERROR("Invalid NFT ID (0)");
        return false;
    }

    if (metadata.image_data.empty()) {
        MERROR("Empty image data in NFT metadata");
        return false;
    }

    // Verify image hash matches
    const crypto::hash calculated_hash = get_image_hash(metadata.image_data);
    if (calculated_hash != metadata.image_hash) {
        MERROR("Image hash mismatch in NFT metadata");
        MDEBUG("Calculated: " << epee::string_tools::pod_to_hex(calculated_hash));
        MDEBUG("Claimed:    " << epee::string_tools::pod_to_hex(metadata.image_hash));
        return false;
    }

    // Additional validation for encrypted address
    if (metadata.encrypted_address.empty()) {
        MERROR("Missing encrypted address in NFT metadata");
        return false;
    }

    return true;
}

} // namespace cryptonote
