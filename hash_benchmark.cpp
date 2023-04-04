#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <optional>
#include <iostream>

#include "sha224.hpp"

#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef uint64_t u64;

typedef std::string (*hash_fn)(std::string);

// Supported algorithms
enum algorithm {
	SHA224,
};

// Information about an algorithm
struct algorithm_info {
	const char *name;
	hash_fn hash;
};

// Print the error message and exit with a failure
[[noreturn]] void die(const char *msg);

// Print usage info to standard output
void usage();

// Parse a string into an algorithm, or nothing if it's not known
std::optional<algorithm> get_algorithm(char *name);

// Parse a number into a u64, or nothing if it's invalid
std::optional<u64> get_number(char *str);

// Run an iteration of the loop
std::string run_hash(u64 i, hash_fn f);

// Take the value or die with message
template<class T> T extract_or_die(std::optional<T> o, const char *msg);

const char tab = '\t';

const char *program_name = "hash_benchmark";

const struct algorithm_info algorithms[] = {
	/* [SHA224] = */ {
		.name = "sha224",
		.hash = sha224,
	},
};

[[noreturn]] void
die(const char *msg)
{
	std::cerr << program_name << ": " << msg << std::endl;
	std::exit(1);
}

void
usage()
{
	std::cout << "usage: " << program_name << " <num_hashes> <algorithm>";
	std::cout << std::endl;
	std::cout << "algorithms:";
	for (size_t i = 0; i < LEN(algorithms); i++)
		std::cout << " " << algorithms[i].name;
	std::cout << std::endl;
}

std::optional<algorithm>
get_algorithm(char *name)
{
	for (size_t i = 0; i < LEN(algorithms); i++) {
		if (std::strcmp(algorithms[i].name, name) == 0)
			return std::optional((algorithm)i);
	}
	return std::optional<algorithm>();
}

std::optional<u64>
get_number(char *str)
{
	char *end;
	u64 result = std::strtoull(str, &end, 0);
	if (*end != '\0')
		return std::optional<algorithm>();
	return result;
}

template<class T> T
extract_or_die(std::optional<T> o, const char *msg)
{
	if (o)
		return *o;
	die(msg);
}

std::string
run_hash(u64 i, hash_fn f)
{
	std::string input = std::to_string(i);
	std::string output = f(input);
	return output;
}

int
main(int argc, char *argv[])
{
	program_name = argv[0];
	if (argc != 3) {
		usage();
		return 1;
	}
	u64 num_hashes = extract_or_die(get_number(argv[1]),
		"first argument must be an unsigned integer");
	algorithm alg = extract_or_die(get_algorithm(argv[2]),
		"second argument must be a known algorithm");

	std::cout << "hashes =" << tab << num_hashes << tab;
	std::cout << "algo =" << tab << algorithms[alg].name << tab;

	using namespace std::chrono;

	auto start_time = high_resolution_clock::now();

	for (u64 i = 0; i < num_hashes; i++)
		run_hash(i, algorithms[alg].hash);

	auto end_time = high_resolution_clock::now();

	duration<double> elapsed_duration = end_time - start_time;
	double elapsed = elapsed_duration.count();

	std::cout << "elapsed (s) =" << tab << elapsed << std::endl;

	return 0;
}
