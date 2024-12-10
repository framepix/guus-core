#ifndef NFT_H
#define NFT_H

#include <string>
#include <vector>
#include <cstdint>

struct nft_metadata {
    std::string name;
    std::string description;
    std::string image_url;

    BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(name)
        KV_SERIALIZE(description)
        KV_SERIALIZE(image_url)
    END_KV_SERIALIZE_MAP()
};

struct nft_data {
    uint64_t token_id;
    std::string owner;
    nft_metadata metadata;

    BEGIN_KV_SERIALIZE_MAP()
        KV_SERIALIZE(token_id)
        KV_SERIALIZE(owner)
        KV_SERIALIZE(metadata)
    END_KV_SERIALIZE_MAP()
};

#endif // NFT_H
