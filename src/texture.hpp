#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <map>
#include <string>

class Texture {
public:
  static void initialize();
  static Texture* loadOrGet(std::string fname, bool useMipmaps);
  static void freeLoadedTextures();

  Texture() {}
  Texture(std::string fname, int width, int height, void* data, bool useMipmaps);
  Texture(GLuint texId, int width, int height);
  ~Texture();

  GLuint getTextureId() {
    return texId;
  }

  static void saveTextureToFile(unsigned char* pixels, int width, int height, std::string filename);

protected:
  GLuint texId;
  std::string name;
  int width;
  int height;

private:
  static std::map<std::string, Texture*> loadedTextures;
};

class TextureCube: public Texture {
public:
  static TextureCube* loadOrGet(std::string fnames[6]);

  TextureCube(std::string fnames[6], int width, int height, void* data[6]);

private:
  // TODO: free these.
  static std::map<std::string, TextureCube*> loadedTextureCubes;
};

#endif
