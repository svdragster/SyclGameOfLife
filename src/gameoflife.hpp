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
#ifndef GAMEOFLIFE_HPP
#define GAMEOFLIFE_HPP

#include <hipSYCL/sycl/libkernel/builtins.hpp>
#include <sycl/sycl.hpp>
#include <utility>
#include <vector>
#include <random>
#include <iostream>


typedef std::vector<int> GameOfLifeFrame;



namespace GameOfLife {
    GameOfLifeFrame buildGameOfLife(size_t width, size_t height) {
        GameOfLifeFrame frame(height * width, 0);

        {
            std::random_device rd;
            std::mt19937 generator(rd());
            std::uniform_int_distribution<int> distribution(0, 1);
            for (int i = 0; i < height; ++i) {
                for (int j = 0; j < width; ++j) {
                    frame[i * width + j] = distribution(generator);
                }
            }
        }
        // Glider
        /*{
            frame[5][5] = 1;
            frame[6][6] = 1;
            frame[7][6] = 1;
            frame[7][5] = 1;
            frame[7][4] = 1;
        }*/
        return frame;
    }

    const int offsets[8][2] = {
            {-1, -1},
            {-1, 0},
            {-1, 1},
            {0,  -1},
            {0,  1},
            {1,  -1},
            {1,  0},
            {1,  1}
    };

    int next(const size_t x, const size_t y, const size_t width, const GameOfLifeFrame &frame) {
        int neighbourCount = 0;
        for (auto offset: offsets) {
            int offsetY = offset[0];
            int offsetX = offset[1];
            neighbourCount += frame[(y + offsetY) * width + (x + offsetX)];
        }

        if (neighbourCount < 2 || neighbourCount > 3) {
            return 0;
        } else if (neighbourCount == 3) {
            return 1;
        } else {
            return frame[y * width + x];
        }
    }


    int nextSycl(const size_t x, const size_t y, const size_t width,
                 const sycl::accessor<int, 1, sycl::access::mode::read, sycl::access::target::global_buffer> &frame) {
        int neighbourCount = 0;
        for (auto offset: offsets) {
            int offsetY = offset[0];
            int offsetX = offset[1];
            neighbourCount += frame[(y + offsetY) * width + (x + offsetX)];
        }

        if (neighbourCount < 2 || neighbourCount > 3) {
            return 0;
        } else if (neighbourCount == 3) {
            return 1;
        } else {
            return frame[y * width + x];
        }
    }
}

namespace GameOfLifeCPU {

    std::vector<GameOfLifeFrame> run(size_t width, size_t height, int iterations, GameOfLifeFrame &initialConfig) {
        GameOfLifeFrame previousFrame = initialConfig;
        GameOfLifeFrame currentFrame(height * width, 0);
        std::vector<GameOfLifeFrame> frames;

        for (int i = 0; i < iterations; i++) {
#pragma omp parallel for
            for (int x = 1; x < width - 1; x++) {
                for (int y = 1; y < height - 1; y++) {
                    int result = GameOfLife::next(x, y, width, previousFrame);
                    currentFrame[y * width + x] = result;
                }
            }
            frames.push_back(currentFrame);
            previousFrame = currentFrame;
        }
        return frames;
    }

}

namespace GameOfLifeSycl {

    std::vector<GameOfLifeFrame> run(size_t width, size_t height, int iterations, GameOfLifeFrame initialConfig) {
        GameOfLifeFrame previousFrame = std::move(initialConfig);
        GameOfLifeFrame currentFrame(height * width, 0);
        std::vector<GameOfLifeFrame> frames;

        sycl::gpu_selector selector;
        //sycl::cpu_selector selector;

        sycl::queue queue(selector);

        std::cout << "Running on device: "
                  << queue.get_device().get_info<sycl::info::device::name>() << "\n";

        for (int i = 0; i < iterations; i++) {
            {
                sycl::buffer<int, 1> bufferCurrent(currentFrame.data(), sycl::range<1>(width * height));
                sycl::buffer<int, 1> bufferPrevious(previousFrame.data(), sycl::range<1>(width * height));


                queue.submit([&](sycl::handler &cgh) {
                    auto ptrCurrent = bufferCurrent.get_access<sycl::access::mode::write>(cgh);
                    auto ptrPrevious = bufferPrevious.get_access<sycl::access::mode::read>(cgh);
                    cgh.parallel_for<class GameOfLifeLoop>(
                            sycl::range<1>((height - 2) * (width - 2)),
                            [=](sycl::id<1> idx) {
                                const size_t index = idx[0];
                                const size_t x = (index % (width - 2)) + 1;
                                const size_t y = (index / (width - 2)) + 1;

                                int result = GameOfLife::nextSycl(x, y, width, ptrPrevious);
                                ptrCurrent[y * width + x] = result;
                            }
                    );
                });
                queue.wait();
            }
            //frames.push_back(currentFrame);
            previousFrame = currentFrame;
        }
        return frames;
    }

}

/*
 * Using workgroups instead of parallelizing every cell calculation
 */
namespace GameOfLifeSycl2 {

    std::vector<GameOfLifeFrame> run(size_t width, size_t height, int iterations, GameOfLifeFrame initialConfig) {
        GameOfLifeFrame previousFrame = std::move(initialConfig);
        GameOfLifeFrame currentFrame(height * width, 0);
        std::vector<GameOfLifeFrame> frames;

        sycl::gpu_selector selector;
        //sycl::cpu_selector selector;

        sycl::queue queue(selector);

        std::cout << "Running on device: "
                  << queue.get_device().get_info<sycl::info::device::name>() << "\n";

        size_t group_size = 4;
        size_t num_groups_x = width / group_size;
        size_t num_groups_y = height / group_size;
        std::cout << "num_groups: " << num_groups_x << " " << num_groups_y << std::endl;

        for (int i = 0; i < iterations; i++) {
            {
                sycl::buffer<int, 1> bufferCurrent(currentFrame.data(), sycl::range<1>(width * height));
                sycl::buffer<int, 1> bufferPrevious(previousFrame.data(), sycl::range<1>(width * height));
                queue.submit([&](sycl::handler &cgh) {

                    auto ptrCurrent = bufferCurrent.get_access<sycl::access::mode::write>(cgh);
                    auto ptrPrevious = bufferPrevious.get_access<sycl::access::mode::read>(cgh);
                    cgh.parallel_for<class GameOfLifeLoop>(
                            sycl::nd_range<2>({width, height}, {group_size, group_size}),
                            [=](sycl::nd_item<2> index) {
                                size_t grp_id_x = index.get_group()[0];
                                size_t loc_id_x = index.get_local_id(0);
                                int start_x = loc_id_x + (grp_id_x * group_size);
                                int stop_x = loc_id_x + ((grp_id_x + 1) * group_size);
                                size_t grp_id_y = index.get_group()[1];
                                size_t loc_id_y = index.get_local_id(1);
                                int start_y = loc_id_y + (grp_id_y * group_size);
                                int stop_y = loc_id_y + ((grp_id_y + 1) * group_size);
                                for (int x = start_x; x < stop_x; x++) {
                                    for (int y = start_y; y < stop_y; y++) {
                                        if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
                                            int result = GameOfLife::nextSycl(x, y, width, ptrPrevious);
                                            ptrCurrent[y * width + x] = result;
                                        }
                                    }
                                }
                            }
                    );
                });
                queue.wait();
            }
            frames.push_back(currentFrame);
            previousFrame = currentFrame;
        }
        return frames;
    }

}
#endif
