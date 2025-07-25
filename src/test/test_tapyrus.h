// Copyright (c) 2015-2018 The Bitcoin Core developers
// Copyright (c) 2019-2020 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TAPYRUS_TEST_TEST_TAPYRUS_H
#define TAPYRUS_TEST_TEST_TAPYRUS_H

#include <federationparams.h>
#include <chainparams.h>
#include <fs.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <scheduler.h>
#include <txdb.h>
#include <txmempool.h>
#include <consensus/consensus.h>

#include <memory>

#include <test/test_keys_helper.h>

/* CXFieldHistoryWithReset class is created to allow reset function in test_tapyrus
 * this functionality is necessary in xfield history tests to reset the xfield history map to the genesis block state.
 */
class CXFieldHistoryWithReset : public CXFieldHistory {
    const CBlock& genesis;
public:
    CXFieldHistoryWithReset(const CBlock& block) : CXFieldHistory(block), genesis(block) {}
    void Reset();
    virtual ~CXFieldHistoryWithReset() {}
};

extern uint256 insecure_rand_seed;
extern FastRandomContext insecure_rand_ctx;

static inline void SeedInsecureRand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint32_t InsecureRand32() { return insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return insecure_rand_ctx.randbool(); }

//utility functions
CBlock getBlock();
std::string getSignedTestBlock();
std::string getTestGenesisBlockHex(const CPubKey& aggregatePubkey,
                                   const CKey& aggregatePrivkey);
void writeTestGenesisBlockToFile(fs::path genesisPath, std::string genesisFileName="");
void createSignedBlockProof(CBlock &block, std::vector<unsigned char>& blockProof);
// define an implicit conversion here so that uint256 may be used directly in BOOST_CHECK_*
std::ostream& operator<<(std::ostream& os, const uint256& num);

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit BasicTestingSetup(const std::string& chainName = TAPYRUS_MODES::PROD);
    ~BasicTestingSetup();

    fs::path SetDataDir(const std::string& name);
    fs::path GetDataDir();
private:
    const fs::path m_path_root;
protected:
    CXFieldHistoryWithReset* pxFieldHistory;
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads setup.
 */
class CConnman;
class CNode;
struct CConnmanTest {
    static void AddNode(CNode& node);
    static void ClearNodes();
};

class PeerLogicValidation;
struct TestingSetup: public BasicTestingSetup {
    CConnman* connman;
    CScheduler scheduler;
    std::unique_ptr<PeerLogicValidation> peerLogic;

    explicit TestingSetup(const std::string& chainName = TAPYRUS_MODES::PROD);
    ~TestingSetup();
};

class CBlock;
struct CMutableTransaction;
class CScript;

//
// Testing fixture that pre-creates a
// 5-block REGTEST-mode block chain
//
struct TestChainSetup : public TestingSetup {
    TestChainSetup();

    // Create a new block with just given transactions, coinbase paying to
    // scriptPubKey, and try to add it to the current chain.
    CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns,
                                 const CScript& scriptPubKey);

    //creates empty blocks and fills m_coinbase_txns with the new txids
    void refillCoinbase(int count = 10);

    ~TestChainSetup(){}

    std::vector<CTransactionRef> m_coinbase_txns; // For convenience, coinbase transactions
    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
};

class CTxMemPoolEntry;

struct TestMemPoolEntryHelper
{
    // Default values
    CAmount nFee;
    int64_t nTime;
    unsigned int nHeight;
    bool spendsCoinbase;
    unsigned int sigOpCost;
    LockPoints lp;

    TestMemPoolEntryHelper() :
        nFee(0), nTime(0), nHeight(1),
        spendsCoinbase(false), sigOpCost(1) { }

    CTxMemPoolEntry FromTx(const CMutableTransaction& tx);
    CTxMemPoolEntry FromTx(const CTransactionRef& tx);

    // Change the default value
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(unsigned int _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
};

#endif //TAPYRUS_TEST_TEST_TAPYRUS_H
