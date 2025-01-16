#ifndef GUUS_NFT_H
#define GUUS_NFT_H

#include <cryptonote_basic/cryptonote_basic.h>
#include <cryptonote_core/blockchain.h>
#include <cryptonote_core/cryptonote_core.h>
#include <vector>
#include <string>

// Create an NFT with an encrypted address
void create_nft_with_address(sqlite3* db,
		             const std::string& name,
                             const std::string& description,
                             uint64_t nft_id,
                             const std::vector<uint8_t>& encrypted_address,
                             const std::string& utility_data,
                             uint64_t block_height);

// Retrieve NFT details by ID
void get_nft_details(uint64_t nft_id,  uint64_t block_height);

void transfer_nft(uint64_t nft_id,
                  const std::vector<uint8_t>& new_encrypted_address,
                  uint64_t block_height);

std::vector<cryptonote::nft_metadata> list_nfts_by_owner(sqlite3* db, const std::vector<uint8_t>& encrypted_address,
                        uint64_t block_height);

void redeem_nft_utility(sqlite3* db, uint64_t nft_id, uint64_t block_height);


#endif // GUUS_NFT_H
