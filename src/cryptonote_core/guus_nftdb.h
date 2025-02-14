#pragma once

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <sstream>
#include <sqlite3.h>
#include <vector>
#include <stdexcept>
#include <string>
#include "cryptonote_basic/tx_extra.h" // Include your NFT metadata structure

namespace cryptonote {

class NFTDB {
public:
    NFTDB(const std::string& db_path);
    ~NFTDB();

    bool open();
    void close();

    //void store_image(uint64_t nft_id, const std::vector<uint8_t>& image_data);
    //std::vector<uint8_t> get_image(uint64_t nft_id);
    std::vector<uint8_t> get_owner(uint64_t nft_id);
    bool store_nft(uint64_t nft_id, const std::string& nft_name, const std::string& nft_description,
                       const std::vector<uint8_t>& encrypted_address, const std::string& utility_data,
                       const std::vector<uint8_t>& image_data, const crypto::hash& image_hash, uint64_t block_height);
private:
    sqlite3* db;
    std::string db_path;
    bool execute_query(const std::string& query);
};
}

// Serialize NFT Metadata
std::vector<uint8_t> serialize_nft(const cryptonote::nft_metadata& nft);

// Deserialize NFT Metadata
cryptonote::nft_metadata deserialize_nft(const std::vector<uint8_t>& blob);

// Save NFT to Database
void save_nft_to_db(sqlite3* db, const cryptonote::nft_metadata& nft);

// Load NFT from Database
cryptonote::nft_metadata load_nft_from_db(sqlite3* db, uint64_t nft_id);

// Database Table Creation
void create_nft_table(sqlite3* db);

cryptonote::nft_metadata get_nft_from_db(sqlite3* db, uint64_t nft_id, uint64_t block_height);

void update_nft_in_db(sqlite3* db, const cryptonote::nft_metadata& nft);

std::vector<cryptonote::nft_metadata> get_nfts_by_address_from_db(sqlite3* db,
                                                                  const std::vector<uint8_t>& encrypted_address,
                                                                  uint64_t block_height);
