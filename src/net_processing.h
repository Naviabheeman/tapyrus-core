// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include <net.h>
#include <validationinterface.h>
#include <consensus/params.h>

/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Default number of orphan+recently-replaced txn to keep around for block reconstruction */
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;
/** Maximum number of outstanding CMPCTBLOCK requests for the same block. */
static const unsigned int MAX_CMPCTBLOCKS_INFLIGHT_PER_BLOCK = 3;

class PeerLogicValidation final : public CValidationInterface, public NetEventsInterface {
private:
    CConnman* const connman;

public:
    explicit PeerLogicValidation(CConnman* connman, CScheduler &scheduler);

    /**
     * Overridden from CValidationInterface.
     */
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    /**
     * Overridden from CValidationInterface.
     */
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    /**
     * Overridden from CValidationInterface.
     */
    void BlockChecked(const CBlock& block, const CValidationState& state) override;
    /**
     * Overridden from CValidationInterface.
     */
    void NewValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &pblock) override;

    /** Initialize a peer by adding it to mapNodeState and pushing a message requesting its version */
    void InitializeNode(CNode* pnode) override;
    /** Handle removal of a peer by updating various state and removing it from mapNodeState */
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) override;
    /**
    * Process protocol messages received from a given node
    *
    * @param[in]   pfrom           The node which we have received messages from.
    * @param[in]   interrupt       Interrupt condition for processing threads
    */
    bool ProcessMessages(CNode* pfrom, std::atomic<bool>& interrupt) override;
    /**
    * Send queued protocol messages to be sent to a give node.
    *
    * @param[in]   pto             The node which we are sending messages to.
    * @return                      True if there is more work to be done
    */
    bool SendMessages(CNode* pto) override;

    /** Consider evicting an outbound peer based on the amount of time they've been behind our tip */
    void ConsiderEviction(CNode *pto, int64_t time_in_seconds);
    /** Evict extra outbound peers. If we think our tip may be stale, connect to an extra outbound */
    void CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams);
    /** If we have extra outbound peers, try to disconnect the one with the oldest block announcement */
    void EvictExtraOutboundPeers(int64_t time_in_seconds);

private:
    int64_t m_stale_tip_check_time; //! Next time to check for stale tip

};

struct CNodeStateStats {
    int nMisbehavior = 0;
    int nSyncHeight = -1;
    int nCommonHeight = -1;
    std::vector<int> vHeightInFlight;
    uint64_t m_addr_processed = 0;
    uint64_t m_addr_rate_limited = 0;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);

/** Relay a transaction to all our peers*/
void RelayTransaction(const CTransaction& tx, CConnman* connman);

#endif // BITCOIN_NET_PROCESSING_H
