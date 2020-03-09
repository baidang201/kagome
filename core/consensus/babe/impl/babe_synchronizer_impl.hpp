/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_BABE_OBSERVER_IMPL_HPP
#define KAGOME_BABE_OBSERVER_IMPL_HPP

#include "consensus/babe/babe_synchronizer.hpp"

#include "common/logger.hpp"
#include "network/types/sync_clients_set.hpp"

namespace kagome::consensus {
  class BabeSynchronizerImpl
      : public BabeSynchronizer,
        public std::enable_shared_from_this<BabeSynchronizerImpl> {
   public:
    ~BabeSynchronizerImpl() override = default;

    BabeSynchronizerImpl(std::shared_ptr<network::SyncClientsSet> sync_clients);

    void request(const primitives::BlockHash &from,
                 const primitives::BlockHash &to,
                 BlocksHandler &&block_list_handler) override;

   private:
    void pollClients(
        network::BlocksRequest request,
        std::unordered_set<std::shared_ptr<network::SyncProtocolClient>>
            &&polled_clients,
        std::function<void(const std::vector<primitives::Block>)>
            &&requested_blocks_handler) const;

    std::shared_ptr<network::SyncClientsSet> sync_clients_;
    common::Logger logger_;
  };
}  // namespace kagome::consensus

#endif  // KAGOME_BABE_OBSERVER_IMPL_HPP
