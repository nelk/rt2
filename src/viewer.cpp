#include <cstdlib>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "texture.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "sound.hpp"

#include "viewer.hpp"
#include "controller.hpp"

#define MIN_REQUIRED_COLOUR_ATTACHMENTS 6
#define RENDER_DEBUG_IMAGES false
#define TARGET_FPS 60
#define TARGET_FRAME_DELTA 0.01666667
#define FPS_SAMPLE_RATE 20

// TODO.
#define NUM_SPHERES 3

void window_size_callback(GLFWwindow* window, int width, int height) {
  Viewer* viewer = (Viewer*)glfwGetWindowUserPointer(window);
  viewer->updateSize(width, height);
  viewer->getController()->reset();
}

void window_focus_callback(GLFWwindow* window, int focussed) {
  if (focussed == GL_TRUE) {
    Viewer* viewer = (Viewer*)glfwGetWindowUserPointer(window);
    viewer->getController()->reset();
  }
}

bool checkGLErrors(std::string msg) {
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    std::cerr << "OpenGL error " << msg << ": " << error << " - " << glewGetErrorString(error) << std::endl;
    return false;
  }
  return true;
}

bool checkGLFramebuffer() {
  GLenum frameBufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (frameBufferStatus != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer check failed! " << frameBufferStatus << " = " << glewGetErrorString(frameBufferStatus) << std::endl;
    return false;
  }
  return true;
}

Viewer::Viewer(): width(DEFAULT_WIDTH), height(DEFAULT_HEIGHT) {
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Open a window and create its OpenGL context
  window = glfwCreateWindow(width, height, "Real Time Ray Tracer", NULL, NULL);

  if( window == NULL ){
    std::cerr << "Failed to open GLFW window. This application requires OpenGL 3.3 support." << std::endl;
    glfwTerminate();
    return;
  }

  // Setup callbacks for window.
  glfwSetWindowUserPointer(window, (void*)this);
  glfwSetWindowSizeCallback(window, window_size_callback);
  glfwSetWindowFocusCallback(window, window_focus_callback);

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

  // Ensure we can capture the escape key.
  glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
}

void Viewer::updateSize(int width, int height) {
  this->width = width;
  this->height = height;

  glBindRenderbuffer(GL_RENDERBUFFER, depthRenderBuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
}

bool Viewer::initialize() {
  // Initialize OpenAL.
  if (!Sound::initialize()) {
    std::cerr << "Couldn't initialize OpenAL" << std::endl;
    return false;
  }

  settings = new Settings();
  controller = new Controller(this, settings);

  // Initial settings (all start on).
  settings->set(Settings::SSAO, false);
  settings->set(Settings::BLUR, false);
  settings->set(Settings::HIGHLIGHT_PICK, false);

  glfwMakeContextCurrent(window);

  // Initialize GLEW
  glewExperimental = true; // Needed for core profile
  if (glewInit() != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW" << std::endl;
    return false;
  }

  // Ignore invalid enum error from glew call.
  glGetError();

  GLint maxAttachments;
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachments);
  if (maxAttachments < MIN_REQUIRED_COLOUR_ATTACHMENTS) {
    std::cerr << "Only " << maxAttachments << " supported FBO Colour Attachments, but this program requires " << MIN_REQUIRED_COLOUR_ATTACHMENTS << std::endl;
  }

  GLint maxUniformBlockSize;
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUniformBlockSize);
  std::cerr << "Max uniform block size: " << maxUniformBlockSize << std::endl;


  // Initialize textures.
  Texture::initialize();

  std::string texture_paths[] =
    { "textures/NiagaraFalls2/posx.jpg"
    , "textures/NiagaraFalls2/negx.jpg"
    , "textures/NiagaraFalls2/negy.jpg" // Flip y.
    , "textures/NiagaraFalls2/posy.jpg"
    , "textures/NiagaraFalls2/posz.jpg"
    , "textures/NiagaraFalls2/negz.jpg"
    };
  skybox = TextureCube::loadOrGet(texture_paths);
  checkGLErrors("dksljfd");

  glGenVertexArrays(1, &vertexArrayId);
  glBindVertexArray(vertexArrayId);

  // Quad for drawing textures.
  static const GLfloat quadVBuffer[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    1.0f,  1.0f, 0.0f,
  };

  glGenBuffers(1, &quadVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, quadVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVBuffer), quadVBuffer, GL_STATIC_DRAW);


  // Compile GLSL programs.
  raytraceProgramId = loadShaders("shaders/raytrace.vert", "shaders/raytrace.frag");

  if (raytraceProgramId == 0) {
    return false;
  }

  // Scene-specific setup:
  glm::vec3 startPosition(0, 0, 0);
  controller->setHorizontalAngle(0);
  controller->setPosition(startPosition);

  // Create scene.
  // Uniform buffer Objects.
  glGenBuffers(1, &sphereUBO);
  glGenBuffers(1, &materialUBO);
  glGenBuffers(1, &lightUBO);

  GLfloat spheres[8*NUM_SPHERES] = {
    0, 0, 3,
    1,
    glm::intBitsToFloat(0),
    0, 0, 0,

    2, 0.2, 15,
    5,
    glm::intBitsToFloat(1),
    0, 0, 0,

    -3, 5, 10,
    3,
    glm::intBitsToFloat(2),
    0, 0, 0

    /*
    0, -10000 - 8, 0,
    10000,
    glm::intBitsToFloat(0),
    0, 0, 0
    */
  };
  sphereBlockId = glGetUniformBlockIndex(raytraceProgramId, "SphereBlock");
  glUniformBlockBinding(raytraceProgramId, sphereBlockId, 0);
  glBindBuffer(GL_UNIFORM_BUFFER, sphereUBO);
  glBufferData(GL_UNIFORM_BUFFER, 819*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(spheres), spheres);

  GLfloat materials[] = {
    0, 0, 0, // ke.
    0, // Refraction percentage.
    0.1, 0.1, 0.1, // ka.
    1, // Index of Refraction.
    0.5, 0.0, 0.5, // kd.
    0.4, // Mirror percentage.
    0.8, 0.8, 0.8, // ks.
    30, // Shine.

    0, 0, 0,
    0.1,
    0.1, 0.1, 0.1,
    1,
    0.0, 0.6, 0.0,
    0.0,
    1.0, 1.0, 1.0,
    80,

    0, 0, 0,
    0.5,
    0, 0, 0,
    1.4, // IOR.
    0.0, 0.0, 0.8,
    0.3,
    0.3, 0.3, 0.3,
    75,
  };
  materialBlockId = glGetUniformBlockIndex(raytraceProgramId, "MaterialBlock");
  glUniformBlockBinding(raytraceProgramId, materialBlockId, 1);
  glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(materials), materials, GL_DYNAMIC_DRAW);

  GLfloat lights[] = {
    2, 0, -5, 0,
    0.8, 0.8, 0.8, 0,

    -4, 2, 1, 0,
    0.8, 0.0, 0.1, 0
  };
  lightBlockId = glGetUniformBlockIndex(raytraceProgramId, "LightBlock");
  glUniformBlockBinding(raytraceProgramId, lightBlockId, 2);
  glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(lights), lights, GL_DYNAMIC_DRAW);

  return true;
}


void Viewer::drawQuad() {
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, quadVertexBuffer);
  glVertexAttribPointer(
    0,                  // attribute 0.
    3,                  // size
    GL_FLOAT,           // type
    GL_FALSE,           // normalized?
    0,                  // stride
    (void*)0            // array buffer offset
  );

  glDrawArrays(GL_TRIANGLES, 0, 2*3);
  glDisableVertexAttribArray(0);
}


void Viewer::bindRenderTarget(GLuint renderTargetFBO) {

  checkGLErrors("bindRenderTarget start");
  // Render to target.
  if (renderTargetFBO == 0) {
    // Render to screen.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_FRONT_LEFT);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, renderTargetFBO);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
  }
  checkGLErrors("bindRenderTarget end");
}

void Viewer::renderScene(GLuint renderTargetFBO, const glm::vec3& cameraPosition, const glm::vec3& cameraDirection, double currentTime, double deltaTime, bool doPicking) {

  static GLuint rtCameraPositionId = glGetUniformLocation(raytraceProgramId, "cameraPosition");
  static GLuint rtCameraDirectionId = glGetUniformLocation(raytraceProgramId, "cameraDirection");
  static GLuint rtScreenResolutionId = glGetUniformLocation(raytraceProgramId, "screenResolution");

  static GLuint rtNumSpheresId = glGetUniformLocation(raytraceProgramId, "numSpheres");
  static GLuint rtNumLightsId = glGetUniformLocation(raytraceProgramId, "numLights");

  static GLuint rtSkyboxId = glGetUniformLocation(raytraceProgramId, "skyboxTexture");

  glUseProgram(raytraceProgramId);
  glViewport(0, 0, width, height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  bindRenderTarget(renderTargetFBO);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  glActiveTexture(GL_TEXTURE0 + 0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->getTextureId());
  glUniform1i(rtSkyboxId, 0);

  glUniform3fv(rtCameraPositionId, 1, &cameraPosition[0]);
  glUniform3fv(rtCameraDirectionId, 1, &cameraDirection[0]);

  float screenResolution[] = {width*1.0f, height*1.0f};
  glUniform2fv(rtScreenResolutionId, 1, &screenResolution[0]);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, sphereUBO);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, materialUBO);
  glBindBufferBase(GL_UNIFORM_BUFFER, 2, lightUBO);

  glUniform1i(rtNumLightsId, 2);
  glUniform1i(rtNumSpheresId, NUM_SPHERES);

  drawQuad();
}

void Viewer::run() {
  controller->reset();

  glBindVertexArray(vertexArrayId);

  double lastTime = glfwGetTime();
  long fpsDisplayCounter = 0;
  double lastFPSTime = lastTime;

  do {
    double currentTime = glfwGetTime();
    double deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    // Get camera info from keyboard and mouse input.
    controller->update();
    const glm::vec3& cameraPosition = controller->getPosition();
    const glm::vec3& cameraDirection = controller->getDirection();

    // Main render of scene.
    renderScene(0, cameraPosition, cameraDirection, currentTime, deltaTime, true);

    // Swap buffers
    glfwSwapBuffers(window);

    fpsDisplayCounter++;
    if (fpsDisplayCounter % FPS_SAMPLE_RATE == 0) {
      double fpsDeltaTime = float(currentTime - lastFPSTime);
      lastFPSTime = currentTime;
      std::cout << FPS_SAMPLE_RATE / fpsDeltaTime << "FPS" << std::endl;
    }

    checkGLErrors("loop");

    glfwPollEvents();
  } // Check if the ESC key was pressed or the window was closed
  while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
      glfwWindowShouldClose(window) == 0 );
}

Viewer::~Viewer() {
  delete controller;
  controller = NULL;

  delete settings;
  settings = NULL;

  Texture::freeLoadedTextures();

  glDeleteProgram(raytraceProgramId);
  glDeleteVertexArrays(1, &vertexArrayId);

  glDeleteBuffers(1, &sphereUBO);
  glDeleteBuffers(1, &materialUBO);
  glDeleteBuffers(1, &lightUBO);

  // Cleans up and closes window.
  glfwTerminate();
}

