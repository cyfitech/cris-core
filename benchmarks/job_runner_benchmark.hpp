#pragma once

#include "cris/core/sched/job_runner.h"

#include <benchmark/benchmark.h>

namespace cris::core {

static void BM_AddJob(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({
        .thread_num_ = 2,
    });
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {});
    }
    job_runner->Stop();
}

static void BM_AddJobWithStrand(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({
        .thread_num_ = 2,
    });
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {}, strand);
    }
    job_runner->Stop();
}

static void BM_AddJobTryImmediately(benchmark::State& state) {
    auto job_runner = JobRunner::MakeJobRunner({
        .thread_num_ = 2,
    });
    auto strand     = job_runner->MakeStrand();
    for ([[maybe_unused]] auto s : state) {
        job_runner->AddJob([]() {}, strand, JobRunner::TryRunImmediately());
    }
    job_runner->Stop();
}

BENCHMARK(BM_AddJob)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobWithStrand)->ThreadRange(1, 4);
BENCHMARK(BM_AddJobTryImmediately);

}  // namespace cris::core
