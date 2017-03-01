#include "OpenGLES/OpenGLWrapper.h"

#include <stdlib.h>

//#define BUFFER_OBJECT_USED

enum {
    Array_Position,
    Array_Color,
    Array_TexCoord0,
    Array_TexCoord1,
    Array_Nb
};

typedef struct OpenGLWrapper_ OpenGLWrapper;

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
    #if defined(GLES2)
    
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

    OpenGLWrapperVertex currentVertex;
    
    int verticesCapacity;
    int verticesLength;
    OpenGLWrapperVertex *vertices;
    
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

static void oglwSetupArrays();
static void oglwCleanupArrays();
static bool oglwReserveVertices(int verticesCapacityMin);
static bool oglwReserveIndices(int indicesCapacityMin);

static OpenGLWrapper *l_openGLWrapper = NULL;

//--------------------------------------------------------------------------------
// Shaders.
//--------------------------------------------------------------------------------
#if defined(GLES2)

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

        oglw->smoothShadingEnabled=oglw->smoothShadingEnabledRequested=true;
        #if defined(GLES1)
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
            glDisable(GL_TEXTURE_2D);
            
            tu->blending=tu->blendingRequested=GL_MODULATE;
            #if defined(GLES1)
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tu->blending);
            #else
            // TODO
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
        #if defined(GLES1)
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
        
        oglwReserveVertices(DEFAULT_VERTEX_CAPACITY);
        oglwReserveIndices(DEFAULT_INDEX_CAPACITY);
    }
	return false;
on_error:
	oglwDestroy();
    return true;
}

void oglwDestroy() {
    OpenGLWrapper *oglw = l_openGLWrapper;
	if (oglw!=NULL) {
        oglwCleanupArrays();

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
// Matrix stacks.
//--------------------------------------------------------------------------------
void oglwGetMatrix(GLenum matrixMode, GLfloat *matrix)
{
    #if defined(GLES1)
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
    // TODO
    #endif
}

void oglwMatrixMode(GLenum matrixMode) {
    #if defined(GLES1)
    glMatrixMode(matrixMode);
    #else
    // TODO
    #endif
}

void oglwPushMatrix() {
    #if defined(GLES1)
    glPushMatrix();
    #else
    // TODO
    #endif
}

void oglwPopMatrix() {
    #if defined(GLES1)
    glPopMatrix();
    #else
    // TODO
    #endif
}

void oglwLoadMatrix(const GLfloat *matrix)
{
    #if defined(GLES1)
    glLoadMatrixf(matrix);
    #else
    // TODO
    #endif
}

void oglwLoadIdentity() {
    #if defined(GLES1)
    glLoadIdentity();
    #else
    // TODO
    #endif
}

void oglwFrustum(float left, float right, float bottom, float top, float zNear, float zFar) {
    #if defined(GLES1)
    glFrustumf(left, right, bottom, top, zNear, zFar);
    #else
    // TODO
    #endif
}

void oglwOrtho(float left, float right, float bottom, float top, float zNear, float zFar) {
    #if defined(GLES1)
    glOrthof(left, right, bottom, top, zNear, zFar);
    #else
    // TODO
    #endif
}

void oglwTranslate(float x, float y, float z) {
    #if defined(GLES1)
    glTranslatef(x, y, z);
    #else
    // TODO
    #endif
}

void oglwScale(float x, float y, float z) {
    #if defined(GLES1)
    glScalef(x, y, z);
    #else
    // TODO
    #endif
}

void oglwRotate(float angle, float x, float y, float z) {
    #if defined(GLES1)
    glRotatef(angle, x, y, z);
    #else
    // TODO
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

/*
void oglwSetCurrentTextureUnit(int unit) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->textureUnitRequested=unit;
}
*/
 
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
    #if defined(GLES1)
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

static void oglwSetupArrays() {
    OpenGLWrapper *oglw = l_openGLWrapper;

    #if defined(BUFFER_OBJECT_USED)
    glBindBuffer(GL_ARRAY_BUFFER, oglw->bufferId);
    glBufferData(GL_ARRAY_BUFFER, oglw->verticesCapacity * sizeof(OpenGLWrapperVertex), NULL, GL_DYNAMIC_DRAW); // Should be GL_STREAM_DRAW but only for GLES 2.
    #endif

    #if defined(GLES1)
    if (!oglw->arrays[Array_Position].enabled) {
        glEnableClientState(GL_VERTEX_ARRAY);
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].position[0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].position[0];
        #endif
        glVertexPointer(4, GL_FLOAT, sizeof(OpenGLWrapperVertex), p);
    }
    if (!oglw->arrays[Array_Color].enabled) {
        glEnableClientState(GL_COLOR_ARRAY);
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].color[0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].color[0];
        #endif
        glColorPointer(4, GL_FLOAT, sizeof(OpenGLWrapperVertex), p);
    }
    if (!oglw->arrays[Array_TexCoord0].enabled) {
        glClientActiveTexture(GL_TEXTURE0);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        #if defined(BUFFER_OBJECT_USED)
		void * p = (void*)((void*)&oglw->vertices[0].texCoord[0][0] - (void*)&oglw->vertices[0]);
        #else
		void *p = &oglw->vertices[0].texCoord[0][0];
        #endif
        glTexCoordPointer(4, GL_FLOAT, sizeof(OpenGLWrapperVertex), p);
    }
    if (!oglw->arrays[Array_TexCoord1].enabled) {
        glClientActiveTexture(GL_TEXTURE1);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        #if defined(BUFFER_OBJECT_USED)
		void *p = (void*)((void*)&oglw->vertices[0].texCoord[1][0] - (void*)&oglw->vertices[0]);
        #else
        void *p = &oglw->vertices[0].texCoord[1][0];
        #endif
        glTexCoordPointer(4, GL_FLOAT, sizeof(OpenGLWrapperVertex), p);
        glClientActiveTexture(GL_TEXTURE0);
    }
    #else
    // TODO
    #endif
}

static void oglwCleanupArrays() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    #if defined(BUFFER_OBJECT_USED)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    #endif
    #if defined(GLES1)
    if (!oglw->arrays[Array_Position].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_Position];
        glDisableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(array->size, array->type, array->stride, array->pointer);
    }
    if (!oglw->arrays[Array_Color].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_Color];
        glDisableClientState(GL_COLOR_ARRAY);
        glColorPointer(array->size, array->type, array->stride, array->pointer);
    }
    if (!oglw->arrays[Array_TexCoord0].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_TexCoord0];
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(array->size, array->type, array->stride, array->pointer);
    }
    if (!oglw->arrays[Array_TexCoord1].enabled) {
        OpenGLWrapperArray *array=&oglw->arrays[Array_TexCoord1];
        glClientActiveTexture(GL_TEXTURE1);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(array->size, array->type, array->stride, array->pointer);
        glClientActiveTexture(GL_TEXTURE0);
    }
    #endif
}

//--------------------------------------------------------------------------------
// Drawing.
//--------------------------------------------------------------------------------
static bool oglwReserveIndices(int indicesCapacityMin) {
    OpenGLWrapper *oglw = l_openGLWrapper;
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

static bool oglwReserveVertices(int verticesCapacityMin) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (verticesCapacityMin < 0) return true;
    int verticesCapacity=oglw->verticesCapacity;
    if (verticesCapacity<verticesCapacityMin) {
        verticesCapacity=verticesCapacityMin;
        oglw->verticesCapacity=verticesCapacity;
        OpenGLWrapperVertex *vertices = oglw->vertices;
        OpenGLWrapperVertex *verticesNew = malloc(sizeof(OpenGLWrapperVertex)*verticesCapacity);
        if (verticesNew==NULL) goto on_error;
        int verticesLength=oglw->verticesLength;
        for (int i=0; i<verticesLength; i++) {
            verticesNew[i]=vertices[i];
        }
        free(vertices);
        oglw->vertices=verticesNew;
        
        oglwSetupArrays();
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
    #if defined(GLES1)
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
                if (oglwReserveIndices(indicesNb)) {
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
        glBufferSubData(GL_ARRAY_BUFFER, 0, oglw->verticesLength * sizeof(OpenGLWrapperVertex), oglw->vertices);
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
    
    if (oglw->smoothShadingEnabled!=oglw->smoothShadingEnabledRequested) {
        oglw->smoothShadingEnabled=oglw->smoothShadingEnabledRequested;
        #if defined(GLES1)
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
                if (tu->texturingEnabledRequested)
                    glEnable(GL_TEXTURE_2D);
                else
                    glDisable(GL_TEXTURE_2D);
            }
//            if (tu->texturingEnabled)
            {
                if (tu->blending!=tu->blendingRequested) {
                    tu->blending=tu->blendingRequested;
                    #if defined(GLES1)
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tu->blendingRequested);
                    #else
                    // TODO
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
        #if defined(GLES1)
        if (oglw->alphaTestEnabledRequested) {
            glEnable(GL_ALPHA_TEST);
           	glAlphaFunc(GL_GREATER, 0.666f);
        } else
            glDisable(GL_ALPHA_TEST);
        #else
        // TODO
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

OpenGLWrapperVertex* oglwAllocateVertex(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (!oglw->beginFlag || vertexNb <= 0) return NULL;
    int verticesCapacity=oglw->verticesCapacity;
    int verticesLength=oglw->verticesLength;
    int verticesLengthNew=verticesLength+vertexNb;
    if (verticesLengthNew>verticesCapacity) {
        if (verticesLengthNew < (DEFAULT_VERTEX_CAPACITY>>1)) verticesCapacity=DEFAULT_VERTEX_CAPACITY;
        else verticesCapacity=2 * verticesLengthNew;
        if (oglwReserveVertices(verticesCapacity)) return NULL;
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
        if (oglwReserveIndices(indicesCapacity)) return NULL;
    }
    oglw->indicesLength=indicesLengthNew;
    return &oglw->indices[indicesLength];
}

OpenGLWrapperVertex* oglwAllocateLineStrip(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int lineNb = vertexNb-1;

    GLushort *index = oglwAllocateIndex(lineNb*2);
    if (index == NULL) goto on_error;
    int vertexLengthLast = oglw->verticesLength;
    for (int ti = 0; ti < lineNb; ti++, index+=2) {
        int vi = vertexLengthLast + ti;
        index[0] = vi;
        index[1] = vi + 1;
    }

    OpenGLWrapperVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OpenGLWrapperVertex* oglwAllocateLineLoop(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int lineNb = vertexNb-1;

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

    OpenGLWrapperVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OpenGLWrapperVertex* oglwAllocateQuad(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int quadNb = vertexNb>>2;

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

    OpenGLWrapperVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OpenGLWrapperVertex* oglwAllocateTriangleFan(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int triangleNb = vertexNb-2;

    int vertexLengthLast = oglw->verticesLength;
    GLushort *index = oglwAllocateIndex(triangleNb*3);
    if (index == NULL) goto on_error;
    for (int ti = 0; ti < triangleNb; ti++, index+=3) {
        int vi = vertexLengthLast + ti;
        index[0] = vertexLengthLast;
        index[1] = vi + 1;
        index[2] = vi + 2;
    }

    OpenGLWrapperVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

OpenGLWrapperVertex* oglwAllocateTriangleStrip(int vertexNb) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    int triangleNb = vertexNb-2;

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

    OpenGLWrapperVertex *v = oglwAllocateVertex(vertexNb);
    if (v == NULL) goto on_error;
    return v;
on_error:
    oglw->indicesLength = 0;
    oglw->verticesLength = 0;
    return NULL;
}

static void olgwAddVertex() {
    OpenGLWrapper *oglw = l_openGLWrapper;
    if (!oglw->beginFlag) return;
    int verticesCapacity=oglw->verticesCapacity;
    int verticesLength=oglw->verticesLength;
    if (verticesLength>=verticesCapacity) {
        if (verticesCapacity<=0) verticesCapacity=DEFAULT_VERTEX_CAPACITY;
        else verticesCapacity*=2;
        if (oglwReserveVertices(verticesCapacity)) return;
    }
    oglw->vertices[verticesLength]=oglw->currentVertex;
    oglw->verticesLength=verticesLength+1;
}

void oglwVertex2f(GLfloat x, GLfloat y) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=x;
    oglw->currentVertex.position[1]=y;
    olgwAddVertex();
}

void oglwVertex2i(GLint x, GLint y) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=(GLfloat)x;
    oglw->currentVertex.position[1]=(GLfloat)y;
    olgwAddVertex();
}

void oglwVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=x;
    oglw->currentVertex.position[1]=y;
    oglw->currentVertex.position[2]=z;
    olgwAddVertex();
}

void oglwVertex3fv(const GLfloat *v) {
    OpenGLWrapper *oglw = l_openGLWrapper;
    oglw->currentVertex.position[0]=v[0];
    oglw->currentVertex.position[1]=v[1];
    oglw->currentVertex.position[2]=v[2];
    olgwAddVertex();
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
