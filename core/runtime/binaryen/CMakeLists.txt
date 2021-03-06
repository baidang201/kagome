#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

add_library(binaryen_wasm_memory
    wasm_memory_impl.hpp
    wasm_memory_impl.cpp
    )
target_link_libraries(binaryen_wasm_memory
    buffer
    binaryen::binaryen
    )

add_library(binaryen_runtime_external_interface
    runtime_external_interface.hpp
    runtime_external_interface.cpp
    )
target_link_libraries(binaryen_runtime_external_interface
    binaryen::binaryen
    binaryen_wasm_memory
    logger
    )

add_library(binaryen_wasm_executor
    wasm_executor.hpp
    wasm_executor.cpp
    )
target_link_libraries(binaryen_wasm_executor
    binaryen::binaryen
    logger
    )

add_library(binaryen_runtime_api INTERFACE)
target_link_libraries(binaryen_runtime_api INTERFACE
    binaryen_runtime_external_interface
    )

add_library(binaryen_core_api
    runtime_api/core_impl.cpp
    )
target_link_libraries(binaryen_core_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_tagged_transaction_queue_api
    runtime_api/tagged_transaction_queue_impl.cpp
    )
target_link_libraries(binaryen_tagged_transaction_queue_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_block_builder_api
    runtime_api/block_builder_impl.cpp
    )
target_link_libraries(binaryen_block_builder_api
    primitives
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_parachain_host_api
    runtime_api/parachain_host_impl.cpp
    )
target_link_libraries(binaryen_parachain_host_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_grandpa_api
    runtime_api/grandpa_impl.cpp
    )
target_link_libraries(binaryen_grandpa_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_metadata_api
    runtime_api/metadata_impl.cpp
    )
target_link_libraries(binaryen_metadata_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )

add_library(binaryen_offchain_worker_api
    runtime_api/offchain_worker_impl.cpp
    )
target_link_libraries(binaryen_offchain_worker_api
    binaryen_wasm_executor
    binaryen_runtime_api
    )
