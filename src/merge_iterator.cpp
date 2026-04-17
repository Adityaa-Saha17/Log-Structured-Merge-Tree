#include "../include/merge_iterator.h"

EntryStream::EntryStream(std::vector<IterEntry> entries, int source_id)
    : entries_ (std::move(entries)), pos_(0) {
    for(auto &e : entries) e.source_id = source_id;
}

bool EntryStream::has_next() const {return pos_ < entries_.size();}

const IterEntry& EntryStream::peek() const {return entries_[pos_];}

IterEntry EntryStream::pop() {return entries_[pos_++];}

MergeIterator::MergeIterator(std::vector<std::shared_ptr<EntryStream>> streams, const std::string& start, const std::string& end)
    : streams_(std::move(streams)), heap_([](const Pair& a, const Pair& b){return a.first > b.first;}), start_(start), end_(end) {
    for(int i = 0; i < static_cast<int>(streams_.size()); i++){
        advance_stream(i);
    }
}

void MergeIterator::advance_stream(int idx){
    auto& s = streams_[idx];
    while(s->has_next() && !start_.empty() && s->peek().key < start_){
        s->pop();
    }
    if(s->has_next() && (end_.empty() || s->peek().key <= end_)){
        heap_.push({s->peek(), idx});
    }
}

bool MergeIterator::has_next() const { return !heap_.empty(); }

IterEntry MergeIterator::next() {
    auto [entry, idx] = heap_.top();
    heap_.pop();
    streams_[idx]->pop();

    if(streams_[idx]->has_next()){
        auto& nxt = streams_[idx]->peek();
        if(end_.empty() || nxt.key <= end_){
            heap_.push({ nxt, idx });
        }
    }

    while(!heap_.empty() && heap_.top().first.key == entry.key){
        auto [dup, dup_idx] = heap_.top();
        heap_.pop();
        streams_[dup_idx]->pop();
        if(streams_[dup_idx]->has_next()){
            auto& nxt = streams_[dup_idx]->peek();
            if(end_.empty() || nxt.key <= end_){
                heap_.push({nxt, dup_idx});
            }
        }
    }
    return entry;
}