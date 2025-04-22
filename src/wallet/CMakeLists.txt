# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

# Wallet functionality used by bitcoind and bitcoin-wallet executables.
add_library(tapyrus_wallet STATIC EXCLUDE_FROM_ALL
  coincontrol.cpp
  coinselection.cpp
  crypter.cpp
  db.cpp
  feebumper.cpp
  fees.cpp
  wallet.cpp
  walletdb.cpp
  walletutil.cpp
)
target_link_libraries(tapyrus_wallet
  PRIVATE
    core_interface
    tapyrus_common
    univalue
    Boost::headers
    $<TARGET_NAME_IF_EXISTS:USDT::headers>
)

if(USE_BDB)
  target_sources(tapyrus_wallet PRIVATE bdb.cpp salvage.cpp)
  target_link_libraries(tapyrus_wallet PUBLIC BerkeleyDB::BerkeleyDB)
endif()
