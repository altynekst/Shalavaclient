#include "framebuffer.h"
#include <base/system.h>

CFramebuffer::~CFramebuffer()
{
	Destroy();
}

bool CFramebuffer::Create(int Width, int Height)
{
	if(Width <= 0 || Height <= 0)
		return false;
	Destroy();

	m_Width = Width;
	m_Height = Height;

	glGenFramebuffers(1, &m_FBO);
	glGenTextures(1, &m_Texture);

	glBindTexture(GL_TEXTURE_2D, m_Texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

	GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if(Status != GL_FRAMEBUFFER_COMPLETE)
	{
		dbg_msg("framebuffer", "incomplete: 0x%x", Status);
		Destroy();
		return false;
	}

	return true;
}

void CFramebuffer::Destroy()
{
	if(m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
	if(m_Texture) { glDeleteTextures(1, &m_Texture); m_Texture = 0; }
	m_Width = m_Height = 0;
}

void CFramebuffer::Bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glViewport(0, 0, m_Width, m_Height);
}

void CFramebuffer::Unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool CFramebuffer::Resize(int Width, int Height)
{
	if(Width == m_Width && Height == m_Height && IsValid())
		return true;
	return Create(Width, Height);
}