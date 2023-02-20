// Copyright (c) 2019-2022 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/xfield.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <federationparams.h>

inline std::string XFieldAggPubKey::ToString() const {
    return HexStr(data);
}

inline std::string XFieldMaxBlockSize::ToString() const {
    return std::to_string(data);
}

template <typename T>
bool GetXFieldValueFrom(XFieldData& xfieldValue, T& value) {
    value = boost::get<T>(xfieldValue);
    return std::atoi(value.BLOCKTREE_DB_KEY) == GetXFieldTypeFrom(xfieldValue);
}

std::string CXField::ToString() const {
    std::stringstream s;
    s << strprintf("CXField(xfieldType=%d, xfieldValue={%s})\n",
        (uint8_t)xfieldType, XFieldDataToString(xfieldValue).c_str());
    return s.str();
}

bool CXField::IsValid() const {
    return ::IsValid(this->xfieldType)
        && boost::apply_visitor(XFieldValidityVisitor(), this->xfieldValue)
        && GetXFieldTypeFrom(this->xfieldValue) == this->xfieldType;
}

std::string XFieldDataToString(const XFieldData &xfieldValue) {
    switch(GetXFieldTypeFrom(xfieldValue))
    {
        case TAPYRUS_XFIELDTYPES::AGGPUBKEY:
            return boost::get<XFieldAggPubKey>(xfieldValue).ToString();
        case TAPYRUS_XFIELDTYPES::MAXBLOCKSIZE:
            return boost::get<XFieldMaxBlockSize>(xfieldValue).ToString();
        case TAPYRUS_XFIELDTYPES::NONE:
        default:
            return "";
    }
    return "";
}

std::string GetXFieldNameForRpc(TAPYRUS_XFIELDTYPES x){
    switch(x){
        case TAPYRUS_XFIELDTYPES::AGGPUBKEY:
            return "aggregatePubkeys";
        case TAPYRUS_XFIELDTYPES::MAXBLOCKSIZE:
            return "maxBlockSizes";
        case TAPYRUS_XFIELDTYPES::NONE:
        default:
            return "";
    }
}

char GetXFieldDBKey(const XFieldData& xfieldValue) {
    switch(GetXFieldTypeFrom(xfieldValue))
    {
        case TAPYRUS_XFIELDTYPES::AGGPUBKEY:
            return boost::get<XFieldAggPubKey>(xfieldValue).BLOCKTREE_DB_KEY;
        case TAPYRUS_XFIELDTYPES::MAXBLOCKSIZE:
            return boost::get<XFieldMaxBlockSize>(xfieldValue).BLOCKTREE_DB_KEY;
        case TAPYRUS_XFIELDTYPES::NONE:
        default:
            return '\0';
    }
}

void SaveToFederationParams(const XFieldData& xfieldValue) {
    /*switch(GetXFieldTypeFrom(xfieldValue))
    {
        case TAPYRUS_XFIELDTYPES::AGGPUBKEY:
            FederationParams().aggPubKey = boost::get<XFieldAggPubKey>(xfieldValue).data; break;
        case TAPYRUS_XFIELDTYPES::MAXBLOCKSIZE:
            FederationParams().maxBlockSize = boost::get<XFieldMaxBlockSize>(xfieldValue).data; break;
        case TAPYRUS_XFIELDTYPES::NONE:
        default:
            return;
    }*/
}