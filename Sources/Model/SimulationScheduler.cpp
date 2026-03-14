//
// Created by ludfr on 07/03/2026.
//

#include <utility>
#include <syslog.h>   // ← Ajout pour SysLog

#include "../../Includes/Model/SimulationScheduler.h"

#include <csignal>
#include <fstream>
#include <unistd.h>

#include "../../Includes/Model/Simulation.h"


SimulationScheduler::SimulationScheduler() {
    // Ouverture du canal SysLog une fois à la construction
    openlog("SandPandaScheduler", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "SimulationScheduler initialisé");
    simulationRepository.load();
}

SimulationScheduler::~SimulationScheduler() {
    // Bonne pratique : fermer le canal à la destruction
    closelog();
}

void SimulationScheduler::add(std::string sandPandaArgs, std::string id, const int threads_number) {
    // ── Log : ajout d'une simulation ──────────────────────────────────────────
    syslog(LOG_INFO,
           "Ajout simulation | id=%s | threads=%d | args=%s",
           id.c_str(), threads_number, sandPandaArgs.c_str());

    simulationRepository.add(Simulation(std::move(sandPandaArgs), std::move(id), threads_number));
    simulationRepository.save();
    startSimulation();
}

std::vector<pid_t> getSandPandaPids()
{
    std::vector<pid_t> pids;

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
        } catch (...) {}
    }

    return pids;
}

void SimulationScheduler::startSimulation() {
    while (simulationRepository.isPending()
        && simulationRepository.get_number_threads_free() >= simulationRepository.get_number_threads_for_next())
    {
        char command[1024];
        auto simulation = simulationRepository.getNext();
        auto sandPandaArgs = simulation.get_sandPandaArgs();

        sprintf(command,
                "export OMP_NUM_THREADS=%d ; /home/ludfr/.local/bin/SandPanda %s &",
                simulation.get_threads_number(),
                sandPandaArgs.data());

        printf("%s\n", command);
        int result = system(command);

        // Récupération du nouveau PID
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

                // ── Log : démarrage effectif avec PID résolu ──────────────────
                syslog(LOG_INFO,
                       "Démarrage simulation | id=%s | pid=%d | threads=%d | args=%s",
                       simulation.get_id().c_str(),
                       static_cast<int>(pid),
                       simulation.get_threads_number(),
                       sandPandaArgs.c_str());
                break;
            }
        }

        if (!isNewPID) {
            // ── Log : avertissement si le PID n'a pas pu être résolu ──────────
            syslog(LOG_WARNING,
                   "Simulation démarrée mais PID introuvable | id=%s | args=%s",
                   simulation.get_id().c_str(),
                   sandPandaArgs.c_str());
        }

        simulationRepository.isRunning(simulation);
        simulationRepository.save();
    }
}

bool isRunning(std::string id)
{
    char command[1024];
    int result;
    bool isAlive = true;
    sprintf(command,
            "pgrep -f \"%s\" >> /home/ludfr/.config/SandPandaScheduler/alive.txt",
            id.data());
    result = system(command);

    if (std::filesystem::file_size("/home/ludfr/.config/SandPandaScheduler/alive.txt") < 16)
        isAlive = false;

    sprintf(command, "rm /home/ludfr/.config/SandPandaScheduler/alive.txt");
    result = system(command);
    return isAlive;
}

bool isProcessRunning(pid_t pid) {
    if (kill(pid, 0) == 0)
        return true;
    return errno == EPERM;
}

void SimulationScheduler::scheduleSimulations() {
    bool isSimulationCompleted = false;

    for (const auto& running = simulationRepository.getRunning(); auto& simulation : running) {
        if (!isProcessRunning(simulation.get_pid())) {

            // ── Log : détection de fin de simulation ──────────────────────────
            syslog(LOG_INFO,
                   "Fin simulation détectée | id=%s | pid=%d",
                   simulation.get_id().c_str(),
                   static_cast<int>(simulation.get_pid()));

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
    syslog(LOG_INFO,
           "Modification du nombre de thread manager par le scheduler : nouvelle valeur = %d",
           newValue);
}