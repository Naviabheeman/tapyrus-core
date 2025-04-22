# Copyright (c) 2023-present The tapyrus Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

include(AddWindowsResources)

configure_file(${PROJECT_SOURCE_DIR}/cmake/tapyrus-build-config.h.in tapyrus-build-config.h USE_SOURCE_PERMISSIONS @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

#=============================
# Subprojects
#=============================
# Subprojects include subdirectories that do or could have tests
# and/or benchmark binaries, such as all subtrees and univalue.
# These need to be included before CMAKE_*_OUTPUT_DIRECTORY variables
# are set, so output locations of subproject tests and libraries are
# not overridden.
include(../cmake/leveldb.cmake)
add_subdirectory(univalue)
#=============================
# secp256k1 subtree
#=============================
message("")
message("Configuring secp256k1 subtree...")
set(SECP256K1_DISABLE_SHARED ON CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_ECDH OFF CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_RECOVERY ON CACHE BOOL "" FORCE)
set(SECP256K1_ENABLE_MODULE_MUSIG OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS ${BUILD_TESTS} CACHE BOOL "" FORCE)
if(NOT BUILD_TESTS)
  # Always skip the ctime tests, if we are building no other tests.
  # Otherwise, they are built if Valgrind is available. See SECP256K1_VALGRIND.
  set(SECP256K1_BUILD_CTIME_TESTS ${BUILD_TESTS} CACHE BOOL "" FORCE)
endif()
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
include(GetTargetInterface)
# -fsanitize and related flags apply to both C++ and C,
# so we can pass them down to libsecp256k1 as CFLAGS and LDFLAGS.
get_target_interface(SECP256K1_APPEND_CFLAGS "" sanitize_interface COMPILE_OPTIONS)
string(STRIP "${SECP256K1_APPEND_CFLAGS} ${APPEND_CPPFLAGS}" SECP256K1_APPEND_CFLAGS)
string(STRIP "${SECP256K1_APPEND_CFLAGS} ${APPEND_CFLAGS}" SECP256K1_APPEND_CFLAGS)
set(SECP256K1_APPEND_CFLAGS ${SECP256K1_APPEND_CFLAGS} CACHE STRING "" FORCE)
get_target_interface(SECP256K1_APPEND_LDFLAGS "" sanitize_interface LINK_OPTIONS)
string(STRIP "${SECP256K1_APPEND_LDFLAGS} ${APPEND_LDFLAGS}" SECP256K1_APPEND_LDFLAGS)
set(SECP256K1_APPEND_LDFLAGS ${SECP256K1_APPEND_LDFLAGS} CACHE STRING "" FORCE)
# We want to build libsecp256k1 with the most tested RelWithDebInfo configuration.
enable_language(C)
foreach(config IN LISTS CMAKE_BUILD_TYPE CMAKE_CONFIGURATION_TYPES)
  if(config STREQUAL "")
    continue()
  endif()
  string(TOUPPER "${config}" config)
  set(CMAKE_C_FLAGS_${config} "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endforeach()
# If the CFLAGS environment variable is defined during building depends
# and configuring this build system, its content might be duplicated.
if(DEFINED ENV{CFLAGS})
  deduplicate_flags(CMAKE_C_FLAGS)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)
#add_subdirectory(secp256k1)
#set_target_properties(secp256k1 PROPERTIES
#  EXCLUDE_FROM_ALL TRUE
#)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set top-level target output locations.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

add_custom_target(generate_build_info
  BYPRODUCTS ${PROJECT_BINARY_DIR}/src/tapyrus-build-info.h
  COMMAND ${CMAKE_COMMAND} -DBUILD_INFO_HEADER_PATH=${PROJECT_BINARY_DIR}/src/tapyrus-build-info.h -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/script/GenerateBuildInfo.cmake
  COMMENT "Generating tapyrus-build-info.h"
  VERBATIM
)
add_library(tapyrus_clientversion STATIC EXCLUDE_FROM_ALL
  clientversion.cpp
)
target_link_libraries(tapyrus_clientversion
  PRIVATE
    core_interface
)
add_dependencies(tapyrus_clientversion generate_build_info)

add_subdirectory(crypto)


add_library(tapyrus_consensus STATIC EXCLUDE_FROM_ALL
  arith_uint256.cpp
  consensus/merkle.cpp
  hash.cpp
  primitives/block.cpp
  primitives/transaction.cpp
  pubkey.cpp
  script/interpreter.cpp
  script/script.cpp
  script/script_error.cpp
  uint256.cpp
)
target_link_libraries(tapyrus_consensus
  PRIVATE
    core_interface
    tapyrus_crypto
    secp256k1
)

if(WITH_ZMQ)
  add_subdirectory(zmq)
endif()

# Home for common functionality shared by different executables and libraries.
# Similar to `tapyrus_util` library, but higher-level.
add_library(tapyrus_common STATIC EXCLUDE_FROM_ALL
  base58.cpp
  bech32.cpp
  chain.cpp
  chainparams.cpp
  federationparams.cpp
  coins.cpp
  bloom.cpp
  init.cpp
  compressor.cpp
  core_read.cpp
  core_write.cpp
  chainparams.cpp
  key.cpp
  key_io.cpp
  merkleblock.cpp
  netaddress.cpp
  netbase.cpp
  outputtype.cpp
  policy/feerate.cpp
  policy/policy.cpp
  protocol.cpp
  rpc/util.cpp
  scheduler.cpp
  script/descriptor.cpp
  script/sign.cpp
)
target_link_libraries(tapyrus_common
  PRIVATE
    core_interface
    tapyrus_consensus
    tapyrus_util
    univalue
    secp256k1
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

include(InstallBinaryComponent)

if(ENABLE_WALLET)
  add_subdirectory(wallet)
endif()


# P2P and RPC server functionality used by `tapyrusd` and `tapyrus-qt` executables.
add_library(tapyrus_node STATIC EXCLUDE_FROM_ALL
  addrdb.cpp
  addrman.cpp
  blockencodings.cpp
  consensus/tx_verify.cpp
  dbwrapper.cpp
  httprpc.cpp
  httpserver.cpp
  index/base.cpp
  index/txindex.cpp
  init.cpp
  chain.cpp
  cs_main.cpp
  net.cpp
  net_processing.cpp
  chainstate.cpp
  coins.cpp
  miner.cpp
  timeoffsets.cpp
  primitives/transaction.cpp
  utxo_snapshot.cpp
  warnings.cpp
  noui.cpp
  policy/policy.cpp
  policy/fees.cpp
  policy/feerate.cpp
  policy/packages.cpp
  policy/rbf.cpp
  policy/packages.cpp
  rest.cpp
  rpc/blockchain.cpp
  rpc/mempool.cpp
  rpc/mining.cpp
  rpc/net.cpp
  rpc/rawtransaction.cpp
  rpc/server.cpp
  script/sigcache.cpp
  torcontrol.cpp
  txdb.cpp
  txmempool.cpp
  validation.cpp
  validationinterface.cpp
  $<$<TARGET_EXISTS:tapyrus_wallet>:wallet/init.cpp>
  $<$<NOT:$<TARGET_EXISTS:tapyrus_wallet>>:dummywallet.cpp>
)
target_link_libraries(tapyrus_node
  PRIVATE
    core_interface
    tapyrus_common
    tapyrus_util
    $<TARGET_NAME_IF_EXISTS:tapyrus_zmq>
    leveldb
    minisketch
    univalue
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:libevent::core>
    $<TARGET_NAME_IF_EXISTS:libevent::extra>
    $<TARGET_NAME_IF_EXISTS:libevent::pthreads>
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
)


# tapyrus Core tapyrusd.
if(BUILD_DAEMON)
  add_executable(tapyrusd
    tapyrusd.cpp
  )
  add_windows_resources(tapyrusd tapyrusd-res.rc)
  target_link_libraries(tapyrusd
    core_interface
    tapyrus_node
    $<TARGET_NAME_IF_EXISTS:tapyrus_wallet>
  )
  install_binary_component(tapyrusd HAS_MANPAGE)
endif()


add_library(tapyrus_cli STATIC EXCLUDE_FROM_ALL
  rpc/client.cpp
)
target_link_libraries(tapyrus_cli
  PUBLIC
    core_interface
    univalue
)


# tapyrus Core RPC client
if(BUILD_CLI)
  add_executable(tapyrus-cli tapyrus-cli.cpp)
  add_windows_resources(tapyrus-cli tapyrus-cli-res.rc)
  target_link_libraries(tapyrus-cli
    core_interface
    tapyrus_cli
    tapyrus_common
    tapyrus_util
    libevent::core
    libevent::extra
  )
  install_binary_component(tapyrus-cli HAS_MANPAGE)
endif()


if(BUILD_TX)
  add_executable(tapyrus-tx tapyrus-tx.cpp)
  add_windows_resources(tapyrus-tx tapyrus-tx-res.rc)
  target_link_libraries(tapyrus-tx
    core_interface
    tapyrus_common
    tapyrus_util
    univalue
  )
  install_binary_component(tapyrus-tx HAS_MANPAGE)
endif()


if(BUILD_GUI)
  add_subdirectory(qt)
endif()


if(BUILD_UTIL_CHAINSTATE)
  add_executable(tapyrus-chainstate
    tapyrus-chainstate.cpp
  )
  # TODO: The `SKIP_BUILD_RPATH` property setting can be deleted
  #       in the future after reordering Guix script commands to
  #       perform binary checks after the installation step.
  # Relevant discussions:
  # - https://github.com/hebasto/tapyrus/pull/236#issuecomment-2183120953
  # - https://github.com/tapyrus/tapyrus/pull/30312#issuecomment-2191235833
  set_target_properties(tapyrus-chainstate PROPERTIES
    SKIP_BUILD_RPATH OFF
  )
  target_link_libraries(tapyrus-chainstate
    PRIVATE
      core_interface
      tapyruskernel
  )
endif()


if(BUILD_BENCH)
  add_subdirectory(bench)
endif()

if(BUILD_TESTS)
  add_subdirectory(test)
endif()

if(BUILD_FUZZ_BINARY)
  add_subdirectory(test/fuzz)
endif()
