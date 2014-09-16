/*
 * Copyright (c) 2014 Rob Clark <robdclark@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Simple test/demo for EGL_EXT_image_dma_buf_import
 */

#include "util.h"

static EGLint const config_attribute_list[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_DEPTH_SIZE, 8,
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE
};

static EGLDisplay display;
static EGLConfig config;
static EGLint num_config;
static EGLContext context;
static GLuint program;
const char *vertex_shader_source =
		"attribute vec4 in_position;    \n"
		"attribute vec2 in_TexCoord;    \n"
		"\n"
		"varying vec2 vTexCoord;      \n"
		"                             \n"
		"void main()                  \n"
		"{                            \n"
		"    gl_Position = in_position;\n"
		"    vTexCoord = in_TexCoord; \n"
		"}                            \n";

const char *fragment_shader_source =
		"precision mediump float;     \n"
		"                             \n"
		"uniform sampler2D uTexture;  \n"
		"varying vec2 vTexCoord;      \n"
		"                             \n"
		"void main()                  \n"
		"{                            \n"
		"    gl_FragColor = texture2D(uTexture, vTexCoord);\n"
		"}                            \n";

static void fill(char *virtual, int n, int width, int height, int stride)
{
	int i, j;
	/* paint the buffer with colored tiles */
	for (j = 0; j < height; j++) {
		uint32_t *fb_ptr = (uint32_t*)((char*)virtual + j * stride);
		for (i = 0; i < width; i++) {
			div_t d = div(n+i+j, width);
			fb_ptr[i] =
					0x00130502 * (d.quot >> 6) +
					0x000a1120 * (d.rem >> 6);
		}
	}
}

static EGLImageKHR
create_image(unsigned w, unsigned h)
{
	int fd;
	unsigned stride = ALIGN(w, 32) * 4;
	void *map = buf_alloc(stride * h, &fd);
	unsigned offset = 0;
	EGLint attr[] = {
			EGL_WIDTH, w,
			EGL_HEIGHT, h,
			EGL_LINUX_DRM_FOURCC_EXT, DRM_FORMAT_ARGB8888,
			EGL_DMA_BUF_PLANE0_FD_EXT, fd,
			EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
			EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
			EGL_NONE
	};

	fill(map, 0, w, h, stride);

	return eglCreateImageKHR(eglGetCurrentDisplay(), EGL_NO_CONTEXT,
			EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
}

int main(int argc, char **argv)
{
	GLint width, height;
	GLuint texturename = 0, texture_handle;
	GLfloat vVertices[] = {
			// front
			-0.45, -0.75, 0.0,
			 0.45, -0.75, 0.0,
			-0.45,  0.75, 0.0,
			 0.45,  0.75, 0.0
	};

	GLfloat vTexCoords[] = {
			1.0f, 1.0f,
			0.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
	};

	EGLSurface surface;
	EGLImageKHR img;
	unsigned w = 640;
	unsigned h = 480;

	display = get_display();

	/* get an appropriate EGL frame buffer configuration */
	ECHK(eglChooseConfig(display, config_attribute_list, &config, 1, &num_config));

	/* create an EGL rendering context */
	ECHK(context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribute_list));

	surface = make_window(display, config, 1024, 768);

	ECHK(eglQuerySurface(display, surface, EGL_WIDTH, &width));
	ECHK(eglQuerySurface(display, surface, EGL_HEIGHT, &height));

	/* connect the context to the surface */
	ECHK(eglMakeCurrent(display, surface, surface, context));

	program = get_program(vertex_shader_source, fragment_shader_source);

	GCHK(glBindAttribLocation(program, 0, "in_position"));
	GCHK(glBindAttribLocation(program, 1, "in_TexCoord"));

	link_program(program);

	GCHK(glViewport(0, 0, width, height));

	/* clear the color buffer */
	GCHK(glClearColor(0.5, 0.5, 0.5, 1.0));
	GCHK(glClear(GL_COLOR_BUFFER_BIT));

	GCHK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices));
	GCHK(glEnableVertexAttribArray(0));

	GCHK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vTexCoords));
	GCHK(glEnableVertexAttribArray(1));

	/* create test egl img: */
	img = create_image(w, h);

	GCHK(glActiveTexture(GL_TEXTURE0));
	GCHK(glGenTextures(1, &texturename));
	GCHK(glBindTexture(GL_TEXTURE_2D, texturename));
	GCHK(glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img));

	/* Note: cube turned black until these were defined. */
	GCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	GCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
	GCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	GCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	GCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R_OES, GL_CLAMP_TO_EDGE));

	GCHK(texture_handle = glGetUniformLocation(program, "uTexture"));

	GCHK(glUniform1i(texture_handle, 0)); /* '0' refers to texture unit 0. */

	GCHK(glEnable(GL_CULL_FACE));

	GCHK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));

	ECHK(eglSwapBuffers(display, surface));
	GCHK(glFlush());

	sleep(1);

	ECHK(eglDestroySurface(display, surface));

	ECHK(eglTerminate(display));
}
