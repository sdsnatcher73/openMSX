// $Id$

#include "GLUtil.hh"
#include <cassert>
#include <cstdlib>
#include <cstring>


namespace openmsx {


// class Texture

Texture::Texture()
{
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

Texture::~Texture()
{
	glDeleteTextures(1, &textureId);
}


// class BitmapTexture

BitmapTexture::BitmapTexture()
	: Texture()
{
	static const unsigned SIZE = WIDTH * HEIGHT * 4;
	void* blackness = malloc(SIZE);
	memset(blackness, 0, SIZE);
	bind();
	glTexImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		GL_RGBA,          // internal format
		512,              // width
		1024,             // height
		0,                // border
		GL_RGBA,          // format
		GL_UNSIGNED_BYTE, // type
		blackness         // data
		);
	free(blackness);
}

void BitmapTexture::update(int y, const GLuint* data, int lineWidth)
{
	assert(0 <= y && y < HEIGHT);
	assert(lineWidth <= WIDTH);
	bind();
	// Replace line in existing texture.
	glTexSubImage2D(
		GL_TEXTURE_2D,    // target
		0,                // level
		0,                // x-offset
		y,                // y-offset
		lineWidth,        // width
		1,                // height
		GL_RGBA,          // format
		GL_UNSIGNED_BYTE, // type
		data              // data
		);
}

void BitmapTexture::draw(
	int srcL, int srcT, int srcR, int srcB,
	int dstL, int dstT, int dstR, int dstB
	)
{
	static const GLfloat fx = 1.0f / static_cast<GLfloat>(WIDTH);
	static const GLfloat fy = 1.0f / static_cast<GLfloat>(HEIGHT);
	const GLfloat srcLF = srcL * fx;
	const GLfloat srcRF = srcR * fx;
	const GLfloat srcTF = srcT * fy;
	const GLfloat srcBF = srcB * fy;
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(srcLF, srcTF); glVertex2i(dstL, dstT);
	glTexCoord2f(srcRF, srcTF); glVertex2i(dstR, dstT);
	glTexCoord2f(srcRF, srcBF); glVertex2i(dstR, dstB);
	glTexCoord2f(srcLF, srcBF); glVertex2i(dstL, dstB);
	glEnd();
}


// class LineTexture

LineTexture::LineTexture()
	: Texture(), prevLineWidth(-1)
{
}

void LineTexture::update(const GLuint* data, int lineWidth)
{
	bind();
	if (prevLineWidth == lineWidth) {
		// reuse existing texture
		glTexSubImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			0,                // x-offset
			0,                // y-offset
			lineWidth,        // width
			1,                // height
			GL_RGBA,          // format
			GL_UNSIGNED_BYTE, // type
			data              // data
			);
	} else {
		// create new texture
		prevLineWidth = lineWidth;
		glTexImage2D(
			GL_TEXTURE_2D,    // target
			0,                // level
			GL_RGBA,          // internal format
			lineWidth,        // width
			1,                // height
			0,                // border
			GL_RGBA,          // format
			GL_UNSIGNED_BYTE, // type
			data              // data
			);
	}
}

void LineTexture::draw(
	int texX, int screenX, int screenY, int width, int height)
{
	double texL = texX / 512.0f;
	double texR = (texX + width) / 512.0f;
	int screenL = screenX;
	int screenR = screenL + width;
	bind();
	glBegin(GL_QUADS);
	glTexCoord2f(texL, 1.0f); glVertex2i(screenL, screenY);
	glTexCoord2f(texR, 1.0f); glVertex2i(screenR, screenY);
	glTexCoord2f(texR, 0.0f); glVertex2i(screenR, screenY + height);
	glTexCoord2f(texL, 0.0f); glVertex2i(screenL, screenY + height);
	glEnd();
}


// class StoredFrame

static unsigned powerOfTwo(unsigned a)
{
	unsigned res = 1;
	while (a > res) res <<= 1;
	return res;
}

StoredFrame::StoredFrame()
	: stored(false)
{
}

void StoredFrame::store(unsigned x, unsigned y)
{
	texture.bind();
	if (width == x && height == y) {
		glCopyTexSubImage2D(
			GL_TEXTURE_2D, // target
			0,             // level
			0,             // x-offset
			0,             // y-offset
			0,             // x
			0,             // y
			width,         // width
			height         // height
			);
	} else {
		width = x;
		height = y;
		glCopyTexImage2D(
			GL_TEXTURE_2D,      // target
			0,                  // level
			GL_RGB,             // internal format
			0,                  // x
			0,                  // y
			powerOfTwo(width),  // width
			powerOfTwo(height), // height
			0                   // border
			);
	}
	stored = true;
}

void StoredFrame::draw(int offsetX, int offsetY, int width, int height)
{
	assert(stored);
	glEnable(GL_TEXTURE_2D);
	texture.bind();
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	// TODO: Create display list(s)?
	float x = static_cast<float>(width)  / powerOfTwo(width);
	float y = static_cast<float>(height) / powerOfTwo(height);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,    y); glVertex2i(offsetX,         offsetY         );
	glTexCoord2f(   x,    y); glVertex2i(offsetX + width, offsetY         );
	glTexCoord2f(   x, 0.0f); glVertex2i(offsetX + width, offsetY + height);
	glTexCoord2f(0.0f, 0.0f); glVertex2i(offsetX,         offsetY + height);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void StoredFrame::drawBlend(
	int offsetX, int offsetY, int width, int height, double alpha
	)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// RGB come from texture, alpha comes from fragment colour.
	glColor4f(1.0, 0.0, 0.0, alpha);
	draw(offsetX, offsetY, width, height);
	glDisable(GL_BLEND);
}

} // namespace openmsx

