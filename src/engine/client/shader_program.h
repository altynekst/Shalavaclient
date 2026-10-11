#pragma once

#include <GL/glew.h>
#include <string>
#include <unordered_map>

class CShaderProgram
{
public:
	CShaderProgram() = default;
	~CShaderProgram();

	bool Compile(const char *pVertexSrc, const char *pFragmentSrc);
	void Bind() const;
	static void Unbind();
	void Destroy();

	void SetFloat(const char *pName, float Value);
	void SetVec2(const char *pName, float x, float y);
	void SetVec4(const char *pName, float x, float y, float z, float w);
	void SetInt(const char *pName, int Value);

	GLuint GetProgram() const { return m_Program; }
	bool IsValid() const { return m_Program != 0; }

private:
	GLuint CompileShader(GLenum Type, const char *pSrc);
	GLint GetUniformLoc(const char *pName);

	GLuint m_Program = 0;
	std::unordered_map<std::string, GLint> m_UniformCache;
};