#pragma once

#include "serialization/keyvalue_serialization.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "sqlite3.h"
#include <string>
#include <mutex>
#include <vector>

namespace cryptonote
{
    struct nft_metadata
    {
        uint8_t version;
        std::string nft_name;
        std::string nft_description;
        uint64_t nft_id;
        std::vector<uint8_t> encrypted_address;
        std::string utility_data;
        std::vector<uint8_t> image_data;
        crypto::hash image_hash;
        uint64_t block_height;
    };

    struct nft_info
    {
        nft_metadata metadata;
        crypto::hash tx_hash;
        uint64_t output_index;
        crypto::public_key owner;
        uint64_t creation_height;
        bool spent;
    };

    class NFTDB
    {
    public:
        NFTDB(const std::string& db_path);
        ~NFTDB();

        bool init();
        void begin_transaction();
        void commit_transaction();
        void rollback_transaction();
        bool add_nft(const nft_info& nft);
        bool update_nft(const nft_info& nft);
        bool get_nft_by_id(uint64_t nft_id, nft_info& nft);
        bool get_nfts_by_owner(const crypto::public_key& owner, std::vector<nft_info>& nfts);
        bool mark_as_spent(uint64_t nft_id);

    private:
        sqlite3* m_db;
        std::string m_db_path;
        std::mutex m_db_mutex;
        int m_schema_version = 1;

        void throw_on_error(int rc, const char* msg);
        void close_db();
        bool migrate_schema();
    };
}
