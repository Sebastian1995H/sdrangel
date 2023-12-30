///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "dvb-s.h"
#include "util/popcount.h"

#include <string.h>

DVBS::DVBS() :
    m_prbsPacketCount(0),
    m_delayLine(0),
    m_punctureState(0),
    m_prevIQValid(false)
{
    // Allocate memory for packet buffer
    m_packet = new uint8_t[rsPacketLen];

    // Allocate memory for interleaver FIFOs
    int unitSize = rsPacketLen/interleaveDepth;

    m_interleaveFIFO = new uint8_t *[interleaveDepth];
    m_interleaveLen = new int[interleaveDepth];
    m_interleaveIdx = new int[interleaveDepth];
    for (int i = 1; i < interleaveDepth; i++)
    {
         m_interleaveFIFO[i] = new uint8_t[unitSize*i]();
         m_interleaveLen[i] = unitSize*i;
         m_interleaveIdx[i] = 0;
    }
}

DVBS::~DVBS()
{
    // Free interleaver FIFO memory
    for (int i = 1; i < interleaveDepth; i++)
        delete[] m_interleaveFIFO[i];
    delete[] m_interleaveIdx;
    delete[] m_interleaveLen;
    delete[] m_interleaveFIFO;

    // Free packet buffer
    delete[] m_packet;
}

// Scramble input packet (except for sync bytes) with psuedo random binary sequence
// Initiliase PRBS sequence every 8 packets and invert sync byte
void DVBS::scramble(const uint8_t *packetIn, uint8_t *packetOut)
{
    if (m_prbsPacketCount == 0)
    {
        // Init
        m_prbsIdx = 0;
        // Invert sync byte to reset receiver
        packetOut[0] = ~tsSync;
    }
    else
    {
        // PRBS continues, but isn't applied to sync bytes 1-7
        m_prbsIdx++;
        packetOut[0] = tsSync;
    }
    m_prbsPacketCount++;
    if (m_prbsPacketCount == 8)
        m_prbsPacketCount = 0;

    // Apply PRBS to data
    for (int i = 1; i < tsPacketLen; i++)
        packetOut[i] = packetIn[i] ^ m_prbsLUT[m_prbsIdx++];
}

// RS(204,188,t=8) shortened code from RS(255,239,t=8) code
// Using GF(2^8=256) so 1 symbol is 1 byte
// Code generator polynomial: (x+l^0)(x+l^1)..(x+l^15) l=0x02
// Field generator polynomial: x^8+x^4+x^3+x^2+1 (a primitive polynomial)
// Add 51 zero bytes before 188-byte packet to pad to 239 bytes, which are
// discarded after RS encoding. They don't change the result so can just be ignored
// n=255 k=239
// RS adds 2t = 16 bytes (n-k) after original data (so is systematic)
// t=8 means we can correct 8 bytes if we don't know where the errors are
// or 2t=16 errors if we do no where the errors are
// https://en.wikiversity.org/wiki/Reed%E2%80%93Solomon_codes_for_coders

// Galois field multiply
uint8_t DVBS::gfMul(uint8_t a, uint8_t b)
{
    if ((a == 0) || (b == 0))
        return 0;
    return m_gfExp[m_gfLog[a] + m_gfLog[b]];
}

// Reed Solomon encoder
void DVBS::reedSolomon(uint8_t *packet)
{
    uint8_t tmp[rsK];
    const int partityBytesIdx = tsPacketLen;

    // Copy in packet
    memcpy(&tmp[0], packet, tsPacketLen);
    // Zero parity bytes
    memset(&tmp[partityBytesIdx], 0, rs2T);

    // Divide input by generator polynomial to compute parity bytes
    for (int i = 0; i < tsPacketLen; i++)
    {
        uint8_t coef = tmp[i];

        if (coef != 0)
        {
            for (int j = 0; j < rs2T; j++)
                tmp[i+j+1] ^= gfMul(rsPoly[j], coef);
        }
    }

    // Append parity bytes to message
    memcpy(&packet[tsPacketLen], &tmp[partityBytesIdx], rs2T);
}

// Forney interleaver
// We have interleaveDepth FIFOs, each of a different length, that the data is split between
// FIFO n's length is n*rsPacketLen/interleaveDepth elements
void DVBS::interleave(uint8_t *packet)
{
    uint8_t *p = packet;
    for (int j = 0; j < rsPacketLen; j += interleaveDepth)
    {
        p++;
        for (int i = 1; i < interleaveDepth; i++)
        {
            uint8_t tmp = m_interleaveFIFO[i][m_interleaveIdx[i]];
            m_interleaveFIFO[i][m_interleaveIdx[i]] = *p;
            *p = tmp;
            m_interleaveIdx[i] = (m_interleaveIdx[i] + 1) % m_interleaveLen[i];
            p++;
        }
    }
}

// Convolution encoder
// Rate 1/2 with puncturing
// Constraint length K=7 G1=171 G2=133
// Similar to CCSDS (without inversion of C2)
// Using popcount is faster than state/output LUTs
int DVBS::convolution(const uint8_t *packet, uint8_t *iq)
{
    const int g1 = 0x79;
    const int g2 = 0x5b;
    const int k = 7;
    uint8_t *iqStart = iq;
    int bit;

    // Output byte left over from last frame
    if (m_prevIQValid)
        *iq++ = m_prevIQ;

    switch (m_codeRate)
    {
    case RATE_1_2:
        for (int i = 0; i < rsPacketLen; i++)
        {
            for (int j = 7; j >= 0; j--)
            {
                bit = (packet[i] >> j) & 1;
                m_delayLine = m_delayLine | (bit << (k-1));
                *iq++ = popcount(m_delayLine & g1) & 1;
                *iq++ = popcount(m_delayLine & g2) & 1;
                m_delayLine >>= 1;
            }
        }
        break;

    case RATE_2_3:
        for (int i = 0; i < rsPacketLen; i++)
        {
            for (int j = 7; j >= 0; j--)
            {
                bit = (packet[i] >> j) & 1;
                m_delayLine = m_delayLine | (bit << (k-1));
                switch (m_punctureState)
                {
                case 0:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 1:
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState = 0;
                    break;
                }
                m_delayLine >>= 1;
            }
        }
        break;

    case RATE_3_4:
        for (int i = 0; i < rsPacketLen; i++)
        {
            for (int j = 7; j >= 0; j--)
            {
                bit = (packet[i] >> j) & 1;
                m_delayLine = m_delayLine | (bit << (k-1));
                switch (m_punctureState)
                {
                case 0:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 1:
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 2:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    m_punctureState = 0;
                    break;
                }
                m_delayLine >>= 1;
            }
        }
        break;

    case RATE_5_6:
        for (int i = 0; i < rsPacketLen; i++)
        {
            for (int j = 7; j >= 0; j--)
            {
                bit = (packet[i] >> j) & 1;
                m_delayLine = m_delayLine | (bit << (k-1));
                switch (m_punctureState)
                {
                case 0:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 1:
                case 3:
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 2:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    m_punctureState++;
                    break;
                case 4:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    m_punctureState = 0;
                    break;
                }
                m_delayLine >>= 1;
            }
        }
        break;

    case RATE_7_8:
        for (int i = 0; i < rsPacketLen; i++)
        {
            for (int j = 7; j >= 0; j--)
            {
                bit = (packet[i] >> j) & 1;
                m_delayLine = m_delayLine | (bit << (k-1));
                switch (m_punctureState)
                {
                case 0:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 1:
                case 2:
                case 3:
                case 5:
                    *iq++ = popcount(m_delayLine & g2) & 1;
                    m_punctureState++;
                    break;
                case 4:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    m_punctureState++;
                    break;
                case 6:
                    *iq++ = popcount(m_delayLine & g1) & 1;
                    m_punctureState = 0;
                    break;
                }
                m_delayLine >>= 1;
            }
        }
        break;
    }

    int iqBytes = iq - iqStart;

    // If puncturing has left half a symbol, save it for next packet
    if (iqBytes & 1)
    {
        m_prevIQ = iq[-1];
        m_prevIQValid = true;
    }
    else
        m_prevIQValid = false;

    return iqBytes >> 1;
}

// Set convolution encoder forward error correction (FEC) code rate
// Rate 1/2 = 1 input bit produces 2 output bits
void DVBS::setCodeRate(CodeRate codeRate)
{
    m_codeRate = codeRate;
    m_delayLine = 0;
    m_punctureState = 0;
    m_prevIQValid = false;
}

// DVB-S packet encoder.
// Encode 188 byte MPEG transport stream to IQ symbols
// IQ symbols are 0 or 1 and interleaved (I0, Q0, I1, Q1...)
// Number of IQ symbols is returned from function and depends on FEC rate
// Maximum number is (188+16)*8=1632 when FEC rate is 1/2
// (This is IQ symbols, so 3264 elements needed in iq array)
int DVBS::encode(const uint8_t *tsStreamPacket, uint8_t *iq)
{
    scramble(tsStreamPacket, m_packet);
    reedSolomon(m_packet);
    interleave(m_packet);
    return convolution(m_packet, iq);
}

// Pseudo random sequence look-up table
// Generated with RandomizeDVBS in util/lfsr.h
const uint8_t DVBS::m_prbsLUT[tsPacketLen*8-1] =
{
    0x03, 0xf6, 0x08, 0x34, 0x30, 0xb8, 0xa3, 0x93,
    0xc9, 0x68, 0xb7, 0x73, 0xb3, 0x29, 0xaa, 0xf5,
    0xfe, 0x3c, 0x04, 0x88, 0x1b, 0x30, 0x5a, 0xa1,
    0xdf, 0xc4, 0xc0, 0x9a, 0x83, 0x5f, 0x0b, 0xc2,
    0x38, 0x8c, 0x93, 0x2b, 0x6a, 0xfb, 0x7e, 0x1b,
    0x04, 0x5a, 0x19, 0xdc, 0x54, 0xc9, 0xfa, 0xb4,
    0x1f, 0xb8, 0x41, 0x91, 0x85, 0x65, 0x1f, 0x5e,
    0x43, 0xc5, 0x88, 0x9d, 0x33, 0x4e, 0xab, 0xa7,
    0xf9, 0xd0, 0x14, 0xe0, 0x7a, 0x41, 0x1d, 0x86,
    0x4d, 0x15, 0xae, 0x7d, 0xe5, 0x0c, 0x5e, 0x29,
    0xc4, 0xf4, 0x9a, 0x3b, 0x5c, 0x9b, 0xcb, 0x58,
    0xbb, 0xd3, 0x98, 0xe9, 0x52, 0x77, 0xed, 0x30,
    0x6e, 0xa1, 0x67, 0xc7, 0x50, 0x93, 0xe3, 0x68,
    0x4b, 0x71, 0xbb, 0x25, 0x9a, 0xdd, 0x5e, 0xcf,
    0xc6, 0xa0, 0x97, 0xc3, 0x70, 0x8b, 0x23, 0x3a,
    0xca, 0x9e, 0xbf, 0x47, 0x83, 0x91, 0x09, 0x66,
    0x37, 0x54, 0xb3, 0xfb, 0xa8, 0x19, 0xf0, 0x54,
    0x21, 0xf8, 0xc4, 0x12, 0x98, 0x6f, 0x51, 0x63,
    0xe7, 0x48, 0x53, 0xb1, 0xe9, 0xa4, 0x75, 0xd9,
    0x3c, 0xd6, 0x8a, 0xf7, 0x3e, 0x32, 0x84, 0xaf,
    0x1b, 0xe2, 0x58, 0x4d, 0xd1, 0xac, 0xe5, 0xea,
    0x5c, 0x7d, 0xc9, 0x0c, 0xb6, 0x2b, 0xb4, 0xf9,
    0xba, 0x15, 0x9c, 0x7d, 0x49, 0x0f, 0xb6, 0x21,
    0xb4, 0xc5, 0xba, 0x9d, 0x9f, 0x4d, 0x43, 0xaf,
    0x89, 0xe1, 0x34, 0x46, 0xb9, 0x97, 0x95, 0x71,
    0x7f, 0x27, 0x02, 0xd2, 0x0e, 0xec, 0x26, 0x68,
    0xd5, 0x72, 0xff, 0x2e, 0x02, 0xe4, 0x0e, 0x58,
    0x25, 0xd0, 0xdc, 0xe2, 0xca, 0x4e, 0xbd, 0xa7,
    0x8d, 0xd1, 0x2c, 0xe6, 0xea, 0x56, 0x7d, 0xf5,
    0x0c, 0x3e, 0x28, 0x84, 0xf3, 0x1a, 0x2a, 0x5c,
    0xfd, 0xca, 0x0c, 0xbc, 0x2b, 0x88, 0xf9, 0x32,
    0x16, 0xac, 0x77, 0xe9, 0x30, 0x76, 0xa1, 0x37,
    0xc6, 0xb0, 0x97, 0xa3, 0x71, 0xcb, 0x24, 0xba,
    0xdb, 0x9e, 0xd9, 0x46, 0xd7, 0x96, 0xf1, 0x76,
    0x27, 0x34, 0xd2, 0xba, 0xef, 0x9e, 0x61, 0x45,
    0x47, 0x9f, 0x91, 0x41, 0x67, 0x87, 0x51, 0x13,
    0xe6, 0x68, 0x55, 0x71, 0xff, 0x24, 0x02, 0xd8,
    0x0e, 0xd0, 0x26, 0xe0, 0xd6, 0x42, 0xf5, 0x8e,
    0x3d, 0x24, 0x8e, 0xdb, 0x26, 0xda, 0xd6, 0xde,
    0xf6, 0xc6, 0x36, 0x94, 0xb7, 0x7b, 0xb3, 0x19,
    0xaa, 0x55, 0xfd, 0xfc, 0x0c, 0x08, 0x28, 0x30,
    0xf0, 0xa2, 0x23, 0xcc, 0xc8, 0xaa, 0xb3, 0xff,
    0xa8, 0x01, 0xf0, 0x04, 0x20, 0x18, 0xc0, 0x52,
    0x81, 0xef, 0x04, 0x62, 0x19, 0x4c, 0x57, 0xa9,
    0xf1, 0xf4, 0x24, 0x38, 0xd8, 0x92, 0xd3, 0x6e,
    0xeb, 0x66, 0x7b, 0x55, 0x1b, 0xfe, 0x58, 0x05,
    0xd0, 0x1c, 0xe0, 0x4a, 0x41, 0xbd, 0x85, 0x8d,
    0x1d, 0x2e, 0x4e, 0xe5, 0xa6, 0x5d, 0xd5, 0xcc,
    0xfc, 0xaa, 0x0b, 0xfc, 0x38, 0x08, 0x90, 0x33,
    0x60, 0xab, 0x43, 0xfb, 0x88, 0x19, 0x30, 0x56,
    0xa1, 0xf7, 0xc4, 0x30, 0x98, 0xa3, 0x53, 0xcb,
    0xe8, 0xb8, 0x73, 0x91, 0x29, 0x66, 0xf7, 0x56,
    0x33, 0xf4, 0xa8, 0x3b, 0xf0, 0x98, 0x23, 0x50,
    0xcb, 0xe2, 0xb8, 0x4f, 0x91, 0xa1, 0x65, 0xc7,
    0x5c, 0x93, 0xcb, 0x68, 0xbb, 0x73, 0x9b, 0x29,
    0x5a, 0xf7, 0xde, 0x30, 0xc4, 0xa2, 0x9b, 0xcf,
    0x58, 0xa3, 0xd3, 0xc8, 0xe8, 0xb2, 0x73, 0xad,
    0x29, 0xee, 0xf4, 0x66, 0x39, 0x54, 0x97, 0xfb,
    0x70, 0x1b, 0x20, 0x5a, 0xc1, 0xde, 0x84, 0xc7,
    0x1a, 0x92, 0x5f, 0x6d, 0xc3, 0x6c, 0x8b, 0x6b,
    0x3b, 0x7a, 0x9b, 0x1f, 0x5a, 0x43, 0xdd, 0x88,
    0xcd, 0x32, 0xae, 0xaf, 0xe7, 0xe0, 0x50, 0x41,
    0xe1, 0x84, 0x45, 0x19, 0x9e, 0x55, 0x45, 0xff,
    0x9c, 0x01, 0x48, 0x07, 0xb0, 0x11, 0xa0, 0x65,
    0xc1, 0x5c, 0x87, 0xcb, 0x10, 0xba, 0x63, 0x9d,
    0x49, 0x4f, 0xb7, 0xa1, 0xb1, 0xc5, 0xa4, 0x9d,
    0xdb, 0x4c, 0xdb, 0xaa, 0xd9, 0xfe, 0xd4, 0x06,
    0xf8, 0x16, 0x10, 0x74, 0x61, 0x39, 0x46, 0x97,
    0x97, 0x71, 0x73, 0x27, 0x2a, 0xd2, 0xfe, 0xee,
    0x06, 0x64, 0x15, 0x58, 0x7f, 0xd1, 0x00, 0xe6,
    0x02, 0x54, 0x0d, 0xf8, 0x2c, 0x10, 0xe8, 0x62,
    0x71, 0x4d, 0x27, 0xae, 0xd1, 0xe6, 0xe4, 0x56,
    0x59, 0xf5, 0xd4, 0x3c, 0xf8, 0x8a, 0x13, 0x3c,
    0x6a, 0x89, 0x7f, 0x37, 0x02, 0xb2, 0x0f, 0xac,
    0x21, 0xe8, 0xc4, 0x72, 0x99, 0x2f, 0x56, 0xe3,
    0xf6, 0x48, 0x35, 0xb0, 0xbd, 0xa3, 0x8d, 0xc9,
    0x2c, 0xb6, 0xeb, 0xb6, 0x79, 0xb5, 0x15, 0xbe,
    0x7d, 0x85, 0x0d, 0x1e, 0x2e, 0x44, 0xe5, 0x9a,
    0x5d, 0x5d, 0xcf, 0xcc, 0xa0, 0xab, 0xc3, 0xf8,
    0x88, 0x13, 0x30, 0x6a, 0xa1, 0x7f, 0xc7, 0x00,
    0x92, 0x03, 0x6c, 0x0b, 0x68, 0x3b, 0x70, 0x9b,
    0x23, 0x5a, 0xcb, 0xde, 0xb8, 0xc7, 0x92, 0x91,
    0x6f, 0x67, 0x63, 0x53, 0x4b, 0xeb, 0xb8, 0x79,
    0x91, 0x15, 0x66, 0x7f, 0x55, 0x03, 0xfe, 0x08,
    0x04, 0x30, 0x18, 0xa0, 0x53, 0xc1, 0xe8, 0x84,
    0x73, 0x19, 0x2a, 0x56, 0xfd, 0xf6, 0x0c, 0x34,
    0x28, 0xb8, 0xf3, 0x92, 0x29, 0x6c, 0xf7, 0x6a,
    0x33, 0x7c, 0xab, 0x0b, 0xfa, 0x38, 0x1c, 0x90,
    0x4b, 0x61, 0xbb, 0x45, 0x9b, 0x9d, 0x59, 0x4f,
    0xd7, 0xa0, 0xf1, 0xc2, 0x24, 0x8c, 0xdb, 0x2a,
    0xda, 0xfe, 0xde, 0x06, 0xc4, 0x16, 0x98, 0x77,
    0x51, 0x33, 0xe6, 0xa8, 0x57, 0xf1, 0xf0, 0x24,
    0x20, 0xd8, 0xc2, 0xd2, 0x8e, 0xef, 0x26, 0x62,
    0xd5, 0x4e, 0xff, 0xa6, 0x01, 0xd4, 0x04, 0xf8,
    0x1a, 0x10, 0x5c, 0x61, 0xc9, 0x44, 0xb7, 0x9b,
    0xb1, 0x59, 0xa7, 0xd5, 0xd0, 0xfc, 0xe2, 0x0a,
    0x4c, 0x3d, 0xa8, 0x8d, 0xf3, 0x2c, 0x2a, 0xe8,
    0xfe, 0x72, 0x05, 0x2c, 0x1e, 0xe8, 0x46, 0x71,
    0x95, 0x25, 0x7e, 0xdf, 0x06, 0xc2, 0x16, 0x8c,
    0x77, 0x29, 0x32, 0xf6, 0xae, 0x37, 0xe4, 0xb0,
    0x5b, 0xa1, 0xd9, 0xc4, 0xd4, 0x9a, 0xfb, 0x5e,
    0x1b, 0xc4, 0x58, 0x99, 0xd3, 0x54, 0xeb, 0xfa,
    0x78, 0x1d, 0x10, 0x4e, 0x61, 0xa5, 0x45, 0xdf,
    0x9c, 0xc1, 0x4a, 0x87, 0xbf, 0x11, 0x82, 0x65,
    0x0d, 0x5e, 0x2f, 0xc4, 0xe0, 0x9a, 0x43, 0x5d,
    0x8b, 0xcd, 0x38, 0xae, 0x93, 0xe7, 0x68, 0x53,
    0x71, 0xeb, 0x24, 0x7a, 0xd9, 0x1e, 0xd6, 0x46,
    0xf5, 0x96, 0x3d, 0x74, 0x8f, 0x3b, 0x22, 0x9a,
    0xcf, 0x5e, 0xa3, 0xc7, 0xc8, 0x90, 0xb3, 0x63,
    0xab, 0x49, 0xfb, 0xb4, 0x19, 0xb8, 0x55, 0x91,
    0xfd, 0x64, 0x0f, 0x58, 0x23, 0xd0, 0xc8, 0xe2,
    0xb2, 0x4f, 0xad, 0xa1, 0xed, 0xc4, 0x6c, 0x99,
    0x6b, 0x57, 0x7b, 0xf3, 0x18, 0x2a, 0x50, 0xfd,
    0xe2, 0x0c, 0x4c, 0x29, 0xa8, 0xf5, 0xf2, 0x3c,
    0x2c, 0x88, 0xeb, 0x32, 0x7a, 0xad, 0x1f, 0xee,
    0x40, 0x65, 0x81, 0x5d, 0x07, 0xce, 0x10, 0xa4,
    0x63, 0xd9, 0x48, 0xd7, 0xb2, 0xf1, 0xae, 0x25,
    0xe4, 0xdc, 0x5a, 0xc9, 0xde, 0xb4, 0xc7, 0xba,
    0x91, 0x9f, 0x65, 0x43, 0x5f, 0x8b, 0xc1, 0x38,
    0x86, 0x93, 0x17, 0x6a, 0x73, 0x7d, 0x2b, 0x0e,
    0xfa, 0x26, 0x1c, 0xd4, 0x4a, 0xf9, 0xbe, 0x15,
    0x84, 0x7d, 0x19, 0x0e, 0x56, 0x25, 0xf4, 0xdc,
    0x3a, 0xc8, 0x9e, 0xb3, 0x47, 0xab, 0x91, 0xf9,
    0x64, 0x17, 0x58, 0x73, 0xd1, 0x28, 0xe6, 0xf2,
    0x56, 0x2d, 0xf4, 0xec, 0x3a, 0x68, 0x9d, 0x73,
    0x4f, 0x2b, 0xa2, 0xf9, 0xce, 0x14, 0xa4, 0x7b,
    0xd9, 0x18, 0xd6, 0x52, 0xf5, 0xee, 0x3c, 0x64,
    0x89, 0x5b, 0x37, 0xda, 0xb0, 0xdf, 0xa2, 0xc1,
    0xce, 0x84, 0xa7, 0x1b, 0xd2, 0x58, 0xed, 0xd2,
    0x6c, 0xed, 0x6a, 0x6f, 0x7d, 0x63, 0x0f, 0x4a,
    0x23, 0xbc, 0xc9, 0x8a, 0xb5, 0x3f, 0xbe, 0x81,
    0x87, 0x05, 0x12, 0x1e, 0x6c, 0x45, 0x69, 0x9f,
    0x75, 0x43, 0x3f, 0x8a, 0x81, 0x3f, 0x06, 0x82,
    0x17, 0x0c, 0x72, 0x29, 0x2c, 0xf6, 0xea, 0x36,
    0x7c, 0xb5, 0x0b, 0xbe, 0x39, 0x84, 0x95, 0x1b,
    0x7e, 0x5b, 0x05, 0xda, 0x1c, 0xdc, 0x4a, 0xc9,
    0xbe, 0xb5, 0x87, 0xbd, 0x11, 0x8e, 0x65, 0x25,
    0x5e, 0xdf, 0xc6, 0xc0, 0x96, 0x83, 0x77, 0x0b,
    0x32, 0x3a, 0xac, 0x9f, 0xeb, 0x40, 0x7b, 0x81,
    0x19, 0x06, 0x56, 0x15, 0xf4, 0x7c, 0x39, 0x08,
    0x96, 0x33, 0x74, 0xab, 0x3b, 0xfa, 0x98, 0x1f,
    0x50, 0x43, 0xe1, 0x88, 0x45, 0x31, 0x9e, 0xa5,
    0x47, 0xdf, 0x90, 0xc1, 0x62, 0x87, 0x4f, 0x13,
    0xa2, 0x69, 0xcd, 0x74, 0xaf, 0x3b, 0xe2, 0x98,
    0x4f, 0x51, 0xa3, 0xe5, 0xc8, 0x5c, 0xb1, 0xcb,
    0xa4, 0xb9, 0xdb, 0x94, 0xd9, 0x7a, 0xd7, 0x1e,
    0xf2, 0x46, 0x2d, 0x94, 0xed, 0x7a, 0x6f, 0x1d,
    0x62, 0x4f, 0x4d, 0xa3, 0xad, 0xc9, 0xec, 0xb4,
    0x6b, 0xb9, 0x79, 0x97, 0x15, 0x72, 0x7f, 0x2d,
    0x02, 0xee, 0x0e, 0x64, 0x25, 0x58, 0xdf, 0xd2,
    0xc0, 0xee, 0x82, 0x67, 0x0d, 0x52, 0x2f, 0xec,
    0xe0, 0x6a, 0x41, 0x7d, 0x87, 0x0d, 0x12, 0x2e,
    0x6c, 0xe5, 0x6a, 0x5f, 0x7d, 0xc3, 0x0c, 0x8a,
    0x2b, 0x3c, 0xfa, 0x8a, 0x1f, 0x3c, 0x42, 0x89,
    0x8f, 0x35, 0x22, 0xbe, 0xcf, 0x86, 0xa1, 0x17,
    0xc6, 0x70, 0x95, 0x23, 0x7e, 0xcb, 0x06, 0xba,
    0x17, 0x9c, 0x71, 0x49, 0x27, 0xb6, 0xd1, 0xb6,
    0xe5, 0xb6, 0x5d, 0xb5, 0xcd, 0xbc, 0xad, 0x8b,
    0xed, 0x38, 0x6e, 0x91, 0x67, 0x67, 0x53, 0x53,
    0xeb, 0xe8, 0x78, 0x71, 0x11, 0x26, 0x66, 0xd5,
    0x56, 0xff, 0xf6, 0x00, 0x34, 0x00, 0xb8, 0x03,
    0x90, 0x09, 0x60, 0x37, 0x40, 0xb3, 0x83, 0xa9,
    0x09, 0xf6, 0x34, 0x34, 0xb8, 0xbb, 0x93, 0x99,
    0x69, 0x57, 0x77, 0xf3, 0x30, 0x2a, 0xa0, 0xff,
    0xc2, 0x00, 0x8c, 0x03, 0x28, 0x0a, 0xf0, 0x3e,
    0x20, 0x84, 0xc3, 0x1a, 0x8a, 0x5f, 0x3d, 0xc2,
    0x8c, 0x8f, 0x2b, 0x22, 0xfa, 0xce, 0x1e, 0xa4,
    0x47, 0xd9, 0x90, 0xd5, 0x62, 0xff, 0x4e, 0x03,
    0xa4, 0x09, 0xd8, 0x34, 0xd0, 0xba, 0xe3, 0x9e,
    0x49, 0x45, 0xb7, 0x9d, 0xb1, 0x4d, 0xa7, 0xad,
    0xd1, 0xec, 0xe4, 0x6a, 0x59, 0x7d, 0xd7, 0x0c,
    0xf2, 0x2a, 0x2c, 0xfc, 0xea, 0x0a, 0x7c, 0x3d,
    0x08, 0x8e, 0x33, 0x24, 0xaa, 0xdb, 0xfe, 0xd8,
    0x06, 0xd0, 0x16, 0xe0, 0x76, 0x41, 0x35, 0x86,
    0xbd, 0x17, 0x8e, 0x71, 0x25, 0x26, 0xde, 0xd6,
    0xc6, 0xf6, 0x96, 0x37, 0x74, 0xb3, 0x3b, 0xaa,
    0x99, 0xff, 0x54, 0x03, 0xf8, 0x08, 0x10, 0x30,
    0x60, 0xa1, 0x43, 0xc7, 0x88, 0x91, 0x33, 0x66,
    0xab, 0x57, 0xfb, 0xf0, 0x18, 0x20, 0x50, 0xc1,
    0xe2, 0x84, 0x4f, 0x19, 0xa2, 0x55, 0xcd, 0xfc,
    0xac, 0x0b, 0xe8, 0x38, 0x70, 0x91, 0x23, 0x66,
    0xcb, 0x56, 0xbb, 0xf7, 0x98, 0x31, 0x50, 0xa7,
    0xe3, 0xd0, 0x48, 0xe1, 0xb2, 0x45, 0xad, 0x9d,
    0xed, 0x4c, 0x6f, 0xa9, 0x61, 0xf7, 0x44, 0x33,
    0x98, 0xa9, 0x53, 0xf7, 0xe8, 0x30, 0x70, 0xa1,
    0x23, 0xc6, 0xc8, 0x96, 0xb3, 0x77, 0xab, 0x31,
    0xfa, 0xa4, 0x1f, 0xd8, 0x40, 0xd1, 0x82, 0xe5,
    0x0e, 0x5e, 0x25, 0xc4, 0xdc, 0x9a, 0xcb,
};

// Generator polynmoial
const uint8_t DVBS::rsPoly[DVBS::rs2T] =
{
    59, 13, 104, 189, 68, 209, 30, 8, 163, 65, 41, 229, 98, 50, 36, 59
};

// Galios Field logarithm
const uint8_t DVBS::m_gfLog[256] =
{
    0x00, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1a, 0xc6,
    0x03, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b,
    0x04, 0x64, 0xe0, 0x0e, 0x34, 0x8d, 0xef, 0x81,
    0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x08, 0x4c, 0x71,
    0x05, 0x8a, 0x65, 0x2f, 0xe1, 0x24, 0x0f, 0x21,
    0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45,
    0x1d, 0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9,
    0xc9, 0x9a, 0x09, 0x78, 0x4d, 0xe4, 0x72, 0xa6,
    0x06, 0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd,
    0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88,
    0x36, 0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd,
    0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40,
    0x1e, 0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e,
    0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d,
    0xca, 0x5e, 0x9b, 0x9f, 0x0a, 0x15, 0x79, 0x2b,
    0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57,
    0x07, 0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0x0d,
    0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18,
    0xe3, 0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c,
    0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e,
    0x37, 0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd,
    0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61,
    0xf2, 0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e,
    0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2,
    0x1f, 0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76,
    0xc4, 0x17, 0x49, 0xec, 0x7f, 0x0c, 0x6f, 0xf6,
    0x6c, 0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa,
    0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a,
    0xcb, 0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51,
    0x0b, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7,
    0x4f, 0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8,
    0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf
};

// Galios Field exponent
const uint8_t DVBS::m_gfExp[512] =
{
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26,
    0x4c, 0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9,
    0x8f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x9d, 0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35,
    0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23,
    0x46, 0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0,
    0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1,
    0x5f, 0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc,
    0x65, 0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0,
    0xfd, 0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f,
    0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2,
    0xd9, 0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
    0x0d, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce,
    0x81, 0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93,
    0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc,
    0x85, 0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9,
    0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54,
    0xa8, 0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa,
    0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73,
    0xe6, 0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e,
    0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff,
    0xe3, 0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4,
    0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41,
    0x82, 0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e,
    0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6,
    0x51, 0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef,
    0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09,
    0x12, 0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5,
    0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16,
    0x2c, 0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83,
    0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1d,
    0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26, 0x4c,
    0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9, 0x8f,
    0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x9d,
    0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35, 0x6a,
    0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23, 0x46,
    0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0, 0x5d,
    0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1, 0x5f,
    0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc, 0x65,
    0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0, 0xfd,
    0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f, 0xfe,
    0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2, 0xd9,
    0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88, 0x0d,
    0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce, 0x81,
    0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93, 0x3b,
    0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc, 0x85,
    0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9, 0x4f,
    0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54, 0xa8,
    0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa, 0x49,
    0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73, 0xe6,
    0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e, 0xfc,
    0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff, 0xe3,
    0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4, 0x95,
    0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41, 0x82,
    0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e, 0x1c,
    0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6, 0x51,
    0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef, 0xc3,
    0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09, 0x12,
    0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5, 0xf7,
    0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16, 0x2c,
    0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83, 0x1b,
    0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01, 0x02
};
