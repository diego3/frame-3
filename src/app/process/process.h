// Process (Game Coding Complete Ch. 4): cooperative, multi-frame behavior that isn't naturally
// owned by a single object's own state (e.g. a camera shake with no single "camera actor" to hang
// a timer field off of). A Process runs across frames via Update(dt) until it reports its own
// completion.
#ifndef PROCESS_H
#define PROCESS_H

class Process {
public:
    virtual ~Process() = default;

    virtual void Update(float dt) = 0;

    bool IsDead() const { return state_ != State::Running; }

protected:
    void Succeed() { state_ = State::Succeeded; }
    void Fail() { state_ = State::Failed; }

private:
    enum class State { Running, Succeeded, Failed };
    State state_ = State::Running;
};

#endif // PROCESS_H
