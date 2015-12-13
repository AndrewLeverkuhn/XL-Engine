////////////////////////////////////////////////////////////////////
// This now works but its a really rough implementation.
// Several things have to be done for it to be good enough,
// such as being able to specify shaders, handling resources
// better, handling dynamic vertex data better (its super basic
// and slow right now), etc.
// Due to these issues this currently runs the UI at 1/2 the
// framerate as the OpenGL 1.3 device - though the 
// actual game view runs at the same speed.
////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include "graphicsDeviceOGL_2_0.h"
#include "graphicsShaders_OGL_2_0.h"
#include "../../log.h"
#include "../../memoryPool.h"
#include "../common/shaderOGL.h"
#include "../common/vertexBufferOGL.h"
#include "../common/indexBufferOGL.h"

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

//Temporary, to get basic rendering working.
struct QuadVertex
{
	f32 pos[3];
	f32 uv[2];
	u32 color;
};

const VertexElement c_quadVertexDecl[]=
{
	{ 0, VATTR_POSITION,  VTYPE_FLOAT32_3, false },
	{ 0, VATTR_TEXCOORD0, VTYPE_FLOAT32_2, false },
	{ 0, VATTR_COLOR,     VTYPE_UINT8_4,   true  },
};


GraphicsDeviceOGL_2_0::GraphicsDeviceOGL_2_0(GraphicsDevicePlatform* platform) : GraphicsDeviceOGL(platform)
{
	m_deviceID = GDEV_OPENGL_2_0;
	m_textureEnabled = true;
}

GraphicsDeviceOGL_2_0::~GraphicsDeviceOGL_2_0()
{
	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &m_VideoFrameBuffer);

	delete m_quadVB;
	delete m_quadIB;
}

void GraphicsDeviceOGL_2_0::drawVirtualScreen()
{
	glViewport( m_virtualViewport[0], m_virtualViewport[1], m_virtualViewport[2], m_virtualViewport[3] );

	//update the video memory framebuffer.
	//TO-DO: This device should support PBOs which should allow for much faster transfers.
	glBindTexture(GL_TEXTURE_2D, m_VideoFrameBuffer);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, m_FrameWidth, m_FrameHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_pFrameBuffer_32bpp);
	glBindTexture(GL_TEXTURE_2D, 0);

	ShaderOGL* shader = m_shaders->getShader( GraphicsShadersOGL_2_0::SHADER_QUAD_UI );
	s32 baseTex = shader->getParameter("baseTex");
	shader->updateParameter(baseTex, m_VideoFrameBuffer, 0);

	drawFullscreenQuad();

	glViewport( m_fullViewport[0], m_fullViewport[1], m_fullViewport[2], m_fullViewport[3] );
}

void GraphicsDeviceOGL_2_0::setVirtualViewport(bool reset, int x, int y, int w, int h)
{
	if (reset)
	{
		for (int i=0; i<4; i++) { m_virtualViewport[i] = m_virtualViewportNoUI[i]; }
	}
	else
	{
		m_virtualViewport[0] = x;
		m_virtualViewport[1] = y;
		m_virtualViewport[2] = w;
		m_virtualViewport[3] = h;
	}
}

bool GraphicsDeviceOGL_2_0::init(int w, int h, int vw, int vh)
{
	m_platform->init();

	m_shaders = new GraphicsShadersOGL_2_0();
	if (!m_shaders)
	{
		return false;
	}
	m_shaders->loadShaders();

	m_quadVB = new VertexBufferOGL(true);
	m_quadVB->allocate( sizeof(QuadVertex), 4 );
	m_quadVB->setVertexDecl( c_quadVertexDecl, arraysize(c_quadVertexDecl) );

	u16 indices[6]=
	{
		0, 1, 2,
		0, 2, 3
	};
	m_quadIB = new IndexBufferOGL();
	m_quadIB->allocate( sizeof(u16), 6, indices );

	/* establish initial viewport */
	// fix the height to the actual screen height.
	// calculate the width such that wxh has a 4:3 aspect ratio (assume square pixels for modern LCD monitors).
	// offset the viewport by half the difference between the actual width and 4x3 width
	// to automatically pillar box the display.
	// Unfortunately, for modern monitors, only 1200p yields exact integer scaling. So we do all the scaling on
	// the horizontal axis, where there are more pixels, to limit the visible distortion.
	int w4x3 = (h<<2) / 3;
	int dw   = w - w4x3;
	m_virtualViewportNoUI[0] = dw>>1;
	m_virtualViewportNoUI[1] = 0;
	m_virtualViewportNoUI[2] = w4x3;
	m_virtualViewportNoUI[3] = h;

	for (int i=0; i<4; i++) { m_virtualViewport[i] = m_virtualViewportNoUI[i]; }

	m_fullViewport[0] = 0;
	m_fullViewport[1] = 0;
	m_fullViewport[2] = w;
	m_fullViewport[3] = h;

	m_nWindowWidth  = w;
	m_nWindowHeight = h;
	
	//frame size - this is the 32 bit version of the framebuffer. The 8 bit framebuffer, rendered by the software renderer, 
	//is converted to 32 bit (using the current palette) - this buffer - before being uploaded to the video card.
	m_FrameWidth  = vw;
	m_FrameHeight = vh;
	m_pFrameBuffer_32bpp = new u32[ m_FrameWidth*m_FrameHeight ];

	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_DEPTH_TEST); // disable depth buffering
	glDisable(GL_CULL_FACE);

	//Create a copy of the framebuffer on the GPU so we can upload the results there.
	glGenTextures(1, &m_VideoFrameBuffer);
	glBindTexture(GL_TEXTURE_2D, m_VideoFrameBuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_TEXTURE_2D);
	m_textureEnabled = true;
	
	glFlush();

	return true;
}

TextureHandle GraphicsDeviceOGL_2_0::createTextureRGBA(int width, int height, unsigned int* data)
{
	GLuint texHandle = 0;
	glGenTextures(1, &texHandle);
	glBindTexture(GL_TEXTURE_2D, texHandle);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return TextureHandle(texHandle);
}

void GraphicsDeviceOGL_2_0::setShaderResource(TextureHandle handle, u32 nameHash)
{
	ShaderOGL* shader = m_shaders->getShader( GraphicsShadersOGL_2_0::SHADER_QUAD_UI );
	s32 parmID = shader->getParameter(nameHash);
	shader->updateParameter(parmID, handle, 0);
}

void GraphicsDeviceOGL_2_0::setTexture(TextureHandle handle, int slot/*=0*/)
{
	glActiveTexture(GL_TEXTURE0 + slot);

	if (handle != INVALID_TEXTURE_HANDLE)
	{
		glBindTexture(GL_TEXTURE_2D, GLuint(handle));
		if (!m_textureEnabled)
		{
			glEnable(GL_TEXTURE_2D);
			m_textureEnabled = true;
		}
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, 0);
		if (m_textureEnabled)
		{
			glDisable(GL_TEXTURE_2D);
			m_textureEnabled = false;
		}
	}
}

void GraphicsDeviceOGL_2_0::drawQuad(const Quad& quad)
{
	//scale and display.
	float posScale[] = {-1.0f, -1.0f, 2.0f, 2.0f};
	float uvTop[] = { 0, 1 };
	float uvBot[] = { 1, 0 };

	posScale[0] =  2.0f * float(quad.p0.x) / float(m_nWindowWidth)  - 1.0f;
	posScale[1] = -2.0f * float(quad.p0.y) / float(m_nWindowHeight) + 1.0f;

	posScale[2] =  2.0f * float(quad.p1.x) / float(m_nWindowWidth)  - 1.0f;
	posScale[3] = -2.0f * float(quad.p1.y) / float(m_nWindowHeight) + 1.0f;

	uvTop[0] = quad.uv0.x;
	uvTop[1] = quad.uv0.y;
	uvBot[0] = quad.uv1.x;
	uvBot[1] = quad.uv1.y;

	//for now lock and fill the vb everytime.
	//this is obviously a bad idea but will get things started.
	const QuadVertex vertex[]=
	{
		{ posScale[0], posScale[1], 0.0f, uvTop[0], uvTop[1], quad.color },
		{ posScale[2], posScale[1], 0.0f, uvBot[0], uvTop[1], quad.color },
		{ posScale[2], posScale[3], 0.0f, uvBot[0], uvBot[1], quad.color },
		{ posScale[0], posScale[3], 0.0f, uvTop[0], uvBot[1], quad.color },
	};
	m_quadVB->update(sizeof(QuadVertex)*4, (void*)vertex);

	ShaderOGL* shader = m_shaders->getShader( GraphicsShadersOGL_2_0::SHADER_QUAD_UI );
	shader->bind();
	shader->uploadData(this);

	m_quadVB->bind( shader->getRequiredVertexAttrib() );
	m_quadIB->bind();

	glDrawRangeElements(GL_TRIANGLES, 0, 6, 6, (m_quadIB->m_stride==2)?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT, 0);
}

void GraphicsDeviceOGL_2_0::drawFullscreenQuad()
{
	//scale and display.
	const float uvTop[] = { 0, 1 };
	const float uvBot[] = { 1, 0 };

	//for now lock and fill the vb everytime.
	//this is obviously a bad idea but will get things started.
	const QuadVertex vertex[]=
	{
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 0xffffffff },
		{  1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0xffffffff },
		{  1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0xffffffff },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0xffffffff },
	};
	m_quadVB->update(sizeof(QuadVertex)*4, (void*)vertex);

	ShaderOGL* shader = m_shaders->getShader( GraphicsShadersOGL_2_0::SHADER_QUAD_UI );
	shader->bind();
	shader->uploadData(this);

	m_quadVB->bind( shader->getRequiredVertexAttrib() );
	m_quadIB->bind();

	glDrawRangeElements(GL_TRIANGLES, 0, 6, 6, (m_quadIB->m_stride==2)?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT, 0);
}