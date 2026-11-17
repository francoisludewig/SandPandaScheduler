//
// Created by ludfr on 07/03/2026.
//

#include <algorithm>
#include <utility>
#include <syslog.h>   // ← Ajout pour SysLog

#include "../../Includes/Model/SimulationScheduler.h"

#include <cerrno>
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

// Un processus termine mais pas encore recupere par son parent (zombie, etat 'Z') repond encore
// a kill(pid, 0) : on lit son etat dans /proc/<pid>/stat (Linux) pour ne pas le croire actif.
bool isProcessRunning(pid_t pid) {
    if (pid <= 0)
        return false;   // kill(-1, 0) viserait tous les processus et reussirait toujours
    if (kill(pid, 0) != 0 && errno != EPERM)
        return false;
    std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
    std::string line;
    if (stat && std::getline(stat, line)) {
        const auto close = line.rfind(')');   // le nom du programme peut contenir des espaces
        if (close != std::string::npos && close + 2 < line.size() && line[close + 2] == 'Z')
            return false;
    }
    return true;
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
            if (isProcessRunning(pid))   // ignore les processus termines (zombies)
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

        printf("Demarrage de %s (%d thread(s)) : %s\n", simulation.get_id().c_str(),
               simulation.get_threads_number(), command);
        // PID des SandPanda deja presents (suivis ou non, p. ex. lances a la main) : le nouveau
        // PID est celui qui apparait apres le lancement
        const auto pidsBefore = getSandPandaPids();
        int result = system(command);

        // Récupération du nouveau PID (le processus peut mettre un instant a apparaitre :
        // on reessaie pendant 2 s au maximum)
        bool isNewPID = false;
        for (int attempt = 0; attempt < 20 && !isNewPID; attempt++) {
            if (attempt > 0)
                usleep(100 * 1000);
            auto pids = getSandPandaPids();
            for (auto& pid : pids) {
                isNewPID = std::find(pidsBefore.begin(), pidsBefore.end(), pid) == pidsBefore.end();
                for (auto& simu : simulationRepository.getRunning()) {
                    if (simu.get_pid() == pid) {
                        isNewPID = false;
                        break;
                    }
                }
                if (isNewPID) {
                    simulation.setId(pid);
                    printf("  -> %s demarree, pid %d\n", simulation.get_id().c_str(), static_cast<int>(pid));

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
        }

        if (!isNewPID) {
            printf("  /!\\ ALERTE : PID introuvable pour %s, simulation placee en erreur\n",
                   simulation.get_id().c_str());
            // ── Log : avertissement si le PID n'a pas pu être résolu ──────────
            syslog(LOG_WARNING,
                   "Simulation démarrée mais PID introuvable | id=%s | args=%s",
                   simulation.get_id().c_str(),
                   sandPandaArgs.c_str());
        }

        if (isNewPID)
            simulationRepository.isRunning(simulation);
        else
            simulationRepository.isError(simulation);
        simulationRepository.save();
    }

    if (simulationRepository.isPending()) {
        const auto &next = simulationRepository.getPending().begin()->second;
        printf("En attente : %s demande %d thread(s), %d libre(s) sur %d\n", next.get_id().c_str(),
               next.get_threads_number(), simulationRepository.get_number_threads_free(),
               simulationRepository.get_max_number_threads());
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



void SimulationScheduler::scheduleSimulations() {
    // Copie : isCompleted() retire les simulations terminees de la liste en cours
    const std::vector<Simulation> running = simulationRepository.getRunning();
    printf("Verification de %zu simulation(s) en cours\n", running.size());

    int completedCount = 0;
    for (const auto &simulation: running) {
        if (simulation.get_pid() <= 0) {
            // Ancienne entree sans PID : impossible a suivre, elle bloquerait ses threads indefiniment
            printf("/!\\ ALERTE : %s n'a pas de PID connu, simulation placee en erreur\n",
                   simulation.get_id().c_str());
            syslog(LOG_WARNING, "Simulation sans PID placee en erreur | id=%s", simulation.get_id().c_str());
            simulationRepository.isError(simulation);
            continue;
        }
        if (!isProcessRunning(simulation.get_pid())) {
            printf("Fin detectee : %s (pid %d)\n", simulation.get_id().c_str(),
                   static_cast<int>(simulation.get_pid()));

            // ── Log : détection de fin de simulation ──────────────────────────
            syslog(LOG_INFO,
                   "Fin simulation détectée | id=%s | pid=%d",
                   simulation.get_id().c_str(),
                   static_cast<int>(simulation.get_pid()));

            simulationRepository.isCompleted(simulation);
            completedCount++;
        } else {
            printf("Toujours en cours : %s (pid %d)\n", simulation.get_id().c_str(),
                   static_cast<int>(simulation.get_pid()));
        }
    }

    // Lancement des simulations en attente tant que des threads sont libres
    // (y compris si des threads ont ete liberes autrement : -stopRunning, -numberThreads)
    const size_t runningBefore = simulationRepository.getRunning().size();
    const size_t errorsBefore = simulationRepository.getErrors().size();
    if (simulationRepository.isPending())
        startSimulation();
    else
        printf("Aucune simulation en attente\n");
    const size_t startedCount = simulationRepository.getRunning().size() - runningBefore;
    const size_t failedCount = simulationRepository.getErrors().size() - errorsBefore;

    printf("Bilan : %d terminee(s), %zu demarree(s), %zu en erreur, %zu en cours, %zu en attente, %d/%d threads utilises\n",
           completedCount, startedCount, failedCount, simulationRepository.getRunning().size(),
           simulationRepository.getPending().size(), simulationRepository.number_of_threads_used(),
           simulationRepository.get_max_number_threads());
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

void SimulationScheduler::status() {
    const int used = simulationRepository.number_of_threads_used();
    const int max = simulationRepository.get_max_number_threads();
    printf("=== SandPandaScheduler : statut ===\n");
    printf("Threads            : %d utilise(s) / %d (%d libre(s))\n", used, max, max - used);
    printf("Prochaine priorite : %d\n", simulationRepository.get_next_priority());

    const auto &running = simulationRepository.getRunning();
    printf("\nEn cours (%zu)\n", running.size());
    if (!running.empty())
        printf("  %-20s %8s %8s  %-10s %s\n", "id", "pid", "threads", "etat", "arguments");
    for (const auto &simulation: running) {
        const pid_t pid = simulation.get_pid();
        const char *state = pid <= 0 ? "PID ?" : (isProcessRunning(pid) && isSandPandaProcess(pid)) ? "actif" : "termine";
        printf("  %-20s %8d %8d  %-10s %s\n", simulation.get_id().c_str(), static_cast<int>(pid),
               simulation.get_threads_number(), state, simulation.get_sandPandaArgs().c_str());
    }

    const auto &pending = simulationRepository.getPending();
    printf("\nEn attente (%zu)\n", pending.size());
    if (!pending.empty())
        printf("  %-9s %-20s %8s  %s\n", "priorite", "id", "threads", "arguments");
    for (const auto &[priority, simulation]: pending) {
        printf("  %-9d %-20s %8d  %s\n", priority, simulation.get_id().c_str(), simulation.get_threads_number(),
               simulation.get_sandPandaArgs().c_str());
    }
    const auto &errors = simulationRepository.getErrors();
    if (!errors.empty()) {
        printf("\n/!\\ En erreur, PID introuvable (%zu) - supprimees par -clean\n", errors.size());
        printf("  %-20s %8s  %s\n", "id", "threads", "arguments");
        for (const auto &simulation: errors)
            printf("  %-20s %8d  %s\n", simulation.get_id().c_str(), simulation.get_threads_number(),
                   simulation.get_sandPandaArgs().c_str());
    }
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