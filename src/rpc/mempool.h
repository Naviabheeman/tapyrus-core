// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TAPYRUS_RPC_MEMPOOL_H
#define TAPYRUS_RPC_MEMPOOL_H

#include <policy/packages.h>
#include <txmempool.h>
#include <mempooloptions.h>

bool TestPackageAcceptance(const Package& package,
                                  CValidationState& state,
                                  PackageValidationState& results,
                                  CTxMempoolAcceptanceOptions& opt);

bool SubmitPackageToMempool(const PackageInMempool& validPool, CValidationState& state);

void RemovePackageFromMempool(const PackageInMempool& validPool);

bool ArePackageTransactionsAccepted(const PackageValidationState& results);

#endif // TAPYRUS_RPC_MEMPOOL_H
