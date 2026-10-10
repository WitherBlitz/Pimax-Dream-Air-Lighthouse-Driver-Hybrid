#pragma once
#include <mutex>
#include <set>
#include <string>
namespace dreamair {
class RuntimeHeadsetSelection {
public:
    void Observe(const std::string& model,const std::string& manufacturer,const std::string& type,const std::string& serial,bool attachedDreamAir){
        if(type!="hmd" || serial.empty())return;
        if(model!="Pimax Dream Air" && !(attachedDreamAir && model=="REF-HMD" && manufacturer.find("Pimax")!=std::string::npos))return;
        std::lock_guard<std::mutex> lock(mutex_);serials_.insert(serial);
    }
    bool Contains(const std::string& serial){std::lock_guard<std::mutex> lock(mutex_);return serials_.count(serial)!=0;}
private:
    std::mutex mutex_;std::set<std::string> serials_;
};
inline RuntimeHeadsetSelection runtimeHeadsetSelection;
}
