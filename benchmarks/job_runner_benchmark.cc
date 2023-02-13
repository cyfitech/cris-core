#include "cris/core/sched/job_runner.h"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <vector>

namespace cris::core {

static void BM_AddJob(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({.thread_num_ = 2});
    for ([[maybe_unused]] const auto s : state) {
        job_runner->AddJob([]() {});
    }
    job_runner->Stop();
}

static void BM_AddJobBatch(benchmark::State& state) {
    auto              job_runner = JobRunner::MakeJobRunner({.thread_num_ = 2});
    const std::size_t kBatchSize = static_cast<std::size_t>(state.range(0));
    for ([[maybe_unused]] const auto s : state) {
        std::vector<JobRunner::job_t> job_batch;
        job_batch.reserve(kBatchSize);

        for (std::size_t i = 0; i < kBatchSize; ++i) {
            job_batch.push_back([]() {});
        }

        job_runner->AddJobs(std::move(job_batch));
    }
    job_runner->Stop();
}

static void BM_AddJobWithStrand(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({.thread_num_ = 2});
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] const auto s : state) {
        job_runner->AddJob([]() {}, strand);
    }
    job_runner->Stop();
}

static void BM_AddJobTryImmediately(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({.thread_num_ = 2});
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] const auto s : state) {
        job_runner->AddJob([]() {}, strand, JobRunner::TryRunImmediately());
    }
    job_runner->Stop();
}

BENCHMARK(BM_AddJob)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobBatch)->ThreadRange(1, 4)->Arg(50)->Arg(100)->Arg(1000)->Arg(2000);
BENCHMARK(BM_AddJobWithStrand)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobTryImmediately);

}  // namespace cris::core
