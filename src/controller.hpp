#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <map>
#include <glm/glm.hpp>
#include "settings.hpp"
#include "viewer.hpp"
#include "sound.hpp"

#define SPEED 8.0f
#define MOUSE_SPEED 0.001f
#define GRAVITY 1.0f

class Viewer;

class Controller {
public:
  Controller(Viewer* viewer, Settings* settings);
  ~Controller();

  void setPosition(glm::vec3& p);
  glm::vec3 getPosition();
  glm::vec3 getDirection();
  void reset();
  void update();
  void setHorizontalAngle(float a);
  void setVerticalAngle(float a);

  bool isJumping() {
    return jumping;
  }

private:
  bool checkKeyJustPressed(int k);
  bool checkMouseJustPressed(int i);

  Viewer* viewer;
  Settings* settings;
  double lastTime;
  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 velocity;
  float horizontalAngle;
  float verticalAngle;
  int skipMovements;
  std::map<int, bool> keysPressed;
  std::map<int, bool> mousePressed;
  bool jumping;
};

#endif
