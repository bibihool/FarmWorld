#pragma once // otherwise processor will dead lock if this file is included twice

#include "OpenglSoftware/glut.h"

namespace CGRenderUtils
{

    inline void drawSolidTri2D(const float vertices[3][3], const float color[3])
    {
        glColor3fv(color);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < 3; i++) {
            glVertex3fv(vertices[i]);
        }
        glEnd();
    }

    //Overload function to support separate rgb parameters for solid colour
    inline void drawSolidTri2D(const float vertices[3][3], float r, float g, float b)
    {
        glColor3f(r, g, b);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < 3; i++) {
            glVertex3fv(vertices[i]);
        }
        glEnd();
    }

    // inline keyword i important to prevent linker errors when including this header in multiple cpp files
    // Tool 1: Draw a solid colour quadrilateral (2D)
    inline void drawSolidQuad2D(const float vertices[4][3], const float color[3])
    {
        glColor3fv(color); // for pure colour, we only need to set the state once before glBegin!
        glBegin(GL_QUADS);
        for (int i = 0; i < 4; i++) {
            glVertex3fv(vertices[i]);
        }
        glEnd();
    }

    // Overload Function to support separate rgb parameters for solid colour
    inline void drawSolidQuad2D(const float vertices[4][3], float r, float g, float b)
    {
        glColor3f(r, g, b);
        glBegin(GL_QUADS);
        for (int i = 0; i < 4; i++) {
            glVertex3fv(vertices[i]);
        }
        glEnd();
    }

    // Tool 2: Draw a quadrilateral with vertex colours (2D)
    inline void drawGradientQuad2D(const float vertices[4][3], const float vertexColors[4][3])
    {
        glBegin(GL_QUADS);
        for (int i = 0; i < 4; i++) {
            glColor3fv(vertexColors[i]); // Transition: colour state can change per vertex for gradient effect
            glVertex3fv(vertices[i]);
        }
        glEnd();
    }

    // Tool 3: Draw a solid colour cuboid (3D)
    inline void drawCuboid3D(float minX, float minY, float minZ,
                             float maxX, float maxY, float maxZ,
                             float r, float g, float b)
    {
        // 1. Define 8 vertices of the cuboid
        // Assume：0-3 is minZ，4-7 is maxZ
        const float v[8][3] = {
            {minX, minY, minZ}, {maxX, minY, minZ}, {maxX, maxY, minZ}, {minX, maxY, minZ}, // 0, 1, 2, 3
            {minX, minY, maxZ}, {maxX, minY, maxZ}, {maxX, maxY, maxZ}, {minX, maxY, maxZ} // 4, 5, 6, 7
        };

        // 2. Six Faces with 4 Vertices (must be in counter-clockwise order when viewed from outside)
        const int faces[6][4] = {
            {4, 5, 6, 7}, // Front  : z = maxZ
            {1, 0, 3, 2}, // Back   : z = minZ
            {0, 4, 7, 3}, // Left   : x = minX
            {5, 1, 2, 6}, // Right  : x = maxX
            {3, 7, 6, 2}, // Top    : y = maxY
            {4, 0, 1, 5}  // Bottom : y = minY
        };


        glColor3f(r, g, b);
        glBegin(GL_QUADS);
        for (int i = 0; i < 6; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                glVertex3fv(v[faces[i][j]]);
            }
        }
        glEnd();
    }

    // Tool 4: Draw a solid colour pyramid (3D)
    inline void drawSolidPyramid3D(float halfWidth, float halfDepth, float height, const float color[3])
    {
        glColor3fv(color);

        // 1. Draw a 4 traingular faces
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, height, 0.0f); // Note that the middle one is apex

        // Counter-clockwise order when viewed from outside (bottom face is not drawn, so we don't care about its winding order)
        glVertex3f(-halfWidth, 0.0f,  halfDepth); // front-left
        glVertex3f( halfWidth, 0.0f,  halfDepth); // front-right
        glVertex3f( halfWidth, 0.0f, -halfDepth); // back-right
        glVertex3f(-halfWidth, 0.0f, -halfDepth); // back-left
        glVertex3f(-halfWidth, 0.0f,  halfDepth); // need this to ensure closing (just like calculating area of polygon using coordinates)
        glEnd();

        // 2. Draw the base of the pyramid
        glBegin(GL_QUADS);
        // Counter Clockwise order when viewed from outside (looking up at the pyramid from below)
        glVertex3f(-halfWidth, 0.0f,  halfDepth); // front-left
        glVertex3f(-halfWidth, 0.0f, -halfDepth); // back-left
        glVertex3f( halfWidth, 0.0f, -halfDepth); // back-right
        glVertex3f( halfWidth, 0.0f,  halfDepth); // front-right
        glEnd();
    }

    // Overload Function to support separate rgb parameters for solid colour
    inline void drawSolidPyramid3D(float halfWidth, float halfDepth, float height, float r, float g, float b)
    {
        float color[3] = {r, g, b};
        drawSolidPyramid3D(halfWidth, halfDepth, height, color);
    }

    // Tool 5: Draw a textured cuboid (3D)
    // uv parameter layout: [6 faces][4 vertices][2 coords (u,v)]
    // Face order: Front, Back, Left, Right, Top, Bottom
    inline void drawTexturedCuboid3D(float minX, float minY, float minZ,
                                     float maxX, float maxY, float maxZ,
                                     const float uv[6][4][2])
    {
        const float v[8][3] = {
            {minX, minY, minZ}, {maxX, minY, minZ}, {maxX, maxY, minZ}, {minX, maxY, minZ},
            {minX, minY, maxZ}, {maxX, minY, maxZ}, {maxX, maxY, maxZ}, {minX, maxY, maxZ}
        };

        const int faces[6][4] = {
            {4, 5, 6, 7}, // Front
            {1, 0, 3, 2}, // Back
            {0, 4, 7, 3}, // Left
            {5, 1, 2, 6}, // Right
            {3, 7, 6, 2}, // Top
            {4, 0, 1, 5}  // Bottom
        };

        glBegin(GL_QUADS);
        for (int i = 0; i < 6; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                glTexCoord2fv(uv[i][j]);   // Bind this vertex's UV
                glVertex3fv(v[faces[i][j]]); // Draw this vertex
            }
        }
        glEnd();
    }

}
