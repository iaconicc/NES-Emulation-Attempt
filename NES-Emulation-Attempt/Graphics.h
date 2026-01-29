#pragma once
#include <Windows.h>

int create_graphics_for_window(HWND hwnd);
void update_window_graphics();
void set_pixel(int x, int y, UINT32 colour);
void delete_graphics();
