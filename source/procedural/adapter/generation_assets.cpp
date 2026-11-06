// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/main.h"
#include "procedural/adapter/generation_assets.h"

#include "app/managers/version_manager.h"
#include "brushes/brush.h"
#include "brushes/ground/auto_border.h"
#include "brushes/ground/ground_brush.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/core/game_sprite.h"
#include "rendering/core/graphics.h"
#include "ui/gui.h"

#include <climits>
#include <string_view>
#include <wx/thread.h>

namespace rme::procedural {

// Friend of GroundBrush (declared there) so it can read the protected terrain fields, including
// those inherited from TerrainBrush, through a GroundBrush object. Read-only.
struct GenerationAssetReader {
	enum class Status { Ok, Invalid, Cancelled };

	// FNV-1a over explicit little-endian field widths; no object representations are hashed.
	class Hasher {
	public:
		explicit Hasher(const std::stop_token& stop) noexcept :
			stop_(stop) { }

		void field(uint64_t value, unsigned bytes) noexcept {
			for (unsigned i = 0; i < bytes; ++i) {
				hash_ = (hash_ ^ uint8_t(value)) * 1099511628211ULL;
				value >>= 8;
			}
		}
		void u8(uint64_t value) noexcept { field(value, 1); }
		void u16(uint64_t value) noexcept { field(value, 2); }
		void u32(uint64_t value) noexcept { field(value, 4); }
		void u64(uint64_t value) noexcept { field(value, 8); }
		void tag(uint32_t domain) noexcept { u32(domain); }
		// Process-local identity token; the pointer is never dereferenced here.
		void token(const void* pointer) noexcept { u64(uint64_t(reinterpret_cast<uintptr_t>(pointer))); }

		// Counts one unit of work and polls the stop token every 4096 units.
		[[nodiscard]] bool tick() noexcept {
			return (++steps_ % 4096 != 0) || !stop_.stop_requested();
		}
		[[nodiscard]] bool stopped() const noexcept { return stop_.stop_requested(); }

		[[nodiscard]] Status text(std::string_view value) noexcept {
			u64(value.size());
			for (const char ch : value) {
				if (!tick()) return Status::Cancelled;
				u8(uint8_t(ch));
			}
			return Status::Ok;
		}

		[[nodiscard]] uint64_t value() const noexcept { return hash_; }

	private:
		const std::stop_token& stop_;
		uint64_t hash_ = 14695981039346656037ULL;
		uint64_t steps_ = 0;
	};

	static void hashAutoBorderValues(Hasher& h, const AutoBorder& border) noexcept {
		h.u32(border.id);
		h.u16(border.group);
		h.u8(border.ground ? 1 : 0);
		h.u32(uint32_t(border.tiles.size()));
		for (const uint32_t tile : border.tiles) h.u32(tile);
	}

	// A raw AutoBorder link is valid when null, equal to its holder's owned border, or one of the
	// current global borders. Only pointer values are compared; the link is never dereferenced.
	static Status checkLink(Hasher& h, const AutoBorder* link, const AutoBorder* owned) noexcept {
		if (!link || link == owned) return Status::Ok;
		for (const auto& [key, border] : g_brushes.getBorders()) {
			if (!h.tick()) return Status::Cancelled;
			if (border.get() == link) return Status::Ok;
		}
		return Status::Invalid;
	}

	// Link token, ownership token and owned values; global border values are hashed separately.
	static Status hashBorderLink(Hasher& h, const AutoBorder* link, const AutoBorder* owned) noexcept {
		if (const Status status = checkLink(h, link, owned); status != Status::Ok) return status;
		h.token(link);
		h.token(owned);
		if (owned) hashAutoBorderValues(h, *owned);
		return Status::Ok;
	}

	static Status hashGround(Hasher& h, const GroundBrush& brush) noexcept {
		h.tag(0x47524e44); // "GRND"
		if (const Status status = h.text(brush.name); status != Status::Ok) return status;
		h.u64(brush.friends.size());
		for (const uint32_t id : brush.friends) {
			if (!h.tick()) return Status::Cancelled;
			h.u32(id);
		}
		h.u8(brush.hate_friends ? 1 : 0);
		h.u16(brush.look_id);
		h.u32(uint32_t(brush.z_order));
		h.u8(brush.has_zilch_outer_border ? 1 : 0);
		h.u8(brush.has_zilch_inner_border ? 1 : 0);
		h.u8(brush.has_outer_border ? 1 : 0);
		h.u8(brush.has_inner_border ? 1 : 0);
		h.u8(brush.optional_border ? 1 : 0);
		h.u8(brush.use_only_optional ? 1 : 0);
		h.u8(brush.randomize ? 1 : 0);
		h.u32(uint32_t(brush.total_chance));
		h.u64(brush.border_items.size());
		for (const auto& item : brush.border_items) {
			if (!h.tick()) return Status::Cancelled;
			h.u16(item.id);
			h.u32(uint32_t(item.chance));
		}

		// Optional border is included even when the terrain path does not use it.
		h.tag(0x4f505442); // "OPTB"
		if (const Status status = hashBorderLink(h, brush.optional_border, brush.owned_optional_border.get()); status != Status::Ok) {
			return status;
		}

		h.tag(0x424c4b53); // "BLKS"
		h.u64(brush.borders.size());
		for (const auto& block : brush.borders) {
			if (!h.tick()) return Status::Cancelled;
			if (!block) return Status::Invalid;
			h.u8(block->outer ? 1 : 0);
			h.u8(block->super ? 1 : 0);
			h.u32(block->to);
			if (const Status status = hashBorderLink(h, block->autoborder, block->owned_autoborder.get()); status != Status::Ok) {
				return status;
			}
			h.u64(block->specific_cases.size());
			for (const auto& specific : block->specific_cases) {
				if (!h.tick()) return Status::Cancelled;
				if (!specific) return Status::Invalid;
				h.u64(specific->items_to_match.size());
				for (const uint16_t id : specific->items_to_match) {
					if (!h.tick()) return Status::Cancelled;
					h.u16(id);
				}
				h.u32(specific->match_group);
				h.u32(uint32_t(specific->group_match_alignment));
				h.u16(specific->to_replace_id);
				h.u16(specific->with_id);
				h.u8(specific->delete_all ? 1 : 0);
				h.u8(specific->keepBorder ? 1 : 0);
			}
		}
		return Status::Ok;
	}

	static Status hashSprite(Hasher& h, ClientItemId clientId) noexcept {
		h.tag(0x53505254); // "SPRT"
		if (clientId > ClientItemId(INT_MAX)) {
			h.u8(2); // Not addressable by the graphics lookup.
			return Status::Ok;
		}
		// Lookup only: no upload, decode, lazy metrics or copies.
		Sprite* sprite = g_gui.gfx.getSprite(int(clientId));
		if (!sprite) {
			h.u8(0);
			return Status::Ok;
		}
		const auto* game = dynamic_cast<const GameSprite*>(sprite);
		if (!game) return Status::Invalid;
		h.u8(1);
		h.token(game);
		h.u32(game->getId());
		h.u16(game->minimap_color);
		h.u8(game->has_light ? 1 : 0);
		h.u8(game->light.intensity);
		h.u8(game->light.color);
		h.u8(game->height);
		h.u8(game->width);
		h.u8(game->layers);
		h.u8(game->pattern_x);
		h.u8(game->pattern_y);
		h.u8(game->pattern_z);
		h.u8(game->frames);
		h.u32(game->numsprites);
		h.u16(game->draw_height);
		h.u16(game->drawoffset_x);
		h.u16(game->drawoffset_y);
		h.u64(game->sprite_ids.size());
		for (const uint32_t id : game->sprite_ids) {
			if (!h.tick()) return Status::Cancelled;
			h.u32(id);
		}
		return Status::Ok;
	}

	static Status hashDefinitions(Hasher& h) noexcept {
		h.tag(0x44454653); // "DEFS"
		h.u32(g_item_definitions.MajorVersion);
		h.u32(g_item_definitions.MinorVersion);
		h.u32(g_item_definitions.BuildNumber);
		h.u32(g_item_definitions.maxServerId());
		const auto ids = g_item_definitions.allIds();
		h.u64(ids.size());
		for (const ServerItemId id : ids) {
			if (!h.tick()) return Status::Cancelled;
			const auto view = g_item_definitions.get(id);
			if (!view || view.serverId() != id) return Status::Invalid;
			h.u32(id);
			h.u32(view.clientId());
			h.u32(uint32_t(view.group()));
			h.u32(uint32_t(view.type()));
			h.u64(view.flagMask());
			for (unsigned key = 0; key <= unsigned(ItemAttributeKey::BorderAlignment); ++key) {
				h.u64(uint64_t(view.attribute(ItemAttributeKey(key))));
			}
			for (const std::string_view text : { view.name(), view.editorSuffix(), view.description(), view.passiveMetadataJson() }) {
				if (const Status status = h.text(text); status != Status::Ok) return status;
			}
			h.token(view.editorData().brush);
			if (const Status status = hashSprite(h, view.clientId()); status != Status::Ok) return status;
		}
		return Status::Ok;
	}

	static Status hashBrushes(Hasher& h) noexcept {
		h.tag(0x42525348); // "BRSH"
		const auto& brushes = g_brushes.getMap();
		h.u64(brushes.size());
		for (const auto& [key, brush] : brushes) {
			if (!h.tick()) return Status::Cancelled;
			if (const Status status = h.text(key); status != Status::Ok) return status;
			if (!brush) return Status::Invalid;
			h.token(brush.get());
			h.u32(brush->getID());
			const auto* ground = dynamic_cast<const GroundBrush*>(brush.get());
			h.u8(ground ? 1 : 0);
			if (ground) {
				if (const Status status = hashGround(h, *ground); status != Status::Ok) return status;
			}
		}

		h.tag(0x42445253); // "BDRS"
		const auto& borders = g_brushes.getBorders();
		h.u64(borders.size());
		for (const auto& [key, border] : borders) {
			if (!h.tick()) return Status::Cancelled;
			if (!border) return Status::Invalid;
			h.u32(key);
			h.token(border.get());
			hashAutoBorderValues(h, *border);
		}

		h.tag(0x42545950); // "BTYP"
		for (const uint32_t type : GroundBrush::border_types) h.u32(type);
		return Status::Ok;
	}
};

std::expected<uint64_t, AssetFingerprintError> fingerprintGenerationAssets(std::stop_token stop) noexcept {
	if (!wxIsMainThread()) return std::unexpected(AssetFingerprintError::WrongThread);
	if (stop.stop_requested()) return std::unexpected(AssetFingerprintError::Cancelled);
	if (!g_version.IsVersionLoaded() || !g_version.getLoadedVersion()) return std::unexpected(AssetFingerprintError::NoClient);

	using Status = GenerationAssetReader::Status;
	GenerationAssetReader::Hasher h(stop);
	h.tag(0x47314153); // "G1AS"
	h.u64(g_version.GetAssetRevision());

	const auto toError = [](Status status) {
		return status == Status::Cancelled ? AssetFingerprintError::Cancelled : AssetFingerprintError::InvalidAssets;
	};
	if (const Status status = h.text(g_version.GetCurrentVersionIDView()); status != Status::Ok) return std::unexpected(toError(status));
	if (const Status status = GenerationAssetReader::hashDefinitions(h); status != Status::Ok) return std::unexpected(toError(status));
	if (const Status status = GenerationAssetReader::hashBrushes(h); status != Status::Ok) return std::unexpected(toError(status));

	if (h.stopped()) return std::unexpected(AssetFingerprintError::Cancelled);
	return h.value();
}

} // namespace rme::procedural
