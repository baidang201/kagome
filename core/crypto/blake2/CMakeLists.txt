#
# Copyright Soramitsu Co., Ltd. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
#

add_library(blake2
  blake2s.c
  blake2b.c
  )
kagome_install(blake2)
