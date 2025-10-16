// **************************************************************** //
// modem - APRS modem                                               // 
// Version 0.1.0                                                    //
// https://github.com/iontodirel/modem                              //
// Copyright (c) 2025 Ion Todirel                                   //
// **************************************************************** //
//
// bitstream.h
//
// MIT License
//
// Copyright (c) 2025 Ion Todirel
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <vector>

#include "external/aprsroute.hpp"

// **************************************************************** //
//                                                                  //
//                                                                  //
// basic_bitstream_converter                                        //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct basic_bitstream_converter
{
    std::vector<uint8_t> encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const;
    bool try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// fx25_bitstream_converter                                         //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct fx25_bitstream_converter
{
    std::vector<uint8_t> encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const;
    bool try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// bitstream_converter_base                                         //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct bitstream_converter_base
{
    virtual std::vector<uint8_t> encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const = 0;
    virtual bool try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const = 0;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// basic_bitstream_converter_adapter                                //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct basic_bitstream_converter_adapter : public bitstream_converter_base
{
    std::vector<uint8_t> encode(const aprs::router::packet& p, int preamble_flags = 45, int postamble_flags = 5) const override;
    bool try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const override;

private:
    basic_bitstream_converter converter;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
// fx25_bitstream_converter_adapter                                 //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct fx25_bitstream_converter_adapter : public bitstream_converter_base
{
    std::vector<uint8_t> encode(const aprs::router::packet& p, int preamble_flags = 45, int postamble_flags = 5) const override;
    bool try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const override;

private:
    fx25_bitstream_converter converter;
};

// **************************************************************** //
//                                                                  //
//                                                                  //
//                                                                  //
// bitstream routines                                               //
//                                                                  //
// bytes_to_bits, bits_to_bytes, compute_crc                        //
// bit_stuff, nrzi_encode, add_hdlc_flags                           //
// encode_basic_bitstream                                           //
//                                                                  //
//                                                                  //
//                                                                  //
// **************************************************************** //

template<typename InputIt, typename OutputIt>
void bytes_to_bits(InputIt first, InputIt last, OutputIt out);

template<typename InputIt, typename OutputIt>
void bits_to_bytes(InputIt first, InputIt last, OutputIt out);

template<typename InputIt>
std::array<uint8_t, 2> compute_crc(InputIt first, InputIt last);

template<typename InputIt, typename OutputIt>
void bit_stuff(InputIt first, InputIt last, OutputIt out);

template<typename InputIt, typename OutputIt>
void bit_unstuff(InputIt first, InputIt last, OutputIt out);

template<typename It>
void nrzi_encode(It first, It last);

template<typename It>
void nrzi_decode(It first, It last);

template<typename OutputIt>
void add_hdlc_flags(OutputIt out, int count);

template<typename It>
It find_last_consecutive_hdlc_flag(It first, It last);

template<typename It>
It find_first_hdlc_flag(It first, It last);

template<typename InputIt, typename OutputIt>
inline void bytes_to_bits(InputIt first, InputIt last, OutputIt out)
{
    // Converts bytes to individual bits (LSB-first per byte)
    // 
    // Example: byte 0x7E (01111110) -> bits [0,1,1,1,1,1,1,0]

    for (auto it = first; it != last; ++it)
    {
        uint8_t byte = *it;
        for (int i = 0; i < 8; ++i)
        {
            *out++ = (byte >> i) & 1;  // Extract bits LSB-first
        }
    }
}

template<typename InputIt, typename OutputIt>
inline void bits_to_bytes(InputIt first, InputIt last, OutputIt out)
{
    // Converts individual bits back to bytes (LSB-first per byte)
    // 
    // Example: bits [0,1,1,1,1,1,1,0] -> byte 0x7E (01111110)
    auto it = first;
    while (it != last)
    {
        uint8_t byte = 0;
        for (int i = 0; i < 8 && it != last; ++i)
        {
            if (*it++)
            {
                byte |= (1 << i);  // Set bit i if input bit is 1
            }
        }
        *out++ = byte;
    }
}

template<typename InputIt>
std::array<uint8_t, 2> compute_crc(InputIt first, InputIt last)
{
    // Computes CRC-16-CCITT checksum for error detection in AX.25 frames
    // Uses reversed polynomial 0x8408 and processes bits LSB-first
    // 
    // Returns 2-byte CRC in little-endian format [low_byte, high_byte]

    const uint16_t poly = 0x8408; // CRC-16-CCITT reversed polynomial

    uint16_t crc = 0xFFFF;

    for (auto it = first; it != last; ++it)
    {
        uint8_t byte = *it;
        for (int i = 0; i < 8; ++i)
        {
            uint8_t bit = (byte >> i) & 1;  // LSB-first
            uint8_t xor_in = (crc ^ bit) & 0x01;
            crc >>= 1;
            if (xor_in)
            {
                crc ^= poly;
            }
        }
    }

    crc ^= 0xFFFF;
    return { static_cast<uint8_t>(crc & 0xFF),
            static_cast<uint8_t>((crc >> 8) & 0xFF) };
}

template<typename InputIt, typename OutputIt>
inline void bit_stuff(InputIt first, InputIt last, OutputIt out)
{
    // Inserts a 0-bit after five consecutive 1-bits to prevent false flag detection
    // Prevents data from accidentally looking like the HDLC flag byte (0x7E = 01111110)
    // 
    // Example:
    // 
    //   Input:  1 1 1 1 1 1 0
    //           ~~~~~~~~~
    //   Output: 1 1 1 1 1 0 1 0  (0 stuffed after 5th and 6th 1)
    //                     ~

    int count = 0;

    for (auto it = first; it != last; ++it)
    {
        *out++ = *it;  // Output the bit

        if (*it == 1)
        {
            count += 1;
            if (count == 5)
            {
                *out++ = 0;  // Stuff a zero
                count = 0;
            }
        }
        else
        {
            count = 0;
        }
    }
}

template<typename InputIt, typename OutputIt>
inline void bit_unstuff(InputIt first, InputIt last, OutputIt out)
{
    // Removes stuffed 0-bits that were inserted after five consecutive 1-bits
    // This is the inverse operation of bit_stuff
    // 
    // Example:
    // 
    //   Input:  1 1 1 1 1 0 1 0  (0 stuffed after 5th 1)
    //   Output: 1 1 1 1 1 1 0

    int count = 0;

    for (auto it = first; it != last; ++it)
    {
        if (*it == 1)
        {
            *out++ = *it;
            count += 1;
        }
        else  // *it == 0
        {
            if (count == 5)
            {
                // This is a stuffed bit, skip it
                count = 0;
            }
            else
            {
                // This is a real data bit
                *out++ = *it;
                count = 0;
            }
        }
    }
}

template<typename It>
inline void nrzi_encode(It first, It last)
{
    // Encodes bitstream in-place to ensure signal transitions for clock recovery
    // NRZI: 0-bit = toggle level, 1-bit = keep level
    // 
    // Example:
    // 
    //   Input:  1 0 1 1 0 0 1
    //   Output: 0 1 1 1 0 1 1

    int level = 0; // Start at level 0

    for (auto it = first; it != last; ++it)
    {
        if (*it == 0)
        {
            level ^= 1;
        }
        *it = level;
    }
}

template<typename It>
inline void nrzi_decode(It first, It last)
{
    if (first == last) return;

    int prev = *first;
    *first = 0;  // First bit ambiguous, often set to 0

    for (auto it = first + 1; it != last; ++it)
    {
        int curr = *it;
        *it = (curr == prev) ? 1 : 0;  // No transition=1, transition=0
        prev = curr;
    }
}

template<typename OutputIt>
inline void add_hdlc_flags(OutputIt out, int count)
{
    constexpr uint8_t HDLC_FLAG = 0x7E;  // 01111110

    for (int j = 0; j < count; ++j)
    {
        for (int i = 0; i < 8; ++i)
        {
            *out++ = (HDLC_FLAG >> i) & 1;
        }
    }
}

template<typename It>
inline It find_last_consecutive_hdlc_flag(It first, It last)
{
    // Finds the last flag in a sequence of consecutive HDLC flags
    // Returns iterator to the start of the last flag, or last if not found

    constexpr std::array<uint8_t, 8> flag_pattern = { 0, 1, 1, 1, 1, 1, 1, 0 };

    auto current_preamble_flag = std::search(first, last, flag_pattern.begin(), flag_pattern.end());

    if (current_preamble_flag == last)
    {
        return last;
    }

    auto last_preamble_flag = current_preamble_flag;

    while (true)
    {
        auto next_search_start = last_preamble_flag + 8;

        if (next_search_start >= last)
        {
            break;
        }

        auto next_flag = std::search(next_search_start, last, flag_pattern.begin(), flag_pattern.end());

        if (next_flag == next_search_start)
        {
            last_preamble_flag = next_flag;
        }
        else
        {
            // Found a gap or no more flags - frame data starts here
            break;
        }
    }

    return last_preamble_flag;
}

template<typename It>
inline It find_first_hdlc_flag(It first, It last)
{
    // Finds the first HDLC flag in the bitstream
    // Returns iterator to the start of the flag, or last if not found

    constexpr std::array<uint8_t, 8> flag_pattern = { 0, 1, 1, 1, 1, 1, 1, 0 };

    return std::search(first, last, flag_pattern.begin(), flag_pattern.end());
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// address                                                          //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct address
{
    std::string text;
    int n = 0;
    int N = 0;
    int ssid = 0;
    bool mark = false;
};

bool try_parse_address(std::string_view address_string, struct address& address);
std::string to_string(const struct address& address);

// **************************************************************** //
//                                                                  //
//                                                                  //
// AX.25                                                            //
//                                                                  //
// encode_header, encode_addresses, encode_address, encode_frame    //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> encode_header(const aprs::router::packet& p);

std::vector<uint8_t> encode_header(const address& from, const address& to, const std::vector<address>& path);

std::vector<uint8_t> encode_addresses(const std::vector<address>& path);

std::array<uint8_t, 7> encode_address(const struct address& address, bool last);

std::array<uint8_t, 7> encode_address(std::string_view address, int ssid, bool mark, bool last);

std::vector<uint8_t> encode_frame(const aprs::router::packet& p);

template <typename InputIt>
inline std::vector<uint8_t> encode_frame(const address& from, const address& to, const std::vector<address>& path, InputIt input_it_begin, InputIt input_it_end)
{
    std::vector<uint8_t> frame;

    std::vector<uint8_t> header = encode_header(from, to, path);
    frame.insert(frame.end(), header.begin(), header.end());

    frame.push_back(static_cast<uint8_t>(0x03));  // Control: UI frame
    frame.push_back(static_cast<uint8_t>(0xF0));  // PID: No layer 3 protocol

    std::vector<uint8_t> payload_bytes(input_it_begin, input_it_end);
    frame.insert(frame.end(), payload_bytes.begin(), payload_bytes.end());

    // Compute 16 bits CRC
    // Append CRC at the end of the frame

    std::array<uint8_t, 2> crc = compute_crc(frame.begin(), frame.end());
    frame.insert(frame.end(), crc.begin(), crc.end());

    return frame;
}

bool try_decode_frame(const std::vector<uint8_t>& frame_bytes, aprs::router::packet& p);

std::vector<uint8_t> encode_basic_bitstream(const aprs::router::packet& p, int preamble_flags, int postamble_flags);

std::vector<uint8_t> encode_basic_bitstream(const std::vector<uint8_t> frame, int preamble_flags, int postamble_flags);

template<typename It>
inline std::vector<uint8_t> encode_basic_bitstream(It frame_it_begin, It frame_it_end, int preamble_flags, int postamble_flags)
{
    std::vector<uint8_t> frame_bits;

    frame_bits.reserve(std::distance(frame_it_begin, frame_it_end) * 8);

    bytes_to_bits(frame_it_begin, frame_it_end, std::back_inserter(frame_bits));

    // Bit stuffing

    std::vector<uint8_t> stuffed_bits;

    bit_stuff(frame_bits.begin(), frame_bits.end(), std::back_inserter(stuffed_bits));

    // Build complete bitstream: preamble + data + postamble

    std::vector<uint8_t> bitstream;

    add_hdlc_flags(std::back_inserter(bitstream), preamble_flags);
    bitstream.insert(bitstream.end(), stuffed_bits.begin(), stuffed_bits.end());
    add_hdlc_flags(std::back_inserter(bitstream), postamble_flags);

    // NRZI encoding of the bitstream

    nrzi_encode(bitstream.begin(), bitstream.end());

    return bitstream;
}

void parse_address(std::string_view data, std::string& address, int& ssid, bool& mark);

void parse_address(std::string_view data, struct address& address);

void parse_addresses(std::string_view data, std::vector<address>& addresses);

bool try_decode_frame(const std::vector<uint8_t>& frame_bytes, aprs::router::packet& p);

bool try_decode_basic_bitstream(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read);

// **************************************************************** //
//                                                                  //
//                                                                  //
// FX.25                                                            //
//                                                                  //
// encode_fx25_frame                                                //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> encode_fx25_frame(const std::vector<uint8_t>& frame);

std::vector<uint8_t> encode_fx25_bitstream(const aprs::router::packet& p, int preamble_flags, int postamble_flags);

