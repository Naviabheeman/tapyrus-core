// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019-2022 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chain.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <httpserver.h>
#include <validation.h>
#include <key_io.h>
#include <net.h>
#include <outputtype.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <rpc/mining.h>
#include <rpc/protocol.h>
#include <rpc/rawtransaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/sign.h>
#include <shutdown.h>
#include <util.h>
#include <utilmoneystr.h>
#include <wallet/coincontrol.h>
#include <wallet/feebumper.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>
#include <xfieldhistory.h>
#include <file_io.h>

#include <stdint.h>

#include <univalue.h>

#include <functional>

static const std::string WALLET_ENDPOINT_BASE = "/wallet/";

bool GetWalletNameFromJSONRPCRequest(const JSONRPCRequest& request, std::string& wallet_name)
{
    if (request.URI.substr(0, WALLET_ENDPOINT_BASE.size()) == WALLET_ENDPOINT_BASE) {
        // wallet endpoint was used
        wallet_name = urlDecode(request.URI.substr(WALLET_ENDPOINT_BASE.size()));
        return true;
    }
    return false;
}

std::shared_ptr<CWallet> GetWalletForJSONRPCRequest(const JSONRPCRequest& request)
{
    std::string wallet_name;
    if (GetWalletNameFromJSONRPCRequest(request, wallet_name)) {
        std::shared_ptr<CWallet> pwallet = GetWallet(wallet_name);
        if (!pwallet) throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Requested wallet does not exist or is not loaded");
        return pwallet;
    }

    std::vector<std::shared_ptr<CWallet>> wallets = GetWallets();
    return wallets.size() == 1 || (request.fHelp && wallets.size() > 0) ? wallets[0] : nullptr;
}

std::string HelpRequiringPassphrase(CWallet * const pwallet)
{
    return pwallet && pwallet->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(CWallet * const pwallet, bool avoidException)
{
    if (pwallet) return true;
    if (avoidException) return false;
    if (!HasWallets()) {
        throw JSONRPCError(
            RPC_METHOD_NOT_FOUND, "Method not found (wallet method is disabled because no wallet is loaded)");
    }
    throw JSONRPCError(RPC_WALLET_NOT_SPECIFIED,
        "Wallet file not specified (must request wallet RPC through /wallet/<filename> uri-path).");
}

void EnsureWalletIsUnlocked(CWallet * const pwallet)
{
    if (pwallet->IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }
}

static void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    entry.pushKV("confirmations", confirms);
    if (wtx.IsCoinBase())
        entry.pushKV("generated", true);
    if (confirms > 0)
    {
        AssertLockHeld(cs_main);
        entry.pushKV("blockhash", wtx.hashBlock.GetHex());
        entry.pushKV("blockindex", wtx.nIndex);
        entry.pushKV("blocktime", LookupBlockIndex(wtx.hashBlock)->GetBlockTime());
    } else {
        entry.pushKV("trusted", wtx.IsTrusted());
    }
    uint256 hash = wtx.GetHash();
    entry.pushKV("txid", hash.GetHex());
    UniValue conflicts(UniValue::VARR);
    for (const uint256& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.pushKV("walletconflicts", conflicts);
    entry.pushKV("time", wtx.GetTxTime());
    entry.pushKV("timereceived", (int64_t)wtx.nTimeReceived);

    // Add opt-in RBF status
    std::string rbfStatus = "no";
    if (confirms <= 0) {
        LOCK(mempool.cs);
        RBFTransactionState rbfState = IsRBFOptIn(*wtx.tx, mempool);
        if (rbfState == RBFTransactionState::UNKNOWN)
            rbfStatus = "unknown";
        else if (rbfState == RBFTransactionState::REPLACEABLE_BIP125)
            rbfStatus = "yes";
    }
    entry.pushKV("bip125-replaceable", rbfStatus);

    for (const std::pair<const std::string, std::string>& item : wtx.mapValue)
        entry.pushKV(item.first, item.second);
}

static std::string LabelFromValue(const UniValue& value)
{
    std::string label = value.get_str();
    if (label == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_LABEL_NAME, "Invalid label name");
    return label;
}

static void addTokenKV(const CTxDestination& address, const CAmount nAmount, UniValue& entry)
{
    ColorIdentifier colorId;
    if(address.index() == 3)
        colorId = std::get<CColorKeyID>(address).color;
    else if(address.index() == 4)
        colorId = std::get<CColorScriptID>(address).color;

    entry.pushKV("token", colorId.toHexString());

    entry.pushKV("amount", (colorId.type == TokenTypes::NONE ? ValueFromAmount(nAmount) : nAmount ));
}

static bool checkColorIdParam(const UniValue& param, ColorIdentifier& colorId) {
    try {
        const std::vector<unsigned char> vColorId(ParseHex(param.get_str()));
        if(vColorId.size() != 33) return false;
        colorId = ColorIdentifier(vColorId);
        if(colorId.type == TokenTypes::NONE) return false;
    }
    catch(...) {
        return false;
    }
    return true;
}

static UniValue getnewaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "getnewaddress ( \"label\" \"color\" )\n"
            "\nReturns a new Tapyrus address for receiving payments.\n"
            "If 'label' is specified, it is added to the address book \n"
            "so payments received with the address will be associated with 'label'.\n"
            "\nArguments:\n"
            "1. \"label\"          (string, optional) The label name for the address to be linked to. If not provided, the default label \"\" is used. It can also be set to the empty string \"\" to represent the default label. The label does not need to exist, it will be created if there is no label by the given name.\n"
            "2. \"color\"          (string, optional) For new token addresses the token or color.\n"
            "\nResult:\n"
            "\"address\"    (string) The new Tapyrus address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleCli("getnewaddress", "\"\", \"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\"")
            + HelpExampleRpc("getnewaddress", "\"\", \"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\"")
        );

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    // Parse the label first so we don't generate a key if there's an error
    std::string label;
    if (!request.params[0].isNull())
        label = LabelFromValue(request.params[0]);

    ColorIdentifier colorId;
    if (!request.params[1].isNull() && !checkColorIdParam(request.params[1], colorId))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color parameter.");

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwallet->GetKeyFromPool(newKey)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    CTxDestination dest;
    OutputType output_type = pwallet->m_default_address_type;

    if (colorId.type == TokenTypes::NONE)
        dest = GetDestinationForKey(newKey, output_type);
    else
        dest = CColorKeyID(newKey.GetID(), colorId);

    pwallet->SetAddressBook(dest, label, "receive");

    return EncodeDestination(dest);
}

CTxDestination GetLabelDestination(CWallet* const pwallet, const std::string& label, bool bForceNew=false)
{
    CTxDestination dest;
    if (!pwallet->GetLabelDestination(dest, label, bForceNew)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    return dest;
}


static UniValue getrawchangeaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getrawchangeaddress ( \"color\" )\n"
            "\nReturns a new Tapyrus address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nArguments:\n"
            "1. \"color\"         (string, optional) For new token addresses the token or color.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    ColorIdentifier colorId;
    if (!request.params[0].isNull() && !checkColorIdParam(request.params[0], colorId))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color parameter.");

    CReserveKey reservekey(pwallet);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey, true))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CTxDestination dest;
    OutputType output_type = pwallet->m_default_change_type;
    if (colorId.type == TokenTypes::NONE)
        dest = GetDestinationForKey(vchPubKey, output_type);
    else
        dest = CColorKeyID(vchPubKey.GetID(), colorId);

    return EncodeDestination(dest);
}


static UniValue setlabel(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "setlabel \"address\" \"label\"\n"
            "\nSets the label associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The tapyrus address to be associated with a label.\n"
            "2. \"label\"           (string, required) The label to assign to the address.\n"
            "\nExamples:\n"
            + HelpExampleCli("setlabel", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"tabby\"")
            + HelpExampleRpc("setlabel", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"tabby\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Tapyrus address");
    }

    std::string old_label = pwallet->mapAddressBook[dest].name;
    std::string label = LabelFromValue(request.params[1]);

    if (IsMine(*pwallet, dest)) {
        pwallet->SetAddressBook(dest, label, "receive");
    } else {
        pwallet->SetAddressBook(dest, label, "send");
    }

    // Detect when there are no addresses using this label.
    // If so, delete the account record for it. Labels, unlike addresses, can be deleted,
    // and if we wouldn't do this, the record would stick around forever.
    bool found_address = false;
    for (const std::pair<const CTxDestination, CAddressBookData>& item : pwallet->mapAddressBook) {
        if (item.second.name == label) {
            found_address = true;
            break;
        }
    }
    if (!found_address) {
        pwallet->DeleteLabel(old_label);
    }

    return NullUniValue;
}


static CTransactionRef SendMoney(CWallet * const pwallet, const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, const CCoinControl& coin_control, mapValue_t mapValue, const ColorIdentifier& colorId)
{
    CAmount curBalance = pwallet->GetBalance()[colorId];

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    if (pwallet->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    // Parse Tapyrus address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    CWallet::ChangePosInOut mapChangePosRet;
    mapChangePosRet[ColorIdentifier()] = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    CTransactionRef tx;
    if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, mapChangePosRet, strError, coin_control)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > curBalance)
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CValidationState state;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return tx;
}

static UniValue sendtoaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 8)
        throw std::runtime_error(
            "sendtoaddress \"address\" amount ( \"comment\" \"comment_to\" subtractfeefromamount replaceable conf_target \"estimate_mode\")\n"
            "\nSend an amount to a given address.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"address\"            (string, required) The tapyrus address to send to.\n"
            "2. \"amount\"             (numeric or string, required) The amount in " + CURRENCY_UNIT + "or tapyrus token to send. eg 0.1\n"
            "3. \"comment\"            (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment_to\"         (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less TPC than you enter in the amount field.\n"
            "6. replaceable            (boolean, optional) Allow this transaction to be replaced by a transaction with higher fees via BIP 125\n"
            "7. conf_target            (numeric, optional) Confirmation target (in blocks)\n"
            "8. \"estimate_mode\"      (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "       \"UNSET\"\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\"\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1  \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }
    ColorIdentifier colorId;
    if(dest.index() == 3)
        colorId = std::get<CColorKeyID>(dest).color;
    else if(dest.index() == 4)
        colorId = std::get<CColorScriptID>(dest).color;

    if (colorId.type != TokenTypes::NONE
      && pwallet->GetBalance()[colorId] == 0 ) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No Token found in wallet. But token address was given.");
    }

    // Amount
    CAmount nAmount = (colorId.type == TokenTypes::NONE ? AmountFromValue(request.params[1]) : TokenAmountFromValue(request.params[1]));
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");

    // Wallet comments
    mapValue_t mapValue;
    if (!request.params[2].isNull() && !request.params[2].get_str().empty())
        mapValue["comment"] = request.params[2].get_str();
    if (!request.params[3].isNull() && !request.params[3].get_str().empty())
        mapValue["to"] = request.params[3].get_str();

    bool fSubtractFeeFromAmount = false;
    if (!request.params[4].isNull()) {
        fSubtractFeeFromAmount = request.params[4].get_bool();
    }

    CCoinControl coin_control;
    if (!request.params[5].isNull()) {
        coin_control.m_signal_bip125_rbf = request.params[5].get_bool();
    }

    if (!request.params[6].isNull()) {
        coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
    }

    if (!request.params[7].isNull()) {
        if (!FeeModeFromString(request.params[7].get_str(), coin_control.m_fee_mode)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
    }

    if (colorId.type != TokenTypes::NONE)
    {
        coin_control.m_colorTxType = ColoredTxType::TRANSFER; 
        coin_control.m_colorId = colorId;
    }

    EnsureWalletIsUnlocked(pwallet);

    if(pwallet->GetBalance()[colorId] < nAmount)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Insufficient token balance in wallet");

    CTransactionRef tx = SendMoney(pwallet, dest, nAmount, fSubtractFeeFromAmount, coin_control, std::move(mapValue), colorId);
    return tx->GetHashMalFix().GetHex();
}

static UniValue listaddressgroupings(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"address\",            (string) The tapyrus address\n"
            "      amount,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"label\"               (string, optional) The label\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    UniValue jsonGroupings(UniValue::VARR);
    std::map<CTxDestination, CAmount> balances = pwallet->GetAddressBalances();
    for (const std::set<CTxDestination>& grouping : pwallet->GetAddressGroupings()) {
        UniValue jsonGrouping(UniValue::VARR);
        for (const CTxDestination& address : grouping)
        {
            ColorIdentifier colorId;
            if(address.index() == 3)
                colorId = std::get<CColorKeyID>(address).color;
            else if(address.index() == 4)
                colorId = std::get<CColorScriptID>(address).color;
            UniValue addressInfo(UniValue::VARR);
            addressInfo.push_back(EncodeDestination(address));
            addressInfo.push_back(colorId.type == TokenTypes::NONE ? ValueFromAmount(balances[address]) : balances[address]);
            {
                if (pwallet->mapAddressBook.find(address) != pwallet->mapAddressBook.end()) {
                    addressInfo.push_back(pwallet->mapAddressBook.find(address)->second.name);
                }
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

static UniValue signmessage(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "signmessage \"address\" \"message\"\n"
            "\nSign a message with the private key of an address"
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The tapyrus address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"my message\"")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    std::string strAddress = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID *keyID = std::get_if<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");
    }

    CKey key;
    if (!pwallet->GetKey(*keyID, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");
    }

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

static UniValue getreceivedbyaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "getreceivedbyaddress \"address\"\n"
            "\nReturns the total amount received by the given address in transactions.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The tapyrus address for transactions.\n"
            "\nResult:\n"
            "amount   (numeric) The total amount in " + CURRENCY_UNIT + " received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", 6")
       );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    // Tapyrus address
    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Tapyrus address");
    }
    CScript scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwallet, scriptPubKey)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Address not found in wallet");
    }

    // Minimum confirmations
    int nMinDepth = 1;

    // Tally
    CAmount nAmount = 0;
    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const CTxOut& txout : wtx.tx->vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    ColorIdentifier colorId;
    if(dest.index() == 3)
        colorId = std::get<CColorKeyID>(dest).color;
    else if(dest.index() == 4)
        colorId = std::get<CColorScriptID>(dest).color;

    return colorId.type == TokenTypes::NONE ? ValueFromAmount(nAmount) : nAmount;
}


static UniValue getreceivedbylabel(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "getreceivedbylabel \"label\"\n"
            "\nReturns the total amount received by addresses with <label> \n"
            "\nArguments:\n"
            "1. \"label\"        (string, required) The selected label, may be the default label using \"\".\n"
            "\nResult:\n"
            "{\n"
            "token:amount        (numeric) The total amount in " + CURRENCY_UNIT + "or token received for this label.\n"
            "}\n"
            "\nExamples:\n"
            "\nAmount received by the default label with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbylabel", "\"\"") +
            "\nAmount received at the tabby label including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbylabel", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbylabel", "\"tabby\", 6")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    // Minimum confirmations
    int nMinDepth = 1;

    // Get the set of pub keys assigned to label
    std::string label = LabelFromValue(request.params[0]);
    std::set<CTxDestination> setAddress = pwallet->GetLabelAddresses(label);

     // Tally
    TxColoredCoinBalancesMap lable_balance;
    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const CTxOut& txout : wtx.tx->vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwallet, address) && setAddress.count(address)) {
                ColorIdentifier colorId;
                if(address.index() == 3)
                    colorId = std::get<CColorKeyID>(address).color;
                else if(address.index() == 4)
                    colorId = std::get<CColorScriptID>(address).color;

                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    lable_balance[colorId] += txout.nValue;
            }
        }
    }

    UniValue balances(UniValue::VOBJ);
    for (auto const& wb : lable_balance) {
        if (wb.first.type == TokenTypes::NONE)
            balances.pushKV(CURRENCY_UNIT.c_str(), ValueFromAmount(wb.second));
        else
            balances.pushKV(wb.first.toHexString(), wb.second);
    };
    return balances;
}


static UniValue getbalance(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || (request.params.size() > 2 ))
        throw std::runtime_error( std::string(
            "getbalance ( include_watchonly color )\n"
            "\nReturns the total available balance.\n"
            "The available balance is what the wallet considers currently spendable, and is\n"
            "thus affected by options which limit spendability such as -spendzeroconfchange.\n"
            "\nArguments:\n"
            "1. include_watchonly (bool, optional, default=false) Also include balance in watch-only addresses (see 'importaddress')\n"
            "2. \"color\"         (string, optional, default="+ CURRENCY_UNIT +") The tapyrus  token whose balance in the wallet is being queried.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + " received for this wallet. If color parameter is provided then this is the total amount in that token.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet with 1 or more confirmations\n"
            + HelpExampleCli("getbalance", "\"false\" \"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\"") +
            "\nThe total amount in the wallet at least 6 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"false\" \"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"false\" \"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\"")
        ));

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    isminefilter filter = ISMINE_SPENDABLE;
    if (!request.params[0].isNull() && request.params[0].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    ColorIdentifier colorId;
    if (!request.params[1].isNull())
    {
        const std::vector<unsigned char> vColorId(ParseHex(request.params[1].get_str()));
        colorId = ColorIdentifier(vColorId);
        if(colorId.type == TokenTypes::NONE)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color parameter.");
    }
    CAmount amount = pwallet->GetBalance(filter, 0)[colorId];
    return colorId.type == TokenTypes::NONE ? ValueFromAmount(amount) : amount;
}

static UniValue getunconfirmedbalance(const JSONRPCRequest &request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n");

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    return ValueFromAmount(pwallet->GetUnconfirmedBalance()[ColorIdentifier()]);
}


static UniValue sendmany(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    std::string help_text = "sendmany {\"address\":amount,...} ( \"comment\" [\"address\",...] replaceable conf_target \"estimate_mode\")\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers."
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments:\n"
            "1. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric or string) The tapyrus address is the key, the numeric amount (can be string) in " + CURRENCY_UNIT + " is the value\n"
            "      ,...\n"
            "    }\n"
            "2. \"comment\"             (string, optional) A comment\n"
            "3. subtractfeefrom         (array, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less TPC than you enter in their corresponding amount field.\n"
            "                           If no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"          (string) Subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "4. replaceable            (boolean, optional) Allow this transaction to be replaced by a transaction with higher fees via BIP 125\n"
            "5. conf_target            (numeric, optional) Confirmation target (in blocks)\n"
            "6. \"estimate_mode\"      (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "       \"UNSET\"\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\"\n"
             "\nResult:\n"
            "\"txid\"                   (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" \"testing\"") +
            "\nSend two amounts to two different addresses, subtract fee from amount:\n"
            + HelpExampleCli("sendmany", "\"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" \"\" \"[\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\",\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "{\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\":0.01,\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\":0.02}, \"testing\"");

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 6) throw std::runtime_error(help_text);

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    UniValue sendTo = request.params[0].get_obj();

    mapValue_t mapValue;
    if (!request.params[1].isNull() && !request.params[1].get_str().empty())
        mapValue["comment"] = request.params[1].get_str();

    UniValue subtractFeeFromAmount(UniValue::VARR);
    if (!request.params[2].isNull())
        subtractFeeFromAmount = request.params[2].get_array();

    CCoinControl coin_control;
    if (!request.params[3].isNull()) {
        coin_control.m_signal_bip125_rbf = request.params[3].get_bool();
    }

    if (!request.params[4].isNull()) {
        coin_control.m_confirm_target = ParseConfirmTarget(request.params[4]);
    }

    if (!request.params[5].isNull()) {
        if (!FeeModeFromString(request.params[5].get_str(), coin_control.m_fee_mode)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
    }

    std::set<CTxDestination> destinations;
    std::vector<CRecipient> vecSend;

    CAmount totalAmount = 0;
    std::vector<std::string> keys = sendTo.getKeys();
    for (const std::string& name_ : keys) {
        CTxDestination dest = DecodeDestination(name_);
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Tapyrus address: ") + name_);
        }

        if (destinations.count(dest)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
        }
        destinations.insert(dest);

        CScript scriptPubKey = GetScriptForDestination(dest);
        CAmount nAmount = AmountFromValue(sendTo[name_]);
        if (nAmount <= 0)
            throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for send");
        totalAmount += nAmount;

        bool fSubtractFeeFromAmount = false;
        for (unsigned int idx = 0; idx < subtractFeeFromAmount.size(); idx++) {
            const UniValue& addr = subtractFeeFromAmount[idx];
            if (addr.get_str() == name_)
                fSubtractFeeFromAmount = true;
        }

        CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount};
        vecSend.push_back(recipient);
    }

    EnsureWalletIsUnlocked(pwallet);

    // Check funds
    ColorIdentifier colorId;
    if (totalAmount > pwallet->GetLegacyBalance(ISMINE_SPENDABLE, 1, nullptr, colorId)) {
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Wallet has insufficient funds");
    }

    // Shuffle recipient list
    std::shuffle(vecSend.begin(), vecSend.end(), FastRandomContext());

    // Send
    CReserveKey keyChange(pwallet);
    CAmount nFeeRequired = 0;
    CWallet::ChangePosInOut mapChangePosRet;
    mapChangePosRet[ColorIdentifier()] = -1;
    std::string strFailReason;
    CTransactionRef tx;
    bool fCreated = pwallet->CreateTransaction(vecSend, tx, keyChange, nFeeRequired, mapChangePosRet, strFailReason, coin_control);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    CValidationState state;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, keyChange, g_connman.get(), state)) {
        strFailReason = strprintf("Transaction commit failed:: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strFailReason);
    }

    return tx->GetHashMalFix().GetHex();
}

static UniValue addmultisigaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }


    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error("addmultisigaddress nrequired [\"key\",...] ( \"label\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet. Requires a new wallet backup.\n"
            "Each key is a Tapyrus address or hex-encoded public key.\n"
            "This functionality is only intended for use with non-watchonly addresses.\n"
            "See `importaddress` for watchonly p2sh address support.\n"
            "If 'label' is specified, assign address to that label.\n"

            "\nArguments:\n"
            "1. nrequired                        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"                         (string, required) A json array of tapyrus addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"                  (string) tapyrus address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"label\"                        (string, optional) A label to assign the addresses to.\n"

            "\nResult:\n"
            "{\n"
            "  \"address\":\"multisigaddress\",    (string) The value of the new multisig address.\n"
            "  \"redeemScript\":\"script\"         (string) The string value of the hex-encoded redemption script.\n"
            "}\n"
            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\""));

    LOCK2(cs_main, pwallet->cs_wallet);

    std::string label;
    if (!request.params[2].isNull())
        label = LabelFromValue(request.params[2]);

    int required = request.params[0].get_int();

    // Get the public keys
    const UniValue& keys_or_addrs = request.params[1].get_array();
    std::vector<CPubKey> pubkeys;
    for (unsigned int i = 0; i < keys_or_addrs.size(); ++i) {
        if (IsHex(keys_or_addrs[i].get_str()) && (keys_or_addrs[i].get_str().length() == 66 || keys_or_addrs[i].get_str().length() == 130)) {
            pubkeys.push_back(HexToPubKey(keys_or_addrs[i].get_str()));
        } else {
            pubkeys.push_back(AddrToPubKey(pwallet, keys_or_addrs[i].get_str()));
        }
    }

    OutputType output_type = pwallet->m_default_address_type;

    // Construct using pay-to-script-hash:
    CScript inner = CreateMultisigRedeemscript(required, pubkeys);
    CTxDestination dest = AddAndGetDestinationForScript(*pwallet, inner, output_type);
    pwallet->SetAddressBook(dest, label, "send");

    UniValue result(UniValue::VOBJ);
    result.pushKV("address", EncodeDestination(dest));
    result.pushKV("redeemScript", HexStr(inner.begin(), inner.end()));
    return result;
}

struct tallyitem
{
    CAmount nAmount;
    int nConf;
    std::vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem():nAmount()
    {
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

static UniValue ListReceived(CWallet * const pwallet, const UniValue& params, bool by_label) EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    // Whether to include empty labels
    bool fIncludeEmpty = false;
    if (!params[0].isNull())
        fIncludeEmpty = params[0].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if(!params[1].isNull())
        if(params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    bool has_filtered_address = false;
    CTxDestination filtered_address = CNoDestination();
    if (!by_label && params.size() > 2) {
        if (!IsValidDestinationString(params[2].get_str())) {
            throw JSONRPCError(RPC_WALLET_ERROR, "address_filter parameter was invalid");
        }
        filtered_address = DecodeDestination(params[2].get_str());
        has_filtered_address = true;
    }

    // Tally
    std::map<CTxDestination, tallyitem> mapTally;
    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        const CWalletTx& wtx = pairWtx.second;

        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 1)
            continue;

        for (const CTxOut& txout : wtx.tx->vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            if (has_filtered_address && !(filtered_address == address)) {
                continue;
            }

            isminefilter mine = IsMine(*pwallet, address);
            if(!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = std::min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    UniValue ret(UniValue::VARR);
    std::map<std::string, std::map<ColorIdentifier, tallyitem> >label_tally;

    // Create mapAddressBook iterator
    // If we aren't filtering, go from begin() to end()
    auto start = pwallet->mapAddressBook.begin();
    auto end = pwallet->mapAddressBook.end();
    // If we are filtering, find() the applicable entry
    if (has_filtered_address) {
        start = pwallet->mapAddressBook.find(filtered_address);
        if (start != end) {
            end = std::next(start);
        }
    }

    for (auto item_it = start; item_it != end; ++item_it)
    {
        const CTxDestination& address = item_it->first;
        const std::string& label = item_it->second.name;
        auto it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        ColorIdentifier colorId;
        if(address.index() == 3)
            colorId = std::get<CColorKeyID>(address).color;
        else if(address.index() == 4)
            colorId = std::get<CColorScriptID>(address).color;

        CAmount nAmount = 0;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (by_label)
        {
            std::map<ColorIdentifier, tallyitem>& _mapitem = label_tally[label];
            tallyitem& _item = _mapitem[colorId];
            _item.nAmount += nAmount;
            _item.nConf = std::min(_item.nConf, nConf);
            _item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            UniValue obj(UniValue::VOBJ);
            if(fIsWatchonly)
                obj.pushKV("involvesWatchonly", true);
            obj.pushKV("address", EncodeDestination(address));
            obj.pushKV("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf));
            obj.pushKV("label", label);
            addTokenKV(address, nAmount, obj);
            UniValue transactions(UniValue::VARR);
            if (it != mapTally.end())
            {
                for (const uint256& _item : (*it).second.txids)
                {
                    transactions.push_back(_item.GetHex());
                }
            }
            obj.pushKV("txids", transactions);
            ret.push_back(obj);
        }
    }

    if (by_label)
    {
        for (const auto& lable_balance : label_tally)
        {
            for (const auto& entry : lable_balance.second)
            {
                CAmount nAmount = entry.second.nAmount;
                int nConf = entry.second.nConf;
                UniValue obj(UniValue::VOBJ);
                if (entry.second.fIsWatchonly)
                    obj.pushKV("involvesWatchonly", true);

                obj.pushKV("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf));
                obj.pushKV("label", lable_balance.first); 
                obj.pushKV("token", entry.first.toHexString());
                obj.pushKV("amount", entry.first.type == TokenTypes::NONE ? ValueFromAmount(nAmount) : nAmount);
                ret.push_back(obj);
            }
        }
    }

    return ret;
}

static UniValue listreceivedbyaddress(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error(
            "listreceivedbyaddress ( include_empty include_watchonly address_filter )\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. include_empty     (bool, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "2. include_watchonly (bool, optional, default=false) Whether to include watch-only addresses (see 'importaddress').\n"
            "3. address_filter    (string, optional) If present, only return information on this address.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) DEPRECATED. Backwards compatible alias for label.\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in " + CURRENCY_UNIT + " received by the address\n"
            "    \"confirmations\" : n,               (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"label\" : \"label\",               (string) The label of the receiving address. The default label is \"\".\n"
            "    \"txids\": [\n"
            "       \"txid\",                         (string) The ids of transactions received with the address \n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "true")
            + HelpExampleRpc("listreceivedbyaddress", "true, true")
            + HelpExampleRpc("listreceivedbyaddress", "true, true, \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    return ListReceived(pwallet, request.params, false);
}

static UniValue listreceivedbylabel(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error(
            "listreceivedbylabel ( include_empty include_watchonly)\n"
            "\nList received transactions by label.\n"
            "\nArguments:\n"
            "1. include_empty     (bool, optional, default=false) Whether to include labels that haven't received any payments.\n"
            "2. include_watchonly (bool, optional, default=false) Whether to include watch-only addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) DEPRECATED. Backwards compatible alias for label.\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this label\n"
            "    \"confirmations\" : n,          (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"label\" : \"label\"           (string) The label of the receiving address. The default label is \"\".\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbylabel", "")
            + HelpExampleCli("listreceivedbylabel", "true")
            + HelpExampleRpc("listreceivedbylabel", "true, true")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    return ListReceived(pwallet, request.params, true);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    if (IsValidDestination(dest)) {
        entry.pushKV("address", EncodeDestination(dest));
    }
}

/**
 * List transactions based on the given criteria.
 *
 * @param  pwallet    The wallet.
 * @param  wtx        The wallet transaction.
 * @param  nMinDepth  The minimum confirmation depth.
 * @param  fLong      Whether to include the JSON version of the transaction.
 * @param  ret        The UniValue into which the result is stored.
 * @param  filter     The "is mine" filter bool.
 */
static void ListTransactions(CWallet* const pwallet, const CWalletTx& wtx, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)  EXCLUSIVE_LOCKS_REQUIRED(pwallet->cs_wallet)
{
    CAmount nFee;
    std::string strSentAccount;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);
    bool feeAdded = false;

    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0))
    {
        for (const COutputEntry& s : listSent)
        {
            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwallet, s.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, s.destination);
            entry.pushKV("category", "send");

            addTokenKV(s.destination, -s.amount, entry);

            if (pwallet->mapAddressBook.count(s.destination)) {
                entry.pushKV("label", pwallet->mapAddressBook[s.destination].name);
            }
            entry.pushKV("vout", s.vout);
            entry.pushKV("fee", ValueFromAmount(-nFee));
            feeAdded = true;
            if (fLong)
                WalletTxToJSON(wtx, entry);
            entry.pushKV("abandoned", wtx.isAbandoned());
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        for (const COutputEntry& r : listReceived)
        {
            std::string account;
            if (pwallet->mapAddressBook.count(r.destination)) {
                account = pwallet->mapAddressBook[r.destination].name;
            }

            UniValue entry(UniValue::VOBJ);
            if (involvesWatchonly || (::IsMine(*pwallet, r.destination) & ISMINE_WATCH_ONLY)) {
                entry.pushKV("involvesWatchonly", true);
            }
            MaybePushAddress(entry, r.destination);
            if (wtx.IsCoinBase())
            {
                if (wtx.GetDepthInMainChain() < 1)
                    entry.pushKV("category", "orphan");
                else
                    entry.pushKV("category", "generate");
            }
            else
            {
                entry.pushKV("category", "receive");
            }
            addTokenKV(r.destination, r.amount, entry);

            if (pwallet->mapAddressBook.count(r.destination)) {
                entry.pushKV("label", account);
            }
            entry.pushKV("vout", r.vout);
            if(!feeAdded)
                entry.pushKV("fee", ValueFromAmount(-nFee));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);

        }
    }
}


UniValue listtransactions(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    std::string help_text = "listtransactions (count skip include_watchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. count          (numeric, optional, default=10) The number of transactions to return\n"
            "2. skip           (numeric, optional, default=0) The number of transactions to skip\n"
            "3. include_watchonly (bool, optional, default=false) Include transactions to watch-only addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"address\":\"address\",    (string) The tapyrus address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"label\": \"label\",       (string) A comment for the address/transaction, if any\n"
            "    \"vout\": n,                (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions. Negative confirmations indicate the\n"
            "                                         transaction conflicts with the block chain\n"
            "    \"trusted\": xxx,           (bool) Whether we consider the outputs of this unconfirmed transaction safe to spend.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) DEPRECATED. This field will be removed in V0.18. For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                     may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx          (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                         'send' category of transactions.\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "20, 100");

    if (request.fHelp || request.params.size() > 3) throw std::runtime_error(help_text);

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    int nCount = 10;
    if (!request.params[0].isNull())
        nCount = request.params[0].get_int();
    int nFrom = 0;
    if (!request.params[1].isNull())
        nFrom = request.params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(!request.params[2].isNull())
        if(request.params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    UniValue ret(UniValue::VARR);

    {
        LOCK2(cs_main, pwallet->cs_wallet);

        const CWallet::TxItems & txOrdered = pwallet->wtxOrdered;

        // iterate backwards until we have nCount items to return:
        for (CWallet::TxItems::const_reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
        {
            CWalletTx *const pwtx = (*it).second.first;
            if (pwtx != nullptr)
                ListTransactions(pwallet, *pwtx, 0, true, ret, filter);

            if ((int)ret.size() >= (nCount+nFrom)) break;
        }
    }

    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    std::vector<UniValue> arrTmp = ret.getValues();

    std::vector<UniValue>::iterator first = arrTmp.begin();
    std::advance(first, nFrom);
    std::vector<UniValue>::iterator last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}


static UniValue listsinceblock(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error(
            "listsinceblock ( \"blockhash\" target_confirmations include_watchonly include_removed )\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted.\n"
            "If \"blockhash\" is no longer a part of the main chain, transactions from the fork point onward are included.\n"
            "Additionally, if include_removed is set, transactions affecting the wallet which were removed are returned in the \"removed\" array.\n"
            "\nArguments:\n"
            "1. \"blockhash\"            (string, optional) The block hash to list transactions since\n"
            "2. target_confirmations:    (numeric, optional, default=1) Return the nth block hash from the main chain. e.g. 1 would mean the best block hash. Note: this is not used as a filter, but only affects [lastblock] in the return value\n"
            "3. include_watchonly:       (bool, optional, default=false) Include transactions to watch-only addresses (see 'importaddress')\n"
            "4. include_removed:         (bool, optional, default=true) Show transactions that were removed due to a reorg in the \"removed\" array\n"
            "                                                           (not guaranteed to work on pruned nodes)\n"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"address\":\"address\",    (string) The tapyrus address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "                                          When it's < 0, it means the transaction conflicted that many blocks ago.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx,         (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the 'send' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"label\" : \"label\"       (string) A comment for the address/transaction, if any\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
            "  ],\n"
            "  \"removed\": [\n"
            "    <structure is the same as \"transactions\" above, only present if include_removed=true>\n"
            "    Note: transactions that were re-added in the active chain will appear as-is in this array, and may thus have a positive confirmation count.\n"
            "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the block (target_confirmations-1) from the best block on the main chain. This is typically used to feed back into listsinceblock the next time you call it. So you would generally use a target_confirmations of say 6, so you will be continually re-notified of transactions until they've reached 6 confirmations plus any new ones\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    const CBlockIndex* pindex = nullptr;    // Block index of the specified block or the common ancestor, if the block provided was in a deactivated chain.
    const CBlockIndex* paltindex = nullptr; // Block index of the specified block, even if it's in a deactivated chain.
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (!request.params[0].isNull() && !request.params[0].get_str().empty()) {
        uint256 blockId;

        blockId.SetHex(request.params[0].get_str());
        paltindex = pindex = LookupBlockIndex(blockId);
        if (!pindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
        if (chainActive[pindex->nHeight] != pindex) {
            // the block being asked for is a part of a deactivated chain;
            // we don't want to depend on its perceived height in the block
            // chain, we want to instead use the last common ancestor
            pindex = chainActive.FindFork(pindex);
        }
    }

    if (!request.params[1].isNull()) {
        target_confirms = request.params[1].get_int();

        if (target_confirms < 1) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
        }
    }

    if (!request.params[2].isNull() && request.params[2].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    bool include_removed = (request.params[3].isNull() || request.params[3].get_bool());

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    UniValue transactions(UniValue::VARR);

    for (const std::pair<const uint256, CWalletTx>& pairWtx : pwallet->mapWallet) {
        CWalletTx tx = pairWtx.second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth) {
            ListTransactions(pwallet, tx, 0, true, transactions, filter);
        }
    }

    // when a reorg'd block is requested, we also list any relevant transactions
    // in the blocks of the chain that was detached
    UniValue removed(UniValue::VARR);
    while (include_removed && paltindex && paltindex != pindex) {
        CBlock block;
        if (!ReadBlockFromDisk(block, paltindex)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");
        }
        for (const CTransactionRef& tx : block.vtx) {
            auto it = pwallet->mapWallet.find(tx->GetHashMalFix());
            if (it != pwallet->mapWallet.end()) {
                // We want all transactions regardless of confirmation count to appear here,
                // even negative confirmation ones, hence the big negative.
                ListTransactions(pwallet, it->second, -100000000, true, removed, filter);
            }
        }
        paltindex = paltindex->pprev;
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    UniValue ret(UniValue::VOBJ);
    ret.pushKV("transactions", transactions);
    if (include_removed) ret.pushKV("removed", removed);
    ret.pushKV("lastblock", lastblock.GetHex());

    return ret;
}

static UniValue gettransaction(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "gettransaction \"txid\" ( include_watchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"                  (string, required) The transaction id\n"
            "2. \"include_watchonly\"     (bool, optional, default=false) Whether to include watch-only addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The index of the transaction in the block that includes it\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"address\" : \"address\",          (string) The tapyrus address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"token\" : \"token\",              (string) color of the token involved. either "+ CURRENCY_UNIT +" or colorid\n"
            "      \"amount\" : x.xxx,                 (numeric) The amount involved\n"
            "      \"label\" : \"label\",              (string) A comment for the address/transaction, if any\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "      \"fee\": x.xxx,                     (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                           'send' category of transactions.\n"
            "      \"abandoned\": xxx                  (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                           'send' category of transactions.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if(!request.params[1].isNull())
        if(request.params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry(UniValue::VOBJ);
    auto it = pwallet->mapWallet.find(hash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    const CWalletTx& wtx = it->second;

    WalletTxToJSON(wtx, entry);

    UniValue details(UniValue::VARR);
    ListTransactions(pwallet, wtx, 0, false, details, filter);
    entry.pushKV("details", details);

    std::string strHex = EncodeHexTx(*wtx.tx, RPCSerializationFlags());
    entry.pushKV("hex", strHex);

    return entry;
}

static UniValue abandontransaction(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "abandontransaction \"txid\"\n"
            "\nMark in-wallet transaction <txid> as abandoned\n"
            "This will mark this transaction and all its in-wallet descendants as abandoned which will allow\n"
            "for their inputs to be respent.  It can be used to replace \"stuck\" or evicted transactions.\n"
            "It only works on transactions which are not included in a block and are not currently in the mempool.\n"
            "It has no effect on transactions which are already abandoned.\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    if (!pwallet->mapWallet.count(hash)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    if (!pwallet->AbandonTransaction(hash)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not eligible for abandonment");
    }

    return NullUniValue;
}


static UniValue backupwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "backupwallet \"destination\"\n"
            "\nSafely copies current wallet file to destination, which can be a directory or a path with filename.\n"
            "\nArguments:\n"
            "1. \"destination\"   (string) The destination directory or file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backup.dat\"")
            + HelpExampleRpc("backupwallet", "\"backup.dat\"")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strDest = request.params[0].get_str();
    if (!pwallet->BackupWallet(strDest)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");
    }

    return NullUniValue;
}


static UniValue keypoolrefill(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (!request.params[0].isNull()) {
        if (request.params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)request.params[0].get_int();
    }

    EnsureWalletIsUnlocked(pwallet);
    pwallet->TopUpKeyPool(kpSize);

    if (pwallet->GetKeyPoolSize() < kpSize) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");
    }

    return NullUniValue;
}


static UniValue walletpassphrase(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending TPC\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds; capped at 100000000 (~3 years).\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");
    }

    // Note that the walletpassphrase is stored in request.params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    strWalletPass = request.params[0].get_str().c_str();

    // Get the timeout
    int64_t nSleepTime = request.params[1].get_int64();
    // Timeout cannot be negative, otherwise it will relock immediately
    if (nSleepTime < 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Timeout cannot be negative.");
    }
    // Clamp timeout
    constexpr int64_t MAX_SLEEP_TIME = 100000000; // larger values trigger a macos/libevent bug?
    if (nSleepTime > MAX_SLEEP_TIME) {
        nSleepTime = MAX_SLEEP_TIME;
    }

    if (strWalletPass.length() > 0)
    {
        if (!pwallet->Unlock(strWalletPass)) {
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
        }
    }
    else
        throw std::runtime_error(
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    pwallet->TopUpKeyPool();

    pwallet->nRelockTime = GetTime() + nSleepTime;

    // Keep a weak pointer to the wallet so that it is possible to unload the
    // wallet before the following callback is called. If a valid shared pointer
    // is acquired in the callback then the wallet is still loaded.
    std::weak_ptr<CWallet> weak_wallet = wallet;
    RPCRunLater(strprintf("lockwallet(%s)", pwallet->GetName()), [weak_wallet] {
        if (auto shared_wallet = weak_wallet.lock()) {
            LOCK(shared_wallet->cs_wallet);
            shared_wallet->Lock();
            shared_wallet->nRelockTime = 0;
        }
    }, nSleepTime);

    return NullUniValue;
}


static UniValue walletpassphrasechange(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");
    }

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = request.params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = request.params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw std::runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwallet->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass)) {
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }

    return NullUniValue;
}


static UniValue walletlock(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");
    }

    pwallet->Lock();
    pwallet->nRelockTime = 0;

    return NullUniValue;
}


static UniValue encryptwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "encryptwallet \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt your wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending bitcoin\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can do something like sign\n"
            + HelpExampleCli("signmessage", "\"address\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->IsCrypted()) {
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");
    }

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = request.params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw std::runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwallet->EncryptWallet(strWalletPass)) {
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");
    }

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Tapyrus server stopping, restart to run with encrypted wallet. The keypool has been flushed and a new HD seed was generated (if you are using HD). You need to make a new backup.";
}

static UniValue lockunspent(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error(
            "lockunspent unlock ([{\"txid\":\"txid\",\"vout\":n},...])\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "If no transaction outputs are specified when unlocking then all current locked transaction outputs are unlocked.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending TPC.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, optional) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    RPCTypeCheckArgument(request.params[0], UniValue::VBOOL);

    bool fUnlock = request.params[0].get_bool();

    if (request.params[1].isNull()) {
        if (fUnlock)
            pwallet->UnlockAllCoins();
        return true;
    }

    RPCTypeCheckArgument(request.params[1], UniValue::VARR);

    const UniValue& output_params = request.params[1];

    // Create and validate the COutPoints first.

    std::vector<COutPoint> outputs;
    outputs.reserve(output_params.size());

    for (unsigned int idx = 0; idx < output_params.size(); idx++) {
        const UniValue& o = output_params[idx].get_obj();

        RPCTypeCheckObj(o,
            {
                {"txid", UniValueType(UniValue::VSTR)},
                {"vout", UniValueType(UniValue::VNUM)},
            });

        const std::string& txid = o.find_value("txid").get_str();
        if (!IsHex(txid)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");
        }

        const int nOutput = o.find_value("vout").get_int();
        if (nOutput < 0) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");
        }

        const COutPoint outpt(uint256S(txid), nOutput);

        const auto it = pwallet->mapWallet.find(outpt.hashMalFix);
        if (it == pwallet->mapWallet.end()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, unknown transaction");
        }

        const CWalletTx& trans = it->second;

        if (outpt.n >= trans.tx->vout.size()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout index out of bounds");
        }

        if (pwallet->IsSpent(outpt.hashMalFix, outpt.n)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected unspent output");
        }

        const bool is_locked = pwallet->IsLockedCoin(outpt.hashMalFix, outpt.n);

        if (fUnlock && !is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected locked output");
        }

        if (!fUnlock && is_locked) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, output already locked");
        }

        outputs.push_back(outpt);
    }

    // Atomically set (un)locked status for the outputs.
    for (const COutPoint& outpt : outputs) {
        if (fUnlock) pwallet->UnlockCoin(outpt);
        else pwallet->LockCoin(outpt);
    }

    return true;
}

static UniValue listlockunspent(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error(
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    LOCK2(cs_main, pwallet->cs_wallet);

    std::vector<COutPoint> vOutpts;
    pwallet->ListLockedCoins(vOutpts);

    UniValue ret(UniValue::VARR);

    for (COutPoint &outpt : vOutpts) {
        UniValue o(UniValue::VOBJ);

        o.pushKV("txid", outpt.hashMalFix.GetHex());
        o.pushKV("vout", (int)outpt.n);
        ret.push_back(o);
    }

    return ret;
}

static UniValue settxfee(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 1) {
        throw std::runtime_error(
            "settxfee amount\n"
            "\nSet the transaction fee per kB for this wallet. Overrides the global -paytxfee command line parameter.\n"
            "\nArguments:\n"
            "1. amount         (numeric or string, required) The transaction fee in " + CURRENCY_UNIT + "/kB\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    CAmount nAmount = AmountFromValue(request.params[0]);

    pwallet->m_pay_tx_fee = CFeeRate(nAmount, 1000);
    return true;
}

static UniValue getwalletinfo(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletname\": xxxxx,               (string) the wallet name\n"
            "  \"walletversion\": xxxxx,            (numeric) the wallet version\n"
            "  \"balance\":                         (object) the balances of each " + CURRENCY_UNIT  + " and colored coins of the wallet\n"
            "       {\n"
            "         \"xxxx\":\"xxxx\"             (string) \"" + CURRENCY_UNIT  + "\" or color id in hex string : (numeric) The total confirmed balance of the wallet. The unit is " + CURRENCY_UNIT + " for the native coin. If it is a colored coin, the unit is the same digits as " + CURRENCY_UNIT + "\n"
            "       }\n"
            "  \"unconfirmed_balance\":             (object) the unconfirmed balances of each " + CURRENCY_UNIT  + " and colored coins of the wallet\n"
            "       {\n"
            "         \"xxxx\":\"xxxx\"             (string) \"" + CURRENCY_UNIT  + "\" or color id in hex string : (numeric) The total unconfirmed balance of the wallet. The unit is " + CURRENCY_UNIT + " for the native coin. If it is a colored coin, the unit is the same digits as " + CURRENCY_UNIT + "\n"
            "       }\n"
            "  \"txcount\": xxxxxxx,                (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,           (numeric) the timestamp (seconds since Unix epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,               (numeric) how many new keys are pre-generated (only counts external keys)\n"
            "  \"keypoolsize_hd_internal\": xxxx,   (numeric) how many new keys are pre-generated for internal use (used for change outputs, only appears if the wallet is using this feature, otherwise external keys are used)\n"
            "  \"unlocked_until\": ttt,             (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,                (numeric) the transaction fee configuration, set in " + CURRENCY_UNIT + "/kB\n"
            "  \"hdseedid\": \"<hash160>\"            (string, optional) the Hash160 of the HD seed (only present when HD is enabled)\n"
            "  \"hdmasterkeyid\": \"<hash160>\"       (string, optional) alias for hdseedid retained for backwards-compatibility. Will be removed in V0.18.\n"
            "  \"private_keys_enabled\": true|false (boolean) false if privatekeys are disabled for this wallet (enforced watch-only wallet)\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);

    UniValue obj(UniValue::VOBJ);
    UniValue balances(UniValue::VOBJ);
    UniValue unconfirmBalancesObj(UniValue::VOBJ);

    TxColoredCoinBalancesMap walletbalances = pwallet->GetBalance();
    for (auto const& wb : walletbalances) {
        balances.pushKV(wb.first.toHexString(), wb.first.type == TokenTypes::NONE ? ValueFromAmount(wb.second) : wb.second);
    };

    TxColoredCoinBalancesMap walletunconfirmedbalances = pwallet->GetUnconfirmedBalance();
    for (auto const& uwb : walletunconfirmedbalances) {
        unconfirmBalancesObj.pushKV(uwb.first.toHexString(), uwb.first.type == TokenTypes::NONE ? ValueFromAmount(uwb.second) : uwb.second);
    };

    size_t kpExternalSize = pwallet->KeypoolCountExternalKeys();
    obj.pushKV("walletname", pwallet->GetName());
    obj.pushKV("walletversion", pwallet->GetVersion());
    obj.pushKV("balance", balances);
    obj.pushKV("unconfirmed_balance", unconfirmBalancesObj);
    obj.pushKV("txcount",       (int)pwallet->mapWallet.size());
    obj.pushKV("keypoololdest", pwallet->GetOldestKeyPoolTime());
    obj.pushKV("keypoolsize", (int64_t)kpExternalSize);
    CKeyID seed_id = pwallet->GetHDChain().seed_id;
    if (!seed_id.IsNull() && pwallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
        obj.pushKV("keypoolsize_hd_internal",   (int64_t)(pwallet->GetKeyPoolSize() - kpExternalSize));
    }
    if (pwallet->IsCrypted()) {
        obj.pushKV("unlocked_until", pwallet->nRelockTime);
    }
    obj.pushKV("paytxfee", ValueFromAmount(pwallet->m_pay_tx_fee.GetFeePerK()));
    if (!seed_id.IsNull()) {
        obj.pushKV("hdseedid", seed_id.GetHex());
        obj.pushKV("hdmasterkeyid", seed_id.GetHex());
    }
    obj.pushKV("private_keys_enabled", !pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS));
    return obj;
}

static UniValue listwallets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "listwallets\n"
            "Returns a list of currently loaded wallets.\n"
            "For full information on the wallet, use \"getwalletinfo\"\n"
            "\nResult:\n"
            "[                         (json array of strings)\n"
            "  \"walletname\"            (string) the wallet name\n"
            "   ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listwallets", "")
            + HelpExampleRpc("listwallets", "")
        );

    UniValue obj(UniValue::VARR);

    for (const std::shared_ptr<CWallet>& wallet : GetWallets()) {
        if (!EnsureWalletIsAvailable(wallet.get(), request.fHelp)) {
            return NullUniValue;
        }

        LOCK(wallet->cs_wallet);

        obj.push_back(wallet->GetName());
    }

    return obj;
}

static UniValue loadwallet(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "loadwallet \"filename\"\n"
            "\nLoads a wallet from a wallet file or directory."
            "\nNote that all wallet command-line options used when starting bitcoind will be"
            "\napplied to the new wallet (eg -zapwallettxes, upgradewallet, rescan, etc).\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet directory or .dat file.\n"
            "\nResult:\n"
            "{\n"
            "  \"name\" :    <wallet_name>,        (string) The wallet name if loaded successfully.\n"
            "  \"warning\" : <warning>,            (string) Warning message if wallet was not loaded cleanly.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("loadwallet", "\"test.dat\"")
            + HelpExampleRpc("loadwallet", "\"test.dat\"")
        );
    std::string wallet_file = request.params[0].get_str();
    std::string error;

    fs::path wallet_path = fs::path(wallet_file).is_absolute() ? fs::path(wallet_file) : GetWalletDir() / fs::path(wallet_file);

    if (fs::symlink_status(wallet_path).type() == std::filesystem::file_type::not_found) {
        throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Wallet " + wallet_file + " not found.");
    } else if (fs::is_directory(wallet_path)) {
        // The given filename is a directory. Check that there's a wallet.dat file.
        fs::path wallet_dat_file = wallet_path / "wallet.dat";
        if (fs::symlink_status(wallet_dat_file).type() == std::filesystem::file_type::not_found) {
            throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Directory " + wallet_file + " does not contain a wallet.dat file.");
        }
    }

    std::string warning;
    if (!CWallet::Verify(wallet_file, false, error, warning)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet file verification failed: " + error);
    }

    std::shared_ptr<CWallet> const wallet = CWallet::CreateWalletFromFile(wallet_file, wallet_path);
    if (!wallet) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet loading failed.");
    }
    AddWallet(wallet);

    wallet->postInitProcess();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("name", wallet->GetName());
    obj.pushKV("warning", warning);

    return obj;
}

static UniValue createwallet(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "createwallet \"wallet_name\" ( disable_private_keys )\n"
            "\nCreates and loads a new wallet.\n"
            "\nArguments:\n"
            "1. \"wallet_name\"          (string, required) The name for the new wallet. If this is a path, the wallet will be created at the path location.\n"
            "2. disable_private_keys   (boolean, optional, default: false) Disable the possibility of private keys (only watchonlys are possible in this mode).\n"
            "\nResult:\n"
            "{\n"
            "  \"name\" :    <wallet_name>,        (string) The wallet name if created successfully. If the wallet was created using a full path, the wallet_name will be the full path.\n"
            "  \"warning\" : <warning>,            (string) Warning message if wallet was not loaded cleanly.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("createwallet", "\"testwallet\"")
            + HelpExampleRpc("createwallet", "\"testwallet\"")
        );
    }
    std::string wallet_name = request.params[0].get_str();
    std::string error;
    std::string warning;

    bool disable_privatekeys = false;
    if (!request.params[1].isNull()) {
        disable_privatekeys = request.params[1].get_bool();
    }

    fs::path wallet_path = fs::path(wallet_name).is_absolute() ? fs::path(wallet_name) : GetWalletDir() / fs::path(wallet_name);

    if (fs::symlink_status(wallet_path).type() != std::filesystem::file_type::not_found) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet " + wallet_name + " already exists.");
    }

    // Wallet::Verify will check if we're trying to create a wallet with a duplication name.
    if (!CWallet::Verify(wallet_name, false, error, warning)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet file verification failed: " + error);
    }

    std::shared_ptr<CWallet> const wallet = CWallet::CreateWalletFromFile(wallet_name, wallet_path, (disable_privatekeys ? (uint64_t)WALLET_FLAG_DISABLE_PRIVATE_KEYS : 0));
    if (!wallet) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet creation failed.");
    }
    AddWallet(wallet);

    wallet->postInitProcess();

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("name", wallet->GetName());
    obj.pushKV("warning", warning);

    return obj;
}

static UniValue unloadwallet(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) {
        throw std::runtime_error(
            "unloadwallet ( \"wallet_name\" )\n"
            "Unloads the wallet referenced by the request endpoint otherwise unloads the wallet specified in the argument.\n"
            "Specifying the wallet name on a wallet endpoint is invalid."
            "\nArguments:\n"
            "1. \"wallet_name\"    (string, optional) The name of the wallet to unload.\n"
            "\nExamples:\n"
            + HelpExampleCli("unloadwallet", "wallet_name")
            + HelpExampleRpc("unloadwallet", "wallet_name")
        );
    }

    std::string wallet_name;
    if (GetWalletNameFromJSONRPCRequest(request, wallet_name)) {
        if (!request.params[0].isNull()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot unload the requested wallet");
        }
    } else {
        wallet_name = request.params[0].get_str();
    }

    std::shared_ptr<CWallet> wallet = GetWallet(wallet_name);
    if (!wallet) {
        throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Requested wallet does not exist or is not loaded");
    }

    // Release the "main" shared pointer and prevent further notifications.
    // Note that any attempt to load the same wallet would fail until the wallet
    // is destroyed (see CheckUniqueFileid).
    if (!RemoveWallet(wallet)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Requested wallet already unloaded");
    }
    UnregisterValidationInterface(wallet.get());

    // The wallet can be in use so it's not possible to explicitly unload here.
    // Just notify the unload intent so that all shared pointers are released.
    // The wallet will be destroyed once the last shared pointer is released.
    wallet->NotifyUnload();

    // There's no point in waiting for the wallet to unload.
    // At this point this method should never fail. The unloading could only
    // fail due to an unexpected error which would cause a process termination.

    return NullUniValue;
}

static UniValue resendwallettransactions(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "resendwallettransactions\n"
            "Immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "Intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "Returns an RPC error if -walletbroadcast is set to false.\n"
            "Returns array of transaction ids that were re-broadcast.\n"
            );

    if (!g_connman)
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->GetBroadcastTransactions()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet transaction broadcasting is disabled with -walletbroadcast");
    }

    std::vector<uint256> txids = pwallet->ResendWalletTransactionsBefore(GetTime(), g_connman.get());
    UniValue result(UniValue::VARR);
    for (const uint256& txid : txids)
    {
        result.push_back(txid.ToString());
    }
    return result;
}

static UniValue listunspent(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 5)
        throw std::runtime_error(
            "listunspent ( minconf maxconf  [\"addresses\",...] [include_unsafe] [query_options])\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"      (string) A json array of tapyrus addresses to filter\n"
            "    [\n"
            "      \"address\"     (string) tapyrus address\n"
            "      ,...\n"
            "    ]\n"
            "4. include_unsafe (bool, optional, default=true) Include outputs that are not safe to spend\n"
            "                  See description of \"safe\" attribute below.\n"
            "5. query_options    (json, optional) JSON with query options\n"
            "    {\n"
            "      \"minimumAmount\"    (numeric or string, default=0) Minimum value of each UTXO in " + CURRENCY_UNIT + "\n"
            "      \"maximumAmount\"    (numeric or string, default=unlimited) Maximum value of each UTXO in " + CURRENCY_UNIT + "\n"
            "      \"maximumCount\"     (numeric or string, default=unlimited) Maximum number of UTXOs\n"
            "      \"minimumSumAmount\" (numeric or string, default=unlimited) Minimum sum value of all UTXOs in " + CURRENCY_UNIT + "\n"
            "      \"only_token\" (boolean, default=false) Filter and return only tokens in the output\n"
            "    }\n"
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"address\" : \"address\",    (string) the tapyrus address\n"
            "    \"label\" : \"label\",        (string) The associated label, or \"\" for the default label\n"
            "    \"scriptPubKey\" : \"key\",   (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction output amount in " + CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "    \"spendable\" : xxx,        (bool) Whether we have the private keys to spend this output\n"
            "    \"solvable\" : xxx,         (bool) Whether we know how to spend this output, ignoring the lack of keys\n"
            "    \"safe\" : xxx              (bool) Whether this output is considered safe to spend. Unconfirmed transactions\n"
            "                              from outside keys and unconfirmed replacement transactions are considered unsafe\n"
            "                              and are not eligible for spending by fundrawtransaction and sendtoaddress.\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleCli("listunspent", "6 9999999 '[]' true '{ \"minimumAmount\": 0.005 }'")
            + HelpExampleRpc("listunspent", "6, 9999999, [] , true, { \"minimumAmount\": 0.005 } ")
            + HelpExampleCli("listunspent", "6 9999999 '[]' true '{ \"only_token\": true }'")
            + HelpExampleRpc("listunspent", "6, 9999999, [] , true, { \"only_token\": true} ")
        );

    int nMinDepth = 1;
    if (!request.params[0].isNull()) {
        RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
        nMinDepth = request.params[0].get_int();
    }

    int nMaxDepth = 9999999;
    if (!request.params[1].isNull()) {
        RPCTypeCheckArgument(request.params[1], UniValue::VNUM);
        nMaxDepth = request.params[1].get_int();
    }

    std::set<CTxDestination> destinations;
    if (!request.params[2].isNull()) {
        RPCTypeCheckArgument(request.params[2], UniValue::VARR);
        UniValue inputs = request.params[2].get_array();
        for (unsigned int idx = 0; idx < inputs.size(); idx++) {
            const UniValue& input = inputs[idx];
            CTxDestination dest = DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Tapyrus address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    bool include_unsafe = true;
    if (!request.params[3].isNull()) {
        RPCTypeCheckArgument(request.params[3], UniValue::VBOOL);
        include_unsafe = request.params[3].get_bool();
    }

    CAmount nMinimumAmount = 0;
    CAmount nMaximumAmount = MAX_MONEY;
    CAmount nMinimumSumAmount = MAX_MONEY;
    uint64_t nMaximumCount = 0;
    bool only_token = false;

    if (!request.params[4].isNull()) {
        const UniValue& options = request.params[4].get_obj();

        if (options.exists("minimumAmount"))
            nMinimumAmount = AmountFromValue(options["minimumAmount"]);

        if (options.exists("maximumAmount"))
            nMaximumAmount = AmountFromValue(options["maximumAmount"]);

        if (options.exists("minimumSumAmount"))
            nMinimumSumAmount = AmountFromValue(options["minimumSumAmount"]);

        if (options.exists("maximumCount"))
            nMaximumCount = options["maximumCount"].get_int64();

        if (options.exists("only_token"))
            only_token =  options["only_token"].get_bool();

    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    UniValue results(UniValue::VARR);
    std::vector<COutput> vecOutputs;
    {
        LOCK2(cs_main, pwallet->cs_wallet);
        pwallet->AvailableCoins(vecOutputs, !include_unsafe, nullptr, nMinimumAmount, nMaximumAmount, nMinimumSumAmount, nMaximumCount, nMinDepth, nMaxDepth, only_token);
    }

    LOCK(pwallet->cs_wallet);

    for (const COutput& out : vecOutputs) {
        CTxDestination address;
        const CScript& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        UniValue entry(UniValue::VOBJ);
        entry.pushKV("txid", out.tx->GetHash().GetHex());
        entry.pushKV("vout", out.i);

        if (fValidAddress) {
            entry.pushKV("address", EncodeDestination(address));

            addTokenKV(address, out.tx->tx->vout[out.i].nValue, entry);

            auto i = pwallet->mapAddressBook.find(address);
            if (i != pwallet->mapAddressBook.end()) {
                entry.pushKV("label", i->second.name);
            }

            if (scriptPubKey.IsPayToScriptHash()) {
                const CScriptID& hash = std::get<CScriptID>(address);
                CScript redeemScript;
                if (pwallet->GetCScript(hash, redeemScript)) {
                    entry.pushKV("redeemScript", HexStr(redeemScript.begin(), redeemScript.end()));
                }
            }
        }

        entry.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));
        entry.pushKV("confirmations", out.nDepth);
        entry.pushKV("spendable", out.fSpendable);
        entry.pushKV("solvable", out.fSolvable);
        entry.pushKV("safe", out.fSafe);
        results.push_back(entry);
    }

    return results;
}

void FundTransaction(CWallet* const pwallet, CMutableTransaction& tx, CAmount& fee_out, CWallet::ChangePosInOut& change_position, UniValue options)
{
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    CCoinControl coinControl;
    int tpc_change_position = -1;
    bool lockUnspents = false;
    UniValue subtractFeeFromOutputs;
    std::set<int> setSubtractFeeFromOutputs;

    if (!options.isNull()) {
      if (options.type() == UniValue::VBOOL) {
        // backward compatibility bool only fallback
        coinControl.fAllowWatchOnly = options.get_bool();
      }
      else {
        RPCTypeCheckArgument(options, UniValue::VOBJ);
        RPCTypeCheckObj(options,
            {
                {"changeAddress", UniValueType(UniValue::VSTR)},
                {"changePosition", UniValueType(UniValue::VNUM)},
                {"includeWatching", UniValueType(UniValue::VBOOL)},
                {"lockUnspents", UniValueType(UniValue::VBOOL)},
                {"feeRate", UniValueType()}, // will be checked below
                {"subtractFeeFromOutputs", UniValueType(UniValue::VARR)},
                {"replaceable", UniValueType(UniValue::VBOOL)},
                {"conf_target", UniValueType(UniValue::VNUM)},
                {"estimate_mode", UniValueType(UniValue::VSTR)},
            },
            true, true);

        if (options.exists("changeAddress")) {
            CTxDestination dest = DecodeDestination(options["changeAddress"].get_str());

            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "changeAddress must be a valid tapyrus address");
            }

            coinControl.destChange = dest;
        }

        if (options.exists("changePosition"))
            tpc_change_position = options["changePosition"].get_int();


        coinControl.m_change_type = pwallet->m_default_change_type;

        if (options.exists("includeWatching"))
            coinControl.fAllowWatchOnly = options["includeWatching"].get_bool();

        if (options.exists("lockUnspents"))
            lockUnspents = options["lockUnspents"].get_bool();

        if (options.exists("feeRate"))
        {
            coinControl.m_feerate = CFeeRate(AmountFromValue(options["feeRate"]));
            coinControl.fOverrideFeeRate = true;
        }

        if (options.exists("subtractFeeFromOutputs"))
            subtractFeeFromOutputs = options["subtractFeeFromOutputs"].get_array();

        if (options.exists("replaceable")) {
            coinControl.m_signal_bip125_rbf = options["replaceable"].get_bool();
        }
        if (options.exists("conf_target")) {
            if (options.exists("feeRate")) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify both conf_target and feeRate");
            }
            coinControl.m_confirm_target = ParseConfirmTarget(options["conf_target"]);
        }
        if (options.exists("estimate_mode")) {
            if (options.exists("feeRate")) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot specify both estimate_mode and feeRate");
            }
            if (!FeeModeFromString(options["estimate_mode"].get_str(), coinControl.m_fee_mode)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
            }
        }
      }
    }

    if (tx.vout.size() == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "TX must have at least one output");

    if (tpc_change_position != -1 && (tpc_change_position < 0 || (unsigned int)tpc_change_position > tx.vout.size()))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "changePosition out of bounds");

    for (unsigned int idx = 0; idx < subtractFeeFromOutputs.size(); idx++) {
        int pos = subtractFeeFromOutputs[idx].get_int();
        if (setSubtractFeeFromOutputs.count(pos))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, duplicated position: %d", pos));
        if (pos < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, negative position: %d", pos));
        if (pos >= int(tx.vout.size()))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, position too large: %d", pos));
        setSubtractFeeFromOutputs.insert(pos);
    }

    std::string strFailReason;

    //if we are funding token transaction make sure to mark the coincontrol as transfer
    for(auto& vout:tx.vout)
    {
        ColorIdentifier colorId = GetColorIdFromScript(vout.scriptPubKey);
        if(colorId.type != TokenTypes::NONE)
        {
            coinControl.m_colorTxType = ColoredTxType::TRANSFER;
            coinControl.m_colorId = colorId;
            break;
        }
    }
    change_position[ColorIdentifier()] =  tpc_change_position;
    if (!pwallet->FundTransaction(tx, fee_out, change_position, strFailReason, lockUnspents, setSubtractFeeFromOutputs, coinControl)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strFailReason);
    }
}

static UniValue fundrawtransaction(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error(
                            "fundrawtransaction \"hexstring\" ( options )\n"
                            "\nAdd inputs to a transaction until it has enough in value to meet its out value.\n"
                            "This will not modify existing inputs, and will add at most one change output to the outputs.\n"
                            "No existing outputs will be modified unless \"subtractFeeFromOutputs\" is specified.\n"
                            "Note that inputs which were signed may need to be resigned after completion since in/outputs have been added.\n"
                            "The inputs added will not be signed, use signrawtransaction for that.\n"
                            "Note that all existing inputs must have their previous output transaction be in the wallet.\n"
                            "Note that all inputs selected must be of standard form and P2SH scripts must be\n"
                            "in the wallet using importaddress or addmultisigaddress (to calculate fees).\n"
                            "You can see whether this is the case by checking the \"solvable\" field in the listunspent output.\n"
                            "Only pay-to-pubkey, multisig, and P2SH versions thereof are currently supported for watch-only\n"
                            "\nArguments:\n"
                            "1. \"hexstring\"           (string, required) The hex string of the raw transaction\n"
                            "2. options                 (object, optional)\n"
                            "   {\n"
                            "     \"changeAddress\"          (string, optional, default pool address) The tapyrus address to receive the change\n"
                            "     \"changePosition\"         (numeric, optional, default random) The index of the " + CURRENCY_UNIT + " change output\n"
                            "     \"includeWatching\"        (boolean, optional, default false) Also select inputs which are watch only\n"
                            "     \"lockUnspents\"           (boolean, optional, default false) Lock selected unspent outputs\n"
                            "     \"feeRate\"                (numeric, optional, default not set: makes wallet determine the fee) Set a specific fee rate in " + CURRENCY_UNIT + "/kB\n"
                            "     \"subtractFeeFromOutputs\" (array, optional) A json array of integers.\n"
                            "                              The fee will be equally deducted from the amount of each specified output.\n"
                            "                              The outputs are specified by their zero-based index, before any change output is added.\n"
                            "                              Those recipients will receive less TPC than you enter in their corresponding amount field.\n"
                            "                              If no outputs are specified here, the sender pays the fee.\n"
                            "                                  [vout_index,...]\n"
                            "     \"replaceable\"            (boolean, optional) Marks this transaction as BIP125 replaceable.\n"
                            "                              Allows this transaction to be replaced by a transaction with higher fees\n"
                            "     \"conf_target\"            (numeric, optional) Confirmation target (in blocks)\n"
                            "     \"estimate_mode\"          (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
                            "         \"UNSET\"\n"
                            "         \"ECONOMICAL\"\n"
                            "         \"CONSERVATIVE\"\n"
                            "   }\n"
                            "                         for backward compatibility: passing in a true instead of an object will result in {\"includeWatching\":true}\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"hex\":       \"value\", (string)  The resulting raw transaction (hex-encoded string)\n"
                            "  \"fee\":       n,         (numeric) Fee in " + CURRENCY_UNIT + " the resulting transaction pays\n"
                            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
                            "}\n"
                            "\nExamples:\n"
                            "\nCreate a transaction with no inputs\n"
                            + HelpExampleCli("createrawtransaction", "\"[]\" \"{\\\"myaddress\\\":0.01}\"") +
                            "\nAdd sufficient unsigned inputs to meet the output value\n"
                            + HelpExampleCli("fundrawtransaction", "\"rawtransactionhex\"") +
                            "\nSign the transaction\n"
                            + HelpExampleCli("signrawtransaction", "\"fundedtransactionhex\"") +
                            "\nSend the transaction\n"
                            + HelpExampleCli("sendrawtransaction", "\"signedtransactionhex\"")
                            );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValueType(), UniValue::VBOOL});

    // parse hex string from parameter
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    CAmount fee;
    CWallet::ChangePosInOut change_position;
    FundTransaction(pwallet, tx, fee, change_position, request.params[1]);

    UniValue result(UniValue::VOBJ);
    result.pushKV("hex", EncodeHexTx(tx));
    result.pushKV("fee", ValueFromAmount(fee));
    result.pushKV("changepos", change_position[ColorIdentifier()]);

    return result;
}

UniValue signrawtransactionwithwallet(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "signrawtransactionwithwallet \"hexstring\" ( [{\"txid\":\"id\",\"vout\":n,\"scriptPubKey\":\"hex\",\"redeemScript\":\"hex\"},...] sighashtype )\n"
            "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
            "The second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            + HelpRequiringPassphrase(pwallet) + "\n"

            "\nArguments:\n"
            "1. \"hexstring\"                      (string, required) The transaction hex string\n"
            "2. \"prevtxs\"                        (string, optional) An json array of previous dependent transaction outputs\n"
            "     [                              (json array of json objects, or 'null' if none provided)\n"
            "       {\n"
            "         \"txid\":\"id\",               (string, required) The transaction id\n"
            "         \"vout\":n,                  (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",     (string, required) script key\n"
            "         \"redeemScript\": \"hex\",     (string, required for P2SH or P2WSH) redeem script\n"
            "         \"amount\": value            (numeric, required) The amount spent\n"
            "       }\n"
            "       ,...\n"
            "    ]\n"
            "3. \"sighashtype\"                    (string, optional, default=ALL) The signature hash type. Must be one of\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"
            "4. \"sigscheme\"                    (string, optional, default=ECDSA) The signature scheme to use for this transaction\n"
            "       \"ECDSA\"\n"
            "       \"SCHNORR\"\n"

            "\nResult:\n"
            "{\n"
            "  \"hex\" : \"value\",                  (string) The hex-encoded raw transaction with signature(s)\n"
            "  \"complete\" : true|false,          (boolean) If the transaction has a complete set of signatures\n"
            "  \"errors\" : [                      (json array of objects) Script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"id\",              (string) The transaction id of the referenced, previous transaction\n"
            "      \"vout\" : n,                   (numeric) The index of the output to spent and used as input\n"
            "      \"scriptSig\" : \"hex\",          (string) The hex-encoded signature script\n"
            "      \"sequence\" : n,               (numeric) Script sequence number\n"
            "      \"error\" : \"text\"              (string) Verification or signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "  \"warning\" : \"token burn detected\" (string, optional) warning only in burn token transactions \n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("signrawtransactionwithwallet", "\"myhex\"")
            + HelpExampleRpc("signrawtransactionwithwallet", "\"myhex\"")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VARR, UniValue::VSTR}, true);

    CMutableTransaction mtx;
    if (!DecodeHexTx(mtx, request.params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    SignatureScheme sigScheme(SignatureScheme::ECDSA);
    if(!request.params[4].isNull() && request.params[3].get_str() == "SCHNORR")
        sigScheme = SignatureScheme::SCHNORR;

    // Sign the transaction
    LOCK2(cs_main, pwallet->cs_wallet);
    return SignTransaction(mtx, request.params[1], pwallet, false, request.params[2], sigScheme);
}

static UniValue bumpfee(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();


    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "bumpfee \"txid\" ( options ) \n"
            "\nBumps the fee of an opt-in-RBF transaction T, replacing it with a new transaction B.\n"
            "An opt-in RBF transaction with the given txid must be in the wallet.\n"
            "The command will pay the additional fee by decreasing (or perhaps removing) its change output.\n"
            "If the change output is not big enough to cover the increased fee, the command will currently fail\n"
            "instead of adding new inputs to compensate. (A future implementation could improve this.)\n"
            "The command will fail if the wallet or mempool contains a transaction that spends one of T's outputs.\n"
            "By default, the new fee will be calculated automatically using estimatesmartfee.\n"
            "The user can specify a confirmation target for estimatesmartfee.\n"
            "Alternatively, the user can specify totalFee, or use RPC settxfee to set a higher fee rate.\n"
            "At a minimum, the new fee rate must be high enough to pay an additional new relay fee (incrementalfee\n"
            "returned by getnetworkinfo) to enter the node's mempool.\n"
            "\nArguments:\n"
            "1. txid                  (string, required) The txid to be bumped\n"
            "2. options               (object, optional)\n"
            "   {\n"
            "     \"confTarget\"        (numeric, optional) Confirmation target (in blocks)\n"
            "     \"totalFee\"          (numeric, optional) Total fee (NOT feerate) to pay, in tapyrus.\n"
            "                         In rare cases, the actual fee paid might be slightly higher than the specified\n"
            "                         totalFee if the tx change output has to be removed because it is too close to\n"
            "                         the dust threshold.\n"
            "     \"replaceable\"       (boolean, optional, default true) Whether the new transaction should still be\n"
            "                         marked bip-125 replaceable. If true, the sequence numbers in the transaction will\n"
            "                         be left unchanged from the original. If false, any input sequence numbers in the\n"
            "                         original transaction that were less than 0xfffffffe will be increased to 0xfffffffe\n"
            "                         so the new transaction will not be explicitly bip-125 replaceable (though it may\n"
            "                         still be replaceable in practice, for example if it has unconfirmed ancestors which\n"
            "                         are replaceable).\n"
            "     \"estimate_mode\"     (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "         \"UNSET\"\n"
            "         \"ECONOMICAL\"\n"
            "         \"CONSERVATIVE\"\n"
            "   }\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\":    \"value\",   (string)  The id of the new transaction\n"
            "  \"origfee\":  n,         (numeric) Fee of the replaced transaction\n"
            "  \"fee\":      n,         (numeric) Fee of the new transaction\n"
            "  \"errors\":  [ str... ] (json array of strings) Errors encountered during processing (may be empty)\n"
            "}\n"
            "\nExamples:\n"
            "\nBump the fee, get the new transaction\'s txid\n" +
            HelpExampleCli("bumpfee", "<txid>"));
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VOBJ});
    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    // optional parameters
    CAmount totalFee = 0;
    CCoinControl coin_control;
    coin_control.m_signal_bip125_rbf = true;
    if (!request.params[1].isNull()) {
        UniValue options = request.params[1];
        RPCTypeCheckObj(options,
            {
                {"confTarget", UniValueType(UniValue::VNUM)},
                {"totalFee", UniValueType(UniValue::VNUM)},
                {"replaceable", UniValueType(UniValue::VBOOL)},
                {"estimate_mode", UniValueType(UniValue::VSTR)},
            },
            true, true);

        if (options.exists("confTarget") && options.exists("totalFee")) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "confTarget and totalFee options should not both be set. Please provide either a confirmation target for fee estimation or an explicit total fee for the transaction.");
        } else if (options.exists("confTarget")) { // TODO: alias this to conf_target
            coin_control.m_confirm_target = ParseConfirmTarget(options["confTarget"]);
        } else if (options.exists("totalFee")) {
            totalFee = options["totalFee"].get_int64();
            if (totalFee <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid totalFee %s (must be greater than 0)", FormatMoney(totalFee)));
            }
        }

        if (options.exists("replaceable")) {
            coin_control.m_signal_bip125_rbf = options["replaceable"].get_bool();
        }
        if (options.exists("estimate_mode")) {
            if (!FeeModeFromString(options["estimate_mode"].get_str(), coin_control.m_fee_mode)) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid estimate_mode parameter");
            }
        }
    }

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);


    std::vector<std::string> errors;
    CAmount old_fee;
    CAmount new_fee;
    CMutableTransaction mtx;
    feebumper::Result res = feebumper::CreateTransaction(pwallet, hash, coin_control, totalFee, errors, old_fee, new_fee, mtx);
    if (res != feebumper::Result::OK) {
        switch(res) {
            case feebumper::Result::INVALID_ADDRESS_OR_KEY:
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, errors[0]);
                break;
            case feebumper::Result::INVALID_REQUEST:
                throw JSONRPCError(RPC_INVALID_REQUEST, errors[0]);
                break;
            case feebumper::Result::INVALID_PARAMETER:
                throw JSONRPCError(RPC_INVALID_PARAMETER, errors[0]);
                break;
            case feebumper::Result::WALLET_ERROR:
                throw JSONRPCError(RPC_WALLET_ERROR, errors[0]);
                break;
            default:
                throw JSONRPCError(RPC_MISC_ERROR, errors[0]);
                break;
        }
    }

    // sign bumped transaction
    if (!feebumper::SignTransaction(pwallet, mtx)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Can't sign transaction.");
    }
    // commit the bumped transaction
    uint256 txid;
    if (feebumper::CommitTransaction(pwallet, hash, std::move(mtx), errors, txid) != feebumper::Result::OK) {
        throw JSONRPCError(RPC_WALLET_ERROR, errors[0]);
    }
    UniValue result(UniValue::VOBJ);
    result.pushKV("txid", txid.GetHex());
    result.pushKV("origfee", ValueFromAmount(old_fee));
    result.pushKV("fee", ValueFromAmount(new_fee));
    UniValue result_errors(UniValue::VARR);
    for (const std::string& error : errors) {
        result_errors.push_back(error);
    }
    result.pushKV("errors", result_errors);

    return result;
}

UniValue generate(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();


    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "generate nblocks ( )\n"
            "\nMine up to nblocks blocks immediately (before the RPC call returns) to an address in the wallet.\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. private key  (string, required) The aggregate private key to sign the generated blocks. The format is WIF.\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks signed with 1 privatekey\n"
            + HelpExampleCli("generate", "11  \"L3wNcWW97SZYV6fb6b5VgoEvtybc9d1Uu6LeiQkVN4Z38VTivKT6\"")
        );
    }

    int num_generate = request.params[0].get_int();

    CKey cPrivKey = DecodeSecret(request.params[1].get_str());
    if(!cPrivKey.IsValid())
        throw JSONRPCError(RPC_WALLET_INVALID_PRIVATE_KEY, "No private key given or invalid private key.");

    XFieldAggPubKey aggpubkeyChange;
    CXFieldHistory().GetLatest(TAPYRUS_XFIELDTYPES::AGGPUBKEY, aggpubkeyChange);

    if(cPrivKey.GetPubKey() != aggpubkeyChange.getPubKey())
        throw JSONRPCError(RPC_WALLET_INVALID_AGGREGATE_KEY, "Given private key doesn't correspond to the Aggregate Key.");

    std::shared_ptr<CReserveScript> coinbase_script;
    pwallet->GetScriptForMining(coinbase_script);

    // If the keypool is exhausted, no script is returned at all.  Catch this.
    if (!coinbase_script) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    //throw an error if no script was provided
    if (coinbase_script->reserveScript.empty()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No coinbase script available");
    }

    return generateBlocks(coinbase_script, num_generate, true, cPrivKey);
}

UniValue rescanblockchain(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "rescanblockchain (\"start_height\") (\"stop_height\")\n"
            "\nRescan the local blockchain for wallet related transactions.\n"
            "\nArguments:\n"
            "1. \"start_height\"    (numeric, optional) block height where the rescan should start\n"
            "2. \"stop_height\"     (numeric, optional) the last block height that should be scanned\n"
            "\nResult:\n"
            "{\n"
            "  \"start_height\"     (numeric) The block height where the rescan has started. If omitted, rescan started from the genesis block.\n"
            "  \"stop_height\"      (numeric) The height of the last rescanned block. If omitted, rescan stopped at the chain tip.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("rescanblockchain", "100000 120000")
            + HelpExampleRpc("rescanblockchain", "100000, 120000")
            );
    }

    WalletRescanReserver reserver(pwallet);
    if (!reserver.reserve()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet is currently rescanning. Abort existing rescan or wait.");
    }

    CBlockIndex *pindexStart = nullptr;
    CBlockIndex *pindexStop = nullptr;
    CBlockIndex *pChainTip = nullptr;
    {
        LOCK(cs_main);
        pindexStart = chainActive.Genesis();
        pChainTip = chainActive.Tip();

        if (!request.params[0].isNull()) {
            pindexStart = chainActive[request.params[0].get_int()];
            if (!pindexStart) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid start_height");
            }
        }

        if (!request.params[1].isNull()) {
            pindexStop = chainActive[request.params[1].get_int()];
            if (!pindexStop) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid stop_height");
            }
            else if (pindexStop->nHeight < pindexStart->nHeight) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "stop_height must be greater than start_height");
            }
        }
    }

    // We can't rescan beyond non-pruned blocks, stop and throw an error
    if (fPruneMode) {
        LOCK(cs_main);
        CBlockIndex *block = pindexStop ? pindexStop : pChainTip;
        while (block && block->nHeight >= pindexStart->nHeight) {
            if (!(block->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPC_MISC_ERROR, "Can't rescan beyond pruned data. Use RPC call getblockchaininfo to determine your pruned height.");
            }
            block = block->pprev;
        }
    }

    CBlockIndex *stopBlock = pwallet->ScanForWalletTransactions(pindexStart, pindexStop, reserver, true);
    if (!stopBlock) {
        if (pwallet->IsAbortingRescan()) {
            throw JSONRPCError(RPC_MISC_ERROR, "Rescan aborted.");
        }
        // if we got a nullptr returned, ScanForWalletTransactions did rescan up to the requested stopindex
        stopBlock = pindexStop ? pindexStop : pChainTip;
    }
    else {
        throw JSONRPCError(RPC_MISC_ERROR, "Rescan failed. Potentially corrupted data files.");
    }
    UniValue response(UniValue::VOBJ);
    response.pushKV("start_height", pindexStart->nHeight);
    response.pushKV("stop_height", stopBlock->nHeight);
    return response;
}

class DescribeWalletAddressVisitor
{
public:
    CWallet * const pwallet;

    void ProcessSubScript(const CScript& subscript, UniValue& obj, bool include_addresses = false) const
    {
        // Always present: script type and redeemscript
        txnouttype which_type;
        std::vector<std::vector<unsigned char>> solutions_data;
        Solver(subscript, which_type, solutions_data);
        obj.pushKV("script", GetTxnOutputType(which_type));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));

        CTxDestination embedded;
        UniValue a(UniValue::VARR);
        if (ExtractDestination(subscript, embedded)) {
            // Only when the script corresponds to an address.
            UniValue subobj(UniValue::VOBJ);
            UniValue detail = DescribeAddress(embedded);
            subobj.pushKVs(detail);
            UniValue wallet_detail = std::visit(*this, embedded);
            subobj.pushKVs(wallet_detail);
            subobj.pushKV("address", EncodeDestination(embedded));
            subobj.pushKV("scriptPubKey", HexStr(subscript.begin(), subscript.end()));
            // Always report the pubkey at the top level, so that `getnewaddress()['pubkey']` always works.
            if (subobj.exists("pubkey")) obj.pushKV("pubkey", subobj["pubkey"]);
            obj.pushKV("embedded", std::move(subobj));
            if (include_addresses) a.push_back(EncodeDestination(embedded));
        } else if (which_type == TX_MULTISIG) {
            // Also report some information on multisig scripts (which do not have a corresponding address).
            // TODO: abstract out the common functionality between this logic and ExtractDestinations.
            obj.pushKV("sigsrequired", solutions_data[0][0]);
            UniValue pubkeys(UniValue::VARR);
            for (size_t i = 1; i < solutions_data.size() - 1; ++i) {
                CPubKey key(solutions_data[i].begin(), solutions_data[i].end());
                if (include_addresses) a.push_back(EncodeDestination(key.GetID()));
                pubkeys.push_back(HexStr(key.begin(), key.end()));
            }
            obj.pushKV("pubkeys", std::move(pubkeys));
        }

        // The "addresses" field is confusing because it refers to public keys using their P2PKH address.
        // For that reason, only add the 'addresses' field when needed for backward compatibility. New applications
        // can use the 'embedded'->'address' field for P2SH or P2WSH wrapped addresses, and 'pubkeys' for
        // inspecting multisig participants.
        if (include_addresses) obj.pushKV("addresses", std::move(a));
    }

    explicit DescribeWalletAddressVisitor(CWallet* _pwallet) : pwallet(_pwallet) {}

    UniValue operator()(const CNoDestination& dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        if (pwallet && pwallet->GetPubKey(keyID, vchPubKey)) {
            obj.pushKV("pubkey", HexStr(vchPubKey));
            obj.pushKV("iscompressed", vchPubKey.IsCompressed());
        }
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        if (pwallet && pwallet->GetCScript(scriptID, subscript)) {
            ProcessSubScript(subscript, obj, IsDeprecatedRPCEnabled("validateaddress"));
        }
        return obj;
    }
};

static UniValue DescribeWalletAddress(CWallet* pwallet, const CTxDestination& dest)
{
    UniValue ret(UniValue::VOBJ);
    UniValue detail = DescribeAddress(dest);
    ret.pushKVs(detail);
    ret.pushKVs(std::visit(DescribeWalletAddressVisitor(pwallet), dest));
    return ret;
}

/** Convert CAddressBookData to JSON record.  */
static UniValue AddressBookDataToJSON(const CAddressBookData& data, const bool verbose)
{
    UniValue ret(UniValue::VOBJ);
    if (verbose) {
        ret.pushKV("name", data.name);
    }
    ret.pushKV("purpose", data.purpose);
    return ret;
}

UniValue getaddressinfo(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "getaddressinfo \"address\"\n"
            "\nReturn information about the given tapyrus address. Some information requires the address\n"
            "to be in the wallet.\n"
            "\nArguments:\n"
            "1. \"address\"                    (string, required) The tapyrus address to get the information of.\n"
            "\nResult:\n"
            "{\n"
            "  \"address\" : \"address\",        (string) The tapyrus address validated\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex encoded scriptPubKey generated by the address\n"
            "  \"ismine\" : true|false,        (boolean) If the address is yours or not\n"
            "  \"iswatchonly\" : true|false,   (boolean) If the address is watchonly\n"
            "  \"isscript\" : true|false,      (boolean) If the key is a script\n"
            "  \"hex\" : \"hex\",                (string, optional) The redeemscript for the p2sh address\n"
            "  \"pubkeys\"                     (string, optional) Array of pubkeys associated with the known redeemscript (only if \"script\" is \"multisig\")\n"
            "    [\n"
            "      \"pubkey\"\n"
            "      ,...\n"
            "    ]\n"
            "  \"sigsrequired\" : xxxxx        (numeric, optional) Number of signatures required to spend multisig output (only if \"script\" is \"multisig\")\n"
            "  \"pubkey\" : \"publickeyhex\",    (string, optional) The hex value of the raw public key, for single-key addresses (possibly embedded in P2SH)\n"
            "  \"embedded\" : {...},           (object, optional) Information about the address embedded in P2SH, if relevant and known. It includes all getaddressinfo output fields for the embedded address, excluding metadata (\"timestamp\", \"hdkeypath\", \"hdseedid\") and relation to the wallet (\"ismine\", \"iswatchonly\", \"account\").\n"
            "  \"iscompressed\" : true|false,  (boolean) If the address is compressed\n"
            "  \"label\" :  \"label\"         (string) The label associated with the address\n"
            "  \"timestamp\" : timestamp,      (number, optional) The creation time of the key if available in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"hdkeypath\" : \"keypath\"       (string, optional) The HD keypath if the key is HD and available\n"
            "  \"hdseedid\" : \"<hash160>\"      (string, optional) The Hash160 of the HD seed\n"
            "  \"hdmasterkeyid\" : \"<hash160>\" (string, optional) alias for hdseedid maintained for backwards compatibility. Will be removed in V0.18.\n"
            "  \"labels\"                      (object) Array of labels associated with the address.\n"
            "    [\n"
            "      { (json object of label data)\n"
            "        \"name\": \"labelname\" (string) The label\n"
            "        \"purpose\": \"string\" (string) Purpose of address (\"send\" for sending address, \"receive\" for receiving address)\n"
            "      },...\n"
            "    ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressinfo", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
            + HelpExampleRpc("getaddressinfo", "\"1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc\"")
        );
    }

    LOCK(pwallet->cs_wallet);

    UniValue ret(UniValue::VOBJ);
    CTxDestination dest = DecodeDestination(request.params[0].get_str());

    // Make sure the destination is valid
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    std::string currentAddress = EncodeDestination(dest);
    ret.pushKV("address", currentAddress);

    ColorIdentifier colorId;
    if(dest.index() == 3)
        colorId = std::get<CColorKeyID>(dest).color;
    else if(dest.index() == 4)
        colorId = std::get<CColorScriptID>(dest).color;

    ret.pushKV("token", colorId.toHexString());

    CScript scriptPubKey = GetScriptForDestination(dest);
    ret.pushKV("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end()));

    isminetype mine = IsMine(*pwallet, dest);
    ret.pushKV("ismine", bool(mine & ISMINE_SPENDABLE));
    ret.pushKV("iswatchonly", bool(mine & ISMINE_WATCH_ONLY));
    UniValue detail = DescribeWalletAddress(pwallet, dest);
    ret.pushKVs(detail);
    if (pwallet->mapAddressBook.count(dest)) {
        ret.pushKV("label", pwallet->mapAddressBook[dest].name);
    }
    const CKeyMetadata* meta = nullptr;
    CKeyID key_id = GetKeyForDestination(*pwallet, dest);
    if (!key_id.IsNull()) {
        auto it = pwallet->mapKeyMetadata.find(key_id);
        if (it != pwallet->mapKeyMetadata.end()) {
            meta = &it->second;
        }
    }
    if (!meta) {
        auto it = pwallet->m_script_metadata.find(CScriptID(scriptPubKey));
        if (it != pwallet->m_script_metadata.end()) {
            meta = &it->second;
        }
    }
    if (meta) {
        ret.pushKV("timestamp", meta->nCreateTime);
        if (!meta->hdKeypath.empty()) {
            ret.pushKV("hdkeypath", meta->hdKeypath);
            ret.pushKV("hdseedid", meta->hd_seed_id.GetHex());
            ret.pushKV("hdmasterkeyid", meta->hd_seed_id.GetHex());
        }
    }

    // Currently only one label can be associated with an address, return an array
    // so the API remains stable if we allow multiple labels to be associated with
    // an address.
    UniValue labels(UniValue::VARR);
    std::map<CTxDestination, CAddressBookData>::iterator mi = pwallet->mapAddressBook.find(dest);
    if (mi != pwallet->mapAddressBook.end()) {
        labels.push_back(AddressBookDataToJSON(mi->second, true));
    }
    ret.pushKV("labels", std::move(labels));

    return ret;
}

static UniValue getaddressesbylabel(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getaddressesbylabel \"label\"\n"
            "\nReturns the list of addresses assigned the specified label.\n"
            "\nArguments:\n"
            "1. \"label\"  (string, required) The label.\n"
            "\nResult:\n"
            "{ (json object with addresses as keys)\n"
            "  \"address\": { (json object with information about address)\n"
            "    \"purpose\": \"string\" (string)  Purpose of address (\"send\" for sending address, \"receive\" for receiving address)\n"
            "  },...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbylabel", "\"tabby\"")
            + HelpExampleRpc("getaddressesbylabel", "\"tabby\"")
        );

    LOCK(pwallet->cs_wallet);

    std::string label = LabelFromValue(request.params[0]);

    // Find all addresses that have the given label
    UniValue ret(UniValue::VOBJ);
    for (const std::pair<const CTxDestination, CAddressBookData>& item : pwallet->mapAddressBook) {
        if (item.second.name == label) {
            ret.pushKV(EncodeDestination(item.first), AddressBookDataToJSON(item.second, false));
        }
    }

    if (ret.empty()) {
        throw JSONRPCError(RPC_WALLET_INVALID_LABEL_NAME, std::string("No addresses with label " + label));
    }

    return ret;
}

static UniValue listlabels(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "listlabels ( \"purpose\" )\n"
            "\nReturns the list of all labels, or labels that are assigned to addresses with a specific purpose.\n"
            "\nArguments:\n"
            "1. \"purpose\"    (string, optional) Address purpose to list labels for ('send','receive'). An empty string is the same as not providing this argument.\n"
            "\nResult:\n"
            "[               (json array of string)\n"
            "  \"label\",      (string) Label name\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            "\nList all labels\n"
            + HelpExampleCli("listlabels", "") +
            "\nList labels that have receiving addresses\n"
            + HelpExampleCli("listlabels", "receive") +
            "\nList labels that have sending addresses\n"
            + HelpExampleCli("listlabels", "send") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listlabels", "receive")
        );

    LOCK(pwallet->cs_wallet);

    std::string purpose;
    if (!request.params[0].isNull()) {
        purpose = request.params[0].get_str();
    }

    // Add to a set to sort by label name, then insert into Univalue array
    std::set<std::string> label_set;
    for (const std::pair<const CTxDestination, CAddressBookData>& entry : pwallet->mapAddressBook) {
        if (purpose.empty() || entry.second.purpose == purpose) {
            label_set.insert(entry.second.name);
        }
    }

    UniValue ret(UniValue::VARR);
    for (const std::string& name : label_set) {
        ret.push_back(name);
    }

    return ret;
}

UniValue sethdseed(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "sethdseed ( \"newkeypool\" \"seed\" )\n"
            "\nSet or generate a new HD wallet seed. Non-HD wallets will not be upgraded to being a HD wallet. Wallets that are already\n"
            "HD will have a new HD seed set so that new keys added to the keypool will be derived from this new seed.\n"
            "\nNote that you will need to MAKE A NEW BACKUP of your wallet after setting the HD wallet seed.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"newkeypool\"         (boolean, optional, default=true) Whether to flush old unused addresses, including change addresses, from the keypool and regenerate it.\n"
            "                             If true, the next address from getnewaddress and change address from getrawchangeaddress will be from this new seed.\n"
            "                             If false, addresses (including change addresses if the wallet already had HD Chain Split enabled) from the existing\n"
            "                             keypool will be used until it has been depleted.\n"
            "2. \"seed\"               (string, optional) The WIF private key to use as the new HD seed; if not provided a random seed will be used.\n"
            "                             The seed value can be retrieved using the dumpwallet command. It is the private key marked hdseed=1\n"
            "\nExamples:\n"
            + HelpExampleCli("sethdseed", "")
            + HelpExampleCli("sethdseed", "false")
            + HelpExampleCli("sethdseed", "true \"wifkey\"")
            + HelpExampleRpc("sethdseed", "true, \"wifkey\"")
            );
    }

    if (IsInitialBlockDownload()) {
        throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Cannot set a new HD seed while still in Initial Block Download");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    // Do not do anything to non-HD wallets
    if (!pwallet->IsHDEnabled()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Cannot set a HD seed on a non-HD wallet. Start with -upgradewallet in order to upgrade a non-HD wallet to HD");
    }

    EnsureWalletIsUnlocked(pwallet);

    bool flush_key_pool = true;
    if (!request.params[0].isNull()) {
        flush_key_pool = request.params[0].get_bool();
    }

    CPubKey master_pub_key;
    if (request.params[1].isNull()) {
        master_pub_key = pwallet->GenerateNewSeed();
    } else {
        CKey key = DecodeSecret(request.params[1].get_str());
        if (!key.IsValid()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
        }

        if (HaveKey(*pwallet, key)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Already have this key (either as an HD seed or as a loose private key)");
        }

        master_pub_key = pwallet->DeriveNewSeed(key);
    }

    pwallet->SetHDSeed(master_pub_key);
    if (flush_key_pool) pwallet->NewKeyPool();

    return NullUniValue;
}

bool ParseHDKeypath(std::string keypath_str, std::vector<uint32_t>& keypath)
{
    std::stringstream ss(keypath_str);
    std::string item;
    bool first = true;
    while (std::getline(ss, item, '/')) {
        if (item.compare("m") == 0) {
            if (first) {
                first = false;
                continue;
            }
            return false;
        }
        // Finds whether it is hardened
        uint32_t path = 0;
        size_t pos = item.find("'");
        if (pos != std::string::npos) {
            // The hardened tick can only be in the last index of the string
            if (pos != item.size() - 1) {
                return false;
            }
            path |= 0x80000000;
            item = item.substr(0, item.size() - 1); // Drop the last character which is the hardened tick
        }

        // Ensure this is only numbers
        if (item.find_first_not_of( "0123456789" ) != std::string::npos) {
            return false;
        }
        uint32_t number;
        if (!ParseUInt32(item, &number)) {
            return false;
        }
        path |= number;

        keypath.push_back(path);
        first = false;
    }
    return true;
}

void AddKeypathToMap(const CWallet* pwallet, const CKeyID& keyID, std::map<CPubKey, std::vector<uint32_t>>& hd_keypaths)
{
    CPubKey vchPubKey;
    if (!pwallet->GetPubKey(keyID, vchPubKey)) {
        return;
    }
    CKeyMetadata meta;
    auto it = pwallet->mapKeyMetadata.find(keyID);
    if (it != pwallet->mapKeyMetadata.end()) {
        meta = it->second;
    }
    std::vector<uint32_t> keypath;
    if (!meta.hdKeypath.empty()) {
        if (!ParseHDKeypath(meta.hdKeypath, keypath)) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Internal keypath is broken");
        }
        // Get the proper master key id
        CKey key;
        pwallet->GetKey(meta.hd_seed_id, key);
        CExtKey masterKey;
        masterKey.SetSeed(key.begin(), key.size());
        // Add to map
        keypath.insert(keypath.begin(), ReadLE32(masterKey.key.GetPubKey().GetID().begin()));
    } else { // Single pubkeys get the master fingerprint of themselves
        keypath.insert(keypath.begin(), ReadLE32(vchPubKey.GetID().begin()));
    }
    hd_keypaths.emplace(vchPubKey, keypath);
}

bool FillPSBT(const CWallet* pwallet, PartiallySignedTransaction& psbtx, const CTransaction* txConst, int sighash_type, bool sign, bool bip32derivs)
{
    LOCK(pwallet->cs_wallet);
    // Get all of the previous transactions
    bool complete = true;
    for (unsigned int i = 0; i < txConst->vin.size(); ++i) {
        const CTxIn& txin = txConst->vin[i];
        PSBTInput& input = psbtx.inputs.at(i);

        // If we don't know about this input, skip it and let someone else deal with it
        const uint256& txhash = txin.prevout.hashMalFix;
        const auto it = pwallet->mapWallet.find(txhash);
        if (it != pwallet->mapWallet.end()) {
            const CWalletTx& wtx = it->second;
            CTxOut utxo = wtx.tx->vout[txin.prevout.n];
            // Update both UTXOs from the wallet.
            input.non_witness_utxo = wtx.tx;
        }

        // Get the Sighash type
        if (sign && input.sighash_type > 0 && input.sighash_type != sighash_type) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Specified Sighash and sighash in PSBT do not match.");
        }

        SignatureData sigdata;
        if (sign) {
            complete &= SignPSBTInput(*pwallet, *psbtx.tx, input, sigdata, i, sighash_type);
        } else {
            complete &= SignPSBTInput(PublicOnlySigningProvider(pwallet), *psbtx.tx, input, sigdata, i, sighash_type);
        }


        // Get public key paths
        if (bip32derivs) {
            for (const auto& pubkey_it : sigdata.misc_pubkeys) {
                AddKeypathToMap(pwallet, pubkey_it.first, input.hd_keypaths);
            }
        }
    }

    // Fill in the bip32 keypaths and redeemscripts for the outputs so that hardware wallets can identify change
    for (unsigned int i = 0; i < txConst->vout.size(); ++i) {
        const CTxOut& out = txConst->vout.at(i);
        PSBTOutput& psbt_out = psbtx.outputs.at(i);

        // Dummy tx so we can use ProduceSignature to get stuff out
        /*CMutableTransaction dummy_tx;
        dummy_tx.vin.push_back(CTxIn());
        dummy_tx.vout.push_back(CTxOut());*/

        // Fill a SignatureData with output info
        SignatureData sigdata;
        psbt_out.FillSignatureData(sigdata);

        MutableTransactionSignatureCreator creator(psbtx.tx.get_ptr(), 0, out.nValue, 1);
        ProduceSignature(*pwallet, creator, out.scriptPubKey, sigdata);
        psbt_out.FromSignatureData(sigdata);

        // Get public key paths
        if (bip32derivs) {
            for (const auto& pubkey_it : sigdata.misc_pubkeys) {
                AddKeypathToMap(pwallet, pubkey_it.first, psbt_out.hd_keypaths);
            }
        }
    }
    return complete;
}

UniValue walletprocesspsbt(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 4)
        throw std::runtime_error(
            "walletprocesspsbt \"psbt\" ( sign \"sighashtype\" bip32derivs )\n"
            "\nUpdate a PSBT with input information from our wallet and then sign inputs\n"
            "that we can sign for.\n"
            + HelpRequiringPassphrase(pwallet) + "\n"

            "\nArguments:\n"
            "1. \"psbt\"                      (string, required) The transaction base64 string\n"
            "2. sign                          (boolean, optional, default=true) Also sign the transaction when updating\n"
            "3. \"sighashtype\"            (string, optional, default=ALL) The signature hash type to sign with if not specified by the PSBT. Must be one of\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"
            "4. bip32derivs                    (boolean, optional, default=false) If true, includes the BIP 32 derivation paths for public keys if we know them\n"

            "\nResult:\n"
            "{\n"
            "  \"psbt\" : \"value\",          (string) The base64-encoded partially signed transaction\n"
            "  \"complete\" : true|false,   (boolean) If the transaction has a complete set of signatures\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("walletprocesspsbt", "\"psbt\"")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VBOOL, UniValue::VSTR});

    // Unserialize the transaction
    PartiallySignedTransaction psbtx;
    std::string error;
    if (!DecodePSBT(psbtx, request.params[0].get_str(), error)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, strprintf("TX decode failed %s", error));
    }

    // Get the sighash type
    int nHashType = ParseSighashString(request.params[2]);

    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(*psbtx.tx);

    // Fill transaction with our data and also sign
    bool sign = request.params[1].isNull() ? true : request.params[1].get_bool();
    bool bip32derivs = request.params[3].isNull() ? false : request.params[3].get_bool();
    bool complete = FillPSBT(pwallet, psbtx, &txConst, nHashType, sign, bip32derivs);

    UniValue result(UniValue::VOBJ);
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;
    result.pushKV("psbt", EncodeBase64((unsigned char*)ssTx.data(), ssTx.size()));
    result.pushKV("complete", complete);

    return result;
}

UniValue walletcreatefundedpsbt(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 5)
        throw std::runtime_error(
                            "walletcreatefundedpsbt [{\"txid\":\"id\",\"vout\":n},...] [{\"address\":amount},{\"data\":\"hex\"},...] ( locktime ) ( replaceable ) ( options bip32derivs )\n"
                            "\nCreates and funds a transaction in the Partially Signed Transaction format. Inputs will be added if supplied inputs are not enough\n"
                            "Implements the Creator and Updater roles.\n"
                            "\nArguments:\n"
                            "1. \"inputs\"                (array, required) A json array of json objects\n"
                            "     [\n"
                            "       {\n"
                            "         \"txid\":\"id\",      (string, required) The transaction id\n"
                            "         \"vout\":n,         (numeric, required) The output number\n"
                            "         \"sequence\":n      (numeric, optional) The sequence number\n"
                            "       } \n"
                            "       ,...\n"
                            "     ]\n"
                            "2. \"outputs\"               (array, required) a json array with outputs (key-value pairs)\n"
                            "   [\n"
                            "    {\n"
                            "      \"address\": x.xxx,    (obj, optional) A key-value pair. The key (string) is the tapyrus address, the value (float or string) is the amount in " + CURRENCY_UNIT + "\n"
                            "    },\n"
                            "    {\n"
                            "      \"data\": \"hex\"        (obj, optional) A key-value pair. The key must be \"data\", the value is hex encoded data\n"
                            "    }\n"
                            "    ,...                     More key-value pairs of the above form. For compatibility reasons, a dictionary, which holds the key-value pairs directly, is also\n"
                            "                             accepted as second parameter.\n"
                            "   ]\n"
                            "3. locktime                  (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs\n"
                            "                             Allows this transaction to be replaced by a transaction with higher fees. If provided, it is an error if explicit sequence numbers are incompatible.\n"
                            "4. options                 (object, optional)\n"
                            "   {\n"
                            "     \"changeAddress\"          (string, optional, default pool address) The tapyrus address to receive the change\n"
                            "     \"changePosition\"         (numeric, optional, default random) The index of the " + CURRENCY_UNIT + " change output\n"
                            "     \"includeWatching\"        (boolean, optional, default false) Also select inputs which are watch only\n"
                            "     \"lockUnspents\"           (boolean, optional, default false) Lock selected unspent outputs\n"
                            "     \"feeRate\"                (numeric, optional, default not set: makes wallet determine the fee) Set a specific fee rate in " + CURRENCY_UNIT + "/kB\n"
                            "     \"subtractFeeFromOutputs\" (array, optional) A json array of integers.\n"
                            "                              The fee will be equally deducted from the amount of each specified output.\n"
                            "                              The outputs are specified by their zero-based index, before any change output is added.\n"
                            "                              Those recipients will receive less TPC than you enter in their corresponding amount field.\n"
                            "                              If no outputs are specified here, the sender pays the fee.\n"
                            "                                  [vout_index,...]\n"
                            "     \"replaceable\"            (boolean, optional) Marks this transaction as BIP125 replaceable.\n"
                            "                              Allows this transaction to be replaced by a transaction with higher fees\n"
                            "     \"conf_target\"            (numeric, optional) Confirmation target (in blocks)\n"
                            "     \"estimate_mode\"          (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
                            "         \"UNSET\"\n"
                            "         \"ECONOMICAL\"\n"
                            "         \"CONSERVATIVE\"\n"
                            "   }\n"
                            "5. bip32derivs                    (boolean, optional, default=false) If true, includes the BIP 32 derivation paths for public keys if we know them\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"psbt\": \"value\",        (string)  The resulting raw transaction (base64-encoded string)\n"
                            "  \"fee\":       n,         (numeric) Fee in " + CURRENCY_UNIT + " the resulting transaction pays\n"
                            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
                            "}\n"
                            "\nExamples:\n"
                            "\nCreate a transaction with no inputs\n"
                            + HelpExampleCli("walletcreatefundedpsbt", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"[{\\\"data\\\":\\\"00010203\\\"}]\"")
                            );

    RPCTypeCheck(request.params, {
        UniValue::VARR,
        UniValueType(), // ARR or OBJ, checked later
        UniValue::VNUM,
        UniValue::VOBJ,
        UniValue::VBOOL
        }, true
    );

    CAmount fee;
    CWallet::ChangePosInOut change_position;
    CMutableTransaction rawTx = ConstructTransaction(request.params[0], request.params[1], request.params[2], request.params[3]["replaceable"]);
    FundTransaction(pwallet, rawTx, fee, change_position, request.params[3]);

    // Make a blank psbt
    PartiallySignedTransaction psbtx;
    psbtx.tx = rawTx;
    for (unsigned int i = 0; i < rawTx.vin.size(); ++i) {
        psbtx.inputs.push_back(PSBTInput());
    }
    for (unsigned int i = 0; i < rawTx.vout.size(); ++i) {
        psbtx.outputs.push_back(PSBTOutput());
    }

    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(*psbtx.tx);

    // Fill transaction with out data but don't sign
    bool bip32derivs = request.params[4].isNull() ? false : request.params[4].get_bool();
    FillPSBT(pwallet, psbtx, &txConst, 1, false, bip32derivs);

    // Serialize the PSBT
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << psbtx;

    UniValue result(UniValue::VOBJ);
    result.pushKV("psbt", EncodeBase64((unsigned char*)ssTx.data(), ssTx.size()));
    result.pushKV("fee", ValueFromAmount(fee));
    result.pushKV("changepos", change_position[ColorIdentifier()]);
    return result;
}

static ColorIdentifier getColorIdFromRequest(const JSONRPCRequest& request, bool tokenValueIsPresent = true) 
{
    TokenTypes tokentype;
    switch(request.params[0].get_int())
    {
        case 1: tokentype = TokenTypes::REISSUABLE; break;
        case 2: tokentype = TokenTypes::NON_REISSUABLE; break;
        case 3: tokentype = TokenTypes::NFT; break;
        default: tokentype = TokenTypes::NONE;
    }

    if (tokentype == TokenTypes::NONE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown token type given.");
    }

    int indexOfTxid = tokenValueIsPresent ? 2 : 1;

    if (tokentype == TokenTypes::REISSUABLE && !request.params[indexOfTxid + 1].isNull()){
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Extra parameter for Reissuable token.");
    }

    if ((tokentype == TokenTypes::NON_REISSUABLE || tokentype == TokenTypes::NFT )
      && request.params[indexOfTxid + 1].isNull()){
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Parameter missing for Non-Reissuable or NFT token.");
    }

    const std::string scriptOrTxid(request.params[indexOfTxid].get_str());
    ColorIdentifier colorId;
    if(tokentype == TokenTypes::REISSUABLE)
    {
        std::vector<unsigned char> vscript = ParseHex(scriptOrTxid);
        CScript script(vscript.begin(), vscript.end());
        if(script.IsColoredScript())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Script input for tokens cannot be another token script.");
        colorId = ColorIdentifier(script);
    }
    else
    {
        COutPoint out(std::move(uint256S(scriptOrTxid)), request.params[indexOfTxid + 1].get_int());
        colorId = ColorIdentifier(out, tokentype);
    }

    return colorId;
}


// Construct two transctions:
// first transaction to create a utxo with the given scriptpubkey.
// second transaction to issue token using tx1's output
static UniValue IssueReissuableToken(CWallet* const pwallet, const std::string& script, CAmount tokenValue, CCoinControl& coin_control)
{
    LOCK2(cs_main, pwallet->cs_wallet);

    CTransactionRef tx1;
    //creating tx1
    {
        std::vector<unsigned char> vscript = ParseHex(script);
        CScript scriptPubKey(vscript.begin(), vscript.end());

        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (!ExtractDestinations(scriptPubKey, type, vDest, nRequired)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid Tapyrus script: ") + script);
        }
        for (const CTxDestination &dest : vDest)
            if(!IsValidDestination(dest))
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid Tapyrus script:") + script);
        pwallet->SetAddressBook(vDest[0], "", "receive");

        // Create and send the transaction
        CReserveKey reservekey(pwallet);
        CAmount nFeeRequired;
        std::string strError;
        std::vector<CRecipient> vecSend;
        CWallet::ChangePosInOut mapChangePosRet;
        mapChangePosRet[ColorIdentifier()] = -1;
        CRecipient recipient = {scriptPubKey, DEFAULT_FALLBACK_FEE, false};
        vecSend.push_back(recipient);

        if (!pwallet->CreateTransaction(vecSend, tx1, reservekey, nFeeRequired, mapChangePosRet, strError, coin_control))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
        CValidationState state;
        mapValue_t mapValue;
        if (!pwallet->CommitTransaction(tx1, std::move(mapValue), {} /* orderForm */, reservekey, g_connman.get(), state)) {
            strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
    }

    CTransactionRef tx2;
    //creating tx2
    {
        if (!pwallet->IsLocked()) {
            pwallet->TopUpKeyPool();
        }

        // Generate a new key that is added to wallet
        CPubKey newKey;
        if (!pwallet->GetKeyFromPool(newKey)) {
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
        }

        CKeyID key_id = newKey.GetID();
        CScript redeemScript = GetScriptForDestination(key_id);
        CColorScriptID colorscriptid(CScriptID(redeemScript), coin_control.m_colorId);
        CTxDestination colorDest = CColorScriptID(colorscriptid, coin_control.m_colorId);

        //setting the lable as colorid
        pwallet->SetAddressBook(colorDest, "", "send");

        CScript scriptpubkey = GetScriptForDestination(colorDest);

        // Create and send the transaction
        CReserveKey reservekey(pwallet);
        CAmount nFeeRequired;
        std::string strError;
        std::vector<CRecipient> vecSend;
        CWallet::ChangePosInOut mapChangePosRet;
        mapChangePosRet[ColorIdentifier()] = -1;
        CRecipient recipient = {scriptpubkey, tokenValue, false};
        vecSend.push_back(recipient);
        COutPoint out(tx1->GetHashMalFix(), 0);
        coin_control.m_colorTxType = ColoredTxType::ISSUE;
        coin_control.Select(out);

        if (!pwallet->CreateTransaction(vecSend, tx2, reservekey, nFeeRequired, mapChangePosRet, strError, coin_control))
        {
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
        CValidationState state;
        mapValue_t mapValue;
        if (!pwallet->CommitTransaction(tx2, std::move(mapValue), {} /* orderForm */, reservekey, g_connman.get(), state)) {
            strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
        }
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("color", coin_control.m_colorId.toHexString());
    UniValue txidlist(UniValue::VARR);
    txidlist.push_back(tx1->GetHashMalFix().GetHex());
    txidlist.push_back(tx2->GetHashMalFix().GetHex());
    result.pushKV("txids", txidlist);
    return result;
}

static UniValue IssueToken(CWallet* const pwallet, CAmount tokenValue, CCoinControl& coin_control)
{
    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwallet->GetKeyFromPool(newKey)) {
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    CKeyID key_id = newKey.GetID();
    CScript redeemScript = GetScriptForDestination(key_id);
    CColorScriptID colorscriptid(CScriptID(redeemScript), coin_control.m_colorId);
    CTxDestination colorDest = CColorScriptID(colorscriptid, coin_control.m_colorId);

    CScript scriptpubkey = GetScriptForDestination(colorDest);

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    CWallet::ChangePosInOut mapChangePosRet;
    mapChangePosRet[ColorIdentifier()] = -1;
    CRecipient recipient = {scriptpubkey, tokenValue, false};
    vecSend.push_back(recipient);
    CTransactionRef tx;

    if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, mapChangePosRet, strError, coin_control)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    CValidationState state;
    mapValue_t mapValue;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("color", coin_control.m_colorId.toHexString());
    result.pushKV("txid", tx->GetHashMalFix().GetHex());
    return result;
}

UniValue issuetoken(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw std::runtime_error(
            "issuetoken \"token_type\" \"token_value\" \"txid/scriptpubkey\" \"index\" \n"
            "\nIssue new colored coins or tokens and store them in the wallet.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"token_type\"       (numberic, required) Value can be 1 or 2 or 3.\n"
            " 1. REISSUABLE\n"
            " 2. NON-REISSUABLE\n"
            " 3. NFT\n"
            "2. \"token_value\"      (numberic, required) Number of tokens to be issued \n"
            "3. \"scriptpubkey\"     (string, optional) Script pubkey that is used to issue REISSUABLE token\n"
            "3. \"txid\"             (string, optional) Transaction id from which the NON-REISSUABLE or NFT tokens are issued\n"
            "4. \"index\"            (numeric, optional) Index in the above transaction id used for issuing token\n"
            "\nResult:\n"
            "{\n"
            "  \"color\"               (string) The color or token.\n"
            "  \"txid\":               (string) The transaction id in case of NON-REISSUABLE and NFC tokens.\n"
            "   or\n"
            "  \"txids:\"              (json array of string) The transaction ids of the two transactions created in case of REISSUABLE token\n"
            "    [\n"
            "      \"txid1\"           (string) transaction to create spendable UTXO\n"
            "      \"txid2\"           (string) transaction to issue token spending the above UTXO\n"
            "    ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("issuetoken", "\"1\" \"100\" 76a9141234567890abcdefa1a2a3a4a5a6a7a8a9a0aaab88ac")
            + HelpExampleCli("issuetoken", "\"2\" \"1000\" 7258ed58b571f8bbff23c779f6d1e7f9c285700c4bcf0aa3f33292ff9983b5d7 0")
            + HelpExampleCli("issuetoken", "\"3\" \"1\" 7258ed58b571f8bbff23c779f6d1e7f9c285700c4bcf0aa3f33292ff9983b5d7 0")
        );
    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VNUM, UniValue::VSTR});

    const ColorIdentifier colorId = getColorIdFromRequest(request);

    // token value
    CAmount tokenValue = TokenAmountFromValue(request.params[1]);
    if (tokenValue <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");

    if(colorId.type == TokenTypes::NFT && tokenValue != 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid token amount for NFT. It must be 1");
    }

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Private keys are disabled for this wallet");
    }
    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    CCoinControl coin_control;
    coin_control.m_colorId = colorId;

    // For reissuable tokens try to create a UTXO with the given scriptpubkey first and then issuing transaction using that utxo 
    if(coin_control.m_colorId.type == TokenTypes::REISSUABLE)
        return IssueReissuableToken(pwallet, request.params[2].getValStr(), tokenValue, coin_control);
    else
    {
        uint256 txid = uint256S(request.params[2].get_str());
        uint8_t vout = request.params[3].get_int();
        COutPoint out(txid, vout);

        //Check if the UTXO belongs to us and is spendable
        {
            LOCK(pwallet->cs_wallet);

            //1. check transaction id
            auto it = pwallet->mapWallet.find(txid);
            if (it == pwallet->mapWallet.end()) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid or non-wallet transaction id :") + request.params[2].get_str());
            }

            //2. vout and its type
            if(vout >= it->second.tx->vout.size() || GetColorIdFromScript(it->second.tx->vout[vout].scriptPubKey).type != TokenTypes::NONE) {
                std::string strError = strprintf("Invalid vout %d in tx: %s", request.params[3].get_int(), request.params[2].get_str());
                throw JSONRPCError(RPC_INVALID_PARAMETER, strError);
            }

            //3. spendable
            CTxOut txout(it->second.tx->vout[vout].nValue, it->second.tx->vout[vout].scriptPubKey);
            if(pwallet->IsMine(txout)  != ISMINE_SPENDABLE)
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Transaction is not spendable :") + request.params[2].get_str() + " : " + request.params[3].get_str());
        }
        //This output is ours. Add it to coin control so that it will be used as an input in transaction creation
        coin_control.Select(out);
        coin_control.m_colorTxType = ColoredTxType::ISSUE;
        return IssueToken(pwallet, tokenValue, coin_control);
    }
}

UniValue reissuetoken(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "reissuetoken \"color\" \"value\" \n"
            "\nReissue colored coins or tokens with the given color. The token must be of type REISSUABLE for this RPC to issue the token.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"color\"              (string, required) The tapyrus color / token to be reissued.\n"
            "2. \"value\"              (numeric, required) The number of tokens to issue. eg 10\n"
            "\nResult:\n"
            "{\n"
            "  \"color\"               (string) The color or token.\n"
            "  \"txids:\"              (json array of string) The transaction ids of the two transactions created to issue the token\n"
            "    [\n"
            "      \"txid1\"           (string) transaction to create spendable UTXO\n"
            "      \"txid2\"           (string) transaction to issue token spending the above UTXO\n"
            "    ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("reissuetoken", "\"c18282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\" 10")
        );

    ColorIdentifier colorId;
    if (!request.params[0].isNull() && !checkColorIdParam(request.params[0], colorId))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color parameter.");

    if(colorId.type != TokenTypes::REISSUABLE)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Token type not supported");

    // token value
    CAmount tokenValue = TokenAmountFromValue(request.params[1]);
    if (tokenValue <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid token amount");

    LOCK2(cs_main, pwallet->cs_wallet);

    CScript script;
    if(!pwallet->GetCScriptForColor(colorId, script, true))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Script corresponding to color " + colorId.toHexString() + " could not be found in the wallet");

    CCoinControl coin_control;
    coin_control.m_colorId = colorId;
    return IssueReissuableToken(pwallet, HexStr(script.begin(), script.end()), tokenValue, coin_control);
}

UniValue transfertoken(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "transfertoken \"address\" \"value\" \n"
            "\nSend colored coins or tokens to a given address.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"address\"            (string, required) The colored tapyrus address to send to.\n"
            "2. \"value\"              (numeric, required) The number of tokens to send. eg 10\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("transfertoken", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 10")
        );

    return sendtoaddress(request);
}

static CTransactionRef BurnToken(CWallet * const pwallet, const ColorIdentifier& colorId, CAmount nValue)
{
    //this is to make create transaction recognize this as colored transaction
    CScript scriptPubKey = CScript() << colorId.toVector() << OP_COLOR << OP_TRUE;
    mapValue_t mapValue;
    mapValue["comment"] = colorId.toHexString();

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    CWallet::ChangePosInOut mapChangePosRet;
    mapChangePosRet[ColorIdentifier()] = -1;
    CRecipient recipient = {scriptPubKey, nValue, false};
    vecSend.push_back(recipient);
    CTransactionRef tx;
    CCoinControl coin_control;
    coin_control.m_colorTxType = ColoredTxType::BURN;
    coin_control.m_colorId = colorId;

    if (!pwallet->CreateTransaction(vecSend, tx, reservekey, nFeeRequired, mapChangePosRet, strError, coin_control)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    CValidationState state;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {} /* orderForm */, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", FormatStateMessage(state));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    return tx;
}

UniValue burntoken(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "burntoken \"color\" \"value\" \n"
            "\nBurn colored coins or tokens in the wallet.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"color\"              (string, required) The tapyrus color / token to burn.\n"
            "2. \"value\"              (numeric, required) The amount to burn. eg 10\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("burntoken", "\"c38282263212c609d9ea2a6e3e172de238d8c39cabd5ac1ca10646e23f\" 10")
        );

    // Make sure the results are valid at least up to the most recent block
    // the user could have gotten from another RPC command prior to now
    pwallet->BlockUntilSyncedToCurrentChain();

    LOCK2(cs_main, pwallet->cs_wallet);
    ColorIdentifier colorId;
    if (!request.params[0].isNull() && !checkColorIdParam(request.params[0], colorId))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color parameter.");

    CAmount nAmount = TokenAmountFromValue(request.params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for burn");

    if (colorId.type == TokenTypes::NONE)
         throw JSONRPCError(RPC_INVALID_PARAMETER, "TPC cannot be burnt using burntoken");

    CAmount curBalance = pwallet->GetBalance()[colorId];
    if (colorId.type != TokenTypes::NONE && (curBalance == 0 || curBalance < nAmount))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Insufficient token balance in wallet");

    CTransactionRef tx = BurnToken(pwallet, colorId, nAmount);

    return tx->GetHashMalFix().GetHex();
}

extern UniValue abortrescan(const JSONRPCRequest& request); // in rpcdump.cpp
extern UniValue dumpprivkey(const JSONRPCRequest& request); // in rpcdump.cpp
extern UniValue importprivkey(const JSONRPCRequest& request);
extern UniValue importaddress(const JSONRPCRequest& request);
extern UniValue importpubkey(const JSONRPCRequest& request);
extern UniValue dumpwallet(const JSONRPCRequest& request);
extern UniValue importwallet(const JSONRPCRequest& request);
extern UniValue importprunedfunds(const JSONRPCRequest& request);
extern UniValue removeprunedfunds(const JSONRPCRequest& request);
extern UniValue importmulti(const JSONRPCRequest& request);

static const CRPCCommand commands[] =
{ //  category              name                                actor (function)                argNames
    //  --------------------- ------------------------          -----------------------         ----------
    { "rawtransactions",    "fundrawtransaction",               &fundrawtransaction,            {"hexstring","options"} },
    { "wallet",             "walletprocesspsbt",                &walletprocesspsbt,             {"psbt","sign","sighashtype","bip32derivs"} },
    { "wallet",             "walletcreatefundedpsbt",           &walletcreatefundedpsbt,        {"inputs","outputs","locktime","options","bip32derivs"} },
    { "hidden",             "resendwallettransactions",         &resendwallettransactions,      {} },
    { "wallet",             "abandontransaction",               &abandontransaction,            {"txid"} },
    { "wallet",             "abortrescan",                      &abortrescan,                   {} },
    { "wallet",             "addmultisigaddress",               &addmultisigaddress,            {"nrequired","keys","label"} },
    { "wallet",             "backupwallet",                     &backupwallet,                  {"destination"} },
    { "wallet",             "bumpfee",                          &bumpfee,                       {"txid", "options"} },
    { "wallet",             "createwallet",                     &createwallet,                  {"wallet_name", "disable_private_keys"} },
    { "wallet",             "dumpprivkey",                      &dumpprivkey,                   {"address"}  },
    { "wallet",             "dumpwallet",                       &dumpwallet,                    {"filename"} },
    { "wallet",             "encryptwallet",                    &encryptwallet,                 {"passphrase"} },
    { "wallet",             "getaddressinfo",                   &getaddressinfo,                {"address"} },
    { "wallet",             "getbalance",                       &getbalance,                    {"include_watchonly", "color"} },
    { "wallet",             "getnewaddress",                    &getnewaddress,                 {"label","color"} },
    { "wallet",             "getrawchangeaddress",              &getrawchangeaddress,           {"color"} },
    { "wallet",             "getreceivedbyaddress",             &getreceivedbyaddress,          {"address"} },
    { "wallet",             "gettransaction",                   &gettransaction,                {"txid","include_watchonly"} },
    { "wallet",             "getunconfirmedbalance",            &getunconfirmedbalance,         {} },
    { "wallet",             "getwalletinfo",                    &getwalletinfo,                 {} },
    { "wallet",             "importmulti",                      &importmulti,                   {"requests","options"} },
    { "wallet",             "importprivkey",                    &importprivkey,                 {"privkey","label","rescan"} },
    { "wallet",             "importwallet",                     &importwallet,                  {"filename"} },
    { "wallet",             "importaddress",                    &importaddress,                 {"address","label","rescan","p2sh"} },
    { "wallet",             "importprunedfunds",                &importprunedfunds,             {"rawtransaction","txoutproof"} },
    { "wallet",             "importpubkey",                     &importpubkey,                  {"pubkey","label","rescan"} },
    { "wallet",             "keypoolrefill",                    &keypoolrefill,                 {"newsize"} },
    { "wallet",             "listaddressgroupings",             &listaddressgroupings,          {} },
    { "wallet",             "listlockunspent",                  &listlockunspent,               {} },
    { "wallet",             "listreceivedbyaddress",            &listreceivedbyaddress,         {"include_empty","include_watchonly","address_filter"} },
    { "wallet",             "listsinceblock",                   &listsinceblock,                {"blockhash","target_confirmations","include_watchonly","include_removed"} },
    { "wallet",             "listtransactions",                 &listtransactions,              {"count","skip","include_watchonly"} },
    { "wallet",             "listunspent",                      &listunspent,                   {"minconf","maxconf","addresses","include_unsafe","query_options"} },
    { "wallet",             "listwallets",                      &listwallets,                   {} },
    { "wallet",             "loadwallet",                       &loadwallet,                    {"filename"} },
    { "wallet",             "lockunspent",                      &lockunspent,                   {"unlock","transactions"} },
    { "wallet",             "sendmany",                         &sendmany,                      {"amounts","comment","subtractfeefrom","replaceable","conf_target","estimate_mode"} },
    { "wallet",             "sendtoaddress",                    &sendtoaddress,                 {"address","amount","comment","comment_to","subtractfeefromamount","replaceable","conf_target","estimate_mode"} },
    { "wallet",             "settxfee",                         &settxfee,                      {"amount"} },
    { "wallet",             "signmessage",                      &signmessage,                   {"address","message"} },
    { "wallet",             "signrawtransactionwithwallet",     &signrawtransactionwithwallet,  {"hexstring","prevtxs","sighashtype"} },
    { "wallet",             "unloadwallet",                     &unloadwallet,                  {"wallet_name"} },
    { "wallet",             "walletlock",                       &walletlock,                    {} },
    { "wallet",             "walletpassphrasechange",           &walletpassphrasechange,        {"oldpassphrase","newpassphrase"} },
    { "wallet",             "walletpassphrase",                 &walletpassphrase,              {"passphrase","timeout"} },
    { "wallet",             "removeprunedfunds",                &removeprunedfunds,             {"txid"} },
    { "wallet",             "rescanblockchain",                 &rescanblockchain,              {"start_height", "stop_height"} },
    { "wallet",             "sethdseed",                        &sethdseed,                     {"newkeypool","seed"} },

    /** Label functions */
    { "wallet",             "getaddressesbylabel",              &getaddressesbylabel,           {"label"} },
    { "wallet",             "getreceivedbylabel",               &getreceivedbylabel,            {"label"} },
    { "wallet",             "listlabels",                       &listlabels,                    {"purpose"} },
    { "wallet",             "listreceivedbylabel",              &listreceivedbylabel,           {"include_empty","include_watchonly"} },
    { "wallet",             "setlabel",                         &setlabel,                      {"address","label"} },

    /** colored coin RPCs */
    { "wallet",                    "issuetoken",             &issuetoken,                      {"type","value","txid","index"} },
    { "wallet",                    "reissuetoken",           &reissuetoken,                      {"color","value"} },
    { "wallet",                    "transfertoken",          &transfertoken,                      {"address","value"} },
    { "wallet",                    "burntoken",              &burntoken,                      {"color","value"} },

    { "generating",         "generate",                         &generate,                      {"nblocks","privkeys"} },
};

void RegisterWalletRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
