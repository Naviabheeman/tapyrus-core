// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MEMPOOLOPTIONS_H
#define BITCOIN_MEMPOOLOPTIONS_H

#include <vector>
#include <variant>
#include <primitives/transaction.h>
#include <policy/packages.h>
#include <coins.h>

/* Options to change the behaviour of Accept to mempool
* these are intended to be consolidated in one integer as flag
* int flags = BYPASSS_LIMITS | TEST_ONLY
*/
enum class MempoolAcceptanceFlags
{
    NONE = 0,
    BYPASSS_LIMITS = 1,
    TEST_ONLY = 2
};

inline MempoolAcceptanceFlags operator|=(MempoolAcceptanceFlags a, MempoolAcceptanceFlags b)
{
    return static_cast<MempoolAcceptanceFlags>(static_cast<int>(a) | static_cast<int>(b));
}

inline MempoolAcceptanceFlags operator&=(MempoolAcceptanceFlags a, MempoolAcceptanceFlags b)
{
    return static_cast<MempoolAcceptanceFlags>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool operator|(MempoolAcceptanceFlags a, MempoolAcceptanceFlags b)
{
    return ((static_cast<int>(a) | static_cast<int>(b)) > 0);
}

inline bool operator&(MempoolAcceptanceFlags a, MempoolAcceptanceFlags b)
{
    return ((static_cast<int>(a) & static_cast<int>(b)) > 0);
}

/* TO identify if tx validation is stand alone or part of a bigger entity 
 * (block, index, package etc) only package is implemented now.
 */
using ValidationContext = std::variant<const CTransactionRef,
                                       const CCachedPackage>;

bool IsPackage(const ValidationContext& context) {
    return context.index() == 1;
}
bool IsTransaction(const ValidationContext& context) {
    return context.index() == 0;
}


/* All configurable inputs and outputs of accept to mempool are consolidated here for ease of use*/
struct CTxMempoolAcceptanceOptions {
    ValidationContext context;
    MempoolAcceptanceFlags flags;
    CAmount nAbsurdFee;
    CValidationState state;
    int64_t nAcceptTime;
    CCoinsViewMemPool* viewCoins;
    std::vector<CTransactionRef> txnReplaced;
    std::vector<COutPoint> coins_to_uncache;
    std::vector<COutPoint> missingInputs;
    std::vector<CTxMemPoolEntry >* submitPool;

    CTxMempoolAcceptanceOptions(ValidationContext& context);
    explicit CTxMempoolAcceptanceOptions(CTransactionRef tx);
    explicit CTxMempoolAcceptanceOptions(const CCachedPackage& pkg);
    ~CTxMempoolAcceptanceOptions() {
        delete viewCoins;
        viewCoins = nullptr;
    }
};

#endif // BITCOIN_MEMPOOLOPTIONS_H
