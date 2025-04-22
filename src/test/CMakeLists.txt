# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Do not use generator expressions in test sources because the
# SOURCES property is processed to gather test suite macros.
add_executable(test_tapyrus
  addrman_tests.cpp
  allocator_tests.cpp
  amount_tests.cpp
  arith_uint256_tests.cpp
  base32_tests.cpp
  base58_tests.cpp
  base64_tests.cpp
  bech32_tests.cpp
  bip32_tests.cpp
  blockencodings_tests.cpp
  bloom_tests.cpp
  bswap_tests.cpp
  checkqueue_tests.cpp
  coins_tests.cpp
  compress_tests.cpp
  crypto_tests.cpp
  cuckoocache_tests.cpp
  dbwrapper_tests.cpp
  denialofservice_tests.cpp
  descriptor_tests.cpp
  getarg_tests.cpp
  hash_tests.cpp
  key_io_tests.cpp
  key_tests.cpp
  mempool_tests.cpp
  merkle_tests.cpp
  merkleblock_tests.cpp
  miner_tests.cpp
  multisig_tests.cpp
  net_tests.cpp
  netbase_tests.cpp
  policyestimator_tests.cpp
  prevector_tests.cpp
  raii_event_tests.cpp
  random_tests.cpp
  reverselock_tests.cpp
  rpc_tests.cpp
  sanity_tests.cpp
  scheduler_tests.cpp
  script_p2sh_tests.cpp
  script_standard_tests.cpp
  script_tests.cpp
  scriptnum_tests.cpp
  serialize_tests.cpp
  sighash_tests.cpp
  sigopcount_tests.cpp
  skiplist_tests.cpp
  streams_tests.cpp
  timeoffsets_tests.cpp
  torcontrol_tests.cpp
  transaction_tests.cpp
  txindex_tests.cpp
  txpackage_tests.cpp
  txvalidation_tests.cpp
  txvalidationcache_tests.cpp
  uint256_tests.cpp
  util_tests.cpp
  validation_block_tests.cpp
)

include(TargetDataSources)
target_json_data_sources(test_tapyrus
  data/base58_encode_decode.json
  data/bip341_wallet_vectors.json
  data/blockfilters.json
  data/key_io_invalid.json
  data/key_io_valid.json
  data/script_tests.json
  data/sighash.json
  data/tx_invalid.json
  data/tx_valid.json
)
target_raw_data_sources(test_tapyrus NAMESPACE test::data
  data/asmap.raw
)

target_link_libraries(test_tapyrus
  core_interface
  test_util
  tapyrus_cli
  tapyrus_node
  tapyrus_consensus
  minisketch
  secp256k1
  Boost::headers
  libevent::extra
)

if(ENABLE_WALLET)
  add_subdirectory(${PROJECT_SOURCE_DIR}/src/wallet/test wallet)
endif()

function(add_boost_test source_file)
  if(NOT EXISTS ${source_file})
    return()
  endif()

  file(READ "${source_file}" source_file_content)
  string(REGEX
    MATCH "(BOOST_FIXTURE_TEST_SUITE|BOOST_AUTO_TEST_SUITE)\\(([A-Za-z0-9_]+)"
    test_suite_macro "${source_file_content}"
  )
  string(REGEX
    REPLACE "(BOOST_FIXTURE_TEST_SUITE|BOOST_AUTO_TEST_SUITE)\\(" ""
    test_suite_name "${test_suite_macro}"
  )
  if(test_suite_name)
    add_test(NAME ${test_suite_name}
      COMMAND test_tapyrus --run_test=${test_suite_name} --catch_system_error=no --log_level=test_suite -- DEBUG_LOG_OUT
    )
    set_property(TEST ${test_suite_name} PROPERTY
      SKIP_REGULAR_EXPRESSION "no test cases matching filter"
    )
  endif()
endfunction()

function(add_all_test_targets)
  get_target_property(test_source_dir test_tapyrus SOURCE_DIR)
  get_target_property(test_sources test_tapyrus SOURCES)
  foreach(test_source ${test_sources})
    cmake_path(IS_RELATIVE test_source result)
    if(result)
      cmake_path(APPEND test_source_dir ${test_source} OUTPUT_VARIABLE test_source)
    endif()
    add_boost_test(${test_source})
  endforeach()
endfunction()

add_all_test_targets()

install_binary_component(test_tapyrus)
