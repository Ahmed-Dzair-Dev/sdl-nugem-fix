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
#include <array>
#include "glgraphics.hpp"

#include <iostream>
#include <fstream>
#include <glm/geometric.hpp>
#include <glm/ext.hpp>

namespace Nugem {

std::unordered_map<GLuint, unsigned int> GlTexture::useCounters;

GlTexture::GlTexture(GLuint tid, int w, int h): tid(tid), w(w), h(h)
{
    if (tid)
        useCounters[tid]++;
}


GlTexture::~GlTexture()
{
    if (tid) {
        useCounters[tid]--;
        if (useCounters[tid] <= 0) {
            glDeleteTextures(1, &tid);
            useCounters.erase(tid);
        }
    }
}

GlTexture::GlTexture(const GlTexture & glTexture)
{
    tid = glTexture.tid;
    w = glTexture.w;
    h = glTexture.h;
    if (tid)
        useCounters[tid]++;
}

GlTexture::GlTexture(GlTexture && glTexture): tid(std::move(glTexture.tid)), w(std::move(glTexture.w)), h(std::move(glTexture.h))
{
}

GlShader::GlShader(GLuint number)
{
    shaderId = number;
}

GlShader::GlShader(GlShader &&original): shaderId(std::move(original.shaderId))
{
}

GlShader::~GlShader()
{
    deleteShader();
}

void GlShader::deleteShader()
{
    if (shaderId) {
        glDeleteShader(shaderId);
        shaderId = 0;
    }
}

GlShader GlShader::fromString(const std::string &contents, GLuint type)
{
    GLuint newShaderId = glCreateShader(type);
    const char *cstr = contents.c_str();
    glShaderSource(newShaderId, 1, &cstr, NULL);
    return GlShader(newShaderId);
}

GlShader GlShader::fromFile(const std::string &filename, GLuint type)
{
    std::ifstream inputfile(filename);
    std::string str;

    inputfile.seekg(0, std::ios::end);
    str.reserve(inputfile.tellg());
    inputfile.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(inputfile)),
               std::istreambuf_iterator<char>());
    return fromString(str, type);
}

bool GlShader::compile()
{
    glCompileShader(shaderId);
    GLint status;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(shaderId, 512, NULL, buffer);
        std::cerr << buffer << std::endl;
        return false;
    }
    return true;
}

GlShaderProgram::GlShaderProgram()
{
    shaderProgramId = glCreateProgram();
}

GlShaderProgram::~GlShaderProgram()
{
    glDeleteProgram(shaderProgramId);
}

void GlShaderProgram::attachShader(const GlShader &glShader)
{
    glAttachShader(shaderProgramId, glShader.shaderId);
}

bool GlShaderProgram::link()
{
    GLint success;
    glLinkProgram(shaderProgramId);
    glGetProgramiv(shaderProgramId, GL_LINK_STATUS, &success);
    if(!success) {
        char buffer[512];
        glGetProgramInfoLog(shaderProgramId, 512, NULL, buffer);
        std::cerr << buffer << std::endl;
        return false;
    }
    return true;
}

void GlShaderProgram::use()
{
    glUseProgram(shaderProgramId);
}

GlGraphics::GlGraphics(Window &window): m_window(window), m_lastTidUsed(0)
{
    //Use OpenGL 3.3 core

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // This is important on macOS
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    // Get window context

    m_sdlGlCtx = m_window.createGlContext();

    glewExperimental = GL_TRUE;
	{
		GLenum glewError = glewInit();
		if (glewError != GLEW_OK) {
			std::cout << "Error initializing GLEW!" << glewGetErrorString(glewError) << std::endl;
		}
	}

//     //Use Vsync
//     if (SDL_GL_SetSwapInterval(1) < 0) {
//         std::cout << "Warning: Unable to set VSync! SDL Error:" << SDL_GetError() << std::endl;
//     }
}

GlGraphics::~GlGraphics()
{
    SDL_GL_DeleteContext(m_sdlGlCtx);
}

const Window &GlGraphics::window() const
{
	return m_window;
}

void GlGraphics::initialize(Game * game)
{
    m_game = game;

	{
		GlShader vertexShader = GlShader::fromFile("assets/shaders/sprite.vert", GL_VERTEX_SHADER);
		vertexShader.compile();
		GlShader fragmentShader = GlShader::fromFile("assets/shaders/sprite.frag", GL_FRAGMENT_SHADER);
		fragmentShader.compile();
		      m_shaderProgram = glCreateProgram();
		glAttachShader(m_shaderProgram, vertexShader.shaderId);
		glAttachShader(m_shaderProgram, fragmentShader.shaderId);
		GLint success;
		glLinkProgram(m_shaderProgram);
		glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
		if(!success) {
			char buffer[512];
			glGetProgramInfoLog(m_shaderProgram, 512, NULL, buffer);
			std::cerr << buffer << std::endl;
		}
		glUseProgram(m_shaderProgram);
	}

        m_positionVertAttrib = glGetAttribLocation(m_shaderProgram, "position");
    if (m_positionVertAttrib == static_cast<GLuint>(-1)) {
        m_positionVertAttrib = 0;
        std::cerr << "Could not bind position attrib" << std::endl;
    }


    m_texCoordsAttrib = glGetAttribLocation(m_shaderProgram, "v_texCoords");
    if (m_texCoordsAttrib == static_cast<GLuint>(-1)) {
        m_texCoordsAttrib = 0;
        std::cerr << "Could not bind tex coords attrib" << std::endl;
    }

    m_uniformMvp = glGetUniformLocation(m_shaderProgram, "mvp");
    if (m_uniformMvp == -1)
        std::cerr << "Could not bind mvp uniform" << std::endl;
    m_UniformGlSpriteTexture = glGetUniformLocation(m_shaderProgram, "glSpriteTexture");
    if (m_UniformGlSpriteTexture == -1)
        std::cerr << "Could not bind sprite texture uniform" << std::endl;

    glGenBuffers(1, &m_itemPositionsBuffer);
    glGenBuffers(1, &m_itemTexCoordsBuffer);
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport(0, 0, 1920, 1080);

	   m_lastTidUsed = 0;
}

void GlGraphics::finish()
{
    glDeleteBuffers(1, &m_itemPositionsBuffer);
    glDeleteBuffers(1, &m_itemTexCoordsBuffer);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteProgram(m_shaderProgram);
}

void GlGraphics::clear()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

GlTexture GlTexture::surfaceToTexture(const SDL_Surface * surface)
{
    GLuint tid = 0;
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);

    int mode = GL_RGB;

    if(surface->format->BytesPerPixel == 4) {
        mode = GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, mode, surface->w, surface->h, 0, mode, GL_UNSIGNED_BYTE, surface->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glGenerateMipmap(GL_TEXTURE_2D);

    GlTexture tx(tid, surface->w, surface->h);

    return tx;
}

void GlGraphics::display()
{
    glUseProgram(m_shaderProgram);

    glm::mat4 mvp =  glm::ortho(0.0f, 1.0f * 1920, 1.0f * 1080, 0.0f);
    glUniformMatrix4fv(m_uniformMvp, 1, GL_FALSE, glm::value_ptr(mvp));
	for (auto &item: frameItems) {
		if (!item.positions.empty()) {
			glEnableVertexAttribArray(m_positionVertAttrib);
			glBindBuffer(GL_ARRAY_BUFFER, m_itemPositionsBuffer);
			testGlError();
			glBufferData(GL_ARRAY_BUFFER, sizeof(item.positions[0]) * item.positions.size(), item.positions.data(), GL_STATIC_DRAW);
			testGlError();
			glVertexAttribPointer(
				            m_positionVertAttrib, // attribute
				2,                  // number of elements per vertex, here (x,y)
				GL_INT,           // the type of each element
				GL_FALSE,           // take our values as-is
				0,                  // no extra data between each position
				0                   // offset of first element
			);
		}
		testGlError();
		if (!item.texCoords.empty()) {
			glUniform1i(m_UniformGlSpriteTexture, 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, item.tid);
			glEnableVertexAttribArray(m_texCoordsAttrib);
			testGlError();
			// Describe our vertices array to OpenGL (it can't guess its format automatically)
			glBindBuffer(GL_ARRAY_BUFFER, m_itemTexCoordsBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(item.texCoords[0]) * item.texCoords.size(), item.texCoords.data(), GL_STATIC_DRAW);
			glVertexAttribPointer(
				            m_texCoordsAttrib, // attribute
				2,                 // number of elements per vertex, here (x,y)
				GL_FLOAT,          // the type of each element
				GL_FALSE,          // take our values as-is
				0,                 // no extra data between each position
				0                  // offset of first element
			);
			         m_lastTidUsed = item.tid;
		}
		testGlError();
		if (!item.positions.empty()) {
			glDrawArrays(GL_TRIANGLES, 0, item.positions.size());
		}
		glDisableVertexAttribArray(m_positionVertAttrib);
		glDisableVertexAttribArray(m_texCoordsAttrib);
	}
	frameItems.clear();
    m_window.swapGlWindow();
}

void GlGraphics::passItem(GLuint tid, Positions && positions, TexCoords && texCoords)
{
	frameItems.push_back({ tid, positions, texCoords });
}

}
