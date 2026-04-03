#include "wal.h"
#include <cstring>

WAL::WAL(std::string& path){
    file_.open(path, std::ios::binary | std::ios::app);
    if(!file_.is_open()){
        throw std::runtime_error("Cannot open WAL: " + path);
    }
}

WAL::~WAL(){
    if(file_.is_open()) file_.close();
}

uint32_t WAL::crc32(const void* data, size_t len) const {
    const uint8_t* buf = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < len; i++){
        crc ^= buf[i];
        for(int j = 0; j < 8; j++){
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1)));
        }
    }
    return ~crc;
}

void WAL::append(RecordType type, const std::string& key, const std::string& value){
    uint32_t klen = static_cast<uint32_t>(key.size());
    uint32_t vlen = static_cast<uint32_t>(value.size());
    uint8_t t = static_cast<uint8_t>(type);

    std::string payload;
    payload.push_back(static_cast<char>(t));
    payload.append(reinterpret_cast<const char*>(&klen), 4);
    payload.append(reinterpret_cast<const char*>(&vlen), 4);
    payload.append(key);
    payload.append(value);

    uint32_t crc = crc32(payload.data(), payload.size());

    file_.write(reinterpret_cast<const char*>(crc), 4);
    file_.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    file_.flush();
}

void WAL::sync(){
    file_.flush();
}

void WAL::clear(){
    file_.close();
    std::remove(path_.c_str());
}

std::vector<WALRecord> WAL::recover(const std::string& path){
    std::ifstream in(path, std::ios::binary);
    std::vector<WALRecord> records;
    if(!in.is_open()) return records;

    while(true){
        uint32_t stored_crc, klen, vlen;
        uint8_t type;

        if(!in.read(reinterpret_cast<char*>(&stored_crc), 4)) break;
        if (!in.read(reinterpret_cast<char*>(&type),  1)) break;
        if (!in.read(reinterpret_cast<char*>(&klen),  4)) break;
        if (!in.read(reinterpret_cast<char*>(&vlen),  4)) break;
        std::string key(klen, '\0'), value(vlen, '\0');
        if (!in.read(key.data(),   static_cast<std::streamsize>(klen))) break;
        if (!in.read(value.data(), static_cast<std::streamsize>(vlen))) break;

        std::string payload;
        payload.push_back(static_cast<char>(type));
        payload.append(reinterpret_cast<const char*>(&klen), 4);
        payload.append(reinterpret_cast<const char*>(&vlen), 4);
        payload.append(key);
        payload.append(value);

        uint32_t computed = crc32(payload.data(), payload.size());
        if (computed != stored_crc) break; // corrupt tail — stop here

        records.push_back({ static_cast<RecordType>(type), key, value });
    }
    return records;
}