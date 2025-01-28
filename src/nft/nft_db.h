#pragma once

#include "serialization/keyvalue_serialization.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "sqlite3.h"
#include <string>


namespace cryptonote
{

    struct nft_metadata;
    struct nft_info;

    class NFTDB
    {
    public:
        NFTDB(const std::string& db_path);
        ~NFTDB();

        bool init();
        bool add_nft(const nft_info& nft);
        bool update_nft(const nft_info& nft);
        bool get_nft_by_id(uint64_t nft_id, nft_info& nft);
        bool get_nfts_by_owner(const crypto::public_key& owner, std::vector<nft_info>& nfts);
        bool mark_as_spent(uint64_t nft_id);

    private:
        sqlite3* m_db;
        std::string m_db_path;

        void throw_on_error(int rc, const char* msg);
    };
}
