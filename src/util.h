// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2021 Chaintope Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * thread wrappers, startup time
 */
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include <tapyrus-config.h>

#include <compat.h>
#include <fs.h>
#include <logging.h>
#include <sync.h>
#include <tinyformat.h>
#include <utiltime.h>
#include <utilmemory.h>
#include <tapyrusmodes.h>

#include <atomic>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/signals2/signal.hpp>

// Application startup time (used for uptime calculation)
int64_t GetStartupTime();

/** Signals for translation. */
class CTranslationInterface
{
public:
    /** Translate a message to the native language of the user. */
    boost::signals2::signal<std::string (const char* psz)> Translate;
};

extern CTranslationInterface translationInterface;

extern const char * const BITCOIN_CONF_FILENAME;
extern const char * const BITCOIN_PID_FILENAME;

/**
 * Translation function: Call Translate signal on UI interface, which returns a boost::optional result.
 * If no translation slot is registered, nothing is returned, and simply return the input.
 */
inline std::string _(const char* psz)
{
    boost::optional<std::string> rv = translationInterface.Translate(psz);
    return rv ? (*rv) : psz;
}

void SetupEnvironment();
bool SetupNetworking();

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    LogPrintf("ERROR: %s\n", tfm::format(fmt, args...));
    return false;
}

void PrintExceptionContinue(const std::exception *pex, std::string_view thread_name);
bool FileCommit(FILE *file);
bool TruncateFile(FILE *file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length);
bool RenameOver(fs::path src, fs::path dest);

enum class LockResult {
 Success,
 ErrorWrite,
 ErrorLock,
};
// Define operator<< for LockResult
std::ostream& operator<<(std::ostream& os, const LockResult& result);

LockResult LockDirectory(const fs::path& directory, const fs::path& lockfile_name, bool probe_only=false);

/** Release all directory locks. This is used for unit testing only, at runtime
 * the global destructor will take care of the locks.
 */
void ReleaseDirectoryLocks();

bool TryCreateDirectories(const fs::path& p);
fs::path GetDefaultDataDir();

/*
 * Helper function to create data directory name using network mode and network id
*/
std::string GetDataDirNameFromNetworkId(const uint32_t networkID);
const fs::path &GetBlocksDir();
const fs::path &GetDataDir(bool fNetSpecific = true);
void ClearDatadirCache();
fs::path GetConfigFile(const std::string& confPath);
#ifndef WIN32
fs::path GetPidFile();
void CreatePidFile(const fs::path &path, pid_t pid);
#endif
#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void runCommand(const std::string& strCommand);

/**
 * Most paths passed as configuration arguments are treated as relative to
 * the datadir if they are not absolute.
 *
 * @param path The path to be conditionally prefixed with datadir.
 * @param net_specific Forwarded to GetDataDir().
 * @return The normalized path.
 */
fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific = true);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

enum class OptionsCategory {
    OPTIONS,
    CONNECTION,
    WALLET,
    WALLET_DEBUG_TEST,
    ZMQ,
    DEBUG_TEST,
    CHAINPARAMS,
    NODE_RELAY,
    BLOCK_CREATION,
    RPC,
    GUI,
    COMMANDS,
    REGISTER_COMMANDS,
    GENESIS,

    HIDDEN // Always the last option to avoid printing these in the help
};

class ArgsManager
{
protected:
    friend class ArgsManagerHelper;

    struct Arg
    {
        std::string m_help_param;
        std::string m_help_text;
        bool m_debug_only;

        Arg(const std::string& help_param, const std::string& help_text, bool debug_only) : m_help_param(help_param), m_help_text(help_text), m_debug_only(debug_only) {};
    };

    mutable RecursiveMutex cs_args;
    std::map<std::string, std::vector<std::string>> m_override_args;
    std::map<std::string, std::vector<std::string>> m_config_args;
    std::string m_network;
    std::set<std::string> m_network_only_args;
    std::map<OptionsCategory, std::map<std::string, Arg>> m_available_args;

    bool ReadConfigStream(std::istream& stream, std::string& error, bool ignore_invalid_keys = false);

public:
    ArgsManager();

    /**
     * Select the network in use
     */
    void SelectConfigNetwork(const std::string& network);

    bool ParseParameters(int argc, const char* const argv[], std::string& error);
    bool ReadConfigFiles(std::string& error, bool ignore_invalid_keys = false);

    /**
     * Log warnings for options in m_section_only_args when
     * they are specified in the default section but not overridden
     * on the command line or in a network-specific section in the
     * config file.
     */
    void WarnForSectionOnlyArgs();

    /**
     * Return a vector of strings of the given argument
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return command-line arguments
     */
    std::vector<std::string> GetArgs(const std::string& strArg) const;

    /**
     * Return true if the given argument has been manually set
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return true if the argument has been set
     */
    bool IsArgSet(const std::string& strArg) const;

    /**
     * Return true if the argument was originally passed as a negated option,
     * i.e. -nofoo.
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @return true if the argument was passed negated
     */
    bool IsArgNegated(const std::string& strArg) const;

    /**
     * Return string argument or default value
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param strDefault (e.g. "1")
     * @return command-line argument or default value
     */
    std::string GetArg(const std::string& strArg, const std::string& strDefault) const;

    /**
     * Return integer argument or default value
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param nDefault (e.g. 1)
     * @return command-line argument (0 if invalid number) or default value
     */
    int64_t GetArg(const std::string& strArg, int64_t nDefault) const;

    /**
     * Return whether an integer argument is within the expected range
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param min (e.g. 1)
     * @param max (e.g. 4294967295)
     * @param nDefault (e.g. 0)
     * @param value return value assigned to this arg
     * @return true if the value is in range. false otherwise(even when not found)
     */
    bool IsGetArgInRange(const std::string& strArg, const int64_t min, const int64_t max, const int64_t nDefault, int64_t& value) const;

    /**
     * Return boolean argument or default value
     *
     * @param strArg Argument to get (e.g. "-foo")
     * @param fDefault (true or false)
     * @return command-line argument or default value
     */
    bool GetBoolArg(const std::string& strArg, bool fDefault) const;

    /**
     * Set an argument if it doesn't already have a value
     *
     * @param strArg Argument to set (e.g. "-foo")
     * @param strValue Value (e.g. "1")
     * @return true if argument gets set, false if it already had a value
     */
    bool SoftSetArg(const std::string& strArg, const std::string& strValue);

    /**
     * Set a boolean argument if it doesn't already have a value
     *
     * @param strArg Argument to set (e.g. "-foo")
     * @param fValue Value (e.g. false)
     * @return true if argument gets set, false if it already had a value
     */
    bool SoftSetBoolArg(const std::string& strArg, bool fValue);

    // Forces an arg setting. Called by SoftSetArg() if the arg hasn't already
    // been set. Also called directly in testing.
    void ForceSetArg(const std::string& strArg, const std::string& strValue);

    /**
     * Looks for -dev and returns the appropriate chain name.
     * @return TAPYRUS_OP_MODE::PROD by default; TAPYRUS_OP_MODE::DEV id -dev was given;
     */
    TAPYRUS_OP_MODE GetChainMode() const;

    /**
     * Add argument
     */
    void AddArg(const std::string& name, const std::string& help, const bool debug_only, const OptionsCategory& cat);

    /**
     * Add many hidden arguments
     */
    void AddHiddenArgs(const std::vector<std::string>& args);

    /**
     * Clear available arguments
     */
    void ClearArgs() { LOCK(cs_args); m_available_args.clear(); }

    /**
     * Get the help string
     */
    std::string GetHelpMessage() const;

    /**
     * Check whether we know of this arg
     */
    bool IsArgKnown(const std::string& key) const;

    /**
     * Clear test arguments
     */
    void ClearOverrideArgs() { LOCK(cs_args); m_override_args.clear(); }
};

extern ArgsManager gArgs;

/**
 * @return true if help has been requested via a command-line arg
 */
bool HelpRequested(const ArgsManager& args);

/**
 * Format a string to be used as group of options in help messages
 *
 * @param message Group name (e.g. "RPC server options:")
 * @return the formatted string
 */
std::string HelpMessageGroup(const std::string& message);

/**
 * Format a string to be used as option description in help messages
 *
 * @param option Option message (e.g. "-rpcuser=<user>")
 * @param message Option description (e.g. "Username for JSON-RPC connections")
 * @return the formatted string
 */
std::string HelpMessageOpt(const std::string& option, const std::string& message);

/**
 * Return the number of cores available on the current system.
 * @note This does count virtual cores, such as those provided by HyperThreading.
 */
int GetNumCores();

void RenameThread(const std::string& name);

/**
 * .. and a wrapper that just calls func once
 */
void TraceThread(std::string name,  std::function<void()> func);

/**
 * On platforms that support it, tell the kernel the calling thread is
 * CPU-intensive and non-interactive. See SCHED_BATCH in sched(7) for details.
 *
 * @return The return value of sched_setschedule(), or 1 on systems without
 * sched_setschedule().
 */
int ScheduleBatchPriority(void);

namespace util {

//! Simplification of std insertion
template <typename Tdst, typename Tsrc>
inline void insert(Tdst& dst, const Tsrc& src) {
    dst.insert(dst.begin(), src.begin(), src.end());
}
template <typename TsetT, typename Tsrc>
inline void insert(std::set<TsetT>& dst, const Tsrc& src) {
    dst.insert(src.begin(), src.end());
}

} // namespace util

#endif // BITCOIN_UTIL_H
