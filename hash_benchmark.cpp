#include <boost/format.hpp>
#include <sycl/sycl.hpp>
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>

#include "sha224.hpp"

typedef uint8_t u8;
typedef uint64_t u64;

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

// Wrapper for printing a hash in hexadecimal.
class hash_hex {
public:
	// Use the hash at base
	hash_hex(u8 *base, size_t bytes);
	// If base is an array of hashes, use the ith hash.
	hash_hex(u8 *base, size_t i, size_t bytes);
	friend std::ostream& operator<<(std::ostream&, const hash_hex&);
private:
	u8 *base;
	size_t bytes;
};

// Print the error message and exit with a failure
[[noreturn]] void die(const char *msg);
[[noreturn]] void die(const std::string& msg);
[[noreturn]] void die(const boost::format& fmt);

// Print usage info to standard output
void usage();

// Parse an argument to the appropriate type, or exit if it's invalid
algorithm parse_arg_algorithm(char **argv, int argidx);
u64 parse_arg_u64(char **argv, int argidx);
runner parse_arg_runner(char **arg, int argidx);

// Return the runtime in seconds of the block of code.
template<class F> double time_execution(F f);

// Run an iteration of the loop
void run_hash(u64 input, u64 slot, algorithm f, u8 *output_buf);

// Run many iterations with SYCL
template<class Selector, class Sink> void run_hashes_sycl(u64 iterations,
	u64 num_blocks, algorithm f, Selector selector,
	u8 *output_buf, Sink sink);

// Print out the hash.
std::ostream& operator<<(std::ostream&, const hash_hex&);

const char *program_name = "hash_benchmark";

const char *algorithm_name[] = {
	/* [SHA224] = */ "sha224",
};

const size_t digest_size[] = {
	/* [SHA224] = */ SHA224::DIGEST_SIZE,
};

const char *runner_name[] = {
	/* [SERIAL_RUNNER] = */ "serial",
	/* [SYCL_CPU_RUNNER] = */ "sycl-cpu",
	/* [SYCL_GPU_RUNNER] = */ "sycl-gpu",
};

const boost::format output_fmt("hashes_per_block =\t%d\tnum_blocks =\t%d\t"
	"algo =\t%s\trunner =\t%s\telapsed (s) =\t%f\n");
const boost::format hex_fmt("%02x");
const boost::format hash_fmt("- %x\n");
const boost::format die_fmt("%s: %s\n");
const boost::format usage_fmt("usage: %s <hashes_per_block> <num_blocks> "
	"<algorithm> <runner>\n"
	"algorithms: sha224\n"
	"runners: serial sycl-cpu sycl-gpu\n");
const boost::format arg_err_fmt("argument %d should be %s");

[[noreturn]] void
die(const char *msg)
{
	std::cerr << boost::format(die_fmt) % program_name % msg;
	std::exit(1);
}

[[noreturn]] void
die(const std::string& msg)
{
	die(msg.c_str());
}

[[noreturn]] void
die(const boost::format& fmt)
{
	die(fmt.str());
}

void
usage()
{
	std::cout << boost::format(usage_fmt) % program_name;
}

algorithm
parse_arg_algorithm(char **argv, int i)
{
	if (std::strcmp(argv[i], "sha224") == 0) {
		return SHA224;
	}
	die(boost::format(arg_err_fmt) % i % "a known hash algorithm");
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
	die(boost::format(arg_err_fmt) % i % "a known runner");
}

u64
parse_arg_u64(char **argv, int i)
{
	char *end;
	u64 result = std::strtoull(argv[i], &end, 0);
	if (*end != '\0')
		die(boost::format(arg_err_fmt) % i % "a natural number");
	return result;
}

template<class F> double
time_execution(F f)
{
	using namespace std::chrono;
	auto start_time = high_resolution_clock::now();
	f();
	auto end_time = high_resolution_clock::now();
	duration<double> elapsed = end_time - start_time;
	return elapsed.count();
}

void
run_hash(u64 input, u64 slot, algorithm alg, u8 *output_buf)
{
	if (alg == SHA224) {
		class SHA224 ctx{};
		ctx.init();
		ctx.update((u8 *)&input, sizeof(input));
		ctx.final(output_buf + slot * SHA224::DIGEST_SIZE);
	}
}

template<class Selector, class Sink> void
run_hashes_sycl(u64 iterations, u64 num_blocks, algorithm alg,
		Selector selector, u8 *output_buf, Sink sink)
{
	using sycl::event, sycl::id, sycl::malloc_device, sycl::queue;
	queue q(selector);
	size_t siz = iterations * digest_size[alg];
	u8 *sycl_buf = malloc_device<u8>(siz, q);
	for (u64 i = 0; i < num_blocks; i++) {
		u64 base = i * iterations;
		event hashes_ev = q.parallel_for(iterations, [=] (id<1> idx) {
			run_hash(base + idx, idx, alg, sycl_buf);
		});
		event copy_ev = q.memcpy(output_buf, sycl_buf, siz, hashes_ev);
		copy_ev.wait();
		sink();
	}
}

hash_hex::hash_hex(u8 *base, size_t bytes):
	base(base), bytes(bytes)
{
	// nothing
}

hash_hex::hash_hex(u8 *base, size_t idx, size_t bytes):
	base(base + bytes * idx), bytes(bytes)
{
	// nothing
}

std::ostream&
operator<<(std::ostream& str, const hash_hex& hash)
{
	for (u64 i = 0; i < hash.bytes; i++)
		str << boost::format(hex_fmt) % (unsigned)hash.base[i];
	return str;
}

int
main(int argc, char *argv[])
{
	bool print_hashes = false;
	program_name = argv[0];
	if (argc > 2 && std::strcmp(argv[1], "-p") == 0) {
		print_hashes = true;
		argv++;
		argv[0] = (char *)program_name;
		argc--;
	}
	if (argc != 5) {
		usage();
		return 1;
	}
	u64 num_hashes = parse_arg_u64(argv, 1);
	u64 num_blocks = parse_arg_u64(argv, 2);
	algorithm alg = parse_arg_algorithm(argv, 3);
	runner r = parse_arg_runner(argv, 4);

	u8 *output_buffer = new u8[num_hashes * digest_size[alg]];
	auto sink_hashes = [&] () {
		if (print_hashes)
			for (u64 i = 0; i < num_hashes; i++)
				std::cerr << boost::format(hash_fmt) %
					hash_hex(output_buffer, i,
					digest_size[alg]);
	};

	double elapsed = time_execution([&] () {
		switch (r) {
		case SERIAL_RUNNER:
			for (u64 i = 0; i < num_hashes * num_blocks; i++) {
				if (i % num_hashes == 0 && i > 0)
					sink_hashes();
				run_hash(i, i % num_hashes, alg, output_buffer);
			}
			sink_hashes();
			break;
		case SYCL_CPU_RUNNER:
			run_hashes_sycl(num_hashes, num_blocks, alg,
					sycl::cpu_selector_v, output_buffer,
					sink_hashes);
			break;
		case SYCL_GPU_RUNNER:
			run_hashes_sycl(num_hashes, num_blocks, alg,
					sycl::gpu_selector_v, output_buffer,
					sink_hashes);
			break;
		}
	});

	std::cout << boost::format(output_fmt) % num_hashes % num_blocks %
		algorithm_name[alg] % runner_name[r] % elapsed;

	delete[] output_buffer;

	return 0;
}
