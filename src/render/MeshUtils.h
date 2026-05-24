#pragma once
#include <QOpenGLFunctions_4_5_Core>
#include <vector>

struct Mesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertexCount = 0;
};

namespace MeshUtils {

using GL = QOpenGLFunctions_4_5_Core;

Mesh uploadMesh(GL* gl, const std::vector<float>& data, int components);
Mesh createBox(GL* gl, float w, float h, float d);
Mesh createSphere(GL* gl, float radius, int sectors, int stacks);
Mesh createCylinder(GL* gl, float radiusTop, float radiusBottom, float height, int sectors);
Mesh createTorus(GL* gl, float R, float r, int sectors, int sides);
Mesh createQuad(GL* gl, float w, float h);
Mesh createParticleMesh(GL* gl, const std::vector<float>& data);
void deleteMesh(GL* gl, Mesh& mesh);

} // namespace MeshUtils
