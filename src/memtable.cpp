#include "memtable.h"

void Memtable::put(const std::string& key, const std::string& value){
    byte_size_ += key.size() + value.size();
    table_.put(key, value);
}

void Memtable::remove(const std::string& key){
    byte_size_ += key.size() + 1;
    table_.remove(key);
}

std::optional<std::string> Memtable::get(const std::string& key) const {
    return table_.get(key);
}

void Memtable::clear(){
    Memtable new_table;
    std::swap(table_, new_table.table_);
    std::swap(byte_size_, new_table.byte_size_);
}