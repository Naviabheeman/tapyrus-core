// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTXO_SNAPSHOT_H
#define BITCOIN_UTXO_SNAPSHOT_H

#include <cs_main.h>
#include <serialize.h>
#include <sync.h>
#include <uint256.h>
#include <fs.h>
#include <xfieldhistory.h>



//! Metadata describing a serialized version of a UTXO set from which an
//! assumeutxo Chainstate can be constructed.
class SnapshotMetadata
{
public:
    //! The network id associated with this snapshot
    uint64_t networkid;

    //! The hash of the block that reflects the tip of the chain for the
    //! UTXO set contained in this snapshot.
    uint256 base_blockhash;

    //! The number of coins in the UTXO set contained in this snapshot. Used
    //! during snapshot load to estimate progress of UTXO set reconstruction.
    uint64_t coins_count = 0;

    SnapshotMetadata() { };

    SnapshotMetadata(const uint256& base_blockhash, uint64_t coins_count);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(networkid);
        READWRITE(base_blockhash);
        READWRITE(VARINT(coins_count));
    }
};

#endif // BITCOIN_UTXO_SNAPSHOT_H
