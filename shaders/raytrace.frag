#version 330 core

#define MAX_MATERIALS 512
#define MAX_SPHERES 819
#define MAX_LIGHTS 100

struct Ray {
  vec3 p;
  vec3 d;
};

struct Intersection {
  bool hit;
  vec3 p;
  vec3 n;
  int materialId;
};

struct Sphere {
  vec3 center;
  float radius;
  int materialId;
};

struct Light {
  vec3 position;
  vec3 colour;
};

struct Material {
  vec3 ke;
  float refraction;
  vec3 ka;
  float ior;
  vec3 kd;
  float mirror;
  vec3 ks;
  float shine;
};


layout(location = 0) out vec3 colour;

layout (std140) uniform SphereBlock {
  Sphere spheres[MAX_SPHERES];
};
uniform int numSpheres;
layout (std140) uniform MaterialBlock {
  Material materials[MAX_SPHERES];
};
uniform int numMaterials;
layout (std140) uniform LightBlock {
  Light lights[MAX_LIGHTS];
};
uniform int numLights;

uniform vec2 screenResolution;
uniform vec3 cameraPosition;
uniform vec3 cameraDirection;


Intersection intersectSphere(Ray r, Sphere s) {
  const float EPSILON = 0.1;

  vec3 sphereToRay = r.p - s.center;
  float A = dot(r.d, r.d);
  float B = 2 * dot(r.d, sphereToRay);
  float C = dot(sphereToRay, sphereToRay) - s.radius * s.radius;

  float t = -1;
  float D;
  float q;

  if (A == 0) {
    if (B == 0) {
      t = -1;
    } else {
      t = -C/B;
    }
  } else {
    // Compute the discriminant D=b^2 - 4ac
    D = B*B - 4*A*C;
    if (D < 0) {
      t = -1;
    } else {
      // Two real roots.
      q = -(B + sign(B) * sqrt(D)) / 2.0;
      t = q/A;
      if (q != 0) {
        float other = C/q;
        if (t < EPSILON || (other > EPSILON && other < t)) {
          t = other;
        }
      }
    }
  }

  if (t > EPSILON) {
    vec3 poi = r.p + t * r.d;
    return Intersection(
      true,
      poi,
      normalize(poi - s.center),
      s.materialId
    );
  }
  return Intersection(false, vec3(0), vec3(0), 0);
}

vec3 lighting(vec3 viewer, Intersection it, Material mat, Light light) {
  // Blinn-phong.
  vec3 E = normalize(viewer - it.p);
  vec3 l = normalize(light.position - it.p);
  vec3 H = normalize(E + l); // Half-angle.
  float cosTheta = clamp(dot(l, it.n), 0, 1);
  float cosAlpha = clamp(dot(H, it.n), 0, 1);
  float attenuation = 1.0;
  vec3 directLightToEyeIntensity = vec3(0);

  return mat.ke * attenuation
    + light.colour * attenuation
    * ( mat.kd * cosTheta                   // Diffuse.
      + mat.ks * pow(cosAlpha, mat.shine) // Specular.
      + directLightToEyeIntensity         // Direct light.
   );
}

Intersection intersectScene(Ray r) {
  Intersection closestIntersection = Intersection(false, vec3(0), vec3(0), 0);
  float closestDist = 10000000;
  for (int i = 0; i < numSpheres; i++) {
    Intersection inter = intersectSphere(r, spheres[i]);
    float dist = distance(r.p, inter.p);
    if (inter.hit && dist < closestDist) {
      closestDist = dist;
      closestIntersection = inter;
    }
  }
  return closestIntersection;
}

vec3 genBackground(Ray r) {
  return vec3(0.0, 0.0, sin(r.d.y*20.0)/4.0 + 0.75);
}

vec3 raytrace(Ray initialRay) {
  Ray r = initialRay;
  vec3 finalColour = vec3(0);
  float colourAdditionMultiplier = 1.0;
  bool isRefractionRay = false;
  float ior = 1;

  // Loop over mirror reflection depth or refraction depth.
  const int MAX_DEPTH = 8;
  for (int depth = 0; depth < MAX_DEPTH && colourAdditionMultiplier > 0.01; depth++) {
    Intersection it = intersectScene(r);
    if (!it.hit) {
      finalColour += colourAdditionMultiplier * genBackground(r);
      break;
    }

    if (isRefractionRay) {
      isRefractionRay = false;
      r = Ray(
        it.p,
        refract(r.d, -it.n, 1.0/ior)
      );
      continue;
    }

    Material mat = materials[it.materialId];

    // Ambience.
    vec3 currentColour = mat.ka;

    // Lights.
    for (int lightIdx = 0; lightIdx < numLights; lightIdx++) {
      Ray pointToLight = Ray(it.p, normalize(lights[lightIdx].position - it.p));
      Intersection shadowIntersection = intersectScene(pointToLight);
      if (!shadowIntersection.hit || distance(it.p, shadowIntersection.p) >= distance(it.p, lights[lightIdx].position)) {
        currentColour += lighting(r.p, it, mat, lights[lightIdx]);
      }
    }

    isRefractionRay = mat.refraction != 0;
    ior = mat.ior;
    float refractOrMirror = isRefractionRay ? mat.refraction : mat.mirror;

    finalColour += colourAdditionMultiplier * (1 - refractOrMirror) * currentColour;

    colourAdditionMultiplier *= refractOrMirror;
    r = Ray(
      it.p,
      isRefractionRay ? refract(r.d, it.n, ior) : reflect(r.d, it.n)
    );
  }

  return finalColour;
}

void main() {
  float d = 1.0;
  float virtualH = 2.0 * d * tan(45.0/2.0);
  float virtualW = screenResolution.x / screenResolution.y * virtualH;

  vec3 pixel = vec3(gl_FragCoord.xy, d);
  // Translate pixel to origin and scale.
  vec3 pixel2 = (pixel - 0.5*vec3(screenResolution, 0)) * vec3(virtualW/screenResolution.x, virtualH/screenResolution.y, 1.0);

  vec3 w = normalize(cameraDirection);
  vec3 u = normalize(cross(w, vec3(0, 1, 0)));
  vec3 v = cross(u, w);
  mat3 rotatePixelToWCS = mat3(u, v, w);

  // Apply view transformation.
  vec3 pixel3 = rotatePixelToWCS * pixel2;
  vec3 pixel4 = pixel3 + cameraPosition;

  // Construct ray.
  Ray r = Ray(cameraPosition, normalize(pixel4 - cameraPosition));
  colour = raytrace(r);
}


