// Copyright (c) 2021-2022 The Bitcoin Core developers
// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TAPYRUS_POLICY_PACKAGES_H
#define TAPYRUS_POLICY_PACKAGES_H

#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <coins.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <txmempool.h>
//#include <validation.h>
#include <cstdint>
#include <vector>
#include <variant>

/** Default maximum number of transactions in a package. */
static constexpr uint32_t MAX_PACKAGE_COUNT{25};

// If a package is submitted, it must be within the mempool's ancestor/descendant limits. Since a
// submitted package must be child-with-unconfirmed-parents (all of the transactions are an ancestor
// of the child), package limits are ultimately bounded by mempool package limits. Ensure that the
// defaults reflect this constraint.
//static_assert(DEFAULT_DESCENDANT_LIMIT >= MAX_PACKAGE_COUNT);
//static_assert(DEFAULT_ANCESTOR_LIMIT >= MAX_PACKAGE_COUNT);

/** A package is an set of transactions. The transactions cannot conflict with (spend the
 * same inputs as) one another. */
using Package = std::vector<CTransactionRef>;

/** A list of all packages in the mempool but not in any block yet.
 * Tracking this helps in package eviction and accurate fee estimation.
 * */
using PackageInMempool = std::vector<CTxMemPoolEntry>;

/** Package validation results are actually the result of
 * validationg each transaction in the package 
 * Its stored in a map with transaction id as key.
 * */
using PackageValidationState = std::map<const uint256, const CValidationState >;

struct PackageCompareIteratorByHash {
    bool operator()(const uint256&a, const uint256 &b) const {
        return a < b;
    }
};

using packageEntries = std::set<uint256>;

/** A package that has been checked for package validity
 * It caches the set of inputs spent in the package and
 * a map of input txid to children within the package.
 * */
struct CCachedPackage: public std::vector<CTransactionRef> {
    std::unordered_set<COutPoint, SaltedOutpointHasher> inputs;
    std::map<const uint256, packageEntries> children;

    void CalculatePackageDescendants(const uint256 hashMalFix, packageEntries& setDescendants) const;
};

/**
 * CCoinsViewPackage is a CCoinsCache needed in package validation.
 * when each transaction in a package is being validated, the coins added by
 * a parent transaction inside the package is not available in any of the other caches.
 * In bitcoin this cache is accomodated within CCoinsViewMemPool.
 * In Tapyrus we make it a separate cache so that it is self managed and
 * it can use CCachedPackage class whenever possible
 */

class CCoinsViewPackage : public CCoinsViewMemPool
{
    /**
    * Coins made available by transactions being validated. Tracking these allows for package
    * validation, since we can access transaction outputs without submitting them to mempool.
    */
    CCoinsMap packageCoins;

    /** package */
    const CCachedPackage &packageCache;


public:

    CCoinsViewPackage(CCoinsView* baseIn, const CTxMemPool& mempoolIn, const CCachedPackage& pkg);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const;

    void AddCoin(const COutPoint &outpoint, Coin&& coin);

     /** Add the coins created by this transaction. These coins are only temporarily stored in
     * packageCoins and cannot be flushed to the cache in memory or db. Only used for package validation. */

    void AddCoins(const CTransactionRef& tx);


};


/** Context-free package policy checks:
 * 1. The number of transactions cannot exceed MAX_PACKAGE_COUNT.
 * 2. The total size cannot exceed  MAX_PACKAGE_COUNT * 1000
 * 3. If any dependencies exist between transactions, parents must appear before children.
 * 4. Transactions cannot conflict, i.e., spend the same inputs.
 * */
bool CheckPackage(const Package& txns, CValidationState& state, PackageValidationState& results, CCachedPackage& cachedPackage);



#endif // TAPYRUS_POLICY_PACKAGES_H
