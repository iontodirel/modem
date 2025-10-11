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

bool basic_bitstream_converter_adapter::try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const
{
    return converter.try_decode(bitstream, p);
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

bool basic_bitstream_converter::try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const
{
    return false;
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
    return encode_fx25_bitstream3(p, preamble_flags, postamble_flags);
}

bool fx25_bitstream_converter::try_decode(const std::vector<uint8_t>& bitstream, aprs::router::packet& p) const
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
    // 0 1 1 0 0 0 0 0 = 0x60                                            starting value
    //   ~~~
    // 0 1 1 0 0 1 0 1 = 0x60 | (ssid + '0') = 0x65                      append ssid
    //         ~~~~~~~
    // 0 1 1 0 1 0 1 0 = 0x60 | (ssid + '0') << 1 = 0x6A                 append ssid
    //       ~~~~~~~
    // 0 1 1 0 1 0 1 1 = 0x60 | (ssid + '0') << 1 | 0x01 = 0x6B          mark as last address
    //               ~
    // 1 1 1 0 1 0 1 1 = 0x60 | (ssid + '0') << 1 | 0x01 | 0x80 = 0xEB   mark address as used
    // ~

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
    std::vector<uint8_t> frame = encode_frame(p);

    std::vector<uint8_t> frame_bits;

    frame_bits.reserve(frame.size() * 8);

    bytes_to_bits(frame.begin(), frame.end(), std::back_inserter(frame_bits));

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