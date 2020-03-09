/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_BLOCK_ANNOUNCE_HPP
#define KAGOME_BLOCK_ANNOUNCE_HPP

#include "primitives/block.hpp"

namespace kagome::network {
  /**
   * Announce a new complete block on the network
   */
  struct BlockAnnounce {
    // TODO(kamilsa): uncomment. We should broadcast block headers according
    // to spec. However, this might be changed
    //
    // primitives::BlockHeader block;
    primitives::Block block;
  };

  /**
   * @brief compares two BlockAnnounce instances
   * @param lhs first instance
   * @param rhs second instance
   * @return true if equal false otherwise
   */
  inline bool operator==(const BlockAnnounce &lhs, const BlockAnnounce &rhs) {
    return lhs.block == rhs.block;
  }
  inline bool operator!=(const BlockAnnounce &lhs, const BlockAnnounce &rhs) {
    return !(lhs == rhs);
  }

  /**
   * @brief outputs object of type BlockAnnounce to stream
   * @tparam Stream output stream type
   * @param s stream reference
   * @param v value to output
   * @return reference to stream
   */
  template <class Stream,
            typename = std::enable_if_t<Stream::is_encoder_stream>>
  Stream &operator<<(Stream &s, const BlockAnnounce &v) {
    return s << v.block;
  }

  /**
   * @brief decodes object of type Block from stream
   * @tparam Stream input stream type
   * @param s stream reference
   * @param v value to decode
   * @return reference to stream
   */
  template <class Stream,
            typename = std::enable_if_t<Stream::is_decoder_stream>>
  Stream &operator>>(Stream &s, BlockAnnounce &v) {
    return s >> v.block;
  }
}  // namespace kagome::network

#endif  // KAGOME_BLOCK_ANNOUNCE_HPP
