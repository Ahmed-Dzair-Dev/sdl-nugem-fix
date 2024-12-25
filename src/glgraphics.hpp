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
#ifndef GLGRAPHICS_H
#define GLGRAPHICS_H

#include "window.hpp"

#include <SDL.h>
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <cassert>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#define testGlError() { \
		auto glError = glGetError(); \
		if (glError != GL_NO_ERROR) \
			std::cerr << gluErrorString(glError) << std::endl; \
		assert(glError == GL_NO_ERROR); \
	}
#else
#include <GL/gl.h>
#include <GL/glu.h>

#define testGlError() { \
		auto glError = glGetError(); \
		if (glError != GL_NO_ERROR) \
			std::cerr << gluErrorString(glError) << std::endl; \
		assert(glError == GL_NO_ERROR); \
	}

#endif

#include <vector>
#include <memory>
#include <unordered_map>


namespace Nugem {

struct GlTexture {
	GLuint tid = 0;
	int w;
	int h;
	GlTexture(GLuint, int, int);
	GlTexture(const GlTexture &);
	GlTexture(GlTexture &&);
	~GlTexture();
	static GlTexture surfaceToTexture(const SDL_Surface * surface);
private:
	static std::unordered_map<GLuint, unsigned int> useCounters;
};

struct GlShader {
	GLuint shaderId;
	GlShader(GLuint);
	GlShader(GlShader &&);
	static GlShader fromFile(const std::string &, GLuint type);
	static GlShader fromString(const std::string &, GLuint type);
	void deleteShader();
	~GlShader();
	bool compile();
};

struct GlShaderProgram {
	GLuint shaderProgramId;
	void attachShader(const GlShader &);
	bool link();
	void use();
	GlShaderProgram();
	~GlShaderProgram();
};
class Game;

class GlGraphics {
public:
	typedef std::vector<std::array<GLint, 2>> Positions;
	typedef std::vector<std::array<GLfloat, 2>> TexCoords;
private:
	struct InternalDisplayItem {
		GLuint tid;
		Positions positions;
		TexCoords texCoords;
	};
public:
	GlGraphics(Window &);
	~GlGraphics();
	void initialize(Game * game);
	void finish();
	void clear();
	void display();
	void passItem(GLuint tid, Positions && positions, TexCoords && texCoords);
	const Window &window() const;
private:
	Window &m_window;
	Game * m_game;
	SDL_GLContext m_sdlGlCtx;
	GLuint m_positionVertAttrib;
	GLuint m_texCoordsAttrib;
	GLuint m_vao;
	GLuint m_itemPositionsBuffer;
	GLuint m_itemTexCoordsBuffer;
	GLint m_uniformMvp;
	GLint m_UniformGlSpriteTexture;
	GLint m_shaderProgram;
	GLuint m_lastTidUsed;
	std::vector<InternalDisplayItem> frameItems;
	
};

}

#endif // GLGRAPHICS_H
