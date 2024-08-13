// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TAPYRUS_RPC_MEMPOOL_H
#define TAPYRUS_RPC_MEMPOOL_H

#include <policy/packages.h>
#include <txmempool.h>
#include <mempooloptions.h>

MempoolPackage TestPackageAcceptance(const Package& transactions,
                                  const CAmount max_raw_tx_fee,
                                  CValidationState& state,
                                  PackageValidationState& pkg_results);

bool SubmitPackageToMempool(const MempoolPackage& mempoolPkg, CValidationState& state);

void RemovePackageFromMempool(const MempoolPackage& mempoolPkg);

bool ArePackageTransactionsAccepted(const PackageValidationState& results);

#endif // TAPYRUS_RPC_MEMPOOL_H
