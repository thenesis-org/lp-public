#include "OpenGLES/OpenGLWrapper.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define OGLW_PI 3.14159265359f

//#define BUFFER_OBJECT_USED

enum {
    Array_Position,
    Array_Color,
    Array_TexCoord0,
    Array_TexCoord1,
    Array_Nb
};

typedef struct OpenGLWrapper_ OpenGLWrapper;

#if defined(EGLW_GLES2)
typedef struct OglwMatrixStack_
{
    float *matrices;
    int capacity, depth;
} OglwMatrixStack;
#endif

typedef struct OpenGLWrapperArray_ {
    bool enabled;
    GLint size;
    GLenum type;
    GLsizei stride;
    const GLvoid *pointer;
} OpenGLWrapperArray;

typedef struct OpenGLWrapperTextureUnit_ {
    GLboolean texturingEnabled, texturingEnabledRequested;
    GLuint texture, textureRequested;
    GLenum blending, blendingRequested;
} OpenGLWrapperTextureUnit;

struct OpenGLWrapper_ {
    struct {
        int x, y, width, height;
        float depthNear, depthFar;
    } viewport;

    #if defined(EGLW_GLES2)
    GLuint vertexShader, fragmentShader, program;

    GLint u_transformation;
    GLint u_tex0Enabled;
    GLint u_tex1Enabled;
    GLint u_tex0BlendingEnabled;
    GLint u_tex1BlendingEnabled;
    GLint s_tex0;
    GLint s_tex1;
    GLint u_alphaThreshold;

    GLint a_position;
    GLint a_color;
    GLint a_texcoord0;
    GLint a_texcoord1;
    
	GLenum matrixMode;
    OglwMatrixStack projectionStack;
    OglwMatrixStack modelViewStack;
    float *transformation;
    bool transformationDirty;
    #endif
    
    // Shading.
    bool smoothShadingEnabled, smoothShadingEnabledRequested;
    int textureUnit, textureUnitRequested;
    OpenGLWrapperTextureUnit textureUnits[2];

    // ROPs.
    bool blendingEnabled, blendingEnabledRequested;
    GLenum blendingSrc, blendingSrcRequested;
    GLenum blendingDst, blendingDstRequested;
    bool alphaTestEnabled, alphaTestEnabledRequested;
    bool depthTestEnabled, depthTestEnabledRequested;
    bool stencilTestEnabled, stencilTestEnabledRequested;
    bool depthWriteEnabled, depthWriteEnabledRequested;

    OglwVertex currentVertex;
    
    int verticesCapacity;
    int verticesLength;
    OglwVertex *vertices;
    
    int indicesCapacity;
    int indicesLength;
    GLushort *indices;
    
    #if defined(BUFFER_OBJECT_USED)
    GLuint bufferId;
    #endif
   
    bool beginFlag;
    GLenum primitive;
    OpenGLWrapperArray arrays[Array_Nb];  
};

#define DEFAULT_VERTEX_CAPACITY (1024*16)
#define DEFAULT_INDEX_CAPACITY (1024*16*6)

static void oglwSetupArrays(OpenGLWrapper *oglw);
static void oglwCleanupArrays(OpenGLWrapper *oglw);
static bool oglwReserveVertices(OpenGLWrapper *oglw, int verticesCapacityMin);
static bool oglwReserveIndices(OpenGLWrapper *oglw, int indicesCapacityMin);

static OpenGLWrapper *l_openGLWrapper = NULL;

//--------------------------------------------------------------------------------
// Error.
//--------------------------------------------------------------------------------
static void oglwCheckError()
{
    #if 1
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("GL error %i\n", error);
    }
    #endif
}

//--------------------------------------------------------------------------------
// Shaders.
//--------------------------------------------------------------------------------
#if defined(EGLW_GLES2)

static const char *oglwVertexShaderSources =
"precision highp float;\n"
"uniform mat4 u_transformation;\n"
"attribute vec4 a_position;\n"
"attribute vec4 a_color;\n"
"attribute vec4 a_texcoord0;\n"
"attribute vec4 a_texcoord1;\n"
"varying vec4 v_color;\n"
"varying vec4 v_texcoord0;\n"
"varying vec4 v_texcoord1;\n"
"void main()\n"
"{\n"
"   v_color = a_color;\n"
"   v_texcoord0 = a_texcoord0;\n"
"   v_texcoord1 = a_texcoord1;\n"
"   gl_Position = a_position * u_transformation;\n"
"}\n"
;

static const char *oglwFragmentShaderSources =
"precision mediump float;\n"
"uniform bool u_tex0Enabled;\n"
"uniform bool u_tex1Enabled;\n"
"uniform bool u_tex0BlendingEnabled;\n"
"uniform bool u_tex1BlendingEnabled;\n"
"uniform sampler2D s_tex0;\n"
"uniform sampler2D s_tex1;\n"
"uniform float u_alphaThreshold;\n"
"varying vec4 v_color;\n"
"varying vec4 v_texcoord0;\n"
"varying vec4 v_texcoord1;\n"
"void main()\n"
"{\n"
"	vec4 color = v_color;\n"
"	if (u_tex0Enabled)\n"
"	{\n"
"	    if (u_tex0BlendingEnabled)\n"
"	        color *= texture2D(s_tex0, v_texcoord0.xy);\n"
"	    else\n"
"	        color = texture2D(s_tex0, v_texcoord0.xy);\n"
"	}\n"
"	if (u_tex1Enabled)\n"
"	{\n"
"	    if (u_tex1BlendingEnabled)\n"
"	        color *= texture2D(s_tex1, v_texcoord1.xy);\n"
"	    else\n"
"	        color = texture2D(s_tex1, v_texcoord1.xy);\n"
"	}\n"
"	if (color.a < u_alphaThreshold)\n"
//"	    discard;\n"
"	    color.a = 0.0;\n"
"	gl_FragColor = color;\n"
"}\n"
;

static void Matrix4x4_setNull(float *m)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
}

static void Matrix4x4_setIdentity(float *m)
{
    m[0 + 0*4] = 1.0f; m[1 + 0*4] = 0.0f; m[2 + 0*4] = 0.0f; m[3 + 0*4] = 0.0f;
    m[0 + 1*4] = 0.0f; m[1 + 1*4] = 1.0f; m[2 + 1*4] = 0.0f; m[3 + 1*4] = 0.0f;
    m[0 + 2*4] = 0.0f; m[1 + 2*4] = 0.0f; m[2 + 2*4] = 1.0f; m[3 + 2*4] = 0.0f;
    m[0 + 3*4] = 0.0f; m[1 + 3*4] = 0.0f; m[2 + 3*4] = 0.0f; m[3 + 3*4] = 1.0f;
}

static void Matrix4x4_transpose(float *m, const float *ms)
{
	for (int j=0; j<4; j++) {
		for (int i=0; i<4; i++) {
			m[j*4+i]=ms[i*4+j];
		}
	}
}

static void Matrix4x4_copy(float *m, const float *ms)
{
    for (int i = 0; i < 16; i++) m[i] = ms[i];
}

static void Matrix4x4_mul(float *m, const float *ms0, const float *ms1)
{
	for (int j=0; j<4; j++) {
		for (int i=0; i<4; i++) {
			m[j*4+i]=ms0[j*4+0]*ms1[0*4+i]+ms0[j*4+1]*ms1[1*4+i]+ms0[j*4+2]*ms1[2*4+i]+ms0[j*4+3]*ms1[3*4+i];
		}
	}
}

static void Matrix4x4_mulPost(float *m, const float *ms)
{
	float mt[16];
	Matrix4x4_copy(mt, m);
	Matrix4x4_mul(m, mt, ms);
}

static void OglwMatrixStack_initialize(OglwMatrixStack *ms)
{
    ms->matrices = NULL;
    ms->capacity = 0;
    ms->depth = 0;
}

static bool OglwMatrixStack_allocate(OglwMatrixStack *ms, int capacity)
{
    float *matrices = (float*)malloc(capacity * 16 * sizeof(float));
    if (matrices == NULL)
        return true;
    ms->matrices = matrices;
    ms->capacity = capacity;
    for (int i = 0; i < capacity; i++) Matrix4x4_setIdentity(&matrices[i * 16]);
    return false;
}

static void OglwMatrixStack_free(OglwMatrixStack *ms)
{
    free(ms->matrices);
    OglwMatrixStack_initialize(ms);
}

static GLuint oglwCreateShader(const char *sources, GLenum type)
{
	GLint compiled;
	GLuint shader = glCreateShader(type);
	if (shader == 0)
	{
		printf("Failed to created shader for '%s'\n", sources);
        goto on_error;
	}
	glShaderSource(shader, 1, (const GLchar **)&sources, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 1)
		{
			char *log = malloc(sizeof(char) * logLength);
			glGetShaderInfoLog(shader, logLength, NULL, log);
			printf("Error compiling shader:\n'%s'\nLog:\n%s\n", sources, log);
			free(log);
		}
		goto on_error;
	}
	return shader;
on_error:
    glDeleteShader(shader);
    return 0;
}

static void oglwInitializeShaders(OpenGLWrapper *oglw)
{
    oglw->vertexShader = 0;
    oglw->fragmentShader = 0;
    oglw->program = 0;
    OglwMatrixStack_initialize(&oglw->modelViewStack);
    OglwMatrixStack_initialize(&oglw->projectionStack);
    oglw->transformation = NULL;
}

static void oglwFinalizeShaders(OpenGLWrapper *oglw)
{
    free(oglw->transformation);
    OglwMatrixStack_free(&oglw->modelViewStack);
    OglwMatrixStack_free(&oglw->projectionStack);
    glDeleteProgram(oglw->program);
    glDeleteShader(oglw->vertexShader);
    glDeleteShader(oglw->fragmentShader);
    oglwInitializeShaders(oglw);
}

static GLint oglwGetUniformLocation(GLuint program, const char *name)
{
    GLint location = glGetUniformLocation(program, name);
    if (location < 0)
    {
        printf("Cannot find uniform location for %s\n", name);
    }
    return location;
}

static GLint oglwGetAttributeLocation(GLuint program, const char *name)
{
    GLint location = glGetAttribLocation(program, name);
    if (location < 0)
    {
        printf("Cannot find attribute location for %s\n", name);
    }
    return location;
}

static bool oglwSetupShaders(OpenGLWrapper *oglw)
{
    GLuint vertexShader, fragmentShader, program;
	GLint linked;

    vertexShader = oglwCreateShader(oglwVertexShaderSources, GL_VERTEX_SHADER);
    if (vertexShader == 0) goto on_error;
    oglw->vertexShader = vertexShader;

    fragmentShader = oglwCreateShader(oglwFragmentShaderSources, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) goto on_error;
    oglw->fragmentShader = fragmentShader;
    
    program = glCreateProgram();
    if (program == 0) goto on_error;
    glAttachShader(program, vertexShader);
    oglw->program = program;
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 1)
        {
            char *log = malloc(logLength);
            glGetProgramInfoLog(program, logLength, NULL, log);
			printf("Error linking program. Log:\n%s\n", log);
            free(log);
        }
        goto on_error;
    }

    if ((oglw->a_position = oglwGetAttributeLocation(program, "a_position")) < 0) goto on_error;
    if ((oglw->a_color = oglwGetAttributeLocation(program, "a_color")) < 0) goto on_error;
    if ((oglw->a_texcoord0 = oglwGetAttributeLocation(program, "a_texcoord0")) < 0) goto on_error;
    if ((oglw->a_texcoord1 = oglwGetAttributeLocation(program, "a_texcoord1")) < 0) goto on_error;

    if ((oglw->u_transformation = oglwGetUniformLocation(program, "u_transformation")) < 0) goto on_error;
    if ((oglw->u_tex0Enabled = oglwGetUniformLocation(program, "u_tex0Enabled")) < 0) goto on_error;
    if ((oglw->u_tex1Enabled = oglwGetUniformLocation(program, "u_tex1Enabled")) < 0) goto on_error;
    if ((oglw->u_tex0BlendingEnabled = oglwGetUniformLocation(program, "u_tex0BlendingEnabled")) < 0) goto on_error;
    if ((oglw->u_tex1BlendingEnabled = oglwGetUniformLocation(program, "u_tex1BlendingEnabled")) < 0) goto on_error;
    if ((oglw->s_tex0 = oglwGetUniformLocation(program, "s_tex0")) < 0) goto on_error;
    if ((oglw->s_tex1 = oglwGetUniformLocation(program, "s_tex1")) < 0) goto on_error;
    if ((oglw->u_alphaThreshold = oglwGetUniformLocation(program, "u_alphaThreshold")) < 0) goto on_error;

    glUseProgram(program);

    if (OglwMatrixStack_allocate(&oglw->modelViewStack, 16)) goto on_error;
    if (OglwMatrixStack_allocate(&oglw->projectionStack, 16)) goto on_error;
    {
        float *m = malloc(16*sizeof(GLfloat));
        if (m == NULL) goto on_error;
        oglw->transformation = m;
        Matrix4x4_setIdentity(m);
    }
    oglw->transformationDirty = true;

    glUniform1i(oglw->u_tex0Enabled, 1);
    glUniform1i(oglw->u_tex1Enabled, 0);
    glUniform1i(oglw->u_tex0BlendingEnabled, 1);
    glUniform1i(oglw->u_tex1BlendingEnabled, 1);
    glUniform1i(oglw->s_tex0, 0);
    glUniform1i(oglw->s_tex1, 1);
    glUniform1f(oglw->u_alphaThreshold, 0);
    
    return false;
on_error:
    oglwFinalizeShaders(oglw);
    return true;
}

#endif

//--------------------------------------------------------------------------------
// Initialization.
//--------------------------------------------------------------------------------
bool oglwCreate() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (oglw == NULL) {
        oglw = malloc(sizeof(OpenGLWrapper));
		if (oglw==NULL) goto on_error;
        l_openGLWrapper = oglw;
        
        oglw->currentVertex.position[0]=0.0f;
        oglw->currentVertex.position[1]=0.0f;
        oglw->currentVertex.position[2]=0.0f;
        oglw->currentVertex.position[3]=1.0f;
        oglw->currentVertex.color[0]=1.0f;
        oglw->currentVertex.color[1]=1.0f;
        oglw->currentVertex.color[2]=1.0f;
        oglw->currentVertex.color[3]=1.0f;
        oglw->currentVertex.texCoord[0][0]=0.0f;
        oglw->currentVertex.texCoord[0][1]=0.0f;
        oglw->currentVertex.texCoord[0][2]=0.0f;
        oglw->currentVertex.texCoord[0][3]=1.0f;
        oglw->currentVertex.texCoord[1][0]=0.0f;
        oglw->currentVertex.texCoord[1][1]=0.0f;
        oglw->currentVertex.texCoord[1][2]=0.0f;
        oglw->currentVertex.texCoord[1][3]=1.0f;
        
        oglw->verticesCapacity=0;
        oglw->verticesLength=0;
        oglw->vertices=NULL;

        oglw->indicesCapacity=0;
        oglw->indicesLength=0;
        oglw->indices=NULL;
        
        #if defined(BUFFER_OBJECT_USED)
        oglw->bufferId = 0;
        #endif

        oglw->viewport.x = 0;
        oglw->viewport.y = 0;
        oglw->viewport.width = 0;
        oglw->viewport.height = 0;
        oglw->viewport.depthNear = 0.0f;
        oglw->viewport.depthFar = 1.0f;
        glViewport(0, 0, 0, 0);
        glDepthRangef(0.0f, 1.0f);

        #if defined(EGLW_GLES2)
        oglwInitializeShaders(oglw);
        if (oglwSetupShaders(oglw)) goto on_error;
        #endif

        oglw->smoothShadingEnabled=oglw->smoothShadingEnabledRequested=true;
        #if defined(EGLW_GLES1)
        glShadeModel(GL_SMOOTH);
        #else
        // TODO
        #endif
        
        oglw->textureUnit=oglw->textureUnitRequested=0;
        for (int i = 1; i >= 0; i--)
        {
            glActiveTexture(GL_TEXTURE0+i);
            OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[i];

            tu->texturingEnabled=tu->texturingEnabledRequested=false;
            #if defined(EGLW_GLES1)
            glDisable(GL_TEXTURE_2D);
            #else
            if (i == 0)
                glUniform1i(oglw->u_tex0Enabled, 0);
            else
                glUniform1i(oglw->u_tex1Enabled, 0);
            #endif
            
            tu->blending=tu->blendingRequested=GL_MODULATE;
            #if defined(EGLW_GLES1)
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tu->blending);
            #else
            if (i == 0)
                glUniform1i(oglw->u_tex0BlendingEnabled, tu->blending == GL_MODULATE);
            else
                glUniform1i(oglw->u_tex1BlendingEnabled, tu->blending == GL_MODULATE);
            #endif

            tu->texture=tu->textureRequested=0;
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        oglw->blendingEnabled=oglw->blendingEnabledRequested=false;
        glDisable(GL_BLEND);
        oglw->blendingSrc=oglw->blendingSrcRequested=GL_ONE;
        oglw->blendingDst=oglw->blendingDstRequested=GL_ZERO;
        glBlendFunc(oglw->blendingSrc, oglw->blendingDst);
        
        oglw->alphaTestEnabled=oglw->alphaTestEnabledRequested=false;
        #if defined(EGLW_GLES1)
        glDisable(GL_ALPHA_TEST);
        #else
        // TODO
        #endif

        oglw->depthTestEnabled=oglw->depthTestEnabledRequested=false;
        glDisable(GL_DEPTH_TEST);

        oglw->stencilTestEnabled=oglw->stencilTestEnabledRequested=false;
        glDisable(GL_STENCIL_TEST);
        
        oglw->depthWriteEnabled=oglw->depthWriteEnabledRequested=true;
        glDepthMask(GL_TRUE);

        oglw->beginFlag=false;
        oglw->primitive=GL_TRIANGLES;
        for (int i=0; i<Array_Nb; i++) {
            OpenGLWrapperArray *array=&oglw->arrays[i];
            array->enabled=false;
            array->size=4;
            array->type=GL_FLOAT;
            array->stride=0;
            array->pointer=NULL;
        }

        #if defined(BUFFER_OBJECT_USED)
        glGenBuffers(1, &oglw->bufferId);
        #endif
        
        oglwReserveVertices(oglw, DEFAULT_VERTEX_CAPACITY);
        oglwReserveIndices(oglw, DEFAULT_INDEX_CAPACITY);
    }

	return false;
on_error:
	oglwDestroy();
    return true;
}

void oglwDestroy() {
    OpenGLWrapper *oglw = l_openGLWrapper;
	if (oglw!=NULL) {
        #if defined(EGLW_GLES2)
        oglwFinalizeShaders(oglw);
        #endif

        oglwCleanupArrays(oglw);

        #if defined(BUFFER_OBJECT_USED)
        glDeleteBuffers(1, &oglw->bufferId);
        #endif

		free(oglw->vertices);
		free(oglw->indices);
		free(oglw);
		l_openGLWrapper=NULL;
	}
}

bool oglwIsCreated() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    return oglw != NULL;
}

//--------------------------------------------------------------------------------
// Viewport.
//--------------------------------------------------------------------------------
void oglwSetViewport(int x, int y, int w, int h)
{
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (oglw->viewport.x != x || oglw->viewport.y != y || oglw->viewport.width != w || oglw->viewport.height != h)
    {
        oglw->viewport.x = x;
        oglw->viewport.y = y;
        oglw->viewport.width = w;
        oglw->viewport.height = h;
        glViewport(x, y, w, h);
    }
}

void oglwSetDepthRange(float depthNear, float depthFar)
{
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (oglw->viewport.depthNear != depthNear || oglw->viewport.depthFar != depthFar)
    {
        oglw->viewport.depthNear = depthNear;
        oglw->viewport.depthFar = depthFar;
        glDepthRangef(depthNear, depthFar);
    }
}

//--------------------------------------------------------------------------------
// Matrix stacks.
//--------------------------------------------------------------------------------
#if defined(EGLW_GLES2)

OglwMatrixStack* oglwGetMatrixStack(OpenGLWrapper *oglw)
{
    OglwMatrixStack *matrixStack;
    switch (oglw->matrixMode) {
    default:
    case GL_MODELVIEW:
        matrixStack = &oglw->modelViewStack;
        break;
    case GL_PROJECTION:
        matrixStack = &oglw->projectionStack;
        break;
    }
    return matrixStack;
}
#endif

void oglwGetMatrix(GLenum matrixMode, GLfloat *matrix)
{
    #if defined(EGLW_GLES1)
    switch (matrixMode) {
    default:
    case GL_MODELVIEW:
        glGetFloatv(GL_MODELVIEW_MATRIX, matrix);
        break;
    case GL_PROJECTION:
        glGetFloatv(GL_PROJECTION_MATRIX, matrix);
        break;
    }  
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack;
    switch (matrixMode) {
    default:
    case GL_MODELVIEW:
        matrixStack = &oglw->modelViewStack;
        break;
    case GL_PROJECTION:
        matrixStack = &oglw->projectionStack;
        break;
    }
    int depth = matrixStack->depth;
    float *matrices = matrixStack->matrices;
    Matrix4x4_transpose(matrix, &matrices[depth * 16]);
    #endif
}

void oglwMatrixMode(GLenum matrixMode) {
    #if defined(EGLW_GLES1)
    glMatrixMode(matrixMode);
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->matrixMode = matrixMode;
    #endif
}

void oglwPushMatrix() {
    #if defined(EGLW_GLES1)
    glPushMatrix();
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    if (depth + 1 >= matrixStack->capacity) return;
    float *matrices = matrixStack->matrices;
    Matrix4x4_copy(&matrices[(depth + 1) * 16], &matrices[depth * 16]);
    matrixStack->depth = depth + 1;
    oglw->transformationDirty = true;
    #endif
}

void oglwPopMatrix() {
    #if defined(EGLW_GLES1)
    glPopMatrix();
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    if (depth <= 0) return;
    matrixStack->depth = depth - 1;
    oglw->transformationDirty = true;
    #endif
}

void oglwLoadMatrix(const GLfloat *matrix)
{
    #if defined(EGLW_GLES1)
    glLoadMatrixf(matrix);
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = matrixStack->matrices;
    Matrix4x4_transpose(&matrices[depth * 16], matrix);
    oglw->transformationDirty = true;
    #endif
}

void oglwLoadIdentity() {
    #if defined(EGLW_GLES1)
    glLoadIdentity();
    #else
    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = matrixStack->matrices;
    Matrix4x4_setIdentity(&matrices[depth * 16]);
    oglw->transformationDirty = true;
    #endif
}

void oglwFrustum(float left, float right, float bottom, float top, float zNear, float zFar) {
    #if defined(EGLW_GLES1)
    glFrustumf(left, right, bottom, top, zNear, zFar);
    #else
    if (zNear<=0.0f || zFar<=0.0f || left==right || bottom==top || zNear==zFar) return;

    float m[16];
    Matrix4x4_setNull(m);
    m[0*4+0]=(2.0f*zNear)/(right-left); // 2*near/(right-left)
    m[1*4+1]=(2.0f*zNear)/(top-bottom); // 2*near/(top-bottom)
    m[0*4+2]=(right+left)/(right-left); // (right+left)/(right-left)
    m[1*4+2]=(top+bottom)/(top-bottom); // (top+bottom)/(top-bottom)
    m[2*4+2]=-(zFar+zNear)/(zFar-zNear); // -(far+near)/(far-near)
    m[3*4+2]=-1.0f;
    m[2*4+3]=(-2.0f*zFar*zNear)/(zFar-zNear); // -2*far*near/(far-near)

    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = &matrixStack->matrices[depth * 16];
    Matrix4x4_mulPost(matrices, m);
    oglw->transformationDirty = true;
    #endif
}

void oglwOrtho(float left, float right, float bottom, float top, float zNear, float zFar) {
    #if defined(EGLW_GLES1)
    glOrthof(left, right, bottom, top, zNear, zFar);
    #else
    if (left==right || bottom==top || zNear==zFar) return;

    float m[16];
    Matrix4x4_setNull(m);
    m[0*4+0]=2.0f/(right-left);
    m[1*4+1]=2.0f/(top-bottom);
    m[2*4+2]=-2.0f/(zFar-zNear);
    m[0*4+3]=-(right+left)/(right-left);
    m[1*4+3]=-(top+bottom)/(top-bottom);
    m[2*4+3]=-(zFar+zNear)/(zFar-zNear);
    m[3*4+3]=1.0f;

    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = &matrixStack->matrices[depth * 16];
    Matrix4x4_mulPost(matrices, m);
    oglw->transformationDirty = true;
    #endif
}

void oglwTranslate(float x, float y, float z) {
    #if defined(EGLW_GLES1)
    glTranslatef(x, y, z);
    #else

    float m[16];
    Matrix4x4_setIdentity(m);
    m[0*4+3]=x;
    m[1*4+3]=y;
    m[2*4+3]=z;

    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = &matrixStack->matrices[depth * 16];
    Matrix4x4_mulPost(matrices, m);
    oglw->transformationDirty = true;
    #endif
}

void oglwScale(float x, float y, float z) {
    #if defined(EGLW_GLES1)
    glScalef(x, y, z);
    #else

    float m[16];
    Matrix4x4_setIdentity(m);
    m[0*4+0]=x;
    m[1*4+1]=y;
    m[2*4+2]=z;

    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = &matrixStack->matrices[depth * 16];
    Matrix4x4_mulPost(matrices, m);
    oglw->transformationDirty = true;
    #endif
}

void oglwRotate(float angle, float x, float y, float z) {
    #if defined(EGLW_GLES1)
    glRotatef(angle, x, y, z);
    #else

    float squaredNorm = x*x+y*y+z*z;
    if (squaredNorm == 0.0f) return;
    float ooNorm = 1.0f/sqrtf(squaredNorm);
    x*=ooNorm;
    y*=ooNorm;
    z*=ooNorm;

    float angleRad=fmodf(angle/360.0f, 1.0f) * 2.0f * OGLW_PI;
    float c=cosf(angleRad), s=sinf(angleRad), oneLessC=1.0f-c;
    float xx=x*x, xy=x*y, xz=x*z, xs=x*s;
    float yy=y*y, yz=y*z, ys=y*s;
    float zz=z*z, zs=z*s;
    float m[16];
    Matrix4x4_setIdentity(m);
    m[0*4+0]=xx*oneLessC+c;  // xx*(1-c)+c
    m[1*4+0]=xy*oneLessC+zs; // xy*(1-c)+zs
    m[2*4+0]=xz*oneLessC-ys; // xz*(1-c)-ys
    m[0*4+1]=xy*oneLessC-zs; // xy*(1-c)-zs
    m[1*4+1]=yy*oneLessC+c;  // yy*(1-c)+c
    m[2*4+1]=yz*oneLessC+xs; // yz*(1-c)+xs
    m[0*4+2]=xz*oneLessC+ys; // xz*(1-c)+ys
    m[1*4+2]=yz*oneLessC-xs; // yz*(1-c)-xs
    m[2*4+2]=zz*oneLessC+c;  // zz*(1-c)+c

    OpenGLWrapper *oglw = l_openGLWrapper;
    OglwMatrixStack *matrixStack = oglwGetMatrixStack(oglw);
    int depth = matrixStack->depth;
    float *matrices = &matrixStack->matrices[depth * 16];
    Matrix4x4_mulPost(matrices, m);
    oglw->transformationDirty = true;
    #endif
}

//--------------------------------------------------------------------------------
// Shading.
//--------------------------------------------------------------------------------
void oglwEnableSmoothShading(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->smoothShadingEnabledRequested = flag;
}

void oglwSetCurrentTextureUnitForced(int unit) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->textureUnitRequested=unit;
    if (oglw->textureUnit!=unit) {
        oglw->textureUnit=unit;
        glActiveTexture(GL_TEXTURE0 + unit);
    }
}

void oglwSetCurrentTextureUnit(int unit) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->textureUnitRequested=unit;
}
 
void oglwBindTextureForced(int unit, GLuint texture) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (oglw->textureUnit != unit) {
        oglw->textureUnit = unit;
        glActiveTexture(GL_TEXTURE0 + unit);
    }

    OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[unit];
    tu->textureRequested = texture;
    if (tu->texture != texture) {
        tu->texture = texture;
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    
    unit = oglw->textureUnitRequested;
    if (oglw->textureUnit != unit) {
        oglw->textureUnit = unit;
        glActiveTexture(GL_TEXTURE0 + unit);
    }
}

void oglwBindTexture(int unit, GLuint texture) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[unit];
    tu->textureRequested = texture;
}

void oglwEnableTexturing(int unit, bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[unit];
    tu->texturingEnabledRequested = flag;
}

void oglwSetTextureBlending(int unit, GLint mode) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[unit];
    tu->blendingRequested = mode;
}

//--------------------------------------------------------------------------------
// ROPs.
//--------------------------------------------------------------------------------
void oglwEnableBlending(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->blendingEnabledRequested = flag;
}

void oglwSetBlendingFunction(GLenum src, GLenum dst) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->blendingSrcRequested = src;
    oglw->blendingDstRequested = dst;
}

void oglwEnableAlphaTest(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->alphaTestEnabledRequested = flag;
}

void oglwSetAlphaFunc(GLenum mode, GLfloat threshold)
{
    #if defined(EGLW_GLES1)
	glAlphaFunc(mode, threshold);
    #else
    // TODO
    #endif
}

void oglwEnableDepthTest(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->depthTestEnabledRequested = flag;
}

void oglwEnableStencilTest(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->stencilTestEnabledRequested = flag;
}

void oglwEnableDepthWrite(bool flag) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->depthWriteEnabledRequested = flag;
}

//--------------------------------------------------------------------------------
// Clearing.
//--------------------------------------------------------------------------------
void oglwClear(GLbitfield mask) {
    oglwUpdateStateWriteMask();
    glClear(mask);
}

//--------------------------------------------------------------------------------
// Arrays.
//--------------------------------------------------------------------------------
/*
static int arrayToIndex(GLenum array) {
    int index=0;
    switch (array) {
    case GL_VERTEX_ARRAY: index = Array_Position; break;
    case GL_COLOR_ARRAY: index = Array_Color; break;
    case GL_TEXTURE_COORD_ARRAY+0: index = Array_TexCoord0; break;
    case GL_TEXTURE_COORD_ARRAY+1: index = Array_TexCoord1; break;
    }
    return index;
}

void oglwEnableClientState(GLenum array) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int arrayIndex = arrayToIndex(array);
    OpenGLWrapperArray *oglwArray=&oglw->arrays[arrayIndex];
    oglwArray->enabled = true;
    glEnableClientState(array);
}

void oglwDisableClientState(GLenum array) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int arrayIndex = arrayToIndex(array);
    OpenGLWrapperArray *oglwArray=&oglw->arrays[arrayIndex];
    oglwArray->enabled = false;
    glDisableClientState(array);
}

void oglwVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    OpenGLWrapperArray *array=&oglw->arrays[Array_Position];
    array->size=size;
    array->type=type;
    array->stride=stride;
    array->pointer=pointer;
    glVertexPointer(size, type, stride, pointer);
}

void oglwColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    OpenGLWrapperArray *array=&oglw->arrays[Array_Color];
    array->size=size;
    array->type=type;
    array->stride=stride;
    array->pointer=pointer;
    glColorPointer(size, type, stride, pointer);
}
*/

static void oglwSetupArrays(OpenGLWrapper *oglw) {
    #if defined(BUFFER_OBJECT_USED)
    glBindBuffer(GL_ARRAY_BUFFER, oglw->bufferId);
    glBufferData(GL_ARRAY_BUFFER, oglw->verticesCapacity * sizeof(OglwVertex), NULL, GL_DYNAMIC_DRAW); // Should be GL_STREAM_DRAW but only for GLES 2.
    #endif

    if (!oglw->arrays[Array_Position].enabled) {
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].position[0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].position[0];
        #endif
        #if defined(EGLW_GLES1)
        glVertexPointer(4, GL_FLOAT, sizeof(OglwVertex), p);
        glEnableClientState(GL_VERTEX_ARRAY);
        #else
        glVertexAttribPointer(oglw->a_position, 4, GL_FLOAT, GL_FALSE, sizeof(OglwVertex), p);
        glEnableVertexAttribArray(oglw->a_position);
        #endif
    }
    if (!oglw->arrays[Array_Color].enabled) {
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].color[0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].color[0];
        #endif
        #if defined(EGLW_GLES1)
        glColorPointer(4, GL_FLOAT, sizeof(OglwVertex), p);
        glEnableClientState(GL_COLOR_ARRAY);
        #else
        glVertexAttribPointer(oglw->a_color, 4, GL_FLOAT, GL_TRUE, sizeof(OglwVertex), p);
        glEnableVertexAttribArray(oglw->a_color);
        #endif
    }
    if (!oglw->arrays[Array_TexCoord0].enabled) {
        #if defined(BUFFER_OBJECT_USED)
		void * p = (void*)((void*)&oglw->vertices[0].texCoord[0][0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].texCoord[0][0];
        #endif
        #if defined(EGLW_GLES1)
        glClientActiveTexture(GL_TEXTURE0);
        glTexCoordPointer(4, GL_FLOAT, sizeof(OglwVertex), p);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        #else
        glVertexAttribPointer(oglw->a_texcoord0, 4, GL_FLOAT, GL_FALSE, sizeof(OglwVertex), p);
        glEnableVertexAttribArray(oglw->a_texcoord0);
        #endif
    }
    if (!oglw->arrays[Array_TexCoord1].enabled) {
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].texCoord[1][0] - (void*)&oglw->vertices[0]);
        #else
        void *p = &oglw->vertices[0].texCoord[1][0];
        #endif
        #if defined(EGLW_GLES1)
        glClientActiveTexture(GL_TEXTURE1);
        glTexCoordPointer(4, GL_FLOAT, sizeof(OglwVertex), p);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glClientActiveTexture(GL_TEXTURE0);
        #else
        glVertexAttribPointer(oglw->a_texcoord1, 4, GL_FLOAT, GL_FALSE, sizeof(OglwVertex), p);
        glEnableVertexAttribArray(oglw->a_texcoord1);
        #endif
    }
}

static void oglwCleanupArrays(OpenGLWrapper *oglw) {
    #if defined(BUFFER_OBJECT_USED)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    #endif
    if (!oglw->arrays[Array_Position].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_Position];
        #if defined(EGLW_GLES1)
        glDisableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(array->size, array->type, array->stride, array->pointer);
        #else
        glDisableVertexAttribArray(oglw->a_position);
        glVertexAttribPointer(oglw->a_position, array->size, array->type, GL_FALSE, array->stride, array->pointer);
        #endif
    }
    if (!oglw->arrays[Array_Color].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_Color];
        #if defined(EGLW_GLES1)
        glDisableClientState(GL_COLOR_ARRAY);
        glColorPointer(array->size, array->type, array->stride, array->pointer);
        #else
        glDisableVertexAttribArray(oglw->a_color);
        glVertexAttribPointer(oglw->a_color, array->size, array->type, GL_TRUE, array->stride, array->pointer);
        #endif
    }
    if (!oglw->arrays[Array_TexCoord0].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_TexCoord0];
        #if defined(EGLW_GLES1)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(array->size, array->type, array->stride, array->pointer);
        #else
        glDisableVertexAttribArray(oglw->a_texcoord0);
        glVertexAttribPointer(oglw->a_texcoord0, array->size, array->type, GL_FALSE, array->stride, array->pointer);
        #endif
    }
    if (!oglw->arrays[Array_TexCoord1].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_TexCoord1];
        #if defined(EGLW_GLES1)
        glClientActiveTexture(GL_TEXTURE1);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(array->size, array->type, array->stride, array->pointer);
        glClientActiveTexture(GL_TEXTURE0);
        #else
        glDisableVertexAttribArray(oglw->a_texcoord1);
        glVertexAttribPointer(oglw->a_texcoord1, array->size, array->type, GL_FALSE, array->stride, array->pointer);
        #endif
    }
}

//--------------------------------------------------------------------------------
// Drawing.
//--------------------------------------------------------------------------------
static bool oglwReserveIndices(OpenGLWrapper *oglw, int indicesCapacityMin) {
    if (indicesCapacityMin < 0) return true;
    int indicesCapacity=oglw->indicesCapacity;
    if (indicesCapacity<indicesCapacityMin) {
        indicesCapacity=indicesCapacityMin;
        oglw->indicesCapacity=indicesCapacity;
        GLushort *indices = oglw->indices;
        GLushort *indicesNew = malloc(sizeof(GLushort)*indicesCapacity);
        if (indicesNew==NULL) goto on_error;
        int indicesLength=oglw->indicesLength;
        for (int i=0; i<indicesLength; i++) {
            indicesNew[i]=indices[i];
        }
        free(indices);
        oglw->indices=indicesNew;
    }
    return false;
on_error:
    free(oglw->indices);
    oglw->indicesCapacity=0;
    oglw->indicesLength=0;
    oglw->indices=NULL;
    return true;
}

static bool oglwReserveVertices(OpenGLWrapper *oglw, int verticesCapacityMin) {
    if (verticesCapacityMin < 0) return true;
    int verticesCapacity=oglw->verticesCapacity;
    if (verticesCapacity<verticesCapacityMin) {
        verticesCapacity=verticesCapacityMin;
        oglw->verticesCapacity=verticesCapacity;
        OglwVertex *vertices = oglw->vertices;
        OglwVertex *verticesNew = malloc(sizeof(OglwVertex)*verticesCapacity);
        if (verticesNew==NULL) goto on_error;
        int verticesLength=oglw->verticesLength;
        for (int i=0; i<verticesLength; i++) {
            verticesNew[i]=vertices[i];
        }
        free(vertices);
        oglw->vertices=verticesNew;
        
        oglwSetupArrays(oglw);
    }
    return false;
on_error:
    free(oglw->vertices);
    oglw->verticesCapacity=0;
    oglw->verticesLength=0;
    oglw->vertices=NULL;
    return true;
}

void oglwPointSize(float size) {
    #if defined(EGLW_GLES1)
    glPointSize(size);
    #else
    // TODO
    #endif
}

void oglwBegin(GLenum primitive) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->beginFlag=true;
    oglw->primitive = primitive;
}

void oglwEnd() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (!oglw->beginFlag) return;
    
    if (oglw->verticesLength > 0)
    {
        GLenum primitive=oglw->primitive;
        switch (oglw->primitive) {
        default:
            break;
        case GL_QUADS:
            {
                primitive = GL_TRIANGLES;
                int quadNb = oglw->verticesLength>>2;
                int indicesNb = quadNb*6;
                if (oglwReserveIndices(oglw, indicesNb)) {
                    oglw->indicesLength=0;
                    oglw->verticesLength=0;
                } else {
                    oglw->indicesLength=indicesNb;
                    GLushort *indices=oglw->indices;
                    for (int i=0; i<quadNb; i++, indices+=6) {
                        int index=i<<2;
                        indices[0]=index+0;
                        indices[1]=index+1;
                        indices[2]=index+2;
                        indices[3]=index+0;
                        indices[4]=index+2;
                        indices[5]=index+3;
                    }
                }
            }
            break;
        case GL_POLYGON:
            primitive = GL_TRIANGLE_FAN;
            break;
        }

        oglwUpdateState();

        #if defined(BUFFER_OBJECT_USED)
        glBufferSubData(GL_ARRAY_BUFFER, 0, oglw->verticesLength * sizeof(OglwVertex), oglw->vertices);
        #endif
        
        if (oglw->indicesLength>0) {
            glDrawElements(primitive, oglw->indicesLength, GL_UNSIGNED_SHORT, oglw->indices);
        } else {
            glDrawArrays(primitive, 0, oglw->verticesLength);
        }
    }
    
    oglwReset();
}

void oglwUpdateState() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    
    #if defined(EGLW_GLES2)
    if (oglw->transformationDirty)
    {
        oglw->transformationDirty = false;
        OglwMatrixStack *projectionStack = &oglw->projectionStack;
        OglwMatrixStack *modelViewStack = &oglw->modelViewStack;
        float *projectionMatrix = &projectionStack->matrices[projectionStack->depth * 16];
        float *modelViewMatrix = &modelViewStack->matrices[modelViewStack->depth * 16];
        float *transformationMatrix = oglw->transformation;
        #if 1
        Matrix4x4_mul(transformationMatrix, projectionMatrix, modelViewMatrix);
        #else
        float transposeMatrix[16];
        Matrix4x4_mul(transposeMatrix, projectionMatrix, modelViewMatrix);
        Matrix4x4_transpose(transformationMatrix, transposeMatrix);
        #endif
        glUniformMatrix4fv(oglw->u_transformation, 1, GL_FALSE, transformationMatrix);
    }
    #endif
    
    if (oglw->smoothShadingEnabled!=oglw->smoothShadingEnabledRequested) {
        oglw->smoothShadingEnabled=oglw->smoothShadingEnabledRequested;
        #if defined(EGLW_GLES1)
        if (oglw->smoothShadingEnabledRequested)
            glShadeModel(GL_SMOOTH);
        else
            glShadeModel(GL_FLAT);
        #else
        // TODO
        #endif
    }
    
    int unit = oglw->textureUnitRequested ^ 1;
    for (int i = 0; i < 2; i++) {
        OpenGLWrapperTextureUnit *tu = &oglw->textureUnits[unit];
        if (tu->texturingEnabled!=tu->texturingEnabledRequested || tu->blending!=tu->blendingRequested || tu->texture!=tu->textureRequested)
        {
            if (oglw->textureUnit!=unit)
            {
                oglw->textureUnit=unit;
                glActiveTexture(GL_TEXTURE0 + unit);
            }
            if (tu->texturingEnabled!=tu->texturingEnabledRequested) {
                tu->texturingEnabled=tu->texturingEnabledRequested;
                #if defined(EGLW_GLES1)
                if (tu->texturingEnabledRequested)
                    glEnable(GL_TEXTURE_2D);
                else
                    glDisable(GL_TEXTURE_2D);
                #else
                if (unit == 0)
                    glUniform1i(oglw->u_tex0Enabled, tu->texturingEnabledRequested);
                else
                    glUniform1i(oglw->u_tex1Enabled, tu->texturingEnabledRequested);
                #endif
            }
//            if (tu->texturingEnabled)
            {
                if (tu->blending!=tu->blendingRequested) {
                    tu->blending=tu->blendingRequested;
                    #if defined(EGLW_GLES1)
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tu->blendingRequested);
                    #else
                    if (unit == 0)
                        glUniform1i(oglw->u_tex0BlendingEnabled, tu->blending == GL_MODULATE);
                    else
                        glUniform1i(oglw->u_tex1BlendingEnabled, tu->blending == GL_MODULATE);
                    #endif
                }
                if (tu->texture!=tu->textureRequested) {
                    tu->texture=tu->textureRequested;
                    glBindTexture(GL_TEXTURE_2D, tu->textureRequested);
                }
            }
        }
        unit^=1;
    }
    /*
    unit = oglw->textureUnitRequested;
    if (oglw->textureUnit!=unit) {
        oglw->textureUnit=unit;
        glActiveTexture(GL_TEXTURE0 + unit);
    }
    */
    
    if (oglw->blendingEnabled!=oglw->blendingEnabledRequested) {
        oglw->blendingEnabled=oglw->blendingEnabledRequested;
        if (oglw->blendingEnabledRequested)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);
    }

    if (oglw->blendingEnabled) {
        if (oglw->blendingSrc!=oglw->blendingSrcRequested || oglw->blendingDst!=oglw->blendingDstRequested) {
            oglw->blendingSrc=oglw->blendingSrcRequested;
            oglw->blendingDst=oglw->blendingDstRequested;
            glBlendFunc(oglw->blendingSrcRequested, oglw->blendingDstRequested);
        }
    }
    
    if (oglw->alphaTestEnabled!=oglw->alphaTestEnabledRequested) {
        oglw->alphaTestEnabled=oglw->alphaTestEnabledRequested;
        #if defined(EGLW_GLES1)
        if (oglw->alphaTestEnabledRequested) {
            glEnable(GL_ALPHA_TEST);
           	glAlphaFunc(GL_GREATER, 0.666f);
        } else
            glDisable(GL_ALPHA_TEST);
        #else
        glUniform1f(oglw->u_alphaThreshold, oglw->alphaTestEnabledRequested ? 0.666f : 0.0f);
        #endif
    }

    if (oglw->depthTestEnabled!=oglw->depthTestEnabledRequested) {
        oglw->depthTestEnabled=oglw->depthTestEnabledRequested;
        if (oglw->depthTestEnabledRequested)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    if (oglw->stencilTestEnabled!=oglw->stencilTestEnabledRequested) {
        oglw->stencilTestEnabled=oglw->stencilTestEnabledRequested;
        if (oglw->stencilTestEnabledRequested)
            glEnable(GL_STENCIL_TEST);
        else
            glDisable(GL_STENCIL_TEST);
    }

    oglwUpdateStateWriteMask();
}

void oglwUpdateStateWriteMask() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (oglw->depthWriteEnabled!=oglw->depthWriteEnabledRequested) {
        oglw->depthWriteEnabled=oglw->depthWriteEnabledRequested;
        if (oglw->depthWriteEnabledRequested)
            glDepthMask(GL_TRUE);
        else
            glDepthMask(GL_FALSE);
    }
}

void oglwReset() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->beginFlag=false;
    oglw->verticesLength=0;
    oglw->indicesLength=0;
}

bool oglwIsEmpty() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    return (oglw->verticesLength <= 0);
}

OglwVertex* oglwAllocateVertex(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (!oglw->beginFlag || vertexNb <= 0) return NULL;
    int verticesCapacity=oglw->verticesCapacity;
    int verticesLength=oglw->verticesLength;
    int verticesLengthNew=verticesLength+vertexNb;
    if (verticesLengthNew>verticesCapacity) {
        if (verticesLengthNew < (DEFAULT_VERTEX_CAPACITY>>1)) verticesCapacity=DEFAULT_VERTEX_CAPACITY;
        else verticesCapacity=2 * verticesLengthNew;
        if (oglwReserveVertices(oglw, verticesCapacity)) return NULL;
    }
    oglw->verticesLength=verticesLengthNew;
    return &oglw->vertices[verticesLength];
}

GLushort* oglwAllocateIndex(int indexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (!oglw->beginFlag || indexNb <= 0) return NULL;
    int indicesCapacity=oglw->indicesCapacity;
    int indicesLength=oglw->indicesLength;
    int indicesLengthNew=indicesLength+indexNb;
    if (indicesLengthNew>indicesCapacity) {
        if (indicesLengthNew < (DEFAULT_INDEX_CAPACITY>>1)) indicesCapacity=DEFAULT_INDEX_CAPACITY;
        else indicesCapacity = 2 * indicesLengthNew;
        if (oglwReserveIndices(oglw, indicesCapacity)) return NULL;
    }
    oglw->indicesLength=indicesLengthNew;
    return &oglw->indices[indicesLength];
}

OglwVertex* oglwAllocateLineStrip(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int lineNb = vertexNb-1;
    if (lineNb <= 0)
        return NULL;

    GLushort *index = oglwAllocateIndex(lineNb*2);
    if (index == NULL) goto on_error;
    int vertexLengthLast = oglw->verticesLength;
    for (int ti = 0; ti < lineNb; ti++, index+=2) {
        int vi = vertexLengthLast + ti;
        index[0] = vi;
        index[1] = vi + 1;
    }

    OglwVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OglwVertex* oglwAllocateLineLoop(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int lineNb = vertexNb-1;
    if (lineNb <= 0)
        return NULL;

    GLushort *index = oglwAllocateIndex((lineNb+1)*2);
    if (index == NULL) goto on_error;
    int vertexLengthLast = oglw->verticesLength;
    for (int ti = 0; ti < lineNb; ti++, index+=2) {
        int vi = vertexLengthLast + ti;
        index[0] = vi;
        index[1] = vi + 1;
    }
    index[0] = vertexLengthLast + lineNb;
    index[1] = vertexLengthLast;

    OglwVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OglwVertex* oglwAllocateQuad(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int quadNb = vertexNb>>2;
    if (quadNb <= 0)
        return NULL;

    int vertexLengthLast = oglw->verticesLength;
    GLushort *index = oglwAllocateIndex(quadNb*6);
    if (index == NULL) goto on_error;
    for (int qi = 0; qi < quadNb; qi++, index+=6) {
        int vi = vertexLengthLast + (qi<<2);
        index[0] = vi + 0;
        index[1] = vi + 1;
        index[2] = vi + 2;
        index[3] = vi + 0;
        index[4] = vi + 2;
        index[5] = vi + 3;
    }

    OglwVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OglwVertex* oglwAllocateTriangleFan(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int triangleNb = vertexNb-2;
    if (triangleNb <= 0)
        return NULL;

    int vertexLengthLast = oglw->verticesLength;
    GLushort *index = oglwAllocateIndex(triangleNb*3);
    if (index == NULL) goto on_error;
    for (int ti = 0; ti < triangleNb; ti++, index+=3) {
        int vi = vertexLengthLast + ti;
        index[0] = vertexLengthLast;
        index[1] = vi + 1;
        index[2] = vi + 2;
    }

    OglwVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OglwVertex* oglwAllocateTriangleStrip(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int triangleNb = vertexNb-2;
    if (triangleNb <= 0)
        return NULL;

    GLushort *index = oglwAllocateIndex(triangleNb*3);
    if (index == NULL) goto on_error;
    int vertexLengthLast = oglw->verticesLength;
    int swap = 0;
    for (int ti = 0; ti < triangleNb; ti++, index+=3) {
        int vi = vertexLengthLast + ti;
        index[0] = vi;
        index[1] = vi + 1 + swap;
        index[2] = vi + 2 - swap;
        swap ^= 1;
    }

    OglwVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

static void olgwAddVertex(OpenGLWrapper *oglw) {
    if (!oglw->beginFlag) return;
    int verticesCapacity=oglw->verticesCapacity;
    int verticesLength=oglw->verticesLength;
    if (verticesLength>=verticesCapacity) {
        if (verticesCapacity<=0) verticesCapacity=DEFAULT_VERTEX_CAPACITY;
        else verticesCapacity*=2;
        if (oglwReserveVertices(oglw, verticesCapacity)) return;
    }
    oglw->vertices[verticesLength]=oglw->currentVertex;
    oglw->verticesLength=verticesLength+1;
}

void oglwVertex2f(GLfloat x, GLfloat y) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=x;
    oglw->currentVertex.position[1]=y;
    olgwAddVertex(oglw);
}

void oglwVertex2i(GLint x, GLint y) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=(GLfloat)x;
    oglw->currentVertex.position[1]=(GLfloat)y;
    olgwAddVertex(oglw);
}

void oglwVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=x;
    oglw->currentVertex.position[1]=y;
    oglw->currentVertex.position[2]=z;
    olgwAddVertex(oglw);
}

void oglwVertex3fv(const GLfloat *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=v[0];
    oglw->currentVertex.position[1]=v[1];
    oglw->currentVertex.position[2]=v[2];
    olgwAddVertex(oglw);
}

void oglwGetColor(GLfloat *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    GLfloat *c = oglw->currentVertex.color;
    v[0] = c[0];
    v[1] = c[1];
    v[2] = c[2];
    v[3] = c[3];
}

void oglwColor3f(GLfloat r, GLfloat g, GLfloat b) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.color[0]=r;
    oglw->currentVertex.color[1]=g;
    oglw->currentVertex.color[2]=b;
}

void oglwColor3fv(const GLfloat *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.color[0]=v[0];
    oglw->currentVertex.color[1]=v[1];
    oglw->currentVertex.color[2]=v[2];
}

void oglwColor4ubv(const GLubyte *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    const float n=1.0f/255.0f; 
    oglw->currentVertex.color[0]=v[0]*n;
    oglw->currentVertex.color[1]=v[1]*n;
    oglw->currentVertex.color[2]=v[2]*n;
}

void oglwColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.color[0]=r;
    oglw->currentVertex.color[1]=g;
    oglw->currentVertex.color[2]=b;
    oglw->currentVertex.color[3]=a;
}

void oglwColor4fv(const GLfloat *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.color[0]=v[0];
    oglw->currentVertex.color[1]=v[1];
    oglw->currentVertex.color[2]=v[2];
    oglw->currentVertex.color[3]=v[3];
}

void oglwTexCoord2f(GLfloat u, GLfloat v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.texCoord[0][0]=u;
    oglw->currentVertex.texCoord[0][1]=v;
}

void oglwMultiTexCoord2f(GLenum unit, GLfloat u, GLfloat v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int unitIndex = unit - GL_TEXTURE0;
    oglw->currentVertex.texCoord[unitIndex][0]=u;
    oglw->currentVertex.texCoord[unitIndex][1]=v;
}
