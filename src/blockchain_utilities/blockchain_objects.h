#ifndef BLOCKCHAIN_OBJECTS_H
#define BLOCKCHAIN_OBJECTS_H

#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/tx_pool.h"
#include "cryptonote_core/frame_pix_list.h"
#include "cryptonote_core/frame_pix_voting.h"

// NOTE(guus): This is done this way because of the circular constructors.
struct blockchain_objects_t
{
  cryptonote::Blockchain m_blockchain;
  cryptonote::tx_memory_pool m_mempool;
  frame_pixs::frame_pix_list m_frame_pix_list;
  blockchain_objects_t() :
    m_blockchain(m_mempool, m_frame_pix_list),
    m_frame_pix_list(m_blockchain),
    m_mempool(m_blockchain) { }
};

#endif // BLOCKCHAIN_OBJECTS_H
