#include <sycl/sycl.hpp>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <string>
#include <optional>
#include <iostream>
#include <iomanip>

#include "sha224.hpp"

#define LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef uint64_t u64;

typedef std::string (*hash_fn)(std::string);

// Supported runners
enum runner {
	SERIAL_RUNNER,
	SYCL_CPU_RUNNER,
	SYCL_GPU_RUNNER,
};

// Supported algorithms
enum algorithm {
	SHA224,
};

// Print the error message and exit with a failure
[[noreturn]] void die(const char *msg);
[[noreturn]] void die(const std::string& msg);

// Print usage info to standard output
void usage();

// Parse an argument to the appropriate type, or exit if it's invalid
algorithm parse_arg_algorithm(char **argv, int argidx);
u64 parse_arg_u64(char **argv, int argidx);
runner parse_arg_runner(char **arg, int argidx);

// Run an iteration of the loop
void run_hash(u64 i, algorithm f, char *output_buf);

// Run many iterations with SYCL
template<class Selector> void run_hashes_sycl(u64 iterations, hash_fn f,
	Selector selector, unsigned char *output_buf);

const char tab = '\t';

const char *program_name = "hash_benchmark";

const char *algorithm_name[] = {
	/* [SHA224] = */ "sha224",
};

const char *runner_name[] = {
	/* [SERIAL_RUNNER] = */ "serial",
	/* [SYCL_CPU_RUNNER] = */ "sycl-cpu",
	/* [SYCL_GPU_RUNNER] = */ "sycl-gpu",
};

[[noreturn]] void
die(const char *msg)
{
	std::cerr << program_name << ": " << msg << std::endl;
	std::exit(1);
}

[[noreturn]] void
die(const std::string& msg)
{
	die(msg.c_str());
}

void
usage()
{
	std::cout << "usage: " << program_name << " <num_hashes> <algorithm> <runner>";
	std::cout << std::endl;
	std::cout << "algorithms: sha224" << std::endl;
	std::cout << "runners: serial sycl-cpu sycl-gpu" << std::endl;
}

algorithm
parse_arg_algorithm(char **argv, int i)
{
	if (std::strcmp(argv[i], "sha224") == 0) {
		return SHA224;
	}
	die("argument " + std::to_string(i) + " should be a known hash algorithm");
}

runner
parse_arg_runner(char **argv, int i)
{
	if (std::strcmp(argv[i], "serial") == 0) {
		return SERIAL_RUNNER;
	}
	if (std::strcmp(argv[i], "sycl-cpu") == 0) {
		return SYCL_CPU_RUNNER;
	}
	if (std::strcmp(argv[i], "sycl-gpu") == 0) {
		return SYCL_GPU_RUNNER;
	}
	die("argument " + std::to_string(i) + " should be a known runner");
}

u64
parse_arg_u64(char **argv, int i)
{
	char *end;
	u64 result = std::strtoull(argv[i], &end, 0);
	if (*end != '\0')
		die("argument " + std::to_string(i) + " should be an natural number");
	return result;
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
	sycl::event hashes_ev = q.parallel_for(iterations, [=] (sycl::id<1> idx) {
		run_hash(idx, alg, sycl_buf);
	});
	sycl::event copy_ev = q.memcpy(output_buf, sycl_buf, siz, hashes_ev);
	copy_ev.wait();
}

int
main(int argc, char *argv[])
{
	bool print_hashes = false;
	program_name = argv[0];
	if (strcmp(argv[1], "-p") == 0) {
		print_hashes = true;
		argv++;
		argv[0] = (char *)program_name;
		argc--;
	}
	if (argc != 4) {
		usage();
		return 1;
	}
	u64 num_hashes = parse_arg_u64(argv, 1);
	algorithm alg = parse_arg_algorithm(argv, 2);
	runner r = parse_arg_runner(argv, 3);

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
	case SYCL_GPU_RUNNER:
		run_hashes_sycl(num_hashes, alg, sycl::gpu_selector_v,
				output_buffer);
		break;
	}

	auto end_time = high_resolution_clock::now();

	duration<double> elapsed_duration = end_time - start_time;
	double elapsed = elapsed_duration.count();

	std::cout << "elapsed (s) =" << tab << elapsed << std::endl;

	if (print_hashes) {
		for (u64 i = 0; i < num_hashes; i++) {
			auto oldfill = std::cerr.fill('0');
			std::cerr << "- " << std::hex;
			for (u64 j = 0; j < SHA224::DIGEST_SIZE; j++) {
				std::cerr << std::setw(2) << (int)output_buffer[i * SHA224::DIGEST_SIZE + j];
			}
			std::cerr << std::endl << std::dec << std::setfill(oldfill);
		}
	}

	delete[] output_buffer;

	return 0;
}
