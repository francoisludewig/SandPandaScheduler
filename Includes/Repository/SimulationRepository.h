//
// Created by ludfr on 07/03/2026.
//

#pragma once
#include <filesystem>
#include <map>
#include <vector>

#include "../Model/Simulation.h"

class SimulationRepository {
public:
    void add(const Simulation& simulation);
    Simulation getNext();
    int get_number_threads_for_next() const;
    void isRunning(const Simulation& simulation);
    void isCompleted(const Simulation& simulation);
    void cleanCompleted();
    int number_of_threads_used() const;

    std::vector<Simulation>& getRunning() {return running;}

    void save() const;
    void load();

    void set_max_number_threads(const int new_value) {max_number_threads = new_value; save();}
    [[nodiscard]] int get_number_threads_free() const {return max_number_threads-number_of_threads_used();}

    [[nodiscard]] bool isPending() const {return pending.size()>0;}

private:

    static std::filesystem::path getConfigFilePath() ;
    int32_t max_number_threads = 12;
    int32_t next_id = 1;
    std::map<int32_t, Simulation> pending{};
    std::vector<Simulation> running{};
    std::vector<Simulation> completed{};
};


