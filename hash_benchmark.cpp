#include <sycl/sycl.hpp>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <optional>
#include <iostream>

#include "sha224.hpp"
#include "sha224.cpp"

#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef uint64_t u64;

typedef std::string (*hash_fn)(std::string);

// Supported runners
enum runner {
	SERIAL_RUNNER,
	SYCL_CPU_RUNNER,
};

// Supported algorithms
enum algorithm {
	SHA224,
};

// Print the error message and exit with a failure
[[noreturn]] void die(const char *msg);

// Print usage info to standard output
void usage();

// Parse a string into an algorithm, or nothing if it's not known
std::optional<algorithm> get_algorithm(char *name);

// Parse a number into a u64, or nothing if it's invalid
std::optional<u64> get_number(char *str);

// Parse a string into a runner, or nothing if it's invalid
std::optional<runner> get_runner(char *str);

// Run an iteration of the loop
void run_hash(u64 i, algorithm f, char *output_buf);

// Run many iterations with SYCL
template<class Selector> void run_hashes_sycl(u64 iterations, hash_fn f,
	Selector selector, unsigned char *output_buf);

// Take the value or die with message
template<class T> T extract_or_die(std::optional<T> o, const char *msg);

const char tab = '\t';

const char *program_name = "hash_benchmark";

const char *algorithm_name[] = {
	/* [SHA224] = */ "sha224",
};

const char *runner_name[] = {
	/* [SERIAL_RUNNER] = */ "serial",
	/* [SYCL_CPU_RUNNER] = */ "sycl-cpu",
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
	std::cout << "usage: " << program_name << " <num_hashes> <algorithm> <runner>";
	std::cout << std::endl;
	std::cout << "algorithms: sha224" << std::endl;
	std::cout << "runners: serial sycl-cpu" << std::endl;
}

std::optional<algorithm>
get_algorithm(char *name)
{
	if (std::strcmp(name, "sha224") == 0) {
		return std::optional(SHA224);
	}
	return std::optional<algorithm>();
}

std::optional<runner>
get_runner(char *name)
{
	if (std::strcmp(name, "serial") == 0) {
		return std::optional(SERIAL_RUNNER);
	}
	if (std::strcmp(name, "sycl-cpu") == 0) {
		return std::optional(SYCL_CPU_RUNNER);
	}
	return std::optional<runner>();
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

void
run_hash(u64 i, algorithm alg, unsigned char *output_buf)
{
	if (alg == SHA224) {
		class SHA224 ctx{};
		ctx.init();
		ctx.update((unsigned char *)&i, sizeof(i));
		ctx.final(output_buf + i * SHA224::DIGEST_SIZE);	
	}
}

template<class Selector> void
run_hashes_sycl(u64 iterations, algorithm alg, Selector selector,
		unsigned char *output_buf)
{
	sycl::queue q(selector);
	size_t siz = iterations * SHA224::DIGEST_SIZE;
	unsigned char *sycl_buf = sycl::malloc_device<unsigned char>(siz, q);
	q.parallel_for(iterations, [=] (sycl::id<1> idx) {
		run_hash(idx, alg, sycl_buf);
	});
	q.wait();
}

int
main(int argc, char *argv[])
{
	program_name = argv[0];
	if (argc != 4) {
		usage();
		return 1;
	}
	u64 num_hashes = extract_or_die(get_number(argv[1]),
		"first argument must be an unsigned integer");
	algorithm alg = extract_or_die(get_algorithm(argv[2]),
		"second argument must be a known algorithm");
	runner r = extract_or_die(get_runner(argv[3]),
		"third argument must be a known runner");

	std::cout << "hashes =" << tab << num_hashes << tab;
	std::cout << "algo =" << tab << algorithm_name[alg] << tab;
	std::cout << "runner =" << tab << runner_name[r] << tab;

	using namespace std::chrono;

	unsigned char *output_buffer = new unsigned char[num_hashes * SHA224::DIGEST_SIZE];

	auto start_time = high_resolution_clock::now();

	switch (r) {
	case SERIAL_RUNNER:
		for (u64 i = 0; i < num_hashes; i++)
			run_hash(i, alg, output_buffer);
		break;
	case SYCL_CPU_RUNNER:
		run_hashes_sycl(num_hashes, alg, sycl::cpu_selector_v,
				output_buffer);
		break;
	}

	auto end_time = high_resolution_clock::now();

	duration<double> elapsed_duration = end_time - start_time;
	double elapsed = elapsed_duration.count();

	std::cout << "elapsed (s) =" << tab << elapsed << std::endl;

	delete[] output_buffer;

	return 0;
}
