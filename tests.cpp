#include "bitstream.h"
#include "audio_stream.h"
#include "modem.h"
#include "demodulator.h"
#include "modulator.h"

#include <random>
#include <fstream>

#include <gtest/gtest.h>

std::vector<uint8_t> generate_random_bits(size_t count)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);

    std::vector<uint8_t> bits(count);
    for (size_t i = 0; i < count; i++)
    {
        bits[i] = dis(gen);
    }
    return bits;
}

TEST(address, to_string)
{
    address s;
    s.text = "WIDE";
    s.n = 2;
    s.N = 1;
    s.mark = false;
    EXPECT_TRUE(to_string(s) == "WIDE2-1");

    s.mark = true;
    EXPECT_TRUE(to_string(s) == "WIDE2-1*");

    s.mark = true;
    s.N = 0;
    EXPECT_TRUE(to_string(s) == "WIDE2*");

    s.N = 0;
    s.n = 0;
    EXPECT_TRUE(to_string(s) == "WIDE*");

    s = address{};
    s.text = "N0CALL";
    s.ssid = 10;
    EXPECT_TRUE(to_string(s) == "N0CALL-10");

    s = address{};
    s.text = "N0CALL";
    s.ssid = 10;
    s.mark = true;
    EXPECT_TRUE(to_string(s) == "N0CALL-10*");

    s = address{};
    s.text = "N0CALL-10";
    s.ssid = 10;
    EXPECT_TRUE(to_string(s) == "N0CALL-10-10"); // to_string preserves the text even if ssid is specified and results in an invalid address
}

TEST(dds_afsk_modulator_dft_demodulator, modulate_demodulate_8bits)
{
    std::vector<double> audio_buffer;

    std::vector<uint8_t> bitstream = { 0, 0, 1, 1, 0, 1, 0, 0 };

    dds_afsk_modulator modulator(1200.0, 2200.0, 1200, 48000, 1.0); // Coherent 1200 baud AFSK

    for (uint8_t bit : bitstream)
    {
        for (int i = 0; i < modulator.samples_per_bit(); ++i)
        {
            audio_buffer.push_back(modulator.modulate(bit));
        }
    }

    dft_demodulator demodulator(1200.0, 2200.0, 1200, 48000);

    std::vector<uint8_t> demodulated_bits = demodulator.demodulate(audio_buffer);
    
    EXPECT_EQ(bitstream, demodulated_bits);
}

TEST(dds_afsk_modulator_dft_demodulator, modulate_demodulate_random_100000bits)
{
    std::vector<double> audio_buffer;

    std::vector<uint8_t> bitstream = generate_random_bits(100'000);

    dds_afsk_modulator modulator(1200.0, 2200.0, 1200, 48000, 1.0); // Coherent 1200 baud AFSK

    for (uint8_t bit : bitstream)
    {
        for (int i = 0; i < modulator.samples_per_bit(); ++i)
        {
            audio_buffer.push_back(modulator.modulate(bit));
        }
    }

    dft_demodulator demodulator(1200.0, 2200.0, 1200, 48000);

    std::vector<uint8_t> demodulated_bits = demodulator.demodulate(audio_buffer);

    EXPECT_EQ(bitstream, demodulated_bits);
}

TEST(dds_afsk_modulator_dft_demodulator, modulate_demodulate_packet)
{
    std::vector<double> audio_buffer;

    aprs::router::packet p = { "N0CALL-10", "APZ001", { "WIDE1-1", "WIDE2-2" }, "Hello, APRS!" };

    std::vector<uint8_t> bitstream = encode_basic_bitstream(p, 45, 30);

    dds_afsk_modulator modulator(1200.0, 2200.0, 1200, 48000, 1.0); // Coherent 1200 baud AFSK

    for (uint8_t bit : bitstream)
    {
        for (int i = 0; i < modulator.samples_per_bit(); ++i)
        {
            audio_buffer.push_back(modulator.modulate(bit));
        }
    }

    dft_demodulator demodulator(1200.0, 2200.0, 1200, 48000);

    std::vector<uint8_t> demodulated_bits = demodulator.demodulate(audio_buffer);

    aprs::router::packet p2;

    size_t read = 0;
    try_decode_basic_bitstream(demodulated_bits, 0, p2, read);

    EXPECT_TRUE(p == p2);
}

TEST(modem, modulate_demodulate_packet)
{
    {
        dds_afsk_modulator_adapter modulator(1200.0, 2200.0, 1200, 48000);
        basic_bitstream_converter_adapter bitstream_converter;
        wav_audio_stream wav_stream("test.wav", true, 48000);

        aprs::router::packet p = { "N0CALL-10", "APZ001", { "WIDE1-1", "WIDE2-2" }, "Hello, APRS!" };

        modem m;
        m.baud_rate(1200);
        m.tx_delay(300);
        m.tx_tail(45);
        m.gain(0.3);
        m.preemphasis(true);
        m.initialize(wav_stream, modulator, bitstream_converter);

        m.transmit(p);

        wav_stream.close();
    }

    {
        std::vector<double> audio_buffer;

        wav_audio_stream wav_stream("test.wav", false, 48'000);

        while (true)
        {
            std::vector<double> audio_samples(4096);
            size_t read = wav_stream.read(audio_samples.data(), audio_samples.size());
            if (read == 0) break;
            audio_buffer.insert(audio_buffer.end(), audio_samples.begin(), audio_samples.begin() + read);
        }

        dft_demodulator demodulator(1200.0, 2200.0, 1200, 48000);

        std::vector<uint8_t> bitstream = demodulator.demodulate(audio_buffer);

        basic_bitstream_converter_adapter bitstream_converter;

        std::vector<aprs::router::packet> packets;
        size_t read = 0;
        size_t offset = 0;
        aprs::router::packet packet;
        while (offset < bitstream.size())
        {
            if (bitstream_converter.try_decode(bitstream, offset, packet, read))
            {
                packets.push_back(packet);
            }
            if (read == 0) break; // No more data
            offset += read;
        }
        wav_stream.close();

        EXPECT_TRUE(packets.size() == 1);
        EXPECT_TRUE(to_string(packets[0]) == "N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!");
    }
}

TEST(ax25, encode_frame)
{
    // N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!
    aprs::router::packet p = { "N0CALL-10", "APZ001", { "WIDE1-1", "WIDE2-2" }, "Hello, APRS!" };

    std::vector<uint8_t> frame = encode_frame(p);

    EXPECT_TRUE(frame.size() == 44);

    EXPECT_TRUE(frame == (std::vector<uint8_t>{
        // Destination: APZ001
        0x82, 0xA0, 0xB4, 0x60, 0x60, 0x62, 0x60,
        // Source: N0CALL-10
        0x9C, 0x60, 0x86, 0x82, 0x98, 0x98, 0x74,
        // Path 1: WIDE1-1
        0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x62,
        // Path 2: WIDE2-2* (last addr, end bit set)
        0xAE, 0x92, 0x88, 0x8A, 0x64, 0x40, 0x65,
        // Control, PID
        0x03, 0xF0,
        // Payload: "Hello, APRS!"
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x41, 0x50, 0x52, 0x53, 0x21,
        // CRC (FCS), little-endian
        0x50, 0x7B
    }));
}

TEST(ax25, encode_address)
{
    {
        std::array<uint8_t, 7> address = encode_address("N0CALL", 10, false, false);
        EXPECT_TRUE(address == (std::array<uint8_t, 7>{ 0x9C, 0x60, 0x86, 0x82, 0x98, 0x98, 0x74 }));
    }

    {
        std::array<uint8_t, 7> address = encode_address("WIDE2", 2, true, false);
        EXPECT_TRUE(address == (std::array<uint8_t, 7>{ 0xAE, 0x92, 0x88, 0x8A, 0x64, 0x40, 0xE4 }));
    }

    {
        std::array<uint8_t, 7> address = encode_address("APZ001", 0, false, true);
        EXPECT_TRUE(address == (std::array<uint8_t, 7>{ 0x82, 0xA0, 0xB4, 0x60, 0x60, 0x62, 0x61 }));
    }

    {
        std::array<uint8_t, 7> address = encode_address("WIDE1", 1, false, true);
        EXPECT_TRUE(address == (std::array<uint8_t, 7>{ 0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x63 }));
    }

    {
        std::array<uint8_t, 7> address = encode_address("WIDE2", 2, true, true);
        EXPECT_TRUE(address == (std::array<uint8_t, 7>{ 0xAE, 0x92, 0x88, 0x8A, 0x64, 0x40, 0xE5 }));
    }
}

TEST(ax25, parse_address)
{
    {
        std::string address;
        int ssid;
        bool mark;
        parse_address(std::string_view("\x9C\x60\x86\x82\x98\x98\x74", 7), address, ssid, mark);
        EXPECT_EQ(address, "N0CALL");
        EXPECT_EQ(ssid, 10);
        EXPECT_FALSE(mark);
    }

    {
        std::string address;
        int ssid;
        bool mark;
        parse_address(std::string_view("\xAE\x92\x88\x8A\x64\x40\xE4", 7), address, ssid, mark);
        EXPECT_EQ(address, "WIDE2");
        EXPECT_EQ(ssid, 2);
        EXPECT_TRUE(mark);
    }

    {
        std::string address;
        int ssid;
        bool mark;
        parse_address(std::string_view("\x82\xA0\xB4\x60\x60\x62\x61", 7), address, ssid, mark);
        EXPECT_EQ(address, "APZ001");
        EXPECT_EQ(ssid, 0);
        EXPECT_FALSE(mark);
    }

    {
        std::string address;
        int ssid;
        bool mark;
        parse_address(std::string_view("\xAE\x92\x88\x8A\x62\x40\x63", 7), address, ssid, mark);
        EXPECT_EQ(address, "WIDE1");
        EXPECT_EQ(ssid, 1);
        EXPECT_FALSE(mark);
    }

    {
        std::string address;
        int ssid;
        bool mark;
        parse_address(std::string_view("\xAE\x92\x88\x8A\x64\x5A\xE5", 7), address, ssid, mark);
        EXPECT_EQ(address, "WIDE2-");
        EXPECT_EQ(ssid, 2);
        EXPECT_TRUE(mark);
    }
}

TEST(ax25, try_decode_frame)
{
    // N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!
    std::vector<uint8_t> frame = {
        // Destination: APZ001
        0x82, 0xA0, 0xB4, 0x60, 0x60, 0x62, 0x60,
        // Source: N0CALL-10
        0x9C, 0x60, 0x86, 0x82, 0x98, 0x98, 0x74,
        // Path 1: WIDE1-1
        0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x62,
        // Path 2: WIDE2-2* (last addr, end bit set)
        0xAE, 0x92, 0x88, 0x8A, 0x64, 0x40, 0x65,
        // Control, PID
        0x03, 0xF0,
        // Payload: "Hello, APRS!"
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x41, 0x50, 0x52, 0x53, 0x21,
        // CRC (FCS), little-endian
        0x50, 0x7B
    };

    aprs::router::packet p;

    EXPECT_TRUE(try_decode_frame(frame, p));

    EXPECT_TRUE(to_string(p) == "N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!");
}

TEST(bitstream, encode_basic_bitstream)
{
    // N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!
    aprs::router::packet p = { "N0CALL-10", "APZ001", { "WIDE1-1", "WIDE2-2" }, "Hello, APRS!" };

    std::vector<uint8_t> bitstream = encode_basic_bitstream(p, 1, 1);

    EXPECT_TRUE(bitstream.size() == 368);

    EXPECT_TRUE(bitstream == (std::vector<uint8_t>{
        // Preamble HDLC flag (0x7E)
        1, 1, 1, 1, 1, 1, 1, 0,
        // Destination: APZ001
        1, 1, 0, 1, 0, 1, 0, 0,
        1, 0, 1, 0, 1, 1, 0, 0,
        1, 0, 0, 1, 1, 1, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 1, 1, 0,
        1, 1, 0, 1, 0, 0, 0, 1,
        0, 1, 0, 1, 0, 0, 0, 1,
        // Source: N0CALL-10
        0, 1, 1, 1, 1, 0, 1, 1,
        0, 1, 0, 1, 0, 0, 0, 1,
        0, 0, 0, 1, 0, 1, 0, 0,
        1, 1, 0, 1, 0, 1, 0, 0,
        1, 0, 1, 1, 1, 0, 1, 1,
        0, 1, 0, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 1, 1, 1, 0,
        // Path 1: WIDE1-1
        1, 1, 1, 1, 0, 0, 1, 1,
        0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 0, 1, 0, 1, 1,
        0, 0, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 1, 1, 1, 0,
        // Path 2: WIDE2-2
        1, 1, 1, 1, 0, 0, 1, 1,
        0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0, 
        1, 1, 0, 0, 1, 0, 1, 1,
        0, 1, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 0, 0, 1, 
        // Control, PID
        1, 1, 0, 1, 0, 1, 0, 1,        
        0, 1, 0, 1, 1, 1, 1, 1, 
        // Data: "Hello, APRS!"
        0, 1, 0, 0, 1, 0, 0, 1,        
        1, 0, 0, 1, 0, 0, 0, 1, 
        0, 1, 1, 1, 0, 0, 0, 1,
        0, 1, 1, 1, 0, 0, 0, 1, 
        1, 1, 1, 1, 0, 0, 0, 1,
        0, 1, 1, 1, 0, 0, 1, 0,
        1, 0, 1, 0, 1, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 0, 1,
        0, 1, 0, 1, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 1, 0, 
        0, 0, 1, 0, 0, 1, 1, 0,
        0, 1, 0, 1, 0, 0, 1, 0,  
        // CRC (FCS), little-endian
        1, 0, 1, 0, 0, 1, 1, 0,
        0, 0, 1, 1, 1, 1, 1, 0,
        // Postamble HDLC flag (0x7E)
        1, 1, 1, 1, 1, 1, 1, 0,
    }));
}

TEST(bitstream, try_decode_basic_bitstream)
{
    std::vector<uint8_t> bitstream = {
        // Preamble HDLC flag (0x7E)
        1, 1, 1, 1, 1, 1, 1, 0,
        // Destination: APZ001
        1, 1, 0, 1, 0, 1, 0, 0,
        1, 0, 1, 0, 1, 1, 0, 0,
        1, 0, 0, 1, 1, 1, 0, 0,
        1, 0, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 1, 1, 0,
        1, 1, 0, 1, 0, 0, 0, 1,
        0, 1, 0, 1, 0, 0, 0, 1,
        // Source: N0CALL-10
        0, 1, 1, 1, 1, 0, 1, 1,
        0, 1, 0, 1, 0, 0, 0, 1,
        0, 0, 0, 1, 0, 1, 0, 0,
        1, 1, 0, 1, 0, 1, 0, 0,
        1, 0, 1, 1, 1, 0, 1, 1,
        0, 1, 0, 0, 0, 1, 0, 0,
        1, 0, 0, 1, 1, 1, 1, 0,
        // Path 1: WIDE1-1
        1, 1, 1, 1, 0, 0, 1, 1,
        0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 0, 1, 0, 1, 1,
        0, 0, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 1, 1, 1, 0,
        // Path 2: WIDE2-2
        1, 1, 1, 1, 0, 0, 1, 1,
        0, 0, 1, 0, 0, 1, 0, 0,
        1, 0, 1, 1, 0, 1, 0, 0,
        1, 1, 0, 0, 1, 0, 1, 1,
        0, 1, 1, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 0, 0, 1,
        // Control, PID
        1, 1, 0, 1, 0, 1, 0, 1,
        0, 1, 0, 1, 1, 1, 1, 1,
        // Data: "Hello, APRS!"
        0, 1, 0, 0, 1, 0, 0, 1,
        1, 0, 0, 1, 0, 0, 0, 1,
        0, 1, 1, 1, 0, 0, 0, 1,
        0, 1, 1, 1, 0, 0, 0, 1,
        1, 1, 1, 1, 0, 0, 0, 1,
        0, 1, 1, 1, 0, 0, 1, 0,
        1, 0, 1, 0, 1, 1, 0, 1,
        1, 0, 1, 0, 1, 0, 0, 1,
        0, 1, 0, 1, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 1, 0,
        0, 0, 1, 0, 0, 1, 1, 0,
        0, 1, 0, 1, 0, 0, 1, 0,
        // CRC (FCS), little-endian
        1, 0, 1, 0, 0, 1, 1, 0,
        0, 0, 1, 1, 1, 1, 1, 0,
        // Postamble HDLC flag (0x7E)
        1, 1, 1, 1, 1, 1, 1, 0,
    };

    aprs::router::packet p;

    size_t read = 0;
    EXPECT_TRUE(try_decode_basic_bitstream(bitstream, 0, p, read));

    EXPECT_TRUE(read == bitstream.size());

    EXPECT_TRUE(to_string(p) == "N0CALL-10>APZ001,WIDE1-1,WIDE2-2:Hello, APRS!");
}

TEST(bitstream, try_decode_basic_bitstream_offset)
{
    std::ifstream file("bitstream.txt");
    if (!file.is_open())
    {
        FAIL() << "Failed to open bitstream.txt";
    }

    std::vector<uint8_t> bitstream;
    char c;
    while (file.get(c))
    {
        if (c == '0') bitstream.push_back(0);
        else if (c == '1') bitstream.push_back(1);
    }

    std::vector<aprs::router::packet> packets;
    size_t read = 0;
    size_t offset = 0;
    while (offset < bitstream.size())
    {
        aprs::router::packet packet;
        if (try_decode_basic_bitstream(bitstream, offset, packet, read))
        {
            packets.push_back(packet);
        }
        if (read == 0) break; // No more data
        offset += read;
    }

    EXPECT_TRUE(packets.size() == 804);
}

TEST(bitstream, nrzi_encode)
{
    {
        std::vector<uint8_t> bits = { 1,0,1,1,0,0,1 };
        nrzi_encode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 0, 1, 1, 1, 0, 1, 1 }));
    }

    {
        std::vector<uint8_t> bits = { 1,1,1,1,1,1,1 };
        nrzi_encode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 0, 0, 0, 0, 0, 0, 0 }));
    }

    {
        std::vector<uint8_t> bits = { 0,0,0,0,0,0,0 };
        nrzi_encode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 1, 0, 1, 0, 1, 0, 1 }));
    }
}

TEST(bitstream, nrzi_decode)
{
    {
        std::vector<uint8_t> bits = { 0,1,1,1,0,1,1 };
        nrzi_decode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 0, 0, 1, 1, 0, 0, 1 }));
    }

    {
        std::vector<uint8_t> bits = { 0,0,0,0,0,0,0 };
        nrzi_decode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 0, 1, 1, 1, 1, 1, 1 }));
    }

    {
        std::vector<uint8_t> bits = { 1,0,1,0,1,0,1 };
        nrzi_decode(bits.begin(), bits.end());
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 0, 0, 0, 0, 0, 0, 0 }));
    }
}

TEST(bitstream, compute_crc)
{
    std::vector<uint8_t> frame = {
        // Destination: APZ001
        0x82, 0xA0, 0xB4, 0x60, 0x60, 0x62, 0x60,
        // Source: N0CALL-10
        0x9C, 0x60, 0x86, 0x82, 0x98, 0x98, 0x74,
        // Path 1: WIDE1-1
        0xAE, 0x92, 0x88, 0x8A, 0x62, 0x40, 0x62,
        // Path 2: WIDE2-2* (last addr, end bit set)
        0xAE, 0x92, 0x88, 0x8A, 0x64, 0x40, 0x65,
        // Control, PID
        0x03, 0xF0,
        // Payload: "Hello, APRS!"
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x41, 0x50, 0x52, 0x53, 0x21
    };

    std::array<uint8_t, 2> crc = compute_crc(frame.begin(), frame.end());

    EXPECT_TRUE(crc == (std::array<uint8_t, 2>{ 0x50, 0x7B }));
}

TEST(bitstream, bytes_to_bits)
{
    {
        std::vector<uint8_t> bytes = { 0xA5 }; // 10100101
        std::vector<uint8_t> bits;
        bytes_to_bits(bytes.begin(), bytes.end(), std::back_inserter(bits));
        EXPECT_TRUE(bits == (std::vector<uint8_t>{ 1, 0, 1, 0, 0, 1, 0, 1 }));
    }

    {
        std::vector<uint8_t> bytes = { 0xFF, 0x00, 0x55 }; // 11111111 00000000 01010101
        std::vector<uint8_t> bits;
        bytes_to_bits(bytes.begin(), bytes.end(), std::back_inserter(bits));
        EXPECT_TRUE(bits == (std::vector<uint8_t> {
            1, 1, 1, 1, 1, 1, 1, 1,  // 0xFF LSB-first
            0, 0, 0, 0, 0, 0, 0, 0,  // 0x00 LSB-first
            1, 0, 1, 0, 1, 0, 1, 0   // 0x55 LSB-first (bit 0, bit 1, bit 2, ...)
        }));
    }
}

TEST(bitstream, bits_to_bytes)
{
    {
        std::vector<uint8_t> bits = { 1, 0, 1, 0, 0, 1, 0, 1 }; // 0xA5
        std::vector<uint8_t> bytes;
        bits_to_bytes(bits.begin(), bits.end(), std::back_inserter(bytes));
        EXPECT_TRUE(bytes == (std::vector<uint8_t>{ 0xA5 }));
    }

    {
        std::vector<uint8_t> bits = {
            1, 1, 1, 1, 1, 1, 1, 1,  // 0xFF LSB-first
            0, 0, 0, 0, 0, 0, 0, 0,  // 0x00 LSB-first
            1, 0, 1, 0, 1, 0, 1, 0   // 0x55 LSB-first (bit 0, bit 1, bit 2, ...)
        };
        std::vector<uint8_t> bytes;
        bits_to_bytes(bits.begin(), bits.end(), std::back_inserter(bytes));
        EXPECT_TRUE(bytes == (std::vector<uint8_t>{ 0xFF, 0x00, 0x55 }));
    }
}

TEST(bitstream, add_hdlc_flags)
{
    std::vector<uint8_t> buffer(20, 0);
    add_hdlc_flags(buffer.begin(), 2);
    EXPECT_TRUE(buffer == std::vector<uint8_t>({ 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 }));
}

TEST(bitstream, find_first_hdlc_flag)
{
    {
        std::vector<uint8_t> bits = { 0,0,0,1,1,1,1,1,1,0,0,0 };
        auto it = find_first_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.begin() + 2);
    }

    {
        std::vector<uint8_t> bits = { 0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0 };
        auto it = find_first_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.begin());
    }

    {
        std::vector<uint8_t> bits = { 0,0,0,0,0 };
        auto it = find_first_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.end());
    }

    {
        std::vector<uint8_t> bits = { 1,1,1,1,1,1,0 };
        auto it = find_first_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.end());
    }
}

TEST(bitstream, find_last_consecutive_hdlc_flag)
{
    {
        std::vector<uint8_t> bits = { 0,0,0,1,1,1,1,1,1,0,0,0 };
        auto it = find_last_consecutive_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.begin() + 2);
    }

    {
        std::vector<uint8_t> bits = { 0,1,1,1,1,1,1,0,0,1,1,1,1,1,1,0 };
        auto it = find_last_consecutive_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.begin() + 8);
    }

    {
        std::vector<uint8_t> bits = { 0,0,0,0,0 };
        auto it = find_last_consecutive_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.end());
    }

    {
        std::vector<uint8_t> bits = { 1,1,1,1,1,1,0 };
        auto it = find_last_consecutive_hdlc_flag(bits.begin(), bits.end());
        EXPECT_TRUE(it == bits.end());
    }
}

TEST(bitstream, bit_stuff)
{
    {
        std::vector<uint8_t> bits = { 1,1,1,1,1,1,0,0,0 }; // 5 consecutive 1s
        std::vector<uint8_t> stuffed;
        bit_stuff(bits.begin(), bits.end(), std::back_inserter(stuffed));
        EXPECT_TRUE(stuffed == (std::vector<uint8_t>{ 1, 1, 1, 1, 1, 0, 1, 0, 0, 0 })); // Stuff a 0 after 5 consecutive 1s
    }

    {
        std::vector<uint8_t> bits = { 1,0,1,1,1,1,1,1,0 }; // 5 consecutive 1s
        std::vector<uint8_t> stuffed;
        bit_stuff(bits.begin(), bits.end(), std::back_inserter(stuffed));
        EXPECT_TRUE(stuffed == (std::vector<uint8_t>{ 1, 0, 1, 1, 1, 1, 1, 0, 1, 0 })); // Stuff a 0 after 5 consecutive 1s
    }

    {
        std::vector<uint8_t> bits = { 0,0,0,0 }; // No 1s
        std::vector<uint8_t> stuffed;
        bit_stuff(bits.begin(), bits.end(), std::back_inserter(stuffed));
        EXPECT_TRUE(stuffed == (std::vector<uint8_t>{ 0, 0, 0, 0 })); // No stuffing needed
    }

    {
        std::vector<uint8_t> bits = { 1,1,1,1,1 }; // 5 consecutive 1s
        std::vector<uint8_t> stuffed;
        bit_stuff(bits.begin(), bits.end(), std::back_inserter(stuffed));
        EXPECT_TRUE(stuffed == (std::vector<uint8_t>{ 1, 1, 1, 1, 1, 0 }));
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}