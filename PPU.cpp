#include "PPU.h"
#include <thread>
#include <stdio.h>
#include <time.h>

PPU::PPU() {
}

PPU::~PPU() {
    close();
}

void PPU::init(CPU *cpu) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0){
        std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return ;
    }
    window = SDL_CreateWindow("C8emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL){
        std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return ;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
        return ;
    } else {
        SDL_SetRenderDrawColor( renderer, 0x00, 0x00, 0x00, 0xFF );
    }
    surface = SDL_CreateRGBSurface(0, 160, 144, 32, 0, 0, 0, 0);
    if (surface == NULL){
        std::cout << "SDL_CreateRGBSurface Error: " << SDL_GetError() << std::endl;
        return ;
    }
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL){
        std::cout << "SDL_CreateTextureFromSurface Error: " << SDL_GetError() << std::endl;
        return ;
    }

    this->cpu = cpu;
    if (!cpu->color_on) {
        this->color_on = false;
        selectPalette(0);
    }

    if (!cpu->cartridge->isCGBGame()) {
        if (this->color_on) {
            color_on_colorless = true;
        }
    }
}

void PPU::selectPalette(int palette) {
    for (int i=0; i<4; i++)
        colors[i] = palettes[palette][i];
}

void PPU::close() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    renderer = NULL;
    window = NULL;
    surface = NULL;
    texture = NULL;

    SDL_Quit();
}

void PPU::set_pixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
  Uint32 * const target_pixel = (Uint32 *) ((Uint8 *) surface->pixels
                                             + y * surface->pitch
                                             + x * surface->format->BytesPerPixel);
  *target_pixel = pixel;
}

void PPU::writeObjLine(struct sprite obj) {
    int trueX = obj.x_pos - 8, trueY = obj.y_pos - 16;
    bool upper, lower;
    uint8_t subRow = ly - trueY;
    if (obj.flags & 0x40) subRow = obj_h - subRow - 1;

    if (obj_h == 16) { // handle 8x16 object tilings
        if (subRow >= 8) {
            obj.tile_ID |= 0x01;
            subRow -= 8;
        } else {
            obj.tile_ID &= 0xfe;
        }
    }

    uint8_t byte1 = cpu->readVRAM(0x8000 + (obj.tile_ID << 4) + 2*subRow,      color_on && (obj.flags & 0x08));
    uint8_t byte2 = cpu->readVRAM(0x8000 + (obj.tile_ID << 4) + 2*subRow + 1,  color_on && (obj.flags & 0x08));

    for (int i=std::max(0, -trueX); i<8 && i+trueX<160; i++) {
        if ((obj_colors_earliest_x[trueX+i] > trueX && !color_on) || 
            (obj_colors_earliest_x[trueX+i] == 0xff &&  color_on)) {
            if (obj.flags & 0x20) {
                lower = (byte1 & (0x1 << i)) > 0;
                upper = (byte2 & (0x1 << i)) > 0;
            } else {
                lower = (byte1 & (0x1 << (7-i))) > 0;
                upper = (byte2 & (0x1 << (7-i))) > 0;
            }

            uint8_t colorID = lower | (upper << 1);
            if (colorID) {
                if (!color_on) {
                    if (obj.flags & 0x08)
                        scanline_objs_colors[trueX+i] = obp1_mapping[colorID];
                    else
                        scanline_objs_colors[trueX+i] = obp0_mapping[colorID];
                } else if (color_on_colorless) {
                    if (obj.flags & 0x08)
                        scanline_objs_colors[trueX+i] = obp1_mapping[colorID]+4;
                    else
                        scanline_objs_colors[trueX+i] = obp0_mapping[colorID];
                } else {
                    scanline_objs_colors[trueX+i] = ((obj.flags & 0x07) * 4) + colorID;
                }
                
                obj_colors_earliest_x[trueX+i] = trueX;
                obj_priority[trueX+i] = (obj.flags & 0x80);
            }
        }
    }
}

void PPU::prepObjLine() {
    for (int i=0; i<160; i++) {
        scanline_objs_colors[i] = 0;
        obj_colors_earliest_x[i] = 0xff;
        obj_priority[i] = 0;
    }
}

void PPU::recomputeMapping(int target) {
    int *mapping;
    uint8_t descr;

    switch (target) {
        case 0:
            descr = cpu->read(0xff47);
            mapping = bgp_mapping;
            break;
        case 1:
            descr = cpu->read(0xff48);
            mapping = obp0_mapping;
            break;
        case 2:
            descr = cpu->read(0xff49);
            mapping = obp1_mapping;
            break;
    }
    
    mapping[0] = (descr & 0x03) >> 0;
    if (target != 0) mapping[0] = 0;
    mapping[1] = (descr & 0x0c) >> 2;
    mapping[2] = (descr & 0x30) >> 4;
    mapping[3] = (descr & 0xc0) >> 6;
}

int PPU::getWindowColorID(int x, int y) {
    int trueX = x-wx+7, trueY = y-wy;
    int tileX = trueX / 8, tileY = trueY / 8;
    int subX = trueX % 8, subY = trueY % 8;

    uint16_t extra = (lcdc & 0x40) ? 0x0400 : 0x0000;
    uint16_t tileID = cpu->readVRAM(0x9800 + tileX + 0x20*tileY + extra, 0);
    uint16_t tile_Attr = 0;
    if (color_on) {
        tile_Attr = cpu->readVRAM(0x9800 + tileX + 0x20*tileY + extra, 1);
        if (tile_Attr & 0x40) subY = 7 - subY;
        if (tile_Attr & 0x20) subX = 7 - subX;
    }

    extra = (lcdc & 0x10) || tileID >= 0x80 ? 0x0000 : 0x1000;
    uint8_t byte1 = cpu->readVRAM(0x8000 + (tileID << 4) + 2*subY + extra,     (tile_Attr & 0x08) > 0);
    uint8_t byte2 = cpu->readVRAM(0x8000 + (tileID << 4) + 2*subY + 1 + extra, (tile_Attr & 0x08) > 0);

    bool lower = (byte1 & (0x1 << (7-subX))) > 0;
    bool upper = (byte2 & (0x1 << (7-subX))) > 0;
    bg_priority = (tile_Attr & 0x80) && (lower || upper);

    if (color_on) return ((tile_Attr & 0x07) << 2) + lower + upper*2;
    return lower | (upper << 1);
}

int PPU::getBackgroundColorID(int x, int y) {
    int trueX = (x + scx) % 256, trueY = (y + scy) % 256;
    int tileX = trueX / 8, tileY = trueY / 8;
    int subX = trueX % 8, subY = trueY % 8;

    uint16_t extra = (lcdc & 0x08) ? 0x0400 : 0x0000;
    uint16_t tileID = cpu->readVRAM(0x9800 + tileX + 0x20*tileY + extra, 0);
    uint16_t tile_Attr = 0;
    if (color_on) {
        tile_Attr = cpu->readVRAM(0x9800 + tileX + 0x20*tileY + extra, 1);
        if (tile_Attr & 0x40) subY = 7 - subY;
        if (tile_Attr & 0x20) subX = 7 - subX;
    }

    extra = (lcdc & 0x10) || tileID >= 0x80 ? 0x0000 : 0x1000;
    uint8_t byte1 = cpu->readVRAM(0x8000 + (tileID << 4) + 2*subY + extra,     (tile_Attr & 0x08) > 0);
    uint8_t byte2 = cpu->readVRAM(0x8000 + (tileID << 4) + 2*subY + 1 + extra, (tile_Attr & 0x08) > 0);

    bool lower = (byte1 & (0x1 << (7-subX))) > 0;
    bool upper = (byte2 & (0x1 << (7-subX))) > 0;
    bg_priority = (tile_Attr & 0x80) && (lower || upper);
    
    if (color_on) {
        if (color_on_colorless) return bgp_mapping[lower | upper * 2];
        return ((tile_Attr & 0x07) << 2) + lower + upper*2;
    }
    return lower | (upper << 1);
}

void PPU::find_and_set_pixel(int x, int y) {
    int final_color;
    uint8_t colorID;
    if ((lcdc & 0x20) && wx-7 <= x && wy <= y) {
        colorID = getWindowColorID(x, y);
    } else {
        colorID = getBackgroundColorID(x, y);
    }

    if (!(lcdc & 0x80)) {
        if (color_on) {
            final_color = 0xffffffff;
        }
        else {
            final_color = colors[0];
        }
    } else if (obj_colors_earliest_x[x] != 0xff && (lcdc & 0x02)) {
        if (color_on) {
            if (colorID % 4 == 0 || (lcdc & 0x01) == 0 || (!obj_priority[x] && !bg_priority))
                final_color = cpu->true_OBJ_COLOR[scanline_objs_colors[x]];
            else
                final_color = cpu->true_BG_COLOR[colorID];
        }
        else if (!obj_priority[x]) {
            final_color = colors[scanline_objs_colors[x]];
        }
        else {
            if (bgp_mapping[colorID] != 0) {
                final_color = colors[bgp_mapping[colorID]];
            } else if (color_on) {
                final_color = colors[scanline_objs_colors[x]];
            }
        }
    } else if (!(lcdc & 0x01) && !color_on) {
        final_color = colors[0];
    } else {
        if (color_on)
            final_color = cpu->true_BG_COLOR[colorID];
        else
            final_color = colors[bgp_mapping[colorID]];
    }

    set_pixel(surface, x, y, final_color);
}

void PPU::dot(int t_cycle_backlog) {
    if (!(cpu->read(0xff40) & 0x80)) return;

    while (t_cycle_backlog --> 0) {
        switch (mode) {
            case 0:
                scanline_dot++;
                if (scanline_dot >= 376) {
                    ly++;
                    cpu->write(0xff44, ly);
                    scanline_dot = 0;
                    curX = 0;

                    stat = cpu->read(0xff41);
                    lyc = cpu->read(0xff45);
                    if (lyc == ly) {
                        stat |= 0x04;
                        cpu->overrideSTAT(stat);
                        if (stat & 0x40)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);
                    } else {
                        stat &= 0xfb;
                        cpu->overrideSTAT(stat);
                    }

                    if (ly >= 144) {
                        mode = 1;
                        frame_ready = true;

                        // update STAT
                        stat = cpu->read(0xff41);
                        stat &= 0xfc;
                        stat |= 1;
                        cpu->overrideSTAT(stat);
                        if (stat & 0x10)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);

                        // send out VBlank interrupt request
                        cpu->write(0xff0f, cpu->read(0xff0f) | 0x01);
                    } else {
                        mode = 2;
                        cur_obj = 0;
                        prepObjLine();
                        lcdc = cpu->read(0xff40);
                        obj_h = (lcdc & 0x04) ? 16 : 8;
                        
                        // update STAT
                        stat = cpu->read(0xff41);
                        stat &= 0xfc;
                        stat |= 2;
                        cpu->overrideSTAT(stat);
                        if (stat & 0x20)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);
                    }
                }
                break;
            case 1:
                scanline_dot++;
                if (scanline_dot >= 456) {
                    ly++;
                    scanline_dot = 0;
                    cur_obj = 0;

                    if (ly >= 154) {
                        ly = 0;
                        mode = 2;
                        prepObjLine();
                        recomputeMapping(1); // update OBP0
                        recomputeMapping(2); // update OBP1
                        lcdc = cpu->read(0xff40);
                        obj_h = (lcdc & 0x04) ? 16 : 8;

                        // update STAT
                        stat = cpu->read(0xff41);
                        stat &= 0xfc;
                        stat |= 2;
                        cpu->overrideSTAT(stat);
                        if (stat & 0x20)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);

                        // lock OAM
                    }

                    cpu->write(0xff44, ly);
                    stat = cpu->read(0xff41);
                    lyc = cpu->read(0xff45);
                    if (lyc == ly) {
                        stat |= 0x04;
                        cpu->overrideSTAT(stat);
                        if (stat & 0x40)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);
                    } else {
                        stat &= 0xfb;
                        cpu->overrideSTAT(stat);
                    }
                }
                break;
            case 2:
                if (scanline_dot % 2 == 0 && cur_obj < 10) {
                    uint8_t obj_y = cpu->read(0xfe00 + scanline_dot*2);
                    if (ly+16 >= obj_y && ly+16 < obj_y+obj_h) {
                        obj_on_line[cur_obj].y_pos = obj_y;
                        obj_on_line[cur_obj].x_pos = cpu->read(0xfe00 + scanline_dot*2 + 1);
                        obj_on_line[cur_obj].tile_ID = cpu->read(0xfe00 + scanline_dot*2 + 2);
                        obj_on_line[cur_obj].flags = cpu->read(0xfe00 + scanline_dot*2 + 3);

                        writeObjLine(obj_on_line[cur_obj++]);
                    }
                }

                scanline_dot++;
                if (scanline_dot >= 80) {
                    scanline_dot = 0;
                    mode = 3;

                    // update STAT
                    stat = cpu->read(0xff41);
                    stat &= 0xfc;
                    stat |= 3;
                    cpu->overrideSTAT(stat);

                    lcdc = cpu->read(0xff40);
                    obj_h = (lcdc & 0x04) ? 16 : 8;
                    scy = cpu->read(0xff42);
                    scx = cpu->read(0xff43);
                    wy = cpu->read(0xff4a);
                    wx = cpu->read(0xff4b);
                    recomputeMapping(0);

                    // lock VRAM
                }
                break;
            case 3:
                scanline_dot++;
                find_and_set_pixel(curX++, ly);
                if (curX >= 160) {
                    mode = 0;

                    // update STAT
                    stat = cpu->read(0xff41);
                    stat &= 0xfc;
                    stat |= 0;
                    cpu->overrideSTAT(stat);
                    if (stat & 0x08)
                            cpu->write(0xff0f, cpu->read(0xff0f) | 0x02);

                    // free VRAM, OAM
                }
                break;
        }
    }
}

void PPU::renderFrame() {
    SDL_RenderClear(renderer);
    SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    frame_ready = false;
}

int PPU::getMode() {
    return mode;
}