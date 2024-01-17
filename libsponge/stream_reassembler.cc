#include "stream_reassembler.hh"

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

    if (eof) {_eof=true;}
    
    std::string stored_string = data;

    if (index > _next_index) {
        // if the index is greater than the next index, then we need to store the substring

        if (stored_string.length() > _unassembled_capacity)
            stored_string = stored_string.substr(0, _unassembled_capacity);

        for (auto it = _buffer.begin(); it != _buffer.end(); ) {
            size_t tmp_idx = get<0>(*it);
            std::string tmp_str = get<1>(*it);
            // if the new substring contains the stored substring, ignore the new substring
            if (tmp_idx >= index && tmp_idx + tmp_str.length() <= index + stored_string.length()) {
                _unassembled_bytes -= get<1>(*it).length();
                it = _buffer.erase(it);
                continue;
            } 
            // if the new substring is contained by some stored string, no need to add
            if (tmp_idx < index && tmp_idx + tmp_str.length() > index + stored_string.length()) {
                return;
            }
            ++it;
        }
        
        _buffer.push_back(std::make_tuple(index, stored_string));
        _unassembled_bytes += stored_string.length();
        _unassembled_capacity -= stored_string.length();
    } else {
        if (_next_index - index > stored_string.length()) return;
        // if index received is smaller than _next_index, ignore the overlapping substring
        stored_string = stored_string.substr(_next_index - index);

        // if the index is the next index, we push it directly
        int num_written = _output.write(stored_string);
        _unassembled_capacity -= num_written;
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
                    it = _buffer.erase(it);
                    continue;
                }
                
                flag = true;
                // ignore overplapping string
                tmp_str = tmp_str.substr(_next_index - tmp_idx); 
                _output.write(tmp_str);
                _next_index += tmp_str.length();
                _unassembled_bytes -= tmp_str.length();
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