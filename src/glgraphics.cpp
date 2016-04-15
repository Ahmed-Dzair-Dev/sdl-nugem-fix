/*
 * Copyright (C) Victor Nivet
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

#include "glgraphics.h"

#include <iostream>

GlGraphics::GlGraphics()
{
}

GlGraphics::~GlGraphics()
{
}

void GlGraphics::windowSize(int * width, int * height)
{
	SDL_GetWindowSize(m_window, width, height);
}

void GlGraphics::initialize(Game * game, SDL_Window * window)
{
	m_window = window;
	m_sdlglctx = SDL_GL_CreateContext(m_window);
	m_game = game;
	
	int winw, winh;
	
	windowSize(&winw, &winh);
	
    glViewport(0, 0, winw, winh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glPushMatrix(); //Start phase

	glOrtho(0, winw, winh, 0, -1, 1);
}

void GlGraphics::finish()
{
	SDL_GL_DeleteContext(m_sdlglctx);
}

void GlGraphics::clear()
{
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

GlTexture GlGraphics::surfaceToTexture(const SDL_Surface * surface)
{
	GLuint tid = 0;
	glGenTextures(1, &tid);
	glBindTexture(GL_TEXTURE_2D, tid);
	
	int Mode = GL_RGB;
	
	if(surface->format->BytesPerPixel == 4) {
		Mode = GL_RGBA;
	}
	
	glTexImage2D(GL_TEXTURE_2D, 0, Mode, surface->w, surface->h, 0, Mode, GL_UNSIGNED_BYTE, surface->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	GlTexture tx;
	
	tx.tid = tid;
	tx.w = surface->w;
	tx.h = surface->h;
	
	return tx;
}

void GlGraphics::render2DTexture(GlTexture & texture, const SDL_Rect * dstrect)
{
	glEnable(GL_TEXTURE_2D);
	
	glBindTexture(GL_TEXTURE_2D, texture.tid);
	
	int X, Y, Width, Height;
	if (dstrect != nullptr) {
		X = dstrect->x;
		Y = dstrect->y;
		Width = dstrect->w;
		Height = dstrect->h;
	}
	else {
		X = 0;
		Y = 0; 
		Width = texture.w;
		Height = texture.h;
	}
	
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex3f(X, Y, 0);
		glTexCoord2f(1, 0); glVertex3f(X + Width, Y, 0);
		glTexCoord2f(1, 1); glVertex3f(X + Width, Y + Height, 0);
		glTexCoord2f(0, 1); glVertex3f(X, Y + Height, 0);
	glEnd();
	
	glDisable(GL_TEXTURE_2D);
}

void GlGraphics::display()
{
	SDL_GL_SwapWindow(m_window);
}

GlTexture::~GlTexture()
{
// 	if (tid)
// 		glDeleteTextures(1, &tid);
}
