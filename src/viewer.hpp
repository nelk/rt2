#ifndef VIEWER_H
#define VIEWER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <vector>
#include "controller.hpp"

#define DEFAULT_WIDTH 1024
#define DEFAULT_HEIGHT 768

class Controller;

class Viewer {
public:
  Viewer();
  ~Viewer();

  bool initialize();
  void run();

  /**
   * Render scene with deferred pipeline.
   * Set renderTarget=0 to render to screen.
   */
  void renderScene(GLuint renderTargetFBO, const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, double currentTime, double deltaTime, bool doPicking);

  void bindRenderTarget(GLuint renderTargetFBO);

  GLFWwindow* getWindow() {
    return window;
  }
  int getWidth() {
    return width;
  }
  int getHeight() {
    return height;
  }
  Controller* getController() {
    return controller;
  }

  void updateSize(int width, int height);
  void drawTextureWithQuadProgram(GLuint tex);
  void drawQuad();

private:
  int width, height;
  GLFWwindow* window;

  Settings* settings;
  Controller* controller;

  GLuint raytraceProgramId;
  GLuint depthRenderBuffer;

  GLuint vertexArrayId;
  GLuint quadVertexBuffer;

  GLuint sphereUBO;
  GLuint materialUBO;
  GLuint lightUBO;

  GLuint sphereBlockId;
  GLuint materialBlockId;
  GLuint lightBlockId;
};

bool checkGLFramebuffer();
bool checkGLErrors(std::string msg);

#endif
