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

#pragma once
#include <string>
#include <vector>
#include <map>
#include "Library.h"
#include "libtcc/libtcc.h"
#include "Imogen.h"
#include <string.h>
#include <stdio.h>

extern int Log(const char *szFormat, ...);

typedef unsigned int TextureID;
struct ImDrawList;
struct ImDrawCmd;

enum BlendOp
{
	ZERO,
	ONE,
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	DST_COLOR,
	ONE_MINUS_DST_COLOR,
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DST_ALPHA,
	ONE_MINUS_DST_ALPHA,
	CONSTANT_COLOR,
	ONE_MINUS_CONSTANT_COLOR,
	CONSTANT_ALPHA,
	ONE_MINUS_CONSTANT_ALPHA,
	SRC_ALPHA_SATURATE,
	BLEND_LAST
};

enum EvaluationStatus
{
	EVAL_OK,
	EVAL_ERR,
};

struct EvaluationInfo
{
	int targetIndex;
	int forcedDirty;
	int uiPass;
	int padding;
	float mouse[4];
	int inputIndices[8];
};

typedef struct Image_t
{
	int width, height;
	int components;
	void *bits;
} Image;

class RenderTarget
{

public:
	RenderTarget() : mGLTexID(0)
	{
		fbo = 0;
		depthbuffer = 0;
		mWidth = mHeight = 0;
		mRefCount = 0;
	}

	void initBuffer(int width, int height, bool hasZBuffer);
	void bindAsTarget() const;

	TextureID txDepth;
	unsigned int mGLTexID;
	int mWidth, mHeight;
	TextureID fbo;
	TextureID depthbuffer;
	int mRefCount;
	void destroy();

	void checkFBO();
};


// simple API
struct Evaluation
{
	Evaluation();

	void Init();
	void Finish();

	void SetEvaluators(const std::vector<EvaluatorFile>& evaluatorfilenames);
	std::string GetEvaluator(const std::string& filename);

	size_t AddEvaluation(size_t nodeType, const std::string& nodeName);

	void DelEvaluationTarget(size_t target);
	unsigned int GetEvaluationTexture(size_t target);
	void SetEvaluationParameters(size_t target, void *parameters, size_t parametersSize);
	void PerformEvaluationForNode(size_t index, int width, int height, bool force, EvaluationInfo& evaluationInfo);
	void SetEvaluationSampler(size_t target, const std::vector<InputSampler>& inputSamplers);
	void AddEvaluationInput(size_t target, int slot, int source);
	void DelEvaluationInput(size_t target, int slot);
	void RunEvaluation(int width, int height, bool forceEvaluation);
	void SetEvaluationOrder(const std::vector<size_t> nodeOrderList);
	void SetTargetDirty(size_t target);
	void SetMouse(int target, float rx, float ry, bool lButDown, bool rButDown);
	void Clear();

	// API
	static int ReadImage(const char *filename, Image *image);
	static int ReadImageMem(unsigned char *data, size_t dataSize, Image *image);
	static int WriteImage(const char *filename, Image *image, int format, int quality);
	static int GetEvaluationImage(int target, Image *image);
	static int SetEvaluationImage(int target, Image *image);
	static int SetThumbnailImage(Image *image);
	static int AllocateImage(Image *image);
	static int FreeImage(Image *image);
	static unsigned int UploadImage(Image *image);
	static int Evaluate(int target, int width, int height, Image *image);
	static void SetBlendingMode(int target, int blendSrc, int blendDst);
	static int EncodePng(Image *image, std::vector<unsigned char> &pngImage);
	static int SetNodeImage(int target, Image *image);
	static int GetEvaluationSize(int target, int *imageWidth, int *imageHeight);
	static int SetEvaluationSize(int target, int imageWidth, int imageHeight);

	static void NodeUICallBack(const ImDrawList* parent_list, const ImDrawCmd* cmd);
	// synchronous texture cache
	// use for simple textures(stock) or to replace with a more efficient one
	unsigned int GetTexture(const std::string& filename);
protected:
	void APIInit();
	std::map<std::string, unsigned int> mSynchronousTextureCache;

	int mEvaluationMode;
	//int mAllocatedTargets;
	unsigned int equiRectTexture;
	int mDirtyCount;

	void ClearEvaluators();
	struct Evaluator
	{
		Evaluator() : mGLSLProgram(0), mCFunction(0), mMem(0) {}
		unsigned int mGLSLProgram;
		int(*mCFunction)(void *parameters, void *evaluationInfo);
		void *mMem;
	};

	std::vector<Evaluator> mEvaluatorPerNodeType;

	struct EvaluatorScript
	{
		EvaluatorScript() : mProgram(0), mCFunction(0), mMem(0), mNodeType(-1) {}
		EvaluatorScript(const std::string & text) : mText(text), mProgram(0), mCFunction(0), mMem(0), mNodeType(-1) {}
		std::string mText;
		unsigned int mProgram;
		int(*mCFunction)(void *parameters, void *evaluationInfo);
		void *mMem;
		int mNodeType;
	};

	std::map<std::string, EvaluatorScript> mEvaluatorScripts;

	struct Input
	{
		Input()
		{
			memset(mInputs, -1, sizeof(int) * 8);
		}
		int mInputs[8];
	};

	enum EvaluationMask
	{
		EvaluationC    = 1 << 0,
		EvaluationGLSL = 1 << 1,
	};
	struct EvaluationStage
	{
		RenderTarget *mTarget;
		size_t mNodeType;
		unsigned int mParametersBuffer;
		void *mParameters;
		size_t mParametersSize;
		Input mInput;
		std::vector<InputSampler> mInputSamplers;
		bool mbDirty;
		bool mbForceEval;
		int mEvaluationMask; // see EvaluationMask
		int mUseCountByOthers;
		int mBlendingSrc;
		int mBlendingDst;
		// mouse
		float mRx;
		float mRy;
		bool mLButDown;
		bool mRButDown;
		void Clear();
	};

	unsigned int mEvaluationStateGLSLBuffer;
	std::vector<EvaluationStage> mEvaluationStages;
	std::vector<size_t> mEvaluationOrderList;

	void SetMouseInfos(EvaluationInfo &evaluationInfo, EvaluationStage &evaluationStage) const;
	void BindGLSLParameters(EvaluationStage& evaluationStage);
	void EvaluateGLSL(EvaluationStage& evaluationStage, EvaluationInfo& evaluationInfo);
	void EvaluateC(EvaluationStage& evaluationStage, size_t index, EvaluationInfo& evaluationInfo);
	void FinishEvaluation();

	std::vector<RenderTarget*> mAllocatedRenderTargets;
	void SetEvaluationMemoryMode(int mode);
	void RecurseGetUse(size_t target, std::vector<size_t>& usedNodes);

};
