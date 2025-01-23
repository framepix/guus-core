#include <sstream>
#include <sqlite3.h>
#include <vector>
#include <stdexcept>
#include <string>
#include "cryptonote_basic/tx_extra.h" // Ensure correct path to your NFT metadata structure

// Serialize NFT Metadata
std::vector<uint8_t> serialize_nft(const cryptonote::nft_metadata& nft) {
    return nft.to_bytes();
}

// Deserialize NFT Metadata
cryptonote::nft_metadata deserialize_nft(const std::vector<uint8_t>& blob) {
    return cryptonote::nft_metadata::from_bytes(blob);
}

// Save NFT to Database
void save_nft_to_db(sqlite3* db, const cryptonote::nft_metadata& nft) {
    std::vector<uint8_t> blob = serialize_nft(nft);

    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO nft_data (nft_id, nft_blob, encrypted_address, block_height, image_data, image_hash) VALUES (?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_int64(stmt, 1, nft.nft_id);
    sqlite3_bind_blob(stmt, 2, blob.data(), blob.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, nft.encrypted_address.data(), nft.encrypted_address.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, nft.block_height);
    sqlite3_bind_blob(stmt, 5, nft.image_data.data(), nft.image_data.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 6, &nft.image_hash, sizeof(nft.image_hash), SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to insert NFT into database: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
}

// Load NFT from Database
cryptonote::nft_metadata load_nft_from_db(sqlite3* db, uint64_t nft_id) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT nft_blob FROM nft_data WHERE nft_id = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_int64(stmt, 1, nft_id);
    cryptonote::nft_metadata nft;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        if (blob && blob_size > 0) {
            std::vector<uint8_t> blob_data((uint8_t*)blob, (uint8_t*)blob + blob_size);
            nft = deserialize_nft(blob_data);
        } else {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Empty NFT blob retrieved from database.");
        }
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("NFT not found in database.");
    }

    sqlite3_finalize(stmt);
    return nft;
}

// Database Table Creation
void create_nft_table(sqlite3* db) {
    const char* sql = "CREATE TABLE IF NOT EXISTS nft_data (\n"
                      "    nft_id INTEGER PRIMARY KEY,\n"
                      "    nft_blob BLOB NOT NULL,\n"
                      "    encrypted_address BLOB NOT NULL,\n"
                      "    block_height INTEGER NOT NULL,\n"
                      "    image_data BLOB,\n"
                      "    image_hash BLOB\n"
                      ");";

    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string error = "Failed to create table: ";
        error += err_msg;
        sqlite3_free(err_msg);
        throw std::runtime_error(error);
    }
}

// Fetch NFT from Database
cryptonote::nft_metadata get_nft_from_db(sqlite3* db, uint64_t nft_id, uint64_t block_height) {
    cryptonote::nft_metadata nft;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT nft_blob, encrypted_address, image_data, image_hash FROM nft_data WHERE nft_id = ? AND block_height = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_int64(stmt, 1, nft_id);
    sqlite3_bind_int64(stmt, 2, block_height);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Get the blob data from the query result
        const void* blob_data = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        // Copy the blob data into a std::vector
        std::vector<uint8_t> blob(blob_size);
        std::memcpy(blob.data(), blob_data, blob_size);

        // Deserialize the NFT using the vector
        nft = deserialize_nft(blob);

        // Retrieve and set the encrypted_address
        const void* address_data = sqlite3_column_blob(stmt, 1);
        int address_size = sqlite3_column_bytes(stmt, 1);
        nft.encrypted_address = std::vector<uint8_t>(static_cast<const uint8_t*>(address_data), 
                                                     static_cast<const uint8_t*>(address_data) + address_size);

        // Retrieve and set the image_data
        const void* image_data = sqlite3_column_blob(stmt, 2);
        int image_size = sqlite3_column_bytes(stmt, 2);
        nft.image_data = std::vector<uint8_t>(static_cast<const uint8_t*>(image_data), 
                                              static_cast<const uint8_t*>(image_data) + image_size);

        // Retrieve and set the image_hash
        const void* hash_data = sqlite3_column_blob(stmt, 3);
        std::memcpy(&nft.image_hash, hash_data, sizeof(nft.image_hash));
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("NFT not found in the database");
    }

    sqlite3_finalize(stmt);
    return nft;
}

// Update NFT in Database
void update_nft_in_db(sqlite3* db, const cryptonote::nft_metadata& nft) {
    std::vector<uint8_t> blob = serialize_nft(nft);  // Serialize the updated NFT metadata

    sqlite3_stmt* stmt;
    const char* sql = "UPDATE nft_data SET nft_blob = ?, image_data = ?, image_hash = ? WHERE nft_id = ? AND block_height = ?";  // Use block height to ensure the correct NFT

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_blob(stmt, 1, blob.data(), blob.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, nft.image_data.data(), nft.image_data.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, &nft.image_hash, sizeof(nft.image_hash), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, nft.nft_id);
    sqlite3_bind_int64(stmt, 5, nft.block_height);  // Ensure we're updating the correct entry by block height

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update NFT in database: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
}

// Retrieve NFTs by Address from Database
std::vector<cryptonote::nft_metadata> get_nfts_by_address_from_db(sqlite3* db,
                                                                  const std::vector<uint8_t>& encrypted_address,
                                                                  uint64_t block_height) {
    std::vector<cryptonote::nft_metadata> nfts;

    sqlite3_stmt* stmt;
    const char* sql = "SELECT nft_blob, image_data, image_hash FROM nft_data WHERE encrypted_address = ? AND block_height = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    // Bind the encrypted address and block height
    sqlite3_bind_blob(stmt, 1, encrypted_address.data(), encrypted_address.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, block_height);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob_data = sqlite3_column_blob(stmt, 0);
        int blob_size = sqlite3_column_bytes(stmt, 0);

        // Copy the blob data into a std::vector<uint8_t>
        std::vector<uint8_t> blob(blob_size);
        std::memcpy(blob.data(), blob_data, blob_size);

        // Deserialize the NFT from the vector
        cryptonote::nft_metadata nft = deserialize_nft(blob);

        // Retrieve and set the image_data
        const void* image_data = sqlite3_column_blob(stmt, 1);
        int image_size = sqlite3_column_bytes(stmt, 1);
        nft.image_data = std::vector<uint8_t>(static_cast<const uint8_t*>(image_data), 
                                              static_cast<const uint8_t*>(image_data) + image_size);

        // Retrieve and set the image_hash
        const void* hash_data = sqlite3_column_blob(stmt, 2);
        std::memcpy(&nft.image_hash, hash_data, sizeof(nft.image_hash));

        nfts.push_back(nft);
    }

    sqlite3_finalize(stmt);
    return nfts;
}


