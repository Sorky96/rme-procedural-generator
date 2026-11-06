// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <expected>
#include <stop_token>

namespace rme::procedural {

enum class AssetFingerprintError { WrongThread, NoClient, InvalidAssets, Cancelled };

// Process-local stamp of the current values of the loaded generation asset catalog, recomputed on
// every call. GUI thread only, synchronous, allocation-free, constant stack; never writes the
// catalog, selection, RNG, graphics or caches and never uploads, decodes or renders.
//
// Covered: VersionManager asset revision and client ID; item definition store versions, id order
// and every definition's identity, flags, all ItemAttributeKey values, text and editor brush link
// (pointer token only; doodad/collection/RAW links are not used by the G1 terrain path); the
// existing GameSprite for each client ID (metadata and sprite_ids, or its absence); the whole brush
// registry (key, identity, ID, ground tag) with every GroundBrush's terrain fields, border rules,
// optional border and specific cases; the global AutoBorder registry; GroundBrush::border_types.
//
// Pointers are hashed as identity tokens and never followed unless ownership or current global
// registry membership is proven first; an AutoBorder link that is neither owned by its holder nor a
// current global border is reported as InvalidAssets instead of being dereferenced.
//
// This is a conservative whole-catalog consistency stamp: unrelated covered edits invalidate, and
// restoring identical values and identities may validate again. It is not cryptographic, not
// persistent or stable across processes, not a history counter, not part of any deterministic
// output fingerprint, does not cover pixel/texture/atlas/animation runtime state, and is not a
// general sanitizer for arbitrary dangling ownership: the public catalog containers themselves must
// be valid.
std::expected<uint64_t, AssetFingerprintError> fingerprintGenerationAssets(std::stop_token stop = {}) noexcept;

} // namespace rme::procedural
