/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/babe/impl/epoch_storage_dumb.hpp"

namespace kagome::consensus {

  void EpochStorageDumb::addEpochDescriptor(
      EpochIndex epoch_number, NextEpochDescriptor epoch_descriptor) {
    epoch_map_.emplace(epoch_number, epoch_descriptor);
  }

  boost::optional<NextEpochDescriptor> EpochStorageDumb::getEpochDescriptor(
      EpochIndex epoch_number) const {
    auto it = epoch_map_.find(epoch_number);
    if (it == epoch_map_.end()) {
      return boost::none;
    }
    return it->second;
  }
}  // namespace kagome::consensus
