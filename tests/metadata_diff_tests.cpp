#include "djmeta/metadata_diff.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

void require(bool ok,const char* description) {
    if(!ok) { std::cerr<<"FAIL: "<<description<<"\n";std::exit(1); }
}
int main() {
    djmeta::AnalysisResult a,b;
    a.proposals.push_back({0,"TITLE",0," Song (Mix) ","Song",
         djmeta::SafetyClass::Review,{"trim","remix-extract"},{"trim whitespace","remix requires review"}});
    a.proposals.push_back({1,"ARTIST",1," Artist  A ","Artist A",
         djmeta::SafetyClass::Safe,{"safe.collapse"},{"collapse spaces"}});
    b.proposals.push_back({0,"TITLE",0,"  Track ","Track",
         djmeta::SafetyClass::Safe,{"safe.trim"},{"trim"}});
    b.proposals.push_back({1,"DATE_RAW",0,"1998-06-15","1998-06-15",
         djmeta::SafetyClass::Safe,{"no-op"},{"leave raw date"}});
    const auto inputs=std::vector<djmeta::AnalysisResult>{a,b};
    auto rows=djmeta::describe_metadata_diffs(inputs);
    require(rows.size()==3,"skip no-ops and preserve source identities");
    require(rows[0].proposal_index==0 && rows[1].proposal_index==1 &&
            rows[2].proposal_index==0,
            "flattened rows preserve proposal identity across source and no-op gaps");
    require(rows[0].source_index==0 && rows[0].field=="TITLE" &&
            rows[0].original==" Song (Mix) " && rows[0].proposed=="Song" &&
            rows[0].safety==djmeta::SafetyClass::Review,
            "chain including REVIEW must not be marked SAFE");
    require(rows[0].rule_ids=="trim, remix-extract" &&
            rows[0].rationales=="trim whitespace, remix requires review",
            "retain all contributing rule provenance");
    require(rows[1].value_index==1 && rows[1].safety==djmeta::SafetyClass::Safe,
            "preserve original multivalue location and safety");
    const std::vector<std::string> labels={"A.mp3","B.mp3"};
    const auto by_file=djmeta::sort_metadata_diff_rows(rows,labels,0,true);
    require((by_file==std::vector<std::size_t>{2,0,1}),"source sort stable");
    const auto by_status=djmeta::sort_metadata_diff_rows(rows,labels,4,false);
    require((by_status==std::vector<std::size_t>{0,1,2}),"status sort deterministic");
    const auto by_title=djmeta::sort_metadata_diff_rows(rows,labels,1,false);
    require((by_title==std::vector<std::size_t>{1,0,2}),"field sort stable");
    const auto unchanged=djmeta::sort_metadata_diff_rows(rows,labels,99,false);
    require((unchanged==std::vector<std::size_t>{0,1,2}),"invalid column keeps stable order");
    require(inputs[0].proposals.size()==2 && inputs[1].proposals.size()==2,
            "flatten/sort must not mutate input");
    std::cout<<"PASS: metadata diff provenance, SAFE/REVIEW, multivalues and stable sorts\n";
}
