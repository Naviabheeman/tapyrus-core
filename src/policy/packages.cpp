// Copyright (c) 2021-2023 The Bitcoin Core developers
// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/packages.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <txmempool.h>
#include <validation.h>
#include <validationinterface.h>
#include <uint256.h>
#include <numeric>
#include <serialize.h>
#include <clientversion.h>

bool CheckPackage(const Package& txns, CValidationState& state)
{
    const unsigned int package_count = txns.size();

    if (package_count > MAX_PACKAGE_COUNT) {
        return state.Invalid(false, REJECT_PACKAGE_INVALID, "package-too-many-transactions");
    }    const int64_t total_size = std::accumulate(txns.cbegin(), txns.cend(), 0,
                               [](int64_t sum, const auto& tx) { return sum + GetTransactionSize(*tx); });

    // If the package only contains 1 tx, it's better to report the policy violation on individual tx size.
    if (package_count > 1 && total_size > MAX_PACKAGE_COUNT * 1000) {
        return state.Invalid(false, REJECT_PACKAGE_INVALID, "package-too-large");
    }

    // Require the package to be sorted in order of dependency, i.e. parents appear before children.
    // An unsorted package will fail anyway on missing-inputs, but it's better to quit earlier and
    // fail on something less ambiguous (missing-inputs could also be an orphan or trying to
    // spend nonexistent coins).
    std::unordered_set<uint256, SaltedTxidHasher> later_txids;
    std::transform(txns.cbegin(), txns.cend(), std::inserter(later_txids, later_txids.end()),
                   [](const auto& tx) { return tx->GetHashMalFix(); });

    // Package must not contain any duplicate transactions
    if (later_txids.size() != txns.size()) {
        return state.Invalid(false, REJECT_PACKAGE_INVALID, "package-contains-duplicates");
    }

    for (const auto& tx : txns) {
        for (const auto& input : tx->vin) {
            if (later_txids.find(input.prevout.hashMalFix) != later_txids.end()) {
                // The parent is a subsequent transaction in the package.
                return state.Invalid(false, REJECT_PACKAGE_INVALID, "package-not-sorted");
            }
        }
        later_txids.erase(tx->GetHashMalFix());
    }

    // Don't allow any conflicting transactions, i.e. spending the same inputs, in a package.
    std::unordered_set<COutPoint, SaltedOutpointHasher> inputs_seen;
    for (const auto& tx : txns) {
        for (const auto& input : tx->vin) {
            if (inputs_seen.find(input.prevout) != inputs_seen.end()) {
                // This input is also present in another tx in the package.
                return state.Invalid(false, REJECT_PACKAGE_INVALID, "conflict-in-package");
            }
        }
        // Batch-add all the inputs for a tx at a time. If we added them 1 at a time, we could
        // catch duplicate inputs within a single tx.  This is a more severe, consensus error,
        // and we want to report that from CheckTransaction instead.
        std::transform(tx->vin.cbegin(), tx->vin.cend(), std::inserter(inputs_seen, inputs_seen.end()),
                       [](const auto& input) { return input.prevout; });
    }
    return true;
}

bool TransformPackage(const Package& txns, CCachedPackage& cachedPackage, PackageValidationState& results) {
    // Don't allow any mempool conflicting transactions
    std::vector<CTransactionRef> duplicates;

    for (const auto& tx : txns) {
        if (mempool.exists(tx->GetHashMalFix())) {
            CValidationState state;
            state.Invalid(false, REJECT_DUPLICATE, "txn-already-in-mempool");
            results.emplace(tx->GetHashMalFix(), state);
            duplicates.emplace_back(tx);
        } else {
            // If not a duplicate, add to cachedPackage
            cachedPackage.emplace_back(tx);
        }
    }

    // Identify the parent-child relationships within the package
    for (const auto& intx : cachedPackage) {
        packageEntries allchildren;

        for (const auto& spendtx : cachedPackage) {
            for (const auto& in : spendtx->vin) {
                if (intx->GetHashMalFix() == in.prevout.hashMalFix) {
                    allchildren.emplace(spendtx->GetHashMalFix()); // Add child transaction hash
                    cachedPackage.inputs.emplace(in.prevout); // Ensure in is of the correct type
                }
            }
        }

        // Only add to children if there are any children found
        if (!allchildren.empty()) {
            cachedPackage.children.emplace(intx->GetHashMalFix(), std::move(allchildren));
        }

        // Log the transaction and its children
        LogPrintf("CheckPackage: tx: %s children:\n", intx->GetHashMalFix().ToString());
        for (const auto& x : allchildren) {
            LogPrintf("\t\t %s\n", x.GetHex());
        }
    }

    return true;
}

CCoinsViewPackage::CCoinsViewPackage(CCoinsView* baseIn, const CTxMemPool& mempoolIn, const CCachedPackage& packageIn) : CCoinsViewMemPool(baseIn, mempoolIn), packageCache(packageIn) {

    for(auto &tx:packageCache)
    {
        ::CCoinsViewPackage::AddCoins(tx);
        LogPrintf("CCoinsViewPackage:: tx: %s\n", tx->GetHashMalFix().ToString());
    }

}

bool CCoinsViewPackage::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    // Check to see if the inputs are made available by another tx in the package.
    // These Coins would not be available in the underlying CoinsView.
    CCoinsMap::const_iterator it = packageCoins.find(outpoint);
    if (it != packageCoins.end()) {
        coin = it->second.coin;
        LogPrintf(" CCoinsViewPackage::GetCoin in packageCoins: %s, %d, %d\n", coin.out.ToString().c_str(), coin.nHeight, coin.IsSpent());

        return !coin.IsSpent();
    }
    LogPrintf(" CCoinsViewPackage::GetCoin not in packageCoins\n");

    return base->GetCoin(outpoint, coin);
}

void CCoinsViewPackage::AddCoin(const COutPoint &outpoint, Coin&& coin)
{
    assert(!coin.IsSpent());
    if (coin.out.scriptPubKey.IsUnspendable()) return;
    CCoinsMap::iterator it;
    bool inserted;
    std::tie(it, inserted) = packageCoins.emplace(std::piecewise_construct, std::forward_as_tuple(outpoint), std::tuple<>());
    bool fresh = false;

    it->second.coin = std::move(coin);
    it->second.flags |= CCoinsCacheEntry::DIRTY | CCoinsCacheEntry::FRESH;

    LogPrintf("CCoinsViewPackage::AddCoin: %s, %d, %d\n", coin.out.ToString().c_str(), coin.nHeight, coin.IsSpent());
}

void CCoinsViewPackage::AddCoins(const CTransactionRef& tx) {
    bool fCoinbase = tx->IsCoinBase();
    const uint256& txid = tx->GetHashMalFix();
    for (size_t i = 0; i < tx->vout.size(); ++i) {
        AddCoin(COutPoint(txid, i), Coin(tx->vout[i], 0, fCoinbase));
    }
}

void CCachedPackage::CalculatePackageDescendants(const uint256 hashMalFix, packageEntries& setDescendants) const
{
    packageEntries stage;
    if (setDescendants.count(hashMalFix) == 0) {
        stage.insert(hashMalFix);
    }
    // Traverse down the children of entry, only adding children that are not
    // accounted for in setDescendants already (because those children have either
    // already been walked, or will be walked in this iteration).
    while (!stage.empty()) {
        const uint256 currentHash = *stage.begin();
        setDescendants.insert(currentHash);
        stage.erase(currentHash);

        // Get the children of the current transaction
        auto it = children.find(currentHash);
        if (it != children.end()) {
            for (const auto& childtxid : it->second) {
                LogPrintf("CalculatePackageDecendants:GetMemPoolChildren %s nConflictingSize: %d\n", childtxid.ToString(), it->second.size());

                // Only add child if it is not already in setDescendants
                if (setDescendants.insert(childtxid).second) { // insert returns a pair, second is true if inserted
                    stage.insert(childtxid);
                    LogPrintf("CalculatePackageDecendants: insert %s\n", childtxid.ToString());
                }
            }
        }
    }
}

