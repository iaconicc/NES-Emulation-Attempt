#pragma once
#include "bus.h"

Bus_device** get_cpu_device_registry(int* count);
Bus_device** get_ppu_device_registry(int* count);