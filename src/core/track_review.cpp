#include "djmeta/track_review.h"

#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include <string_view>

namespace djmeta {
namespace {
int compare_folded(std::string_view a, std::string_view b) {
    const auto common = (std::min)(a.size(), b.size());
    for (std::size_t i=0; i<common; ++i) {
        unsigned char x = static_cast<unsigned char>(a[i]);
        unsigned char y = static_cast<unsigned char>(b[i]);
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return x<y ? -1 : 1;
    }
    if (a.size()==b.size()) return 0;
    return a.size()<b.size() ? -1 : 1;
}
std::string upper_ascii(std::string_view input) {
    std::string out(input);
    for (char& ch:out) if (ch>='a' && ch<='z') ch=static_cast<char>(ch-'a'+'A');
    return out;
}
}
bool is_music_metadata_field(std::string_view name) {
    // High-value DJ/foobar tag fields. Do not change engine eligibility;
    // this list is only the default UI focus, and is versioned separately.
    static constexpr std::array<std::string_view,31> fields = {
        "ALBUM", "ALBUM ARTIST", "ALBUMARTIST", "ARTIST", "BPM", "CATALOGNUMBER",
        "COMMENT", "COMPILATION", "COPYRIGHT", "DATE", "DATE_RAW", "DISCNUMBER",
        "DISCTOTAL", "GENRE", "ISRC", "LABEL", "MIX", "MIXED BY",
        "ORIGINAL ARTIST", "ORIGINAL TITLE", "PUBLISHER", "REMIX", "REMIXED BY",
        "REMIXER", "SONGWRITER", "STYLE", "TITLE", "TOTALDISCS", "TOTALTRACKS",
        "TRACKNUMBER", "VERSION"
    };
    const auto folded = upper_ascii(name);
    return std::find(fields.begin(),fields.end(),folded)!=fields.end();
}
std::vector<TrackReviewSummary> summarize_track_changes(
    std::size_t count, const std::vector<MetadataDiffRow>& proposals) {
    std::vector<TrackReviewSummary> result(count);
    for (std::size_t i=0;i<count;++i) result[i].source_index=i;
    for (const auto& proposal : proposals) {
        if (proposal.source_index >= count) continue; // invalid projection not displayed
        auto& item=result[proposal.source_index];
        if(is_music_metadata_field(proposal.field)) ++item.music_changes;
        else ++item.extended_changes;
        switch(proposal.safety) {
            case SafetyClass::Safe: ++item.safe_changes; break;
            case SafetyClass::Confident: ++item.confident_changes; break;
            case SafetyClass::Review: ++item.review_required; break;
        }
    }
    return result;
}
std::vector<MetadataDiffRow> selected_track_diffs(
    const std::vector<MetadataDiffRow>& all, std::size_t index,
    MetadataFocus focus) {
    std::vector<MetadataDiffRow> rows;
    for(const auto& change:all){
        if(change.source_index!=index) continue;
        const bool music=is_music_metadata_field(change.field);
        if(focus==MetadataFocus::All ||
           (focus==MetadataFocus::Music && music) ||
           (focus==MetadataFocus::Extended && !music))
            rows.push_back(change);
    }
    return rows;
}
std::vector<std::size_t> filter_track_view(
    const std::vector<TrackReviewSummary>& summaries,
    const std::vector<std::size_t>& sorted_source_indices,
    TrackDiscovery discovery) {
    std::vector<std::size_t> visible;
    visible.reserve(sorted_source_indices.size());
    for (const auto index : sorted_source_indices) {
        if (index >= summaries.size()) continue;
        const auto& track = summaries[index];
        const bool changed = track.music_changes + track.extended_changes > 0;
        if (discovery == TrackDiscovery::All ||
            (discovery == TrackDiscovery::Changed && changed) ||
            (discovery == TrackDiscovery::NeedsReview && track.review_required > 0))
            visible.push_back(index);
    }
    return visible;
}

std::vector<std::size_t> sort_track_summaries(
    const std::vector<TrackReviewSummary>& rows,
    const std::vector<std::string>& labels, int col, bool desc) {
    std::vector<std::size_t> view(rows.size());
    std::iota(view.begin(),view.end(),std::size_t{0});
    if(col<0 || col>3) return view;
    std::stable_sort(view.begin(),view.end(),[&](std::size_t a,std::size_t b){
        int comparison=0;
        if(col==0) {
            const std::string_view left=rows[a].source_index<labels.size()
                 ? std::string_view(labels[rows[a].source_index]):std::string_view{};
            const std::string_view right=rows[b].source_index<labels.size()
                 ? std::string_view(labels[rows[b].source_index]):std::string_view{};
            comparison=compare_folded(left,right);
        } else {
            const auto value=[&](const TrackReviewSummary& row) -> std::size_t {
                return col==1?row.music_changes:
                       col==2?row.extended_changes:row.review_required;
            };
            if(value(rows[a])<value(rows[b])) comparison=-1;
            if(value(rows[a])>value(rows[b])) comparison=1;
        }
        return desc?comparison>0:comparison<0;
    });
    return view;
}
} // namespace djmeta
