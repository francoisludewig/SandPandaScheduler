//
// Created by ludfr on 07/03/2026.
//

#include <utility>
#include <syslog.h>   // ← Ajout pour SysLog

#include "../../Includes/Model/SimulationScheduler.h"

#include <csignal>
#include <fstream>
#include <unistd.h>
#include <filesystem>
#include <string>

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

void SimulationScheduler::flushPending() {
    const int count = simulationRepository.clearPending();
    printf("%d simulation(s) en attente supprimee(s)\n", count);
    syslog(LOG_INFO, "Suppression des simulations en attente | nombre=%d", count);
}

// Verifie que le PID designe bien un processus SandPanda (un PID peut etre reutilise
// par un autre programme apres la fin de la simulation). Sous Linux on lit /proc/<pid>/comm ;
// si /proc n'est pas disponible, on ne peut pas verifier et on fait confiance au PID.
static bool isSandPandaProcess(const pid_t pid) {
    const std::filesystem::path comm = std::filesystem::path("/proc") / std::to_string(pid) / "comm";
    if (!std::filesystem::exists("/proc/self"))
        return true;
    std::ifstream file(comm);
    std::string name;
    if (!file || !std::getline(file, name))
        return false;
    return name == "SandPanda";
}

// Attend la fin du processus pendant au plus timeoutMs millisecondes
static bool waitForExit(const pid_t pid, const int timeoutMs) {
    for (int waited = 0; waited < timeoutMs; waited += 100) {
        if (!isProcessRunning(pid))
            return true;
        usleep(100 * 1000);
    }
    return !isProcessRunning(pid);
}

void SimulationScheduler::stopRunning() {
    // Copie : isCompleted() retire les simulations de la liste en cours
    const std::vector<Simulation> running = simulationRepository.getRunning();
    if (running.empty()) {
        printf("Aucune simulation en cours\n");
        return;
    }

    for (const auto &simulation: running) {
        const pid_t pid = simulation.get_pid();
        const std::string idString = simulation.get_id();  // get_id() renvoie une copie
        const char *id = idString.c_str();

        if (pid <= 0) {
            // PID inconnu : impossible d'arreter le processus sans risque, on le laisse
            printf("Simulation %s : PID inconnu, arret impossible\n", id);
            syslog(LOG_WARNING, "Arret impossible, PID inconnu | id=%s", id);
            continue;
        }

        if (!isProcessRunning(pid) || !isSandPandaProcess(pid)) {
            printf("Simulation %s (pid %d) deja terminee\n", id, static_cast<int>(pid));
            syslog(LOG_INFO, "Simulation deja terminee lors de l'arret | id=%s | pid=%d", id,
                   static_cast<int>(pid));
            simulationRepository.isCompleted(simulation);
            continue;
        }

        // Arret propre (SIGTERM), puis force (SIGKILL) si le processus ne s'arrete pas
        kill(pid, SIGTERM);
        bool stopped = waitForExit(pid, 10000);
        if (!stopped) {
            kill(pid, SIGKILL);
            stopped = waitForExit(pid, 5000);
        }

        if (stopped) {
            printf("Simulation %s (pid %d) arretee\n", id, static_cast<int>(pid));
            syslog(LOG_INFO, "Simulation arretee | id=%s | pid=%d", id, static_cast<int>(pid));
            simulationRepository.isCompleted(simulation);
        } else {
            printf("Simulation %s (pid %d) : echec de l'arret\n", id, static_cast<int>(pid));
            syslog(LOG_ERR, "Echec de l'arret de la simulation | id=%s | pid=%d", id, static_cast<int>(pid));
        }
    }
    simulationRepository.save();
}

void SimulationScheduler::resetPriority() {
    const int next = simulationRepository.resetPriority();
    if (next == 1)
        printf("Compteur de priorite remis a 1\n");
    else
        printf("Simulations encore en attente : le compteur de priorite repart a %d\n", next);
    syslog(LOG_INFO, "Reset du compteur de priorite | prochaine priorite=%d", next);
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