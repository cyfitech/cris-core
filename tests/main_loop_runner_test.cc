#include "cris/core/node_runner/main_loop_runner.h"

#include "cris/core/node/multi_queue_node.h"

#include "gtest/gtest.h"

#include <cstddef>
#include <vector>

using namespace cris::core;

static constexpr std::size_t kMainThreadNum = 4;

class NodeWithMainLoopForTest : public CRMultiQueueNode<> {
   public:
    NodeWithMainLoopForTest() : CRMultiQueueNode<>(0), main_loop_is_run_(kMainThreadNum, 0) {}

    void MainLoop(const std::size_t thread_idx, const std::size_t /* thread_num */) {
        main_loop_is_run_[thread_idx] = true;
    }

    bool IsMainLoopRun(std::size_t thread_idx) const { return main_loop_is_run_[thread_idx]; }

   private:
    std::vector<bool> main_loop_is_run_;
};

TEST(MainLoopRunnerTest, MainLoopRunnerTest) {
    NodeWithMainLoopForTest node;
    CRNodeMainLoopRunner    node_runner(&node, kMainThreadNum);

    node_runner.Run();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    node_runner.Stop();
    node_runner.Join();

    for (std::size_t i = 0; i < kMainThreadNum; ++i) {
        EXPECT_TRUE(node.IsMainLoopRun(i));
    }
}
