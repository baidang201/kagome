/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_CORE_API_JRPC_JRPC_PROCESSOR_HPP
#define KAGOME_CORE_API_JRPC_JRPC_PROCESSOR_HPP

#include <jsonrpc-lean/server.h>

namespace kagome::api {
  /**
   * @class JRpcProcessor is base class for JSON RPC processors
   */
  class JRpcProcessor {
   public:
    virtual ~JRpcProcessor() = default;

    /**
     * @brief registers callbacks for jrpc requests
     */
    virtual void registerHandlers() = 0;
  };
}  // namespace kagome::api

#endif  // KAGOME_CORE_API_JRPC_JRPC_PROCESSOR_HPP
