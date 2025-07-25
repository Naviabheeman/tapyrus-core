# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS) $(GPROF_LDFLAGS) $(SANITIZER_LDFLAGS)
AM_CXXFLAGS = $(DEBUG_CXXFLAGS) $(HARDENED_CXXFLAGS) $(WARN_CXXFLAGS) $(NOWARN_CXXFLAGS) $(ERROR_CXXFLAGS) $(GPROF_CXXFLAGS) $(SANITIZER_CXXFLAGS)
AM_CPPFLAGS = $(DEBUG_CPPFLAGS) $(HARDENED_CPPFLAGS)
AM_LIBTOOLFLAGS = --preserve-dup-deps
PTHREAD_FLAGS = $(PTHREAD_CFLAGS) $(PTHREAD_LIBS)
EXTRA_LIBRARIES =


BITCOIN_INCLUDES=-I$(builddir) $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS)

BITCOIN_INCLUDES += -I$(srcdir)/secp256k1/include
BITCOIN_INCLUDES += -I$(srcdir)/$(UNIVALUE_INCLUDE_DIR_INT)

LIBBITCOIN_SERVER=libtapyrus_server.a
LIBBITCOIN_COMMON=libtapyrus_common.a
LIBBITCOIN_CONSENSUS=libtapyrus_consensus.a
LIBBITCOIN_CLI=libtapyrus_cli.a
LIBBITCOIN_UTIL=libtapyrus_util.a
LIBBITCOIN_CRYPTO_BASE=crypto/libtapyrus_crypto_base.a
LIBBITCOINQT=qt/libtapyrusqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBBITCOIN_ZMQ=libtapyrus_zmq.a
endif
if BUILD_BITCOIN_LIBS
LIBBITCOINCONSENSUS=libtapyrusconsensus.la
endif
if ENABLE_WALLET
LIBBITCOIN_WALLET=libtapyrus_wallet.a
endif

LIBBITCOIN_CRYPTO= $(LIBBITCOIN_CRYPTO_BASE)
if ENABLE_SSE41
LIBBITCOIN_CRYPTO_SSE41 = crypto/libtapyrus_crypto_sse41.a
LIBBITCOIN_CRYPTO += $(LIBBITCOIN_CRYPTO_SSE41)
endif
if ENABLE_AVX2
LIBBITCOIN_CRYPTO_AVX2 = crypto/libtapyrus_crypto_avx2.a
LIBBITCOIN_CRYPTO += $(LIBBITCOIN_CRYPTO_AVX2)
endif
if ENABLE_SHANI
LIBBITCOIN_CRYPTO_SHANI = crypto/libtapyrus_crypto_shani.a
LIBBITCOIN_CRYPTO += $(LIBBITCOIN_CRYPTO_SHANI)
endif

LIBLEVELDB =
LIBMEMENV =

$(LIBSECP256K1): $(wildcard secp256k1/src/*.h) $(wildcard secp256k1/src/*.c) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_ZMQ)

lib_LTLIBRARIES = $(LIBBITCOINCONSENSUS)
noinst_LTLIBRARIES =

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_DAEMON
  bin_PROGRAMS += tapyrusd
endif

if BUILD_UTILS
  bin_PROGRAMS += tapyrus-cli tapyrus-tx tapyrus-genesis
endif

.PHONY: FORCE check-symbols check-security
# tapyrus core #
BITCOIN_CORE_H = \
  addrdb.h \
  addrman.h \
  base58.h \
  bech32.h \
  bloom.h \
  blockencodings.h \
  blockprune.h \
  chain.h \
  chainparams.h \
  chainparamsseeds.h \
  chainstate.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/cpuid.h \
  compat/endian.h \
  compressor.h \
  connecttrace.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  core_io.h \
  core_memusage.h \
  cs_main.h \
  cuckoocache.h \
  federationparams.h \
  file_io.h \
  fs.h \
  httprpc.h \
  httpserver.h \
  index/base.h \
  index/txindex.h \
  indirectmap.h \
  init.h \
  interfaces/handler.h \
  interfaces/node.h \
  interfaces/wallet.h \
  key.h \
  key_io.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  logging.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  outputtype.h \
  policy/feerate.h \
  policy/fees.h \
  policy/packages.h \
  policy/policy.h \
  policy/rbf.h \
  protocol.h \
  random.h \
  randomenv.h \
  reverse_iterator.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/client.h \
  rpc/mempool.h \
  rpc/mining.h \
  rpc/protocol.h \
  rpc/server.h \
  rpc/rawtransaction.h \
  rpc/register.h \
  rpc/util.h \
  scheduler.h \
  scriptcheck.h \
  script/descriptor.h \
  script/ismine.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  shutdown.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  tapyrusmodes.h \
  threadsafety.h \
  threadinterrupt.h \
  timeoffsets.h \
  torcontrol.h \
  trace.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  util.h \
  utilmemory.h \
  utilmoneystr.h \
  utiltime.h \
  utxo_snapshot.h \
  validation.h \
  validationinterface.h \
  walletinitinterface.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/fees.h \
  wallet/rpcwallet.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  wallet/walletutil.h \
  wallet/coinselection.h \
  warnings.h \
  verifydb.h \
  xfieldhistory.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h \
  zmq/zmqrpc.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libtapyrus_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between tapyrusd and tapyrus-qt
libtapyrus_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libtapyrus_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_server_a_SOURCES = \
  addrdb.cpp \
  addrman.cpp \
  bloom.cpp \
  blockencodings.cpp \
  blockprune.cpp \
  chain.cpp \
  chainstate.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  cs_main.cpp \
  file_io.cpp \
  httprpc.cpp \
  httpserver.cpp \
  index/base.cpp \
  index/txindex.cpp \
  init.cpp \
  dbwrapper.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  outputtype.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/packages.cpp \
  policy/rbf.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/mempool.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/server.cpp \
  rpc/util.cpp \
  script/sigcache.cpp \
  shutdown.cpp \
  timeoffsets.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  ui_interface.cpp \
  utxo_snapshot.cpp \
  validation.cpp \
  verifydb.cpp \
  validationinterface.cpp \
  $(BITCOIN_CORE_H)

if ENABLE_ZMQ
libtapyrus_zmq_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(ZMQ_CFLAGS)
libtapyrus_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp \
  zmq/zmqrpc.cpp
endif


# wallet: shared between tapyrusd and tapyrus-qt, but only linked
# when wallet enabled
libtapyrus_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libtapyrus_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_wallet_a_SOURCES = \
  interfaces/wallet.cpp \
  wallet/coincontrol.cpp \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/walletdb.cpp \
  wallet/walletutil.cpp \
  wallet/coinselection.cpp \
  $(BITCOIN_CORE_H)

# crypto primitives library
crypto_libtapyrus_crypto_base_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libtapyrus_crypto_base_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libtapyrus_crypto_base_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h

if USE_ASM
crypto_libtapyrus_crypto_base_a_SOURCES += crypto/sha256_sse4.cpp
endif

crypto_libtapyrus_crypto_sse41_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libtapyrus_crypto_sse41_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libtapyrus_crypto_sse41_a_CXXFLAGS += $(SSE41_CXXFLAGS)
crypto_libtapyrus_crypto_sse41_a_CPPFLAGS += -DENABLE_SSE41
crypto_libtapyrus_crypto_sse41_a_SOURCES = crypto/sha256_sse41.cpp

crypto_libtapyrus_crypto_avx2_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libtapyrus_crypto_avx2_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libtapyrus_crypto_avx2_a_CXXFLAGS += $(AVX2_CXXFLAGS)
crypto_libtapyrus_crypto_avx2_a_CPPFLAGS += -DENABLE_AVX2
crypto_libtapyrus_crypto_avx2_a_SOURCES = crypto/sha256_avx2.cpp

crypto_libtapyrus_crypto_shani_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libtapyrus_crypto_shani_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libtapyrus_crypto_shani_a_CXXFLAGS += $(SHANI_CXXFLAGS)
crypto_libtapyrus_crypto_shani_a_CPPFLAGS += -DENABLE_SHANI
crypto_libtapyrus_crypto_shani_a_SOURCES = crypto/sha256_shani.cpp

# consensus: shared between all executables that validate any consensus rules.
libtapyrus_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libtapyrus_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_consensus_a_SOURCES = \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  coloridentifier.cpp \
  coloridentifier.h \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  primitives/xfield.cpp \
  primitives/xfield.h \
  pubkey.cpp \
  pubkey.h \
  script/tapyrusconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  support/cleanse.cpp \
  support/cleanse.h \
  serialize.h \
  span.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h

# common: shared between tapyrusd, and tapyrus-qt and non-server tools
libtapyrus_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libtapyrus_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_common_a_SOURCES = \
  base58.cpp \
  bech32.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  federationparams.cpp \
  key.cpp \
  key_io.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/descriptor.cpp \
  script/ismine.cpp \
  script/sign.cpp \
  script/standard.cpp \
  warnings.cpp \
  xfieldhistory.cpp \
  $(BITCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libtapyrus_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libtapyrus_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_util_a_SOURCES = \
  support/lockedpool.cpp \
  clientversion.cpp \
  fs.cpp \
  interfaces/handler.cpp \
  interfaces/node.cpp \
  logging.cpp \
  random.cpp \
  rpc/protocol.cpp \
  randomenv.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(BITCOIN_CORE_H)

# cli: shared between tapyrus-cli and tapyrus-qt
libtapyrus_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
libtapyrus_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libtapyrus_cli_a_SOURCES = \
  rpc/client.cpp \
  $(BITCOIN_CORE_H)

nodist_libtapyrus_util_a_SOURCES = $(srcdir)/obj/build.h
#

# tapyrusd binary #
tapyrusd_SOURCES = tapyrusd.cpp
tapyrusd_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
tapyrusd_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
tapyrusd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS) $(PTHREAD_FLAGS)

if TARGET_WINDOWS
tapyrusd_SOURCES += tapyrus-res.rc
endif

tapyrusd_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_WALLET) \
  $(LIBBITCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_ZMQ) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

tapyrusd_LDADD += $(BDB_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS)

# tapyrus-cli binary #
tapyrus_cli_SOURCES = tapyrus-cli.cpp
tapyrus_cli_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES) $(EVENT_CFLAGS)
tapyrus_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
tapyrus_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS) $(PTHREAD_FLAGS)

if TARGET_WINDOWS
tapyrus_cli_SOURCES += tapyrus-cli-res.rc
endif

tapyrus_cli_LDADD = \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1) \
  $(LIBBITCOIN_CONSENSUS)
  

tapyrus_cli_LDADD += $(EVENT_LIBS)
#

# tapyrus-tx binary #
tapyrus_tx_SOURCES = tapyrus-tx.cpp
tapyrus_tx_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
tapyrus_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
tapyrus_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS) $(PTHREAD_FLAGS)

if TARGET_WINDOWS
tapyrus_tx_SOURCES += tapyrus-tx-res.rc
endif

tapyrus_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1)


# tapyrus-genesis binary #
tapyrus_genesis_SOURCES = tapyrus-genesis.cpp
tapyrus_genesis_CPPFLAGS = $(AM_CPPFLAGS) $(BITCOIN_INCLUDES)
tapyrus_genesis_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
tapyrus_genesis_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS) $(PTHREAD_FLAGS)

if TARGET_WINDOWS
tapyrus_genesis_SOURCES += tapyrus-genesis-res.rc
endif

tapyrus_genesis_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CONSENSUS) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1) \
  $(LIBLEVELDB) \
  $(LIBMEMENV)

tapyrus_genesis_LDADD += $(BDB_LIBS)
#

# tapyrusconsensus library #
if BUILD_BITCOIN_LIBS
include_HEADERS = script/tapyrusconsensus.h
include_HEADERS += coloridentifier.h
libtapyrusconsensus_la_SOURCES = $(crypto_libtapyrus_crypto_base_a_SOURCES) $(libtapyrus_consensus_a_SOURCES)
libtapyrusconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libtapyrusconsensus_la_LIBADD = $(LIBSECP256K1) $(BOOST_SYSTEM_LIB)
libtapyrusconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_BITCOIN_INTERNAL
libtapyrusconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno
CLEANFILES += obj/build.h

EXTRA_DIST = $(CTAES_DIST)

config/tapyrus-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/tapyrus-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/tapyrus-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/tapyrus-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

check-symbols: $(bin_PROGRAMS)

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py $(bin_PROGRAMS)
endif


include Makefile.leveldb.include

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif

include Makefile.univalue.include

# Add a special case for Darwin in the install-libLTLIBRARIES target
install-libLTLIBRARIES: $(lib_LTLIBRARIES)
    @$(NORMAL_INSTALL)
    @list='$(lib_LTLIBRARIES)'; test -n "$(libdir)" || list=; \
    list2=; for p in $$list; do \
      if test -f $$p; then \
        list2="$$list2 $$p"; \
      else :; fi; \
    done; \
    test -z "$$list2" || { \
      echo " $(MKDIR_P) '$(DESTDIR)$(libdir)'"; \
      $(MKDIR_P) "$(DESTDIR)$(libdir)" || exit 1; \
      for p in $$list2; do \
        if test "$$p" = "libtapyrusconsensus.la" && test "$(host_os)" = "darwin*"; then \
          echo "Installing libtapyrusconsensus.la without stripping on Darwin"; \
          $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $$p '$(DESTDIR)$(libdir)'; \
        else \
          echo " $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $(INSTALL_STRIP_FLAG) $$p '$(DESTDIR)$(libdir)'"; \
          $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(INSTALL) $(INSTALL_STRIP_FLAG) $$p '$(DESTDIR)$(libdir)'; \
        fi; \
      done; \
    }