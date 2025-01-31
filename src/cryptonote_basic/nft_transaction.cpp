
#include "cryptonote_basic.h"
#include "tx_extra.h"

namespace cryptonote {


const cryptonote::tx_extra_nft* transaction::get_nft_metadata() const
{
    static tx_extra_nft nft; // Make nft static to keep it alive beyond this function call
    if (parse_tx_extra_nft(extra, nft)) {
        return &nft;  // Return a pointer to the static object
    }
    return nullptr;  // Return nullptr if parsing fails
}


bool transaction::is_nft_transaction() const noexcept
{
    return get_nft_metadata() != nullptr;
}


uint64_t transaction::get_nft_id() const noexcept {
    const tx_extra_nft* nft_metadata = get_nft_metadata();
    if (nft_metadata) {
        return nft_metadata->metadata.nft_id;
    }
    return 0;  // Return 0 if NFT metadata is not found
}
}
