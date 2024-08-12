// Copyright (c) 2024 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mempooloptions.h>
#include <policy/packages.h>
#include <validation.h>
#include <coins.h>
#include <txmempool.h>

CTxMempoolAcceptanceOptions::CTxMempoolAcceptanceOptions(ValidationContext& _context):
                                            context(_context),
                                            flags(MempoolAcceptanceFlags::NONE),
                                            nAbsurdFee(0),
                                            nAcceptTime(0) {

    if(IsPackage(_context)){
        const CCachedPackage* cachedPackage = std::get_if<const CCachedPackage>(&_context);
        if (cachedPackage) {
            viewCoins = new CCoinsViewPackage(pcoinsTip.get(), mempool, *cachedPackage);
        } 
    }
    else if(IsTransaction(_context))
        viewCoins = new CCoinsViewMemPool(pcoinsTip.get(), mempool);
    else
        viewCoins = nullptr;
}

CTxMempoolAcceptanceOptions::CTxMempoolAcceptanceOptions(const CCachedPackage& pkg):
                                            context(ValidationContext(pkg)),
                                            flags(MempoolAcceptanceFlags::NONE),
                                            nAbsurdFee(0),
                                            nAcceptTime(0) {

    viewCoins = new CCoinsViewPackage(pcoinsTip.get(), mempool, pkg);

}

CTxMempoolAcceptanceOptions::CTxMempoolAcceptanceOptions(CTransactionRef tx):
                                            context(ValidationContext(tx)),
                                            flags(MempoolAcceptanceFlags::NONE),
                                            nAbsurdFee(0),
                                            nAcceptTime(0) {

    viewCoins = new CCoinsViewMemPool(pcoinsTip.get(), mempool);
}