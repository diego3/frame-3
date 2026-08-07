#include "doctest/doctest.h"

#include <memory>

#include "app/process/process_manager.h"

namespace {
    struct FakeProcessState {
        int updateCount = 0;
        float lastDt = 0.0f;
    };

    // A fake Process whose observable state lives in a shared FakeProcessState, not in the
    // Process object itself: ProcessManager::Update destroys dead processes as part of the same
    // call that runs them, so a test can't safely read through a pointer into the process after
    // an Update that might have just killed it. Reading through the shared state sidesteps that.
    class FakeProcess : public Process {
    public:
        static constexpr int kNeverDies = -1;

        FakeProcess(int framesToLive, std::shared_ptr<FakeProcessState> state)
            : framesToLive_(framesToLive), state_(std::move(state)) {}

        void Update(float dt) override {
            state_->updateCount++;
            state_->lastDt = dt;

            if (framesToLive_ == kNeverDies) return;
            if (--framesToLive_ <= 0) Succeed();
        }

    private:
        int framesToLive_;
        std::shared_ptr<FakeProcessState> state_;
    };

    class FailingProcess : public Process {
    public:
        void Update(float) override { Fail(); }
    };
}

TEST_CASE("Update advances every attached process by dt") {
    ProcessManager processes;
    auto state = std::make_shared<FakeProcessState>();
    processes.Attach(std::make_unique<FakeProcess>(FakeProcess::kNeverDies, state));

    processes.Update(0.5f);

    CHECK(state->updateCount == 1);
    CHECK(state->lastDt == doctest::Approx(0.5f));
}

TEST_CASE("A process that reports completion is dropped on the next Update") {
    ProcessManager processes;
    auto state = std::make_shared<FakeProcessState>();
    processes.Attach(std::make_unique<FakeProcess>(1, state));

    processes.Update(0.016f);  // process dies here (framesToLive hits 0 -> Succeed())
    processes.Update(0.016f);  // would bump updateCount to 2 if it were still attached

    CHECK(state->updateCount == 1);
}

TEST_CASE("A failed process is dropped just like a succeeded one") {
    ProcessManager processes;
    processes.Attach(std::make_unique<FailingProcess>());

    CHECK_NOTHROW(processes.Update(0.016f));
    CHECK_NOTHROW(processes.Update(0.016f));  // would touch a dangling process if not dropped
}

TEST_CASE("Multiple attached processes are all advanced independently") {
    ProcessManager processes;
    auto stateA = std::make_shared<FakeProcessState>();
    auto stateB = std::make_shared<FakeProcessState>();
    processes.Attach(std::make_unique<FakeProcess>(FakeProcess::kNeverDies, stateA));
    processes.Attach(std::make_unique<FakeProcess>(FakeProcess::kNeverDies, stateB));

    processes.Update(0.1f);
    processes.Update(0.1f);

    CHECK(stateA->updateCount == 2);
    CHECK(stateB->updateCount == 2);
}
