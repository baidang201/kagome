/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_EPOCH_STORAGE_DUMB_HPP
#define KAGOME_EPOCH_STORAGE_DUMB_HPP

#include <unordered_map>

#include "consensus/babe/epoch_storage.hpp"

namespace kagome::consensus {
  /**
   * Dumb implementation of epoch storage, which always returns the current
   * epoch; later will be substituted by the one, which will use runtime API
   */
  class EpochStorageDumb : public EpochStorage {
   public:
    ~EpochStorageDumb() override = default;

    void addEpochDescriptor(EpochIndex epoch_number,
                            NextEpochDescriptor epoch_descriptor) override;

    boost::optional<NextEpochDescriptor> getEpochDescriptor(
        EpochIndex epoch_number) const override;

   private:
    std::unordered_map<EpochIndex, NextEpochDescriptor> epoch_map_{};
  };
}  // namespace kagome::consensus

#endif  // KAGOME_EPOCH_STORAGE_DUMB_HPP
