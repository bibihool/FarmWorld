#define STB_IMAGE_IMPLEMENTATION

#include <windows.h>
#include <iostream>
#include <cmath>
#include "OpenglSoftware/glut.h"
#include "stb_image.h"
#include "main.hpp"
#include "CGRenderUtils.hpp"

using namespace std;

// =====================================================================
//  Texture loader (used by SceneResources for the floor and skybox)
// =====================================================================
GLuint loadTexture(const char* filename, bool isPixelArt)
{
    GLuint textureID = 0;
    int width, height, channels;

    // tell the library to flip loaded textures on the y-axis.
    stbi_set_flip_vertically_on_load(true);

    // load the original image pixel data to main memory
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 4);

    if (!data)
    {
        std::cerr << "ERROR: Failed to load texture: " << filename << std::endl;
        return 0; // 0 is the standard ID for "no texture" in OpenGL
    }

    // assign and bind a texture ID
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // GL_REPEAT lets us tile the floor texture into a repeating pattern
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // GL_LINEAR gives smoother filtering; GL_NEAREST keeps pixel-art crisp
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, isPixelArt ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, isPixelArt ? GL_NEAREST : GL_LINEAR);

    // push the texture data to the GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // release the original pixel data from main memory to avoid leaks
    stbi_image_free(data);

    return textureID;
}

// =====================================================================
//  Small modelling helper
//  ---------------------------------------------------------------------
//  CGRenderUtils::drawCuboid3D() forces the colour to be fully opaque.
//  Our characters are built from plain coloured cuboids, but the chicken
//  is also drawn as a semi-transparent "after-image" trail, so we need a
//  version that keeps an alpha value. This is the only extra primitive we
//  add; everything else reuses our own cuboids.
// =====================================================================
static void drawColourCuboidA(float minX, float minY, float minZ,
                              float maxX, float maxY, float maxZ,
                              float r, float g, float b, float a)
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

    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 4; j++)
            glVertex3fv(v[faces[i][j]]);
    glEnd();
}

// =====================================================================
//  PLAYER 2 :  CHICKEN  (blind-box / toy style, modelled from cuboids)
//  ---------------------------------------------------------------------
//  Built entirely from our own polygons (no GLUT objects) so it counts
//  towards the "objects you created on your own" marks. The alpha
//  parameter lets the same model be reused for the after-image trail.
// =====================================================================
void drawChicken(const Character &c, float alpha = 1.0f)
{
    glPushMatrix();

    // flat-coloured model: disable lighting while drawing, restore after
    GLboolean lightingWasOn;
    glGetBooleanv(GL_LIGHTING, &lightingWasOn);
    glDisable(GL_LIGHTING);

    // place, face the movement direction, and stand on the ground
    glTranslatef(c.pos.x, c.pos.y, c.pos.z);
    float angle = atan2(c.facingDir.x, c.facingDir.z) * (180.0f / M_PI);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, -c.extents.y, 0.0f);

    // --- animation drivers ---
    float speed    = sqrt(c.velocity.x * c.velocity.x + c.velocity.z * c.velocity.z);
    float legSwing = sin(c.animTime) * (speed > 0.01f ? 35.0f : 0.0f);
    float wingFlap = sin(c.animTime * 1.5f) * (speed > 0.01f ? 25.0f : 4.0f);
    float bob      = sin(c.animTime) * 0.03f;
    float peck     = (c.attackTimer > 0)
                     ? sin((c.attackTimer / 10.0f) * M_PI) * 30.0f : 0.0f;
    glTranslatef(0.0f, bob, 0.0f);

    // --- Minecraft chicken colours (front faces +z) ---
    float wR = 0.95f, wG = 0.95f, wB = 0.93f; // white feathers
    float bR = 0.95f, bG = 0.55f, bB = 0.10f; // orange beak / legs / feet
    float rR = 0.80f, rG = 0.10f, rB = 0.10f; // red wattle

    // --- legs + feet (orange), animated walk ---
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.18f, 0.55f, 0.0f);          // hip pivot
        glRotatef(side * legSwing, 1.0f, 0.0f, 0.0f);
        drawColourCuboidA(-0.07f, -0.55f, -0.07f, 0.07f, 0.0f, 0.07f, bR, bG, bB, alpha);   // leg
        drawColourCuboidA(-0.09f, -0.55f, 0.0f, 0.09f, -0.46f, 0.22f, bR, bG, bB, alpha);   // foot (forward)
        glPopMatrix();
    }

    // --- body : a horizontal white box (Minecraft proportions) ---
    drawColourCuboidA(-0.42f, 0.55f, -0.56f, 0.42f, 1.35f, 0.50f, wR, wG, wB, alpha);

    // --- wings : a thin flat box on each side, gently flapping ---
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.42f, 1.30f, 0.0f);          // shoulder pivot
        glRotatef(side * wingFlap, 0.0f, 0.0f, 1.0f);
        drawColourCuboidA(side < 0 ? -0.12f : 0.0f, -0.55f, -0.42f,
                          side < 0 ? 0.0f : 0.12f, 0.0f, 0.42f,
                          wR, wG, wB, alpha);
        glPopMatrix();
    }

    // --- head (white), with beak, wattle and eyes; pecks when attacking ---
    glPushMatrix();
    glTranslatef(0.0f, 1.25f, 0.50f);                      // neck base at front-top of body
    glRotatef(peck, 1.0f, 0.0f, 0.0f);

    drawColourCuboidA(-0.28f, 0.0f, 0.0f, 0.28f, 0.80f, 0.42f, wR, wG, wB, alpha);   // head
    drawColourCuboidA(-0.20f, 0.28f, 0.42f, 0.20f, 0.50f, 0.66f, bR, bG, bB, alpha); // beak (orange, front)
    drawColourCuboidA(-0.10f, 0.12f, 0.42f, 0.10f, 0.30f, 0.60f, rR, rG, rB, alpha); // wattle (red, under beak)

    for (int side = -1; side <= 1; side += 2)                                         // eyes (black)
        drawColourCuboidA(side * 0.28f - 0.03f, 0.45f, 0.18f,
                          side * 0.28f + 0.03f, 0.58f, 0.32f,
                          0.05f, 0.05f, 0.05f, alpha);
    glPopMatrix();

    if (lightingWasOn) glEnable(GL_LIGHTING);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
}

// =====================================================================
//  PLAYER 1 :  COWBOY  (chibi "Woody"-style, modelled from cuboids)
//  ---------------------------------------------------------------------
//  Holds a gun in the right hand. The arm raises when aiming/shooting.
//  Built from our own cuboids only.
// =====================================================================
void drawCowboy(const Character &c)
{
    glPushMatrix();

    GLboolean lightingWasOn;
    glGetBooleanv(GL_LIGHTING, &lightingWasOn);
    glDisable(GL_LIGHTING);

    glTranslatef(c.pos.x, c.pos.y, c.pos.z);
    float angle = atan2(c.facingDir.x, c.facingDir.z) * (180.0f / M_PI);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, -c.extents.y, 0.0f); // feet on the ground

    // --- animation drivers -------------------------------------------
    float speed = sqrt(c.velocity.x * c.velocity.x + c.velocity.z * c.velocity.z);
    float swing = sin(c.animTime) * (speed > 0.01f ? 40.0f : 6.0f);
    float shoot = (c.attackTimer > 0) ? sin((c.attackTimer / 10.0f) * M_PI) * 60.0f : 0.0f;

    // --- colours ------------------------------------------------------
    float skR = 0.95f, skG = 0.78f, skB = 0.60f; // skin
    float shR = 0.95f, shG = 0.80f, shB = 0.20f; // yellow shirt
    float veR = 0.55f, veG = 0.35f, veB = 0.15f; // brown vest
    float jeR = 0.20f, jeG = 0.35f, jeB = 0.70f; // blue jeans
    float htR = 0.45f, htG = 0.28f, htB = 0.12f; // hat
    float boR = 0.30f, boG = 0.18f, boB = 0.08f; // boots / belt
    float gnR = 0.15f, gnG = 0.15f, gnB = 0.17f; // gun metal

    // --- legs (animated walk) ----------------------------------------
    for (int side = -1; side <= 1; side += 2)
    {
        glPushMatrix();
        glTranslatef(side * 0.18f, 0.70f, 0.0f);
        glRotatef(side * swing, 1.0f, 0.0f, 0.0f);
        CGRenderUtils::drawCuboid3D(-0.14f, -0.70f, -0.14f, 0.14f, 0.0f, 0.14f, jeR, jeG, jeB);  // jeans
        CGRenderUtils::drawCuboid3D(-0.16f, -0.75f, -0.10f, 0.16f, -0.60f, 0.22f, boR, boG, boB); // boot
        glPopMatrix();
    }

    // --- torso : shirt, vest overlay, belt ---------------------------
    CGRenderUtils::drawCuboid3D(-0.32f, 0.70f, -0.20f, 0.32f, 1.50f, 0.20f, shR, shG, shB); // shirt
    CGRenderUtils::drawCuboid3D(-0.33f, 0.85f, -0.22f, 0.33f, 1.45f, 0.22f, veR, veG, veB); // vest
    CGRenderUtils::drawCuboid3D(-0.33f, 0.68f, -0.21f, 0.33f, 0.80f, 0.21f, boR, boG, boB); // belt

    // --- left arm (swings) -------------------------------------------
    glPushMatrix();
    glTranslatef(-0.42f, 1.45f, 0.0f);
    glRotatef(-swing, 1.0f, 0.0f, 0.0f);
    CGRenderUtils::drawCuboid3D(-0.12f, -0.70f, -0.12f, 0.12f, 0.0f, 0.12f, shR, shG, shB); // sleeve
    CGRenderUtils::drawCuboid3D(-0.10f, -0.85f, -0.10f, 0.10f, -0.68f, 0.10f, skR, skG, skB); // hand
    glPopMatrix();

    // --- right arm holds the gun (raises when aiming / shooting) ------
    glPushMatrix();
    glTranslatef(0.42f, 1.45f, 0.0f);
    float rightArm = (c.isAiming ? -90.0f : swing) - shoot;
    glRotatef(rightArm, 1.0f, 0.0f, 0.0f);
    CGRenderUtils::drawCuboid3D(-0.12f, -0.70f, -0.12f, 0.12f, 0.0f, 0.12f, shR, shG, shB); // sleeve
    CGRenderUtils::drawCuboid3D(-0.10f, -0.85f, -0.10f, 0.10f, -0.68f, 0.10f, skR, skG, skB); // hand

    // gun in the hand, barrel pointing forward (+z)
    glPushMatrix();
    glTranslatef(0.0f, -0.78f, 0.10f);
    CGRenderUtils::drawCuboid3D(-0.05f, -0.05f, 0.0f, 0.05f, 0.05f, 0.45f, gnR, gnG, gnB);  // barrel
    CGRenderUtils::drawCuboid3D(-0.05f, -0.28f, 0.0f, 0.05f, -0.05f, 0.12f, gnR, gnG, gnB); // handle
    glPopMatrix();
    glPopMatrix();

    // --- head (big, chibi) + cowboy hat ------------------------------
    glPushMatrix();
    glTranslatef(0.0f, 1.50f, 0.0f);
    CGRenderUtils::drawCuboid3D(-0.34f, 0.0f, -0.32f, 0.34f, 0.68f, 0.32f, skR, skG, skB); // head

    for (int side = -1; side <= 1; side += 2) // eyes
        CGRenderUtils::drawCuboid3D(side * 0.16f - 0.05f, 0.32f, 0.30f,
                                    side * 0.16f + 0.05f, 0.45f, 0.36f,
                                    0.05f, 0.05f, 0.05f);
    CGRenderUtils::drawCuboid3D(-0.12f, 0.12f, 0.31f, 0.12f, 0.18f, 0.35f, 0.40f, 0.20f, 0.10f); // smile

    glTranslatef(0.0f, 0.68f, 0.0f);
    CGRenderUtils::drawCuboid3D(-0.30f, 0.0f, -0.28f, 0.30f, 0.28f, 0.28f, htR, htG, htB);   // crown
    CGRenderUtils::drawCuboid3D(-0.50f, -0.04f, -0.50f, 0.50f, 0.04f, 0.50f, htR, htG, htB);  // brim
    glPopMatrix();

    if (lightingWasOn) glEnable(GL_LIGHTING);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glPopMatrix();
}
