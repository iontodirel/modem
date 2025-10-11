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
    bool try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const;
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
    virtual bool try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const = 0;
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
    bool try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const override;

private:
    basic_bitstream_converter converter;
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

template<typename It>
void nrzi_encode(It first, It last);

template<typename OutputIt>
void add_hdlc_flags(OutputIt out, int count);

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
    //   Output: 1 1 1 1 1 0 1 0  (0 stuffed after 5th and 6th 1)

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

std::vector<uint8_t> encode_basic_bitstream(const aprs::router::packet& p, int preamble_flags, int postamble_flags);

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