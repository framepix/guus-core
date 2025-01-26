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
    // Adjusted SQL to include nft_name and nft_description columns
    const char* sql = "INSERT OR REPLACE INTO nft_data (nft_id, nft_name, nft_description, nft_blob, encrypted_address, block_height, image_data, image_hash) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    // Binding the values to the statement
    sqlite3_bind_int64(stmt, 1, nft.nft_id);
    sqlite3_bind_text(stmt, 2, nft.nft_name.c_str(), -1, SQLITE_STATIC);  // Bind NFT name
    sqlite3_bind_text(stmt, 3, nft.nft_description.c_str(), -1, SQLITE_STATIC);  // Bind NFT description
    sqlite3_bind_blob(stmt, 4, blob.data(), blob.size(), SQLITE_STATIC);  // Bind serialized NFT metadata
    sqlite3_bind_blob(stmt, 5, nft.encrypted_address.data(), nft.encrypted_address.size(), SQLITE_STATIC);  // Bind encrypted address
    sqlite3_bind_int64(stmt, 6, nft.block_height);  // Bind block height
    sqlite3_bind_blob(stmt, 7, nft.image_data.data(), nft.image_data.size(), SQLITE_STATIC);  // Bind image data
    sqlite3_bind_blob(stmt, 8, &nft.image_hash, sizeof(nft.image_hash), SQLITE_STATIC);  // Bind image hash

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
                      "    nft_name TEXT NOT NULL,           -- Column for NFT name\n"
		      "    nft_description TEXT NOT NULL,    -- Column for NFT description\n"
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
    // Updated SQL to include nft_name and nft_description
    const char* sql = "UPDATE nft_data SET nft_name = ?, nft_description = ?, nft_blob = ?, image_data = ?, image_hash = ? WHERE nft_id = ? AND block_height = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    // Bind values to the prepared statement
    sqlite3_bind_text(stmt, 1, nft.nft_name.c_str(), -1, SQLITE_STATIC);  // Bind nft_name
    sqlite3_bind_text(stmt, 2, nft.nft_description.c_str(), -1, SQLITE_STATIC);  // Bind nft_description
    sqlite3_bind_blob(stmt, 3, blob.data(), blob.size(), SQLITE_STATIC);   // Bind nft_blob
    sqlite3_bind_blob(stmt, 4, nft.image_data.data(), nft.image_data.size(), SQLITE_STATIC);  // Bind image_data
    sqlite3_bind_blob(stmt, 5, &nft.image_hash, sizeof(nft.image_hash), SQLITE_STATIC);  // Bind image_hash
    sqlite3_bind_int64(stmt, 6, nft.nft_id);  // Bind nft_id
    sqlite3_bind_int64(stmt, 7, nft.block_height);  // Bind block_height to ensure correct entry

    // Execute the update
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
    // Updated SQL query to select nft_name, nft_description, nft_blob, image_data, and image_hash
    const char* sql = "SELECT nft_name, nft_description, nft_blob, image_data, image_hash "
                      "FROM nft_data WHERE encrypted_address = ? AND block_height = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    // Bind the encrypted address and block height
    sqlite3_bind_blob(stmt, 1, encrypted_address.data(), encrypted_address.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, block_height);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cryptonote::nft_metadata nft;

        // Retrieve and set the nft_name
        const char* nft_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (nft_name) {
            nft.nft_name = nft_name;
        }

        // Retrieve and set the nft_description
        const char* nft_description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (nft_description) {
            nft.nft_description = nft_description;
        }

        // Retrieve and set the nft_blob
        const void* blob_data = sqlite3_column_blob(stmt, 2);
        int blob_size = sqlite3_column_bytes(stmt, 2);

        // Copy the blob data into a std::vector<uint8_t>
        std::vector<uint8_t> blob(blob_size);
        std::memcpy(blob.data(), blob_data, blob_size);

        // Deserialize the NFT from the vector
        nft = deserialize_nft(blob);

        // Retrieve and set the image_data
        const void* image_data = sqlite3_column_blob(stmt, 3);
        int image_size = sqlite3_column_bytes(stmt, 3);
        nft.image_data = std::vector<uint8_t>(static_cast<const uint8_t*>(image_data),
                                              static_cast<const uint8_t*>(image_data) + image_size);

        // Retrieve and set the image_hash
        const void* hash_data = sqlite3_column_blob(stmt, 4);
        std::memcpy(&nft.image_hash, hash_data, sizeof(nft.image_hash));

        nfts.push_back(nft);
    }

    sqlite3_finalize(stmt);
    return nfts;
}


