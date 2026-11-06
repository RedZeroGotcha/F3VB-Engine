// STUB de GL/gl.h apenas para checar compilacao/execucao headless no Linux. NAO usado no build real.
#pragma once
#include <cstddef>
typedef unsigned int GLuint, GLenum, GLbitfield; typedef int GLint, GLsizei; typedef float GLfloat; typedef unsigned char GLubyte; typedef void GLvoid;
#define GL_AMBIENT 4096
#define GL_AMBIENT_AND_DIFFUSE 4097
#define GL_BACK 4098
#define GL_BLEND 4099
#define GL_CLAMP 4100
#define GL_COLOR_ARRAY 4101
#define GL_COLOR_BUFFER_BIT 4102
#define GL_COLOR_MATERIAL 4103
#define GL_COMPILE 4104
#define GL_CONSTANT_ATTENUATION 4105
#define GL_CULL_FACE 4106
#define GL_DEPTH_BUFFER_BIT 4107
#define GL_DEPTH_TEST 4108
#define GL_DIFFUSE 4109
#define GL_FALSE 4110
#define GL_FLOAT 4111
#define GL_FOG 4112
#define GL_FOG_COLOR 4113
#define GL_FOG_END 4114
#define GL_FOG_HINT 4115
#define GL_FOG_MODE 4116
#define GL_FOG_START 4117
#define GL_FRONT_AND_BACK 4118
#define GL_LEQUAL 4119
#define GL_LIGHT0 4120
#define GL_LIGHT1 4121
#define GL_LIGHT2 4122
#define GL_LIGHTING 4123
#define GL_LIGHT_MODEL_AMBIENT 4124
#define GL_LIGHT_MODEL_TWO_SIDE 4125
#define GL_LINEAR 4126
#define GL_LINEAR_ATTENUATION 4127
#define GL_LINES 4128
#define GL_LINE_LOOP 4129
#define GL_MODELVIEW 4130
#define GL_NICEST 4131
#define GL_NORMALIZE 4132
#define GL_NORMAL_ARRAY 4133
#define GL_ONE 4134
#define GL_ONE_MINUS_SRC_ALPHA 4135
#define GL_PACK_ALIGNMENT 4136
#define GL_PERSPECTIVE_CORRECTION_HINT 4137
#define GL_POINTS 4138
#define GL_POSITION 4139
#define GL_PROJECTION 4140
#define GL_QUADS 4141
#define GL_QUAD_STRIP 4142
#define GL_RGB 4143
#define GL_SCISSOR_TEST 4144
#define GL_SMOOTH 4145
#define GL_SPECULAR 4146
#define GL_SPOT_CUTOFF 4147
#define GL_SPOT_DIRECTION 4148
#define GL_SPOT_EXPONENT 4149
#define GL_SRC_ALPHA 4150
#define GL_TEXTURE_2D 4151
#define GL_TEXTURE_MAG_FILTER 4152
#define GL_TEXTURE_MIN_FILTER 4153
#define GL_TEXTURE_WRAP_S 4154
#define GL_TEXTURE_WRAP_T 4155
#define GL_TRIANGLES 4156
#define GL_TRIANGLE_FAN 4157
#define GL_TRUE 4158
#define GL_UNPACK_ALIGNMENT 4159
#define GL_UNSIGNED_BYTE 4160
#define GL_VERTEX_ARRAY 4161
template<class... A> inline void glBegin(A...) {}
template<class... A> inline void glBindTexture(A...) {}
template<class... A> inline void glBlendFunc(A...) {}
template<class... A> inline void glCallList(A...) {}
template<class... A> inline void glCallLists(A...) {}
template<class... A> inline void glClear(A...) {}
template<class... A> inline void glClearColor(A...) {}
template<class... A> inline void glColor3f(A...) {}
template<class... A> inline void glColor4f(A...) {}
template<class... A> inline void glColorMaterial(A...) {}
template<class... A> inline void glColorPointer(A...) {}
template<class... A> inline void glDeleteLists(A...) {}
template<class... A> inline void glDepthFunc(A...) {}
template<class... A> inline void glDepthMask(A...) {}
template<class... A> inline void glDisable(A...) {}
template<class... A> inline void glDisableClientState(A...) {}
template<class... A> inline void glDrawArrays(A...) {}
template<class... A> inline void glEnable(A...) {}
template<class... A> inline void glEnableClientState(A...) {}
template<class... A> inline void glEnd(A...) {}
template<class... A> inline void glEndList(A...) {}
template<class... A> inline void glFogf(A...) {}
template<class... A> inline void glFogfv(A...) {}
template<class... A> inline void glFogi(A...) {}
template<class... A> inline void glFrustum(A...) {}
template<class... A> inline void glGenTextures(A...) {}
template<class... A> inline void glHint(A...) {}
template<class... A> inline void glLightModelfv(A...) {}
template<class... A> inline void glLightModeli(A...) {}
template<class... A> inline void glLightf(A...) {}
template<class... A> inline void glLightfv(A...) {}
template<class... A> inline void glLineWidth(A...) {}
template<class... A> inline void glListBase(A...) {}
template<class... A> inline void glLoadIdentity(A...) {}
template<class... A> inline void glMatrixMode(A...) {}
template<class... A> inline void glMultMatrixf(A...) {}
template<class... A> inline void glNewList(A...) {}
template<class... A> inline void glNormal3f(A...) {}
template<class... A> inline void glNormalPointer(A...) {}
template<class... A> inline void glOrtho(A...) {}
template<class... A> inline void glPixelStorei(A...) {}
template<class... A> inline void glPointSize(A...) {}
template<class... A> inline void glPopMatrix(A...) {}
template<class... A> inline void glPushMatrix(A...) {}
template<class... A> inline void glRasterPos2f(A...) {}
template<class... A> inline void glReadBuffer(A...) {}
template<class... A> inline void glReadPixels(A...) {}
template<class... A> inline void glRotatef(A...) {}
template<class... A> inline void glScalef(A...) {}
template<class... A> inline void glScissor(A...) {}
template<class... A> inline void glShadeModel(A...) {}
template<class... A> inline void glTexCoord2f(A...) {}
template<class... A> inline void glTexImage2D(A...) {}
template<class... A> inline void glTexParameteri(A...) {}
template<class... A> inline void glTranslatef(A...) {}
template<class... A> inline void glVertex2f(A...) {}
template<class... A> inline void glVertex3f(A...) {}
template<class... A> inline void glVertex3fv(A...) {}
template<class... A> inline void glVertexPointer(A...) {}
template<class... A> inline void glViewport(A...) {}
inline GLuint glGenLists(GLsizei n) { static GLuint nx = 1; GLuint r = nx; nx += (GLuint)n; return r; }
