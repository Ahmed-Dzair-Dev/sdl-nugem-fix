/*
 * Copyright (c) 2016 Victor Nivet
 *
 * This file is part of Nugem.
 *
 * Nugem is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 * Nugem is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with Nugem.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sffv2.hpp"

#include <ios>
#include <fstream>
#include <iostream>
#include <array>

namespace Nugem {
namespace Mugen {

bool Sffv2::SpriteInfo::usesTData() const
{
	return flags & 0x01;;
}

Sffv2::Sffv2(const char * filename): m_filename(filename)
{
    m_ldata = nullptr;
    m_tdata = nullptr;
    m_sffv2Container.clear();
    m_palettes.clear();
    m_groups.clear();
    loadSffFile();
}

Sffv2::~Sffv2()
{
    if (m_ldata != nullptr)
        delete [] m_ldata;
    if (m_tdata != nullptr)
        delete [] m_tdata;
}

void Sffv2::loadSffFile()
{
    uint32_t fileptr;
    uint8_t * readbuf[READBUF_SIZE];
    std::ifstream charfile(m_filename.c_str());
    // Reading the filename

    // First 512 bytes: header
    // Signature at the start of the file: 'ElecbyteSpr\0'
    charfile.read((char *) readbuf, 12);
    if (strcmp((char *) readbuf, "ElecbyteSpr")) {
        throw std::runtime_error(std::string("Invalid sprite file: ") + m_filename);
    }
    // Version bytes
    std::array<uint8_t, 4> version = extract_version(charfile);
    // Reserved bytes
    charfile.seekg(8, std::ios::cur);
    // Compatibility version bytes
    std::array<uint8_t, 4> compatver = extract_version(charfile);
    charfile.seekg(8, std::ios::cur);
    uint32_t first_sprite_offset = read_uint32(charfile);
    size_t nSprites = read_uint32(charfile);
    uint32_t first_palette_offset = read_uint32(charfile);
    size_t nPalettes = read_uint32(charfile);

    // ldata (literal data block) information
    uint32_t ldata_offset = read_uint32(charfile);
    m_ldataLength = read_uint32(charfile);
    if (m_ldataLength > 0)
        m_ldata = new uint8_t[m_ldataLength];
    // Copying ldata into memory
    fileptr = charfile.tellg();
    charfile.seekg(ldata_offset, std::ios::beg);
    charfile.read((char *) m_ldata, m_ldataLength);
    charfile.seekg(fileptr, std::ios::beg);

    // tdata (translated data block) information: it is supposed to be translated during load
    uint32_t tdata_offset = read_uint32(charfile);
    m_tdataLength = read_uint32(charfile);
    if (m_tdataLength > 0)
        m_tdata = new uint8_t[m_tdataLength];
    // Copying tdata into memory
    fileptr = charfile.tellg();
    charfile.seekg(tdata_offset, std::ios::beg);
    charfile.read((char *) m_tdata, m_tdataLength);
    charfile.seekg(fileptr, std::ios::beg);

    // Going to first sprite data
    charfile.seekg(first_sprite_offset, std::ios::beg);
    for (size_t i_sprite = 0; i_sprite < nSprites; i_sprite++)
        m_sffv2Container.push_back(readSprite(charfile));

    // Going to first palette data
    charfile.seekg(first_palette_offset, std::ios::beg);
    for (size_t i_palette = 0; i_palette < nPalettes; i_palette++)
        m_palettes.push_back(readPalette(charfile));

    charfile.close();
}

Sffv2::SpriteInfo Sffv2::readSprite(std::ifstream & fileobj)
{
    Sffv2::SpriteInfo sprite;
    sprite.groupno = read_uint16(fileobj);
    sprite.itemno = read_uint16(fileobj);
    sprite.width = read_uint16(fileobj);
    sprite.height = read_uint16(fileobj);
    sprite.axisx = read_uint16(fileobj);
    sprite.axisy = read_uint16(fileobj);
    sprite.linkedindex = read_uint16(fileobj);
    sprite.fmt = fileobj.get();
    sprite.coldepth = fileobj.get();
    sprite.dataOffset = read_uint32(fileobj);
    sprite.dataLength = read_uint32(fileobj);
    sprite.paletteIndex = read_uint16(fileobj);
    sprite.flags = read_uint16(fileobj);
    sprite.texture = nullptr;
    m_groups[sprite.groupno].i[sprite.itemno] = m_sffv2Container.size();
    return sprite;
}

Sffv2::PaletteInfo Sffv2::readPalette(std::ifstream & fileobj)
{
    Sffv2::PaletteInfo palette;
    palette.groupno = read_uint16(fileobj);
    palette.itemno = read_uint16(fileobj);
    palette.numcols = read_uint16(fileobj);
    palette.linkedindex = read_uint16(fileobj);
    palette.ldataOffset = read_uint32(fileobj);
    palette.dataLength = read_uint32(fileobj);
    return palette;
}

Sffv2::Drawer::Drawer(const SpriteInfo& sprite, const PaletteInfo& palette, uint8_t * ldata, uint8_t * tdata): Nugem::SurfaceDrawer(sprite.width, sprite.height), m_sprite(sprite), m_palette(palette), m_ldata(ldata), m_tdata(tdata)
{
}

void Sffv2::Drawer::draw(uint32_t * pixelData, size_t width, size_t height)
{
	const size_t surfaceSize = width * height;
	auto outputColoredPixel = [&](uint8_t color, const uint32_t indexPixel) {
		if (indexPixel < surfaceSize) {
			const uint8_t * colorarray = m_ldata + (m_palette.ldataOffset + color * 4);
			uint8_t red, green, blue, alpha;
			red = colorarray[0];
			green = colorarray[1];
			blue = colorarray[2];
			if (color)
				alpha = 0xFF;
			else
				alpha = 0x00;
			* (pixelData + indexPixel) = rgba(red, green, blue, alpha);
		}
	};
    // Initializing the variables we will need for the rest
    const uint8_t * sdata;
	if (m_sprite.usesTData())
		sdata = m_tdata + m_sprite.dataOffset;
	else
		sdata = m_ldata + m_sprite.dataOffset;
    uint64_t indexPixel = 0;
    uint32_t i_byte = 0;
    switch (m_sprite.fmt) {
    case 0: // raw
        for (i_byte = 4; i_byte < m_sprite.dataLength; i_byte++, indexPixel++) {
            outputColoredPixel(sdata[i_byte], indexPixel);
        }
        break;
    case 1: // invalid
        break;
    case 2: // RLE8 (Run-Length Encoding at 8 bits-per-pixel pixmap)
        for (i_byte = 4; i_byte < m_sprite.dataLength; i_byte++) {
            uint8_t first_byte = sdata[i_byte];
            if ((first_byte & 0xC0) == 0x40) {  // in the case of a RLE control packet
                i_byte++;
                uint8_t color = sdata[i_byte]; // <- the next byte
                uint8_t run_length = (first_byte & 0x3F);
                for (int run_count = 0; run_count < run_length; run_count++)
                    outputColoredPixel(color, indexPixel++);
            }
            else
                outputColoredPixel(first_byte, indexPixel++);
        }
        break;
    case 3: // RLE5 (5 bits-per-pixel pixmaps)
        for (i_byte = 4; i_byte < m_sprite.dataLength; i_byte++) {
            uint8_t run_length = sdata[i_byte];
            i_byte++;
            uint8_t data_length = sdata[i_byte];
            uint8_t color;
            if (data_length & 0x80) // testing the color bit
                color = sdata[++i_byte];
            else
                color = 0;
            for (int run_count = 0; run_count < run_length; run_count++)
                outputColoredPixel(color, indexPixel++);
            for (int bytes_processed = 0; bytes_processed < (data_length & 0x7F) - 1; bytes_processed++) {
                uint8_t byte = sdata[++i_byte];
                color = byte & 0x1F;
                run_length = byte >> 5;
                for (uint8_t run_count = 0; run_count < run_length; run_count++)
                    outputColoredPixel(color, indexPixel++);
            }
        }
        break;
    case 4: // LZ5
        // see the documentation on LZ5 compression: https://web.archive.org/web/20141230125932/http://elecbyte.com/wiki/index.php/LZ5
        uint32_t shortlzpackets = 1;
        uint8_t recycledbits = 0;
        // first 4 bytes are the size of uncompressed data
        // so we skip them
        for (i_byte = 4; i_byte < m_sprite.dataLength; i_byte++) {
            uint8_t control_packet = sdata[i_byte];
            uint8_t i_data_packet;
            for (i_data_packet = 0; i_data_packet < 8 && i_byte < m_sprite.dataLength; i_data_packet++) {
                uint8_t flag = control_packet & (1 << i_data_packet); // value 0-> it is a RLE packet, value 1-> LZ packet
                i_byte++;
                if (!flag) { // RLE packet (short or long)
                    uint8_t color = sdata[i_byte] & 0x1F;
                    uint32_t run_length = sdata[i_byte] & 0xE0;
                    size_t run_count;
                    if (run_length > 0) { // short RLE packet
                        run_length >>= 5; // the color runs in 1 to 7 pixels, so it's mapped on 3 bits all right
                    }
                    else {   // long RLE packet
                        i_byte++;
                        run_length = sdata[i_byte] + 8;
                        // the color runs in 8 to 263 times
                    }
                    for (run_count = 0; run_count < run_length; run_count++)
                        outputColoredPixel(color, indexPixel++);
                }
                else {   // LZ packet (short or long)
                    uint32_t copylength = sdata[i_byte] & 0x3F; // bits 0-5
                    uint16_t offset = 0;
                    if (copylength) { // short LZ packet, if initial copy length is not null
                        copylength += 1; // <- see the docs
                        // How recycled bits work:
// 						bits 6-7: recycled bits of short LZ packet 4k + 1
// 						bits 4-5: recycled bits of short LZ packet 4k + 2
// 						bits 2-3: recycled bits of short LZ packet 4k + 3
// 						bits 0-1: recycled bits of short LZ packet 4k + 4
                        if (shortlzpackets % 4 == 0) { // use the recycled bits
                            recycledbits |= (sdata[i_byte] & 0xC0) >> 6;
                            offset = recycledbits + 1;
                            // new recycled bits cycle
                            recycledbits = 0;
                        }
                        else { // read the extra byte
                            recycledbits |= (sdata[i_byte] & 0xC0) >> (2 * ((shortlzpackets - 1) % 4));
                            i_byte++;
                            offset = sdata[i_byte] + 1;
                        }
                        shortlzpackets++;
                    }
                    else {   // long LZ packet
                        offset = sdata[i_byte] << 2; // since the 0x3F-masked bits are null, there is only the 0xC0-masked bits left
                        // ^ these are the highest 2 bits out of the 10-bit offset
                        i_byte++;
                        offset |= sdata[i_byte]; // low 8 bits of the 10-bit offset
                        offset += 1;
                        i_byte++;
                        copylength = sdata[i_byte] + 3; // value range: 8 to 263
                    }
                    // memory copy
                    // stay safe...
                    // If the length is greater than the offset, then the copy pointer must go back to the beginning of the source when it reaches the full length
                    // According to the Nomen developper
                    // So that is why the offset has a factor here
                    for (uint32_t i_pixel = 0; i_pixel < copylength && indexPixel < surfaceSize; i_pixel++, indexPixel++) {
                        uint32_t offsetFromBeginning;
                        if (offset)
                            offsetFromBeginning = offset * (1 + i_pixel / offset);
                        else // the offset should never be null though...
                            offsetFromBeginning = i_pixel; // just copy the same pixel over and over again I guess
                        * (pixelData + indexPixel) = * (pixelData + indexPixel - offsetFromBeginning);
                    }
                }
            }
        }
        break;
    }
}

SDL_Surface * Sffv2::renderToSurface(size_t spriteNumber, size_t currentPaletteId)
{
    // Case of a linked sprite
    size_t displayedSprite = spriteNumber;
    if (m_sffv2Container[spriteNumber].linkedindex)
        displayedSprite = m_sffv2Container[spriteNumber].linkedindex;
    Sffv2::SpriteInfo & sprite = m_sffv2Container[displayedSprite];
    // Guess:
    // if the sprite indicates a palette number other than 0, it's forcing that one
    size_t paletteUsed = sprite.paletteIndex;
    if (!paletteUsed)
        paletteUsed = currentPaletteId;
    // case of a linked palette
    if (m_palettes[paletteUsed].linkedindex)
        paletteUsed = m_palettes[paletteUsed].linkedindex;
    return Drawer(sprite, m_palettes[paletteUsed], m_ldata, m_tdata)();
}

void Sffv2::load()
{
    m_sprites.clear();
    for (size_t currentPalette = 0; currentPalette < m_palettes.size(); currentPalette++) {
        std::unordered_map<Spriteref, Sprite> currentPaletteSprites;
        for (size_t currentSprite = 0; currentSprite < m_sffv2Container.size(); currentSprite++) {
            SpriteInfo & sprite = m_sffv2Container[currentSprite];
            Spriteref ref(sprite.groupno, sprite.itemno);
            currentPaletteSprites.insert(std::pair<Spriteref, Sprite>(ref, Sprite(ref, renderToSurface(currentSprite, currentPalette), currentPalette)));
        }
        m_sprites.push_back(currentPaletteSprites);
    }
}

void Sffv2::load(std::vector<Spriteref>::iterator first, std::vector<Spriteref>::iterator last)
{
    m_sprites.clear();
    for (size_t currentPalette = 0; currentPalette < m_palettes.size(); currentPalette++)
        m_sprites.push_back(std::unordered_map<Spriteref, Sprite>());

    for (; first != last; first++) {
        for (size_t currentPalette = 0; currentPalette < m_palettes.size(); currentPalette++) {
            Spriteref & ref = *first;
            size_t currentSprite = m_groups[ref.group].i[ref.image];
            m_sprites[currentPalette].insert(std::pair<Spriteref, Sprite>(ref, Sprite(ref, renderToSurface(currentSprite, currentPalette), currentPalette)));
        }
    }
}

}
}

