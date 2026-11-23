// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/copy_guard.h"
#include "procedural/core/reference_header.h"
#include <algorithm>
#include <filesystem>
#include <format>
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
    index.references.resize(2);
    index.references[0] = {"M06", std::string(64, 'c'), std::string(64, 'd'), std::string(64, 'e'), values(0, 100)};
    index.references[1] = {"M19", std::string(64, 'f'), std::string(64, 'a'), std::string(64, 'e'), values(90, 100)};
    auto parsed = parseCopyIndex(serializeCopyIndex(index)); require(bool(parsed), "valid index"); return std::move(*parsed);
}
std::string sign(std::string body) {
    Sha256 sum; sum.update(std::as_bytes(std::span(body.data(), body.size())));
    return body + "sha256 " + Sha256::hex(sum.finish()) + '\n';
}
// The same index as written by g6_build_copy_index.py (i53-index/2: no reference count line).
std::string asV2(const std::string& v3) {
    auto body = v3.substr(0, v3.size() - 72);
    body.replace(0, body.find('\n'), "i53-index/2 g4-natural-v1 d4-canonical-v1");
    const auto count = body.find("references ");
    body.erase(count, body.find('\n', count) + 1 - count);
    return sign(body);
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
    require(good.starts_with("i53-index/3 g4-natural-v1 d4-canonical-v1\n"), "current schema written");
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
    const auto v2 = asV2(good);
    auto bad = v2; bad.replace(bad.find("M19"), 3, "M01"); require(!parseCopyIndex(bad), "holdout refused");
    bad = good; bad.replace(bad.find("modulo=4"), 8, "modulo=1"); require(!parseCopyIndex(bad), "different fingerprint params");
    bad = v2.substr(0, v2.size() - 72); bad.replace(bad.find("M19"), 3, "M01");
    require(!parseCopyIndex(sign(bad)), "i53-index/2 holdout refused even with valid checksum");
    index.references[1].alias = "M 1";
    require(!parseCopyIndex(serializeCopyIndex(index)), "bad alias refused even with valid checksum");
    index.references[1].alias = "M06";
    require(!parseCopyIndex(serializeCopyIndex(index)), "duplicate alias refused even with valid checksum");
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
// I57: i53-index/2 reads as before, any 1..16 named references, edits through addCopyReference.
void i57() {
    const auto v3 = fixture();
    const auto v2 = parseCopyIndex(asV2(serializeCopyIndex(v3)));
    require(v2 && v2->schema == copyIndexSchemaV2 && v3.schema == copyIndexSchema, "both schemas read");
    require(v2->generic == v3.generic && v2->estimatedBytes == v3.estimatedBytes, "same generic windows and estimate");
    for (const uint64_t first : {0ull, 50ull, 95ull, 500ull}) {
        const auto a = assessCopy(values(first, 40), *v2), b = assessCopy(values(first, 40), v3);
        require(a && b && *a == *b, "i53-index/2 and /3 give identical assessments");
    }
    const uint32_t width = 96, height = 80;
    const auto cells = noise(width, height, 777);
    const auto sample = classFingerprint(width, height, cells);
    require(sample && sample->hashes.size() >= 16, "sample fingerprint");
    CopyIndex one; one.client = "10.98"; one.otbSha256 = std::string(64, 'a'); one.datSha256 = std::string(64, 'b');
    const CopyReference mine{"my-island", std::string(64, '1'), std::string(64, '2'), std::string(64, '3'), *sample};
    auto added = addCopyReference(one, mine);
    require(added && added->references.size() == 1 && added->generic.empty(), "one reference, no generic windows");
    const auto reread = parseCopyIndex(serializeCopyIndex(*added));
    require(reread && reread->references.size() == 1 && reread->references[0].alias == "my-island", "one reference round trip");
    uint32_t w = 0, h = 0;
    const auto rotated = transformed(cells, width, height, 5, w, h);
    const auto copied = assessCopy(*classFingerprint(w, h, rotated), *reread);
    require(copied && copied->hardFail() && copied->shared.size() == 1, "rotated and mirrored copy of the sample blocked");
    const auto other = assessCopy(*classFingerprint(width, height, noise(width, height, 31)), *reread);
    require(other && other->state == CopyState::Clear, "different terrain passes");
    require(addCopyReference(*added, mine).error() == CopyEditError::DuplicateAlias, "duplicate alias");
    auto same = mine; same.alias = "again";
    require(addCopyReference(*added, same).error() == CopyEditError::DuplicateSample, "same sample twice");
    auto bad = mine; bad.alias = "../x";
    require(addCopyReference(*added, bad).error() == CopyEditError::BadAlias, "path as alias");
    bad.alias = "COM1";
    require(addCopyReference(*added, bad).error() == CopyEditError::BadAlias, "device name as alias");
    auto tiny = mine; tiny.alias = "tiny"; tiny.sourceSha256 = std::string(64, '4'); tiny.fingerprint = values(1000, 8);
    require(addCopyReference(*added, tiny).error() == CopyEditError::TooSmall, "sample with too few windows");
    auto full = *added;
    for (uint64_t r = 1; r < maxCopyReferences; ++r) {
        auto next = mine; next.alias = "ref" + std::to_string(r);
        next.sourceSha256 = std::string(62, 'a') + std::format("{:02}", r);
        next.fingerprint = values(10000 * r, 40);
        auto grown = addCopyReference(full, next); require(bool(grown), "up to 16 references"); full = std::move(*grown);
    }
    require(full.references.size() == maxCopyReferences && parseCopyIndex(serializeCopyIndex(full)), "16 references round trip");
    auto extra = mine; extra.alias = "extra"; extra.sourceSha256 = std::string(64, 'f'); extra.fingerprint = values(900000, 40);
    require(addCopyReference(full, extra).error() == CopyEditError::TooMany, "17th reference refused");
    const auto fullText = serializeCopyIndex(full);
    auto text = fullText.substr(0, fullText.size() - 72);
    text.replace(text.find("references 16"), 13, "references 17");
    require(!parseCopyIndex(sign(text)), "declared count above 16 refused");

    const FloorBounds bounds{7, 100, 200, 3, 2};
    auto map = SemanticMap::create(std::span(&bounds, 1), {64});
    require(bool(map), "sample map");
    const auto grass = map->internGroundKey("grass"), water = map->internGroundKey("sea"), road = map->internGroundKey("road");
    const auto mystery = map->internGroundKey("mystery");
    const auto present = bit(Trait::HasGround);
    require(map->set(100, 200, 7, *grass, present) && map->set(101, 200, 7, *water, present) &&
        map->set(102, 200, 7, *road, present) && map->set(100, 201, 7, *mystery, present) &&
        map->set(101, 201, 7, noGroundKey, 0), "sample cells");
    const std::vector<KeyMapEntry> mapping{{"grass", StyleKind::Grass, ""}, {"sea", StyleKind::Water, ""}, {"road", StyleKind::Other, ""}};
    const auto raster = sampleClassRaster(*map, mapping);
    require(raster && *raster == std::vector<uint8_t>{uint8_t(StyleKind::Grass), uint8_t(StyleKind::Water), 255, 255, 255, 255},
        "natural kinds kept; other kinds, unmapped key and missing tiles absent");

    const auto dir = std::filesystem::temp_directory_path() / "rme-i57-index";
    std::filesystem::remove_all(dir);
    require(!saveCopyIndex(dir, "../escape", *added, {}), "index name cannot be a path");
    require(bool(saveCopyIndex(dir, "mine", *added, {})), "new index saved");
    require(!saveCopyIndex(dir, "mine", *added, {}), "new index does not overwrite an existing file");
    const auto before = fileSha256(dir / "mine.i53");
    require(!saveCopyIndex(dir, "mine", full, std::string(64, '0')), "changed file is not overwritten");
    require(fileSha256(dir / "mine.i53") == before, "refused write leaves the file unchanged");
    require(bool(saveCopyIndex(dir, "mine", full, before)), "edit of an unchanged file saved");
    { std::ofstream(dir / "broken.i53", std::ios::binary) << "i53-index/3 garbage\n"; }
    const auto listed = listCopyIndices(dir, "10.98", std::string(64, 'a'), std::string(64, 'b'));
    require(listed.size() == 2 && listed[0].name == "broken" && !listed[0].problem.empty() &&
        listed[1].name == "mine" && listed[1].problem.empty() && listed[1].references == maxCopyReferences, "folder listing");
    const auto foreign = listCopyIndices(dir, "8.60", std::string(64, 'a'), std::string(64, 'b'));
    require(foreign.size() == 2 && !foreign[1].problem.empty(), "index of another client cannot be chosen");
    std::filesystem::remove_all(dir);
}
int main() try { arithmetic(); projection(); symmetries(); legacy(); corrupt(); i57(); std::cout << "COPY_GUARD_PASS\n"; }
catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
