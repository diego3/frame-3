#include "app/process/process_manager.h"

#include <algorithm>

void ProcessManager::Attach(std::unique_ptr<Process> process) {
    processes_.push_back(std::move(process));
}

void ProcessManager::Update(float dt) {
    for (auto &process : processes_) process->Update(dt);

    std::erase_if(processes_, [](const std::unique_ptr<Process> &process) {
        return process->IsDead();
    });
}
