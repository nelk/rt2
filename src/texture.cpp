
#include <FreeImage.h>
#include <iostream>
#include "texture.hpp"

std::map<std::string, Texture*> Texture::loadedTextures;

static bool fiError = false;

void fiMessageFunction(FREE_IMAGE_FORMAT fif, const char *msg) {
  std::cerr << (int)fif << ": " << msg << std::endl;
  fiError = true;
}

void Texture::initialize() {
  FreeImage_SetOutputMessage(fiMessageFunction);
}

Texture* Texture::loadOrGet(std::string fname, bool useMipmaps) {
  if (loadedTextures.find(fname) != loadedTextures.end()) {
    return loadedTextures[fname];
  }

  FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFileType(fname.c_str(), 0), fname.c_str());
  FIBITMAP *pImage = FreeImage_ConvertTo24Bits(bitmap);

  const int texWidth = FreeImage_GetWidth(bitmap);
  const int texHeight = FreeImage_GetHeight(bitmap);

  FreeImage_Unload(bitmap);

  Texture* texture = new Texture(fname, texWidth, texHeight, (void*) FreeImage_GetBits(pImage), useMipmaps);

  FreeImage_Unload(pImage);

  if (fiError) {
    fiError = false;
    return 0;
  }

  loadedTextures[fname] = texture;
  std::cout << "Loaded Texture " << fname << std::endl;

  return texture;
}

void Texture::freeLoadedTextures() {
  for (std::map<std::string, Texture*>::iterator it = loadedTextures.begin(); it != loadedTextures.end(); it++) {
    delete it->second;
  }
  loadedTextures.clear();
}

Texture::Texture(std::string fname, int width, int height, void* data, bool useMipmaps): name(fname), width(width), height(height) {
  texId = 0;
  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_2D, texId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // Note: Assuming texture was BGR.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
  if (useMipmaps) {
    glGenerateMipmap(GL_TEXTURE_2D);
  }
}

Texture::Texture(GLuint texId, int width, int height): texId(texId), name(""), width(width), height(height) {}

Texture::~Texture() {
  glDeleteTextures(1, &texId);
}

void Texture::saveTextureToFile(unsigned char* pixels, int width, int height, std::string filename) {
  FIBITMAP* img = FreeImage_ConvertFromRawBits(pixels, width, height, 3*width, 24, 0xFF0000, 0x00FF00, 0x0000FF, false);
  FreeImage_Save(FIF_PNG, img, filename.c_str(), 0);
  FreeImage_Unload(img);
}



std::map<std::string, TextureCube*> TextureCube::loadedTextureCubes;

TextureCube* TextureCube::loadOrGet(std::string fnames[6]) {
  if (loadedTextureCubes.find(fnames[0]) != loadedTextureCubes.end()) {
    return loadedTextureCubes[fnames[0]];
  }

  void* data[6];
  FIBITMAP* pImages[6];
  int texWidth, texHeight;

  int i;
  for (i = 0; !fiError && i < 6; i++) {
    std::string fname = fnames[i];

    FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFileType(fname.c_str(), 0), fname.c_str());
    pImages[i] = FreeImage_ConvertTo24Bits(bitmap);

    texWidth = FreeImage_GetWidth(bitmap);
    texHeight = FreeImage_GetHeight(bitmap);

    FreeImage_Unload(bitmap);

    data[i] = (void*) FreeImage_GetBits(pImages[i]);
  }

  TextureCube* texture = 0;
  if (!fiError) {
    texture = new TextureCube(fnames, texWidth, texHeight, data);
    loadedTextureCubes[fnames[0]] = texture;
    std::cout << "Loaded TextureCube " << fnames[0] << std::endl;
  }

  i--;
  for (; i >= 0; i--) {
    FreeImage_Unload(pImages[i]);
  }

  fiError = false;
  return texture;
}

TextureCube::TextureCube(std::string fnames[6], int width, int height, void* data[6]) {
  this->name = fnames[0];
  this->width = width;
  this->height = height;
  texId = 0;

  glGenTextures(1, &texId);
  glBindTexture(GL_TEXTURE_CUBE_MAP, texId);

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  // Note: Assuming texture was BGR.
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[0]);
  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[1]);
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[2]);
  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[3]);
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[4]);
  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data[5]);
}

