#pragma once

#include <GL/glew.h>

class CFramebuffer
{
public:
	CFramebuffer() = default;
	~CFramebuffer();

	bool Create(int Width, int Height);
	void Destroy();
	void Bind();
	void Unbind();
	bool Resize(int Width, int Height);

	GLuint GetTexture() const { return m_Texture; }
	GLuint GetFBO() const { return m_FBO; }
	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }
	bool IsValid() const { return m_FBO != 0 && m_Texture != 0; }

private:
	GLuint m_FBO = 0;
	GLuint m_Texture = 0;
	int m_Width = 0;
	int m_Height = 0;
};