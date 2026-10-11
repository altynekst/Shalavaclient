#include "glass_renderer.h"
#include <base/system.h>
#include <algorithm>

static const char *s_pQuadVertexShader = R"glsl(
#version 130
in vec2 aPos;
in vec2 aTexCoord;
out vec2 vTexCoord;
void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
	vTexCoord = aTexCoord;
}
)glsl";

static const char *s_pDownsampleFragShader = R"glsl(
#version 130
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uTexture;
uniform vec2 uHalfTexel;
void main()
{
	vec4 sum = texture(uTexture, vTexCoord) * 4.0;
	sum += texture(uTexture, vTexCoord + vec2(-uHalfTexel.x, -uHalfTexel.y));
	sum += texture(uTexture, vTexCoord + vec2( uHalfTexel.x, -uHalfTexel.y));
	sum += texture(uTexture, vTexCoord + vec2(-uHalfTexel.x,  uHalfTexel.y));
	sum += texture(uTexture, vTexCoord + vec2( uHalfTexel.x,  uHalfTexel.y));
	fragColor = sum / 8.0;
}
)glsl";

static const char *s_pUpsampleFragShader = R"glsl(
#version 130
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D uTexture;
uniform vec2 uHalfTexel;
void main()
{
	vec4 sum = vec4(0.0);
	sum += texture(uTexture, vTexCoord + vec2(-uHalfTexel.x * 2.0, 0.0));
	sum += texture(uTexture, vTexCoord + vec2(-uHalfTexel.x, uHalfTexel.y)) * 2.0;
	sum += texture(uTexture, vTexCoord + vec2(0.0, uHalfTexel.y * 2.0));
	sum += texture(uTexture, vTexCoord + vec2( uHalfTexel.x, uHalfTexel.y)) * 2.0;
	sum += texture(uTexture, vTexCoord + vec2( uHalfTexel.x * 2.0, 0.0));
	sum += texture(uTexture, vTexCoord + vec2( uHalfTexel.x, -uHalfTexel.y)) * 2.0;
	sum += texture(uTexture, vTexCoord + vec2(0.0, -uHalfTexel.y * 2.0));
	sum += texture(uTexture, vTexCoord + vec2(-uHalfTexel.x, -uHalfTexel.y)) * 2.0;
	fragColor = sum / 12.0;
}
)glsl";

CGlassRenderer &CGlassRenderer::Instance()
{
	static CGlassRenderer s;
	return s;
}

bool CGlassRenderer::Init()
{
	if(m_Initialized)
		return true;

	if(!CompileShaders())
	{
		dbg_msg("glass", "failed to compile shaders");
		return false;
	}

	CreateQuadGeometry();

	m_Initialized = true;
	dbg_msg("glass", "initialized");
	return true;
}

void CGlassRenderer::Shutdown()
{
	if(!m_Initialized)
		return;

	m_DownsampleShader.Destroy();
	m_UpsampleShader.Destroy();
	m_ScreenCapture.Destroy();

	for(int i = 0; i < MAX_BLUR_MIPS; i++)
		m_BlurMips[i].Destroy();

	if(m_QuadVAO) { glDeleteVertexArrays(1, &m_QuadVAO); m_QuadVAO = 0; }
	if(m_QuadVBO) { glDeleteBuffers(1, &m_QuadVBO); m_QuadVBO = 0; }

	m_Initialized = false;
}

bool CGlassRenderer::CompileShaders()
{
	if(!m_DownsampleShader.Compile(s_pQuadVertexShader, s_pDownsampleFragShader))
		return false;
	if(!m_UpsampleShader.Compile(s_pQuadVertexShader, s_pUpsampleFragShader))
		return false;
	return true;
}

void CGlassRenderer::CreateQuadGeometry()
{
	float Vertices[] = {
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f,
	};

	glGenVertexArrays(1, &m_QuadVAO);
	glGenBuffers(1, &m_QuadVBO);

	glBindVertexArray(m_QuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CGlassRenderer::DrawFullscreenQuad()
{
	glBindVertexArray(m_QuadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void CGlassRenderer::SaveGLState()
{
	glGetIntegerv(GL_CURRENT_PROGRAM, &m_SavedState.Program);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_SavedState.VAO);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_SavedState.ArrayBuffer);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &m_SavedState.ActiveTexture);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_SavedState.BoundTexture);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_SavedState.BoundFBO);
	glGetIntegerv(GL_VIEWPORT, m_SavedState.Viewport);
	m_SavedState.Blend = glIsEnabled(GL_BLEND);
	m_SavedState.DepthTest = glIsEnabled(GL_DEPTH_TEST);
	m_SavedState.CullFace = glIsEnabled(GL_CULL_FACE);
	m_SavedState.ScissorTest = glIsEnabled(GL_SCISSOR_TEST);
}

void CGlassRenderer::RestoreGLState()
{
	glUseProgram(m_SavedState.Program);
	glBindVertexArray(m_SavedState.VAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_SavedState.ArrayBuffer);
	glActiveTexture(m_SavedState.ActiveTexture);
	glBindTexture(GL_TEXTURE_2D, m_SavedState.BoundTexture);
	glBindFramebuffer(GL_FRAMEBUFFER, m_SavedState.BoundFBO);
	glViewport(m_SavedState.Viewport[0], m_SavedState.Viewport[1],
		m_SavedState.Viewport[2], m_SavedState.Viewport[3]);

	if(m_SavedState.Blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if(m_SavedState.DepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if(m_SavedState.CullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if(m_SavedState.ScissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

void CGlassRenderer::CaptureScreen(int ScreenW, int ScreenH)
{
	if(!m_Initialized || ScreenW <= 0 || ScreenH <= 0)
		return;

	bool SizeChanged = (ScreenW != m_ScreenW || ScreenH != m_ScreenH);
	m_ScreenW = ScreenW;
	m_ScreenH = ScreenH;

	m_ScreenCapture.Resize(ScreenW, ScreenH);

	if(SizeChanged)
	{
		for(int i = 0; i < m_BlurLevels; i++)
		{
			int Mw = std::max(1, ScreenW >> (i + 1));
			int Mh = std::max(1, ScreenH >> (i + 1));
			m_BlurMips[i].Resize(Mw, Mh);
		}
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, m_ScreenCapture.GetTexture());
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, ScreenW, ScreenH);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void CGlassRenderer::GenerateBlur(int Iterations)
{
	if(!m_Initialized || !m_ScreenCapture.IsValid())
		return;

	m_BlurLevels = std::clamp(Iterations, 1, MAX_BLUR_MIPS);
	SaveGLState();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glActiveTexture(GL_TEXTURE0);

	m_DownsampleShader.Bind();
	m_DownsampleShader.SetInt("uTexture", 0);

	glBindTexture(GL_TEXTURE_2D, m_ScreenCapture.GetTexture());
	m_DownsampleShader.SetVec2("uHalfTexel",
		0.5f / (float)m_ScreenCapture.GetWidth(),
		0.5f / (float)m_ScreenCapture.GetHeight());
	m_BlurMips[0].Bind();
	DrawFullscreenQuad();

	for(int i = 1; i < m_BlurLevels; i++)
	{
		glBindTexture(GL_TEXTURE_2D, m_BlurMips[i - 1].GetTexture());
		m_DownsampleShader.SetVec2("uHalfTexel",
			0.5f / (float)m_BlurMips[i - 1].GetWidth(),
			0.5f / (float)m_BlurMips[i - 1].GetHeight());
		m_BlurMips[i].Bind();
		DrawFullscreenQuad();
	}

	m_UpsampleShader.Bind();
	m_UpsampleShader.SetInt("uTexture", 0);

	for(int i = m_BlurLevels - 2; i >= 0; i--)
	{
		glBindTexture(GL_TEXTURE_2D, m_BlurMips[i + 1].GetTexture());
		m_UpsampleShader.SetVec2("uHalfTexel",
			0.5f / (float)m_BlurMips[i + 1].GetWidth(),
			0.5f / (float)m_BlurMips[i + 1].GetHeight());
		m_BlurMips[i].Bind();
		DrawFullscreenQuad();
	}

	glBindTexture(GL_TEXTURE_2D, m_BlurMips[0].GetTexture());
	m_UpsampleShader.SetVec2("uHalfTexel",
		0.5f / (float)m_BlurMips[0].GetWidth(),
		0.5f / (float)m_BlurMips[0].GetHeight());
	m_ScreenCapture.Bind();
	DrawFullscreenQuad();

	CShaderProgram::Unbind();
	glBindTexture(GL_TEXTURE_2D, 0);
	RestoreGLState();
}

GLuint CGlassRenderer::GetBlurredTexture() const
{
	return m_ScreenCapture.GetTexture();
}

void CGlassRenderer::AddGlassRect(ImDrawList *pDraw, ImVec2 Min, ImVec2 Max,
	ImVec4 TintColor, float Rounding, float BorderAlpha, float BlurAlpha)
{
	if(!pDraw || !IsReady() || m_ScreenW <= 0 || m_ScreenH <= 0)
	{
		pDraw->AddRectFilled(Min, Max,
			ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.07f, 0.95f)), Rounding);
		return;
	}

	float Sw = (float)m_ScreenW;
	float Sh = (float)m_ScreenH;

	ImVec2 UvMin(Min.x / Sw, 1.0f - Min.y / Sh);
	ImVec2 UvMax(Max.x / Sw, 1.0f - Max.y / Sh);

	ImTextureID BlurTex = (ImTextureID)(intptr_t)GetBlurredTexture();
	ImU32 BlurCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, BlurAlpha));

	pDraw->AddImageRounded(BlurTex, Min, Max, UvMin, UvMax, BlurCol, Rounding);

	pDraw->AddRectFilled(Min, Max,
		ImGui::ColorConvertFloat4ToU32(TintColor), Rounding);

	if(BorderAlpha > 0.005f)
	{
		pDraw->AddRect(Min, Max,
			ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, BorderAlpha)),
			Rounding, 0, 0.5f);

		pDraw->AddLine(
			ImVec2(Min.x + Rounding, Min.y + 0.5f),
			ImVec2(Max.x - Rounding, Min.y + 0.5f),
			ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, BorderAlpha * 0.5f)),
			0.5f);
	}
}