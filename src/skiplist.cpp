#include "skiplist.h"
#include <cstring>
#include <ctime>

SkipList::SkipList(): current_level_(0), size_(0) {
    head_ = new SkipNode("", "", MAX_LEVEL);
}

SkipList::~SkipList(){
    if(!head_) return;
    SkipNode* cur = head_;
    while(cur){
        SkipNode* next = cur->forward[0];
        delete cur;
        cur = next;
    }
}

int SkipList::random_level(){
    int level = 0;
    while(static_cast<float>(rand())/ RAND_MAX < P && level < MAX_LEVEL){
        level++;
    }
    return level;
}

void SkipList::put(const std::string& key, const std::string& value){
    std::vector<SkipNode*> update(MAX_LEVEL + 1);
    SkipNode* cur = head_;

    for(int i = current_level_; i >= 0; i--){
        while(cur->forward[i] && cur->forward[i]->key < key){
            cur = cur->forward[i];
        }
        update[i] = cur;
    }

    cur = cur->forward[0];
    if(cur && cur->key == key){
        cur->value = value;
        cur->tombstone = false;
        return;
    }

    int new_level = random_level();
    if(new_level > current_level_){
        for(int i = current_level_ + 1; i <= new_level; i++){
            update[i] = head_;
        }
        current_level_ = new_level;
    }

    SkipNode* newNode = new SkipNode(key, value, new_level);
    for(int i = 0; i <= new_level; i++){
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }
    size_++;
}

void SkipList::remove(const std::string& key){
    std::vector<SkipNode*> update(MAX_LEVEL+1);
    SkipNode* cur = head_;

    for(int i = current_level_; i >= 0; i--){
        while(cur->forward[i] && cur->forward[i]->key < key){
            cur = cur->forward[i];
        }
        update[i] = cur;
    }

    SkipNode* target = cur->forward[0];
    if(target && target->key == key){
        target->tombstone = true;
        target->value.clear();
        return;
    }

    int new_level = random_level();
    if(new_level > current_level_){
        for(int i = current_level_ + 1; i <= new_level; i++){
            update[i] = head_;
        }
        current_level_ = new_level;
    }

    SkipNode* newNode = new SkipNode(key, "", new_level, true);
    for(int i = 0; i <= new_level; i++){
        newNode->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = newNode;
    }
    size_++;
}

std::optional<std::string>  SkipList::get(const std::string& key) const {
    SkipNode* cur = head_;
    for(int i = current_level_; i >= 0; i--){
        while(cur->forward[i] && cur->forward[i]->key < key){
            cur = cur->forward[i];
        }
    }
    cur = cur->forward[0];

    if(cur && cur->key == key){
        if(cur->tombstone) return std::nullopt;
        return cur->value;
    }
    return std::nullopt;
}