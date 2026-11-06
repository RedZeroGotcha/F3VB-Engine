// STUB minimo do GL/gl.h (OpenGL 1.1), so para checar sintaxe no Linux.
#pragma once
typedef unsigned int GLenum, GLuint, GLbitfield; typedef int GLint, GLsizei; typedef float GLfloat; typedef unsigned char GLubyte, GLboolean; typedef double GLdouble;
typedef void GLvoid;
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_DEPTH_TEST 0xB71
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_LIGHTING 0xB50
#define GL_LIGHT0 0x4000
#define GL_COLOR_MATERIAL 0xB57
#define GL_FRONT_AND_BACK 0x408
#define GL_FRONT_AND_BACK_PLACEHOLDER 0
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_LIGHT_MODEL_TWO_SIDE 0xB52
#define GL_NORMALIZE 0xBA1
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_CULL_FACE 0xB44
#define GL_BACK 0x405
#define GL_CCW 0x901
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_TEXTURE_2D 0xDE1
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_BLEND 0xBE2
#define GL_ALPHA_TEST 0xBC0
#define GL_SRC_ALPHA 0x302
#define GL_ONE_MINUS_SRC_ALPHA 0x303
#define GL_ONE 1
#define GL_GREATER 0x204
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 4
#define GL_LINES 1
#define GL_POINTS 0
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_REPEAT 0x2901
#define GL_RGBA 0x1908
#define GL_POSITION 0x1203
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
void glViewport(GLint, GLint, GLsizei, GLsizei); void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat); void glClear(GLbitfield);
void glEnable(GLenum); void glDisable(GLenum); void glMatrixMode(GLenum); void glLoadIdentity(); void glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble);
void glFrustum(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble); void glTranslatef(GLfloat, GLfloat, GLfloat); void glRotatef(GLfloat, GLfloat, GLfloat, GLfloat);
void glLightfv(GLenum, GLenum, const GLfloat*); void glColor4f(GLfloat, GLfloat, GLfloat, GLfloat); void glColorMaterial(GLenum, GLenum); void glLightModeli(GLenum, GLint);
void glPolygonMode(GLenum, GLenum); void glCullFace(GLenum); void glFrontFace(GLenum); void glEnableClientState(GLenum); void glDisableClientState(GLenum);
void glTexEnvi(GLenum, GLenum, GLint); void glBindTexture(GLenum, GLuint); void glBlendFunc(GLenum, GLenum); void glAlphaFunc(GLenum, GLfloat); void glDepthMask(GLboolean);
void glVertexPointer(GLint, GLenum, GLsizei, const GLvoid*); void glNormalPointer(GLenum, GLsizei, const GLvoid*); void glTexCoordPointer(GLint, GLenum, GLsizei, const GLvoid*);
void glDrawElements(GLenum, GLsizei, GLenum, const GLvoid*); void glBegin(GLenum); void glEnd(); void glVertex3f(GLfloat, GLfloat, GLfloat); void glPointSize(GLfloat);
void glGenTextures(GLsizei, GLuint*); void glDeleteTextures(GLsizei, const GLuint*); void glTexParameteri(GLenum, GLenum, GLint);
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
GLuint glGenLists(GLsizei); void glListBase(GLuint); void glCallLists(GLsizei, GLenum, const GLvoid*); void glRasterPos2f(GLfloat, GLfloat);
// --- v12: glGetString e constantes (existem no GL/gl.h do Windows SDK)
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
inline const GLubyte* glGetString(GLenum) { return reinterpret_cast<const GLubyte*>("stub"); }
// --- v15: constantes e funcoes do GL 1.1 usadas pelo mapviewer (existem no GL/gl.h do Windows SDK)
#define GL_QUADS 0x0007
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_NEAREST 0x2600
#define GL_COLOR_ARRAY 0x8076
inline void glColor3f(GLfloat, GLfloat, GLfloat) {}
inline void glColor3fv(const GLfloat*) {}
inline void glColorPointer(GLint, GLenum, GLsizei, const GLvoid*) {}
inline void glTexCoord2f(GLfloat, GLfloat) {}
inline void glVertex3fv(const GLfloat*) {}
// --- v19: GL 1.1 usado pelo jogo e pela interface (existem no GL/gl.h do Windows SDK)
#define GL_FOG 0xB60
#define GL_FOG_MODE 0xB65
#define GL_FOG_COLOR 0xB66
#define GL_FOG_START 0xB63
#define GL_FOG_END 0xB64
#define GL_ALPHA 0x1906
#define GL_UNPACK_ALIGNMENT 0xCF5
#define GL_SRC_COLOR 0x0300
inline void glFogi(GLenum, GLint) {}
inline void glFogf(GLenum, GLfloat) {}
inline void glFogfv(GLenum, const GLfloat*) {}
inline void glLoadMatrixf(const GLfloat*) {}
inline void glScalef(GLfloat, GLfloat, GLfloat) {}
inline void glPushMatrix() {}
inline void glPopMatrix() {}
inline void glPixelStorei(GLenum, GLint) {}
inline void glLineWidth(GLfloat) {}
inline void glVertex2f(GLfloat, GLfloat) {}
inline void glColor4ub(GLubyte, GLubyte, GLubyte, GLubyte) {}
