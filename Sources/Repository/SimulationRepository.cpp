//
// Created by ludfr on 07/03/2026.
//

#include<algorithm>
#include <fstream>

#include "../../Includes/Repository/SimulationRepository.h"
#include "../../Includes/Model/Simulation.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

void SimulationRepository::add(const Simulation& simulation) {
    pending[next_id] = simulation;
    next_id++;
}

Simulation SimulationRepository::getNext() {
    auto simulation = pending.begin()->second;
    pending.erase(pending.begin());
    return simulation;
}

int SimulationRepository::get_number_threads_for_next() const {
        return pending.begin()->second.get_threads_number();
}

void SimulationRepository::isRunning(const Simulation &simulation) {
    running.push_back(simulation);
}

void SimulationRepository::isCompleted(const Simulation& simulation) {
    auto it = std::find(running.begin(), running.end(), simulation);
    if (it != running.end()) {
        running.erase(it);
    }
    completed.push_back(simulation);
}

void SimulationRepository::cleanCompleted() {
    completed.clear();
    save();
}

int SimulationRepository::number_of_threads_used() const {
    int number_of_threads = 0;
    for (const auto& runningSimulation : running) {
        number_of_threads += runningSimulation.get_threads_number();
    }
    return number_of_threads;
}

std::filesystem::path SimulationRepository::getConfigFilePath()
{
    return std::filesystem::path(std::string(std::getenv("HOME"))) / ".config" / "SandPandaScheduler" / "config.json";
}

void SimulationRepository::save() const
{
    const auto configFilePath = getConfigFilePath();
    std::filesystem::create_directories(configFilePath.parent_path());

    nlohmann::json jsonData;
    if (!pending.empty()) {
        for (const auto& simulation : pending) {
            jsonData["pending"].push_back({
                {"id", simulation.first},
                {"sandPandaArgs", simulation.second.get_sandPandaArgs()},
                {"threads_number", simulation.second.get_threads_number()}
            });
        }
    }
    if (!running.empty()) {
        for (const auto& simulation : running) {
            jsonData["running"].push_back({
                {"id", simulation.get_id()},
                {"pid", simulation.get_pid()},
                {"sandPandaArgs", simulation.get_sandPandaArgs()},
                {"threads_number", simulation.get_threads_number()}
            });
        }
    }
    //TODO Comprendre pourquoi cette collection pose problème ! Elle n'est pas essentielle on plus !
    /*
    if (!completed.empty()) {
        for (const auto& simulation : completed) {
            jsonData["completed"].push_back({
                    {"id", simulation.get_id()},
                    {"pid", simulation.get_pid()},
                {"sandPandaArgs", simulation.get_sandPandaArgs()},
                {"threads_number", simulation.get_threads_number()}
            });
        }
    }
    */
    jsonData["next_id"] = next_id;
    jsonData["max_number_threads"] = max_number_threads;

    std::ofstream configFile(configFilePath, std::ios::trunc);
    configFile << jsonData.dump(4);
}


void SimulationRepository::load()
{
    const auto configFilePath = getConfigFilePath();
    if (std::filesystem::exists(configFilePath)) {
        std::ifstream configFile(configFilePath);
        nlohmann::json jsonData;
        configFile >> jsonData;

        pending.clear();
        running.clear();
        completed.clear();

        for (const auto& pendingSimulation : jsonData["pending"]) {
            pending.emplace(pendingSimulation["id"].get<int32_t>(), Simulation{
                pendingSimulation["sandPandaArgs"].get<std::string>(),
                std::to_string(pendingSimulation["id"].get<int32_t>()),
                pendingSimulation["threads_number"].get<int>()
            });
        }
        for (const auto& runningSimulation : jsonData["running"]) {
            running.emplace_back(
                runningSimulation["sandPandaArgs"].get<std::string>(),
                runningSimulation["id"].get<std::string>(),
                runningSimulation["threads_number"].get<int>(),
                runningSimulation["pid"].get<int>()
            );
        }
        for (const auto& completedSimulation : jsonData["completed"]) {
            completed.emplace_back(
                completedSimulation["sandPandaArgs"].get<std::string>(),
                completedSimulation["id"].get<std::string>(),
                completedSimulation["threads_number"].get<int>(),
                completedSimulation["pid"].get<int>()
            );
        }

        next_id = jsonData["next_id"].get<int32_t>();
        max_number_threads = jsonData["max_number_threads"].get<int32_t>();
    }
}
