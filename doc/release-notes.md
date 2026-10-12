(note: this is a temporary file, to be added-to by anybody, and moved to
release-notes at release time)

Notable changes
===============

Updated RPCs
------------

- `gettxoutsetinfo` returns a new field, `issued_colorids_hash`: a hash of the
  issued NON_REISSUABLE and NFT colorIds, which are kept in the chainstate
  database alongside the UTXO set. `hash_serialized_3` is unchanged and still
  covers only the UTXO set, so it stays comparable with older versions. To
  check that two nodes have the same chainstate, compare both fields.

Deprecated RPCs
---------------

The following are deprecated and will be removed in the next release:

- `validateaddress` wallet fields. `ismine`, `iswatchonly`, `script`, `hex`,
  `pubkeys`, `sigsrequired`, `pubkey`, `addresses`, `embedded`,
  `iscompressed`, `account`, `timestamp`, `hdkeypath` and `kdmasterkeyid` are
  returned only with `-deprecatedrpc=validateaddress`. Use `getaddressinfo`
  instead.
- `signrawtransaction`. It works only with
  `-deprecatedrpc=signrawtransaction`. Use `signrawtransactionwithkey` or
  `signrawtransactionwithwallet` instead.
- `getinfo`. This hidden command only returns an error naming the RPCs that
  replace it: `getblockchaininfo`, `getnetworkinfo` and `getwalletinfo`.
  `tapyrus-cli -getinfo` shows the same information in one view.

P2P
---

- When a block arrives, the node now clears outstanding requests for that
  block to every peer, not only the one that sent it. A block completed from
  one peer's compact block used to leave a request to another peer pending,
  and that peer was disconnected for a block download timeout 10 minutes or
  more later.
