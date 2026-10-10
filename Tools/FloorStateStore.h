#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <nlohmann/json.hpp>
#include "FloorHeightOptions.h"
#include <chrono>
#include <fstream>
#include <functional>
namespace floorheight {
using Json=nlohmann::json;
class MutationLock {
    HANDLE mutex=nullptr;
public:
    explicit MutationLock(const wchar_t* name=L"Local\\DreamAirLH1FloorHeight") {
        mutex=CreateMutexW(nullptr,FALSE,name);
        if(!mutex)throw std::runtime_error("Cannot acquire the floor-adjustment lock.");
        const DWORD result=WaitForSingleObject(mutex,0);
        if(result!=WAIT_OBJECT_0 && result!=WAIT_ABANDONED){CloseHandle(mutex);mutex=nullptr;throw std::runtime_error("Another floor adjustment is running. Wait for it to finish.");}
    }
    MutationLock(const MutationLock&)=delete;
    MutationLock& operator=(const MutationLock&)=delete;
    ~MutationLock(){if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}}
};
inline Json MatrixJson(const Matrix& matrix) {
    Json rows=Json::array();for(const auto& row:matrix.m)rows.push_back({row[0],row[1],row[2],row[3]});return rows;
}
inline Matrix ReadMatrix(const Json& rows) {
    if(!rows.is_array() || rows.size()!=3)throw std::invalid_argument("Invalid backup matrix.");Matrix matrix{};
    for(int i=0;i<3;i++) {
        if(!rows[i].is_array() || rows[i].size()!=4)throw std::invalid_argument("Invalid backup matrix row.");
        for(int j=0;j<4;j++)matrix.m[i][j]=rows[i][j].get<float>();
    }
    Rigid(matrix);return matrix;
}
inline void WriteDurable(const std::filesystem::path& path,const std::string& text) {
    if(text.size()>16*1024*1024)throw std::runtime_error("Floor backup is too large.");
    const auto temp=std::filesystem::path(path.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create floor backup file.");
    DWORD written=0;bool ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)!=0;
    ok=ok && written==text.size();if(ok)ok=FlushFileBuffers(file)!=0;CloseHandle(file);
    if(!ok){DeleteFileW(temp.c_str());throw std::runtime_error("Cannot flush floor backup file.");}
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());throw std::runtime_error("Cannot save floor backup file.");
    }
}
class FileStore:public Store {
    std::filesystem::path root;
    std::function<std::string()> exportCalibration;
    std::filesystem::path RecordPath(const std::string& id)const {
        if(!ValidBackupId(id))throw std::invalid_argument("Invalid backup ID.");return root/id/"floor.json";
    }
    void CheckPaths()const {
        for(auto path=root;!path.empty();) {
            const DWORD attributes=GetFileAttributesW(path.c_str());
            if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Floor backup directory cannot be a linked path.");
            auto parent=path.parent_path();if(parent==path)break;path=parent;
        }
    }
    Json ReadJson(const std::filesystem::path& path)const {
        CheckPaths();if(std::filesystem::file_size(path)>1024*1024)throw std::runtime_error("Floor metadata is too large.");
        std::ifstream stream(path);if(!stream)throw std::runtime_error("Cannot read floor backup.");return Json::parse(stream);
    }
    void UpdateStatus(const Backup& backup,const char* status) {
        auto path=RecordPath(backup.id);auto record=ReadJson(path);record["status"]=status;WriteDurable(path,record.dump(2));
    }
public:
    FileStore(const std::filesystem::path& directory,std::function<std::string()> exporter):root(std::filesystem::absolute(directory).lexically_normal()),exportCalibration(std::move(exporter)){}
    void SavePending(Backup& backup)override {
        CheckPaths();std::filesystem::create_directories(root);CheckPaths();
        const auto timestamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for(unsigned attempt=0;;attempt++) {
            backup.id=std::to_string(timestamp)+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(attempt);
            if(std::filesystem::create_directory(root/backup.id))break;
            if(attempt>1000)throw std::runtime_error("Cannot allocate a unique floor backup.");
        }
        auto full=exportCalibration();if(full.empty())throw std::runtime_error("SteamVR calibration export is empty. No floor was changed.");
        WriteDurable(root/backup.id/"chaperone-export.json",full);
        Json record={{"version",1},{"id",backup.id},{"serial",backup.serial},{"universe",backup.universe},{"targetMeters",backup.target},
            {"originalStandingToRaw",MatrixJson(backup.original)},{"appliedStandingToRaw",MatrixJson(backup.applied)},{"status","pending"}};
        WriteDurable(RecordPath(backup.id),record.dump(2));
    }
    void MarkApplied(const Backup& backup)override {
        UpdateStatus(backup,"applied");WriteDurable(root/"latest.json",Json{{"id",backup.id}}.dump(2));
    }
    void MarkRestored(const Backup& backup)override { UpdateStatus(backup,"restored"); }
    Backup Load(std::string id={})const {
        if(id.empty())id=ReadJson(root/"latest.json").at("id").get<std::string>();
        const auto record=ReadJson(RecordPath(id));
        if(record.at("version").get<int>()!=1 || record.at("id").get<std::string>()!=id)throw std::runtime_error("Invalid floor backup metadata.");
        Backup backup{};backup.id=id;backup.serial=record.at("serial").get<std::string>();backup.universe=record.at("universe").get<uint64_t>();
        backup.target=Height(record.at("targetMeters").get<double>());
        backup.original=ReadMatrix(record.at("originalStandingToRaw"));backup.applied=ReadMatrix(record.at("appliedStandingToRaw"));return backup;
    }
};
}
