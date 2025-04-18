#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <random>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <ctime>
#include <sstream>
#include <limits.h>
extern "C" {
static int deterministic_mutation(int siteID, int envseed) {
    return (siteID * 31 + envseed * 13) & 0xFFFF;
}


int replace(int oldVal, int siteID, const char *fnName, const char *instStr) {
    // 读取 REPLACE 环境变量，用于比对 siteID
    const char *rEnv = getenv("REPLACE");
    if (rEnv != NULL) {
        int targetID = atoi(rEnv);
        if (targetID == siteID) {
            // 再读 ENVSEED，默认为0
            int envseed = 0;
            const char *sEnv = getenv("ENVSEED");
            if (sEnv != NULL) {
                envseed = atoi(sEnv);
            }
            // 用确定性公式生成变异值
            int mutated = deterministic_mutation(siteID, envseed);

            fprintf(stderr,
                "[*] replace: siteID=%d in %s\n"
                "    oldVal=%d => mutated=%d\n"
                "    IR: %s\n",
                siteID, fnName, oldVal, mutated, instStr);

            return mutated;
        }
    }

    // 默认不变异
    return oldVal;
}
	// int32 mutation
	int32_t mutate(int32_t value) {
		static int lastCase = -1;
		int option = rand() % 8;
		while(option == lastCase) {
			option = rand() % 8;
		}
		lastCase = option;
		// return (option == 0) ? value-1 : 0;
		switch (option) {
			case 0:
			            return 0;
			case 1:
			            return INT_MIN;
			case 2:
			            return INT_MAX;
			case 3:
			            // XOR with a random number
			return value ^ (rand() % INT_MAX);
			case 4:
			            return value + 1;
			case 5:
			            return value - 1;
			case 6:
			            return value + 2;
			case 7:
			            return value - 2;
			default:
			            return value;
			// Default case, should never be reached
		}
	}

	// int64 mutation
	int64_t mutate_i64(int64_t value) {
		static int lastCase = -1;
		int option = rand() % 8;
		while(option == lastCase) {
			option = rand() % 8;
		}
		lastCase = option;
		switch (option) {
			case 0:
			            return 0;
			case 1:
			            return LLONG_MIN;
			case 2:
			            return LLONG_MAX;
			case 3:
			            // XOR with a random number
			return value ^ (rand() % LLONG_MAX);
			case 4:
			            return value + 1;
			case 5:
			            return value - 1;
			case 6:
			            return value + 2;
			case 7:
			            return value - 2;
			default:
			            return value;
			// Default case, should never be reached
		}
	}

	// int8 mutation
	int8_t mutate_i8(int8_t value) {
		// return value - 1;
		static int lastCase = -1;
		int option = rand() % 8;
		while(option == lastCase) {
			option = rand() % 8;
		}
		lastCase = option;
		switch (option) {
			case 0:
			            return 0;
			case 1:
			            return INT8_MIN;
			// Minimum value for int8_t
			case 2:
			            return INT8_MAX;
			// Maximum value for int8_t
			case 3:
			            // XOR with a random number
			return value ^ (rand() % INT8_MAX);
			case 4:
			            return value + 1;
			case 5:
			            return value - 1;
			case 6:
			            return value + 2;
			case 7:
			            return value - 2;
			default:
			            return value;
			// Default case, should never be reached
		}
	}

	#include <sstream>
	// int mutation handler
	int32_t mutate_int(int32_t value, int32_t id, const char *function_name, const char *inst) {
		const char *mutation_points_str = std::getenv("MUTATION_POINT");
		if (mutation_points_str == nullptr) {
			return value;
		}
		std::istringstream iss(mutation_points_str);
		std::string point;
		while (std::getline(iss, point, ',')) {
			int32_t mutation_point = std::atoi(point.c_str());
			if (id == mutation_point) {
				int32_t original_value = value;
				srand(time(NULL));
				value = mutate(value);
				printf("[+]Mutating point %d in function %s, instruction:%s, original value: %d, mutated value: %d\n", id, function_name, inst, original_value, value);
				break;
			}
		}
		return value;
	}

	// switch mutation handler
	int32_t mutate_switch(int32_t value, int32_t id, const char *function_name, const char *inst) {
		const char *mutation_points_str = std::getenv("MUTATION_POINT");
		if (mutation_points_str == nullptr) {
			return value;
		}
		std::istringstream iss(mutation_points_str);
		std::string point;
		int32_t ret = value;
		while (std::getline(iss, point, ',')) {
			int32_t mutation_point = std::atoi(point.c_str());
			if (id == mutation_point) {
				int tmp = rand() % 2;
				ret += (tmp == 0) ? -1 : (tmp == 1) ? 1 : 0;
				printf("[+]Mutating point %d in function %s, instruction:%s, mutate switch\n", id, function_name, inst);
				break;
			}
		}
		return ret;
	}

	// cmp mutation handler
	int32_t mutate_cmp(bool value, int32_t id, const char *function_name, const char *inst) {
		const char *mutation_points_str = std::getenv("MUTATION_POINT");
		if (mutation_points_str == nullptr) {
			return value ? 1:0;
		}
		std::istringstream iss(mutation_points_str);
		std::string point;
		int32_t ret = value ? 1:0;
		while (std::getline(iss, point, ',')) {
			int32_t mutation_point = std::atoi(point.c_str());
			if (id == mutation_point) {
				ret = value ? 0:1;
				printf("[+]Mutating point %d in function %s, instruction:%s, invert cmp result\n", id, function_name, inst);
				break;
			}
		}
		return ret;
	}

	int32_t print_int(int32_t value, const char *function_name) {
		printf("[+]Load %d in function %s\n", value, function_name);
		return value;
	}

	int32_t print_int_store(int32_t value, const char *function_name) {
		printf("[+]Store %d in function %s\n", value, function_name);
		return value;
	}

	int64_t mutate_int64(int64_t value, int32_t id, const char *function_name, const char *inst) {
		const char *mutation_points_str = std::getenv("MUTATION_POINT");
		if (mutation_points_str == nullptr) {
			return value;
		}
		std::istringstream iss(mutation_points_str);
		std::string point;
		while (std::getline(iss, point, ',')) {
			int32_t mutation_point = std::atoi(point.c_str());
			if (id == mutation_point) {
				int64_t original_value = value;
				srand(time(NULL));
				value = mutate_i64(value);
				printf("[+]Mutating point %d in function %s, instruction:%s, original value: %ld, mutated value: %ld\n", id, function_name, inst, original_value, value);
				break;
			}
		}
		return value;
	}

	int8_t mutate_int8(int8_t value, int32_t id, const char *function_name, const char *inst) {
		const char *mutation_points_str = std::getenv("MUTATION_POINT");
		if (mutation_points_str == nullptr) {
			return value;
		}
		std::istringstream iss(mutation_points_str);
		std::string point;
		while (std::getline(iss, point, ',')) {
			int32_t mutation_point = std::atoi(point.c_str());
			if (id == mutation_point) {
				int8_t original_value = value;
				srand(static_cast<unsigned int>(time(NULL)));
				value = mutate_i8(value);
				// Assuming you have a mutate_i8 function
				printf("[+] Mutating point %d in function %s, instruction:%s, original value: %d, mutated value: %d\n", id, function_name, inst, original_value, value);
				break;
			}
		}
		return value;
	}
	
	void log_function_call(const char *function_name) {
		printf("[+]%s\n", function_name);
	}
}
// extern "C"
