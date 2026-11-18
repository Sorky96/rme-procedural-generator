// SPDX-License-Identifier: GPL-3.0-or-later
// Offline class raster -> G4 fingerprint. Only the explicitly supplied local file is read.
#include "procedural/core/copy_guard.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace rme::procedural;
int main(int argc, char** argv) try {
    if (argc != 2) throw std::runtime_error("usage: copy_index CLASS_RASTER (i53-raster/1 WIDTH HEIGHT newline then bytes 0..4 or 255)");
    std::ifstream in(argv[1], std::ios::binary);
    std::string line, schema, extra;
    uint64_t w = 0, h = 0;
    std::getline(in, line);
    std::istringstream header(line);
    if (!(header >> schema >> w >> h) || header >> extra || schema != "i53-raster/1" || !w || !h ||
        w > 65536 || h > 65536 || w * h > (64ULL << 20)) throw std::runtime_error("invalid or excessive raster");
    std::vector<uint8_t> cells(size_t(w * h));
    if (!in.read(reinterpret_cast<char*>(cells.data()), std::streamsize(cells.size())) || in.peek() != EOF)
        throw std::runtime_error("truncated raster or trailing data");
    const auto fp = classFingerprint(uint32_t(w), uint32_t(h), cells);
    if (!fp) throw std::runtime_error("fingerprint failed");
    std::cout << serializeFingerprint(*fp);
    return 0;
} catch (const std::exception& error) {
    std::cerr << "copy_index: " << error.what() << '\n'; return 2;
}
