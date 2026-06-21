#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <windows.h>   // for PlaySound
#include <mmsystem.h>  // background music (links with winmm)
#include "OpenglSoftware/glut.h"

#include "CGLabmain.hpp"

// Ideally, you should include only the files that you wish
//   to use, however, all of them are included here just for
//   convenience.
#include "main.hpp"

// IMPORTANT:
//   Change the namespace scope below corresponding to
//   to the lab number which you wish to use.
//  using main::MyVirtualWorld;

MyVirtualWorld myvirtualworld;

using namespace std;

MyWindow window;
MyWorld world;
MyViewer viewer;
MySetting setting;
MyAxis worldaxis;

void myDisplayFunc(void)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   // Lock camera parameters to fixed values
   viewer.centerX = 0.0;
   viewer.centerY = 1.8;
   viewer.centerZ = 0.0;
   viewer.eyeX = 0.0;
   viewer.eyeY = 2.5;
   viewer.eyeZ = 12.0;
   myViewingInit();

   glPushMatrix();

   glTranslatef(world.posX, world.posY, world.posZ);
   // Lock rotation for fixed side view
   // glRotatef(world.rotateX, 1.0f, 0.0f, 0.0f);
   // glRotatef(world.rotateY, 0.0f, 1.0f, 0.0f);
   // glRotatef(world.rotateZ, 0.0f, 0.0f, 1.0f);
   glScalef(world.scaleX, world.scaleY, world.scaleZ);

   worldaxis.draw();

   myvirtualworld.draw();

   glPopMatrix();

   glFlush(); // send any buffered output to be rendered
   glutSwapBuffers();
}

void myReshapeFunc(int width, int height)
{
   window.width = width;
   window.height = height;
   glViewport(0, 0, width, height);
   viewer.aspectRatio = static_cast<GLdouble>(width) / height;
}

void myKeyboardFunc(unsigned char key, int x, int y)
{
   // create a boost value for velocity changes, which will be applied to the player's velocity when movement keys are pressed.
   // This allows for easy adjustments to the character's speed by changing this single variable.
   GLfloat speedPulse = 0.2f;

   switch (key)
   {
   case 'a':
   case 'A':
      if (!myvirtualworld.player1.isDead)
      {
         myvirtualworld.player1.facingDir = {-1.0f, 0.0f, 0.0f};
         if (!myvirtualworld.player1.isAiming)
            myvirtualworld.player1.velocity.x = -speedPulse;
      }
      break;
   case 'd':
   case 'D':
      if (!myvirtualworld.player1.isDead)
      {
         myvirtualworld.player1.facingDir = {1.0f, 0.0f, 0.0f};
         if (!myvirtualworld.player1.isAiming)
            myvirtualworld.player1.velocity.x = speedPulse;
      }
      break;
   case 'w':
   case 'W':
   case ' ':
      // Character will only allowed to jump if they are currently grounded, which prevents double-jumping in mid-air
      if (myvirtualworld.player1.isGrounded && !myvirtualworld.player1.isDead)
      {
         myvirtualworld.player1.velocity.y = 0.4f;  // up boost
         myvirtualworld.player1.isGrounded = false; // lock to avoid double jump
      }
      break;
   case 'c':
   case 'C':
      if (!myvirtualworld.player1.isDead)
      {
         if (myvirtualworld.player1.attackTimer <= 0)
         {
            myvirtualworld.player1.attackTimer = 10;
         }
      }
      break;
   case 'v':
   case 'V':
      if (!myvirtualworld.player1.isDead && myvirtualworld.player1.skillCooldown <= 0)
      {
         myvirtualworld.player1.isAiming = false;
         myvirtualworld.player1.skillCooldown = 900; // 15 second cooldown

         // activate shockwave
         myvirtualworld.shockwave.isActive = true;
         myvirtualworld.shockwave.extents = {1.0f, 0.5f, 1.0f};

         // generate from the front of the caster
         myvirtualworld.shockwave.pos.x = myvirtualworld.player1.pos.x + myvirtualworld.player1.facingDir.x * 2.0f;
         myvirtualworld.shockwave.pos.y = 0.5f;
         myvirtualworld.shockwave.pos.z = myvirtualworld.player1.pos.z + myvirtualworld.player1.facingDir.z * 2.0f;

         // fast wave movement speed
         float waveSpeed = 0.4f;
         myvirtualworld.shockwave.velocity.x = myvirtualworld.player1.facingDir.x * waveSpeed;
         myvirtualworld.shockwave.velocity.y = 0.0f;
         myvirtualworld.shockwave.velocity.z = myvirtualworld.player1.facingDir.z * waveSpeed;
      }
      break;
   case 'o':
   case 'O':
      if (myvirtualworld.player2.attackTimer <= 0 && !myvirtualworld.player2.isDead)
      {
         myvirtualworld.player2.attackTimer = 10;
      }
      break;
   case 'p':
   case 'P':
      if (!myvirtualworld.player2.isDead && myvirtualworld.player2.skillCooldown <= 0)
      {
         myvirtualworld.startEggVolley();
         myvirtualworld.player2.skillCooldown = 900; // 15 second cooldown

         // player 2 stops moving while firing
         myvirtualworld.player2.velocity.x = 0.0f;
         myvirtualworld.player2.velocity.z = 0.0f;
      }
      break;
   case 27:
      exit(1);
      break; // ESC to quit
   }

   // NEVER EVER use world.move() here！
   // Force render for the next tickTime()
   glutPostRedisplay();
}

void mySpecialFunc(int key, int x, int y)
{
   switch (key)
   {
   case GLUT_KEY_DOWN:
      break;
   case GLUT_KEY_UP:
      if (myvirtualworld.player2.isGrounded && !myvirtualworld.player2.isDead)
      {
         myvirtualworld.player2.velocity.y = 0.4f;  // up boost
         myvirtualworld.player2.isGrounded = false; // lock to avoid double jump
      }
      break;
   case GLUT_KEY_LEFT:
      if (!myvirtualworld.player2.isDead)
      {
         myvirtualworld.player2.velocity.x = -0.2f;
         myvirtualworld.player2.facingDir = {-1.0f, 0.0f, 0.0f};
      }
      break;
   case GLUT_KEY_RIGHT:
      if (!myvirtualworld.player2.isDead)
      {
         myvirtualworld.player2.velocity.x = 0.2f;
         myvirtualworld.player2.facingDir = {1.0f, 0.0f, 0.0f};
      }
      break;
   case GLUT_KEY_HOME:
      myDataInit();
      break;
   case GLUT_KEY_F1:
      setting.shadingMode = !setting.shadingMode;
      if (setting.shadingMode)
         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      else
         glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      break;
   case GLUT_KEY_F2:
      worldaxis.toggle();
      break;
   case GLUT_KEY_F3:
      GLboolean lightingIsOn;
      glGetBooleanv(GL_LIGHTING, &lightingIsOn);
      if (lightingIsOn == GL_TRUE)
         glDisable(GL_LIGHTING);
      else
         glEnable(GL_LIGHTING);
      break;
   }
   glutPostRedisplay();
}

void myMouseFunc(int button, int state, int x, int y)
{
   y = window.height - y;
   switch (button)
   {
   case GLUT_RIGHT_BUTTON:
      if (state == GLUT_DOWN && !setting.mouseRightMode)
      {
         setting.mouseX = x;
         setting.mouseY = y;
         setting.mouseRightMode = true;
      }
      if (state == GLUT_UP && setting.mouseRightMode)
      {
         setting.mouseRightMode = false;
      }
      break;
   case GLUT_LEFT_BUTTON:
      if (state == GLUT_DOWN && !setting.mouseLeftMode)
      {
         setting.mouseX = x;
         setting.mouseY = y;
         setting.mouseLeftMode = true;
      }
      if (state == GLUT_UP && setting.mouseLeftMode)
      {
         setting.mouseLeftMode = false;
      }
      break;
   }
}

void myMotionFunc(int x, int y)
{
   y = window.height - y;
   GLint xinc = x - setting.mouseX;
   GLint yinc = y - setting.mouseY;

   if (setting.mouseRightMode)
   {
      world.rotate(0.0f, 0.0f, -xinc * 0.5);
   }
   if (setting.mouseLeftMode)
   {
      world.rotate(-yinc * 0.5, xinc * 0.5, 0.0f);
   }

   setting.mouseX = x;
   setting.mouseY = y;
   glutPostRedisplay();
}

void myTimerFunc(int value)
{
   // 1. push physical time forward by one tick (16 ms)
   myvirtualworld.tickTime();

   // 2. Force OpenGL to call myDisplayFunc() again, which will render the next frame of the animation
   glutPostRedisplay();

   // 3. Register the timer callback function again for the next tick, creating a continuous loop
   glutTimerFunc(16, myTimerFunc, 0);
}

void myDataInit()
{
   window.title = "Farm Arena - Cowboy vs MC Chicken";
   window.posX = 100;
   window.posY = 100;
   window.width = 800;
   window.height = 500;

   world.rotateX = 0.0;
   world.rotateY = 0.0;
   world.rotateZ = 0.0;
   world.posX = 0.0;
   world.posY = 0.0;
   world.posZ = 0.0;
   world.scaleX = 1.0;
   world.scaleY = 1.0;
   world.scaleZ = 1.0;

   viewer.eyeX = 0.0;
   viewer.eyeY = 6.0;
   viewer.eyeZ = 18.0;
   viewer.centerX = 0.0;
   viewer.centerY = 0.0;
   viewer.centerZ = 0.0;
   viewer.upX = 0.0;
   viewer.upY = 1.0;
   viewer.upZ = 0.0;
   viewer.zNear = 0.1;
   viewer.zFar = 500.0;
   viewer.fieldOfView = 60.0;
   viewer.aspectRatio = static_cast<GLdouble>(window.width) / window.height;

   setting.posInc = 1.0;
   setting.angleInc = 2.0;
   setting.mouseX = 0;
   setting.mouseY = 0;

   setting.mouseRightMode = false;
   setting.mouseLeftMode = false;

   setting.shadingMode = true;
}

void myViewingInit()
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(viewer.fieldOfView,
                  viewer.aspectRatio,
                  viewer.zNear,
                  viewer.zFar);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   gluLookAt(viewer.eyeX, viewer.eyeY, viewer.eyeZ,
             viewer.centerX, viewer.centerY, viewer.centerZ,
             viewer.upX, viewer.upY, viewer.upZ);
}

void myLightingInit()
{
   static GLfloat ambient[] = {0.0f, 0.0f, 0.0f, 1.0f};
   static GLfloat diffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
   static GLfloat specular[] = {1.0f, 1.0f, 1.0f, 1.0f};
   static GLfloat specref[] = {1.0f, 1.0f, 1.0f, 1.0f};
   static GLfloat position[] = {10.0f, 10.0f, 10.0f, 1.0f};
   short shininess = 128;

   glDisable(GL_LIGHTING);
   glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
   glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
   glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
   glLightfv(GL_LIGHT0, GL_POSITION, position);
   glEnable(GL_LIGHT0);

   glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
   glEnable(GL_COLOR_MATERIAL);

   glMaterialfv(GL_FRONT, GL_SPECULAR, specref);
   glMateriali(GL_FRONT, GL_SHININESS, shininess);

   glEnable(GL_NORMALIZE);
}

void myInit()
{
   myDataInit();

   glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
   glutInitWindowPosition(window.posX, window.posY); // Set top-left position
   glutInitWindowSize(window.width, window.height);  // Set width and height
   glutCreateWindow(window.title.c_str());           // Create display window

   glutDisplayFunc(myDisplayFunc); // Specify the display callback function
   glutReshapeFunc(myReshapeFunc);
   glutKeyboardFunc(myKeyboardFunc);
   glutSpecialFunc(mySpecialFunc);
   glutMotionFunc(myMotionFunc);
   glutMouseFunc(myMouseFunc);

   glPointSize(4.0);
   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_LESS);
   glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   glFrontFace(GL_CCW);
   glShadeModel(GL_SMOOTH);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

   glEnable(GL_CULL_FACE);

   myViewingInit();

   myLightingInit();

   myvirtualworld.init();
   glutTimerFunc(16, myTimerFunc, 0);
}

void myWelcome()
{
   cout << "*****************************************************************\n";
   cout << "*                   TCG6223 Computer Graphics                   *\n";
   cout << "*                       Farm Arena V1.1                         *\n";
   cout << "*****************************************************************\n";
   cout << "| Controls:                                                     |\n";
   cout << "|   COWBOY  (Player 1)                                          |\n";
   cout << "|   <a>,<d>                 => Move Left / Right                |\n";
   cout << "|   <Space>,<w>             => Jump                             |\n";
   cout << "|   <c>                     => Shoot / Attack                   |\n";
   cout << "|   <v>                     => Special (fire shockwave)         |\n";
   cout << "|                                                               |\n";
   cout << "|   CHICKEN (Player 2)                                          |\n";
   cout << "|   <Left>,<Right Arrow>    => Move Left / Right                |\n";
   cout << "|   <Up Arrow>              => Jump                             |\n";
   cout << "|   <o>                     => Peck / Attack                    |\n";
   cout << "|   <p>                     => Special (egg volley)             |\n";
   cout << "|                                                               |\n";
   cout << "*****************************************************************\n";
   cout << "|                      H A V E   F U N  !!!                     |\n";
   cout << "*****************************************************************\n";
}

//--------------------------------------------------------------------
int main(int argc, char **argv)
{
   glutInit(&argc, argv);

   myWelcome();

   myInit();

   // Start looping background music. Drop a WAV file at assets/music.wav.
   // If the file is missing, the game just runs silently (no crash).
   PlaySound("assets/music.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);

   glutMainLoop(); // Display everything and wait
}
//--------------------------------------------------------------------
