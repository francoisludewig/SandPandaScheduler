//
// Created by ludfr on 07/03/2026.
//

#pragma once

#include "../../Includes/Repository/SimulationRepository.h"

class SimulationScheduler {
public:
    SimulationScheduler();
    void add(std::string sandPandaArgs, std::string id, int threads_number);

    void startSimulation();
    void scheduleSimulations();
    void clean();
    void update_max_number_threads(int newValue);

private:
    SimulationRepository simulationRepository;

};


