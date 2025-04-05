#pragma once

#include <cstdio>

#define log_info(message, ...) printf("[INF] " message "\n", ##__VA_ARGS__)
#define log_warn(message, ...) printf("[WRN] " message "\n", ##__VA_ARGS__)
#define log_error(message, ...) printf("[ERR] " message "\n", ##__VA_ARGS__)