// SPDX-License-Identifier: GPL-3.0-or-later
#include "procedural/core/style_history.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>

using namespace rme::procedural;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
StyleProfile profile(){StyleProfile p;p.client="test";p.otbSha256="aa";p.datSha256="bb";p.tuning=SurfaceTuning{};p.sources={{"M06",1000}};return p;}
StyleFeedback feedback(){StyleFeedback f;f.settings=profile();f.adjustment={40,30,0,0,0,0,2,1};f.policy="g6-bands-v1/A/I54-v1";
    applyStyleAdjustment(f.generation,f.adjustment);
    f.userSeed=1729;f.initialSeed=123;f.actualSeed=456;f.candidate=3;f.fingerprints={1,2,3,4,UINT64_MAX};return f;}
std::string read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path& p,std::string_view s){std::ofstream out(p,std::ios::binary);out<<s;}
}
int main(int argc,char** argv) try {
    const auto root = argc==2 ? std::filesystem::path(argv[1]) :
        std::filesystem::absolute(argv[0]).parent_path().parent_path().parent_path().parent_path()/"scratchpad/style-history-tests";
    const auto dir=root/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    check(!std::filesystem::exists(dir),"Test directory must be new");
    std::filesystem::create_directories(dir);
    const auto path=dir/"history.json";
    auto a=StyleHistoryStore::open(path);auto stale=StyleHistoryStore::open(path);check(a && stale,"Missing history opens");
    auto f=feedback(); f.note=std::string(1022,'a')+"\xc4\x85";
    check(bool(a->record(f)),"Approve and UTF-8 exactly 1024 bytes");
    check(!stale->record(f),"Concurrent initial creation conflicts");
    auto before=read(path);f.note+='a';check(!a->record(f) && read(path)==before,"Overlimit preserves file");
    f.note="\xff";check(!a->record(f) && read(path)==before,"Invalid UTF-8 refused");
    f=feedback();f.action=FeedbackAction::Reject;f.hardFail=true;
    check(bool(a->record(f)),"Reject accepts hardFail");
    f.action=FeedbackAction::Approve;check(!a->record(f),"Approve refuses hardFail");
    f=feedback();f.action=FeedbackAction::Adjust;
    check(!a->record(f),"Adjust without base refused");
    auto base=profile();f.base=styleIdentity(base);
    check(bool(a->record(f,&base)),"First Adjust");
    const auto first=a->data().active;
    check(a->data().versions.size()==2 && a->data().versions[1].parent==f.base,"Immutable root and child");
    check(!a->activate(f.base,"other","aa","bb") && !a->activate(f.base,"test","wrong","bb"),"Client and assets gated");
    check(bool(a->activate(f.base,"test","aa","bb")),"Rollback root");
    f.adjustment[0]=80;
    applyStyleAdjustment(f.generation,f.adjustment);
    check(bool(a->record(f,&base,f.base)),"Branch Adjust after rollback");
    const auto branch=a->data().active;
    check(first!=branch && a->data().versions.size()==3 && a->data().versions[2].parent==f.base,"Branch preserves newer child");
    auto reopened=StyleHistoryStore::open(path);check(reopened && reopened->data().active==branch && reopened->data().feedback.size()==4,"Restart retains all actions and active branch");
    check(reopened->data().feedback[0].fingerprints[4]==UINT64_MAX && reopened->data().feedback[0].candidate==3 &&
        reopened->data().feedback[0].actualSeed==456 && reopened->data().feedback[0].note.size()==1024,"Exact candidate identity/uint64/UTF-8 roundtrip");
    check(reopened->data().versions[2].profile==base && reopened->data().versions[2].adjustment->at(0)==80,"Provenance and adjustment retained");
    check(reopened->data().feedback.back().generation.pathLength==80,"Full generation settings roundtrip");
    check(bool(reopened->activate(first,"test","aa","bb")),"Second writer activation");
    check(!a->record(feedback()),"Old snapshot conflicts after second writer");
    // A successful activation is a write even if it picks the same node.
    before=read(path);write(path,"unknown");check(!reopened->record(feedback()) && read(path)=="unknown","External replacement conflicts");
    write(path,before);
    std::filesystem::create_directory(path.wstring()+L".lock");
    check(!reopened->record(feedback()) && read(path)==before,"Lock conflict preserves data");
    std::filesystem::remove(path.wstring()+L".lock");
    std::filesystem::create_directory(path.wstring()+L".tmp");
    check(!reopened->record(feedback()) && read(path)==before,"Write error preserves data");
    std::filesystem::remove(path.wstring()+L".tmp");
    const auto bad=dir/"bad.json";
    auto negative=before;
    const auto candidate=negative.find("\"candidate\": 3");check(candidate!=std::string::npos,"Candidate fixture field");
    negative.replace(candidate,std::string("\"candidate\": 3").size(),"\"candidate\": -1");
    write(bad,negative);check(!StyleHistoryStore::open(bad),"Negative integer refused without wrapping");
    auto cycle=before;
    const auto parent=cycle.find("\"parent\": \"\"");check(parent!=std::string::npos,"Root fixture field");
    cycle.replace(parent,std::string("\"parent\": \"\"").size(),"\"parent\": \""+first+"\"");
    write(bad,cycle);check(!StyleHistoryStore::open(bad),"Invalid ancestry refused");
    for(const std::string text:{"{}","{broken", "{\"schema\":\"g6-style-history/999\"}"}){
        write(bad,text);check(!StyleHistoryStore::open(bad) && read(bad)==text,"Corrupt/foreign schema is never reset");}
    write(bad,std::string(8u*1024u*1024u+1,'x'));check(!StyleHistoryStore::open(bad),"Bounded read");
    auto t=SurfaceTuning{};t.passes=3;t.plateauLevels=2;applyStyleAdjustment(t,{80,40,5,2,4,1,2,0});
    check(t.passes==3 && t.plateauLevels==2 && t.pathLength==80,"Adjust scope excludes mountains");
    check(!validStyleAdjustment({401,0,0,0,0,0,0,0}),"Adjustment ranges enforced");
    std::cout<<"STYLE_HISTORY_PASS approve reject adjust restart utf8 conflict write-error rollback branch client-assets scope\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAILED "<<e.what()<<'\n';return 1;}
