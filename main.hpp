#include "OpenglSoftware/glut.h"
#include "CGRenderUtils.hpp"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

GLuint loadTexture(const char *filename, bool isPixelArt = true);

#define GHOST_COUNT 5 // record last 5 frames for spheal

struct SceneResources
{
    GLuint floorTexture;
    GLuint windChargeTexture; // projectiles for player 1
    GLuint explosionTexture;  // explosion effect for player 1 projectiles
    GLuint treeTexture;       // background tree billboard
    bool isLoaded;

    // This constructor is only responsible for initializing member variables to safe defaults.
    SceneResources() : floorTexture(0), windChargeTexture(0), explosionTexture(0), treeTexture(0), isLoaded(false)
    {
    }

    // This method is responsible for loading all textures.
    void loadAll()
    {
        if (isLoaded)
            return; // avoid repeating loading if already done

        floorTexture = loadTexture("assets/textures/brick.png");
        windChargeTexture = loadTexture("assets/textures/wind_charge.png");
        explosionTexture = loadTexture("assets/textures/explosion.png");
        treeTexture = loadTexture("assets/background/tree.png", false);

        isLoaded = true;
    }

    // cleanup the memory used by textures when the program is closing.
    void cleanup()
    {
        if (!isLoaded)
            return;

        if (floorTexture)
            glDeleteTextures(1, &floorTexture);
        if (windChargeTexture)
            glDeleteTextures(1, &windChargeTexture);
        if (explosionTexture)
            glDeleteTextures(1, &explosionTexture);
        if (treeTexture)
            glDeleteTextures(1, &treeTexture);

        isLoaded = false;
    }
};

// Character Modification
struct Vec3
{
    float x, y, z;
};

struct Shockwave
{
    Vec3 pos;
    Vec3 velocity;
    Vec3 extents; // wave size
    bool isActive;
};

struct ExplosionFx
{
    Vec3 pos;
    int timer;
};

struct GhostState
{
    Vec3 pos;
    Vec3 facingDir;
    float attackTimer; // use timer to count
    bool active;       // check if the attack counts
};

struct Character
{
    // dynamic physical movement
    Vec3 pos;      // the character centre position in the world
    Vec3 velocity; // the current movement velocity, which will be applied to the position every tick and modified by collisions

    // hitbox border points relative to the character's centre position, which will be used for collision detection.
    // For simplicity, we can treat the character as an axis-aligned box (AABB), so we only need the extents (half-sizes) along each axis.
    Vec3 extents;

    // basic state flags for movement and actions
    bool isGrounded; // check if it is on the ground for jumping logic

    Vec3 facingDir; // record the direction of X,Y,Z facing for animation purposes (e.g., which way to punch)
    int attackTimer;

    float hp;    // default: 20.0f
    bool isDead; // check if defeat

    int skillCooldown; // cooldown timer (frames)
    bool isAiming;     // aiming

    int laserTimer; // only for player 2, which shoots laser beams instead of shockwaves, so we can reuse this timer for the laser attack cooldown and duration.

    // Animation Zone
    float animTime = 0.0f;

    // Special Skill: the ghost trail for Spheal, which records the last few frames of position and facing direction to create afterimages when using the special skill.
    GhostState ghosts[GHOST_COUNT];
    int ghostHead = 0;
};

void drawCowboy(const Character &c);
void drawChicken(const Character &c, float alpha);

inline bool checkAABBIntersect(const Vec3 &posA, const Vec3 &extA,
                               const Vec3 &posB, const Vec3 &extB)
{
    return (std::abs(posA.x - posB.x) < (extA.x + extB.x)) &&
           (std::abs(posA.y - posB.y) < (extA.y + extB.y)) &&
           (std::abs(posA.z - posB.z) < (extA.z + extB.z));
}

class MyVirtualWorld
{
public:
    SceneResources sceneRes;
    Character player1;
    Character player2;

    // Player 1 Skill
    Shockwave shockwave;
    ExplosionFx explosion;

    // --- dynamic environment state (this is what "changes during battle") ---
    float envLight = 1.0f;            // 1.0 = bright day, falls toward dusk as HP is lost
    static const int MAX_SCORCH = 12; // limit on burn marks kept on the ground
    Vec3 scorches[MAX_SCORCH];        // positions of skill-impact burn marks
    int scorchCount = 0;              // how many have happened (ring buffer index)

    // --- Player 2 egg-volley skill (replaces the old laser beam) ---
    static const int MAX_EGGS = 18;
    struct Egg { Vec3 pos; Vec3 vel; bool active; };
    Egg eggs[MAX_EGGS];
    int eggToFire = 0;   // eggs still to launch in the current volley
    int eggFireGap = 0;  // frames until the next egg launches

    // Record a new burn mark on the ground (ring buffer, never overflows).
    void addScorch(const Vec3 &p)
    {
        scorches[scorchCount % MAX_SCORCH] = p;
        scorchCount++;
    }

    void draw()
    {
        glEnable(GL_TEXTURE_2D);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        // ---- update the daylight level from how much total damage was dealt ----
        // As the two fighters lose HP, the whole farm darkens from day to dusk.
        float damage = (20.0f - player1.hp) + (20.0f - player2.hp);
        float progress = damage / 40.0f;
        if (progress > 1.0f) progress = 1.0f;
        envLight = 1.0f - 0.65f * progress;

        // we need to turn off the lightning, otherwise F3 will be very dark and invisible due to the lack of normals and lighting calculations
        GLboolean lightingWasOn;
        glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        glDisable(GL_LIGHTING);

        // ==========================================
        //  Sky : a smooth blue gradient (deep blue at the top fading to a
        //  pale horizon). It warms to orange and darkens toward dusk using
        //  the daylight level, so the sky changes during the battle.
        // ==========================================
        glDepthMask(GL_FALSE); // the sky must never block anything
        glDisable(GL_TEXTURE_2D);

        float s = 100.0f;
        float L = envLight;

        // zenith (top) and horizon (bottom) colours
        float zR = 0.25f * L, zG = 0.45f * L, zB = 0.85f * L;
        float hR = 0.75f * L + 0.25f * (1.0f - L); // horizon warms up at dusk
        float hG = 0.88f * L;
        float hB = 1.00f * L;

        glBegin(GL_QUADS);
        // four walls: horizon colour along the bottom, zenith along the top
        glColor3f(hR, hG, hB); glVertex3f(-s, -s, -s); glVertex3f(s, -s, -s);
        glColor3f(zR, zG, zB); glVertex3f(s, s, -s);   glVertex3f(-s, s, -s); // front
        glColor3f(hR, hG, hB); glVertex3f(s, -s, s);   glVertex3f(-s, -s, s);
        glColor3f(zR, zG, zB); glVertex3f(-s, s, s);   glVertex3f(s, s, s);   // back
        glColor3f(hR, hG, hB); glVertex3f(s, -s, -s);  glVertex3f(s, -s, s);
        glColor3f(zR, zG, zB); glVertex3f(s, s, s);    glVertex3f(s, s, -s);  // right
        glColor3f(hR, hG, hB); glVertex3f(-s, -s, s);  glVertex3f(-s, -s, -s);
        glColor3f(zR, zG, zB); glVertex3f(-s, s, -s);  glVertex3f(-s, s, s);  // left
        glEnd();

        // top cap (solid zenith blue)
        glColor3f(zR, zG, zB);
        glBegin(GL_QUADS);
        glVertex3f(-s, s, -s); glVertex3f(s, s, -s); glVertex3f(s, s, s); glVertex3f(-s, s, s);
        glEnd();

        glDepthMask(GL_TRUE); // restore depth writing for the rest of the scene

        // ==========================================
        //  Farm ground : a real grass TEXTURE (brick.png is actually a
        //  grass-and-stones tile) mapped over a large quad and tiled many
        //  times so the whole field is covered.
        // ==========================================
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, sceneRes.floorTexture);
        glColor3f(envLight, envLight, envLight); // dim with the daylight level
        glNormal3f(0.0f, 1.0f, 0.0f);

        float ground = 80.0f;
        float rep = 40.0f; // how many times the grass tile repeats across the field
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-ground, 0.0f, ground);
        glTexCoord2f(rep, 0.0f);  glVertex3f(ground, 0.0f, ground);
        glTexCoord2f(rep, rep);   glVertex3f(ground, 0.0f, -ground);
        glTexCoord2f(0.0f, rep);  glVertex3f(-ground, 0.0f, -ground);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        // 3. revert anything we changed for the skybox and floor back to the default state,
        // so that it won't affect the character rendering and UI rendering that follow.
        if (lightingWasOn)
        {
            glEnable(GL_LIGHTING);
        }
        glDisable(GL_TEXTURE_2D);

        // Background scenery : sun, clouds, distant hills and trees
        drawBackground();

        // Burn marks the battle has left on the grass
        drawScorchMarks();

        // Farm props : fence around the arena and a barn in the background
        drawFarmProps();

        // Player 1 (Cowboy)
        drawCowboy(player1);

        if (player2.attackTimer > 0)
        {
            // Enable semi-transparent blending state
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); // Critical: disable depth writes to avoid transparency sorting artifacts

            // Iterate the ring buffer in reverse order
            for (int i = 1; i <= GHOST_COUNT; i++)
            {
                // Compute the history index via modulo (larger i = older frame)
                int index = (player2.ghostHead - i + GHOST_COUNT) % GHOST_COUNT;

                if (player2.ghosts[index].active)
                {
                    // The older the frame, the lower the alpha. Fades from 0.5 down to 0.1
                    float ghostAlpha = 0.5f - (i * (0.5f / GHOST_COUNT));

                    // Build a temporary Character to hand to the renderer
                    Character ghostData = player2; // Copy base data (size, animation params, etc.)
                    ghostData.pos = player2.ghosts[index].pos;
                    ghostData.facingDir = player2.ghosts[index].facingDir;
                    ghostData.attackTimer = player2.ghosts[index].attackTimer;

                    // draw the semi-transparent after-image
                    drawChicken(ghostData, ghostAlpha);
                }
            }

            // Restore the render state!
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // Player 2 (Chicken)
        drawChicken(player2, 1.0f);

        // ==========================================
        // [only for debug] (sighting rays)
        // ==========================================
        // glDisable(GL_TEXTURE_2D);
        // glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        // glDisable(GL_LIGHTING);

        // glColor3f(0.0f, 1.0f, 0.0f);
        // glLineWidth(2.0f);

        // glBegin(GL_LINES);
        // float rayLength = 2.0f;

        // // the line starts from the character's center position and extends in the direction they are facing,
        // // which visually indicates where the character is "looking" or "attacking" towards.
        // glVertex3f(player1.pos.x, player1.pos.y, player1.pos.z);

        // // the end point is calculated by taking the character's center position
        // // and adding the facing direction vector multiplied by a length factor.
        // glVertex3f(player1.pos.x + player1.facingDir.x * rayLength,
        //            player1.pos.y + player1.facingDir.y * rayLength,
        //            player1.pos.z + player1.facingDir.z * rayLength);

        // // same applies for player 2!
        // glVertex3f(player2.pos.x, player2.pos.y, player2.pos.z);

        // glVertex3f(player2.pos.x + player2.facingDir.x * rayLength,
        //            player2.pos.y + player2.facingDir.y * rayLength,
        //            player2.pos.z + player2.facingDir.z * rayLength);
        // glEnd();

        // ==========================================
        // Visual Effect: When Player 1 charging the shockwave, draw a cyan sphere.
        // ==========================================
        if (player1.isAiming)
        {
            glDisable(GL_TEXTURE_2D); // Create pure color for the shockwave indicator, without any texture influence.

            // turn off lighting to make the sphere fully bright and visible, otherwise it will be affected by the scene's lighting and might become too dark to see, especially since it's a simple solid color without any texture details.
            GLboolean lightingWasOn;
            glGetBooleanv(GL_LIGHTING, &lightingWasOn);
            glDisable(GL_LIGHTING);

            glColor3f(0.0f, 1.0f, 1.0f);

            glPushMatrix();
            // 1. position it to player center, so it will look like it's "charging" from the character itself.
            glTranslatef(player1.pos.x, player1.pos.y, player1.pos.z);
            // 2. offset it forward in the facing direction, so it won't be hidden inside the character's hitbox and will look more natural as a "projectile being formed in front of the character" rather than "a ball growing inside the character".
            glTranslatef(player1.facingDir.x * 1.0f,
                         0.0f, // stay at the same height as the character's center, which is roughly where the hands are, so it looks like the shockwave is being formed from the character's hands.
                         player1.facingDir.z * 1.0f);

            // // 3. draw a sphere with radius 0.4, using 16 slices and 16 stacks for decent smoothness without too much performance cost.
            // glutSolidSphere(0.4, 16, 16);
            glPopMatrix();

            // revert back the lightning
            if (lightingWasOn)
                glEnable(GL_LIGHTING);
            glEnable(GL_TEXTURE_2D);
        }

        // Draw Shockwave (now texture) as visual effect
        if (shockwave.isActive)
        {
            glPushMatrix(); // 1. Save the coordinate system

            // 2. Translate to the wind charge's physics position
            glTranslatef(shockwave.pos.x, shockwave.pos.y, shockwave.pos.z);

            // 3. Face the texture toward the wind charge's travel direction (optional, if you want it to spin)
            float waveAngle = atan2(shockwave.velocity.x, shockwave.velocity.z) * (180.0f / 3.14159265f);
            glRotatef(waveAngle, 0.0f, 1.0f, 0.0f);
            glRotatef(90.0f, 1.0f, 0.0f, 0.0f); // Stand the plane upright

            // 4. Isolate from lighting interference
            GLboolean lightWasOn;
            glGetBooleanv(GL_LIGHTING, &lightWasOn);
            glDisable(GL_LIGHTING);

            // 5. Enable texturing and alpha testing
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, sceneRes.windChargeTexture); // Make sure this isn't 0!
            glColor3f(1.0f, 1.0f, 1.0f);                              // Reset to white to clear any previous color
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.1f);

            // 6. Draw the 2D panel (Wind Charge)
            float size = shockwave.extents.x; // Adjust this to change the wind charge size
            glBegin(GL_QUADS);
            // Front face
            glTexCoord2f(0, 0);
            glVertex3f(-size, 0, 0);
            glTexCoord2f(1, 0);
            glVertex3f(size, 0, 0);
            glTexCoord2f(1, 1);
            glVertex3f(size, size * 2, 0);
            glTexCoord2f(0, 1);
            glVertex3f(-size, size * 2, 0);
            // Back face (so it doesn't disappear when viewed from behind)
            glTexCoord2f(0, 0);
            glVertex3f(-size, 0, 0);
            glTexCoord2f(0, 1);
            glVertex3f(-size, size * 2, 0);
            glTexCoord2f(1, 1);
            glVertex3f(size, size * 2, 0);
            glTexCoord2f(1, 0);
            glVertex3f(size, 0, 0);
            glEnd();

            // 7. Fully restore the render state
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_TEXTURE_2D);
            if (lightWasOn)
                glEnable(GL_LIGHTING);

            glPopMatrix();
        }

        if (explosion.timer > 0)
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, sceneRes.explosionTexture);
            glColor3f(1.0f, 1.0f, 1.0f);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.1f);

            GLboolean lightWasOn;
            glGetBooleanv(GL_LIGHTING, &lightWasOn);
            glDisable(GL_LIGHTING);

            glPushMatrix();
            glTranslatef(explosion.pos.x, explosion.pos.y, explosion.pos.z);

            // Animation core: as the timer counts down, the explosion cloud expands
            float expandScale = 2.0f + (15.0f - explosion.timer) * 0.2f;
            glScalef(expandScale, expandScale, expandScale);

            // Use the full-cover textured box helper to fake a 3D explosion smoke block
            // For simplicity, all 6 faces use the full texture UV range (0~1)
            float fullUV[6][4][2];
            for (int i = 0; i < 6; i++)
            {
                fullUV[i][0][0] = 0;
                fullUV[i][0][1] = 0;
                fullUV[i][1][0] = 1;
                fullUV[i][1][1] = 0;
                fullUV[i][2][0] = 1;
                fullUV[i][2][1] = 1;
                fullUV[i][3][0] = 0;
                fullUV[i][3][1] = 1;
            }
            CGRenderUtils::drawTexturedCuboid3D(-0.5, -0.5, -0.5, 0.5, 0.5, 0.5, fullUV);

            glPopMatrix();
            glDisable(GL_ALPHA_TEST);
            if (lightWasOn)
                glEnable(GL_LIGHTING);
        }

        // Player 2 egg volley
        drawEggs();

        glLineWidth(1.0f);
        if (lightingWasOn)
        {
            glEnable(GL_LIGHTING);
        }

        drawUI();

        // restore the default state for any subsequent rendering that might rely on textures
        glEnable(GL_TEXTURE_2D);
    }

    // --- Simple OpenGL Text Rendering ---
    void drawText(float x, float y, const char *string)
    {
        // we need to do a raster position transformation to convert it to the orthographic projection coordinates we set up in drawUI(),
        // where (0,0) is at the bottom-left corner of the window and (800,500) is at the top-right corner.
        // This way, we can specify text positions in pixel coordinates that directly correspond to the window size, making it easier to position UI elements like text.
        glRasterPos2f(x, y);
        // C-style text print after it hits "\0"
        for (const char *c = string; *c != '\0'; c++)
        {
            // use the times roman default 24 font.
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
        }
    }

    void drawUI()
    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE); // 2D HUD: don't cull, or the bar fills vanish

        // Switch to a 2D orthographic projection matching the window pixels.
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        float sw = (float)viewport[2];
        float sh = (float)viewport[3];
        if (sw <= 0.0f) sw = 800.0f;
        if (sh <= 0.0f) sh = 500.0f;

        glOrtho(0, sw, 0, sh, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        float barWidth = sw * 0.32f;
        float barHeight = 22.0f;
        float marginX = sw * 0.04f;
        float topMargin = sh - 55.0f;
        float cdTop = topMargin - 16.0f;
        float cdHeight = 10.0f;

        // ---- Player 1 (left) : red HP bar + blue energy bar ----
        draw3DBar(marginX, topMargin, barWidth, barHeight, player1.hp / 20.0f,
                  0.20f, 1.0f, 0.30f, false);   // bright green health
        draw3DBar(marginX, cdTop, barWidth, cdHeight,
                  (900.0f - player1.skillCooldown) / 900.0f, 1.0f, 0.85f, 0.05f, false); // bright gold energy
        drawArcade(marginX, topMargin + 12.0f, 0.14f, "WOODY", 1.0f, 0.85f, 0.1f);

        // ---- Player 2 (right) : drains toward the centre ----
        float p2x = sw - marginX - barWidth;
        draw3DBar(p2x, topMargin, barWidth, barHeight, player2.hp / 20.0f,
                  0.20f, 1.0f, 0.30f, true);    // bright green health
        draw3DBar(p2x, cdTop, barWidth, cdHeight,
                  (900.0f - player2.skillCooldown) / 900.0f, 1.0f, 0.85f, 0.05f, true); // bright gold energy
        float nameW = strokeWidth("MC CHICKEN", 0.14f);
        drawArcade(sw - marginX - nameW, topMargin + 12.0f, 0.14f, "MC CHICKEN", 1.0f, 0.85f, 0.1f);

        // ---- "VS" badge in the top middle ----
        float vsScale = 0.5f;
        float vsW = strokeWidth("VS", vsScale);
        drawArcade(sw * 0.5f - vsW * 0.5f, topMargin - 4.0f, vsScale, "VS", 1.0f, 0.2f, 0.1f);

        // ---- Game over banner ----
        if (player1.isDead || player2.isDead)
        {
            const char *msg = (player1.isDead && player2.isDead) ? "DRAW!"
                              : (player1.isDead ? "MC CHICKEN WINS!" : "WOODY WINS!");
            float scale = 0.7f;
            float w = strokeWidth(msg, scale);
            drawArcade(sw * 0.5f - w * 0.5f, sh * 0.5f, scale, msg, 1.0f, 0.85f, 0.1f);

            float w2 = strokeWidth("PRESS ESC TO EXIT", 0.18f);
            drawArcade(sw * 0.5f - w2 * 0.5f, sh * 0.5f - 50.0f, 0.18f,
                       "PRESS ESC TO EXIT", 1.0f, 1.0f, 1.0f);
        }

        // Restore the 3D projection.
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE); // restore for the 3D scene next frame
    }

    void tickTime()
    {
        applyGravityAndMovement();
        resolveCharacterCollisions();
        resolveArenaBounds();
        processAttacksAndSkills();

        // Lock Z-axis positions and velocities to 0.0f
        player1.pos.z = 0.0f;
        player1.velocity.z = 0.0f;
        player2.pos.z = 0.0f;
        player2.velocity.z = 0.0f;


    }


    // init function is here!
    void init()
    {
        // Load everything to GPU!
        sceneRes.loadAll();

        // Initialize player states
        player1.hp = 20.0f;
        player1.pos = {-10.0f, player1.extents.y, 0.0f};
        player1.velocity = {0.0f, 0.0f, 0.0f};
        player1.extents = {1.0f, 2.0f, 1.0f};
        player1.isGrounded = true;
        player1.skillCooldown = 900;
        player1.isAiming = false;
        player1.facingDir = {1.0f, 0.0f, 0.0f}; // Face right initially

        // player 1 special skills
        shockwave.isActive = false;

        player2.hp = 20.0f;
        player2.pos = {10.0f, player2.extents.y, 0.0f};
        player2.velocity = {0.0f, 0.0f, 0.0f};
        player2.extents = {1.0f, 2.0f, 1.0f};
        player2.isGrounded = true;
        player2.skillCooldown = 900;
        // player2.isAiming = false;
        player2.facingDir = {-1.0f, 0.0f, 0.0f}; // Face left initially

        player2.laserTimer = 0;

        // reset the egg volley
        for (int i = 0; i < MAX_EGGS; i++)
            eggs[i].active = false;
        eggToFire = 0;
        eggFireGap = 0;
    }

    // =================================================================
    //  Player 2 special skill : egg volley
    // =================================================================
    // Start a new volley (called when the O key is pressed).
    void startEggVolley()
    {
        eggToFire = 14; // launch 14 eggs one after another (longer volley)
        eggFireGap = 0;
    }

    // Launch a single egg from the chicken in its facing direction.
    void launchEgg()
    {
        for (int i = 0; i < MAX_EGGS; i++)
        {
            if (!eggs[i].active)
            {
                eggs[i].active = true;
                eggs[i].pos = {
                    player2.pos.x + player2.facingDir.x * (player2.extents.x + 0.5f),
                    player2.pos.y + 0.4f,
                    player2.pos.z + player2.facingDir.z * (player2.extents.z + 0.5f)};
                float spd = 0.45f;
                eggs[i].vel = {player2.facingDir.x * spd, 0.18f, player2.facingDir.z * spd};
                return;
            }
        }
    }

    // Advance the volley timer and move every active egg (called each tick).
    void updateEggs()
    {
        // launch the next egg of the volley once the gap has elapsed
        if (eggToFire > 0)
        {
            if (eggFireGap <= 0)
            {
                launchEgg();
                eggToFire--;
                eggFireGap = 7; // frames between consecutive eggs
            }
            else
            {
                eggFireGap--;
            }
        }

        for (int i = 0; i < MAX_EGGS; i++)
        {
            if (!eggs[i].active)
                continue;

            eggs[i].vel.y -= 0.02f; // gravity makes the eggs arc
            eggs[i].pos.x += eggs[i].vel.x;
            eggs[i].pos.y += eggs[i].vel.y;
            eggs[i].pos.z += eggs[i].vel.z;

            // hit player 1?
            Vec3 eggExt = {0.3f, 0.3f, 0.3f};
            if (!player1.isDead && checkAABBIntersect(eggs[i].pos, eggExt, player1.pos, player1.extents))
            {
                player1.hp -= 1.0f;
                if (player1.hp <= 0.0f)
                {
                    player1.hp = 0.0f;
                    player1.isDead = true;
                }
                player1.velocity.x += player2.facingDir.x * 0.15f;
                player1.velocity.y += 0.15f;
                addScorch(eggs[i].pos);
                eggs[i].active = false;
                continue;
            }

            // landed on the ground or flew out of the arena?
            if (eggs[i].pos.y <= 0.0f ||
                std::abs(eggs[i].pos.x) > 14.0f || std::abs(eggs[i].pos.z) > 14.0f)
            {
                Vec3 splat = {eggs[i].pos.x, 0.0f, eggs[i].pos.z};
                addScorch(splat); // leave a splat where it lands
                eggs[i].active = false;
            }
        }
    }

    // Draw every active egg as a small egg-shaped block (our own geometry).
    void drawEggs()
    {
        GLboolean lightingWasOn;
        glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);

        for (int i = 0; i < MAX_EGGS; i++)
        {
            if (!eggs[i].active)
                continue;
            glPushMatrix();
            glTranslatef(eggs[i].pos.x, eggs[i].pos.y, eggs[i].pos.z);
            glScalef(1.0f, 1.3f, 1.0f); // stretch slightly into an egg shape
            CGRenderUtils::drawCuboid3D(-0.22f, -0.22f, -0.22f, 0.22f, 0.22f, 0.22f,
                                        0.98f, 0.95f, 0.85f); // cream eggshell
            glPopMatrix();
        }

        if (lightingWasOn)
            glEnable(GL_LIGHTING);
    }

    // =================================================================
    //  HUD helpers : a 3D-looking bar and arcade (vector) text
    // =================================================================
    // A health / energy bar with a drop shadow, a bright-to-dark gradient
    // fill, a glossy highlight and a bevel border, so it looks 3D.
    void draw3DBar(float x, float y, float w, float h, float ratio,
                   float fr, float fg, float fb, bool mirror)
    {
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        float fw = w * ratio;
        float fx = mirror ? (x + w - fw) : x; // P2 drains toward the centre

        glColor3f(0.0f, 0.0f, 0.0f); // drop shadow
        glRectf(x + 3.0f, y - 3.0f, x + w + 3.0f, y + h - 3.0f);

        glColor3f(0.12f, 0.12f, 0.12f); // empty background
        glRectf(x, y, x + w, y + h);

        glBegin(GL_QUADS); // gradient fill (bright top -> dark bottom)
        glColor3f(fr, fg, fb);
        glVertex2f(fx, y + h);
        glVertex2f(fx + fw, y + h);
        glColor3f(fr * 0.85f, fg * 0.85f, fb * 0.85f);
        glVertex2f(fx + fw, y);
        glVertex2f(fx, y);
        glEnd();

        glEnable(GL_BLEND); // glossy highlight strip
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 1.0f, 0.35f);
        glRectf(fx, y + h * 0.55f, fx + fw, y + h * 0.85f);
        glDisable(GL_BLEND);

        glColor3f(0.85f, 0.85f, 0.9f); // bevel border
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
        glLineWidth(1.0f);
    }

    // Width of a string in the scalable stroke font (for centring text).
    float strokeWidth(const char *s, float scale)
    {
        float w = 0.0f;
        for (const char *c = s; *c; c++)
            w += glutStrokeWidth(GLUT_STROKE_ROMAN, *c);
        return w * scale;
    }

    // Draw scalable vector text at the given position and scale.
    void drawStroke(float x, float y, float scale, const char *s)
    {
        glPushMatrix();
        glTranslatef(x, y, 0.0f);
        glScalef(scale, scale, 1.0f);
        for (const char *c = s; *c; c++)
            glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
        glPopMatrix();
    }

    // Bold "arcade" title (dark outline + bright fill), Mortal-Kombat style.
    void drawArcade(float x, float y, float scale, const char *s,
                    float r, float g, float b)
    {
        glLineWidth(4.0f);
        glColor3f(0.15f, 0.0f, 0.0f); // outline / shadow
        drawStroke(x + 2.0f, y - 2.0f, scale, s);
        glColor3f(r, g, b); // bright fill
        drawStroke(x, y, scale, s);
        glLineWidth(1.0f);
    }

    // =================================================================
    //  Farm scenery : a wooden fence around the arena and a red barn.
    //  Everything is built from our own cuboids; the fence posts are
    //  placed with a loop (no hard-coding).
    // =================================================================
    void drawFarmProps()
    {
        GLboolean lightingWasOn;
        glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE); // draw both sides of every prop face

        float arena = 12.0f;       // matches the play boundary
        float L = envLight;        // dim every prop by the current daylight
        float wR = 0.55f * L, wG = 0.35f * L, wB = 0.15f * L; // wood colour

        // --- fence posts on all four sides (loop) ---
        int posts = 9;
        float span = (arena * 2.0f) / (posts - 1);
        for (int i = 0; i < posts; i++)
        {
            float t = -arena + i * span;
            for (int s = -1; s <= 1; s += 2) // both opposite sides at once
            {
                // posts along the front/back edges (vary X, fixed Z)
                glPushMatrix();
                glTranslatef(t, 0.0f, s * arena);
                CGRenderUtils::drawCuboid3D(-0.1f, 0.0f, -0.1f, 0.1f, 1.0f, 0.1f, wR, wG, wB);
                glPopMatrix();
                // posts along the left/right edges (fixed X, vary Z)
                glPushMatrix();
                glTranslatef(s * arena, 0.0f, t);
                CGRenderUtils::drawCuboid3D(-0.1f, 0.0f, -0.1f, 0.1f, 1.0f, 0.1f, wR, wG, wB);
                glPopMatrix();
            }
        }

        // --- horizontal fence rails on all four sides ---
        for (int s = -1; s <= 1; s += 2)
        {
            CGRenderUtils::drawCuboid3D(-arena, 0.55f, s * arena - 0.05f,
                                        arena, 0.70f, s * arena + 0.05f, wR, wG, wB);
            CGRenderUtils::drawCuboid3D(s * arena - 0.05f, 0.55f, -arena,
                                        s * arena + 0.05f, 0.70f, arena, wR, wG, wB);
        }

        // --- a red barn behind the arena ---
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -arena - 3.0f);

        // walls : classic solid-red barn (dimmed by the daylight level)
        CGRenderUtils::drawCuboid3D(-3.0f, 0.0f, -2.0f, 3.0f, 3.0f, 2.0f,
                                    0.70f * L, 0.15f * L, 0.12f * L);

        // roof (two slopes + two gable triangles)
        glColor3f(0.45f * L, 0.10f * L, 0.08f * L);
        glBegin(GL_TRIANGLES);
        glVertex3f(-3.0f, 3.0f, 2.0f);  glVertex3f(3.0f, 3.0f, 2.0f);  glVertex3f(0.0f, 4.5f, 2.0f);
        glVertex3f(3.0f, 3.0f, -2.0f);  glVertex3f(-3.0f, 3.0f, -2.0f); glVertex3f(0.0f, 4.5f, -2.0f);
        glEnd();
        glBegin(GL_QUADS);
        glVertex3f(-3.0f, 3.0f, 2.0f); glVertex3f(0.0f, 4.5f, 2.0f); glVertex3f(0.0f, 4.5f, -2.0f); glVertex3f(-3.0f, 3.0f, -2.0f);
        glVertex3f(0.0f, 4.5f, 2.0f);  glVertex3f(3.0f, 3.0f, 2.0f); glVertex3f(3.0f, 3.0f, -2.0f);  glVertex3f(0.0f, 4.5f, -2.0f);
        glEnd();

        // barn door
        CGRenderUtils::drawCuboid3D(-0.8f, 0.0f, 1.95f, 0.8f, 1.8f, 2.05f, 0.30f * L, 0.15f * L, 0.08f * L);
        glPopMatrix();

        glEnable(GL_CULL_FACE);
        if (lightingWasOn) glEnable(GL_LIGHTING);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // =================================================================
    //  Burn marks left on the grass wherever a skill exploded, so the
    //  battlefield visibly takes damage as the fight goes on.
    // =================================================================
    void drawScorchMarks()
    {
        int n = (scorchCount < MAX_SCORCH) ? scorchCount : MAX_SCORCH;
        if (n <= 0) return;

        GLboolean lightingWasOn;
        glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (int i = 0; i < n; i++)
        {
            float x = scorches[i].x;
            float z = scorches[i].z;
            float r = 0.9f; // half-size of the burn patch

            glColor4f(0.10f, 0.08f, 0.05f, 0.6f);
            glBegin(GL_QUADS);
            glVertex3f(x - r, 0.02f, z + r);
            glVertex3f(x + r, 0.02f, z + r);
            glVertex3f(x + r, 0.02f, z - r);
            glVertex3f(x - r, 0.02f, z - r);
            glEnd();
        }

        glDisable(GL_BLEND);
        if (lightingWasOn) glEnable(GL_LIGHTING);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // One tree, made of two crossed textured quads so it looks 3D from any
    // angle. L is the current daylight level (dims the tree at dusk).
    void drawTree(float x, float z, float size, float L)
    {
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        glColor4f(L, L, L, 1.0f);
        float w = size * 0.7f, h = size;
        for (int q = 0; q < 2; q++) // two quads crossed at 90 degrees
        {
            glPushMatrix();
            glRotatef(q * 90.0f, 0.0f, 1.0f, 0.0f);
            glBegin(GL_QUADS);
            glTexCoord2f(0, 0); glVertex3f(-w, 0.0f, 0.0f);
            glTexCoord2f(1, 0); glVertex3f(w, 0.0f, 0.0f);
            glTexCoord2f(1, 1); glVertex3f(w, h, 0.0f);
            glTexCoord2f(0, 1); glVertex3f(-w, h, 0.0f);
            glEnd();
            glPopMatrix();
        }
        glPopMatrix();
    }

    // A small clump of grass blades (three little triangles).
    void drawGrassTuft(float x, float z)
    {
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        glBegin(GL_TRIANGLES);
        for (int b = 0; b < 3; b++)
        {
            float off = (b - 1) * 0.12f;
            glVertex3f(off - 0.05f, 0.0f, 0.0f);
            glVertex3f(off + 0.05f, 0.0f, 0.0f);
            glVertex3f(off,         0.45f, 0.0f);
        }
        glEnd();
        glPopMatrix();
    }

    // =================================================================
    //  Background scenery : sun, clouds, rolling hills and a ring of
    //  trees around the arena. All dimmed by envLight so the backdrop
    //  changes from day to dusk together with the rest of the farm.
    // =================================================================
    void drawBackground()
    {
        const float PI = 3.14159265f;
        float L = envLight;

        GLboolean lightingWasOn;
        glGetBooleanv(GL_LIGHTING, &lightingWasOn);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);

        // --- sun : yellow by day, sunset-orange as it gets dark ---
        glPushMatrix();
        glTranslatef(-30.0f, 35.0f, -60.0f);
        glColor4f(1.0f, 0.55f + 0.40f * L, 0.10f + 0.35f * L, 1.0f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, 0.0f, 0.0f);
        for (int i = 0; i <= 20; i++)
        {
            float a = (float)i / 20.0f * 2.0f * PI;
            glVertex3f(cos(a) * 5.0f, sin(a) * 5.0f, 0.0f);
        }
        glEnd();
        glPopMatrix();

        // --- clouds : a few white blocks high in the sky ---
        float cloudX[4] = {20.0f, -15.0f, 40.0f, -35.0f};
        float cloudY[4] = {30.0f, 38.0f, 25.0f, 33.0f};
        float cloudZ[4] = {-50.0f, -45.0f, -30.0f, -40.0f};
        for (int i = 0; i < 4; i++)
        {
            float c = 0.95f * L + 0.05f;
            CGRenderUtils::drawCuboid3D(cloudX[i] - 4.0f, cloudY[i] - 1.0f, cloudZ[i] - 1.5f,
                                        cloudX[i] + 4.0f, cloudY[i] + 1.0f, cloudZ[i] + 1.5f,
                                        c, c, c);
        }

        // --- many rolling hills, in two overlapping rings (forest hills) ---
        for (int ring = 0; ring < 2; ring++)
        {
            float radius = (ring == 0) ? 30.0f : 42.0f;
            int hillCount = (ring == 0) ? 24 : 30;
            for (int i = 0; i < hillCount; i++)
            {
                float ang = ((float)i / hillCount + ring * 0.02f) * 2.0f * PI;
                float hx = cos(ang) * radius;
                float hz = sin(ang) * radius;
                float tx = -sin(ang), tz = cos(ang); // tangent (hill base direction)
                float gw = 13.0f;                     // half width of the hill base
                float gh = (ring == 0 ? 5.0f : 8.0f) + 2.0f * (i % 3);
                float g = (i % 2) ? 0.50f : 0.40f;
                glColor4f(0.16f * L, g * L, 0.18f * L, 1.0f);
                glBegin(GL_TRIANGLES);
                glVertex3f(hx - tx * gw, 0.0f, hz - tz * gw);
                glVertex3f(hx + tx * gw, 0.0f, hz + tz * gw);
                glVertex3f(hx, gh, hz);
                glEnd();
            }
        }

        // --- a thick forest of trees in three rings around the arena ---
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, sceneRes.treeTexture);
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.3f);
        float treeRadius[3] = {18.0f, 24.0f, 30.0f};
        int treeRingCount[3] = {16, 22, 28};
        for (int ring = 0; ring < 3; ring++)
        {
            for (int i = 0; i < treeRingCount[ring]; i++)
            {
                float ang = ((float)i / treeRingCount[ring] + ring * 0.03f) * 2.0f * PI;
                float size = 5.5f + ring * 0.8f + (i % 2);
                drawTree(cos(ang) * treeRadius[ring], sin(ang) * treeRadius[ring], size, L);
            }
        }
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);

        // --- little tufts of grass scattered across the field ---
        glColor3f(0.22f * L, 0.52f * L, 0.16f * L);
        for (int i = 0; i < 90; i++)
        {
            // deterministic pseudo-random spread (no rand needed)
            float gx = sin(i * 12.9898f) * 16.0f;
            float gz = cos(i * 4.1414f) * 16.0f;
            drawGrassTuft(gx, gz);
        }

        glEnable(GL_CULL_FACE);
        if (lightingWasOn) glEnable(GL_LIGHTING);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

private:
    void applyGravityAndMovement()
    {
        player1.isGrounded = false;
        player2.isGrounded = false;

        float gravity = 0.02f;
        player1.velocity.y -= gravity;
        player2.velocity.y -= gravity;

        player1.pos.x += player1.velocity.x;
        player1.pos.y += player1.velocity.y;
        player1.pos.z += player1.velocity.z;

        player2.pos.x += player2.velocity.x;
        player2.pos.y += player2.velocity.y;
        player2.pos.z += player2.velocity.z;

        // Friction
        player1.velocity.x *= 0.85f;
        player1.velocity.z *= 0.85f;
        player2.velocity.x *= 0.85f;
        player2.velocity.z *= 0.85f;

        // Update animation time
        float speed1 = std::sqrt(player1.velocity.x * player1.velocity.x + player1.velocity.z * player1.velocity.z);
        player1.animTime += 0.05f + speed1 * 0.5f;

        float speed2 = std::sqrt(player2.velocity.x * player2.velocity.x + player2.velocity.z * player2.velocity.z);
        player2.animTime += 0.05f + speed2 * 0.5f;
    }

    void resolveCharacterCollisions()
    {
        float dx = player1.pos.x - player2.pos.x;
        float dy = player1.pos.y - player2.pos.y;
        float dz = player1.pos.z - player2.pos.z;

        float absDx = std::abs(dx);
        float absDy = std::abs(dy);
        float absDz = std::abs(dz);

        float extX = player1.extents.x + player2.extents.x;
        float extY = player1.extents.y + player2.extents.y;
        float extZ = player1.extents.z + player2.extents.z;

        float penX = extX - absDx;
        float penY = extY - absDy;
        float penZ = extZ - absDz;

        if (penX > 0 && penY > 0 && penZ > 0)
        {
            if (!player1.isGrounded || !player2.isGrounded)
            {
                player1.velocity.x = 0;
                player1.velocity.z = 0;
                player2.velocity.x = 0;
                player2.velocity.z = 0;
            }

            if (penX < penZ && penX < penY)
            {
                float sign = (dx > 0) ? 1.0f : -1.0f;
                player1.pos.x += sign * (penX / 2.0f);
                player2.pos.x -= sign * (penX / 2.0f);
            }
            else if (penZ < penX && penZ < penY)
            {
                float sign = (dz > 0) ? 1.0f : -1.0f;
                player1.pos.z += sign * (penZ / 2.0f);
                player2.pos.z -= sign * (penZ / 2.0f);
            }
            else
            {
                float sign = (dy > 0) ? 1.0f : -1.0f;
                player1.pos.y += sign * (penY / 2.0f);
                player2.pos.y -= sign * (penY / 2.0f);
                if (sign > 0)
                {
                    player1.velocity.y = 0;
                    player1.isGrounded = true;
                }
                else
                {
                    player2.velocity.y = 0;
                    player2.isGrounded = true;
                }
            }
        }
    }

    void enforceArenaBounds(Character &p, float boundary)
    {
        if (p.pos.y <= p.extents.y)
        {
            p.pos.y = p.extents.y;
            p.velocity.y = 0.0f;
            p.isGrounded = true;
        }
        if (p.pos.x > boundary)
            p.pos.x = boundary;
        if (p.pos.x < -boundary)
            p.pos.x = -boundary;
        if (p.pos.z > boundary)
            p.pos.z = boundary;
        if (p.pos.z < -boundary)
            p.pos.z = -boundary;
    }

    void resolveArenaBounds()
    {
        enforceArenaBounds(player1, 12.0f - player1.extents.x);
        enforceArenaBounds(player2, 12.0f - player2.extents.x);
    }

    void processAttacksAndSkills()
    {
        if (player1.skillCooldown > 0)
            player1.skillCooldown--;
        if (player2.skillCooldown > 0)
            player2.skillCooldown--;

        // P1 attack
        if (player1.attackTimer > 0)
        {
            player1.attackTimer--;
            Vec3 hitExt = {1.2f, 0.4f, 3.0f};
            Vec3 hitPos = {
                player1.pos.x + (player1.extents.x + hitExt.x) * player1.facingDir.x,
                player1.pos.y,
                player1.pos.z + (player1.extents.z + hitExt.z) * player1.facingDir.z};

            if (checkAABBIntersect(hitPos, hitExt, player2.pos, player2.extents))
            {
                if (!player2.isDead)
                {
                    player2.hp -= 1.0f;
                    if (player2.hp <= 0.0f)
                    {
                        player2.hp = 0.0f;
                        player2.isDead = true;
                    }
                }
                player2.velocity.x = 0.5f * player1.facingDir.x;
                player2.velocity.z = 0.5f * player1.facingDir.z;
                player2.velocity.y = 0.2f;
                player1.attackTimer = 0;
            }
        }

        // P2 attack
        if (player2.attackTimer > 0)
        {
            player2.attackTimer--;

            player2.ghostHead = (player2.ghostHead + 1) % GHOST_COUNT;

            // Record the status
            player2.ghosts[player2.ghostHead].pos = player2.pos;
            player2.ghosts[player2.ghostHead].facingDir = player2.facingDir;
            player2.ghosts[player2.ghostHead].attackTimer = player2.attackTimer;
            player2.ghosts[player2.ghostHead].active = true;

            Vec3 hitExt = {0.6f, 0.2f, 1.5f};
            Vec3 hitPos = {
                player2.pos.x + (player2.extents.x + hitExt.x) * player2.facingDir.x,
                player2.pos.y,
                player2.pos.z + (player2.extents.z + hitExt.z) * player2.facingDir.z};

            if (checkAABBIntersect(hitPos, hitExt, player1.pos, player1.extents))
            {
                if (!player1.isDead)
                {
                    player1.hp -= 1.0f;
                    if (player1.hp <= 0.0f)
                    {
                        player1.hp = 0.0f;
                        player1.isDead = true;
                    }
                }
                player1.velocity.x = 0.5f * player2.facingDir.x;
                player1.velocity.z = 0.5f * player2.facingDir.z;
                player1.velocity.y = 0.2f;
                player2.attackTimer = 0;
            }
        }
        else
        {
            for (int i = 0; i < GHOST_COUNT; ++i)
            {
                player2.ghosts[i].active = false;
            }
        }

        // Shockwave
        if (shockwave.isActive)
        {
            shockwave.pos.x += shockwave.velocity.x;
            shockwave.pos.z += shockwave.velocity.z;

            bool hit = false; // Flag whether an impact was triggered

            // Wall collision check
            if (std::abs(shockwave.pos.x) > 14.0f || std::abs(shockwave.pos.z) > 14.0f)
            {
                hit = true;
            }
            // Player collision check
            else if (!player2.isDead && checkAABBIntersect(shockwave.pos, shockwave.extents, player2.pos, player2.extents))
            {
                player2.hp -= (20.0f / 3.0f);
                if (player2.hp <= 0.0f)
                {
                    player2.hp = 0.0f;
                    player2.isDead = true;
                }
                player2.velocity.x = shockwave.velocity.x * 2.0f;
                player2.velocity.z = shockwave.velocity.z * 2.0f;
                player2.velocity.y = 0.5f;
                hit = true;
            }

            // Key decoupling point: on impact, detonate and destroy the physics wave!
            if (hit)
            {
                shockwave.isActive = false;    // destroy the physical wave
                explosion.pos = shockwave.pos; // hand its position to the explosion FX
                explosion.timer = 15;          // play the explosion for 15 frames
                addScorch(explosion.pos);      // burn the ground where it landed
            }
        }

        // Remember to tick down the explosion timer at the end of the physics loop
        if (explosion.timer > 0)
        {
            explosion.timer--;
        }

        // Player 2 egg volley : launch and fly the eggs
        updateEggs();
    }
};

// let's say, if other places need to use this, just use myvirtualworld.sceneRes to access.
// extern SceneResources sceneRes;
