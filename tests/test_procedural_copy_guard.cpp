// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/copy_guard.h"
#include "procedural/core/reference_header.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace rme::procedural;
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
Fingerprint values(uint64_t first, uint64_t count) {
    Fingerprint f; f.windows = f.candidateWindows = count;
    for (uint64_t i = first; i < first + count; ++i) f.hashes.push_back(i);
    return f;
}
CopyIndex fixture() {
    CopyIndex index; index.client = "10.98"; index.otbSha256 = std::string(64, 'a'); index.datSha256 = std::string(64, 'b');
    index.references[0] = {"M06", std::string(64, 'c'), std::string(64, 'd'), std::string(64, 'e'), values(0, 100)};
    index.references[1] = {"M19", std::string(64, 'f'), std::string(64, 'a'), std::string(64, 'e'), values(90, 100)};
    auto parsed = parseCopyIndex(serializeCopyIndex(index)); require(bool(parsed), "valid index"); return std::move(*parsed);
}
void arithmetic() {
    const auto index = fixture();
    // M06 intersection 16, denominator 160: exactly 100 per mille, both inclusive.
    auto candidate = values(1000, 144);
    candidate.hashes.insert(candidate.hashes.begin(), index.references[0].fingerprint.hashes.begin(), index.references[0].fingerprint.hashes.begin() + 16);
    candidate.windows = candidate.candidateWindows = candidate.hashes.size();
    auto result = assessCopy(candidate, index);
    require(result && result->hardFail() && result->shared[0] == 16 && result->permille[0] == 100, "inclusive 16 and 100");
    candidate.hashes.push_back(2000); ++candidate.windows; ++candidate.candidateWindows;
    result = assessCopy(candidate, index);
    require(result && !result->hardFail() && result->permille[0] == 99, "16 of 161 below ratio");
    candidate = values(0, 15);
    result = assessCopy(candidate, index);
    require(result && result->state == CopyState::TooSmall, "small sample never declared clear");
    candidate = values(0, 16); candidate.hashes.erase(candidate.hashes.begin()); candidate.hashes.push_back(1000);
    result = assessCopy(candidate, index);
    require(result && !result->hardFail() && result->shared[0] == 15, "ratio alone insufficient");
    candidate = values(90, 26);
    result = assessCopy(candidate, index);
    require(result && result->before == 26 && result->after == 16 && result->shared[0] == 0 && result->shared[1] == 16, "reference-only generic filter");
    require(assessCopy(values(1000, 100), index)->state == CopyState::Clear, "independent terrain");
    std::stop_source cancelled; cancelled.request_stop();
    require(assessCopy(values(0, 100), index, cancelled.get_token()).error() == SimilarityError::Cancelled, "cancel comparison");
}
void projection() {
    std::vector<uint8_t> cells(64 * 64);
    uint64_t rng = 1729;
    for (auto& c : cells) { rng = rng * 6364136223846793005ULL + 1; c = uint8_t((rng >> 32) % 5); }
    const auto fp = classFingerprint(64, 64, cells); require(fp && fp->hashes.size() >= 16, "class fingerprint");
    SurfaceGrid grid; grid.region = {100, 200, 7, 64, 64};
    const SurfaceKind kinds[] = {SurfaceKind::Grass, SurfaceKind::Dirt, SurfaceKind::Rock, SurfaceKind::Shore, SurfaceKind::River};
    for (auto c : cells) grid.cells.push_back(kinds[c]);
    auto index = fixture(); index.references[0].fingerprint = *fp; index.generic.clear();
    const auto copy = assessCopy(grid, index); require(copy && copy->hardFail() && copy->permille[0] == 1000, "shifted class copy");
    grid.region.x += 777; grid.region.z = 3;
    require(assessCopy(grid, index) == copy, "position and floor ignored");
    std::fill(cells.begin(), cells.end(), 0);
    const auto uniform = classFingerprint(64, 64, cells); require(uniform && uniform->hashes.empty(), "uniform skipped");
    std::fill(cells.begin(), cells.end(), 255); cells[0] = 0;
    const auto sparse = classFingerprint(64, 64, cells); require(sparse && sparse->hashes.empty() && sparse->sparseWindows == sparse->windows, "sparse skipped");
    cells[0] = 6; require(!classFingerprint(64, 64, cells), "unknown raster class refused");
    std::stop_source cancelled; cancelled.request_stop();
    require(classFingerprint(64, 64, cells, cancelled.get_token()).error() == SimilarityError::Cancelled, "cancel projection");
}
std::vector<uint8_t> noise(uint32_t width, uint32_t height, uint64_t seed) {
    std::vector<uint8_t> cells(size_t(width) * height);
    for (auto& c : cells) { seed = seed * 6364136223846793005ULL + 1; c = uint8_t((seed >> 32) % 6); if (c == 5) c = 255; }
    return cells;
}
// D10: the eight symmetries of the square, applied to a width x height raster.
std::vector<uint8_t> transformed(const std::vector<uint8_t>& cells, uint32_t width, uint32_t height, int symmetry,
    uint32_t& outWidth, uint32_t& outHeight) {
    const bool swap = symmetry >= 4;
    outWidth = swap ? height : width; outHeight = swap ? width : height;
    std::vector<uint8_t> out(cells.size());
    for (uint32_t y = 0; y < outHeight; ++y) for (uint32_t x = 0; x < outWidth; ++x) {
        uint32_t sx = swap ? y : x, sy = swap ? x : y;
        if (symmetry & 1) sx = width - 1 - sx;
        if (symmetry & 2) sy = height - 1 - sy;
        out[size_t(y) * outWidth + x] = cells[size_t(sy) * width + sx];
    }
    return out;
}
void symmetries() {
    const uint32_t width = 48, height = 80;
    const auto cells = noise(width, height, 4242);
    const auto original = classFingerprint(width, height, cells);
    require(original && original->hashes.size() >= 16, "symmetry fixture sufficient");
    auto index = fixture(); index.references[1].fingerprint = *original; index.generic.clear();
    for (int symmetry = 0; symmetry < 8; ++symmetry) {
        uint32_t w = 0, h = 0;
        const auto copy = transformed(cells, width, height, symmetry, w, h);
        require(symmetry == 0 || copy != cells, "transform changes raster");
        const auto fp = classFingerprint(w, h, copy);
        require(fp && fp->hashes == original->hashes && fp->windows == original->windows &&
            fp->sparseWindows == original->sparseWindows && fp->uniformWindows == original->uniformWindows, "symmetry-invariant fingerprint");
        const auto result = assessCopy(*fp, index);
        require(result && result->hardFail() && result->permille[1] == 1000, "rotated or mirrored copy detected");
    }
    const auto independent = classFingerprint(width, height, noise(width, height, 99));
    const auto clear = assessCopy(*independent, index);
    require(clear && clear->state == CopyState::Clear && clear->shared[1] < 16, "independent terrain not a copy");
}
void legacy() {
    const auto good = serializeCopyIndex(fixture());
    require(good.starts_with("i53-index/2 g4-natural-v1 d4-canonical-v1\n"), "current schema written");
    auto old = good.substr(0, good.size() - 72);
    old.replace(0, old.find('\n'), "i53-index/1 g4-natural-v1");
    Sha256 sum; sum.update(std::as_bytes(std::span(old.data(), old.size())));
    old += "sha256 " + Sha256::hex(sum.finish()) + '\n';
    require(!parseCopyIndex(old), "orientation-sensitive index refused even with valid checksum");
    const auto dir = std::filesystem::temp_directory_path();
    const auto oldPath = dir / "rme-d10-legacy.i53", goodPath = dir / "rme-d10-current.i53";
    { std::ofstream(oldPath, std::ios::binary) << old; std::ofstream(goodPath, std::ios::binary) << good; }
    require(copyIndexNeedsRebuild(oldPath) && !copyIndexNeedsRebuild(goodPath) && !copyIndexNeedsRebuild(dir / "rme-d10-missing.i53"),
        "rebuild hint only for the superseded format");
    require(!readCopyIndex(oldPath, "10.98", std::string(64, 'a'), std::string(64, 'b')), "legacy file refused");
    require(bool(readCopyIndex(goodPath, "10.98", std::string(64, 'a'), std::string(64, 'b'))), "current file accepted");
    std::filesystem::remove(oldPath); std::filesystem::remove(goodPath);
}
void corrupt() {
    auto index = fixture(); const auto good = serializeCopyIndex(index);
    require(!parseCopyIndex(good + "garbage"), "trailing data");
    require(!parseCopyIndex(good.substr(0, good.size() - 1)), "truncated data");
    auto bad = good; bad.replace(bad.find("M19"), 3, "M01"); require(!parseCopyIndex(bad), "holdout refused");
    bad = good; bad.replace(bad.find("modulo=4"), 8, "modulo=1"); require(!parseCopyIndex(bad), "different fingerprint params");
    index.references[1].alias = "M01";
    require(!parseCopyIndex(serializeCopyIndex(index)), "holdout refused even with valid checksum");
    index.references[1].alias = "M19";
    index.references[1].fingerprint.options.sampleModulo = 1;
    require(!parseCopyIndex(serializeCopyIndex(index)), "options refused even with valid checksum");
    index.references[1].fingerprint.options = {};
    bad = good.substr(0, good.size() - 72);
    bad.replace(bad.find("kept=100"), 8, "kept=18446744073709551615");
    Sha256 sum; sum.update(std::as_bytes(std::span(bad.data(), bad.size())));
    bad += "sha256 " + Sha256::hex(sum.finish()) + '\n';
    require(!parseCopyIndex(bad), "declared allocation bounded before legacy parser reserve");
    index.references[1].fingerprint = index.references[0].fingerprint;
    require(!parseCopyIndex(serializeCopyIndex(index)), "all generic means insufficient reference");
    require(copyWorkingBytes(UINT64_MAX) == UINT64_MAX, "memory estimate saturates");
}
}
int main() try { arithmetic(); projection(); symmetries(); legacy(); corrupt(); std::cout << "COPY_GUARD_PASS\n"; }
catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
