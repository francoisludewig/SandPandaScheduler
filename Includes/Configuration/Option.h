#pragma once
#include <string>

class Option{
private:
	char **list{};
	int Nlist{};
	std::string sandPandaArgs{};
	std::string id{};

public:
	[[nodiscard]] std::string sand_panda_args() const {
		return sandPandaArgs;
	}

	[[nodiscard]] std::string id1() const {
		return id;
	}

	[[nodiscard]] int threads_number1() const {
		return threads_number;
	}

	[[nodiscard]] int number_threads() const {
		return numberThreads;
	}

private:
	int threads_number = 1;
	int numberThreads = -1;
public:
	bool add = false, clean = false, schedule = false, change_numberThreads = false;
	Option() noexcept;
	int Management(char **argv, int argc) noexcept;
};
