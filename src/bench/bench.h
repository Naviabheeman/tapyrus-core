// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_BENCH_BENCH_H
#define BITCOIN_BENCH_BENCH_H

#include <fs.h>

#include <functional>
#include <limits>
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>

// Simple micro-benchmarking framework; API mostly matches a subset of the Google Benchmark
// framework (see https://github.com/google/benchmark)
// Why not use the Google Benchmark framework? Because adding Yet Another Dependency
// (that uses cmake as its build system and has lots of features we don't need) isn't
// worth it.

/*
 * Usage:

static void CODE_TO_TIME(benchmark::State& state)
{
    ... do any setup needed...
    while (state.KeepRunning()) {
       ... do stuff you want to time...
    }
    ... do any cleanup needed...
}

// default to running benchmark for 5000 iterations
BENCHMARK(CODE_TO_TIME, 5000);

 */

namespace benchmark {
// In case high_resolution_clock is steady, prefer that, otherwise use steady_clock.
struct best_clock {
    using hi_res_clock = std::chrono::high_resolution_clock;
    using steady_clock = std::chrono::steady_clock;
    using type = std::conditional<hi_res_clock::is_steady, hi_res_clock, steady_clock>::type;
};
using clock = best_clock::type;
using time_point = clock::time_point;
using duration = clock::duration;

class Printer;

class State
{
public:
    std::string m_name;
    uint64_t m_num_iters_left;
    const uint64_t m_num_iters;
    const uint64_t m_num_evals;
    std::vector<double> m_elapsed_results;
    time_point m_start_time;

    bool UpdateTimer(time_point finish_time);

    State(std::string name, uint64_t num_evals, double num_iters, Printer& printer) : m_name(name), m_num_iters_left(0), m_num_iters(num_iters), m_num_evals(num_evals)
    {
    }

    inline bool KeepRunning()
    {
        if (m_num_iters_left--) {
            return true;
        }

        bool result = UpdateTimer(clock::now());
        // measure again so runtime of UpdateTimer is not included
        m_start_time = clock::now();
        return result;
    }
};

typedef std::function<void(State&)> BenchFunction;

class BenchRunner
{
    struct Bench {
        BenchFunction func;
        uint64_t num_iters_for_one_second;
    };
    typedef std::map<std::string, Bench> BenchmarkMap;
    static BenchmarkMap& benchmarks();

public:
    BenchRunner(std::string name, BenchFunction func, uint64_t num_iters_for_one_second);

    static void RunAll(Printer& printer, uint64_t num_evals, double scaling, const std::string& filter, bool is_list_only);
};

// interface to output benchmark results.
class Printer
{
public:
    virtual ~Printer() {}
    virtual void header() = 0;
    virtual void result(const State& state) = 0;
    virtual void footer() = 0;
};

// default printer to console, shows min, max, median.
class ConsolePrinter : public Printer
{
public:
    void header() override;
    void result(const State& state) override;
    void footer() override;
};

// creates box plot with plotly.js
class PlotlyPrinter : public Printer
{
public:
    PlotlyPrinter(std::string plotly_url, int64_t width, int64_t height);
    void header() override;
    void result(const State& state) override;
    void footer() override;

private:
    std::string m_plotly_url;
    int64_t m_width;
    int64_t m_height;
};
} //namespace benchmark


// BENCHMARK(foo, num_iters_for_one_second) expands to:  benchmark::BenchRunner bench_11foo("foo", num_iterations);
// Choose a num_iters_for_one_second that takes roughly 1 second. The goal is that all benchmarks should take approximately
// the same time, and scaling factor can be used that the total time is appropriate for your system.
#define BENCHMARK(n, num_iters_for_one_second) \
    benchmark::BenchRunner BOOST_PP_CAT(bench_, BOOST_PP_CAT(__LINE__, n))(BOOST_PP_STRINGIZE(n), n, (num_iters_for_one_second));


const std::string GENESIS_BLOCK("0100000000000000000000000000000000000000000000000000000000000000000000002b5331139c6bc8646bb4e5737c51378133f70b9712b75548cb3c05f9188670e7440d295e7300c5640730c4634402a3e66fb5d921f76b48d8972a484cc0361e660f288661012103af80b90d25145da28c583359beb47b21796b2fe1a23c1511e443e7a64dfdb27d40214f99266b9f569fbff5fdd9fff78bbdf258fafd79f5df2578030914f58913a6ecaf0a1564223a3366be20da378aa3555bdc961b1a09ae966f21d3c0c8eaddc201010000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0100f2052a010000001976a91445d405b9ed450fec89044f9b7a99a4ef6fe2cd3f88ac00000000");

const std::string SIGN_BLOCK_PUBKEY("03af80b90d25145da28c583359beb47b21796b2fe1a23c1511e443e7a64dfdb27d");

const std::string SIGN_BLOCK_PRIVKEY("cUJN5RVzYWFoeY8rUztd47jzXCu1p57Ay8V7pqCzsBD3PEXN7Dd4");

void writeTestGenesisBlockToFile(fs::path genesisPath);

#endif // BITCOIN_BENCH_BENCH_H
