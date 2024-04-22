// perf analysis commands
// perf record --call-graph dwarf bench
// perf report -g

#include <benchmark/benchmark.h>
#include <cstdint>
#include <digest/data_structure.hpp>
#include <digest/mod_minimizer.hpp>
#include <digest/syncmer.hpp>
#include <digest/thread_out.hpp>
#include <digest/window_minimizer.hpp>
#include <fstream>
#include <nthash/nthash.hpp>

#define DEFAULT_LARGE_WIND 16
#define DEFAULT_KMER_LEN 16
#define DEFAULT_KMER_LEN2 64
#define STR_LEN 62460029

std::string bench_str;
std::string s;
std::string s1;
std::string s2;

void setupStrings() {
	std::string files[] = {
		"../tests/bench/chrY.txt",
	};

	for (auto &file : files) {
		std::ifstream ifs(file);
		ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		std::string str;
		ifs >> bench_str;
		s = bench_str;
	}
}

// roll_minimizers grouping
// --------------------------------------------------------------

static void BM_NtHashRoll(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
		nthash::NtHash dig(s, 1, state.range(0));
		state.ResumeTiming();
		while (dig.roll())
			benchmark::DoNotOptimize(*dig.hashes());
	}
}
BENCHMARK(BM_NtHashRoll)
	->Args({4})	 // spumoni2
	->Args({15}) // minimap
	->Args({31})
	->Iterations(16); // kraken v1

static void BM_ModMinRoll(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
		digest::ModMin<digest::BadCharPolicy::SKIPOVER> dig(s, state.range(0),
															17);
		std::vector<uint32_t> vec;
		vec.reserve(STR_LEN);
		state.ResumeTiming();

		benchmark::DoNotOptimize(vec);
		dig.roll_minimizer(STR_LEN, vec);
		benchmark::ClobberMemory();
	}
}
BENCHMARK(BM_ModMinRoll)
	->Args({4})	 // spumoni2
	->Args({15}) // minimap
	->Args({31}) // kraken v1
	->Args({16})
	->Iterations(16); // comparison for threads

static void BM_WindowMinRoll(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
#define WINDOW(k)                                                              \
	digest::WindowMin<digest::BadCharPolicy::SKIPOVER,                         \
					  digest::ds::SegmentTree<k>>                              \
		dig(s, state.range(0), k);                                             \
	std::vector<uint32_t> vec;                                                 \
	vec.reserve(STR_LEN);                                                      \
	state.ResumeTiming();                                                      \
	benchmark::DoNotOptimize(vec);                                             \
	dig.roll_minimizer(STR_LEN, vec);                                          \
	benchmark::ClobberMemory();

		if (state.range(1) == 11) {
			WINDOW(11)
		} else if (state.range(1) == 10) {
			WINDOW(10)
		} else if (state.range(1) == 15) {
			WINDOW(15)
		} else if (state.range(1) == 16) {
			WINDOW(16)
		}
	}
}
BENCHMARK(BM_WindowMinRoll)
	->Args({4, 11})	 // spumoni2
	->Args({15, 10}) // minimap
	->Args({31, 15}) // kraken v1
	->Args({16, 16})
	->Iterations(16); // comparison for threads

static void BM_SyncmerRoll(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
#define SYNCMER(k)                                                             \
	digest::Syncmer<digest::BadCharPolicy::SKIPOVER,                           \
					digest::ds::SegmentTree<k>>                                \
		dig(s, state.range(0), k);                                             \
	std::vector<uint32_t> vec;                                                 \
	vec.reserve(STR_LEN);                                                      \
	state.ResumeTiming();                                                      \
	benchmark::DoNotOptimize(vec);                                             \
	dig.roll_minimizer(STR_LEN, vec);                                          \
	benchmark::ClobberMemory();

		if (state.range(1) == 12) {
			SYNCMER(12)
		} else if (state.range(1) == 11) {
			SYNCMER(11)
		} else if (state.range(1) == 16) {
			SYNCMER(16)
		}
	}
}
BENCHMARK(BM_SyncmerRoll)
	->Args({4, 12})	 // spumoni2
	->Args({15, 11}) // minimap
	->Args({31, 16}) // kraken v1
	->Args({16, 16})
	->Iterations(16); // comparison for threads

// thread benchmarking
// ---------------------------------------------------------------------
static void BM_ThreadMod(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
		std::vector<std::vector<uint32_t>> vec;
		state.ResumeTiming();

		benchmark::DoNotOptimize(vec);
		thread_out::thread_mod<digest::BadCharPolicy::SKIPOVER>(
			state.range(0), vec, s, DEFAULT_KMER_LEN, 17);
		benchmark::ClobberMemory();
	}
}
BENCHMARK(BM_ThreadMod)
	->Args({1})
	->ArgsProduct({benchmark::CreateDenseRange(2, 64, 2)})
	->UseRealTime()
	->Iterations(16);

static void BM_ThreadWind(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
		std::vector<std::vector<uint32_t>> vec;
		state.ResumeTiming();

		benchmark::DoNotOptimize(vec);
		thread_out::thread_wind<digest::BadCharPolicy::SKIPOVER,
								digest::ds::SegmentTree<DEFAULT_LARGE_WIND>>(
			state.range(0), vec, s, DEFAULT_KMER_LEN, DEFAULT_LARGE_WIND);
		benchmark::ClobberMemory();
	}
}
BENCHMARK(BM_ThreadWind)
	->Args({1})
	->ArgsProduct({benchmark::CreateDenseRange(2, 64, 2)})
	->UseRealTime()
	->Iterations(16);

static void BM_ThreadSync(benchmark::State &state) {
	for (auto _ : state) {
		state.PauseTiming();
		std::vector<std::vector<uint32_t>> vec;
		state.ResumeTiming();

		benchmark::DoNotOptimize(vec);
		thread_out::thread_sync<digest::BadCharPolicy::SKIPOVER,
								digest::ds::SegmentTree<DEFAULT_LARGE_WIND>>(
			state.range(0), vec, s, DEFAULT_KMER_LEN, DEFAULT_LARGE_WIND);
		benchmark::ClobberMemory();
	}
}
BENCHMARK(BM_ThreadSync)
	->Args({1})
	->ArgsProduct({benchmark::CreateDenseRange(2, 64, 2)})
	->UseRealTime()
	->Iterations(16);

// constructor sanity check grouping
// -----------------------------------------------------
/*
static void BM_NtHashConstruction(benchmark::State& state){
		for(auto _ : state) {
				nthash::NtHash dig(s, 1, DEFAULT_KMER_LEN);
				benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
		}
  state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_NtHashConstruction)->Range(1<<6,
1<<18)->Setup(random)->Complexity();

static void BM_ModMinConstruction(benchmark::State& state){
		for(auto _ : state) {
				digest::ModMin dig(s, DEFAULT_KMER_LEN, 17);
				benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
		}
		state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_ModMinConstruction)->Range(1<<6,
1<<18)->Setup(random)->Complexity();

static void BM_WindowMinConstructionFixWind(benchmark::State& state){
	for(auto _ : state){
		digest::WindowMin dig(s, DEFAULT_KMER_LEN, DEFAULT_LARGE_WIND);
		benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
	}
		state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_WindowMinConstructionFixWind)->Range(1<<6,
1<<18)->Setup(random)->Complexity();

static void BM_WindowMinConstructionFixLen(benchmark::State& state){
	for(auto _ : state){
		digest::WindowMin dig(s, DEFAULT_KMER_LEN, state.range(0));
		benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
	}
		state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_WindowMinConstructionFixLen)->Range(1<<6,
1<<18)->Setup(random)->Complexity();

static void BM_SyncmerConstructionFixWind(benchmark::State& state){
	for(auto _ : state){
		digest::Syncmer dig(s, DEFAULT_KMER_LEN, DEFAULT_LARGE_WIND);
		benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
	}
		state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SyncmerConstructionFixWind)->Range(1<<6,
1<<18)->Setup(random)->Complexity();

static void BM_SyncmerConstructionFixLen(benchmark::State& state){
	for(auto _ : state){
		digest::Syncmer dig(s, DEFAULT_KMER_LEN, state.range(0));
		benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
	}
		state.SetComplexityN(state.range(0));
}
BENCHMARK(BM_SyncmerConstructionFixLen)->Range(1<<6,
1<<18)->Setup(random)->Complexity();
*/

// append_seq sanity check grouping
// ---------------------------------------------------------------
/*
static void random_append_seq(const benchmark::State& state){
		s1 = bench_strs[0].substr(0, state.range(0));
		s2 = s = bench_strs[0].substr(state.range(0), state.range(1));
}

static void BM_ModMinConstruction(benchmark::State& state){
		for(auto _ : state) {
				digest::ModMin dig(s, DEFAULT_KMER_LEN2, 17);
				benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
		}
}

static void BM_ModMinRoll(benchmark::State& state) {
		for(auto _ : state) {
				digest::ModMin dig(s, DEFAULT_KMER_LEN2, 17);
				std::vector<size_t> vec;
				vec.reserve(state.range(0));

				benchmark::DoNotOptimize(vec);
				dig.roll_minimizer(state.range(0), vec);
				benchmark::ClobberMemory();
		}
}

static void BM_append_seq(benchmark::State& state){

		for(auto _ : state) {
				digest::ModMin dig(s1, DEFAULT_KMER_LEN2, 17);
				dig.append_seq(s2);

				benchmark::DoNotOptimize(dig);
				benchmark::ClobberMemory();
		}
}

static void BM_append_seq_roll(benchmark::State& state){

		for(auto _ : state) {
				digest::ModMin dig(s1, DEFAULT_KMER_LEN2, 17);
				dig.append_seq(s2);
				std::vector<size_t> vec;
				vec.reserve(state.range(0) + state.range(1));

				benchmark::DoNotOptimize(vec);
				dig.roll_minimizer(state.range(0), vec);
				benchmark::ClobberMemory();
		}
}
BENCHMARK(BM_ModMinConstruction)->Arg(127)->Setup(random);
BENCHMARK(BM_append_seq)->Args({63, 64})->Setup(random_append_seq);
BENCHMARK(BM_ModMinRoll)->Arg(127)->Arg(263)->Arg(563)->Setup(random);
BENCHMARK(BM_append_seq_roll)->Args({63, 64})->Args({63, 200})->Args({63,
500})->Setup(random_append_seq);
*/

int main(int argc, char **argv) {
	setupStrings();
	benchmark::Initialize(&argc, argv);
	benchmark::RunSpecifiedBenchmarks();
}
