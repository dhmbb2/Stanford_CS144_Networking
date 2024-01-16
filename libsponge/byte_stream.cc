#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), _buffer("") {}

size_t ByteStream::write(const string &data) {
    if (data.length() + _buf_ptr <= _capacity) {
        _buffer += data;
        _buf_ptr += data.length();
        _bytes_written += data.length();
        return data.length();
    } else {
        _buffer += data.substr(0, _capacity - _buf_ptr);
        int len_written = _capacity - _buf_ptr;
        _buf_ptr = _capacity;
        _bytes_written += len_written;
        return len_written;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return _buffer.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _buffer = _buffer.substr(len);
    _bytes_popped += len;
    _buf_ptr -= len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() {_input_ended = true;}

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _buf_ptr; }

bool ByteStream::buffer_empty() const { return _buf_ptr == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_popped; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buf_ptr; }
