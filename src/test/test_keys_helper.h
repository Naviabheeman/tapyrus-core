// Copyright (c) 2018-2019 chaintope Inc.

#ifndef TAPYRUS_TEST_TEST_KEYS_HELPER_H
#define TAPYRUS_TEST_TEST_KEYS_HELPER_H

#include <pubkey.h>
#include <secp256k1.h>

const std::vector<std::string> ValidPubKeyStrings = {
        "021a564bd5d483d1f248e15d25d8a77e7a0993080e9ecd1a254cb6f6b2515a1fc0",
        "020ad770bc838f167b9e0dc8781ff5dea1cc5c175ec8fc5af355855efaf69e8760",
        "028d14924263f705b0142f21d6a9673ec8e3a30704bb502c5c3ab5a019cd0b624d",
        "03fa7ff860dd5b24a0d988d7b5bae84f246296d393d690c89dd2625be9908061e4",
        "02958a0b98998310637bb7a032fb4d97234e5a38e01c07ea7b7a541c25e907b387",
        "033ac73090f32a210c40d72a685a611f0331d5f2f78d05dea1b4faa59fa5f909e7",
        "0393277249809d8f9397bebd191a169ad4e46c9a5d39f2ef2653d1ba8dc682b54a",
        "02e9698347edd21006af11da7384eb44a734e6631dbcbe20d1ea52dd0f76caa9e3",
        "03fcccf899c66027dc0fb39bc2b60a8ecf00156a79106c12eeb557280b53b76220",
        "026bda2fe2ff85358b57a3fb5f5dd9ba749ea3851e30607a692a4580623963d7b4",
        "03b90e054989ea73c12fbf9d1ff2dd0cc4cb027e4893acf78657e36cba19ec2415",
        "02aaac0f82c3f14ed46736eb01a5d372ca96540269680fc1e200da4e3f5f704c0f",
        "02eb6858400391f8d9f169f6de56204ea8467926604e709e1f35c81bbcf3cfff90",
        "03a89ac28fdda61eb9e2100a26b2fa2e37bddb4967d45d9869853557e91627075d",
        "03d367480efde98e7366c3d169caf6e923389c202340ddd1461553ea5d50268c5f",
        "0357780e3b3d366b60a720dd4ffc57ae96487469661be7dfe9c94385903154c77e"
};

const std::string UncompressedPubKeyString = "046b93737b4e8d93e79464f2054434015326f1834be1ec47e23377a8cc622b94a03f3c58c0c33248e2bb733269751facb479c098eec6ce254e00c7e45c103b7cd7";

std::string combinedPubkeyString(unsigned int keyCount);
std::vector<CPubKey> validPubKeys(unsigned int keyCount);

#endif //TAPYRUS_TEST_TEST_KEYS_HELPER_H