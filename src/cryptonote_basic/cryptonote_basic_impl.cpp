// Copyright (c) 2014-2019, The Monero Project
// 
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "include_base_utils.h"
using namespace epee;

#include "cryptonote_basic_impl.h"
#include "string_tools.h"
#include "serialization/binary_utils.h"
#include "serialization/container.h"
#include "cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "misc_language.h"
#include "common/base58.h"
#include "crypto/hash.h"
#include "int-util.h"
#include "common/dns_utils.h"
#include "common/guus.h"
#include <cfenv>

#undef GUUS_DEFAULT_LOG_CATEGORY
#define GUUS_DEFAULT_LOG_CATEGORY "cn"

namespace cryptonote {

  struct integrated_address {
    account_public_address adr;
    crypto::hash8 payment_id;

    BEGIN_SERIALIZE_OBJECT()
      FIELD(adr)
      FIELD(payment_id)
    END_SERIALIZE()

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(adr)
      KV_SERIALIZE(payment_id)
    END_KV_SERIALIZE_MAP()
  };

  /************************************************************************/
  /* Cryptonote helper functions                                          */
  /************************************************************************/
  //-----------------------------------------------------------------------------------------------
  size_t get_min_block_weight(uint8_t version)
  {
    return CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5;
  }
  //-----------------------------------------------------------------------------------------------
  size_t get_max_tx_size()
  {
    return CRYPTONOTE_MAX_TX_SIZE;
  }
  //-----------------------------------------------------------------------------------------------
  // Append NFT data to transaction extra
  bool append_nft_to_tx_extra(std::vector<uint8_t>& extra, const nft_data& nft) {
    if (extra.size() + sizeof(nft_data) > CRYPTONOTE_TX_EXTRA_NONCE_MAX_COUNT) {
      MERROR("Failed to append NFT data to transaction extra: extra data too large");
      return false;
    }
    std::string nft_blob = t_serializable_object_to_blob(nft);
    return add_extra_nonce_to_tx_extra(extra, nft_blob);
  }
  //------------------------------------------------------------------------------------------------
  // Extract NFT data from transaction extra
  bool get_nft_from_tx_extra(const std::vector<uint8_t>& extra, nft_data& nft) {
    std::vector<tx_extra_field> tx_extra_fields;
    if (!parse_tx_extra(extra, tx_extra_fields)) {
      MERROR("Failed to parse transaction extra fields");
      return false;
    }

    tx_extra_nonce extra_nonce;
    if (find_tx_extra_field_by_type(tx_extra_fields, extra_nonce)) {
      if (!extra_nonce.nonce.empty()) {
        return ::serialization::parse_binary(extra_nonce.nonce, nft);
      }
    }
    return false;
  }
//-----------------------------------------------------------------------------------
bool get_base_block_reward(size_t median_weight, size_t current_block_weight, uint64_t already_generated_coins, uint64_t &reward, uint64_t &reward_unpenalized, uint8_t version, uint64_t height, const std::vector<uint8_t>& extra) {

    //premine reward
    if (already_generated_coins == 0)
    {
      reward = 122'740'188'075;
      return true;
    }

    static_assert(DIFFICULTY_TARGET_V2%60==0,"difficulty targets must be a multiple of 60");

    uint64_t base_reward =
      version >= network_version_15 ? BLOCK_REWARD_HF15 :
      version >= network_version_8  ? block_reward_unpenalized_formula_v8(height) :
        block_reward_unpenalized_formula_v7(already_generated_coins, height);

    uint64_t full_reward_zone = get_min_block_weight(version);

    //make it soft
    if (median_weight < full_reward_zone) {
      median_weight = full_reward_zone;
    }

    if (current_block_weight <= median_weight) {
      reward = reward_unpenalized = base_reward;
    } else {
      if(current_block_weight > 2 * median_weight) {
        MERROR("Block cumulative weight is too big: " << current_block_weight << ", expected less than " << 2 * median_weight);
        return false;
      }

      assert(median_weight < std::numeric_limits<uint32_t>::max());
      assert(current_block_weight < std::numeric_limits<uint32_t>::max());

      uint64_t product_hi;
      // BUGFIX: 32-bit saturation bug (e.g. ARM7), the result was being
      // treated as 32-bit by default.
      uint64_t multiplicand = 2 * median_weight - current_block_weight;
      multiplicand *= current_block_weight;
      uint64_t product_lo = mul128(base_reward, multiplicand, &product_hi);

      uint64_t reward_hi;
      uint64_t reward_lo;
      div128_32(product_hi, product_lo, static_cast<uint32_t>(median_weight), &reward_hi, &reward_lo);
      div128_32(reward_hi, reward_lo, static_cast<uint32_t>(median_weight), &reward_hi, &reward_lo);
      assert(0 == reward_hi);
      assert(reward_lo < base_reward);

      reward_unpenalized = base_reward;
      reward = reward_lo;
    }

    // Check for NFT data in transaction extra
    nft_data nft;
    if (get_nft_from_tx_extra(extra, nft)) {
      // Adjust reward for NFT transactions
      // Reduce reward by 10% for NFT transactions to potentially discourage spam
      uint64_t nft_reward_reduction = reward / 10; // 10% reduction
      reward = reward > nft_reward_reduction ? reward - nft_reward_reduction : 0; // Ensure reward doesn't go below 0

      // Log or apply other incentives or penalties here if needed
      MINFO("Adjusted block reward due to NFT transaction: Original reward: " << reward_unpenalized << ", Adjusted reward: " << reward);
    }

    return true;
}
  //------------------------------------------------------------------------------------
  uint8_t get_account_address_checksum(const public_address_outer_blob& bl)
  {
    const unsigned char* pbuf = reinterpret_cast<const unsigned char*>(&bl);
    uint8_t summ = 0;
    for(size_t i = 0; i!= sizeof(public_address_outer_blob)-1; i++)
      summ += pbuf[i];

    return summ;
  }
  //------------------------------------------------------------------------------------
  uint8_t get_account_integrated_address_checksum(const public_integrated_address_outer_blob& bl)
  {
    const unsigned char* pbuf = reinterpret_cast<const unsigned char*>(&bl);
    uint8_t summ = 0;
    for(size_t i = 0; i!= sizeof(public_integrated_address_outer_blob)-1; i++)
      summ += pbuf[i];

    return summ;
  }
  //-----------------------------------------------------------------------
  std::string get_account_address_as_str(
      network_type nettype
    , bool subaddress
    , account_public_address const & adr
    )
  {
    uint64_t address_prefix = subaddress ? get_config(nettype).CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX : get_config(nettype).CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;

    return tools::base58::encode_addr(address_prefix, t_serializable_object_to_blob(adr));
  }
  //-----------------------------------------------------------------------
  std::string get_account_integrated_address_as_str(
      network_type nettype
    , account_public_address const & adr
    , crypto::hash8 const & payment_id
    )
  {
    uint64_t integrated_address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;

    integrated_address iadr = {
      adr, payment_id
    };
    return tools::base58::encode_addr(integrated_address_prefix, t_serializable_object_to_blob(iadr));
  }
  //-----------------------------------------------------------------------
  bool is_coinbase(const transaction& tx)
  {
    if(tx.vin.size() != 1)
      return false;

    if(tx.vin[0].type() != typeid(txin_gen))
      return false;

    return true;
  }
  //-----------------------------------------------------------------------
  bool get_account_address_from_str(
      address_parse_info& info
    , network_type nettype
    , std::string const & str
    )
  {
    uint64_t address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t integrated_address_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t subaddress_prefix = get_config(nettype).CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;

    if (2 * sizeof(public_address_outer_blob) != str.size())
    {
      blobdata data;
      uint64_t prefix;
      if (!tools::base58::decode_addr(str, prefix, data))
      {
        LOG_PRINT_L2("Invalid address format");
        return false;
      }

      if (integrated_address_prefix == prefix)
      {
        info.is_subaddress = false;
        info.has_payment_id = true;
      }
      else if (address_prefix == prefix)
      {
        info.is_subaddress = false;
        info.has_payment_id = false;
      }
      else if (subaddress_prefix == prefix)
      {
        info.is_subaddress = true;
        info.has_payment_id = false;
      }
      else {
        LOG_PRINT_L1("Wrong address prefix: " << prefix << ", expected " << address_prefix 
          << " or " << integrated_address_prefix
          << " or " << subaddress_prefix);
        return false;
      }

      if (info.has_payment_id)
      {
        integrated_address iadr;
        if (!::serialization::parse_binary(data, iadr))
        {
          LOG_PRINT_L1("Account public address keys can't be parsed");
          return false;
        }
        info.address = iadr.adr;
        info.payment_id = iadr.payment_id;
      }
      else
      {
        if (!::serialization::parse_binary(data, info.address))
        {
          LOG_PRINT_L1("Account public address keys can't be parsed");
          return false;
        }
      }

      if (!crypto::check_key(info.address.m_spend_public_key) || !crypto::check_key(info.address.m_view_public_key))
      {
        LOG_PRINT_L1("Failed to validate address keys");
        return false;
      }
    }
    else
    {
      // Old address format
      std::string buff;
      if(!string_tools::parse_hexstr_to_binbuff(str, buff))
        return false;

      if(buff.size()!=sizeof(public_address_outer_blob))
      {
        LOG_PRINT_L1("Wrong public address size: " << buff.size() << ", expected size: " << sizeof(public_address_outer_blob));
        return false;
      }

      public_address_outer_blob blob = *reinterpret_cast<const public_address_outer_blob*>(buff.data());


      if(blob.m_ver > CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER)
      {
        LOG_PRINT_L1("Unknown version of public address: " << blob.m_ver << ", expected " << CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER);
        return false;
      }

      if(blob.check_sum != get_account_address_checksum(blob))
      {
        LOG_PRINT_L1("Wrong public address checksum");
        return false;
      }

      //we success
      info.address = blob.m_address;
      info.is_subaddress = false;
      info.has_payment_id = false;
    }

    return true;
  }
  //--------------------------------------------------------------------------------
  bool get_account_address_from_str_or_url(
      address_parse_info& info
    , network_type nettype
    , const std::string& str_or_url
    , std::function<std::string(const std::string&, const std::vector<std::string>&, bool)> dns_confirm
    )
  {
    if (get_account_address_from_str(info, nettype, str_or_url))
      return true;
    bool dnssec_valid;
    std::string address_str = tools::dns_utils::get_account_address_as_str_from_url(str_or_url, dnssec_valid, dns_confirm);
    return !address_str.empty() &&
      get_account_address_from_str(info, nettype, address_str);
  }
  //--------------------------------------------------------------------------------
  bool operator ==(const cryptonote::transaction& a, const cryptonote::transaction& b) {
    return cryptonote::get_transaction_hash(a) == cryptonote::get_transaction_hash(b);
  }

  bool operator ==(const cryptonote::block& a, const cryptonote::block& b) {
    return cryptonote::get_block_hash(a) == cryptonote::get_block_hash(b);
  }
}

//--------------------------------------------------------------------------------
bool parse_hash256(const std::string &str_hash, crypto::hash& hash)
{
  std::string buf;
  bool res = epee::string_tools::parse_hexstr_to_binbuff(str_hash, buf);
  if (!res || buf.size() != sizeof(crypto::hash))
  {
    MERROR("invalid hash format: " << str_hash);
    return false;
  }
  else
  {
    buf.copy(reinterpret_cast<char *>(&hash), sizeof(crypto::hash));
    return true;
  }
}

//------------------------------------------------------------------------------------
  // Check if a transaction contains NFT data
  bool is_nft_transaction(const transaction& tx) {
    nft_data nft;
    return get_nft_from_tx_extra(tx.extra, nft);
  }
//-----------------------------------------------------------------------------------
  // Validate NFT data within a transaction
  bool validate_nft_transaction(const transaction& tx) {
    nft_data nft;
    if (!get_nft_from_tx_extra(tx.extra, nft)) {
      MERROR("Failed to extract NFT data from transaction");
      return false;
    }

    // Validate NFT data here, e.g., check if token_id is unique or if the owner matches the transaction output
    if (nft.token_id == 0 || nft.name.empty() || nft.description.empty() || nft.image_url.empty()) {
      MERROR("Invalid NFT data in transaction");
      return false;
    }

    // (TODO): Checking if the owner address matches one of the transaction outputs
    for (const auto &out : tx.vout) {
      if (out.target.type() == typeid(txout_to_key)) {
        const txout_to_key& tk = boost::get<txout_to_key>(out.target);
        if (tk.key == nft.owner.m_spend_public_key) {
          return true;
        }
      }
    }

    MERROR("NFT owner address does not match any output in the transaction");
    return false;
  }
//------------------------------------------------------------------------------------
  // Create an NFT transaction
  bool create_nft_transaction(const std::string& name, const std::string& description, const std::string& image_url, uint64_t token_id, const account_public_address& owner, transaction& tx) {
    nft_data nft;
    nft.name = name;
    nft.description = description;
    nft.image_url = image_url;
    nft.token_id = token_id;
    nft.owner = owner;

    // Construct the transaction, add inputs and outputs, and set up the extra data
    if (!append_nft_to_tx_extra(tx.extra, nft)) {
      MERROR("Failed to append NFT data to transaction");
      return false;
    }

    // (TODO): Adding inputs, outputs, and setting up fees

    return true;
  }
//-------------------------------------------------------------------------------------
  // Transfer an NFT
  bool transfer_nft_transaction(const crypto::hash& nft_token_id, const account_public_address& current_owner, const account_public_address& new_owner, transaction& tx) {
    nft_data nft;
    // Retrieve NFT data from blockchain or current state.
    // Fetch NFT by token_id from the blockchain
    if (!get_nft_by_token_id(nft_token_id, nft)) {
      MERROR("Could not find NFT with token_id: " << epee::string_tools::pod_to_hex(nft_token_id));
      return false;
    }

    // Check if current owner matches
    if (nft.owner != current_owner) {
      MERROR("Current owner does not match the NFT owner");
      return false;
    }

    // Update the NFT owner
    nft.owner = new_owner;

    // Reconstruct transaction with updated NFT data
    if (!append_nft_to_tx_extra(tx.extra, nft)) {
      MERROR("Failed to append NFT data to transaction for transfer");
      return false;
    }
    // TODO:
    // Setup transaction details for transfer (inputs, outputs, etc.)
    // Construct the transaction with the correct inputs and outputs

    return true;
  }
//---------------------------------------------------------------------------------
// Get NFT by token_id
bool get_nft_by_token_id(const crypto::hash& token_id, nft_data& nft) {
  blockchain_db* db = block_chain.get_db_context();
  if (!db) {
    MERROR("Failed to get database context");
    return false;
  }

  std::vector<crypto::hash> tx_hashes;
  if (!db->get_tx_hashes_by_nft_token_id(token_id, tx_hashes)) {
    MERROR("Could not find transaction hashes for NFT token_id: " << epee::string_tools::pod_to_hex(token_id));
    return false;
  }

  // Since an NFT might be transferred multiple times, we would fetch the most recent one
  if (tx_hashes.empty()) {
    MERROR("No transactions found for NFT token_id: " << epee::string_tools::pod_to_hex(token_id));
    return false;
  }

  // Get the latest transaction hash
  crypto::hash latest_tx_hash = tx_hashes.back();
  transaction tx;
  if (!db->get_tx(latest_tx_hash, tx)) {
    MERROR("Failed to retrieve transaction for NFT token_id: " << epee::string_tools::pod_to_hex(token_id));
    return false;
  }

  // Extract NFT data from the transaction's extra
  if (!get_nft_from_tx_extra(tx.extra, nft)) {
    MERROR("Failed to extract NFT data from transaction extra for token_id: " << epee::string_tools::pod_to_hex(token_id));
    return false;
  }

  return true;
}
