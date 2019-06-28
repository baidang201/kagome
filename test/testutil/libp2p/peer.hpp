/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_TESTUTIL_PEER_HPP
#define KAGOME_TESTUTIL_PEER_HPP

#include "libp2p/crypto/key.hpp"
#include "libp2p/multi/multihash.hpp"
#include "libp2p/peer/peer_id.hpp"

namespace testutil {

  using libp2p::crypto::PublicKey;
  using libp2p::multi::Multihash;
  using libp2p::peer::PeerId;
  using T = libp2p::crypto::PublicKey::Type;

  inline PeerId randomPeerId() {
    PublicKey k;

    k.type = T::ED25519;
    for (auto i = 0u; i < 32u; i++) {
      k.data.putUint8(rand() & 0xff);
    }

    return PeerId::fromPublicKey(k).value();
  }

}  // namespace testutil

#endif  // KAGOME_TESTUTIL_PEER_HPP