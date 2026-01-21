#pragma once
#include <stdbool.h>

bool initialise_app(char* file);
void set_quit();
int run();
bool is_running();
void deinitialise_app();
