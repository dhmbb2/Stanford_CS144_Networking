#include "stream_reassembler.hh"
#include<iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // update capacity in case the bytestream has been read
    _update_unassembled_capacity();

    if (_unassembled_capacity >= 1ul<<62) {
        std::cout << "caosinima" << '\n';
        return;
    }

    if (eof) {_eof=true;}
    
    std::string string_to_store = data;
    size_t idx = index;

    if (idx > _next_index) {
        // if the index is greater than the next index, then we need to store the substring

        // do some merging 
        for (auto it = _buffer.begin(); it != _buffer.end(); ) {
            size_t tmp_idx = get<0>(*it);
            std::string tmp_str = get<1>(*it);
            // if the new substring contains the stored substring, ignore the new substring
            if (tmp_idx >= idx && tmp_idx + tmp_str.length() <= idx + string_to_store.length()) {
                _unassembled_bytes -= tmp_str.length();
                _unassembled_capacity += tmp_str.length();
                it = _buffer.erase(it);
                continue;
            } 
            // if the new substring is contained by some stored string, no need to add
            if (tmp_idx <= idx && tmp_idx + tmp_str.length() >= idx + string_to_store.length()) {
                return;
            }
            // if there is any overlap, merge them together
            // case 1: [---stingtostore--{--]---storedstring---}
            if (idx < tmp_idx && idx + string_to_store.length() >= tmp_idx) {
                _unassembled_bytes -= tmp_str.length();
                _unassembled_capacity += tmp_str.length();
                string_to_store = string_to_store + tmp_str.substr(idx + string_to_store.length() - tmp_idx);
                it = _buffer.erase(it);
                continue;
            }
            // case 2: {---storedstring--[--}---stringtostore---]
            if (tmp_idx < idx && tmp_idx + tmp_str.length() >= idx) {
                _unassembled_bytes -= tmp_str.length();
                _unassembled_capacity += tmp_str.length();
                string_to_store = tmp_str + string_to_store.substr(tmp_idx + tmp_str.length() - idx);
                idx = tmp_idx;
                it = _buffer.erase(it);
                continue;
            }
            ++it;
        }

        if (string_to_store.length() > _unassembled_capacity)
            string_to_store = string_to_store.substr(0, _unassembled_capacity);
        
        _buffer.push_back(std::make_tuple(idx, string_to_store));
        _unassembled_bytes += string_to_store.length();
        _unassembled_capacity -= string_to_store.length();
    } else {    
        if (_next_index - index > string_to_store.length()) return;
        // if index received is smaller than _next_index, ignore the overlapping substring
        string_to_store = string_to_store.substr(_next_index - index);

        // if the index is the next index, we push it directly
        int num_written = _output.write(string_to_store);
        _unassembled_capacity -= num_written; // this may cause _unassembled_capacity to be temporarily overflowed, but i don't want to change this
        _next_index += num_written; // notice that there can be some special case

        // if the index is the next index, we need to check if there are any substrings in the buffer that can be pushed
        bool flag = false;
        while (_unassembled_bytes != 0) {
            flag = false;
            for (auto it = _buffer.begin(); it != _buffer.end(); ) {
                size_t tmp_idx = get<0>(*it);
                if (tmp_idx > _next_index) {++it; continue;}
                std::string tmp_str = get<1>(*it);
                if (_next_index - tmp_idx >= tmp_str.length()) {
                    _unassembled_bytes -= tmp_str.length();
                    _unassembled_capacity += tmp_str.length();
                    it = _buffer.erase(it);
                    continue;
                }
                
                flag = true;
                // ignore overplapping string
                tmp_str = tmp_str.substr(_next_index - tmp_idx); 
                num_written = _output.write(tmp_str);
                // since tmp_str has changed, can't use tmp_str here
                _unassembled_bytes -= _next_index - tmp_idx + num_written; // very tricky
                _unassembled_capacity += get<1>(*it).length() - num_written;
                _next_index += num_written;
                // should have pushed the unassemble bytes (if num_written < tmp_str.length()), but i'm lazy
                it = _buffer.erase(it);
                break;
            }
            if (!flag) break;
        }
    }

    // end the input_stream if eof is true
    if (empty() && _eof) _output.end_input();

}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return !_unassembled_bytes; }