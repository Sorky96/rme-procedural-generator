// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/copy_guard.h"
#include "procedural/core/reference_header.h"
#include "procedural/core/style_learning.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <new>
#include <sstream>

namespace rme::procedural {
namespace {
// The hash primitives match G4 similarity (kept private there); I53 combines them into a
// window value invariant under the eight symmetries of the square.
constexpr uint64_t base1 = 0x9E3779B97F4A7C15ull, base2 = 0xC2B2AE3D27D4EB4Full;
constexpr uint64_t emptyCell = 0x51ED270B0B9E6A4Dull;
constexpr uint64_t mix(uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}
constexpr uint64_t nameHash(std::string_view name) noexcept {
    uint64_t h = 0xCBF29CE484222325ull;
    for (const char c : name) h = (h ^ static_cast<uint8_t>(c)) * 0x100000001B3ull;
    return mix(h);
}
constexpr uint64_t power(uint64_t base, uint32_t exponent) noexcept {
    uint64_t result = 1;
    for (uint32_t i = 0; i < exponent; ++i) result *= base;
    return result;
}
constexpr uint64_t geometricSum(uint64_t base, uint32_t count) noexcept {
    uint64_t sum = 0, term = 1;
    for (uint32_t i = 0; i < count; ++i) { sum += term; term *= base; }
    return sum;
}
// Multiplicative inverse modulo 2^64 of an odd base (Newton iteration).
constexpr uint64_t inverse(uint64_t base) noexcept {
    uint64_t x = base;
    for (int i = 0; i < 6; ++i) x *= 2 - base * x;
    return x;
}
static_assert(base1 * inverse(base1) == 1 && base2 * inverse(base2) == 1);
std::string checksum(std::string_view text) {
    Sha256 hash;
    hash.update(std::as_bytes(std::span(text.data(), text.size())));
    return Sha256::hex(hash.finish());
}
bool sha(std::string_view value) {
    return value.size() == 64 && std::ranges::all_of(value, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
// Recomputes generic windows and the estimate; false when a reference keeps too few windows.
bool finish(CopyIndex& index) {
    index.generic.clear();
    uint64_t hashes = 0;
    for (const auto& r : index.references) hashes += r.fingerprint.hashes.size();
    if (index.references.size() >= 2) {
        index.generic = index.references[0].fingerprint.hashes;
        for (size_t r = 1; r < index.references.size() && !index.generic.empty(); ++r) {
            std::vector<uint64_t> next;
            std::ranges::set_intersection(index.generic, index.references[r].fingerprint.hashes, std::back_inserter(next));
            index.generic = std::move(next);
        }
    }
    for (const auto& r : index.references)
        if (r.fingerprint.hashes.size() - index.generic.size() < minCopyReferenceWindows) return false;
    index.estimatedBytes = 65536 + 16 * (hashes + index.generic.size());
    return true;
}
bool valid(const Fingerprint& f) {
    return f.options == FingerprintOptions{} && std::ranges::is_sorted(f.hashes) &&
        std::adjacent_find(f.hashes.begin(), f.hashes.end()) == f.hashes.end() &&
        f.sparseWindows <= f.windows && f.candidateWindows <= f.windows - f.sparseWindows &&
        f.uniformWindows <= f.candidateWindows && f.hashes.size() <= f.candidateWindows - f.uniformWindows;
}
}
const char* copyStateName(CopyState state) noexcept {
    switch (state) {
        case CopyState::NotEvaluated: return "not-evaluated (no index)";
        case CopyState::TooSmall: return "not-evaluated (too few windows)";
        case CopyState::Clear: return "evaluated (below copy thresholds)";
        case CopyState::Copy: return "copy thresholds met; Place blocked";
    }
    return "not-evaluated";
}
std::expected<Fingerprint, SimilarityError> classFingerprint(uint32_t width, uint32_t height,
    std::span<const uint8_t> classes, std::stop_token stop) try {
    if (!width || !height || width > INT32_MAX || height > INT32_MAX || uint64_t(width) * height != classes.size())
        return std::unexpected(SimilarityError::BadFormat);
    if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
    if (std::ranges::any_of(classes, [](uint8_t c) { return c >= naturalKindCount && c != 255; }))
        return std::unexpected(SimilarityError::BadFormat);
    const FingerprintOptions options;
    const uint32_t window = options.window;
    Fingerprint result;
    result.options = options;
    std::array<uint64_t, 256> value;
    value.fill(emptyCell);
    for (size_t k = 0; k < naturalKindCount; ++k) value[k] = nameHash(styleKindName(StyleKind(k)));
    const uint64_t uniformFactor = geometricSum(base1, window) * geometricSum(base2, window);
    std::array<uint64_t, naturalKindCount + 1> uniform;
    for (size_t k = 0; k < naturalKindCount; ++k) uniform[k] = mix(value[k] * uniformFactor);
    uniform[naturalKindCount] = mix(emptyCell * uniformFactor);
    std::ranges::sort(uniform);
    if (width < window || height < window) return result;

    // Each of the eight square symmetries maps a window's cells onto the same polynomial with
    // one base per original axis (which axis carries base1) and an ascending or descending
    // exponent along each axis. Kind k is the horizontal half: base1/base2, descending/ascending.
    struct Base { uint64_t b, inv, top, full; };
    auto makeBase = [&](uint64_t b) { return Base{b, inverse(b), power(b, window - 1), power(b, window)}; };
    const std::array<Base, 2> bases{makeBase(base1), makeBase(base2)};
    const uint32_t columns = width - window + 1;
    const uint64_t minPresent = (uint64_t(window) * window * options.minPresentPercent + 99) / 100;
    std::vector<uint64_t> ring(size_t(4) * window * columns), descending(size_t(4) * columns), ascending(size_t(4) * columns);
    std::vector<uint64_t> rowHash(size_t(4) * columns), rowValue(width);
    std::vector<uint32_t> present(width);
    for (uint32_t y = 0; y < height; ++y) {
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        const uint8_t* row = classes.data() + size_t(y) * width;
        for (uint32_t x = 0; x < width; ++x) rowValue[x] = value[row[x]];
        for (size_t k = 0; k < 4; ++k) {
            const Base& h = bases[k / 2];
            uint64_t* out = rowHash.data() + k * columns;
            uint64_t acc = 0;
            if (k % 2 == 0) {
                for (uint32_t i = 0; i < window; ++i) acc = acc * h.b + rowValue[i];
                out[0] = acc;
                for (uint32_t x = 1; x < columns; ++x) out[x] = acc = acc * h.b - rowValue[x - 1] * h.full + rowValue[x + window - 1];
            } else {
                for (uint32_t i = window; i-- > 0;) acc = acc * h.b + rowValue[i];
                out[0] = acc;
                for (uint32_t x = 1; x < columns; ++x) out[x] = acc = (acc - rowValue[x - 1]) * h.inv + rowValue[x + window - 1] * h.top;
            }
        }
        for (size_t k = 0; k < 4; ++k) {
            const Base& v = bases[1 - k / 2];
            const uint64_t entering = y < window ? power(v.b, y) : v.top;
            uint64_t* slot = ring.data() + (k * window + y % window) * columns;
            const uint64_t* in = rowHash.data() + k * columns;
            uint64_t* d = descending.data() + k * columns;
            uint64_t* a = ascending.data() + k * columns;
            for (uint32_t x = 0; x < columns; ++x) {
                if (y < window) {
                    d[x] = d[x] * v.b + in[x];
                    a[x] += in[x] * entering;
                } else {
                    d[x] = d[x] * v.b - slot[x] * v.full + in[x];
                    a[x] = (a[x] - slot[x]) * v.inv + in[x] * entering;
                }
                slot[x] = in[x];
            }
        }
        for (uint32_t x = 0; x < width; ++x) {
            present[x] += row[x] != 255;
            if (y >= window) present[x] -= classes[size_t(y - window) * width + x] != 255;
        }
        if (y + 1 < window) continue;
        uint64_t sum = 0;
        for (uint32_t i = 0; i < window; ++i) sum += present[i];
        for (uint32_t x = 0; x < columns; ++x) {
            if (x > 0) { sum += present[x + window - 1]; sum -= present[x - 1]; }
            ++result.windows;
            if (sum < minPresent) { ++result.sparseWindows; continue; }
            uint64_t canonical = UINT64_MAX;
            for (size_t k = 0; k < 4; ++k)
                canonical = std::min({canonical, descending[k * columns + x], ascending[k * columns + x]});
            const uint64_t hash = mix(canonical);
            if (hash % options.sampleModulo != 0) continue;
            ++result.candidateWindows;
            if (std::ranges::binary_search(uniform, hash)) { ++result.uniformWindows; continue; }
            result.hashes.push_back(hash);
        }
    }
    std::ranges::sort(result.hashes);
    result.hashes.erase(std::unique(result.hashes.begin(), result.hashes.end()), result.hashes.end());
    return result;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyAssessment, SimilarityError> assessCopy(const Fingerprint& candidate,
    const CopyIndex& index, std::stop_token stop) try {
    if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
    if (!valid(candidate)) return std::unexpected(SimilarityError::BadFormat);
    CopyAssessment result;
    result.before = candidate.hashes.size();
    Fingerprint filtered = candidate;
    std::erase_if(filtered.hashes, [&](uint64_t h) { return std::ranges::binary_search(index.generic, h); });
    if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
    result.after = filtered.hashes.size();
    bool enough = !index.references.empty(), copied = false;
    const size_t count = index.references.size();
    result.referenceBefore.resize(count); result.referenceAfter.resize(count);
    result.shared.resize(count); result.permille.resize(count);
    for (size_t r = 0; r < count; ++r) {
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        const auto& reference = index.references[r].fingerprint;
        if (!valid(reference)) return std::unexpected(SimilarityError::BadFormat);
        result.referenceBefore[r] = reference.hashes.size();
        if (index.generic.size() > reference.hashes.size()) return std::unexpected(SimilarityError::BadFormat);
        result.referenceAfter[r] = reference.hashes.size() - index.generic.size();
        enough &= result.referenceAfter[r] >= minCopyReferenceWindows;
        const auto overlap = compareFingerprints(filtered, reference); // filtered candidate contains no generic hashes
        if (stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        result.shared[r] = overlap.shared;
        result.permille[r] = overlap.aInBPermille();
        copied |= overlap.shared >= 16 && result.permille[r] >= 100;
    }
    if (!enough) return std::unexpected(SimilarityError::BadFormat);
    result.state = result.after < 16 ? CopyState::TooSmall : copied ? CopyState::Copy : CopyState::Clear;
    return result;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<Fingerprint, SimilarityError> surfaceClassFingerprint(const SurfaceGrid& surface, std::stop_token stop) try {
    if (!surface.valid()) return std::unexpected(SimilarityError::BadFormat);
    std::vector<uint8_t> classes(surface.cells.size(), 255);
    for (size_t i = 0; i < classes.size(); ++i) {
        if ((i % surface.region.width) == 0 && stop.stop_requested()) return std::unexpected(SimilarityError::Cancelled);
        const auto kind = styleKindOf(surface.cells[i]);
        if (size_t(kind) < naturalKindCount) classes[i] = uint8_t(kind);
    }
    return classFingerprint(surface.region.width, surface.region.height, classes, stop);
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyAssessment, SimilarityError> assessCopy(const SurfaceGrid& surface,
    const CopyIndex& index, std::stop_token stop) {
    const auto fp = surfaceClassFingerprint(surface, stop);
    if (!fp) return std::unexpected(fp.error());
    return assessCopy(*fp, index, stop);
}

std::string serializeCopyIndex(const CopyIndex& index) {
    auto text = std::format("{} {} {}\n{} {} {}\nreferences {}\n", copyIndexSchema, copyProjection, copyFingerprintKind,
        index.client, index.otbSha256, index.datSha256, index.references.size());
    for (const auto& r : index.references) {
        const auto fp = serializeFingerprint(r.fingerprint);
        text += std::format("{} {} {} {} {}\n", r.alias, r.sourceSha256, r.semanticSha256, r.keymapSha256, fp.size());
        text += fp;
    }
    return text + "sha256 " + checksum(text) + '\n';
}
std::expected<CopyIndex, SimilarityError> parseCopyIndex(std::string_view text) try {
    if (text.size() > (64ULL << 20)) return std::unexpected(SimilarityError::BadFormat);
    if (text.size() < 72 || text.substr(text.size() - 72) != "sha256 " + checksum(text.substr(0, text.size() - 72)) + '\n')
        return std::unexpected(SimilarityError::BadFormat);
    text.remove_suffix(72);
    CopyIndex index;
    auto line = [&]() {
        const auto end = text.find('\n');
        if (end == text.npos) return std::string{};
        std::string value(text.substr(0, end)); text.remove_prefix(end + 1); return value;
    };
    const auto first = line();
    if (first == std::format("{} {} {}", copyIndexSchema, copyProjection, copyFingerprintKind)) index.schema = copyIndexSchema;
    else if (first == std::format("{} {} {}", copyIndexSchemaV2, copyProjection, copyFingerprintKind)) index.schema = copyIndexSchemaV2;
    else return std::unexpected(SimilarityError::BadFormat);
    const bool legacy = index.schema == copyIndexSchemaV2;
    std::istringstream header(line());
    std::string extra;
    if (!(header >> index.client >> index.otbSha256 >> index.datSha256) || header >> extra ||
        !sha(index.otbSha256) || !sha(index.datSha256)) return std::unexpected(SimilarityError::BadFormat);
    size_t count = 2;
    if (!legacy) {
        std::istringstream row(line());
        std::string word;
        if (!(row >> word >> count) || row >> extra || word != "references" || count < 1 || count > maxCopyReferences)
            return std::unexpected(SimilarityError::BadFormat);
    }
    index.references.resize(count);
    for (size_t i = 0; i < count; ++i) {
        auto& r = index.references[i];
        uint64_t length = 0;
        std::istringstream row(line());
        if (!(row >> r.alias >> r.sourceSha256 >> r.semanticSha256 >> r.keymapSha256 >> length) || row >> extra ||
            (legacy ? r.alias != (i == 0 ? "M06" : "M19") : !validCopyAlias(r.alias)) || !sha(r.sourceSha256) || !sha(r.semanticSha256) ||
            !sha(r.keymapSha256) || length > text.size()) return std::unexpected(SimilarityError::BadFormat);
        const auto payload = text.substr(0, size_t(length));
        // Bound the legacy fingerprint parser's declared reserve BEFORE it allocates.
        // Canonical hashes occupy 17 bytes each; a forged kept count must not escape
        // the index's joint parsing allowance, even with a valid payload checksum.
        auto counts = payload.substr(std::min(payload.size(), payload.find('\n') + 1));
        counts = counts.substr(0, counts.find('\n'));
        const auto at = counts.find("kept=");
        uint64_t kept = 0;
        if (at == counts.npos) return std::unexpected(SimilarityError::BadFormat);
        const auto parsed = std::from_chars(counts.data() + at + 5, counts.data() + counts.size(), kept);
        if (parsed.ec != std::errc{} || kept > payload.size() / 17) return std::unexpected(SimilarityError::BadFormat);
        auto fp = parseFingerprint(payload);
        if (!fp || !valid(*fp) || serializeFingerprint(*fp) != payload) return std::unexpected(SimilarityError::BadFormat);
        r.fingerprint = std::move(*fp);
        text.remove_prefix(size_t(length));
    }
    if (!text.empty()) return std::unexpected(SimilarityError::BadFormat);
    if (legacy && index.references[0].keymapSha256 != index.references[1].keymapSha256)
        return std::unexpected(SimilarityError::BadFormat);
    for (size_t i = 0; i < count; ++i)
        for (size_t j = 0; j < i; ++j)
            if (index.references[i].alias == index.references[j].alias ||
                index.references[i].sourceSha256 == index.references[j].sourceSha256) return std::unexpected(SimilarityError::BadFormat);
    if (!finish(index)) return std::unexpected(SimilarityError::BadFormat);
    return index;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::expected<CopyIndex, SimilarityError> readCopyIndex(const std::filesystem::path& path,
    std::string_view client, std::string_view otb, std::string_view dat, uint64_t maxBytes) try {
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > (64ULL << 20) || bytes * 16 + 65536 > maxBytes) return std::unexpected(SimilarityError::BadFormat);
    std::ifstream file(path, std::ios::binary);
    std::string text(size_t(bytes), '\0');
    if (!file.read(text.data(), std::streamsize(bytes)) || file.peek() != EOF) return std::unexpected(SimilarityError::BadFormat);
    auto index = parseCopyIndex(text);
    if (!index || index->client != client || index->otbSha256 != otb || index->datSha256 != dat)
        return std::unexpected(SimilarityError::BadFormat);
    // Includes bounded file parsing peak, even though the text is released on return.
    index->estimatedBytes = std::max(index->estimatedBytes, bytes * 16 + 65536);
    return index;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }
bool copyIndexNeedsRebuild(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::string first(32, '\0');
    file.read(first.data(), std::streamsize(first.size()));
    first.resize(size_t(file.gcount()));
    return first.starts_with("i53-index/1 ");
}
bool validCopyAlias(std::string_view alias) { return validLearnedProfileName(alias); }
const char* copyEditErrorText(CopyEditError error) noexcept {
    switch (error) {
        case CopyEditError::BadAlias: return "Use 1-64 letters, digits, '-' or '_' for the reference name.";
        case CopyEditError::DuplicateAlias: return "The copy index already has a reference with this name.";
        case CopyEditError::DuplicateSample: return "The copy index already holds this exact sample.";
        case CopyEditError::TooMany: return "A copy index holds at most 16 references. Choose another index.";
        case CopyEditError::TooSmall: return "The sample has too few distinct 16 x 16 windows to protect. Read a larger area.";
        case CopyEditError::WrongClient: return "The copy index belongs to another client or other client assets.";
        case CopyEditError::AllocationFailed: return "Not enough memory to update the copy index.";
    }
    return "The copy index cannot be updated.";
}
std::expected<CopyIndex, CopyEditError> addCopyReference(CopyIndex index, CopyReference reference) try {
    if (!validCopyAlias(reference.alias)) return std::unexpected(CopyEditError::BadAlias);
    if (!sha(reference.sourceSha256) || !sha(reference.semanticSha256) || !sha(reference.keymapSha256) || !valid(reference.fingerprint))
        return std::unexpected(CopyEditError::TooSmall);
    if (index.references.size() >= maxCopyReferences) return std::unexpected(CopyEditError::TooMany);
    for (const auto& r : index.references) {
        if (r.alias == reference.alias) return std::unexpected(CopyEditError::DuplicateAlias);
        if (r.sourceSha256 == reference.sourceSha256) return std::unexpected(CopyEditError::DuplicateSample);
    }
    index.references.push_back(std::move(reference));
    index.schema = copyIndexSchema;
    if (!finish(index)) return std::unexpected(CopyEditError::TooSmall);
    return index;
} catch (const std::bad_alloc&) { return std::unexpected(CopyEditError::AllocationFailed); }

std::expected<std::vector<uint8_t>, SimilarityError> sampleClassRaster(const SemanticMap& sample,
    std::span<const KeyMapEntry> mapping) try {
    if (sample.floors().size() != 1) return std::unexpected(SimilarityError::BadFormat);
    const auto& floor = sample.floors().front();
    std::array<uint8_t, 65536> classOfKey;
    classOfKey.fill(255);
    const auto& keys = sample.groundKeys();
    for (size_t k = 1; k < keys.size() && k < classOfKey.size(); ++k)
        for (const auto& entry : mapping)
            if (entry.key == keys[k]) {
                if (size_t(entry.kind) < naturalKindCount) classOfKey[k] = uint8_t(entry.kind);
                break;
            }
    std::vector<uint8_t> raster(floor.ground.size(), 255);
    for (size_t i = 0; i < raster.size(); ++i)
        if (floor.traits[i] & bit(Trait::Present)) raster[i] = classOfKey[floor.ground[i]];
    return raster;
} catch (const std::bad_alloc&) { return std::unexpected(SimilarityError::AllocationFailed); }

std::string fileSha256(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    Sha256 hash;
    std::array<char, 65536> chunk;
    while (file) {
        file.read(chunk.data(), std::streamsize(chunk.size()));
        if (const auto got = size_t(file.gcount())) hash.update(std::as_bytes(std::span(chunk.data(), got)));
    }
    return file.eof() ? Sha256::hex(hash.finish()) : std::string{};
}

std::expected<void, std::string> saveCopyIndex(const std::filesystem::path& directory, std::string_view name,
    const CopyIndex& index, std::string_view expectedSha256) {
    if (!validCopyAlias(name)) return std::unexpected("Use 1-64 letters, digits, '-' or '_' for the index name; no paths or device names.");
    std::string text;
    try { text = serializeCopyIndex(index); }
    catch (const std::bad_alloc&) { return std::unexpected("Not enough memory to write the copy index."); }
    if (!parseCopyIndex(text)) return std::unexpected("The copy index is invalid; nothing saved.");
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return std::unexpected("Cannot create the local profile directory.");
    const auto path = directory / (std::string(name) + ".i53");
    const auto lock = directory / ".copy-index-save.lock";
    if (!std::filesystem::create_directory(lock, ec)) return std::unexpected("The profile folder is busy. Retry after the other save finishes.");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove_all(path, e); } } cleanup{lock};
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) return std::unexpected("Cannot inspect the copy index file.");
    if (expectedSha256.empty() ? exists : (!exists || fileSha256(path) != expectedSha256))
        return std::unexpected("The copy index changed or was created meanwhile; nothing saved. Choose the index again.");
    const auto temporary = lock / "pending";
    {
        std::ofstream out(temporary, std::ios::binary);
        out.write(text.data(), std::streamsize(text.size()));
        out.flush();
        if (!out) return std::unexpected("Cannot write the copy index; previous file unchanged.");
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) return std::unexpected("Cannot replace the copy index; previous file unchanged.");
    return {};
}

std::vector<CopyIndexFile> listCopyIndices(const std::filesystem::path& directory,
    std::string_view client, std::string_view otb, std::string_view dat) {
    std::vector<CopyIndexFile> files;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end && files.size() < 64; it.increment(ec)) {
        if (it->path().extension() != ".i53" || !it->is_regular_file(ec)) continue;
        CopyIndexFile file{it->path(), it->path().stem().string(), {}, 0};
        if (copyIndexNeedsRebuild(file.path)) file.problem = "old format, rebuild it";
        else if (auto index = readCopyIndex(file.path, client, otb, dat)) file.references = index->references.size();
        else file.problem = index.error() == SimilarityError::AllocationFailed ? "not enough memory" : "unreadable or for another client";
        files.push_back(std::move(file));
    }
    std::ranges::sort(files, {}, &CopyIndexFile::name);
    return files;
}
} // namespace rme::procedural
