#!/usr/bin/env python3
# Copyright (c) 2024 Chaintope Inc.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test utxosnapshot rpc call
#

from test_framework.test_framework import BitcoinTestFramework


class UtxoSnapshotTest(BitcoinTestFramework):

    start_height = 1
    max_stat_pos = 99
        
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def get_stats(self):
        return [self.nodes[0].getblockstats(hash_or_height=self.start_height + i) for i in range(self.max_stat_pos+1)]

    def run_test(self):

        self.nodes[0].generate(100, self.signblockprivkey_wif)

        self.sync_all()
        stats = self.get_stats()

        self.nodes[0].utxosnapshot("utxosnapshot.file")

if __name__ == '__main__':
    UtxoSnapshotTest().main()
