#include "djmeta/musicbrainz_provider.h"
#include "djmeta/cue_online_bridge.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool yes, const char* msg) {
    if (!yes) { std::cerr << "FAIL: " << msg << "\n"; std::exit(1); }
}
template<typename Function> void rejects(Function fn,const char* why) {
    bool rejected=false;
    try { fn(); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,why);
}
constexpr const char* first="11111111-1111-1111-1111-111111111111";
constexpr const char* second="22222222-2222-2222-2222-222222222222";
}
int main() {
    using namespace djmeta;
    using namespace djmeta::online;
    using namespace djmeta::online::musicbrainz;
    const auto request=make_search_path(SearchKind::Recording,
        "Björk: It's \"Alive\" & 東京", "Artist / Guest");
    check(request.starts_with("/ws/2/recording?query=") &&
          request.find("fmt=json&limit=8") != std::string::npos &&
          request.find("Björk") == std::string::npos &&
          request.find("%22") != std::string::npos &&
          request.find("%26") != std::string::npos &&
          request.find("%E6%9D%B1%E4%BA%AC") != std::string::npos,
          "recording search encoded, Lucene quoted, bounded and UTF-8");
    check(make_search_path(SearchKind::Release,"Album","").find("/ws/2/release?")==0,
          "release search without unknown artist is permitted");
    check(make_release_lookup_path(first)==std::string("/ws/2/release/")+first+
          "?inc=recordings%2Bartist-credits%2Bisrcs&fmt=json",
          "release lookup only uses MBID to prevent URL injection");
    rejects([]{(void)make_release_lookup_path("../ws/2/recording");},
            "invalid MBIDs never become URL paths");
    rejects([]{(void)make_search_path(SearchKind::Release,"Album\nOther","");},
            "CRLF injection in search rejected");
    rejects([]{(void)make_search_path(SearchKind::Release,"","");},
            "empty search title rejected");

    const std::string recording_json=std::string(R"({
       "created":"2026-10-09","recording-count":2,
       "recordings":[
         {"id":")")+first+R"(","title":"Bj\u00f6rk & \u6771\u4eac",
          "score":99,"length":215540,
          "artist-credit":[{"name":"Alpha","joinphrase":" feat. "},{"name":"Beta"}],
          "releases":[{"id":"ignored"}]},
         {"id":")"+second+R"(","title":"Track (Radio Edit)",
          "score":55,"length":200000,
          "artist-credit":[{"name":"Other Artist"}]},
         {"id":"invalid/redirect","title":"Bad","artist-credit":[{"name":"X"}]}
       ]
    })";
    const auto found=parse_search(recording_json,SearchKind::Recording);
    check(found.candidates.size()==2 &&
          found.candidates[0].mbid==first &&
          found.candidates[0].title=="Björk & 東京" &&
          found.candidates[0].artist=="Alpha feat. Beta" &&
          found.candidates[0].duration_ms==215540 &&
          found.candidates[0].search_score==99 &&
          found.candidates[1].search_score==55,
          "official recording response preserves Unicode, artist roles, duration, score");
    check(parse_search(R"({"releases":[]})",SearchKind::Release).candidates.empty(),
          "empty permitted source response never fabricates a hit");
    const auto release_search=std::string(R"({"releases":[{"id":")")+first+
        R"(","title":"Album Edition","date":"2001-06-15","score":91,
        "artist-credit":[{"name":"Example Artist"}]}]})";
    const auto releases=parse_search(release_search,SearchKind::Release);
    check(releases.candidates.size()==1&&
          releases.candidates[0].kind==SearchKind::Release&&
          releases.candidates[0].release_date=="2001-06-15",
          "release search retains explicit release-edition date");

    const auto edition_json=std::string(R"({
      "id":")")+first+R"(", "title":"Album Edition", "date":"2001-06-15",
      "artist-credit":[{"name":"Example Artist"}],
      "media":[
       {"position":1,"tracks":[
         {"position":1,"title":"First (Extended Mix)","length":420000,
          "artist-credit":[{"name":"Example Artist"}],
          "recording":{"id":")"+second+R"(","title":"First (Extended Mix)",
             "isrcs":["GBABC2600001"],"artist-credit":[{"name":"Example Artist"}]}},
         {"position":2,"title":"Second","length":301000,
          "recording":{"id":")"+second+R"(","title":"Second",
             "artist-credit":[{"name":"Example Artist"}]}}]},
       {"position":2,"tracks":[
         {"position":1,"title":"Third","length":240000,
          "recording":{"id":")"+second+R"(","title":"Third",
             "artist-credit":[{"name":"Guest Artist"}]}}]}
     ]})";
    const auto edition=parse_release_lookup(edition_json,first);
    check(edition.provider=="musicbrainz"&&edition.edition_id==first &&
          edition.fields.size()==3 &&
          edition.fields[2].date_meaning==DateMeaning::EditionRelease &&
          edition.tracks.size()==3 &&
          edition.tracks[0].recording.isrc=="GBABC2600001" &&
          edition.tracks[1].disc_number==1 && edition.tracks[1].track_number==2 &&
          edition.tracks[2].disc_number==2 && edition.tracks[2].track_number==1,
          "release lookup preserves exact edition, recording, date/disc/track scope");

    rejects([&]{(void)parse_release_lookup(edition_json,second);},
            "mismatched edition ID is a blocker");
    rejects([]{(void)JsonParser("{\"x\":1,\"x\":2}").parse();},
            "ambiguous duplicate JSON keys rejected");
    rejects([]{(void)JsonParser("\"\\uD800\"").parse();},
            "unpaired surrogate rejected");
    rejects([]{(void)JsonParser("{\"recordings\":[1,]}").parse();},
            "trailing comma rejected");
    rejects([]{(void)parse_search(
        R"({"releases":[{"id":"evil/traversal","title":"T","artist-credit":[{"name":"A"}]}]})",
        SearchKind::Recording);},
        "wrong official resource schema rejected");
    rejects([]{(void)JsonParser(std::string(2u*1024u*1024u+1,'x')).parse();},
            "oversized JSON response rejected");
    const auto cue=inspect_cue_metadata(
        "TITLE \"Album Edition\"\nPERFORMER \"Example Artist\"\n"
        "FILE \"disc.flac\" WAVE\nTRACK 01 AUDIO\nTITLE \"First (Extended Mix)\"\n"
        "PERFORMER \"Example Artist\"\nTRACK 02 AUDIO\nTITLE Second\n"
        "TRACK 03 AUDIO\nTITLE Third\nPERFORMER \"Guest Artist\"\n",
        CueCarrierKind::ExternalText);
    const auto local=prepare_cue_release_lookup(cue);
    check(local.eligible,"qualified synthetic CUE release");
    const auto assigned=align_release(local.tracks,edition);
    check(assigned.matched==3 && assigned.unmatched==0,
          "real provider release edition feeds existing whole-CUE matching engine");

    std::cout << "PASS: official MusicBrainz query encoding, bounded JSON, artist roles and release alignment\n";
}
