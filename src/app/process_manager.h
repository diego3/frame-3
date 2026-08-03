// ProcessManager (Game Coding Complete Ch. 4): owns every attached Process and drives them all
// forward one frame at a time, dropping each once it reports completion.
#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <memory>
#include <vector>

#include "process.h"

class ProcessManager {
public:
    void Attach(std::unique_ptr<Process> process);

    // Advances every attached process by dt (seconds), then removes the ones that finished.
    void Update(float dt);

private:
    std::vector<std::unique_ptr<Process>> processes_;
};

#endif // PROCESS_MANAGER_H
