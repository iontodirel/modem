// **************************************************************** //
// modem - APRS modem                                               // 
// Version 0.1.0                                                    //
// https://github.com/iontodirel/modem                              //
// Copyright (c) 2025 Ion Todirel                                   //
// **************************************************************** //
//
// bitstream.cpp
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

#include "bitstream.h"

#include <array>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <string>

extern "C" {
#include <correct.h>
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// basic_bitstream_converter_adapter                                //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> basic_bitstream_converter_adapter::encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const
{
    return converter.encode(p, preamble_flags, postamble_flags);
}

bool basic_bitstream_converter_adapter::try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const
{
    return converter.try_decode(bitstream, offset, p, read);
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// fx25_bitstream_converter_adapter                                //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> fx25_bitstream_converter_adapter::encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const
{
    return converter.encode(p, preamble_flags, postamble_flags);
}

bool fx25_bitstream_converter_adapter::try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const
{
    return converter.try_decode(bitstream, offset, p, read);
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// basic_bitstream_converter                                        //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> basic_bitstream_converter::encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const
{
    return encode_basic_bitstream(p, preamble_flags, postamble_flags);
}

bool basic_bitstream_converter::try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const
{
    return try_decode_basic_bitstream(bitstream, offset, p, read);
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// fx25_bitstream_converter                                        //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> fx25_bitstream_converter::encode(const aprs::router::packet& p, int preamble_flags, int postamble_flags) const
{
    return encode_fx25_bitstream(p, preamble_flags, postamble_flags);
}

bool fx25_bitstream_converter::try_decode(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read) const
{
    return false;
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// address                                                          //
//                                                                  //
//                                                                  //
// **************************************************************** //

bool try_parse_address(std::string_view address_string, struct address& address)
{
    std::string_view address_text = address_string;

    address.text = address_text;
    address.mark = false;
    address.ssid = 0;
    address.n = 0;
    address.N = 0;

    // Check to see if the address is used (ending with *)
    if (!address_text.empty() && address_text.back() == '*')
    {
        address.mark = true;
        address_text.remove_suffix(1); // remove the *
        address.text = address_text; // set the text to the address without the *
    }

    auto sep_position = address_text.find("-");

    // No separator found
    if (sep_position == std::string::npos)
    {
        if (!address_text.empty() && isdigit(address_text.back()))
        {
            address.n = address_text.back() - '0'; // get the last character as a number
            address_text.remove_suffix(1); // remove the digit from the address text

            // Validate the n is in the range 1-7
            if (address.n > 0 && address.n <= 7)
            {
                address.text = address_text;
            }
            else
            {
                address.n = 0;
            }
        }

        return true;
    }

    // Separator found, check if we have exactly one digit on both sides of the separator, ex WIDE1-1
    // If the address does not match the n-N format, we will treat it as a regular address ex address with SSID
    if (sep_position != std::string::npos &&
        std::isdigit(static_cast<int>(address_text[sep_position - 1])) &&
        (sep_position + 1) < address_text.size() && std::isdigit(static_cast<int>(address_text[sep_position + 1])) &&
        (sep_position + 2 == address_text.size()))
    {
        address.n = address_text[sep_position - 1] - '0';
        address.N = address_text[sep_position + 1] - '0';

        if (address.N >= 0 && address.N <= 7 && address.n > 0 && address.n <= 7)
        {
            address.text = address_text.substr(0, sep_position - 1); // remove the separator and both digits from the address text
        }
        else
        {
            address.n = 0;
            address.N = 0;
        }

        return true;
    }

    // Handle SSID parsing
    // Expecting the separator to be followed by a digit, ex: CALL-1
    if ((sep_position + 1) < address_text.size() && std::isdigit(static_cast<int>(address_text[sep_position + 1])))
    {
        std::string ssid_str = std::string(address_text.substr(sep_position + 1));

        // Check for a single digit or two digits, ex: CALL-1 or CALL-12
        if (ssid_str.size() == 1 || (ssid_str.size() == 2 && std::isdigit(static_cast<int>(ssid_str[1]))))
        {
            int ssid;
            try
            {
                ssid = std::atoi(ssid_str.c_str());
            }
            catch (const std::invalid_argument&)
            {
                return true;
            }
            catch (const std::out_of_range&)
            {
                return true;
            }

            if (ssid >= 0 && ssid <= 15)
            {
                address.ssid = ssid;
                address.text = address_text.substr(0, sep_position);
            }
        }
    }

    return true;
}

std::string to_string(const struct address& address)
{
    if (address.text.empty())
    {
        return "";
    }

    std::string result = address.text;

    if (address.n > 0)
    {
        result += char('0' + address.n);
    }

    if (address.N > 0)
    {
        result += '-';
        result += char('0' + address.N);
    }

    if (address.ssid > 0)
    {
        result += '-';
        if (address.ssid < 10)
        {
            result += char('0' + address.ssid);
        }
        else
        {
            // 10 .. 15 => "1" + '0'..'5'
            result += '1';
            result += char('0' + (address.ssid - 10));
        }
    }

    if (address.mark)
    {
        result += '*';
    }

    return result;
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// trim                                                             //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, last - first + 1);
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// AX.25                                                            //
//                                                                  //
// encode_header, encode_addresses, encode_address, encode_frame    //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> encode_header(const address& from, const address& to, const std::vector<address>& path)
{
    std::vector<uint8_t> header;

    auto to_bytes = encode_address(to, false);
    header.insert(header.end(), to_bytes.begin(), to_bytes.end());

    auto from_bytes = encode_address(from, false);
    header.insert(header.end(), from_bytes.begin(), from_bytes.end());

    std::vector<uint8_t> addresses = encode_addresses(path);
    header.insert(header.end(), addresses.begin(), addresses.end());

    return header;
}

std::vector<uint8_t> encode_addresses(const std::vector<address>& path)
{
    std::vector<uint8_t> result;

    for (size_t i = 0; i < path.size(); i++)
    {
        bool last = (i == path.size() - 1);
        std::array<uint8_t, 7> addr_bytes = encode_address(path[i], last);
        result.insert(result.end(), addr_bytes.begin(), addr_bytes.end());
    }

    return result;
}

std::array<uint8_t, 7> encode_address(const struct address& address, bool last)
{
    std::string address_text = address.text;
    int ssid = 0;

    if (address.n > 0)
    {
        address_text += std::to_string(address.n);
    }

    if (address.N > 0)
    {
        ssid = address.N;
    }

    if (address.ssid > 0)
    {
        ssid = address.ssid;
    }

    return encode_address(address_text, ssid, address.mark, last);
}

std::array<uint8_t, 7> encode_address(std::string_view address, int ssid, bool mark, bool last)
{
    std::array<uint8_t, 7> data = {};

    // AX.25 addresses are always exactly 7 bytes:
    // - Bytes 0-5: Callsign (6 characters, space-padded)
    // - Byte 6: SSID + control bits

    for (size_t i = 0; i < 6; i++)
    {
        if (i < address.length())
        {
            // Shift each character left by 1 bit
            // Example: 'W' (0x57 = 01010111) << 1 = 0xAE (10101110)
            // AX.25 uses 7-bit encoding, leaving the LSB for other purposes
            data[i] = ((uint8_t)address[i]) << 1; // shift left by 1 bit
        }
        else
        {
            // Pad remaining positions with space character
            // Space ' ' (0x20 = 00100000) << 1 = 0x40 (01000000)
            data[i] = ' ' << 1; // pad with spaces
        }
    }

    // Encode the SSID byte (byte 6)
    // This byte contains multiple fields.
    //
    // Byte Format:
    //
    //      H-bit  Reserved     SSID        Last
    //   -------------------------------------------------------------
    //        7       6 5      4 3 2 1        0          bits
    //   -------------------------------------------------------------
    //        1        2          4           1
    //
    // Examples:
    //
    //   Callsign    SSID       |  H-bit  | 0x60  | SSID      | last
    //   ------------------   --+---------+-------+-----------+-------
    //   W7ION-5*    = 5        |  1      |  1 1  | 0 1 0 1   | 0        = 0x6B
    //   W7ION-12    = 12       |  0      |  1 1  | 1 1 0 0   | 0        = 0x78
    //   APRS-0      = 0        |  0      |  1 1  | 0 0 0 0   | 0        = 0x60
    //   WIDE1-1*    = 1        |  1      |  1 1  | 0 0 0 1   | 0        = 0x63
    //   WIDE2-2     = 2        |  0      |  1 1  | 0 0 1 0   | 0        = 0x64
    //   RELAY-15*   = 15       |  1      |  1 1  | 1 1 1 1   | 0        = 0x7F 
    //
    // Example with W7ION-5*
    // 
    //   0 1 1 0 0 0 0 0 = 0x60                                            starting value
    //     ~~~
    //   0 1 1 0 0 1 0 1 = 0x60 | (ssid + '0') = 0x65                      append ssid
    //           ~~~~~~~
    //   0 1 1 0 1 0 1 0 = 0x60 | (ssid + '0') << 1 = 0x6A                 append ssid
    //         ~~~~~~~
    //   0 1 1 0 1 0 1 1 = 0x60 | (ssid + '0') << 1 | 0x01 = 0x6B          mark as last address
    //                 ~
    //   1 1 1 0 1 0 1 1 = 0x60 | (ssid + '0') << 1 | 0x01 | 0x80 = 0xEB   mark address as used
    //   ~

    data[6] = 0b01100000; // 0 1 1 0 0 0 0 0, 0x60

    data[6] |= ((ssid + '0') << 1);

    if (last)
    {
        data[6] |= 0b00000001; // Extension bit (bit 0)
    }

    if (mark)
    {
        data[6] |= 0b10000000; // H-bit (bit 7)
    }

    return data;
}

std::vector<uint8_t> encode_frame(const aprs::router::packet& p)
{
    address to_address;
    try_parse_address(p.to, to_address);

    address from_address;
    try_parse_address(p.from, from_address);

    std::vector<address> path;
    for (const auto& address_string : p.path)
    {
        address path_address;
        try_parse_address(address_string, path_address);
        path.push_back(path_address);
    }

    return encode_frame(from_address, to_address, path, p.data.begin(), p.data.end());
}

std::vector<uint8_t> encode_frame(const address& from, const address& to, const std::vector<address>& path, std::string_view data)
{
    return encode_frame(from, to, path, data.begin(), data.end());
}

std::vector<uint8_t> encode_basic_bitstream(const aprs::router::packet& p, int preamble_flags, int postamble_flags)
{
    return encode_basic_bitstream(encode_frame(p), preamble_flags, postamble_flags);
}

std::vector<uint8_t> encode_basic_bitstream(const std::vector<uint8_t> frame, int preamble_flags, int postamble_flags)
{
    return encode_basic_bitstream(frame.begin(), frame.end(), preamble_flags, postamble_flags);
}

void parse_address(std::string_view data, std::string& address_text, int& ssid, bool& mark)
{
    address_text = std::string(6, '\0'); // addresses are 6 characters long

    for (size_t i = 0; i < 6; i++)
    {
        address_text[i] = ((uint8_t)data[i]) >> 1; // data is organized in 7 bits
    }

    ssid = (data[6] >> 1) & 0xf; // 0b00001111

    mark = (data[6] & 0b10000000) != 0; // 0x80 masks for the H bit in the last byte

    address_text = trim(address_text);
}

void parse_address(std::string_view data, struct address& address)
{
    std::string text;
    int ssid = 0;
    bool mark = false;

    parse_address(data, text, ssid, mark);

    std::string address_string = text;

    if (ssid > 0)
    {
        address_string += "-" + std::to_string(ssid);
    }

    if (mark)
    {
        address_string += "*";
    }

    try_parse_address(address_string, address);
}

void parse_addresses(std::string_view data, std::vector<address>& addresses)
{
    for (size_t i = 0; i < data.size(); i += 7)
    {
        struct address address;
        parse_address(data.substr(i, 7), address);
        addresses.push_back(address);
    }
}

bool try_decode_frame(const std::vector<uint8_t>& frame_bytes, aprs::router::packet& p)
{
    if (frame_bytes.size() < 18)
    {
        return false;
    }

    std::array<uint8_t, 2> computed_crc = compute_crc(frame_bytes.begin(), frame_bytes.end() - 2);
    std::array<uint8_t, 2> received_crc = { frame_bytes[frame_bytes.size() - 2], frame_bytes[frame_bytes.size() - 1] };

    if (computed_crc != received_crc)
    {
        return false;
    }

    address to_address;
    parse_address({ reinterpret_cast<const char*>(&(*frame_bytes.begin())), 7 }, to_address);

    address from_address;
    parse_address({ reinterpret_cast<const char*>(&(*(frame_bytes.begin() + 7))), 7 }, from_address);

    size_t addresses_start = 14;
    auto addresses_end = std::find(frame_bytes.begin() + addresses_start, frame_bytes.end() - 2, 0x03);

    if (addresses_end == frame_bytes.end() - 2)
    {
        return false;
    }

    size_t addresses_end_position = std::distance(frame_bytes.begin(), addresses_end);
    size_t addresses_length = addresses_end_position - addresses_start;

    if (addresses_length % 7 != 0)
    {
        return false;
    }

    std::vector<address> path_addresses;
    parse_addresses({ reinterpret_cast<const char*>(&(*(frame_bytes.begin() + addresses_start))), addresses_length }, path_addresses);

    size_t info_field_start = addresses_end_position + 2; // skip the Control Field byte and the Protocol ID byte
    size_t info_field_length = (frame_bytes.size() - 2) - info_field_start; // subtract CRC bytes

    if ((info_field_start + info_field_length) > (frame_bytes.size() - 2))
    {
        return false;
    }

    std::string info_field;
    if (info_field_length > 0)
    {
        info_field = std::string(frame_bytes.begin() + info_field_start, frame_bytes.begin() + info_field_start + info_field_length);
    }

    p.from = to_string(from_address);
    p.to = to_string(to_address);

    p.path.clear();

    for (const auto& path_address : path_addresses)
    {
        p.path.push_back(to_string(path_address));
    }

    p.data = info_field;

    return true;
}

bool try_decode_basic_bitstream(const std::vector<uint8_t>& bitstream, size_t offset, aprs::router::packet& p, size_t& read)
{
    read = 0;

    if (offset >= bitstream.size())
    {
        return false;
    }

    std::vector<uint8_t> decoded_bits(bitstream.begin() + offset, bitstream.end());

    nrzi_decode(decoded_bits.begin(), decoded_bits.end());

    auto last_preamble_flag = find_last_consecutive_hdlc_flag(decoded_bits.begin(), decoded_bits.end());
    if (last_preamble_flag == decoded_bits.end())
    {
        return false;
    }

    auto frame_data_start = last_preamble_flag + 8;
    if (frame_data_start >= decoded_bits.end())
    {
        return false;
    }
    
    auto frame_data_end = find_first_hdlc_flag(frame_data_start, decoded_bits.end());
    if (frame_data_end == decoded_bits.end())
    {
        return false;
    }

    // Distance within decoded_bits = distance from bitstream[offset]
    // So read is the correct delta to add to offset
    read = std::distance(decoded_bits.begin(), frame_data_end) + 8; // include the ending flag

    std::vector<uint8_t> unstuffed_bits;

    bit_unstuff(frame_data_start, frame_data_end, std::back_inserter(unstuffed_bits));

    std::vector<uint8_t> frame_bytes;

    bits_to_bytes(unstuffed_bits.begin(), unstuffed_bits.end(), std::back_inserter(frame_bytes));

    return try_decode_frame(frame_bytes, p);
}

// **************************************************************** //
//                                                                  //
//                                                                  //
// FX.25                                                            //
//                                                                  //
// encode_fx25_bitstream, encode_fx25_frame                         //
//                                                                  //
//                                                                  //
// **************************************************************** //

std::vector<uint8_t> encode_fx25_bitstream(const aprs::router::packet& p, int preamble_flags, int postamble_flags)
{
    // Create AX.25 frame from the packet
    // Convert AX.25 frame to bits
    // Bit-stuff the bits

    std::vector<uint8_t> ax25_frame = encode_frame(p);   
        
    std::vector<uint8_t> frame_bits;

    bytes_to_bits(ax25_frame.begin(), ax25_frame.end(), std::back_inserter(frame_bits));

    std::vector<uint8_t> stuffed_bits;

    bit_stuff(frame_bits.begin(), frame_bits.end(), std::back_inserter(stuffed_bits));

    // Build complete Ax.25 frame bits: preamble + stuffed bits + postamble

    std::vector<uint8_t> ax25_bits;

    add_hdlc_flags(std::back_inserter(ax25_bits), 1);
    ax25_bits.insert(ax25_bits.end(), stuffed_bits.begin(), stuffed_bits.end());
    add_hdlc_flags(std::back_inserter(ax25_bits), 1);

    // Create FX.25 frame

    std::vector<uint8_t> ax25_packet_bytes;

    bits_to_bytes(ax25_bits.begin(), ax25_bits.end(), std::back_inserter(ax25_packet_bytes));

    std::vector<uint8_t> fx25_frame = encode_fx25_frame(ax25_packet_bytes);

    if (fx25_frame.empty())
    {
        return {};
    }

    // Build complete bitstream: preamble + data + postamble

    std::vector<uint8_t> bitstream;

    add_hdlc_flags(std::back_inserter(bitstream), preamble_flags);
    bytes_to_bits(fx25_frame.begin(), fx25_frame.end(), std::back_inserter(bitstream));
    add_hdlc_flags(std::back_inserter(bitstream), postamble_flags);

    // NRZI encoding of the bitstream

    nrzi_encode(bitstream.begin(), bitstream.end());

    return bitstream;
}

std::vector<uint8_t> encode_fx25_frame(const std::vector<uint8_t>& packet_bytes)
{
    // FX.25 RS code modes from the specification
    // Each mode defines: correlation_tag, transmitted_size, data_size, check_bytes
    constexpr std::tuple<uint64_t, int, int, int> modes[] =
    {
        {0xB74DB7DF8A532F3EULL, 255, 239, 16},  // Tag_01: RS(255,239)
        {0x26FF60A600CC8FDEULL, 144, 128, 16},  // Tag_02: RS(144,128)
        {0xC7DC0508F3D9B09EULL,  80,  64, 16},  // Tag_03: RS(80,64)
        {0x8F056EB4369660EEULL,  48,  32, 16},  // Tag_04: RS(48,32)
        {0x6E260B1AC5835FAEULL, 255, 223, 32},  // Tag_05: RS(255,223)
        {0xFF94DC634F1CFF4EULL, 160, 128, 32},  // Tag_06: RS(160,128)
        {0x1EB7B9CDBC09C00EULL,  96,  64, 32},  // Tag_07: RS(96,64)
        {0xDBF869BD2DBB1776ULL,  64,  32, 32},  // Tag_08: RS(64,32)
    };

    // Select smallest RS code that fits
    uint64_t tag = 0;
    int total = 0, data_size = 0, check_size = 0;
    int mode_index = -1;
    for (int i = 0; i < 8; i++)
    {
        auto [t, tot, d, c] = modes[i];
        if (packet_bytes.size() <= d)
        {
            tag = t;
            total = tot;         // Total bytes transmitted (data + check)
            data_size = d;       // Data portion size
            check_size = c;      // Number of RS check bytes
            mode_index = i + 1;
            break;
        }
    }
    // FX.25 Frame Structure (transmitted left to right):
    // 
    // +-----------------+------------------------+--------------------+
    // | Correlation Tag |    AX.25 packet        |   RS Check Bytes   |
    // |    (8 bytes)    | (unmodified) + padding |   (16/32/64 bytes) |
    // +-----------------+------------------------+--------------------+
    //
    // The correlation tag tells receivers:
    // 
    //   1. This is an FX.25 frame (not plain AX.25)
    //   2. How many data and check bytes follow
    //
    // Non-FX.25 receivers see the correlation tag as random noise and ignore it.
    // They then see the AX.25 flags and sync up normally to decode the AX.25 packet.
    // The RS check bytes at the end are also ignored as noise.

    if (tag == 0)
    {
        // Packet too large for any FX.25 format
        return {};
    }

    std::vector<uint8_t> output;

    // Add correlation tag (8 bytes, transmitted LSB first)
    // This identifies the frame as FX.25 and specifies the format

    for (int i = 0; i < 8; i++)
    {
        output.push_back((tag >> (i * 8)) & 0xFF);
    }

    // Prepare the data block for RS encoding
    // The AX.25 packet bytes are placed here UNMODIFIED
    // This preserves backward compatibility - the AX.25 portion is unchange

    std::vector<uint8_t> rs_data_block(data_size, 0x00);

    // Copy the complete AX.25 packet(with flags, bit - stuffing, everything)
    // This is placed at the beginning of the data block exactly as-is
    // packet_bytes contains: [0x7E] [AX.25 frame with bit stuffing] [0x7E]

    std::copy(packet_bytes.begin(), packet_bytes.end(), rs_data_block.begin());
   
    // Pad the rest with 0x7E (HDLC flag pattern)
    // This padding allows the RS encoder to work with fixed block sizes
    // 0x7E is chosen because AX.25 receivers will see it as idle flags

    for (size_t i = packet_bytes.size(); i < data_size; i++)
    {
        rs_data_block[i] = 0x7E;
    }

    // At this point, rs_data_block contains:
    // [Complete unmodified AX.25 packet][0x7E padding to fill data_size]
    //
    // Reed - Solomon encoding
    // The RS encoder treats the data block as symbols and calculates parity
    // RS encoding does NOT modify the data portion!
    // It only ADDS check bytes for error correction
    //
    // Create RS encoder with:
    // 
    //   - polynomial 0x11d (x^8 + x^4 + x^3 + x^2 + 1)
    //   - fcr = 1 (first consecutive root)
    //   - prim = 1 (primitive element)

    correct_reed_solomon* rs = correct_reed_solomon_create(correct_rs_primitive_polynomial_8_4_3_2_0, 1, 1, check_size);

    if (rs == nullptr)
    {
        // Failed to create RS encoder
        return {};
    }

    std::vector<uint8_t> encoded_block(total);

    // Encode: creates data + check bytes
    // The first 'data_size' bytes are our data (unchanged)
    // The last 'check_size' bytes are the calculated RS parity

    ssize_t result = correct_reed_solomon_encode(rs, rs_data_block.data(), data_size, encoded_block.data());

    if (result != total)
    {
        correct_reed_solomon_destroy(rs);
        return {};
    }

    correct_reed_solomon_destroy(rs);

    // Append the encoded block to output
    // 
    // This contains:
    // 
    //   - First 'data_size' bytes: The EXACT SAME AX.25 packet + padding
    //   - Last 'check_size' bytes: RS parity for error correction

    output.insert(output.end(), encoded_block.begin(), encoded_block.end());

    // Final transmitted frame structure:
    // [8-byte correlation tag][Unmodified AX.25][0x7E padding][RS check bytes]
    //
    // The AX.25 packet remains completely unaltered, allowing:
    // 1. FX.25 receivers to apply error correction then extract AX.25
    // 2. Regular AX.25 receivers to ignore FX.25 overhead and decode normally

    return output;
}
