#pragma once

#include "cris/core/sched/job_runner.h"

#include <benchmark/benchmark.h>

namespace cris::core {

static auto GetGlobalRunner() {
    static auto job_runner = JobRunner::MakeJobRunner({
        .thread_num_ = 2,
    });
    return job_runner;
}

static void BM_AddJob(benchmark::State& state) {
    auto job_runner = GetGlobalRunner();
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {});
    }
}

static void BM_AddJobWithStrand(benchmark::State& state) {
    auto job_runner = GetGlobalRunner();
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {}, strand);
    }
}

static void BM_AddJobTryImmediately(benchmark::State& state) {
    auto job_runner = GetGlobalRunner();
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {}, strand, JobRunner::TryRunImmediately());
    }
}

BENCHMARK(BM_AddJob)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobWithStrand)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobTryImmediately);

}  // namespace cris::core
