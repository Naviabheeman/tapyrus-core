// Copyright (c) 2021-2022 The Bitcoin Core developers
// Copyright (c) 2023 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/packages.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <txmempool.h>
#include <validation.h>
#include <uint256.h>
#include <numeric>

bool  CheckPackage(const Package& txns, CValidationState& state)
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

    // Make sure that none of the package transactions are in the mempool already
    for (const auto& tx : txns) {
        if(mempool.exists(tx->GetHashMalFix()))
            return state.Invalid(false, REJECT_PACKAGE_INVALID, "package-tx-in-mempool");
    }
    return true;
}


bool ArePackageTransactionsAccepted(const PackageValidationState& results)
{
    for (const auto& r : results) {
        if(r.second.IsInvalid() || r.second.IsError() || r.second.missingInputs) {
            return false;
        }
    }
    return true;
}