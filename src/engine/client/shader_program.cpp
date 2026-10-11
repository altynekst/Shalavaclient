#include "shader_program.h"
#include <base/system.h>

CShaderProgram::~CShaderProgram()
{
	Destroy();
}

bool CShaderProgram::Compile(const char *pVertexSrc, const char *pFragmentSrc)
{
	Destroy();

	GLuint vs = CompileShader(GL_VERTEX_SHADER, pVertexSrc);
	if(!vs) return false;

	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, pFragmentSrc);
	if(!fs)
	{
		glDeleteShader(vs);
		return false;
	}

	m_Program = glCreateProgram();
	glAttachShader(m_Program, vs);
	glAttachShader(m_Program, fs);

	glBindAttribLocation(m_Program, 0, "aPos");
	glBindAttribLocation(m_Program, 1, "aTexCoord");

	glLinkProgram(m_Program);

	GLint Linked = 0;
	glGetProgramiv(m_Program, GL_LINK_STATUS, &Linked);

	glDeleteShader(vs);
	glDeleteShader(fs);

	if(!Linked)
	{
		char aLog[512];
		glGetProgramInfoLog(m_Program, sizeof(aLog), nullptr, aLog);
		dbg_msg("shader", "link error: %s", aLog);
		glDeleteProgram(m_Program);
		m_Program = 0;
		return false;
	}

	return true;
}

GLuint CShaderProgram::CompileShader(GLenum Type, const char *pSrc)
{
	GLuint Shader = glCreateShader(Type);
	glShaderSource(Shader, 1, &pSrc, nullptr);
	glCompileShader(Shader);

	GLint Compiled = 0;
	glGetShaderiv(Shader, GL_COMPILE_STATUS, &Compiled);
	if(!Compiled)
	{
		char aLog[512];
		glGetShaderInfoLog(Shader, sizeof(aLog), nullptr, aLog);
		dbg_msg("shader", "compile error (%s): %s",
			Type == GL_VERTEX_SHADER ? "vertex" : "fragment", aLog);
		glDeleteShader(Shader);
		return 0;
	}
	return Shader;
}

void CShaderProgram::Bind() const
{
	if(m_Program)
		glUseProgram(m_Program);
}

void CShaderProgram::Unbind()
{
	glUseProgram(0);
}

void CShaderProgram::Destroy()
{
	if(m_Program)
	{
		glDeleteProgram(m_Program);
		m_Program = 0;
	}
	m_UniformCache.clear();
}

GLint CShaderProgram::GetUniformLoc(const char *pName)
{
	auto it = m_UniformCache.find(pName);
	if(it != m_UniformCache.end())
		return it->second;
	GLint Loc = glGetUniformLocation(m_Program, pName);
	m_UniformCache[pName] = Loc;
	return Loc;
}

void CShaderProgram::SetFloat(const char *pName, float Value) { glUniform1f(GetUniformLoc(pName), Value); }
void CShaderProgram::SetVec2(const char *pName, float x, float y) { glUniform2f(GetUniformLoc(pName), x, y); }
void CShaderProgram::SetVec4(const char *pName, float x, float y, float z, float w) { glUniform4f(GetUniformLoc(pName), x, y, z, w); }
void CShaderProgram::SetInt(const char *pName, int Value) { glUniform1i(GetUniformLoc(pName), Value); }