#pragma once

#include "shader_program.h"
#include "framebuffer.h"
#include <GL/glew.h>
#include <imgui.h>

class CGlassRenderer
{
public:
	static CGlassRenderer &Instance();

	bool Init();
	void Shutdown();

	void CaptureScreen(int ScreenW, int ScreenH);
	void GenerateBlur(int Iterations = 4);

	void AddGlassRect(ImDrawList *pDraw, ImVec2 Min, ImVec2 Max,
		ImVec4 TintColor = ImVec4(0.06f, 0.06f, 0.08f, 0.35f),
		float Rounding = 12.0f,
		float BorderAlpha = 0.12f,
		float BlurAlpha = 0.75f);

	GLuint GetBlurredTexture() const;
	bool IsReady() const { return m_Initialized && m_ScreenCapture.IsValid(); }

private:
	CGlassRenderer() = default;

	bool CompileShaders();
	void CreateQuadGeometry();
	void DrawFullscreenQuad();
	void SaveGLState();
	void RestoreGLState();

	bool m_Initialized = false;
	int m_ScreenW = 0;
	int m_ScreenH = 0;

	CFramebuffer m_ScreenCapture;
	static constexpr int MAX_BLUR_MIPS = 5;
	CFramebuffer m_BlurMips[MAX_BLUR_MIPS];
	int m_BlurLevels = 4;

	CShaderProgram m_DownsampleShader;
	CShaderProgram m_UpsampleShader;

	GLuint m_QuadVAO = 0;
	GLuint m_QuadVBO = 0;

	struct SGLState
	{
		GLint Program;
		GLint VAO;
		GLint ArrayBuffer;
		GLint ActiveTexture;
		GLint BoundTexture;
		GLint BoundFBO;
		GLint Viewport[4];
		GLboolean Blend;
		GLboolean DepthTest;
		GLboolean CullFace;
		GLboolean ScissorTest;
	} m_SavedState;
};