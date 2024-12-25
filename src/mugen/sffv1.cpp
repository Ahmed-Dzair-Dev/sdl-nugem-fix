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

#include "sffv1.hpp"

#include "../character.hpp"

#include <ios>
#include <fstream>
#include <iostream>
#include <array>

namespace Nugem {
namespace Mugen {
	

uint16_t Sffv1::SpriteInfo::xmin() const
{
	return *( ((uint16_t * ) data) + 2);
}

uint16_t Sffv1::SpriteInfo::xmax() const
{
	return *( ((uint16_t * ) data) + 4);;
}

uint16_t Sffv1::SpriteInfo::ymin() const
{
	return *( ((uint16_t * ) data) + 3);;
}

uint16_t Sffv1::SpriteInfo::ymax() const
{
	return *( ((uint16_t * ) data) + 5);;
}

uint8_t Sffv1::SpriteInfo::nplanes() const
{
	return *( (uint8_t * ) data + 65);
}

uint16_t Sffv1::SpriteInfo::bytesPerLine() const
{
	return *( (uint16_t * ) data + 33);
}

uint16_t Sffv1::SpriteInfo::width() const
{
	return xmax() - xmin() + 1;
}

uint16_t Sffv1::SpriteInfo::height() const
{
	return ymax() - ymin() + 1;
}

uint32_t Sffv1::SpriteInfo::totalBytesPerLine() const
{
	return nplanes() * bytesPerLine();
}

Sffv1::Sffv1(const char * filename, const char * paletteFile): m_filename(filename), m_paletteFile(paletteFile)
{
    m_sffv1Container.clear();
    m_palettes.clear();
    loadSffFile();
    loadSharedPalettes();
}

Sffv1::~Sffv1()
{
    for (size_t i = 0; i < m_sffv1Container.size(); i++)
        delete [] m_sffv1Container[i].data;
}

void Sffv1::loadSffFile()
{
    uint8_t * readbuf[READBUF_SIZE];
    std::ifstream charfile(m_filename.c_str());
    // First 512 bytes: header
    // Signature at the start of the file: 'ElecbyteSpr\0'
    charfile.read((char *) readbuf, 12);
    if (strcmp((char *) readbuf, "ElecbyteSpr")) {
        throw std::runtime_error(std::string("Invalid sprite file: ") + m_filename);
    }
    // Version bytes
    std::array<uint8_t, 4> version = extract_version(charfile);
    // if the version is too high, throw it
    if (version[3] > 1)
        throw version;
    // Number of groups
   m_ngroups = read_uint32(charfile);
   m_nimages = read_uint32(charfile);
   uint32_t nextSubfileOffset = read_uint32(charfile);
   uint32_t subheaderSize = read_uint32(charfile);
   uint8_t sharedPaletteByte;
   charfile >> sharedPaletteByte;
   m_sharedPalette = (sharedPaletteByte != 0);
    // Reading the subfiles
    while (charfile.good() && ((int) nextSubfileOffset) > 0 && m_sffv1Container.size() < m_nimages) {
        SpriteInfo sprite;
        charfile.seekg(nextSubfileOffset, std::ios::beg);
        nextSubfileOffset = read_uint32(charfile);
        sprite.dataSize = read_uint32(charfile);
        sprite.axisX = read_uint16(charfile);
        sprite.axisY = read_uint16(charfile);
        sprite.group = read_uint16(charfile);
        sprite.groupimage = read_uint16(charfile);
        sprite.linkedindex = read_uint16(charfile);
        uint8_t usesSharedPaletteByte;
        charfile >> usesSharedPaletteByte;
        sprite.usesSharedPalette = (usesSharedPaletteByte != 0);
        charfile.seekg(13, std::ios::cur); // skipping the next 12 bytes, from byte 19 to byte 31
        // According to formats.txt:
        // "PCX graphic data. If palette data is available, it is the last 768 bytes."
        sprite.data = new uint8_t[sprite.dataSize];
        charfile.read((char *) sprite.data, sprite.dataSize);
        // Add the sprites' index to its group and image number
        m_groups[sprite.group].i[sprite.groupimage] = m_sffv1Container.size();
        m_sffv1Container.emplace_back(std::move(sprite));
    }
    charfile.close();
}

void Sffv1::loadSharedPalettes()
{
	if (m_paletteFile.empty())
		return;
    bool continueLoop = true;
    // We get the values of pal1, pal2, pal3, ... etc in order until pal12
    for (int i = 1; continueLoop && i <= 12; i++) {
        std::string keyname = "pal";
        keyname += std::to_string(i);
        try {
            continueLoop = readActPalette(m_paletteFile.c_str());
        }
        catch
            (std::out_of_range) { // thrown by character.getdef() if the method at doesn't find the key
            break;
        }
    }
}

const Sffv1::PaletteInfo Sffv1::getPaletteForSprite(size_t spriteNumber, size_t currentPaletteId)
{
    PaletteInfo s;
    size_t iterationNumber = m_sprites.size();
    if (m_sharedPalette && m_sffv1Container[spriteNumber].usesSharedPalette)
        return m_palettes[currentPaletteId];
    while (m_sffv1Container[spriteNumber].usesSharedPalette && iterationNumber > 0) {
        iterationNumber--;
		if (spriteNumber > 0)
			spriteNumber--;
        else
            spriteNumber += m_sffv1Container.size() - 1;
    }
    SpriteInfo & paletteSprite = m_sffv1Container[spriteNumber];
    if (paletteSprite.dataSize > 768 && paletteSprite.data[paletteSprite.dataSize - 768 - 1] == 0x0C) {
        uint8_t * paletteData = paletteSprite.data + (paletteSprite.dataSize - 768);
        for (int i = 0; i < PALETTE_NCOLORS; i++) {
            s.colors[i] = (Color) {
                *(paletteData + 3 * i), *(paletteData + 3 * i + 1), *(paletteData + 3 * i + 2)
            };
        }
        return s;
    }
    return m_palettes[currentPaletteId];
}


Sffv1::Drawer::Drawer(const SpriteInfo& sprite, const PaletteInfo& palette): Nugem::SurfaceDrawer(sprite.width(), sprite.height()), m_sprite(sprite), m_palette(palette)
{}

void Sffv1::Drawer::draw(uint32_t * pixelData, size_t width, size_t height)
{
	// PCX file format: data starts at offset 128
	// before offset 128 is the header: see the Sffv1Sprite methods
    uint8_t * dataStart = m_sprite.data + 128;
    size_t i_pixel, i_byte;
    for (i_pixel = 0, i_byte = 0; i_pixel < width * height && i_byte < (m_sprite.dataSize - 128); i_byte++) {
        uint16_t runLength;
        SDL_Color sdlcolor;
        uint8_t colorIndex;
        if ((dataStart[i_byte] & 0xC0) == 0xC0) { // RLE byte
            runLength = dataStart[i_byte] & 0x3F;
            i_byte++;
            colorIndex = dataStart[i_byte];
        }
        else { // simple pixel byte
            runLength = 1;
            colorIndex = dataStart[i_byte];
        }
        if (colorIndex) { // if not null
            const Color & color = m_palette.colors[dataStart[i_byte]];
            sdlcolor = (SDL_Color) {
                color.red, color.green, color.blue, 0xFF
            };
        }
        else // apply transparency if the color value is zero
            sdlcolor = (SDL_Color) {
            0, 0, 0, 0
        };
        for (int runCount = runLength; runCount > 0 && i_pixel < width * height; runCount--, i_pixel++)
            *(pixelData + i_pixel) = rgba(sdlcolor.r, sdlcolor.g, sdlcolor.b, sdlcolor.a);
    }
}

SDL_Surface * Sffv1::renderToSurface(size_t spriteNumber, size_t currentPaletteId)
{
    size_t displayedSpriteNumber = spriteNumber;
    if (m_sffv1Container[spriteNumber].linkedindex && !m_sffv1Container[spriteNumber].dataSize)
        displayedSpriteNumber = m_sffv1Container[spriteNumber].linkedindex;
    return Drawer(m_sffv1Container[displayedSpriteNumber], getPaletteForSprite(displayedSpriteNumber, currentPaletteId))();
}

bool Sffv1::readActPalette(const char * filepath)
{
    Sffv1::PaletteInfo palette;
    std::ifstream actfile;
    // reading a .act file: a Photoshop 8-bit palette
    try {
        actfile.open(filepath);
        if (actfile.fail())
            return false;
        // for some reason the colors are in reverse order
        // it didn't appear to be in the official specification though
        for (int i_palette = 0; i_palette < PALETTE_NCOLORS && actfile.good(); i_palette++) {
            palette.colors[PALETTE_NCOLORS - 1 - i_palette].red = actfile.get();
            palette.colors[PALETTE_NCOLORS - 1 - i_palette].green = actfile.get();
            palette.colors[PALETTE_NCOLORS - 1 - i_palette].blue = actfile.get();
        }
        actfile.close();
    }
    catch
        (std::ios_base::failure()) {
        if (actfile.is_open())
            actfile.close();
        return false;
    }
    m_palettes.push_back(palette);
    return true;
}

void Sffv1::load()
{
    m_sprites.clear();
    for (size_t currentPalette = 0; currentPalette < m_palettes.size(); currentPalette++) {
        std::unordered_map<Spriteref, Sprite> currentPaletteSprites;
        for (size_t currentSprite = 0; currentSprite < m_sffv1Container.size(); currentSprite++) {
            SpriteInfo & sprite = m_sffv1Container[currentSprite];
            Spriteref ref(sprite.group, sprite.groupimage);
            currentPaletteSprites.insert(std::pair<Spriteref, Sprite>(ref, Sprite(ref, renderToSurface(currentSprite, currentPalette), currentPalette)));
        }
        m_sprites.push_back(currentPaletteSprites);
    }
}

void Sffv1::load(std::vector<Spriteref>::iterator first, std::vector<Spriteref>::iterator last)
{
    m_sprites.clear();
    for (size_t currentPalette = 0; currentPalette < m_palettes.size(); currentPalette++) {
        m_sprites.push_back(std::unordered_map<Spriteref, Sprite>());
    }

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
