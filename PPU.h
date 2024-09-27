#ifndef PPU_H
#define PPU_H

#include "SDL.h"
#include "CPU.h"
#include <string.h>
#include <iostream>

#define WIDTH 160*4
#define HEIGHT 144*4
#define FRAMES_PER_SEC 60.0
#define DOTS_PER_FRAME 70224

using namespace std;

struct sprite {
    uint8_t y_pos;
    uint8_t x_pos;
    uint8_t tile_ID;
    uint8_t flags;
};

class PPU {
public:
    PPU();
    ~PPU();
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    void dot(int t_cycle_backlog);
    void init(CPU *cpu);
    void close();
    void renderFrame();

    bool frame_ready = false;
    bool color_on = true;
    bool color_on_colorless = false;
    int getMode();

private:
    void set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
    void find_and_set_pixel(int x, int y);
    int getWindowColorID(int x, int y);
    int getBackgroundColorID(int x, int y);
    CPU *cpu;

    uint8_t lcdc;
    uint8_t ly = 0, lyc, stat;
    uint8_t scx, scy;
    int wy, wx;
    int obj_h = 8;
    bool bg_priority = false;

    void recomputeMapping(int target);
    int bgp_mapping[4] = {};
    int obp0_mapping[4] = {};
    int obp1_mapping[4] = {};

    int mode = 0;
    int scanline_dot = 0;
    int curX = 0;

    static int const num_palettes = 11;
    uint32_t const palettes[num_palettes][4] = {
        {0xffb0c0a0, 0xff889878, 0xff607050, 0xff283818},
        {0xff9bbc0f, 0xff8bac0f, 0xff306230, 0xff0f380f},
        {0xfffff6d3, 0xfff9a875, 0xffeb6b6f, 0xff7c3f58},
        {0xffe9efec, 0xffa0a08b, 0xff555568, 0xff211e20},
        {0xffe2f3e4, 0xff94e344, 0xff46878f, 0xff332c50},
        {0xffedb4a1, 0xffa96868, 0xff764462, 0xff2c2137},
        {0xffc4f0c2, 0xff5ab9a8, 0xff1e606e, 0xff2d1b00},
        {0xff8be5ff, 0xff608fcf, 0xff7550e8, 0xff622e4c},
        {0xfff8e3c4, 0xffcc3495, 0xff6b1fb1, 0xff0b0630},
        {0xffcfab51, 0xff9d654c, 0xff4d222c, 0xff210b1b},
        {0xffe8d6c0, 0xff92938d, 0xffa1281c, 0xff000000}
    };
    uint32_t colors[4];
    void selectPalette(int palette);

    struct sprite obj_on_line[10] = {};
    int cur_obj = 0;
    void writeObjLine(struct sprite obj);
    void prepObjLine();
    int scanline_objs_colors[160] = {};
    int obj_colors_earliest_x[160] = {};
    bool obj_priority[160] = {};
};

#endif