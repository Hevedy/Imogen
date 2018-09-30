// https://github.com/CedricGuillemet/Imogen
//
// The MIT License(MIT)
// 
// Copyright(c) 2018 Cedric Guillemet
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <GL/gl3w.h>    // Initialize with gl3wInit()
#include "Evaluation.h"
#include <vector>
#include <algorithm>
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <fstream>
#include <streambuf>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"
#include "tiny_obj_loader.h"
#include "imgui.h"
#include "imgui_internal.h"

static const int SemPosition = 0;
static const int SemNormal = 1;
static const int SemUV = 2;
static const unsigned int wrap[] = { GL_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT };
static const unsigned int filter[] = { GL_LINEAR, GL_NEAREST };

inline void TexParam(TextureID MinFilter, TextureID MagFilter, TextureID WrapS, TextureID WrapT, TextureID texMode)
{
	glTexParameteri(texMode, GL_TEXTURE_MIN_FILTER, MinFilter);
	glTexParameteri(texMode, GL_TEXTURE_MAG_FILTER, MagFilter);
	glTexParameteri(texMode, GL_TEXTURE_WRAP_S, WrapS);
	glTexParameteri(texMode, GL_TEXTURE_WRAP_T, WrapT);
}

void RenderTarget::bindAsTarget() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, mWidth, mHeight);
}

void RenderTarget::destroy()
{
	if (mGLTexID)
		glDeleteTextures(1, &mGLTexID);
	if (fbo)
		glDeleteFramebuffers(1, &fbo);
	if (depthbuffer)
		glDeleteRenderbuffers(1, &depthbuffer);
	fbo = depthbuffer = 0;
	mWidth = mHeight = 0;
	mGLTexID = 0;
}

void RenderTarget::clear()
{
	glClear(GL_COLOR_BUFFER_BIT | (depthbuffer ? GL_DEPTH_BUFFER_BIT : 0));
}

void RenderTarget::initBuffer(int width, int height, bool hasZBuffer)
{
	if ((width == mWidth) && (mHeight == height) && (!(hasZBuffer ^ (depthbuffer != 0))))
		return;
	destroy();

	mWidth = width;
	mHeight = height;

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// diffuse
	glGenTextures(1, &mGLTexID);
	glBindTexture(GL_TEXTURE_2D, mGLTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	TexParam(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_TEXTURE_2D);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mGLTexID, 0);
	/*
	if (hasZBuffer)
	{
	// Z
	glGenTextures(1, &mGLTexID2);
	glBindTexture(GL_TEXTURE_2D, mGLTexID2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	TexParam(GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_TEXTURE_2D);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mGLTexID2, 0);
	}
	*/
	static const GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(sizeof(DrawBuffers) / sizeof(GLenum), DrawBuffers);

	checkFBO();
}

void RenderTarget::checkFBO()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	switch (status)
	{
	case GL_FRAMEBUFFER_COMPLETE:
		//Log("Framebuffer complete.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		Log("[ERROR] Framebuffer incomplete: Attachment is NOT complete.");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		Log("[ERROR] Framebuffer incomplete: No image is attached to FBO.");
		break;
		/*
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
		Log("[ERROR] Framebuffer incomplete: Attached images have different dimensions.");
		break;

		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
		Log("[ERROR] Framebuffer incomplete: Color attached images have different internal formats.");
		break;
		*/
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		Log("[ERROR] Framebuffer incomplete: Draw buffer.\n");
		break;

	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		Log("[ERROR] Framebuffer incomplete: Read buffer.\n");
		break;

	case GL_FRAMEBUFFER_UNSUPPORTED:
		Log("[ERROR] Unsupported by FBO implementation.\n");
		break;

	default:
		Log("[ERROR] Unknow error.\n");
		break;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

class FullScreenTriangle
{
public:
	FullScreenTriangle() : mGLFullScreenVertexArrayName(-1)
	{
	}
	~FullScreenTriangle()
	{
	}
	void Init();
	void Render();
protected:
	TextureID mGLFullScreenVertexArrayName;
};

void FullScreenTriangle::Init()
{
	TextureID fsVA;

	float fsVts[] = { 0.f,0.f, 2.f,0.f, 0.f,2.f };
	glGenBuffers(1, &fsVA);
	glBindBuffer(GL_ARRAY_BUFFER, fsVA);
	glBufferData(GL_ARRAY_BUFFER, 3 * sizeof(float) * 2, fsVts, GL_STATIC_DRAW);

	glGenVertexArrays(1, &mGLFullScreenVertexArrayName);
	glBindVertexArray(mGLFullScreenVertexArrayName);
	glBindBuffer(GL_ARRAY_BUFFER, fsVA);
	glVertexAttribPointer(SemUV, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(SemUV);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void FullScreenTriangle::Render()
{
	glBindVertexArray(mGLFullScreenVertexArrayName);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

unsigned int LoadShader(const std::string &shaderString, const char *fileName)
{
	TextureID programObject = glCreateProgram();
	if (programObject == 0)
		return 0;

	GLint compiled;
	static const char *shaderTypeStrings[] = { "\n#version 430 core\n#define VERTEX_SHADER\n", "\n#version 430 core\n#define FRAGMENT_SHADER\n" };
	static const char *passShader = {
	"layout(location = 0)in vec4 inPosition;\n"
	"layout(location = 1)in vec3 inNormal;\n"
	//"layout(location = 2)in vec2 inUV;\n"
	"layout(std140) uniform PassUniform\n"
	"{\n"
	"	mat4 ViewProjection;\n"
	"};\n"
	};

	TextureID shaderTypes[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	TextureID compiledShader[2];

	for (int i = 0; i<2; i++)
	{
		// Create the shader object
		int shader = glCreateShader(shaderTypes[i]);

		if (shader == 0)
			return false;

		int stringsCount = 3;
		const char ** strings = (const char**)malloc(sizeof(char*) * stringsCount); //new const char*[stringsCount];
		int * stringLength = (int*)malloc(sizeof(int) * stringsCount); //new int[stringsCount];
		strings[0] = shaderTypeStrings[i];
		stringLength[0] = int(strlen(shaderTypeStrings[i]));
		strings[1] = passShader;
		stringLength[1] = int(strlen(passShader));
		strings[stringsCount - 1] = shaderString.c_str();
		stringLength[stringsCount - 1] = int(shaderString.length());

		// Load and compile the shader source
		glShaderSource(shader, stringsCount, strings, stringLength);
		glCompileShader(shader);


		free(stringLength);
		free(strings);

		// Check the compile status
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled == 0)
		{
			GLint info_len = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
			if (info_len > 1)
			{
				char* info_log = (char*)malloc(sizeof(char) * info_len);
				glGetShaderInfoLog(shader, info_len, NULL, info_log);
				Log("Error compiling shader: %s \n", fileName);
				Log(info_log);
				Log("\n");
				free(info_log);
			}
			glDeleteShader(shader);
			return 0;
		}
		compiledShader[i] = shader;
	}



	GLint linked;

	for (int i = 0; i<2; i++)
		glAttachShader(programObject, compiledShader[i]);


	// Link the program
	glLinkProgram(programObject);

	glBindAttribLocation(programObject, SemUV, "inUV");
	glBindAttribLocation(programObject, SemPosition, "inPosition");
	glBindAttribLocation(programObject, SemNormal, "inNormal");

	// Check the link status
	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
	if (linked == 0)
	{
		GLint info_len = 0;
		glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &info_len);
		if (info_len > 1)
		{
			char* info_log = (char*)malloc(sizeof(char) * info_len);
			glGetProgramInfoLog(programObject, info_len, NULL, info_log);
			Log("Error linking program:\n");
			Log(info_log);
			free(info_log);
		}
		glDeleteProgram(programObject);
		return 0;
	}

	// Delete these here because they are attached to the program object.
	for (int i = 0; i<2; i++)
		glDeleteShader(compiledShader[i]);

	// attributes
	return programObject;
}

FullScreenTriangle mFSQuad;

void Evaluation::APIInit()
{
	mFSQuad.Init();
}

static void libtccErrorFunc(void *opaque, const char *msg)
{
	Log(msg);
	Log("\n");
}

extern Evaluation gEvaluation;

int Evaluation::ReadImage(const char *filename, Image *image)
{
	unsigned char *data = stbi_load(filename, &image->width, &image->height, &image->components, 0);
	if (!data)
		return EVAL_ERR;
	image->bits = data;
	return EVAL_OK;
}



static std::string GetFilePathExtension(const std::string &FileName) {
	if (FileName.find_last_of(".") != std::string::npos)
		return FileName.substr(FileName.find_last_of(".") + 1);
	return "";
}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

std::map<std::string, MeshOGL> mOGLMeshes;

int Evaluation::ReadMesh(char *filename, Mesh *mesh)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF gltf_ctx;
	std::string err;
	std::string warn;
	std::string ext = GetFilePathExtension(filename);

	bool ret = false;
	if (ext.compare("glb") == 0) {
		ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn, filename);
	}
	else {
		ret = gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, filename);
	}

	if (!warn.empty()) {
		Log("Warn: %s\n", warn.c_str());
	}


	if (!err.empty()) {
		Log("Err: %s\n", err.c_str());
	}

	if (!ret) {
		Log("Failed to parse glTF\n");
		return EVAL_ERR;
	}

	MeshOGL oglMesh;
	/*
	glBindBuffer(GL_ARRAY_BUFFER, fsVA);
	glVertexAttribPointer(SemUV0, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(SemUV0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	*/

	std::vector<unsigned int> bufs(model.bufferViews.size());
	{
		for (size_t i = 0; i < model.bufferViews.size(); i++) 
		{
			const tinygltf::BufferView &bufferView = model.bufferViews[i];
			if (bufferView.target == 0) 
			{
				//std::cout << "WARN: bufferView.target is zero" << std::endl;
				continue;  // Unsupported bufferView.
			}

			const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
			unsigned int state;
			glGenBuffers(1, &state);
			glBindBuffer(bufferView.target, state);
			//std::cout << "buffer.size= " << buffer.data.size()
			//	<< ", byteOffset = " << bufferView.byteOffset << std::endl;
			glBufferData(bufferView.target, bufferView.byteLength,
				&buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
			glBindBuffer(bufferView.target, 0);

			bufs[i] = state;
			//gBufferState[i] = state;
		}
	}
	oglMesh.mBufers = bufs;
	/*
	gGLProgramState.attribs["POSITION"] = vtloc;
	gGLProgramState.attribs["NORMAL"] = nrmloc;
	gGLProgramState.attribs["TEXCOORD_0"] = uvloc;
	*/

	for (auto& mesh : model.meshes)
	{
		for (size_t i = 0; i < mesh.primitives.size(); i++) 
		{
			const tinygltf::Primitive &primitive = mesh.primitives[i];

			if (primitive.indices < 0) return EVAL_ERR;

			// Assume TEXTURE_2D target for the texture object.
			// glBindTexture(GL_TEXTURE_2D, gMeshState[mesh.name].diffuseTex[i]);

			std::map<std::string, int>::const_iterator it(primitive.attributes.begin());
			std::map<std::string, int>::const_iterator itEnd(primitive.attributes.end());

			unsigned int mGLFullScreenVertexArrayName;

			glGenVertexArrays(1, &mGLFullScreenVertexArrayName);
			glBindVertexArray(mGLFullScreenVertexArrayName);


			for (; it != itEnd; it++) 
			{
				assert(it->second >= 0);
				const tinygltf::Accessor &accessor = model.accessors[it->second];
				glBindBuffer(GL_ARRAY_BUFFER, bufs[accessor.bufferView]);
				//CheckErrors("bind buffer");
				int size = 1;
				if (accessor.type == TINYGLTF_TYPE_SCALAR) {
					size = 1;
				}
				else if (accessor.type == TINYGLTF_TYPE_VEC2) {
					size = 2;
				}
				else if (accessor.type == TINYGLTF_TYPE_VEC3) {
					size = 3;
				}
				else if (accessor.type == TINYGLTF_TYPE_VEC4) {
					size = 4;
				}
				else {
					assert(0);
				}
				int state = -1;
				if (it->first.compare("POSITION") == 0)
					state = 0;
				if (it->first.compare("NORMAL") == 0)
					state = 1;
				if (it->first.compare("TEXCOORD_0") == 0)
					state = 2;

				// it->first would be "POSITION", "NORMAL", "TEXCOORD_0", ...
				if (state != -1) 
				{
					//if (gGLProgramState.attribs[it->first] >= 0) 
					{
						// Compute byteStride from Accessor + BufferView combination.
						int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
						assert(byteStride != -1);
						
						glVertexAttribPointer(state, size,
							accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE,
							byteStride,
							BUFFER_OFFSET(accessor.byteOffset));
						//CheckErrors("vertex attrib pointer");
						glEnableVertexAttribArray(state);
						//CheckErrors("enable vertex attrib array");
					}
				}
			}
			glBindVertexArray(0);

			const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];

			int mode = -1;
			if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
				mode = GL_TRIANGLES;
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
				mode = GL_TRIANGLE_STRIP;
			}
			else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) {
				mode = GL_TRIANGLE_FAN;
			}
			else if (primitive.mode == TINYGLTF_MODE_POINTS) {
				mode = GL_POINTS;
			}
			else if (primitive.mode == TINYGLTF_MODE_LINE) {
				mode = GL_LINES;
			}
			else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP) {
				mode = GL_LINE_LOOP;
			}
			else {
				assert(0);
			}
				
			oglMesh.mDCs.push_back({ bufs[indexAccessor.bufferView], mGLFullScreenVertexArrayName, mode, indexAccessor.count, indexAccessor.componentType,
				BUFFER_OFFSET(indexAccessor.byteOffset) });
		}
	}
	mOGLMeshes[filename] = oglMesh;
	mesh->meshIndex = 0;
	return EVAL_OK;
}

void RenderMesh(MeshOGL *mesh)
{
	for (auto& dc : mesh->mDCs)
	{
		glBindVertexArray(dc.mVAO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dc.mIndexArray);
		glDrawElements(dc.mode, GLsizei(dc.count), dc.componentType, dc.indices);
	}
	glBindVertexArray(0);
}



void Evaluation::MeshDrawCallBack(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
	// Backup GL state
	GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
	glActiveTexture(GL_TEXTURE0);
	GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
#ifdef GL_SAMPLER_BINDING
	GLint last_sampler; glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
#endif
	GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
#ifdef GL_POLYGON_MODE
	GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
	GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
	GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
	GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
	GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
	GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
	GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
	GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
	GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
	GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
	GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
	GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
	ImGuiIO& io = ImGui::GetIO();
	extern std::vector<ImRect> mCallbackRects;
	ImRect cbRect = mCallbackRects[int(cmd->UserCallbackData)];
	float h = cbRect.Max.y - cbRect.Min.y;
	float w = cbRect.Max.x - cbRect.Min.x;


	glViewport(int(cbRect.Min.x), int(io.DisplaySize.y - cbRect.Max.y), int(w), int(h));
	
	cbRect.Min.x = ImMax(cbRect.Min.x, cmd->ClipRect.x);

	
	glScissor(int(cbRect.Min.x), int(io.DisplaySize.y - cbRect.Max.y), int(cbRect.Max.x - cbRect.Min.x), int(cbRect.Max.y - cbRect.Min.y));
	glEnable(GL_SCISSOR_BOX);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	struct PassBuffer
	{
		float ViewProj[16];
	};
	PassBuffer passBuffer;
	static unsigned int meshDisplayShader = 0;
	static unsigned int parametersBuffer = 0;
	if (meshDisplayShader == 0)
	{
		std::string meshShader = { ""
"#ifdef VERTEX_SHADER\n"
"		void main()\n"
"		{\n"
"			mat4 transformViewProj = ViewProjection;\n"
"			gl_Position = transformViewProj * vec4(inPosition.xyz, 1.0);\n"
"		}\n"
"#endif\n"
"#ifdef FRAGMENT_SHADER\n"
"layout(location = 0) out vec4 outPixDiffuse;"
"		void main()\n"
"		{\n"
"			outPixDiffuse = vec4(1.0,0.0,1.0,1.0);\n"
"		}\n"
"#endif\n"
		};
		meshDisplayShader = LoadShader(meshShader, "shaderMesh");


		glGenBuffers(1, &parametersBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, parametersBuffer);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(passBuffer), &passBuffer, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, parametersBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

	}

	float perspectiveMat[16], viewMat[16];

	Perspective(53.f, w / h, 0.01f, 1000.f, perspectiveMat);
	float eye[3] = { 2.f, 2.f, 2.f };
	float tgt[3] = { 0.f, 0.f, 0.f };
	float up[3] = { 0.f, 1.f, 0.f };
	LookAt(eye, tgt, up, viewMat);
	FPU_MatrixF_x_MatrixF(viewMat, perspectiveMat, passBuffer.ViewProj);

	glUseProgram(meshDisplayShader);

	glBindBuffer(GL_UNIFORM_BUFFER, parametersBuffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(passBuffer), &passBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


	RenderMesh(&mOGLMeshes.begin()->second);

	// Restore modified GL state
	glUseProgram(last_program);
	glBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef GL_SAMPLER_BINDING
	glBindSampler(0, last_sampler);
#endif
	glActiveTexture(last_active_texture);
	glBindVertexArray(last_vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
	glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
	glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
	if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
#ifdef GL_POLYGON_MODE
	glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
#endif
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
	glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);

}

int Evaluation::SetEvaluationMesh(int target, Mesh *mesh)
{
	return EVAL_OK;
}

int Evaluation::WriteImage(const char *filename, Image *image, int format, int quality)
{
	switch (format)
	{
	case 0:
		if (!stbi_write_jpg(filename, image->width, image->height, image->components, image->bits, quality))
			return EVAL_ERR;
		break;
	case 1:
		if (!stbi_write_png(filename, image->width, image->height, image->components, image->bits, image->width * image->components))
			return EVAL_ERR;
		break;
	case 2:
		if (!stbi_write_tga(filename, image->width, image->height, image->components, image->bits))
			return EVAL_ERR;
		break;
	case 3:
		if (!stbi_write_bmp(filename, image->width, image->height, image->components, image->bits))
			return EVAL_ERR;
		break;
	case 4:
		//if (stbi_write_hdr(filename, image->width, image->height, image->components, image->bits))
			return EVAL_ERR;
		break;
	}
	return EVAL_OK;
}

int Evaluation::GetEvaluationImage(int target, Image *image)
{
	if (target == -1 || target >= gEvaluation.mEvaluations.size())
		return EVAL_ERR;

	Evaluation::EvaluationStage &evaluation = gEvaluation.mEvaluations[target];
	//evaluation.mTarget.initBuffer(image->width, image->height, false);
	RenderTarget& tgt = evaluation.mTarget;
	image->components = 4;
	image->width = tgt.mWidth;
	image->height = tgt.mHeight;
	image->bits = malloc(tgt.mWidth * tgt.mHeight * 4);
	glBindTexture(GL_TEXTURE_2D, evaluation.mTarget.mGLTexID);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->bits);

	return EVAL_OK;
}

int Evaluation::SetEvaluationImage(int target, Image *image)
{
	Evaluation::EvaluationStage &evaluation = gEvaluation.mEvaluations[target];
	evaluation.mTarget.initBuffer(image->width, image->height, false);
	glBindTexture(GL_TEXTURE_2D, evaluation.mTarget.mGLTexID);
	switch (image->components)
	{
	case 3:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->bits);
		break;
	case 4:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->bits);
		break;
	default:
		Log("SetEvaluationImage: unsupported component format.\n");
		return EVAL_ERR;
	}

	return EVAL_OK;
}

int Evaluation::AllocateImage(Image *image)
{
	return EVAL_OK;
}

int Evaluation::FreeImage(Image *image)
{
	free(image->bits);
	return EVAL_OK;
}

int Evaluation::SetThumbnailImage(Image *image)
{
	int outlen;
	unsigned char *bits = stbi_write_png_to_mem((unsigned char*)image->bits, image->width * image->components, image->width, image->height, image->components, &outlen);
	if (!bits)
		return EVAL_ERR;
	std::vector<unsigned char> pngImage(outlen);
	memcpy(pngImage.data(), bits, outlen);

	extern Library library;
	extern Imogen imogen;

	int materialIndex = imogen.GetCurrentMaterialIndex();
	Material & material = library.mMaterials[materialIndex];
	material.mThumbnail = pngImage;
	material.mThumbnailTextureId = 0;
	return EVAL_OK;
}

void Evaluation::Evaluate(int target, int width, int height)
{
	gEvaluation.RunEvaluation(target, width, height);
}

void Evaluation::RunEvaluation(int target, int width, int height)
{
	if (mEvaluationOrderList.empty())
		return;
	if (!mDirtyCount)
		return;

	mTransientTextureMaxCount = 0;
	std::vector<int> evaluationUseCount(mEvaluationOrderList.size(), 0); // use count of texture by others
	std::vector<TransientTarget*> evaluationTransientTargets(mEvaluationOrderList.size(), NULL);
	for (auto& evaluation : mEvaluations)
	{
		for (int targetIndex : evaluation.mInput.mInputs)
			if (targetIndex != -1)
				evaluationUseCount[targetIndex]++;
	}
	if (evaluationUseCount[target] < 1)
		evaluationUseCount[target] = 1;

	// todo : revert pass. dec use count for parent nodes whose children have 0 use count

	for (size_t i = 0; i < mEvaluationOrderList.size(); i++)
	{
		size_t nodeIndex = mEvaluationOrderList[i];
		if (!evaluationUseCount[nodeIndex])
			continue;
		EvaluationStage& evaluation = mEvaluations[nodeIndex];
		TransientTarget* transientTarget = GetTransientTarget(width, height, evaluationUseCount[nodeIndex]);
		evaluationTransientTargets[nodeIndex] = transientTarget;
		const Input& input = evaluation.mInput;

		switch (evaluation.mEvaluationType)
		{
		case 0: // GLSL
			if (nodeIndex == target)
			{
				evaluation.mTarget.initBuffer(width, height, false);
				EvaluateGLSL(evaluation, evaluation.mTarget, &evaluationTransientTargets);
			}
			else
			{
				EvaluateGLSL(evaluation, transientTarget->mTarget, &evaluationTransientTargets);
			}

			for (size_t inputIndex = 0; inputIndex < 8; inputIndex++)
			{
				int targetIndex = input.mInputs[inputIndex];
				if (targetIndex >= 0)
					LoseTransientTarget(evaluationTransientTargets[targetIndex]);
			}
			break;
		case 1: // C
			EvaluateC(evaluation, nodeIndex);
			break;
		}


		if (nodeIndex == target)
			break;
	}


	for (auto& evaluation : mEvaluations)
	{
		if (evaluation.mbDirty)
		{
			evaluation.mbDirty = false;
			evaluation.mbForceEval = false;
			mDirtyCount--;
		}
	}

	// free transient textures
	for (auto transientTarget : mFreeTargets)
	{
		assert(transientTarget->mUseCount <= 0);
		transientTarget->mTarget.destroy();
	}
	FinishEvaluation();
}

static const EValuationFunction evaluationFunctions[] = {
	{ "Log", (void*)Log },
	{ "ReadImage", (void*)Evaluation::ReadImage },
	{ "WriteImage", (void*)Evaluation::WriteImage },
	{ "GetEvaluationImage", (void*)Evaluation::GetEvaluationImage },
	{ "SetEvaluationImage", (void*)Evaluation::SetEvaluationImage },
	{ "AllocateImage", (void*)Evaluation::AllocateImage },
	{ "FreeImage", (void*)Evaluation::FreeImage },
	{ "SetThumbnailImage", (void*)Evaluation::SetThumbnailImage },
	{ "Evaluate", (void*)Evaluation::Evaluate},
	{ "ReadMesh", (void*)Evaluation::ReadMesh },
	{ "SetEvaluationMesh", (void*)Evaluation::SetEvaluationMesh }
};

static const char* samplerName[] = { "Sampler0", "Sampler1", "Sampler2", "Sampler3", "Sampler4", "Sampler5", "Sampler6", "Sampler7" };

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to)
{
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}


void Evaluation::ClearEvaluators()
{
	// clear
	for (auto& program : mEvaluatorPerNodeType)
	{
		if (program.mGLSLProgram)
			glDeleteProgram(program.mGLSLProgram);
		if (program.mMem)
			free(program.mMem);
	}
}

void Evaluation::SetEvaluators(const std::vector<EvaluatorFile>& evaluatorfilenames)
{
	ClearEvaluators();

	mEvaluatorPerNodeType.clear();
	mEvaluatorPerNodeType.resize(evaluatorfilenames.size(), Evaluator());

	// GLSL
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_GLSL)
			continue;
		const std::string filename = file.mFilename;

		std::ifstream t(file.mDirectory + filename);
		if (t.good())
		{
			std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
			if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
				mEvaluatorScripts[filename] = EvaluatorScript(str);
			else
				mEvaluatorScripts[filename].mText = str;
		}
	}

	std::string baseShader = mEvaluatorScripts["Shader.glsl"].mText;
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_GLSL)
			continue;
		const std::string filename = file.mFilename;

		if (filename == "Shader.glsl")
			continue;

		EvaluatorScript& shader = mEvaluatorScripts[filename];
		std::string shaderText = ReplaceAll(baseShader, "__NODE__", shader.mText);
		std::string nodeName = ReplaceAll(filename, ".glsl", "");
		shaderText = ReplaceAll(shaderText, "__FUNCTION__", nodeName + "()");

		unsigned int program = LoadShader(shaderText, filename.c_str());
		unsigned int parameterBlockIndex = glGetUniformBlockIndex(program, (nodeName + "Block").c_str());
		glUniformBlockBinding(program, parameterBlockIndex, 1);
		shader.mProgram = program;
		if (shader.mNodeType != -1)
			mEvaluatorPerNodeType[shader.mNodeType].mGLSLProgram = program;
	}

	// C
	for (auto& file : evaluatorfilenames)
	{
		if (file.mEvaluatorType != EVALUATOR_C)
			continue;
		const std::string filename = file.mFilename;

		std::ifstream t(file.mDirectory + filename);
		if (!t.good())
		{
			Log("%s - Unable to load file.\n", filename.c_str());
			continue;
		}
		std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
		if (mEvaluatorScripts.find(filename) == mEvaluatorScripts.end())
			mEvaluatorScripts[filename] = EvaluatorScript(str);
		else
			mEvaluatorScripts[filename].mText = str;

		EvaluatorScript& program = mEvaluatorScripts[filename];
		TCCState *s = tcc_new();

		int *noLib = (int*)s;
		noLib[2] = 1; // no stdlib

		tcc_set_error_func(s, 0, libtccErrorFunc);
		tcc_add_include_path(s, "C\\");
		tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

		if (tcc_compile_string(s, program.mText.c_str()) != 0)
		{
			Log("%s - Compilation error!\n", filename.c_str());
			continue;
		}

		for (auto& evaluationFunction : evaluationFunctions)
			tcc_add_symbol(s, evaluationFunction.szFunctionName, evaluationFunction.function);

		int size = tcc_relocate(s, NULL);
		if (size == -1)
		{
			Log("%s - Libtcc unable to relocate program!\n", filename.c_str());
			continue;
		}
		program.mMem = malloc(size);
		tcc_relocate(s, program.mMem);

		*(void**)(&program.mCFunction) = tcc_get_symbol(s, "main");
		if (!program.mCFunction)
		{
			Log("%s - No main function!\n", filename.c_str());
		}
		tcc_delete(s);

		if (program.mNodeType != -1)
		{
			mEvaluatorPerNodeType[program.mNodeType].mCFunction = program.mCFunction;
			mEvaluatorPerNodeType[program.mNodeType].mMem = program.mMem;
		}

	}
}

void Evaluation::BindGLSLParameters(EvaluationStage& stage)
{
	if (!stage.mParametersBuffer)
	{
		glGenBuffers(1, &stage.mParametersBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, stage.mParametersBuffer);

		glBufferData(GL_UNIFORM_BUFFER, stage.mParametersSize, stage.mParameters, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, stage.mParametersBuffer);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	else
	{
		glBindBuffer(GL_UNIFORM_BUFFER, stage.mParametersBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, stage.mParametersSize, stage.mParameters);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
}

void Evaluation::EvaluateGLSL(const EvaluationStage& evaluation, const RenderTarget &tg, std::vector<TransientTarget*> *evaluationTransientTargets)
{
	const Input& input = evaluation.mInput;
	tg.bindAsTarget();
	unsigned int program = mEvaluatorPerNodeType[evaluation.mNodeType].mGLSLProgram;

	glUseProgram(program);

	glBindBufferBase(GL_UNIFORM_BUFFER, 1, evaluation.mParametersBuffer);

	int samplerIndex = 0;
	for (size_t inputIndex = 0; inputIndex < 8; inputIndex++)
	{
		unsigned int parameter = glGetUniformLocation(program, samplerName[inputIndex]);
		if (parameter == 0xFFFFFFFF)
			continue;
		glUniform1i(parameter, samplerIndex);
		glActiveTexture(GL_TEXTURE0 + samplerIndex);
		samplerIndex++;
		int targetIndex = input.mInputs[inputIndex];
		if (targetIndex < 0)
		{
			glBindTexture(GL_TEXTURE_2D, 0);
			continue;
		}
		//assert(!mEvaluations[targetIndex].mbDirty);
		if (evaluationTransientTargets)
			glBindTexture(GL_TEXTURE_2D, evaluationTransientTargets->at(targetIndex)->mTarget.mGLTexID);
		else
			glBindTexture(GL_TEXTURE_2D, mEvaluations[targetIndex].mTarget.mGLTexID);

		const InputSampler& inputSampler = evaluation.mInputSamplers[inputIndex];
		TexParam(filter[inputSampler.mFilterMin], filter[inputSampler.mFilterMag], wrap[inputSampler.mWrapU], wrap[inputSampler.mWrapV], GL_TEXTURE_2D);
	}
	//
	mFSQuad.Render();
}

void Evaluation::EvaluateC(const EvaluationStage& evaluation, size_t index)
{
	const Input& input = evaluation.mInput;
	struct EvaluationInfo
	{
		int targetIndex;
		int inputIndices[8];
		int forcedDirty;
	};
	EvaluationInfo evaluationInfo;
	evaluationInfo.targetIndex = int(index);
	memcpy(evaluationInfo.inputIndices, input.mInputs, sizeof(evaluationInfo.inputIndices));
	evaluationInfo.forcedDirty = evaluation.mbForceEval ? 1 : 0;
	mEvaluatorPerNodeType[evaluation.mNodeType].mCFunction(evaluation.mParameters, &evaluationInfo);
}

void Evaluation::EvaluationStage::Clear()
{
	if (mEvaluationType == EVALUATOR_GLSL)
		glDeleteBuffers(1, &mParametersBuffer);
	mTarget.destroy();
}

void Evaluation::FinishEvaluation()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}

unsigned int Evaluation::UploadImage(Image *image)
{
	unsigned int textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	switch (image->components)
	{
	case 3:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->bits);
		break;
	case 4:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->bits);
		break;
	default:
		Log("Texture cache : unsupported component format.\n");
		return EVAL_ERR;
	}
	TexParam(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_TEXTURE_2D);
	return textureId;
}

unsigned int Evaluation::GetTexture(const std::string& filename)
{
	auto iter = mSynchronousTextureCache.find(filename);
	if (iter != mSynchronousTextureCache.end())
		return iter->second;

	Image image;
	unsigned int textureId = 0;
	if (ReadImage(filename.c_str(), &image) == EVAL_OK)
	{
		textureId = UploadImage(&image);
	}

	mSynchronousTextureCache[filename] = textureId;
	return textureId;
}

