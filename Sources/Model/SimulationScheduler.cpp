//
// Created by ludfr on 07/03/2026.
//

#include <utility>

#include "../../Includes/Model/SimulationScheduler.h"

#include <csignal>
#include <fstream>
#include <unistd.h>

#include "../../Includes/Model/Simulation.h"


SimulationScheduler::SimulationScheduler() {
    simulationRepository.load();
}

void SimulationScheduler::add(std::string sandPandaArgs, std::string id, const int threads_number) {
    simulationRepository.add(Simulation(std::move(sandPandaArgs), std::move(id) , threads_number));
    simulationRepository.save();
    startSimulation();
}

std::vector<pid_t> getSandPandaPids()
{
    std::vector<pid_t> pids;

    // Utilise pgrep pour trouver tous les PIDs correspondant à "SandPanda"
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("pgrep -x SandPanda", "r"),
        pclose
    );

    if (!pipe)
        return pids;

    char buffer[32];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr)
    {
        try {
            pid_t pid = static_cast<pid_t>(std::stol(buffer));
            pids.push_back(pid);
        } catch (...) {
            // Ignore les lignes non parsables
        }
    }

    return pids;
}

void SimulationScheduler::startSimulation() {
    while (simulationRepository.isPending() && simulationRepository.get_number_threads_free() >= simulationRepository.get_number_threads_for_next()) {
        char command[1024];
        auto simulation = simulationRepository.getNext();
        auto sandPandaArgs = simulation.get_sandPandaArgs();

        sprintf(command, "export OMP_NUM_THREADS=%d ; /home/ludfr/.local/bin/SandPanda %s &", simulation.get_threads_number(), sandPandaArgs.data());
        printf("%s\n", command);
        system(command);

        auto pids = getSandPandaPids();
        bool isNewPID = false;
        for (auto& pid : pids) {
            isNewPID = true;
            for (auto& simu : simulationRepository.getRunning()) {
                if (simu.get_pid() == pid) {
                    isNewPID = false;
                    break;
                }
            }
            if (isNewPID) {
                simulation.setId(pid);
                break;
            }
        }
        simulationRepository.isRunning(simulation);
        simulationRepository.save();
    }
}

bool isRunning(std::string id)
{
    char command[1024];
    bool isAlive = true;
    sprintf(command, "pgrep -f \"%s\"  >> /home/ludfr/.config/SandPandaScheduler/alive.txt", id.data());
    system(command);

    if (std::filesystem::file_size("/home/ludfr/.config/SandPandaScheduler/alive.txt") < 16)
        isAlive = false;

    sprintf(command, "rm /home/ludfr/.config/SandPandaScheduler/alive.txt");
    system(command);
    return isAlive;
}

bool isProcessRunning(pid_t pid)
{
    // kill avec signal 0 ne tue pas le processus :
    // - retourne 0  si le processus existe et est accessible
    // - retourne -1 si le processus n'existe pas (errno == ESRCH)
    //               ou si accès refusé       (errno == EPERM) → il tourne quand même
    if (kill(pid, 0) == 0)
        return true;

    return errno == EPERM; // Processus existant mais appartenant à un autre utilisateur
}

void SimulationScheduler::scheduleSimulations() {
    bool isSimulationCompleted = false;
    for (const auto & running = simulationRepository.getRunning(); auto & simulation : running) {
        if (!isProcessRunning(simulation.get_pid())) {
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
