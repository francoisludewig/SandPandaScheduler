//
// Created by ludfr on 07/03/2026.
//

#pragma once
#include <string>
#include <utility>

class Simulation{
public:
    Simulation() = default;

    Simulation(std::string sand_panda_args, std::string id, const int threads_number)
        : sandPandaArgs(std::move(sand_panda_args)),
          id(std::move(id)),
          threads_number(threads_number) {
    }

    Simulation(std::string sand_panda_args, std::string id, const int threads_number, const int pid)
        : sandPandaArgs(std::move(sand_panda_args)),
          id(std::move(id)),
          threads_number(threads_number),
          pid(pid) {
    }

    [[nodiscard]] std::string get_sandPandaArgs() const {
        return sandPandaArgs;
    }


    [[nodiscard]] std::string get_id() const {
        return id;
    }

    [[nodiscard]] int get_pid() const {
        return pid;
    }

    [[nodiscard]] int get_threads_number() const {
        return threads_number;
    }

    bool operator==(const Simulation& other) const {
        return id == other.id;
    }

    void setId(__pid_t new_pid) {this->pid = new_pid; }

private:
    std::string sandPandaArgs{};
    std::string id{};
    int threads_number = 1;
    int pid = -1;
};

