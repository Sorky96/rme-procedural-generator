// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_history.h"
#include "procedural/core/reference_header.h"
#include "procedural/core/style_profile_set.h"
#include "procedural/core/surface.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <set>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace rme::procedural {
namespace {
using Json = nlohmann::json;
#define SURFACE_FIELDS(X) \
    X(featureSize) X(lakePermille) X(minLakeCells) X(riverCount) X(minRiverLength) X(shoreWidth) \
    X(rockPermille) X(minRockCells) X(dirtPermille) X(transitionWidth) X(meander) X(pathLength) \
    X(buildingDensity) X(fordLength) X(rockPasses) X(shoreSmoothing) X(shoreRadius) X(repairReach) \
    X(ruinPercent) X(campCount) X(caveEntrances) X(plateauMinTop) X(plateauLevels) X(plateauMaxPercent) \
    X(caveFarExits) X(caveDoorCorridors) X(passes) X(buildingStoreys) X(upperFloorPercent) X(rockBlocks) \
    X(buildingDungeons) X(dungeonPercent)
Json surfaceJson(const SurfaceTuning& t) {
    Json j;
#define ENCODE_FIELD(name) j[#name]=t.name;
    SURFACE_FIELDS(ENCODE_FIELD)
#undef ENCODE_FIELD
    if(t.islandPermille)j["islandPermille"]=t.islandPermille; // optional: older histories have no sea
    if(t.dirtSize)j["dirtSize"]=t.dirtSize;
    if(t.cleanEdges)j["cleanEdges"]=t.cleanEdges;
    if(t.ruinFloorPercent)j["ruinFloorPercent"]=t.ruinFloorPercent;
    if(t.compoundPercent)j["compoundPercent"]=t.compoundPercent;
    j["caveEntranceStyle"]=static_cast<uint8_t>(t.caveEntranceStyle);return j;
}
uint64_t unsignedValue(const Json& j,uint64_t maximum=UINT64_MAX) {
    if(!j.is_number_unsigned() || j.get<uint64_t>()>maximum)throw std::runtime_error("Invalid unsigned history value.");
    return j.get<uint64_t>();
}
template<class T> void surfaceField(const Json& j,T& out) {
    if constexpr(std::is_same_v<T,bool>) { if(!j.is_boolean())throw std::runtime_error("Invalid history flag.");out=j.get<bool>(); }
    else out=static_cast<T>(unsignedValue(j,UINT32_MAX));
}
SurfaceTuning surfaceFrom(const Json& j) {
    SurfaceTuning t;
#define DECODE_FIELD(name) surfaceField(j.at(#name),t.name);
    SURFACE_FIELDS(DECODE_FIELD)
#undef DECODE_FIELD
    if(j.contains("islandPermille"))surfaceField(j.at("islandPermille"),t.islandPermille);
    if(j.contains("dirtSize"))surfaceField(j.at("dirtSize"),t.dirtSize);
    if(j.contains("cleanEdges"))surfaceField(j.at("cleanEdges"),t.cleanEdges);
    if(j.contains("ruinFloorPercent"))surfaceField(j.at("ruinFloorPercent"),t.ruinFloorPercent);
    if(j.contains("compoundPercent"))surfaceField(j.at("compoundPercent"),t.compoundPercent);
    t.caveEntranceStyle=static_cast<CaveEntranceStyle>(unsignedValue(j.at("caveEntranceStyle"),1));
    if(!validateSurface({{100,100,7,100,100},0,t},{65001,65001,16},{10000,UINT64_MAX,UINT64_MAX}))
        throw std::runtime_error("Invalid Surface settings.");
    return t;
}
#undef SURFACE_FIELDS
StyleAdjustment adjustmentFrom(const Json& j) {
    if(!j.is_array() || j.size()!=8)throw std::runtime_error("Invalid adjustment length.");
    StyleAdjustment out;
    for(size_t i=0;i<out.size();++i)out[i]=static_cast<uint32_t>(unsignedValue(j[i],UINT32_MAX));
    return out;
}
constexpr size_t maxBytes = 8u << 20, maxFeedback = 4096, maxVersions = 256;
constexpr const char* schema = "g6-style-history/1";
std::string digest(std::string_view text) {
    Sha256 hash; hash.update(std::as_bytes(std::span(text.data(), text.size())));
    return Sha256::hex(hash.finish());
}
std::string uniqueId() {
    std::random_device random;
    std::string entropy = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    for (int i = 0; i < 8; ++i) entropy += ":" + std::to_string(random());
    return digest(entropy);
}
const char* actionName(FeedbackAction action) {
    switch (action) {
    case FeedbackAction::Approve: return "approve";
    case FeedbackAction::Reject: return "reject";
    case FeedbackAction::Adjust: return "adjust";
    }
    throw std::runtime_error("Unknown feedback action.");
}
StyleProfile profileFrom(const Json& value) {
    const auto p = parseProfile(value.get<std::string>());
    if (!p) throw std::runtime_error("Invalid profile snapshot.");
    return *p;
}
std::string profileText(const StyleProfile& p) {
    const auto text = serializeProfile(p);
    if (!text) throw std::runtime_error("Invalid profile snapshot.");
    return *text;
}
bool usable(const StyleProfile& p) {
    const NamedProfileText text{"snapshot", profileText(p)};
    return offerProfiles(std::span(&text, 1), p.client, p.otbSha256, p.datSha256).offered.size() == 1;
}
Json encode(const StyleHistory& h) {
    Json out{{"schema",schema},{"active",h.active},{"versions",Json::array()},{"feedback",Json::array()}};
    for (const auto& v : h.versions) out["versions"].push_back({{"id",v.id},{"parent",v.parent},
        {"base",v.base},{"profile",profileText(v.profile)},
        {"adjustment",v.adjustment ? Json(*v.adjustment) : Json(nullptr)}});
    for (const auto& f : h.feedback) out["feedback"].push_back({{"id",f.id},{"action",actionName(f.action)},
        {"note",f.note},{"settings",profileText(f.settings)},{"surface_settings",surfaceJson(f.generation)},
        {"vegetation",f.vegetation},{"adjustment",f.adjustment},
        {"base",f.base},{"version",f.version},{"policy",f.policy},{"candidate",f.candidate},
        {"user_seed",f.userSeed},{"initial_seed",f.initialSeed},{"actual_seed",f.actualSeed},
        {"floor_fingerprints",f.fingerprints},{"hard_fail",f.hardFail}});
    return out;
}
StyleHistory decode(const Json& j) {
    if (j.at("schema") != schema) throw std::runtime_error("Unsupported history format; file preserved.");
    if (!j.at("versions").is_array() || !j.at("feedback").is_array() ||
        j.at("versions").size() > maxVersions || j.at("feedback").size() > maxFeedback)
        throw std::runtime_error("History exceeds record limits.");
    StyleHistory h; h.active = j.at("active").get<std::string>();
    std::set<std::string> ids;
    for (const auto& item : j.at("versions")) {
        StyleVersion v{item.at("id"),item.at("parent"),item.at("base"),profileFrom(item.at("profile"))};
        if (!item.at("adjustment").is_null()) v.adjustment = adjustmentFrom(item.at("adjustment"));
        if (v.id.size()!=64 || v.base.size()!=64 || !ids.insert(v.id).second || !usable(v.profile) ||
            (v.adjustment && !validStyleAdjustment(*v.adjustment))) throw std::runtime_error("Invalid history version.");
        if (v.parent.empty()) {
            if (v.id != v.base || v.base != styleIdentity(v.profile) || v.adjustment)
                throw std::runtime_error("Invalid history root.");
        } else {
            const auto parent = std::ranges::find(h.versions, v.parent, &StyleVersion::id);
            const auto root = std::ranges::find(h.versions, v.base, &StyleVersion::id);
            if (parent==h.versions.end() || root==h.versions.end() || parent->base!=v.base ||
                root->profile!=v.profile || !v.adjustment) throw std::runtime_error("Invalid history ancestry.");
        }
        h.versions.push_back(std::move(v));
    }
    if (!h.active.empty() && !ids.contains(h.active)) throw std::runtime_error("Unknown active version.");
    for (const auto& item : j.at("feedback")) {
        StyleFeedback f;
        f.id=item.at("id"); f.note=item.at("note"); f.settings=profileFrom(item.at("settings"));
        f.generation=surfaceFrom(item.at("surface_settings")); f.vegetation=item.at("vegetation").get<bool>();
        f.adjustment=adjustmentFrom(item.at("adjustment"));
        f.base=item.at("base"); f.version=item.at("version"); f.policy=item.at("policy");
        f.candidate=static_cast<uint32_t>(unsignedValue(item.at("candidate"),15)); f.userSeed=unsignedValue(item.at("user_seed"));
        f.initialSeed=unsignedValue(item.at("initial_seed")); f.actualSeed=unsignedValue(item.at("actual_seed"));
        f.fingerprints=item.at("floor_fingerprints").get<std::array<uint64_t,5>>(); f.hardFail=item.at("hard_fail");
        if(item.at("floor_fingerprints").size()!=5 || item.at("adjustment").size()!=8)
            throw std::runtime_error("Invalid history array length.");
        for(const auto& value:item.at("floor_fingerprints")) (void)unsignedValue(value);
        for(const auto& value:item.at("adjustment")) (void)unsignedValue(value,UINT32_MAX);
        const std::string action=item.at("action");
        if (action=="approve") f.action=FeedbackAction::Approve;
        else if (action=="reject") f.action=FeedbackAction::Reject;
        else if (action=="adjust") f.action=FeedbackAction::Adjust;
        else throw std::runtime_error("Unknown feedback action.");
        if (f.id.size()!=64 || !ids.insert(f.id).second || f.note.size()>1024 || f.candidate>=16 ||
            !validStyleAdjustment(f.adjustment) || f.adjustment!=styleAdjustment(f.generation) || !usable(f.settings) ||
            (f.hardFail && f.action!=FeedbackAction::Reject) ||
            (!f.base.empty() && f.base.size()!=64) || f.policy.size()>128)
            throw std::runtime_error("Invalid feedback record.");
        if (!f.version.empty()) {
            const auto v=std::ranges::find(h.versions,f.version,&StyleVersion::id);
            if (v==h.versions.end() || v->base!=f.base || v->profile.client!=f.settings.client ||
                v->profile.otbSha256!=f.settings.otbSha256 || v->profile.datSha256!=f.settings.datSha256)
                throw std::runtime_error("Feedback version mismatch.");
        }
        if (f.action==FeedbackAction::Adjust && f.version.empty()) throw std::runtime_error("Adjust needs a version.");
        auto aligned=f.settings;aligned.tuning=f.generation;
        if(profileText(aligned)!=profileText(f.settings))throw std::runtime_error("Feedback tuning mismatch.");
        h.feedback.push_back(std::move(f));
    }
    return h;
}
std::expected<std::optional<std::string>,std::string> read(const std::filesystem::path& path) {
    std::error_code ec;
    const bool exists=std::filesystem::exists(path,ec);
    if (ec) return std::unexpected("Cannot inspect local history.");
    if (!exists) return std::optional<std::string>{};
    const auto size=std::filesystem::file_size(path,ec);
    if (ec || size>maxBytes) return std::unexpected("Cannot read history or history exceeds 8 MiB.");
    std::ifstream in(path,std::ios::binary);
    if (!in) return std::unexpected("Cannot read local history.");
    std::string text(static_cast<size_t>(size),'\0');
    in.read(text.data(),static_cast<std::streamsize>(size));
    if (!in || in.peek()!=std::char_traits<char>::eof()) return std::unexpected("History changed during read.");
    return std::optional<std::string>(std::move(text));
}
std::expected<void,std::string> atomicWrite(const std::filesystem::path& path,const std::string& text) {
    auto temporary=path; temporary+=L".tmp";
    struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code ec;std::filesystem::remove(p,ec);} } cleanup{temporary};
#ifdef _WIN32
    const HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file==INVALID_HANDLE_VALUE) return std::unexpected("Cannot create history temporary file.");
    DWORD written=0;
    const bool ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) && written==text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        return std::unexpected("Cannot atomically save history; previous data preserved.");
#else
    const int file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
    if (file<0) return std::unexpected("Cannot create history temporary file.");
    const bool ok=::write(file,text.data(),text.size())==static_cast<ssize_t>(text.size()) && ::fsync(file)==0;
    ::close(file);
    std::error_code ec;
    if (ok) std::filesystem::rename(temporary,path,ec);
    if (!ok || ec) return std::unexpected("Cannot atomically save history; previous data preserved.");
#endif
    return {};
}
} // namespace

StyleAdjustment styleAdjustment(const SurfaceTuning& t) {
    return {t.pathLength,t.buildingDensity,t.ruinPercent,t.campCount,t.fordLength,t.rockPasses,t.shoreSmoothing,t.repairReach};
}
void applyStyleAdjustment(SurfaceTuning& t,const StyleAdjustment& v) {
    t.pathLength=v[0];t.buildingDensity=v[1];t.ruinPercent=v[2];t.campCount=v[3];
    t.fordLength=v[4];t.rockPasses=v[5];t.shoreSmoothing=v[6];t.repairReach=v[7];
}
bool validStyleAdjustment(const StyleAdjustment& v) {
    return v[0]<=400 && v[1]<=100 && v[2]<=100 && v[3]<=50 && v[4]<=256 && v[5]<=1 && (v[6]==0 || v[6]==2) && v[7]<=1;
}
std::string styleIdentity(const StyleProfile& p) { return digest(profileText(p)); }

std::expected<StyleHistoryStore,std::string> StyleHistoryStore::open(const std::filesystem::path& path) try {
    auto token=read(path);if(!token)return std::unexpected(token.error());
    StyleHistoryStore store;store.path_=path;store.token_=std::move(*token);
    if(store.token_) {
        // Bound nesting as well as total bytes before the DOM allocates recursively.
        auto j=Json::parse(*store.token_,[](int depth,Json::parse_event_t,Json&){
            if(depth>16)throw std::runtime_error("History nesting limit exceeded.");return true;});
        store.data_=decode(j);
    }
    return store;
} catch(const std::exception& e){return std::unexpected(std::string("History unavailable: ")+e.what());}

std::expected<void,std::string> StyleHistoryStore::commit(StyleHistory next) try {
    const auto encoded=encode(next);
    next=decode(encoded); // Apply the same validation to writes and reads; dump validates UTF-8.
    const std::string text=encoded.dump(2);
    if(text.size()>maxBytes)return std::unexpected("History exceeds 8 MiB; previous data preserved.");
    std::error_code ec;std::filesystem::create_directories(path_.parent_path(),ec);
    if(ec)return std::unexpected("Cannot create local history directory.");
    auto lock=path_;lock+=L".lock";
    if(!std::filesystem::create_directory(lock,ec))return std::unexpected("History write conflict or unavailable lock; reopen the dialog.");
    struct Unlock {std::filesystem::path p;~Unlock(){std::error_code ec;std::filesystem::remove(p,ec);}} unlock{lock};
    const auto current=read(path_);
    if(!current)return std::unexpected(current.error());
    if(*current!=token_)return std::unexpected("History write conflict; reopen the dialog. Previous data preserved.");
    std::optional<std::string> nextToken(text); // allocate before replacing the durable file
    const auto saved=atomicWrite(path_,text);if(!saved)return saved;
    token_=std::move(nextToken);data_=std::move(next);return {};
}catch(const std::exception& e){return std::unexpected(std::string("History save refused: ")+e.what());}

std::expected<void,std::string> StyleHistoryStore::record(StyleFeedback f,const StyleProfile* base,std::string_view parent) try {
    StyleHistory next=data_;f.id=uniqueId();
    if(f.action==FeedbackAction::Adjust) {
        if(!base || f.hardFail)return std::unexpected("Adjust needs an admissible candidate and a base profile.");
        const std::string key=styleIdentity(*base);
        if(f.base!=key)return std::unexpected("Adjust base profile mismatch.");
        if(std::ranges::find(next.versions,key,&StyleVersion::id)==next.versions.end())next.versions.push_back({key,"",key,*base,{}});
        const std::string parentId=parent.empty()?key:std::string(parent);
        const auto p=std::ranges::find(next.versions,parentId,&StyleVersion::id);
        if(p==next.versions.end() || p->base!=key)return std::unexpected("Adjust parent profile mismatch.");
        const std::string id=uniqueId();
        next.versions.push_back({id,parentId,key,*base,f.adjustment});
        f.version=id;next.active=id;
    }
    next.feedback.push_back(std::move(f));return commit(std::move(next));
}catch(const std::exception& e){return std::unexpected(std::string("Feedback refused: ")+e.what());}

std::expected<void,std::string> StyleHistoryStore::activate(std::string_view id,std::string_view client,std::string_view otb,std::string_view dat) {
    const auto v=std::ranges::find(data_.versions,id,&StyleVersion::id);
    if(v==data_.versions.end() || checkProfile(v->profile,client,otb,dat,7)!=ProfileCompatibility::Compatible)
        return std::unexpected("Version is missing or incompatible with the loaded client/assets.");
    StyleHistory next=data_;next.active=id;return commit(std::move(next));
}
} // namespace rme::procedural
