
#include "nft.h"
#include <sstream>

bool create_nft(const std::string& name, const std::string& description, const std::string& image_url, uint64_t token_id, const std::string& owner, nft_data& nft) {
    nft.token_id = token_id;
    nft.owner = owner;
    nft.metadata.name = name;
    nft.metadata.description = description;
    nft.metadata.image_url = image_url;

    return true;
}

