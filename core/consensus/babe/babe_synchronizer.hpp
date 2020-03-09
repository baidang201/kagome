/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_CONSENSUS_BABE_BABE_SYNCHRONIZER_HPP
#define KAGOME_CORE_CONSENSUS_BABE_BABE_SYNCHRONIZER_HPP

#include "primitives/block.hpp"
#include "primitives/block_id.hpp"

namespace kagome::consensus {

  class BabeSynchronizer {
   public:
    using BlocksHandler =
        std::function<void(const std::vector<primitives::Block> &)>;

    virtual ~BabeSynchronizer() = 0;

    virtual void request(const primitives::BlockHash &from,
                         const primitives::BlockHash &to,
                         BlocksHandler &&block_list_handler) = 0;
  };

}  // namespace kagome::consensus

#endif  // KAGOME_CORE_CONSENSUS_BABE_BABE_SYNCHRONIZER_HPP
