#include "../Tools/FloorHeightCore.h"
#include "../Tools/FloorHeightOptions.h"
#include "../Tools/FloorStateStore.h"
#include <functional>
#include <iostream>
#include <limits>
#include <thread>

using namespace floorheight;
static Matrix Identity() { Matrix m{}; for(int i=0;i<3;i++)m.m[i][i]=1; return m; }
static Sample Head(double height=1.7) {
    Sample s{}; s.pose=Identity(); s.pose.m[1][3]=static_cast<float>(height);
    s.connected=true; s.valid=true; s.result=vr::TrackingResult_Running_OK; s.serial="hmd"; s.universe=42; return s;
}
static void Check(bool b) { if(!b)throw std::runtime_error("assertion failed"); }
template<class F> static void Reject(F f) { bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected); }
struct FakeRuntime: Runtime {
    Matrix origin=Identity(),pending{}; Sample head=Head(); bool calibrated=true,stageOk=true,commitOk=true,commitNoEffect=false;
    int stages=0,commits=0,discarded=0; bool hasPending=false;
    bool Calibrated() override { return calibrated; }
    Matrix ReadOrigin() override { return origin; }
    Sample ReadHeadset() override { return head; }
    bool Stage(const Matrix& expected,const Matrix& replacement) override {
        stages++; if(!stageOk || !Same(origin,expected))return false; pending=replacement;hasPending=true;return true;
    }
    bool Commit() override { commits++;if(!commitOk)return false;if(hasPending && !commitNoEffect)origin=pending;hasPending=false;return true; }
    void Discard() override { discarded++;hasPending=false; }
};
struct FakeStore: Store {
    bool saveOk=true,markOk=true; int saves=0,marked=0,restored=0; Backup backup{};std::function<void()> onSave,onMark;
    void SavePending(Backup& b) override { saves++;if(!saveOk)throw std::runtime_error("disk full");b.id="backup";backup=b;if(onSave)onSave(); }
    void MarkApplied(const Backup&) override { marked++;if(onMark)onMark();if(!markOk)throw std::runtime_error("pointer write failed"); }
    void MarkRestored(const Backup&) override { restored++; }
};
static std::vector<Sample> Samples(const Sample& s=Head()) { return std::vector<Sample>(20,s); }
int main() {
    int failed=0,total=0;
    auto test=[&](const char* name,std::function<void()> f){total++;try{f();std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){failed++;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}};
    test("6ft3 is exactly nominal 1.905m",[]{Check(std::abs(FeetInches(6,3)-1.905)<1e-12);});
    test("custom metric and inch fractions",[]{Check(std::abs(Centimeters(172.5)-1.725)<1e-12);Check(std::abs(FeetInches(5,7.5)-1.7145)<1e-12);});
    test("custom height accepts zero and upper bound",[]{Check(Height(0)==0);Check(Height(10)==10);});
    test("invalid units and nonfinite height rejected",[]{Reject([]{FeetInches(-1,3);});Reject([]{FeetInches(6,12);});Reject([]{FeetInches(1.5,2);});Reject([]{Height(-.1);});Reject([]{Height(10.1);});Reject([]{Height(std::numeric_limits<double>::quiet_NaN());});});
    test("standing floor lowered with correct sign",[]{auto m=MoveFloor(Identity(),1.7,1.905);Check(std::abs(m.m[1][3]+.205)<1e-6);Check(std::abs(HeadHeight(m,Head().pose)-1.905)<1e-6);});
    test("standing floor raised with correct sign",[]{auto m=MoveFloor(Identity(),1.7,1.4);Check(std::abs(m.m[1][3]-.3)<1e-6);Check(std::abs(HeadHeight(m,Head().pose)-1.4)<1e-6);});
    test("rotated tilted origin shifts along local up",[]{auto m=Identity();const float c=.8660254f,s=.5f;m.m[0][0]=c;m.m[0][1]=-s;m.m[1][0]=s;m.m[1][1]=c;m.m[0][3]=4;m.m[1][3]=-2;m.m[2][3]=7;auto h=Identity();for(int i=0;i<3;i++)h.m[i][3]=m.m[i][3]+m.m[i][0]*.4f+m.m[i][1]*1.7f+m.m[i][2]*.2f;auto changed=MoveFloor(m,HeadHeight(m,h),1.905);Check(std::abs(HeadHeight(changed,h)-1.905)<1e-6);for(int i=0;i<3;i++)for(int j=0;j<3;j++)Check(changed.m[i][j]==m.m[i][j]);});
    test("invalid rigid matrices rejected",[]{auto m=Identity();m.m[1][1]=2;Reject([&]{HeadHeight(m,Head().pose);});m=Identity();m.m[0][0]=-1;Reject([&]{MoveFloor(m,1.7,1.9);});m=Identity();m.m[2][3]=std::numeric_limits<float>::infinity();Reject([&]{HeadHeight(m,Head().pose);});});
    test("valid application backs up and commits",[]{FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);Check(s.saves==1 && s.marked==1 && r.commits==1);Check(std::abs(HeadHeight(r.origin,r.head.pose)-1.905)<1e-6);Check(Same(s.backup.original,Identity()));});
    test("repeated application does not accumulate offsets",[]{FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);auto once=r.origin;Apply(r,s,once,Samples(),1.905);Check(Same(r.origin,once));});
    test("invalid disconnected or fallback poses never calibrate",[]{for(int n=0;n<3;n++){FakeRuntime r;FakeStore s;auto poses=Samples();if(n==0)poses[3].valid=false;if(n==1)poses[3].connected=false;if(n==2)poses[3].result=vr::TrackingResult_Fallback_RotationOnly;Reject([&]{Apply(r,s,Identity(),poses,1.905);});Check(s.saves==0 && r.stages==0);}});
    test("NaN poses or velocities never calibrate",[]{for(int n=0;n<2;n++){FakeRuntime r;FakeStore s;auto poses=Samples();if(n==0)poses[3].pose.m[1][3]=std::numeric_limits<float>::quiet_NaN();else poses[3].velocity[0]=std::numeric_limits<double>::infinity();Reject([&]{Apply(r,s,Identity(),poses,1.905);});Check(s.saves==0);}});
    test("movement or excessive speed never calibrates",[]{for(int n=0;n<2;n++){FakeRuntime r;FakeStore s;auto poses=Samples();if(n==0)poses[3].pose.m[1][3]+=.02f;else poses[3].velocity[0]=.2;Reject([&]{Apply(r,s,Identity(),poses,1.905);});Check(s.saves==0);}});
    test("too few stable samples rejected",[]{FakeRuntime r;FakeStore s;Reject([&]{Apply(r,s,Identity(),std::vector<Sample>(3,Head()),1.905);});Check(s.saves==0);});
    test("headset and universe changes rejected",[]{for(int n=0;n<2;n++){FakeRuntime r;FakeStore s;auto poses=Samples();if(n==0)poses[3].serial="other";else poses[3].universe=99;Reject([&]{Apply(r,s,Identity(),poses,1.905);});Check(s.saves==0);}});
    test("missing identity rejected",[]{FakeRuntime r;FakeStore s;r.head.serial.clear();auto poses=Samples(r.head);Reject([&]{Apply(r,s,Identity(),poses,1.905);});Check(s.saves==0);});
    test("final pose loss prevents write",[]{FakeRuntime r;FakeStore s;r.head.valid=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(s.saves==0 && r.stages==0);});
    test("uncalibrated runtime prevents write",[]{FakeRuntime r;FakeStore s;r.calibrated=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(s.saves==0 && r.stages==0);});
    test("changed origin prevents write",[]{FakeRuntime r;FakeStore s;r.origin.m[0][3]=.1f;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(s.saves==0 && r.stages==0);});
    test("backup failure prevents stage or commit",[]{FakeRuntime r;FakeStore s;s.saveOk=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0 && Same(r.origin,Identity()));});
    test("stage failure prevents commit",[]{FakeRuntime r;FakeStore s;r.stageOk=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.commits==0 && Same(r.origin,Identity()));});
    test("commit failure discards working changes",[]{FakeRuntime r;FakeStore s;r.commitOk=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.discarded>0 && s.marked==0 && Same(r.origin,Identity()));});
    test("pointer failure restores original origin",[]{FakeRuntime r;FakeStore s;s.markOk=false;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(Same(r.origin,Identity()));});
    test("tracking loss during backup prevents floor commit",[]{FakeRuntime r;FakeStore s;s.onSave=[&]{r.head.valid=false;};Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0);});
    test("vertical movement during backup requires recapture",[]{FakeRuntime r;FakeStore s;s.onSave=[&]{r.head.pose.m[1][3]+=.005f;};Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0);});
    test("headset universe change during backup prevents commit",[]{FakeRuntime r;FakeStore s;s.onSave=[&]{r.head.universe=99;};Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0);});
    test("commit success without origin change is not reported applied",[]{FakeRuntime r;FakeStore s;r.commitNoEffect=true;Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(s.marked==0 && Same(r.origin,Identity()));});
    test("automatic rollback refuses a changed headset universe",[]{FakeRuntime r;FakeStore s;s.markOk=false;s.onMark=[&]{r.head.universe=99;};Reject([&]{Apply(r,s,Identity(),Samples(),1.905);});Check(r.commits==1 && !Same(r.origin,Identity()));});
    test("restore changes standing origin to exact backup",[]{FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);Restore(r,s,s.backup);Check(Same(r.origin,Identity()) && s.restored==1);});
    test("restore refuses other headset or universe",[]{for(int n=0;n<2;n++){FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);if(n==0)r.head.serial="other";else r.head.universe=99;int before=r.stages;Reject([&]{Restore(r,s,s.backup);});Check(r.stages==before);}});
    test("restore refuses intervening room calibration",[]{FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);r.origin.m[2][3]=1;int before=r.stages;Reject([&]{Restore(r,s,s.backup);});Check(r.stages==before);});
    test("already restored backup does not reapply",[]{FakeRuntime r;FakeStore s;Apply(r,s,Identity(),Samples(),1.905);Restore(r,s,s.backup);int before=r.stages;Restore(r,s,s.backup);Check(r.stages==before);});
    test("inverse origin preserves headset-relative height",[]{auto m=Identity();m.m[0][0]=0;m.m[0][2]=1;m.m[2][0]=-1;m.m[2][2]=0;m.m[0][3]=2;m.m[1][3]=-3;m.m[2][3]=4;Check(Same(Inverse(Inverse(m)),m));});
    test("default and explicit show are read only",[]{Check(Parse({}).action==Action::Show);Check(Parse({L"--show"}).action==Action::Show);});
    test("feet and inches CLI selects apply",[]{auto o=Parse({L"--feet",L"6",L"--inches",L"3"});Check(o.action==Action::Apply && std::abs(o.target-1.905)<1e-12);});
    test("metric CLI and delay accepted",[]{auto o=Parse({L"--centimeters",L"172.5",L"--delay",L"0"});Check(o.action==Action::Apply && std::abs(o.target-1.725)<1e-12 && o.delay==0);Check(Parse({L"--meters",L"1.8"}).action==Action::Apply);});
    test("restore and state path CLI accepted",[]{auto o=Parse({L"--restore",L"--backup-id",L"20261010-123-1",L"--state-dir",L"C:\\folder with spaces"});Check(o.action==Action::Restore && o.backupId=="20261010-123-1" && o.stateDirectory==L"C:\\folder with spaces");});
    test("CLI conflicting units or actions rejected",[]{Reject([]{Parse({L"--feet",L"6",L"--meters",L"1.9"});});Reject([]{Parse({L"--show",L"--meters",L"1.9"});});Reject([]{Parse({L"--restore",L"--meters",L"1.9"});});Reject([]{Parse({L"--inches",L"3"});});});
    test("CLI malformed nonfinite and missing values rejected",[]{Reject([]{Parse({L"--meters",L"NaN"});});Reject([]{Parse({L"--meters",L"1.9junk"});});Reject([]{Parse({L"--meters"});});Reject([]{Parse({L"--unknown"});});});
    test("CLI repeated values rejected",[]{Reject([]{Parse({L"--meters",L"1",L"--meters",L"2"});});Reject([]{Parse({L"--feet",L"6",L"--feet",L"5"});});});
    test("CLI delay and backup traversal bounded",[]{Reject([]{Parse({L"--meters",L"1",L"--delay",L"1.5"});});Reject([]{Parse({L"--meters",L"1",L"--delay",L"61"});});Reject([]{Parse({L"--restore",L"--backup-id",L"..\\elsewhere"});});Reject([]{Parse({L"--show",L"--backup-id",L"1"});});});
    const auto storageRoot=std::filesystem::current_path().parent_path()/L"floor-state-tests"/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    const auto mutexName=L"Local\\FloorHeightTests-"+std::to_wstring(GetCurrentProcessId());
    test("simultaneous tool mutation is refused",[&]{MutationLock first(mutexName.c_str());bool refused=false;std::thread second([&]{try{MutationLock conflict(mutexName.c_str());}catch(const std::exception&){refused=true;}});second.join();Check(refused);});
    test("tool mutation lock is released",[&]{{MutationLock first(mutexName.c_str());}bool acquired=false;std::thread second([&]{MutationLock next(mutexName.c_str());acquired=true;});second.join();Check(acquired);});
    test("file backup roundtrips exact matrices and identity",[&]{FakeRuntime r;FileStore store(storageRoot/L"roundtrip",[]{return std::string("{\"export\":true}");});Apply(r,store,Identity(),Samples(),1.905);auto b=store.Load();Check(b.serial=="hmd" && b.universe==42 && b.target==1.905 && Same(b.original,Identity()) && Same(b.applied,r.origin));Restore(r,store,b);Check(Same(r.origin,Identity()));});
    test("pending backup cannot replace previous successful pointer",[&]{FakeRuntime r;FileStore store(storageRoot/L"pending",[]{return std::string("{}");});Apply(r,store,Identity(),Samples(),1.905);auto original=store.Load();Backup pending=original;store.SavePending(pending);Check(store.Load().id==original.id && pending.id!=original.id);});
    test("empty calibration export prevents runtime mutation",[&]{FakeRuntime r;FileStore store(storageRoot/L"empty",[]{return std::string();});Reject([&]{Apply(r,store,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0);});
    test("unwritable backup location prevents runtime mutation",[&]{std::filesystem::create_directories(storageRoot);auto blocked=storageRoot/L"blocked-file";std::ofstream(blocked)<<"file";FakeRuntime r;FileStore store(blocked,[]{return std::string("{}");});Reject([&]{Apply(r,store,Identity(),Samples(),1.905);});Check(r.stages==0 && r.commits==0);});
    test("unicode and space backup directory supported",[&]{FakeRuntime r;FileStore store(storageRoot/L"folder with spaces-\u03bb",[]{return std::string("{}");});Apply(r,store,Identity(),Samples(),1.905);Check(store.Load().serial=="hmd");});
    test("missing or traversing backup rejected",[&]{FileStore store(storageRoot/L"missing",[]{return std::string("{}");});Reject([&]{store.Load();});Reject([&]{store.Load("..\\other");});});
    test("malformed stored matrix rejected before restoration",[&]{FakeRuntime r;FileStore store(storageRoot/L"malformed",[]{return std::string("{}");});Apply(r,store,Identity(),Samples(),1.905);auto b=store.Load();auto path=storageRoot/L"malformed"/b.id/L"floor.json";std::ifstream input(path);auto j=Json::parse(input);input.close();j["originalStandingToRaw"][0][0]=2;WriteDurable(path,j.dump());int before=r.stages;Reject([&]{Restore(r,store,store.Load());});Check(r.stages==before);});
    std::cout<<(total-failed)<<'/'<<total<<" passed\n";return failed?1:0;
}
