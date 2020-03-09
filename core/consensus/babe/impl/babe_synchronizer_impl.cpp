/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/babe/impl/babe_synchronizer_impl.hpp"

#include <boost/assert.hpp>
#include "blockchain/block_tree_error.hpp"
#include "primitives/block.hpp"

namespace kagome::consensus {
  BabeSynchronizerImpl::BabeSynchronizerImpl(
      std::shared_ptr<network::SyncClientsSet> sync_clients)
      : sync_clients_{std::move(sync_clients)},
        logger_{common::createLogger("BabeObserver")} {
    BOOST_ASSERT(sync_clients_);
    BOOST_ASSERT(std::all_of(sync_clients_->clients.begin(),
                             sync_clients_->clients.end(),
                             [](const auto &client) { return client; }));
  }

  /*
  void BabeSynchronizerImpl::onBlockAnnounce(
      const network::BlockAnnounce &announce) const {
    // maybe later it will be consensus message with a body
    primitives::Block block{announce.header};

    auto epoch_opt = epoch_storage_->getEpoch(announce.header.number);
    if (!epoch_opt) {
      // TODO(akvinikym) 04.09.19: probably some logic should be done here - we
      // cannot validate block without an epoch, but still it needs to be
      // inserted
      logger_->error("Epoch does not exist");
      return;
    }

    auto validation_res = validator_->validate(block, *epoch_opt);
    if (validation_res) {
      // the block was inserted to the tree by a validator
      auto add_block_res = tree_->addBlock(block);
      if (add_block_res) {
        if (auto execute_res = core_->execute_block(block); not execute_res) {
          logger_->error("Block could not be applied: {}",
                         execute_res.error().message());
          return;
        }
        logger_->debug("Block with number {} was inserted into the storage",
                       block.header.number);
      } else if (add_block_res.error()
                 == blockchain::BlockTreeError::BLOCK_EXISTS) {
        logger_->warn(
            "Block with number {} was not inserted into the storage. Reason: "
            "Already block already exists "
            "{}",
            block.header.number);
      } else {
        logger_->error(
            "Block with number {} was not inserted into the storage. Reason: "
            "{}",
            block.header.number,
            add_block_res.error().message());
      }
      return;
    }

    logger_->warn(
        "Block with number {} was not inserted into the storage. Reason: {}",
        block.header.number,
        validation_res.error().message());
    if (validation_res.error() != blockchain::BlockTreeError::NO_PARENT) {
      // pure validation error, the block is invalid
      return;
    }

    // if there's no parent, try to download the lacking blocks; as for now,
    // issue a request to each client one-by-one
    if (sync_clients_->clients.empty()) {
      return;
    }

    // using last_finalized, because if the block, which we want to get, is in
    // non-finalized fork, we are not interested in it; otherwise, it 100% will
    // be a descendant of the last_finalized
    network::BlocksRequest request{network::BlocksRequest::kBasicAttributes,
                                   tree_->getLastFinalized().block_hash,
                                   announce.header.parent_hash,
                                   network::Direction::DESCENDING,
                                   boost::none};

    pollClients(request, decltype(sync_clients_->clients)());
  }*/

  void BabeSynchronizerImpl::pollClients(
      network::BlocksRequest request,
      std::unordered_set<std::shared_ptr<network::SyncProtocolClient>>
          &&polled_clients,
      std::function<void(const std::vector<primitives::Block>)>
          &&requested_blocks_handler) const {
    // we want to ask each client until we get the blocks we lack, but the
    // sync_clients_ set can change between the requests, so we need to keep
    // track of the clients we already asked
    std::shared_ptr<network::SyncProtocolClient> next_client;
    for (const auto &client : sync_clients_->clients) {
      if (polled_clients.find(client) == polled_clients.end()) {
        next_client = client;
        polled_clients.insert(client);
        break;
      }
    }

    if (!next_client) {
      // we asked all clients we could, so start over
      polled_clients.clear();
      next_client = *sync_clients_->clients.begin();
      polled_clients.insert(next_client);
    }

    next_client->blocksRequest(
        request,
        [self{shared_from_this()},
         request{std::move(request)},
         polled_clients{std::move(polled_clients)},
         requested_blocks_handler{std::move(requested_blocks_handler)}](
            auto &&response_res) mutable {
          if (!response_res) {
            // proceed to the next client
            return self->pollClients(std::move(request),
                                     std::move(polled_clients),
                                     std::move(requested_blocks_handler));
          }
          auto response = std::move(response_res.value());

          // now we need to validate each block from the response, which will
          // also add them to the tree; if any of them fails, we should proceed
          // to the next client
          auto success = true;
          std::vector<primitives::Block> blocks;
          for (const auto &block_data : response.blocks) {
            primitives::Block block;
            if (!block_data.header) {
              // that's bad, we can't insert a block, which does not have at
              // least a header
              success = false;
              break;
            }
            block.header = *block_data.header;

            if (block_data.body) {
              block.body = *block_data.body;
            }

            blocks.push_back(block);
          }

          if (!success) {
            // proceed to the next client
            return self->pollClients(std::move(request),
                                     std::move(polled_clients),
                                     std::move(requested_blocks_handler));
          }

          requested_blocks_handler(blocks);
        });
  }
  void BabeSynchronizerImpl::request(const primitives::BlockHash &from,
                                     const primitives::BlockHash &to,
                                     BlocksHandler &&block_list_handler) {
    network::BlocksRequest request{network::BlocksRequest::kBasicAttributes,
                                   from,
                                   to,
                                   network::Direction::DESCENDING,
                                   boost::none};

    return pollClients(request, {}, std::move(block_list_handler));
  }
}  // namespace kagome::consensus
