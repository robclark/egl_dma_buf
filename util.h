/*
 * Copyright (c) 2014 Rob Clark <robdclark@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <drm_fourcc.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

/*****************************************************************************/

#define ALIGN(_v, _d) (((_v) + ((_d) - 1)) & ~((_d) - 1))

#define DEBUG_MSG(fmt, ...) \
		do { \
			printf(fmt " (%s:%d)\n", \
					##__VA_ARGS__, __FUNCTION__, __LINE__); \
		} while (0)
#define ERROR_MSG(fmt, ...) \
		do { printf("ERROR: " fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)


#define ECHK(x) do { \
		EGLBoolean status; \
		DEBUG_MSG(">>> %s", #x); \
		status = (EGLBoolean)(x); \
		if (!status) { \
			EGLint err = eglGetError(); \
			ERROR_MSG("<<< %s: failed: 0x%04x (%s)", #x, err, eglStrError(err)); \
			exit(-1); \
		} \
		DEBUG_MSG("<<< %s: succeeded", #x); \
	} while (0)

#define GCHK(x) do { \
		GLenum err; \
		DEBUG_MSG(">>> %s", #x); \
		x; \
		err = glGetError(); \
		if (err != GL_NO_ERROR) { \
			ERROR_MSG("<<< %s: failed: 0x%04x (%s)", #x, err, glStrError(err)); \
			exit(-1); \
		} \
		DEBUG_MSG("<<< %s: succeeded", #x); \
	} while (0)

#define ENUM(x) case x: return #x
static inline const char *
typename(GLenum type)
{
	switch(type) {
	ENUM(GL_BYTE);
	ENUM(GL_UNSIGNED_BYTE);
	ENUM(GL_SHORT);
	ENUM(GL_UNSIGNED_SHORT);
	ENUM(GL_INT);
	ENUM(GL_UNSIGNED_INT);
	ENUM(GL_FIXED);
	ENUM(GL_FLOAT);
	ENUM(GL_UNSIGNED_INT_10_10_10_2_OES);
	ENUM(GL_INT_10_10_10_2_OES);
	ENUM(GL_UNSIGNED_SHORT_5_6_5);
	ENUM(GL_UNSIGNED_SHORT_4_4_4_4);
	ENUM(GL_UNSIGNED_SHORT_5_5_5_1);
	ENUM(GL_HALF_FLOAT_OES);
	ENUM(GL_BGRA_EXT);
	ENUM(GL_UNSIGNED_INT_2_10_10_10_REV_EXT);
	}
	ERROR_MSG("invalid type: %04x", type);
	exit(1);
	return NULL;
}

static inline const char *
formatname(GLenum format)
{
	switch (format) {
	ENUM(GL_RGB);
	ENUM(GL_RGBA);
	ENUM(GL_ALPHA);
	ENUM(GL_LUMINANCE);
	ENUM(GL_LUMINANCE_ALPHA);
	ENUM(GL_DEPTH_COMPONENT);
	}
	ERROR_MSG("invalid format: %04x", format);
	exit(1);
	return NULL;
}

static char *
eglStrError(EGLint error)
{
	switch (error) {
	case EGL_SUCCESS:
		return "EGL_SUCCESS";
	case EGL_BAD_ALLOC:
		return "EGL_BAD_ALLOC";
	case EGL_BAD_CONFIG:
		return "EGL_BAD_CONFIG";
	case EGL_BAD_PARAMETER:
		return "EGL_BAD_PARAMETER";
	case EGL_BAD_MATCH:
		return "EGL_BAD_MATCH";
	case EGL_BAD_ATTRIBUTE:
		return "EGL_BAD_ATTRIBUTE";
	default:
		return "UNKNOWN";
	}
}

static char *
glStrError(GLenum error)
{
	switch (error) {
	// TODO
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	default:
		return "UNKNOWN";
	}
}

static EGLNativeDisplayType native_dpy;

static EGLDisplay
get_display(void)
{
	EGLDisplay display;
	EGLint egl_major, egl_minor;

	native_dpy = XOpenDisplay(NULL);
	display = eglGetDisplay(native_dpy);
	if (display == EGL_NO_DISPLAY) {
		ERROR_MSG("No display found!");
		exit(-1);
	}

	ECHK(eglInitialize(display, &egl_major, &egl_minor));

	DEBUG_MSG("Using display %p with EGL version %d.%d",
			display, egl_major, egl_minor);

	DEBUG_MSG("EGL Version \"%s\"", eglQueryString(display, EGL_VERSION));
	DEBUG_MSG("EGL Vendor \"%s\"", eglQueryString(display, EGL_VENDOR));
	DEBUG_MSG("EGL Extensions \"%s\"", eglQueryString(display, EGL_EXTENSIONS));

	return display;
}

static EGLSurface make_window(EGLDisplay display, EGLConfig config, int width, int height)
{
	EGLSurface surface;
	XVisualInfo *visInfo, visTemplate;
	int num_visuals;
	Window root, xwin;
	XSetWindowAttributes attr;
	unsigned long mask;
	EGLint vid;
	const char *title = "egl";

	ECHK(eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &vid));

	/* The X window visual must match the EGL config */
	visTemplate.visualid = vid;
	visInfo = XGetVisualInfo(native_dpy, VisualIDMask, &visTemplate, &num_visuals);
	if (!visInfo) {
		ERROR_MSG("failed to get an visual of id 0x%x", vid);
		exit(-1);
	}

	root = RootWindow(native_dpy, DefaultScreen(native_dpy));

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(native_dpy,
			root, visInfo->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	xwin = XCreateWindow(native_dpy, root, 0, 0, width, height,
			0, visInfo->depth, InputOutput, visInfo->visual, mask, &attr);
	if (!xwin) {
		ERROR_MSG("failed to create a window");
		exit (-1);
	}

	XFree(visInfo);

	/* set hints and properties */
	{
		XSizeHints sizehints;
		sizehints.x = 0;
		sizehints.y = 0;
		sizehints.width  = width;
		sizehints.height = height;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(native_dpy, xwin, &sizehints);
		XSetStandardProperties(native_dpy, xwin,
				title, title, None, (char **) NULL, 0, &sizehints);
	}

	XMapWindow(native_dpy, xwin);

	surface = eglCreateWindowSurface(display, config, xwin, NULL);

	return surface;
}

static GLuint
get_program(const char *vertex_shader_source, const char *fragment_shader_source)
{
	GLuint vertex_shader, fragment_shader, program;
	GLint ret;

	DEBUG_MSG("vertex shader:\n%s", vertex_shader_source);
	DEBUG_MSG("fragment shader:\n%s", fragment_shader_source);

	GCHK(vertex_shader = glCreateShader(GL_VERTEX_SHADER));

	GCHK(glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL));
	GCHK(glCompileShader(vertex_shader));

	GCHK(glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret));
	if (!ret) {
		char *log;

		ERROR_MSG("vertex shader compilation failed!:");
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(vertex_shader, ret, NULL, log);
			printf("%s", log);
		}
		exit(-1);
	}

	DEBUG_MSG("Vertex shader compilation succeeded!");

	GCHK(fragment_shader = glCreateShader(GL_FRAGMENT_SHADER));

	GCHK(glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL));
	GCHK(glCompileShader(fragment_shader));

	GCHK(glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret));
	if (!ret) {
		char *log;

		ERROR_MSG("fragment shader compilation failed!:");
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetShaderInfoLog(fragment_shader, ret, NULL, log);
			printf("%s", log);
		}
		exit(-1);
	}

	DEBUG_MSG("Fragment shader compilation succeeded!");

	GCHK(program = glCreateProgram());

	GCHK(glAttachShader(program, vertex_shader));
	GCHK(glAttachShader(program, fragment_shader));

	return program;
}

static void
link_program(GLuint program)
{
	GLint ret, len;
	GLenum binary_format;
	void *binary;

	GCHK(glLinkProgram(program));

	GCHK(glGetProgramiv(program, GL_LINK_STATUS, &ret));
	if (!ret) {
		char *log;

		ERROR_MSG("program linking failed!:");
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(program, ret, NULL, log);
			printf("%s", log);
		}
		exit(-1);
	}

	DEBUG_MSG("program linking succeeded!");

	GCHK(glUseProgram(program));
}

void *buf_alloc(int size, int *fd);

#endif /* _UTIL_H_ */
