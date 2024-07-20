/*
 *  GameOfLife - a GameOfLife application with SyCL
 *  Copyright (C) 2024  Sven Vollmar & David Schwarzbeck

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "gameoflife.hpp"
#include <string>
#include <cstdio>
#include <chrono>

#include <SDL2/SDL.h>
#include <array>
#include <iostream>

void render_on_surfaces(const size_t width, const size_t height, SDL_Window *window, SDL_Surface *window_surface, const std::vector<GameOfLifeFrame> &frames);

using namespace std::chrono;


const size_t frameAmount = 1000;

void render_frames_directly(size_t width, size_t height, SDL_Renderer *renderer, std::vector<GameOfLifeFrame> &frames) {
    for (GameOfLifeFrame frame: frames) {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                bool alive = frame[y * width + x] == 1;
                Uint8 r = alive ? 255 : 0;
                Uint8 g = alive ? 255 : 0;
                Uint8 b = alive ? 255 : 0;
                SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }
}

int main(int argc, char **argv) {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    const size_t width = 1920;
    const size_t height = 1080;
    //const size_t width = 2560;
    //const size_t height = 1440;
    //const size_t width = 7680;
    //const size_t height = 4320;

    SDL_Window *window = SDL_CreateWindow("Simple SDL2 Example",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          width, height,
                                          SDL_WINDOW_SHOWN);


    if (window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create a renderer for the window
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Surface *window_surface = SDL_GetWindowSurface(window);

    // Set the background color to blue
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);

    // Clear the screen with the background color
    SDL_RenderClear(renderer);

    // Update the screen
    SDL_RenderPresent(renderer);

    const bool cpu = false;

    GameOfLifeFrame initialFrame = GameOfLife::buildGameOfLife(width, height);

    auto start_time = high_resolution_clock::now();
    std::vector<GameOfLifeFrame> frames;
    if (cpu) {
        frames = GameOfLifeCPU::run(width, height, frameAmount, initialFrame);
    } else {
        frames = GameOfLifeSycl::run(width, height, frameAmount, initialFrame);
    }

    auto end_time = high_resolution_clock::now();
    auto elapsed_time = duration_cast<milliseconds>(end_time - start_time);

    std::cout << "Elapsed time: " << elapsed_time.count() << " milliseconds\n";

    render_on_surfaces(width, height, window, window_surface, frames);

    //render_frames_directly(width, height, renderer, frames);

    // Clean up SDL
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void render_on_surfaces(const size_t width, const size_t height, SDL_Window *window, SDL_Surface *window_surface,
                        const std::vector<GameOfLifeFrame> &frames) {
    std::array<SDL_Surface *, frameAmount> surfaces{};
    for (int i = 0; i < frameAmount; i++) {
        GameOfLifeFrame frame = frames[i];

        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
        // Get the surface pixel data
        SDL_LockSurface(surface);
        int pitch = surface->pitch; // Width of surface in bytes
        Uint32 *buffer = (Uint32 *) surface->pixels;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                size_t imageIndex = y * width + x;
                bool alive = frame[imageIndex] == 1;
                Uint8 r = alive ? 255 : 0;
                Uint8 g = alive ? 255 : 0;
                Uint8 b = alive ? 255 : 0;
                Uint8 a = 255;

                Uint32 rgba = SDL_MapRGBA(surface->format, r, g, b, a);

                buffer[imageIndex] = rgba;
            }
        }

        // Unlock the surface
        SDL_UnlockSurface(surface);

        //surfaces[i] = surface;

        SDL_Rect dstRect = {0, 0, static_cast<int>(width), static_cast<int>(height)};

        // Render the texture onto the renderer
        SDL_BlitSurface(surface, nullptr, window_surface, nullptr);
        SDL_UpdateWindowSurface(window);

        //SDL_Delay(1);

        SDL_FreeSurface(surface);
    }
}
