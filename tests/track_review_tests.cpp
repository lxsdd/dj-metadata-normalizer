#include "djmeta/track_review.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

void check(bool good,const char* msg){
    if(!good){std::cerr<<"FAIL: "<<msg<<"\n";std::exit(1);}
}
djmeta::MetadataDiffRow diff(std::size_t i,const char* field,djmeta::SafetyClass safety){
    djmeta::MetadataDiffRow item;
    item.source_index=i;item.field=field;item.original=" old ";item.proposed="new";
    item.safety=safety;item.rule_ids="safe.trim";return item;
}
int main(){
    using namespace djmeta;
    const std::vector<MetadataDiffRow> proposals={
        diff(0,"DISCOGS_RELEASE_CREDITS",SafetyClass::Safe),
        diff(0,"TITLE",SafetyClass::Review),
        diff(0,"DATE_RAW",SafetyClass::Confident),
        diff(1,"ARTIST",SafetyClass::Safe),
        diff(1,"X_CUSTOM_FIELD",SafetyClass::Review),
        diff(1,"DISCOGS_ARTIST_PROFILE",SafetyClass::Safe)
    };
    check(is_music_metadata_field("artist") &&
          is_music_metadata_field("REMIXED BY") &&
          is_music_metadata_field("date_raw") &&
          !is_music_metadata_field("DISCOGS_RELEASE_NOTES") &&
          !is_music_metadata_field("X_CUSTOM_FIELD"),
          "default music/extended scope classification");
    const auto all=summarize_track_changes(3,proposals);
    check(all.size()==3 && all[0].music_changes==2 &&
          all[0].extended_changes==1 && all[0].review_required==1 &&
          all[1].music_changes==1 && all[1].extended_changes==2 &&
          all[1].review_required==1 && all[2].music_changes==0,
          "one summary per selected track including unchanged");
    const auto music=selected_track_diffs(proposals,0,MetadataFocus::Music);
    const auto extended=selected_track_diffs(proposals,1,MetadataFocus::Extended);
    const auto both=selected_track_diffs(proposals,1,MetadataFocus::All);
    check(music.size()==2 && music[0].field=="TITLE" &&
          music[0].safety==SafetyClass::Review &&
          extended.size()==2 && both.size()==3,
          "focus must only filter presentation and preserve provenance");
    const std::vector<std::string> names={"Z-track","A-track","B-track"};
    const auto by_name=sort_track_summaries(all,names,0,false);
    const auto by_review=sort_track_summaries(all,names,3,true);
    check((by_name==std::vector<std::size_t>{1,2,0}) &&
          (by_review==std::vector<std::size_t>{0,1,2}),
          "stable master row sort and index mapping");
    check(proposals[0].field=="DISCOGS_RELEASE_CREDITS" &&
          proposals[4].field=="X_CUSTOM_FIELD",
          "no mutation or loss of secondary metadata");
    std::cout<<"PASS: master/detail summaries, music/extended filters and stable source IDs\n";
}
