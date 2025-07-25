// Copyright (c) 2011-2018 The Bitcoin Core developers
// Copyright (c) 2019-2020 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util.h>

#include <clientversion.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>
#include <test/test_tapyrus.h>

#include <stdint.h>
#include <vector>
#ifndef WIN32
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_criticalsection)
{
    Mutex cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while(0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest)
            break;

        BOOST_ERROR("break was swallowed!");
    } while(0);
}

static const unsigned char ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67, 0xf1, 0xa6, 0x71, 0x30, 0xb7,
    0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0, 0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde,
    0xb6, 0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04, 0xe5, 0x1e, 0xc1, 0x12,
    0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b, 0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d,
    0x5f
};
BOOST_AUTO_TEST_CASE(util_ParseHex)
{
    std::vector<unsigned char> result;
    std::vector<unsigned char> expected(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
    // Basic test vector
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());

    // Spaces between bytes must be supported
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4);
    BOOST_CHECK(result[0] == 0x12);
    BOOST_CHECK(result[1] == 0x34);
    BOOST_CHECK(result[2] == 0x56);
    BOOST_CHECK(result[3] == 0x78);

    // Leading space must be supported (used in BerkeleyEnvironment::Salvage)
    result = ParseHex(" 89 34 56 78");
    BOOST_CHECK(result.size() == 4);
    BOOST_CHECK(result[0] == 0x89);
    BOOST_CHECK(result[1] == 0x34);
    BOOST_CHECK(result[2] == 0x56);
    BOOST_CHECK(result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2);
    BOOST_CHECK(result[0] == 0x12);
    BOOST_CHECK(result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr)
{
    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected)),
        "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected + 5, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected + sizeof(ParseHex_expected),
               ParseHex_expected + sizeof(ParseHex_expected)),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected + sizeof(ParseHex_expected),
               ParseHex_expected + sizeof(ParseHex_expected), true),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_expected, ParseHex_expected, true),
        "");

    std::vector<unsigned char> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec, true),
        "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend()),
        "b0fd8a6704"
    );

    BOOST_CHECK_EQUAL(
        HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend(), true),
        "b0 fd 8a 67 04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        ""
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        ""
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "b0fd8a6704"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "b0 fd 8a 67 04"
    );

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 65),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "5f1df16b2b704c8a578d0bbaf74d385cde12c11ee50455f3c438ef4c3fbcf649b6de611feae06279a60939e028a8d65c10b73071a6f16719274855feb0fd8a6704"
    );
}


BOOST_AUTO_TEST_CASE(util_FormatISO8601DateTime)
{
    BOOST_CHECK_EQUAL(FormatISO8601DateTime(1317425777), "2011-09-30T23:36:17Z");
}

BOOST_AUTO_TEST_CASE(util_FormatISO8601Date)
{
    BOOST_CHECK_EQUAL(FormatISO8601Date(1317425777), "2011-09-30");
}

BOOST_AUTO_TEST_CASE(util_FormatISO8601Time)
{
    BOOST_CHECK_EQUAL(FormatISO8601Time(1317425777), "23:36:17Z");
}

struct TestArgsManager : public ArgsManager
{
    TestArgsManager() { m_network_only_args.clear(); }
    std::map<std::string, std::vector<std::string> >& GetOverrideArgs() { return m_override_args; }
    std::map<std::string, std::vector<std::string> >& GetConfigArgs() { return m_config_args; }
    void ReadConfigString(const std::string str_config)
    {
        std::istringstream streamConfig(str_config);
        {
            LOCK(cs_args);
            m_config_args.clear();
        }
        std::string error;
        ReadConfigStream(streamConfig, error);
    }
    void SetNetworkOnlyArg(const std::string arg)
    {
        LOCK(cs_args);
        m_network_only_args.insert(arg);
    }
    void SetupArgs(int argv, const char* args[])
    {
        for (int i = 0; i < argv; ++i) {
            AddArg(args[i], "", false, OptionsCategory::OPTIONS);
        }
    }
};

BOOST_AUTO_TEST_CASE(util_ParseParameters)
{
    TestArgsManager testArgs;
    const char* avail_args[] = {"-a", "-b", "-ccc", "-d"};
    const char *argv_test[] = {"-ignored", "-a", "-b", "-ccc=argument", "-ccc=multiple", "f", "-d=e"};

    std::string error;
    testArgs.SetupArgs(4, avail_args);
    testArgs.ParseParameters(0, (char**)argv_test, error);
    BOOST_CHECK(testArgs.GetOverrideArgs().empty());
    BOOST_CHECK(testArgs.GetConfigArgs().empty());

    testArgs.ParseParameters(1, (char**)argv_test, error);
    BOOST_CHECK(testArgs.GetOverrideArgs().empty());
    BOOST_CHECK(testArgs.GetConfigArgs().empty());

    testArgs.ParseParameters(7, (char**)argv_test, error);
    // expectation: -ignored is ignored (program name argument),
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 3);
    BOOST_CHECK(testArgs.GetConfigArgs().empty());
    BOOST_CHECK(testArgs.IsArgSet("-a"));
    BOOST_CHECK(testArgs.IsArgSet("-b"));
    BOOST_CHECK(testArgs.IsArgSet("-ccc"));
    BOOST_CHECK(!testArgs.IsArgSet("f"));
    BOOST_CHECK(!testArgs.IsArgSet("-d"));
    BOOST_CHECK(testArgs.GetOverrideArgs().count("-a"));
    BOOST_CHECK(testArgs.GetOverrideArgs().count("-b"));
    BOOST_CHECK(testArgs.GetOverrideArgs().count("-ccc"));
    BOOST_CHECK(!testArgs.GetOverrideArgs().count("f"));
    BOOST_CHECK(!testArgs.GetOverrideArgs().count("-d"));

    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].size() == 1);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].front() == "");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].size() == 2);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].front() == "argument");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].back() == "multiple");
    BOOST_CHECK(testArgs.GetArgs("-ccc").size() == 2);
}

BOOST_AUTO_TEST_CASE(util_GetBoolArg)
{
    TestArgsManager testArgs;
    const char* avail_args[] = {"-a", "-b", "-c", "-d", "-e", "-f"};
    const char *argv_test[] = {
        "ignored", "-a", "-nob", "-c=0", "-d=1", "-e=false", "-f=true"};
    std::string error;
    testArgs.SetupArgs(6, avail_args);
    testArgs.ParseParameters(7, (char**)argv_test, error);

    // Each letter should be set.
    for (char opt : "abcdef") {
        if (opt) {
            BOOST_CHECK(testArgs.IsArgSet({'-', opt}));
        } else {
            BOOST_CHECK(true);
        }
    }

    // Nothing else should be in the map
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 6);
    BOOST_CHECK(testArgs.GetConfigArgs().empty());

    // The -no prefix should get stripped on the way in.
    BOOST_CHECK(!testArgs.IsArgSet("-nob"));

    // The -b option is flagged as negated, and nothing else is
    BOOST_CHECK(testArgs.IsArgNegated("-b"));
    BOOST_CHECK(!testArgs.IsArgNegated("-a"));

    // Check expected values.
    BOOST_CHECK(testArgs.GetBoolArg("-a", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-b", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-c", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-d", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-e", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-f", true) == false);
}

BOOST_AUTO_TEST_CASE(util_GetBoolArgEdgeCases)
{
    // Test some awful edge cases that hopefully no user will ever exercise.
    TestArgsManager testArgs;

    // Params test
    const char* avail_args[] = {"-foo", "-bar"};
    const char *argv_test[] = {"ignored", "-nofoo", "-foo", "-nobar=0"};
    testArgs.SetupArgs(2, avail_args);
    std::string error;
    testArgs.ParseParameters(4, (char**)argv_test, error);

    // This was passed twice, second one overrides the negative setting.
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "");

    // A double negative is a positive, and not marked as negated.
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

    // Config test
    const char *conf_test = "nofoo=1\nfoo=1\nnobar=0\n";
    testArgs.ParseParameters(1, (char**)argv_test, error);
    testArgs.ReadConfigString(conf_test);

    // This was passed twice, second one overrides the negative setting,
    // and the value.
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "1");

    // A double negative is a positive, and does not count as negated.
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

    // Combined test
    const char *combo_test_args[] = {"ignored", "-nofoo", "-bar"};
    const char *combo_test_conf = "foo=1\nnobar=1\n";
    testArgs.ParseParameters(3, (char**)combo_test_args, error);
    testArgs.ReadConfigString(combo_test_conf);

    // Command line overrides, but doesn't erase old setting
    BOOST_CHECK(testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "0");
    BOOST_CHECK(testArgs.GetArgs("-foo").size() == 0);

    // Command line overrides, but doesn't erase old setting
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "");
    BOOST_CHECK(testArgs.GetArgs("-bar").size() == 1);
    BOOST_CHECK(testArgs.GetArgs("-bar").front() == "");
}

BOOST_AUTO_TEST_CASE(util_ReadConfigStream)
{
    const char *str_config =
       "a=\n"
       "b=1\n"
       "ccc=argument\n"
       "ccc=multiple\n"
       "d=e\n"
       "nofff=1\n"
       "noggg=0\n"
       "h=1\n"
       "noh=1\n"
       "noi=1\n"
       "i=1\n"
       "sec1.ccc=extend1\n"
       "\n"
       "[sec1]\n"
       "ccc=extend2\n"
       "d=eee\n"
       "h=1\n"
       "[sec2]\n"
       "ccc=extend3\n"
       "iii=2\n";

    TestArgsManager test_args;
    const char* avail_args[] = {"-a", "-b", "-ccc", "-d", "-e", "-fff", "-ggg", "-h", "-i", "-iii"};
    test_args.SetupArgs(10, avail_args);

    test_args.ReadConfigString(str_config);
    // expectation: a, b, ccc, d, fff, ggg, h, i end up in map
    // so do sec1.ccc, sec1.d, sec1.h, sec2.ccc, sec2.iii

    BOOST_CHECK(test_args.GetOverrideArgs().empty());
    BOOST_CHECK(test_args.GetConfigArgs().size() == 13);

    BOOST_CHECK(test_args.GetConfigArgs().count("-a"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-b"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-ccc"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-d"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-fff"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-ggg"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-h"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-i"));
    BOOST_CHECK(!test_args.GetConfigArgs().count("-zzz"));
    BOOST_CHECK(!test_args.GetConfigArgs().count("-iii"));

    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "");
    BOOST_CHECK(test_args.GetArg("-b", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "argument");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "e");
    BOOST_CHECK(test_args.GetArg("-fff", "xxx") == "0");
    BOOST_CHECK(test_args.GetArg("-ggg", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");
    BOOST_CHECK(test_args.GetArg("-i", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-zzz", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArg("-iii", "xxx") == "xxx");

    for (bool def : {false, true}) {
        BOOST_CHECK(test_args.GetBoolArg("-a", def));
        BOOST_CHECK(test_args.GetBoolArg("-b", def));
        BOOST_CHECK(!test_args.GetBoolArg("-ccc", def));
        BOOST_CHECK(!test_args.GetBoolArg("-d", def));
        BOOST_CHECK(!test_args.GetBoolArg("-fff", def));
        BOOST_CHECK(test_args.GetBoolArg("-ggg", def));
        BOOST_CHECK(!test_args.GetBoolArg("-h", def));
        BOOST_CHECK(test_args.GetBoolArg("-i", def));
        BOOST_CHECK(test_args.GetBoolArg("-zzz", def) == def);
        BOOST_CHECK(test_args.GetBoolArg("-iii", def) == def);
    }

    BOOST_CHECK(test_args.GetArgs("-a").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-a").front() == "");
    BOOST_CHECK(test_args.GetArgs("-b").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-b").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK( test_args.GetArgs("-ccc").front() == "argument");
    BOOST_CHECK(test_args.GetArgs("-ccc").back() == "multiple");
    BOOST_CHECK(test_args.GetArgs("-fff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-nofff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ggg").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-ggg").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noggg").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-h").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-noh").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-i").size() == 1);
    BOOST_CHECK( test_args.GetArgs("-i").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noi").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-zzz").size() == 0);

    BOOST_CHECK(!test_args.IsArgNegated("-a"));
    BOOST_CHECK(!test_args.IsArgNegated("-b"));
    BOOST_CHECK(!test_args.IsArgNegated("-ccc"));
    BOOST_CHECK(!test_args.IsArgNegated("-d"));
    BOOST_CHECK(test_args.IsArgNegated("-fff"));
    BOOST_CHECK(!test_args.IsArgNegated("-ggg"));
    BOOST_CHECK(test_args.IsArgNegated("-h")); // last setting takes precedence
    BOOST_CHECK(!test_args.IsArgNegated("-i")); // last setting takes precedence
    BOOST_CHECK(!test_args.IsArgNegated("-zzz"));

    // Test sections work
    test_args.SelectConfigNetwork("sec1");

    // same as original
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "");
    BOOST_CHECK(test_args.GetArg("-b", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-fff", "xxx") == "0");
    BOOST_CHECK(test_args.GetArg("-ggg", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-zzz", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArg("-iii", "xxx") == "xxx");
    // d is overridden
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
    // section-specific setting
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");
    // section takes priority for multiple values
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend1");
    // check multiple values works
    const std::vector<std::string> sec1_ccc_expected = {"extend1","extend2","argument","multiple"};
    const auto& sec1_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec1_ccc_res.begin(), sec1_ccc_res.end(), sec1_ccc_expected.begin(), sec1_ccc_expected.end());

    test_args.SelectConfigNetwork("sec2");

    // same as original
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "");
    BOOST_CHECK(test_args.GetArg("-b", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "e");
    BOOST_CHECK(test_args.GetArg("-fff", "xxx") == "0");
    BOOST_CHECK(test_args.GetArg("-ggg", "xxx") == "1");
    BOOST_CHECK(test_args.GetArg("-zzz", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");
    // section-specific setting
    BOOST_CHECK(test_args.GetArg("-iii", "xxx") == "2");
    // section takes priority for multiple values
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend3");
    // check multiple values works
    const std::vector<std::string> sec2_ccc_expected = {"extend3","argument","multiple"};
    const auto& sec2_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec2_ccc_res.begin(), sec2_ccc_res.end(), sec2_ccc_expected.begin(), sec2_ccc_expected.end());

    // Test section only options

    test_args.SetNetworkOnlyArg("-d");
    test_args.SetNetworkOnlyArg("-ccc");
    test_args.SetNetworkOnlyArg("-h");

    test_args.SelectConfigNetwork(TAPYRUS_MODES::PROD);
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "e");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");

    test_args.SelectConfigNetwork("sec1");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");

    test_args.SelectConfigNetwork("sec2");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 1);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");
}

BOOST_AUTO_TEST_CASE(util_GetArg)
{
    TestArgsManager testArgs;
    testArgs.GetOverrideArgs().clear();
    testArgs.GetOverrideArgs()["strtest1"] = {"string..."};
    // strtest2 undefined on purpose
    testArgs.GetOverrideArgs()["inttest1"] = {"12345"};
    testArgs.GetOverrideArgs()["inttest2"] = {"81985529216486895"};
    // inttest3 undefined on purpose
    testArgs.GetOverrideArgs()["booltest1"] = {""};
    // booltest2 undefined on purpose
    testArgs.GetOverrideArgs()["booltest3"] = {"0"};
    testArgs.GetOverrideArgs()["booltest4"] = {"1"};

    // priorities
    testArgs.GetOverrideArgs()["pritest1"] = {"a", "b"};
    testArgs.GetConfigArgs()["pritest2"] = {"a", "b"};
    testArgs.GetOverrideArgs()["pritest3"] = {"a"};
    testArgs.GetConfigArgs()["pritest3"] = {"b"};
    testArgs.GetOverrideArgs()["pritest4"] = {"a","b"};
    testArgs.GetConfigArgs()["pritest4"] = {"c","d"};

    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest2", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest3", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest4", false), true);

    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest1", "default"), "b");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest2", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest3", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest4", "default"), "b");
}

BOOST_AUTO_TEST_CASE(util_GetChainMode)
{
    TestArgsManager test_args;
    const char* avail_args[] = {"-dev"};
    test_args.SetupArgs(1, avail_args);

    const char* argv_testnet[] = {"cmd", "-testnet"};
    const char* argv_dev[] = {"cmd", "-dev"};

    // equivalent to "-dev"
    // dev in prod section is ignored
    const char* testnetconf = "dev=0\n[prod]\ndev=1";
    std::string error;

    test_args.ParseParameters(0, (char**)argv_testnet, error);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_testnet, error);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_dev, error);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::DEV);

    test_args.ParseParameters(0, (char**)argv_testnet, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_testnet, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_dev, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::DEV);

    // check setting the network to prod (and thus making
    // [prod] dev=1 potentially relevant) doesn't break things
    test_args.SelectConfigNetwork("prod");

    test_args.ParseParameters(0, (char**)argv_testnet, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_testnet, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::PROD);

    test_args.ParseParameters(2, (char**)argv_dev, error);
    test_args.ReadConfigString(testnetconf);
    BOOST_CHECK_EQUAL(TAPYRUS_MODES::GetChainName(test_args.GetChainMode()), TAPYRUS_MODES::DEV);
}

BOOST_AUTO_TEST_CASE(util_FormatMoney)
{
    BOOST_CHECK_EQUAL(FormatMoney(0), "0.00");
    BOOST_CHECK_EQUAL(FormatMoney((COIN/10000)*123456789), "12345.6789");
    BOOST_CHECK_EQUAL(FormatMoney(-COIN), "-1.00");

    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000000), "100000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000000), "10000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000000), "1000000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100000), "100000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10000), "10000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*1000), "1000.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*100), "100.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN*10), "10.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN), "1.00");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10), "0.10");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100), "0.01");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000), "0.001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000), "0.0001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000), "0.00001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/1000000), "0.000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/10000000), "0.0000001");
    BOOST_CHECK_EQUAL(FormatMoney(COIN/100000000), "0.00000001");
}

BOOST_AUTO_TEST_CASE(util_ParseMoney)
{
    CAmount ret = 0;
    BOOST_CHECK(ParseMoney("0.0", ret));
    BOOST_CHECK_EQUAL(ret, 0);

    BOOST_CHECK(ParseMoney("12345.6789", ret));
    BOOST_CHECK_EQUAL(ret, (COIN/10000)*123456789);

    BOOST_CHECK(ParseMoney("100000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000000);
    BOOST_CHECK(ParseMoney("10000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000000);
    BOOST_CHECK(ParseMoney("1000000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000000);
    BOOST_CHECK(ParseMoney("100000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100000);
    BOOST_CHECK(ParseMoney("10000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10000);
    BOOST_CHECK(ParseMoney("1000.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*1000);
    BOOST_CHECK(ParseMoney("100.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*100);
    BOOST_CHECK(ParseMoney("10.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN*10);
    BOOST_CHECK(ParseMoney("1.00", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("1", ret));
    BOOST_CHECK_EQUAL(ret, COIN);
    BOOST_CHECK(ParseMoney("0.1", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10);
    BOOST_CHECK(ParseMoney("0.01", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100);
    BOOST_CHECK(ParseMoney("0.001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000);
    BOOST_CHECK(ParseMoney("0.0001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000);
    BOOST_CHECK(ParseMoney("0.00001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000);
    BOOST_CHECK(ParseMoney("0.000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/1000000);
    BOOST_CHECK(ParseMoney("0.0000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/10000000);
    BOOST_CHECK(ParseMoney("0.00000001", ret));
    BOOST_CHECK_EQUAL(ret, COIN/100000000);

    // Attempted 63 bit overflow should fail
    BOOST_CHECK(!ParseMoney("92233720368.54775808", ret));

    // Parsing negative amounts must fail
    BOOST_CHECK(!ParseMoney("-1", ret));
}

BOOST_AUTO_TEST_CASE(util_IsHex)
{
    BOOST_CHECK(IsHex("00"));
    BOOST_CHECK(IsHex("00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHex("ff"));
    BOOST_CHECK(IsHex("FF"));

    BOOST_CHECK(!IsHex(""));
    BOOST_CHECK(!IsHex("0"));
    BOOST_CHECK(!IsHex("a"));
    BOOST_CHECK(!IsHex("eleven"));
    BOOST_CHECK(!IsHex("00xx00"));
    BOOST_CHECK(!IsHex("0x0000"));
}

BOOST_AUTO_TEST_CASE(util_IsHexNumber)
{
    BOOST_CHECK(IsHexNumber("0x0"));
    BOOST_CHECK(IsHexNumber("0"));
    BOOST_CHECK(IsHexNumber("0x10"));
    BOOST_CHECK(IsHexNumber("10"));
    BOOST_CHECK(IsHexNumber("0xff"));
    BOOST_CHECK(IsHexNumber("ff"));
    BOOST_CHECK(IsHexNumber("0xFfa"));
    BOOST_CHECK(IsHexNumber("Ffa"));
    BOOST_CHECK(IsHexNumber("0x00112233445566778899aabbccddeeffAABBCCDDEEFF"));
    BOOST_CHECK(IsHexNumber("00112233445566778899aabbccddeeffAABBCCDDEEFF"));

    BOOST_CHECK(!IsHexNumber(""));   // empty string not allowed
    BOOST_CHECK(!IsHexNumber("0x")); // empty string after prefix not allowed
    BOOST_CHECK(!IsHexNumber("0x0 ")); // no spaces at end,
    BOOST_CHECK(!IsHexNumber(" 0x0")); // or beginning,
    BOOST_CHECK(!IsHexNumber("0x 0")); // or middle,
    BOOST_CHECK(!IsHexNumber(" "));    // etc.
    BOOST_CHECK(!IsHexNumber("0x0ga")); // invalid character
    BOOST_CHECK(!IsHexNumber("x0"));    // broken prefix
    BOOST_CHECK(!IsHexNumber("0x0x00")); // two prefixes not allowed

}

BOOST_AUTO_TEST_CASE(util_seed_insecure_rand)
{
    SeedInsecureRand(true);
    for (int mod=2;mod<11;mod++)
    {
        int mask = 1;
        // Really rough binomial confidence approximation.
        int err = 30*10000./mod*sqrt((1./mod*(1-1./mod))/10000.);
        //mask is 2^ceil(log2(mod))-1
        while(mask<mod-1)mask=(mask<<1)+1;

        int count = 0;
        //How often does it get a zero from the uniform range [0,mod)?
        for (int i = 0; i < 10000; i++) {
            uint32_t rval;
            do{
                rval=InsecureRand32()&mask;
            }while(rval>=(uint32_t)mod);
            count += rval==0;
        }
        BOOST_CHECK(count<=10000/mod+err);
        BOOST_CHECK(count>=10000/mod-err);
    }
}

BOOST_AUTO_TEST_CASE(util_TimingResistantEqual)
{
    BOOST_CHECK(TimingResistantEqual(std::string(""), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("")));
    BOOST_CHECK(!TimingResistantEqual(std::string(""), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("a"), std::string("aa")));
    BOOST_CHECK(!TimingResistantEqual(std::string("aa"), std::string("a")));
    BOOST_CHECK(TimingResistantEqual(std::string("abc"), std::string("abc")));
    BOOST_CHECK(!TimingResistantEqual(std::string("abc"), std::string("aba")));
}

/* Test strprintf formatting directives.
 * Put a string before and after to ensure sanity of element sizes on stack. */
#define B "check_prefix"
#define E "check_postfix"
BOOST_AUTO_TEST_CASE(strprintf_numbers)
{
    int64_t s64t = -9223372036854775807LL; /* signed 64 bit test value */
    uint64_t u64t = 18446744073709551615ULL; /* unsigned 64 bit test value */
    BOOST_CHECK(strprintf("%s %d %s", B, s64t, E) == B" -9223372036854775807 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, u64t, E) == B" 18446744073709551615 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, u64t, E) == B" ffffffffffffffff " E);

    size_t st = 12345678; /* unsigned size_t test value */
    ssize_t sst = -12345678; /* signed size_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, sst, E) == B" -12345678 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, st, E) == B" 12345678 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, st, E) == B" bc614e " E);

    ptrdiff_t pt = 87654321; /* positive ptrdiff_t test value */
    ptrdiff_t spt = -87654321; /* negative ptrdiff_t test value */
    BOOST_CHECK(strprintf("%s %d %s", B, spt, E) == B" -87654321 " E);
    BOOST_CHECK(strprintf("%s %u %s", B, pt, E) == B" 87654321 " E);
    BOOST_CHECK(strprintf("%s %x %s", B, pt, E) == B" 5397fb1 " E);
}
#undef B
#undef E

/* Check for mingw/wine issue #3494
 * Remove this test before time.ctime(0xffffffff) == 'Sun Feb  7 07:28:15 2106'
 */
BOOST_AUTO_TEST_CASE(gettime)
{
    BOOST_CHECK((GetTime() & ~0xFFFFFFFFLL) == 0);
}
BOOST_AUTO_TEST_CASE(util_time_GetTime)
{
    SetMockTime(111);
    // Check that mock time does not change after a sleep
    for (const auto& num_sleep : {0, 1}) {
        MilliSleep(num_sleep);
        BOOST_CHECK_EQUAL(111, GetTime());
        BOOST_CHECK_EQUAL(111, GetAdjustedTime());
        BOOST_CHECK_EQUAL(111000, GetTimeMillis(true));
        BOOST_CHECK_EQUAL(111000000, GetTimeMicros(true));

        const auto ntime = GetSystemTimeInSeconds();
        BOOST_CHECK_EQUAL(ntime, GetTimeMillis()/1000);
        BOOST_CHECK_EQUAL(ntime, GetTimeMicros()/1000000);
    }

    SetMockTime(0);
    // Check that steady time and system time changes after a sleep
    const auto steady_ms_0 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto steady_0 = std::chrono::steady_clock::now();
    const auto ms_0 = GetTimeMillis();
    const auto us_0 = GetTimeMicros();
    MilliSleep(1);
    BOOST_CHECK(steady_ms_0 < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    BOOST_CHECK(steady_0 + std::chrono::milliseconds(1) <= std::chrono::steady_clock::now());
    BOOST_CHECK(ms_0 < GetTimeMillis());
    BOOST_CHECK(us_0 < GetTimeMicros());
}

BOOST_AUTO_TEST_CASE(test_IsDigit)
{
    BOOST_CHECK_EQUAL(IsDigit('0'), true);
    BOOST_CHECK_EQUAL(IsDigit('1'), true);
    BOOST_CHECK_EQUAL(IsDigit('8'), true);
    BOOST_CHECK_EQUAL(IsDigit('9'), true);

    BOOST_CHECK_EQUAL(IsDigit('0' - 1), false);
    BOOST_CHECK_EQUAL(IsDigit('9' + 1), false);
    BOOST_CHECK_EQUAL(IsDigit(0), false);
    BOOST_CHECK_EQUAL(IsDigit(1), false);
    BOOST_CHECK_EQUAL(IsDigit(8), false);
    BOOST_CHECK_EQUAL(IsDigit(9), false);
}

BOOST_AUTO_TEST_CASE(test_ParseInt32)
{
    int32_t n;
    // Valid values
    BOOST_CHECK(ParseInt32("1234", nullptr));
    BOOST_CHECK(ParseInt32("0", &n));
    BOOST_CHECK(n == 0);
    BOOST_CHECK(ParseInt32("1234", &n));
    BOOST_CHECK(n == 1234);
    BOOST_CHECK(ParseInt32("01234", &n)); // no octal
    BOOST_CHECK(n == 1234);
    BOOST_CHECK(ParseInt32("2147483647", &n));
    BOOST_CHECK(n == 2147483647);
    BOOST_CHECK(ParseInt32("-2147483648", &n));
    BOOST_CHECK(n == (-2147483647 - 1)); // (-2147483647 - 1) equals INT_MIN
    BOOST_CHECK(ParseInt32("-1234", &n));
    BOOST_CHECK(n == -1234);

    // Invalid values
    BOOST_CHECK(!ParseInt32("", &n));
    BOOST_CHECK(!ParseInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt32("1 ", &n));
    BOOST_CHECK(!ParseInt32("1a", &n));
    BOOST_CHECK(!ParseInt32("aap", &n));
    BOOST_CHECK(!ParseInt32("0x1", &n)); // no hex
    BOOST_CHECK(!ParseInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt32("-2147483649", nullptr));
    BOOST_CHECK(!ParseInt32("2147483648", nullptr));
    BOOST_CHECK(!ParseInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseInt64)
{
    int64_t n;
    // Valid values
    BOOST_CHECK(ParseInt64("0", &n));
    BOOST_CHECK(n == 0LL);
    BOOST_CHECK(ParseInt64("1234", &n));
    BOOST_CHECK(n == 1234LL);
    BOOST_CHECK(ParseInt64("01234", &n));
    BOOST_CHECK(n == 1234LL); // no octal
    BOOST_CHECK(ParseInt64("2147483647", &n));
    BOOST_CHECK(n == 2147483647LL);
    BOOST_CHECK(ParseInt64("-2147483648", &n));
    BOOST_CHECK(n == -2147483648LL);
    BOOST_CHECK(ParseInt64("-1234", &n));
    BOOST_CHECK(n == -1234LL);
    // Invalid values
    BOOST_CHECK(!ParseInt64("", &n));
    BOOST_CHECK(!ParseInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseInt64("1 ", &n));
    BOOST_CHECK(!ParseInt64("1a", &n));
    BOOST_CHECK(!ParseInt64("aap", &n));
    BOOST_CHECK(!ParseInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseInt64("9223372036854775808", nullptr));
    BOOST_CHECK(!ParseInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseInt64("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt32)
{
    uint32_t n;
    // Valid values
    BOOST_CHECK(ParseUInt32("1234", nullptr));
    BOOST_CHECK(ParseUInt32("0", &n));
    BOOST_CHECK( n == 0);
    BOOST_CHECK(ParseUInt32("1234", &n));
    BOOST_CHECK( n == 1234);
    BOOST_CHECK(ParseUInt32("01234", &n));
    BOOST_CHECK( n == 1234); // no octal
    BOOST_CHECK(ParseUInt32("2147483647", &n));
    BOOST_CHECK( n == 2147483647);
    BOOST_CHECK(ParseUInt32("2147483648", &n));
    BOOST_CHECK( n == (uint32_t)2147483648);
    BOOST_CHECK(ParseUInt32("4294967295", &n));
    BOOST_CHECK( n == (uint32_t)4294967295);
    // Invalid values
    BOOST_CHECK(!ParseUInt32("", &n));
    BOOST_CHECK(!ParseUInt32(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt32(" -1", &n));
    BOOST_CHECK(!ParseUInt32("1 ", &n));
    BOOST_CHECK(!ParseUInt32("1a", &n));
    BOOST_CHECK(!ParseUInt32("aap", &n));
    BOOST_CHECK(!ParseUInt32("0x1", &n)); // no hex
    BOOST_CHECK(!ParseUInt32("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt32(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt32("-2147483648", &n));
    BOOST_CHECK(!ParseUInt32("4294967296", &n));
    BOOST_CHECK(!ParseUInt32("-1234", &n));
    BOOST_CHECK(!ParseUInt32("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt32("32482348723847471234", nullptr));
}

BOOST_AUTO_TEST_CASE(test_ParseUInt64)
{
    uint64_t n;
    // Valid values
    BOOST_CHECK(ParseUInt64("1234", nullptr));
    BOOST_CHECK(ParseUInt64("0", &n));
    BOOST_CHECK( n == 0LL);
    BOOST_CHECK(ParseUInt64("1234", &n));
    BOOST_CHECK( n == 1234LL);
    BOOST_CHECK(ParseUInt64("01234", &n));
    BOOST_CHECK(n == 1234LL);
    BOOST_CHECK(ParseUInt64("2147483647", &n));
    BOOST_CHECK( n == 2147483647LL);
    BOOST_CHECK(ParseUInt64("9223372036854775807", &n));
    BOOST_CHECK( n == 9223372036854775807ULL);
    BOOST_CHECK(ParseUInt64("9223372036854775808", &n));
    BOOST_CHECK( n == 9223372036854775808ULL);
    BOOST_CHECK(ParseUInt64("18446744073709551615", &n));
    BOOST_CHECK( n == 18446744073709551615ULL);
    // Invalid values
    BOOST_CHECK(!ParseUInt64("", &n));
    BOOST_CHECK(!ParseUInt64(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseUInt64(" -1", &n));
    BOOST_CHECK(!ParseUInt64("1 ", &n));
    BOOST_CHECK(!ParseUInt64("1a", &n));
    BOOST_CHECK(!ParseUInt64("aap", &n));
    BOOST_CHECK(!ParseUInt64("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseUInt64(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseUInt64("-9223372036854775809", nullptr));
    BOOST_CHECK(!ParseUInt64("18446744073709551616", nullptr));
    BOOST_CHECK(!ParseUInt64("-32482348723847471234", nullptr));
    BOOST_CHECK(!ParseUInt64("-2147483648", &n));
    BOOST_CHECK(!ParseUInt64("-9223372036854775808", &n));
    BOOST_CHECK(!ParseUInt64("-1234", &n));
}

BOOST_AUTO_TEST_CASE(test_ParseDouble)
{
    double n;
    // Valid values
    BOOST_CHECK(ParseDouble("1234", nullptr));
    BOOST_CHECK(ParseDouble("0", &n));
    BOOST_CHECK( n == 0.0);
    BOOST_CHECK(ParseDouble("1234", &n));
    BOOST_CHECK( n == 1234.0);
    BOOST_CHECK(ParseDouble("01234", &n));
    BOOST_CHECK( n == 1234.0); // no octal
    BOOST_CHECK(ParseDouble("2147483647", &n));
    BOOST_CHECK( n == 2147483647.0);
    BOOST_CHECK(ParseDouble("-2147483648", &n));
    BOOST_CHECK( n == -2147483648.0);
    BOOST_CHECK(ParseDouble("-1234", &n));
    BOOST_CHECK( n == -1234.0);
    BOOST_CHECK(ParseDouble("1e6", &n));
    BOOST_CHECK( n == 1e6);
    BOOST_CHECK(ParseDouble("-1e6", &n));
    BOOST_CHECK( n == -1e6);
    // Invalid values
    BOOST_CHECK(!ParseDouble("", &n));
    BOOST_CHECK(!ParseDouble(" 1", &n)); // no padding inside
    BOOST_CHECK(!ParseDouble("1 ", &n));
    BOOST_CHECK(!ParseDouble("1a", &n));
    BOOST_CHECK(!ParseDouble("aap", &n));
    BOOST_CHECK(!ParseDouble("0x1", &n)); // no hex
    const char test_bytes[] = {'1', 0, '1'};
    std::string teststr(test_bytes, sizeof(test_bytes));
    BOOST_CHECK(!ParseDouble(teststr, &n)); // no embedded NULs
    // Overflow and underflow
    BOOST_CHECK(!ParseDouble("-1e10000", nullptr));
    BOOST_CHECK(!ParseDouble("1e10000", nullptr));
}

BOOST_AUTO_TEST_CASE(test_FormatParagraph)
{
    BOOST_CHECK_EQUAL(FormatParagraph("", 79, 0), "");
    BOOST_CHECK_EQUAL(FormatParagraph("test", 79, 0), "test");
    BOOST_CHECK_EQUAL(FormatParagraph(" test", 79, 0), " test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 79, 0), "test test");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 0), "test\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("testerde test", 4, 0), "testerde\ntest");
    BOOST_CHECK_EQUAL(FormatParagraph("test test", 4, 4), "test\n    test");

    // Make sure we don't indent a fully-new line following a too-long line ending
    BOOST_CHECK_EQUAL(FormatParagraph("test test\nabc", 4, 4), "test\n    test\nabc");

    BOOST_CHECK_EQUAL(FormatParagraph("This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length until it gets here", 79), "This_is_a_very_long_test_string_without_any_spaces_so_it_should_just_get_returned_as_is_despite_the_length\nuntil it gets here");

    // Test wrap length is exact
    BOOST_CHECK_EQUAL(FormatParagraph("a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "a b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p", 79), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\nf g h i j k l m n o p");
    // Indent should be included in length of lines
    BOOST_CHECK_EQUAL(FormatParagraph("x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg h i j k", 79, 4), "x\na b c d e f g h i j k l m n o p q r s t u v w x y z 1 2 3 4 5 6 7 8 9 a b c de\n    f g h i j k l m n o p q r s t u v w x y z 0 1 2 3 4 5 6 7 8 9 a b c d e fg\n    h i j k");

    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string. This is a second sentence in the very long test string.", 79), "This is a very long test string. This is a second sentence in the very long\ntest string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third sentence in the very long test string.", 79), "This is a very long test string.\n\nThis is a second sentence in the very long test string. This is a third\nsentence in the very long test string.");
    BOOST_CHECK_EQUAL(FormatParagraph("Testing that normal newlines do not get indented.\nLike here.", 79), "Testing that normal newlines do not get indented.\nLike here.");
}

BOOST_AUTO_TEST_CASE(test_FormatSubVersion)
{
    std::vector<std::string> comments;
    comments.push_back(std::string("comment1"));
    std::vector<std::string> comments2;
    comments2.push_back(std::string("comment1"));
    comments2.push_back(SanitizeString(std::string("Comment2; .,_?@-; !\"#$%&'()*+/<=>[]\\^`{|}~"), SAFE_CHARS_UA_COMMENT)); // Semicolon is discouraged but not forbidden by BIP-0014
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, std::vector<std::string>()),std::string("/Test:0.9.99/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments),std::string("/Test:0.9.99(comment1)/"));
    BOOST_CHECK_EQUAL(FormatSubVersion("Test", 99900, comments2),std::string("/Test:0.9.99(comment1; Comment2; .,_?@-; )/"));
}

BOOST_AUTO_TEST_CASE(test_ParseFixedPoint)
{
    int64_t amount = 0;
    BOOST_CHECK(ParseFixedPoint("0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000LL);
    BOOST_CHECK(ParseFixedPoint("0.0", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 0LL);
    BOOST_CHECK(ParseFixedPoint("-0.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -10000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.10000000000000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 110000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1100000000LL);
    BOOST_CHECK(ParseFixedPoint("1.1e-1", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 11000000LL);
    BOOST_CHECK(ParseFixedPoint("1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000LL);
    BOOST_CHECK(ParseFixedPoint("-1000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -100000000000LL);
    BOOST_CHECK(ParseFixedPoint("0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("0.0000000100000000", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 1LL);
    BOOST_CHECK(ParseFixedPoint("-0.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -1LL);
    BOOST_CHECK(ParseFixedPoint("1000000000.00000001", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 100000000000000001LL);
    BOOST_CHECK(ParseFixedPoint("9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, 999999999999999999LL);
    BOOST_CHECK(ParseFixedPoint("-9999999999.99999999", 8, &amount));
    BOOST_CHECK_EQUAL(amount, -999999999999999999LL);

    BOOST_CHECK(!ParseFixedPoint("", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("a-1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-a1000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-1000a", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-01000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("00.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint(".1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("--0.1", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-0.000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("0.00000001000000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000000", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000001", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("10000000000.00000009", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-99999999999.99999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("99999909999.09999999", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775807", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775808", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("-92233720368.54775809", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.1e-", 8, &amount));
    BOOST_CHECK(!ParseFixedPoint("1.", 8, &amount));
}

[[maybe_unused]] static void TestOtherThread(fs::path dirname, fs::path lockname, bool *result)
{
    *result = LockDirectory(dirname, lockname) == LockResult::Success;
}

#ifndef WIN32 // Cannot do this test on WIN32 due to lack of fork()
static constexpr char LockCommand = 'L';
static constexpr char UnlockCommand = 'U';
static constexpr char ExitCommand = 'X';
enum : char {
    ResSuccess = 2, // Start with 2 to avoid accidental collision with common values 0 and 1
    ResErrorWrite,
    ResErrorLock,
    ResUnlockSuccess,
};

[[noreturn]] static void TestOtherProcess(fs::path dirname, fs::path lockname, int fd)
{
    char ch;
    while (true) {
        int rv = read(fd, &ch, 1); // Wait for command
        if (rv != 1) {
            fprintf(stderr, "Child process: Failed to read command, rv = %d\n", rv);
            exit(1); // Exit with error
        }
        switch(ch) {
        case LockCommand:
            ch = [&] { // Lambda to capture LockDirectory result
                LockResult lock_result = LockDirectory(dirname, lockname);
                switch (lock_result) {
                    case LockResult::Success: return ResSuccess;
                    case LockResult::ErrorWrite: return ResErrorWrite;
                    case LockResult::ErrorLock: return ResErrorLock;
                } // No default case, so the compiler can warn about missing cases
                fprintf(stderr, "Child process: Unexpected LockDirectory result: %d\n", (int)lock_result);
                exit(2); // Exit with error for unexpected LockResult
            }();
            rv = write(fd, &ch, 1);
            if (rv != 1) {
                fprintf(stderr, "Child process: Failed to write lock result, rv = %d\n", rv);
                exit(3); // Exit with error
            }
            break;
        case UnlockCommand:
            ReleaseDirectoryLocks();
            ch = ResUnlockSuccess; // Always succeeds
            rv = write(fd, &ch, 1);
            if (rv != 1) {
                fprintf(stderr, "Child process: Failed to write unlock result, rv = %d\n", rv);
                exit(4); // Exit with error
            }
            break;
        case ExitCommand:
            close(fd);
            exit(0);
        default:
            fprintf(stderr, "Child process: Unknown command received: %d\n", (int)ch);
            exit(5); // Exit with error for unknown command
        }
    }
}
#endif

/* temporarily disabling test as it fails in cmake CI
BOOST_AUTO_TEST_CASE(test_LockDirectory)
{
    fs::path dirname = GetDataDir() / "lock_dir";
    const fs::path lockname = ".lock";
#ifndef WIN32
    // Fork another process for testing before creating the lock, so that we
    // won't fork while holding the lock (which might be undefined, and is not
    // relevant as test case as that is avoided with -daemonize).
    int fd[2];
    BOOST_CHECK_EQUAL(socketpair(AF_UNIX, SOCK_STREAM, 0, fd), 0);
    pid_t pid = fork();
    if (!pid) {
        BOOST_CHECK_EQUAL(close(fd[1]), 0); // Child: close parent end
        TestOtherProcess(dirname, lockname, fd[0]);
        //avoid seg fault in child process
        ECC_Stop();
    }
    BOOST_CHECK_EQUAL(close(fd[0]), 0); // Parent: close child end

    char ch;
    // Lock on non-existent directory should fail
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL(ch, ResErrorWrite);
#endif
    // Lock on non-existent directory should fail
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), LockResult::ErrorWrite);

    fs::create_directories(dirname);

    // Probing lock on new directory should succeed
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), LockResult::Success);

    // Persistent lock on new directory should succeed
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), LockResult::Success);

    // Another lock on the directory from the same thread should succeed
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname), LockResult::Success);

    // Another lock on the directory from a different thread within the same process should succeed
    bool threadresult;
    std::thread thr([&] { threadresult = LockDirectory(dirname, lockname) == LockResult::Success; });
    thr.join();
    BOOST_CHECK_EQUAL(threadresult, LockResult::Success);
#ifndef WIN32
    // Try to acquire lock in child process while we're holding it, this should fail.

    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL(ch, ResErrorLock);

    // Give up our lock
    ReleaseDirectoryLocks();
    // Probing lock from our side now should succeed, but not hold on to the lock.
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), LockResult::Success);

    // Try to acquire the lock in the child process, this should be successful.
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL(ch, ResSuccess);

    // When we try to probe the lock now, it should fail.
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), LockResult::ErrorLock);

    // Unlock the lock in the child process
    BOOST_CHECK_EQUAL(write(fd[1], &UnlockCommand, 1), 1);
    BOOST_CHECK_EQUAL(read(fd[1], &ch, 1), 1);
    BOOST_CHECK_EQUAL(ch, ResUnlockSuccess);

    // When we try to probe the lock now, it should succeed.
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), LockResult::Success);

    // Re-lock the lock in the child process, then wait for it to exit, check
    // successful return. After that, we check that exiting the process
    // has released the lock as we would expect by probing it.
    int processstatus;
    BOOST_CHECK_EQUAL(write(fd[1], &LockCommand, 1), 1);
    BOOST_CHECK_EQUAL(write(fd[1], &ExitCommand, 1), 1);
    BOOST_CHECK_EQUAL(waitpid(pid, &processstatus, 0), pid);
    BOOST_CHECK_EQUAL(processstatus, 0);
    BOOST_CHECK_EQUAL(LockDirectory(dirname, lockname, true), LockResult::Success);

    BOOST_CHECK_EQUAL(close(fd[1]), 0); // Close our side of the socketpair
#endif
    // Clean up
    ReleaseDirectoryLocks();
    fs::remove_all(dirname);
}
*/

BOOST_AUTO_TEST_SUITE_END()
