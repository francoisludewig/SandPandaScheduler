//
// Created by ludfr on 07/03/2026.
//

#include <utility>

#include "../../Includes/Model/SimulationScheduler.h"

#include <csignal>
#include <unistd.h>

#include "../../Includes/Model/Simulation.h"


SimulationScheduler::SimulationScheduler() {
    simulationRepository.load();
}

void SimulationScheduler::add(std::string sandPandaArgs, std::string id, const int threads_number) {
    simulationRepository.add(Simulation(std::move(sandPandaArgs), std::move(id) , threads_number));
    simulationRepository.save();
}

void SimulationScheduler::startSimulation() {
    std::string app{"SandPanda"};
    auto simulation = simulationRepository.getNext();
    const auto pid = fork();

    if (pid < 0)
        throw std::runtime_error("fork failed");

    if (pid == 0)
    {

        // --- Processus fils ---
        std::vector<char*> argv {app.data(), simulation.get_sandPandaArgs().data()};
        argv.push_back(nullptr);

        setenv("OMP_NUM_THREADS", std::to_string(simulation.get_threads_number()).c_str(), 1);
        execvp(argv[0], argv.data());

        // si exec échoue
        _exit(EXIT_FAILURE);
    }

    simulation.setId(pid);
    simulationRepository.isRunning(simulation);
    simulationRepository.save();
}

bool isRunning(const int pid)
{
    if (pid <= 0)
        return false;

    if (kill(pid, 0) == 0)
        return true;

    return false;
}

void SimulationScheduler::scheduleSimulations() {
    bool isSimulationCompleted = false;
    for (const auto & running = simulationRepository.getRunning(); auto & simulation : running) {
        if (!isRunning(simulation.get_pid())) {
            simulationRepository.isCompleted(simulation);
            startSimulation();
            isSimulationCompleted = true;
            break;
        }
    }
    if (isSimulationCompleted)
        scheduleSimulations();
}

void SimulationScheduler::clean() {
    simulationRepository.cleanCompleted();
}

void SimulationScheduler::update_max_number_threads(const int newValue) {
    simulationRepository.set_max_number_threads(newValue);
}
