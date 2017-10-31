#ifndef OpenGLWrapper_h
#define OpenGLWrapper_h

#include <EGL/egl.h>
#if defined(EGLW_GLES2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GLES/gl.h>
#include <GLES/glext.h>
#endif

#include <stdbool.h>

//--------------------------------------------------------------------------------
// Compatibility constants.
//--------------------------------------------------------------------------------
#if defined(EGLW_GLES2)
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_MODULATE 0x2100
#endif

#define GL_QUADS 0x0007
#define GL_POLYGON 0x0009

//--------------------------------------------------------------------------------
// Initialization.
//--------------------------------------------------------------------------------
bool oglwCreate();
void oglwDestroy();
bool oglwIsCreated();

//--------------------------------------------------------------------------------
// Viewport.
//--------------------------------------------------------------------------------
void oglwSetViewport(int x, int y, int w, int h);
void oglwSetDepthRange(float depthNear, float depthFar);

//--------------------------------------------------------------------------------
// Matrix stacks.
//--------------------------------------------------------------------------------
void oglwGetMatrix(GLenum matrixMode, GLfloat *matrix);
void oglwMatrixMode(GLenum matrixMode);
void oglwPushMatrix();
void oglwPopMatrix();
void oglwLoadMatrix(const GLfloat *matrix);
void oglwLoadIdentity();
void oglwFrustum(float left, float right, float bottom, float top, float zNear, float zFar);
void oglwOrtho(float left, float right, float bottom, float top, float zNear, float zFar);
void oglwTranslate(float x, float y, float z);
void oglwScale(float x, float y, float z);
void oglwRotate(float angle, float x, float y, float z);

//--------------------------------------------------------------------------------
// Shading.
//--------------------------------------------------------------------------------
// Enable smooth shading.
void oglwEnableSmoothShading(bool flag);

void oglwSetCurrentTextureUnitForced(int unit);
// Set the current unit.
void oglwSetCurrentTextureUnit(int unit);
// Bind the texture immediately.
void oglwBindTextureForced(int unit, GLuint texture);
// Bind the texture for the next oglwBegin() / oglwEnd().
void oglwBindTexture(int unit, GLuint texture);
// Enable texturing for the next oglwBegin() / oglwEnd().
void oglwEnableTexturing(int unit, bool flag);
// Set the current texture blending (GL_REPLACE, GL_MODULATE) for the next oglwBegin() / oglwEnd().
void oglwSetTextureBlending(int unit, GLint mode);

//--------------------------------------------------------------------------------
// ROPs.
//--------------------------------------------------------------------------------
// Enable blending.
void oglwEnableBlending(bool flag);
// Set blending function.
void oglwSetBlendingFunction(GLenum src, GLenum dst);
// Enable alpha test.
void oglwEnableAlphaTest(bool flag);
// Enable alpha test.
void oglwSetAlphaFunc(GLenum mode, GLfloat threshold);
// Enable depth test.
void oglwEnableDepthTest(bool flag);
// Enable stencil test.
void oglwEnableStencilTest(bool flag);
// Enable depth write.
void oglwEnableDepthWrite(bool flag);

//--------------------------------------------------------------------------------
// Clearing.
//--------------------------------------------------------------------------------
void oglwClear(GLbitfield mask);

//--------------------------------------------------------------------------------
// Drawing.
//--------------------------------------------------------------------------------
typedef struct oglwVertex_ {
    float position[4];
    float color[4];
    float texCoord[2][4];
} OglwVertex;

void oglwPointSize(float size);

/*
void oglwEnableClientState(GLenum array);
void oglwDisableClientState(GLenum array);
void oglwVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void oglwColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
*/

void oglwBegin(GLenum primitive);
void oglwEnd();
void oglwUpdateState();
void oglwUpdateStateWriteMask();
void oglwReset();
bool oglwIsEmpty();

OglwVertex* oglwAllocateVertex(int vertexNb);
GLushort* oglwAllocateIndex(int indexNb);
OglwVertex* oglwAllocateLineStrip(int vertexNb);
OglwVertex* oglwAllocateLineLoop(int vertexNb);
OglwVertex* oglwAllocateQuad(int vertexNb);
OglwVertex* oglwAllocateTriangleFan(int vertexNb);
OglwVertex* oglwAllocateTriangleStrip(int vertexNb);

void oglwVertex2f(GLfloat x, GLfloat y);
void oglwVertex2i(GLint x, GLint y);
void oglwVertex3f(GLfloat x, GLfloat y, GLfloat z);
void oglwVertex3fv(const GLfloat *v);

void oglwGetColor(GLfloat *v);
void oglwColor3f(GLfloat r, GLfloat g, GLfloat b);
void oglwColor3fv(const GLfloat *v);
void oglwColor4ubv(const GLubyte *v);
void oglwColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void oglwColor4fv(const GLfloat *v);

void oglwTexCoord2f(GLfloat u, GLfloat v);

void oglwMultiTexCoord2f(GLenum unit, GLfloat u, GLfloat v);

static inline void Vertex_set2(float *t, float x, float y)
{
    t[0]=x; t[1]=y; t[2]=0.0f; t[3]=1.0f;
}

static inline void Vertex_set3(float *t, float x, float y, float z)
{
    t[0]=x; t[1]=y; t[2]=z; t[3]=1.0f;
}

static inline void Vertex_set4(float *t, float x, float y, float z, float w)
{
    t[0]=x; t[1]=y; t[2]=z; t[3]=w;
}

static inline OglwVertex* AddVertex3D(OglwVertex *v, float px, float py, float pz)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); v++;
    return v;
}

static inline OglwVertex* AddVertex3D_C(OglwVertex *v, float px, float py, float pz, float r, float g, float b, float a)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, r, g, b, a); v++;
    return v;
}

static inline OglwVertex* AddVertex3D_T1(OglwVertex *v, float px, float py, float pz, float tx, float ty)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tx, ty); v++;
    return v;
}

static inline OglwVertex* AddVertex3D_CT1(OglwVertex *v, float px, float py, float pz, float r, float g, float b, float a, float tx, float ty)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tx, ty); v++;
    return v;
}

static inline OglwVertex* AddVertex3D_T2(OglwVertex *v, float px, float py, float pz, float tx0, float ty0, float tx1, float ty1)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tx0, ty0); Vertex_set2(v->texCoord[1], tx1, ty1); v++;
    return v;
}

static inline OglwVertex* AddVertex3D_CT2(OglwVertex *v, float px, float py, float pz, float r, float g, float b, float a, float tx0, float ty0, float tx1, float ty1)
{
    Vertex_set3(v->position, px, py, pz); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tx0, ty0); Vertex_set2(v->texCoord[1], tx1, ty1); v++;
    return v;
}

static inline OglwVertex* AddQuad2D_C(OglwVertex *v, float px0, float py0, float px1, float py1, float r, float g, float b, float a)
{
    Vertex_set2(v->position, px0, py0); Vertex_set4(v->color, r, g, b, a); v++;
    Vertex_set2(v->position, px1, py0); Vertex_set4(v->color, r, g, b, a); v++;
    Vertex_set2(v->position, px1, py1); Vertex_set4(v->color, r, g, b, a); v++;
    Vertex_set2(v->position, px0, py1); Vertex_set4(v->color, r, g, b, a); v++;
    return v;
}

static inline OglwVertex* AddQuad2D_T1(OglwVertex *v, float px0, float py0, float px1, float py1, float tpx0, float tpy0, float tpx1, float tpy1)
{
    Vertex_set2(v->position, px0, py0); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tpx0, tpy0); v++;
    Vertex_set2(v->position, px1, py0); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tpx1, tpy0); v++;
    Vertex_set2(v->position, px1, py1); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tpx1, tpy1); v++;
    Vertex_set2(v->position, px0, py1); Vertex_set4(v->color, 1.0f, 1.0f, 1.0f, 1.0f); Vertex_set2(v->texCoord[0], tpx0, tpy1); v++;
    return v;
}

static inline OglwVertex* AddQuad2D_CT1(OglwVertex *v, float px0, float py0, float px1, float py1, float r, float g, float b, float a, float tpx0, float tpy0, float tpx1, float tpy1)
{
    Vertex_set2(v->position, px0, py0); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tpx0, tpy0); v++;
    Vertex_set2(v->position, px1, py0); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tpx1, tpy0); v++;
    Vertex_set2(v->position, px1, py1); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tpx1, tpy1); v++;
    Vertex_set2(v->position, px0, py1); Vertex_set4(v->color, r, g, b, a); Vertex_set2(v->texCoord[0], tpx0, tpy1); v++;
    return v;
}

#endif
