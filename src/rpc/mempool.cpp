// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <consensus/validation.h>
#include <policy/packages.h>
#include <rpc/mempool.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <txmempool.h>
#include <utilstrencodings.h>
#include <mempooloptions.h>
#include <validation.h>
#include <validationinterface.h>

#include <stdint.h>

#include <univalue.h>
#include <logging.h>

static void encodePackageResult(bool success, const PackageValidationState& pkg_results, CValidationState state, UniValue& result)
{
    if(success && ArePackageTransactionsAccepted(pkg_results)) {
        for (const auto& r : pkg_results) {
            UniValue result_0(UniValue::VOBJ);
            result_0.pushKV("allowed", true);
            result.pushKV(r.first.GetHex(), result_0);
        }
    }
    else if(state.IsInvalid() || state.IsError()) {
        result.pushKV("allowed", false);
        result.pushKV("reject-reason", strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
    } else {
        for (const auto& r : pkg_results) {
            UniValue result_0(UniValue::VOBJ);
            if(r.second.IsInvalid() || r.second.IsError()) {
                result_0.pushKV("allowed", false);
                result_0.pushKV("reject-reason", strprintf("%i: %s", r.second.GetRejectCode(), r.second.GetRejectReason()));
            } else if (r.second.missingInputs && r.second.IsValid()) {
                result_0.pushKV("allowed", false);
                result_0.pushKV("reject-reason", "missing-inputs");
            } else
                result_0.pushKV("allowed", true);
            result.pushKV(r.first.GetHex(), result_0);
        }
    }
}

bool TestPackageAcceptance(const Package& package,
                                  CValidationState& state,
                                  PackageValidationState& results,
                                  CTxMempoolAcceptanceOptions& opt)
{


    bool all_valid = true;
    bool test_accept_res = false;

    // testmempool acceptance first
    for(auto &tx : package) {
        {
            opt.state = CValidationState();
            LOCK(::cs_main);
            test_accept_res = AcceptToMemoryPool(tx, opt);
        }

        opt.state.missingInputs = opt.missingInputs.size() > 0;
        results.emplace(tx->GetHashMalFix(), opt.state);
        all_valid &= test_accept_res;
    }

    return all_valid;
}

bool SubmitPackageToMempool(const PackageInMempool& validPool, CValidationState& state)
{
    for(auto &entry : validPool) {
        auto &tx(entry.GetTx());
        {
            // Store transaction in mempool if validation succeeded
            LOCK(::cs_main);
            LOCK(mempool.cs);
            mempool.addUnchecked(tx.GetHashMalFix(), entry, false);
        }

        //signlaing for gui, wallet etc
        GetMainSignals().TransactionAddedToMempool(entry.GetSharedTx());
    }


    LimitMempoolSize(mempool, gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000, gArgs.GetArg("-mempoolexpiry", DEFAULT_MEMPOOL_EXPIRY) * 60 * 60);

    // check if any of the package transaction were evited because of a full mempool
    // if so remove all package transactions from mempool and return error
    for(auto &entry : validPool) {
        auto &tx(entry.GetTx());
        if (!mempool.exists(tx.GetHashMalFix()))
        {
            RemovePackageFromMempool(validPool);
            return state.DoS(0, false, REJECT_PACKAGE_MEMPOOL, "mempool full");
        }
    }

    return true;
}
// remove package transaction from mempool if it was rejected due to a full mempool
void RemovePackageFromMempool(const PackageInMempool& validPool)
{
    for(auto &entry : validPool) {
        auto &tx(entry.GetTx());
        mempool.removeRecursive(tx, MemPoolRemovalReason::PACKAGE);
    }
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


static UniValue testmempoolaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            // clang-format off
            "testmempoolaccept [\"rawtxs\"] ( allowhighfees )\n"
            "\nReturns if raw transaction (serialized, hex-encoded) would be accepted by mempool.\n"
            "\nThis checks if the transaction violates the consensus or policy rules.\n"
            "\nSee sendrawtransaction call.\n"
            "\nArguments:\n"
            "1. [\"rawtxs\"]        (array, required) An array of hex strings of raw transactions.\n"
            "2. allowhighfees    (boolean, optional, default=false) Allow high fees\n"
            "\nResult:\n"
            "  \"allowed\"            (boolean) If the mempool allows this tx to be inserted\n"
            "  \"reject-reason\"  (string) Rejection string (only present when 'allowed' is false)\n"
            "[                   (array) The result of the mempool acceptance test for each raw transaction in the input array. Length is equal to the number of input transactions."
            " {\n"
            "  \"<txid>:\"                  (string) The transaction id in hex\n"
            "   {\n"
            "    \"allowed\"               (boolean) If the mempool allows this tx to be inserted\n"
            "    \"reject-reason\"     (string) Rejection string (only present when 'allowed' is false)\n"
            "   }\n"
            " }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("testmempoolaccept", "\"signedhex\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("testmempoolaccept", "[\"signedhex\"]")
            // clang-format on
            );
    }

    RPCTypeCheck(request.params, {UniValue::VARR, UniValue::VBOOL});
    const UniValue raw_transactions = request.params[0].get_array();
    if (raw_transactions.size() < 1 || raw_transactions.size() > MAX_PACKAGE_COUNT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                            "Too many transactions in package." );
    }

    std::vector<CTransaction> transactions;
    for (const auto& rawtx : raw_transactions.getValues()) {
        CMutableTransaction mtx;
        if (!DecodeHexTx(mtx, rawtx.get_str())) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                "TX decode failed: " + rawtx.get_str() + " Make sure the tx has at least one input.");
        }
        transactions.push_back(std::move(mtx));
    }

    CAmount max_raw_tx_fee = ::maxTxFee;
    if (!request.params[1].isNull() && request.params[1].get_bool()) {
        max_raw_tx_fee = 0;
    }

    //make a package with only unknown transactions i.e. omit transactions which are already in the mempool
    Package package;
    CValidationState state;
    PackageValidationState pkg_results;
    CCachedPackage chachedPackage;

    if(!CheckPackage(package, state, pkg_results, chachedPackage))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "package acceptance failed: " + state.Describe());
    }

    std::vector<CTxMemPoolEntry> submitPool;
    submitPool.reserve(package.size());

    CTxMempoolAcceptanceOptions opt(chachedPackage);
    opt.flags = MempoolAcceptanceFlags::TEST_ONLY;
    opt.flags |= MempoolAcceptanceFlags::BYPASSS_LIMITS;
    opt.submitPool = &submitPool;
    opt.nAbsurdFee = max_raw_tx_fee;

    bool success = TestPackageAcceptance(package, state, pkg_results, opt);

    UniValue result(UniValue::VOBJ);
    encodePackageResult(success, pkg_results, state, result);
    return result;
}

static UniValue submitpackage(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
    throw std::runtime_error(
        "submitpackage \"[rawtx1, rawtx2]\" \n"
        "\nSubmit a package of raw transactions (serialized, hex-encoded) to local node \n"
        "\nArguments:\n"
        "1. transaction_hex                        (array, required) An array of hex strings of raw transactions.\n"
        "2. allowhighfees                            (boolean, optional, default=false) Allow high fees\n"
        "\nResult:\n"
        "   {\n"
        "    \"allowed\"               (boolean) If submitting all transaction was successful \n"
        "   }\n"
        "\nExamples:\n"
        + HelpExampleCli("submitpackage", "\"raw_tx1, raw_tx2\"") +
        "\nAs a json rpc call\n"
        + HelpExampleRpc("submitpackage", "[\"raw_tx1, raw_tx2\"]")
    );

    RPCTypeCheck(request.params, {UniValue::VARR, UniValue::VBOOL});

    const UniValue raw_transactions = request.params[0].get_array();
    if (raw_transactions.size() < 1 || raw_transactions.size() > MAX_PACKAGE_COUNT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                            "Too many transactions in package");
    }

    std::vector<CTransaction> transactions;
    for (const auto& rawtx : raw_transactions.getValues()) {
        CMutableTransaction mtx;
        if (!DecodeHexTx(mtx, rawtx.get_str())) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR,
                                "TX decode failed: " + rawtx.get_str() + " Make sure the tx has at least one input.");
        }
        transactions.push_back(std::move(mtx));
    }

    CAmount max_raw_tx_fee = ::maxTxFee;
    if (!request.params[1].isNull() && request.params[1].get_bool()) {
        max_raw_tx_fee = 0;
    }

    //make a package with only unknown transactions i.e. omit transactions which are already in the mempool
    Package package;
    CValidationState state;
    PackageValidationState pkg_results;
    CCachedPackage chachedPackage;

    if(!CheckPackage(package, state, pkg_results, chachedPackage))
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                    "package acceptance failed: " + state.Describe());
    }

    std::vector<CTxMemPoolEntry> submitPool;
    submitPool.reserve(package.size());

    CTxMempoolAcceptanceOptions opt(chachedPackage);
    opt.flags = MempoolAcceptanceFlags::TEST_ONLY;
    opt.flags |= MempoolAcceptanceFlags::BYPASSS_LIMITS;
    opt.submitPool = &submitPool;
    opt.nAbsurdFee = max_raw_tx_fee;

    bool success = TestPackageAcceptance(package, state, pkg_results, opt);

    if(success && ArePackageTransactionsAccepted(pkg_results))
    {
        success = SubmitPackageToMempool(submitPool, state);
    }

    UniValue result(UniValue::VOBJ);
    encodePackageResult(success, pkg_results, state, result);
    return result;
}


static const CRPCCommand commands[] =
{ //  category              name                            actor (function)            argNames
  //  --------------------- ------------------------        -----------------------     ----------
    { "packages",        "testmempoolaccept",           &testmempoolaccept,         {"rawtxs","allowhighfees"} },
    { "packages",           "submitpackage",                    &submitpackage,             {"rawtxs","allowhighfees"} },
};

void RegisterMempoolRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
