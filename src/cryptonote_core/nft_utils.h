#ifndef NFT_UTILS_H
#define NFT_UTILS_H

#include "crypto/hash.h"
#include "nft_db.h"

namespace cryptonote {

bool parse_nft_metadata_from_tx_extra(const std::vector<uint8_t>& extra, nft_metadata& metadata);
crypto::hash get_image_hash(const std::vector<uint8_t>& image_data);
bool validate_against_schema(const nft_metadata& metadata);

} // namespace cryptonote

#endif // NFT_UTILS_H
