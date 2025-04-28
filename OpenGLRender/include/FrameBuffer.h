#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Texture.h"
#include "EnumsOpenGL.h"

namespace OpenGL {

struct FrameBufferAttachment{
	std::shared_ptr<Texture> tex = nullptr;
	uint32_t layer = 0; //用于cubemap,表示不同的面
	uint32_t level = 0; //mipmap层级
};

class FrameBuffer {
public:
	explicit FrameBuffer(bool offscreen) : offscreen_(offscreen) {
		GL_CHECK(glGenBuffers(1, &fbo_));
	}
	
	int getId() const {
		return static_cast<int>(fbo_);
	}

	bool isVaild() {
		//基础检查
		if (!fbo_) {
			return false;
		}
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));

		//检查完整性
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			LOGE("glCheckFramebufferStatus: %x", status);
			return false;
		}

		return true;
	}

	void setColorAttachment(std::shared_ptr<Texture>& color, int level) {
		if (color == colorAttachment_.tex && level == colorAttachment_.level) {
			return;
		}

		colorAttachment_.tex = color;
		colorAttachment_.layer = 0;
		colorAttachment_.level = level;
		colorReady_ = true;

		//绑定纹理附件
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER,
										GL_COLOR_ATTACHMENT0,
										color->type,
										color->getId(),
										level));
	}

	void setColorAttachment(std::shared_ptr<Texture>& color, CubeMapFace face, int level) {
		if (color == colorAttachment_.tex && face == colorAttachment_.layer && level == colorAttachment_.level) {
			return;
		}

		FrameBuffer::setColorAttachment(color, face, level);
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER,
										GL_COLOR_ATTACHMENT0,
										cvtCubeFace(face),
										color->getId(),
										level));
	}

	void setDepthAttachment(std::shared_ptr<Texture>& depth) {
		if (depth == depthAttachment_.tex) {
			return;
		}

		FrameBuffer::setDepthAttachment(depth);
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
		GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER,
										GL_DEPTH_ATTACHMENT,
										depth->type,
										depth->getId(),
										0));
	}

	void bind() const {
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo_));
	}


private:
	bool offscreen_ = false;
	bool colorReady_ = false;
	bool depthReady_ = false;

	FrameBufferAttachment colorAttachment_{};
	FrameBufferAttachment depthAttachment_{};

	GLuint fbo_ = 0;
};

}


#endif
