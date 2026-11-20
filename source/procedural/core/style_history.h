// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "procedural/core/style_profile.h"
#include <filesystem>

namespace rme::procedural {
// Deliberately limited to controls supported by the Surface dialog. No brush/item IDs,
// mountain geometry, cave geometry or learned bands are modified by Adjust.
using StyleAdjustment = std::array<uint32_t, 8>;
inline constexpr const char* adjustmentScope =
    "Adjust saves: path length, building density, ruin %, camps, ford length, rock passes, smooth beaches, reach repair. Other settings and brushes are not saved.";
StyleAdjustment styleAdjustment(const SurfaceTuning& tuning);
void applyStyleAdjustment(SurfaceTuning& tuning, const StyleAdjustment& values);
bool validStyleAdjustment(const StyleAdjustment& values);
std::string styleIdentity(const StyleProfile& profile);

enum class FeedbackAction { Approve, Reject, Adjust };
struct StyleFeedback {
    std::string id;
    FeedbackAction action = FeedbackAction::Approve;
    std::string note;
    // Serialized profile supplies exact client/asset identities and built-in or profile tuning.
    StyleProfile settings;
    SurfaceTuning generation; // full settings, including controls outside Adjust's scope
    bool vegetation = false;
    StyleAdjustment adjustment {};
    std::string base, version, policy;
    uint32_t candidate = 0;
    uint64_t userSeed = 0, initialSeed = 0, actualSeed = 0;
    std::array<uint64_t, 5> fingerprints {};
    bool hardFail = false;
};
struct StyleVersion {
    std::string id, parent, base;
    StyleProfile profile;
    std::optional<StyleAdjustment> adjustment; // root retains legacy profile behaviour
};
struct StyleHistory {
    std::vector<StyleFeedback> feedback;
    std::vector<StyleVersion> versions;
    std::string active;
};

// A snapshot token detects external edits (including deletion). Commit uses a sibling lock
// directory and a flushed sibling temporary file, then atomic replacement. Failed writes leave
// both the previous on-disk file and this snapshot intact. Unknown schemas are never reset.
class StyleHistoryStore {
public:
    static std::expected<StyleHistoryStore, std::string> open(const std::filesystem::path& path);
    const StyleHistory& data() const { return data_; }
    std::expected<void, std::string> record(StyleFeedback feedback,
        const StyleProfile* base = nullptr, std::string_view parent = {});
    std::expected<void, std::string> activate(std::string_view id,
        std::string_view client, std::string_view otb, std::string_view dat);
private:
    std::expected<void, std::string> commit(StyleHistory next);
    std::filesystem::path path_;
    std::optional<std::string> token_;
    StyleHistory data_;
};
} // namespace rme::procedural
