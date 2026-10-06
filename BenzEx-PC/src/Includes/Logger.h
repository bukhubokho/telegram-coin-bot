#pragma once
// Desktop stub: replace Android __android_log_print with fprintf
#include <cstdio>
#define LOGI(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "[ERR]  " fmt "\n", ##__VA_ARGS__)
