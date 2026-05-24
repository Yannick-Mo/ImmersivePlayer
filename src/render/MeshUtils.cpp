#include "MeshUtils.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MeshUtils {

static void push3f(std::vector<float>& v, float x, float y, float z) {
    v.push_back(x); v.push_back(y); v.push_back(z);
}

Mesh uploadMesh(GL* gl, const std::vector<float>& data, int components) {
    Mesh mesh;
    mesh.vertexCount = static_cast<int>(data.size()) / components;

    gl->glGenVertexArrays(1, &mesh.vao);
    gl->glGenBuffers(1, &mesh.vbo);
    gl->glBindVertexArray(mesh.vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                     data.data(), GL_STATIC_DRAW);
    if (components >= 3) {
        gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                                  components * static_cast<int>(sizeof(float)), (void*)0);
        gl->glEnableVertexAttribArray(0);
    }
    if (components >= 6) {
        gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                                  components * static_cast<int>(sizeof(float)),
                                  (void*)(3 * sizeof(float)));
        gl->glEnableVertexAttribArray(1);
    }
    gl->glBindVertexArray(0);
    return mesh;
}

Mesh createBox(GL* gl, float w, float h, float d) {
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    std::vector<float> v;

    auto addFace = [&](float cx, float cy, float cz,
                       float ex, float ey, float ez,
                       float fx, float fy, float fz,
                       float nx, float ny, float nz) {
        push3f(v, cx, cy, cz);                   push3f(v, nx, ny, nz);
        push3f(v, cx+ex, cy+ey, cz+ez);           push3f(v, nx, ny, nz);
        push3f(v, cx+ex+fx, cy+ey+fy, cz+ez+fz);  push3f(v, nx, ny, nz);
        push3f(v, cx, cy, cz);                    push3f(v, nx, ny, nz);
        push3f(v, cx+ex+fx, cy+ey+fy, cz+ez+fz);  push3f(v, nx, ny, nz);
        push3f(v, cx+fx, cy+fy, cz+fz);           push3f(v, nx, ny, nz);
    };

    addFace(-hw, -hh, hd,  w,0,0,  0,h,0,  0,0,1);
    addFace( hw, -hh,-hd, -w,0,0,  0,h,0,  0,0,-1);
    addFace( hw, -hh,-hd,  0,0,d,  0,h,0,  1,0,0);
    addFace(-hw, -hh, hd,  0,0,-d, 0,h,0,  -1,0,0);
    addFace(-hw,  hh,-hd,  w,0,0,  0,0,d,  0,1,0);
    addFace(-hw, -hh, hd,  w,0,0,  0,0,-d, 0,-1,0);

    return uploadMesh(gl, v, 6);
}

Mesh createSphere(GL* gl, float radius, int sectors, int stacks) {
    std::vector<float> v;
    for (int i = 0; i <= stacks; i++) {
        float phi = M_PI * float(i) / stacks;
        float sinPhi = sin(phi), cosPhi = cos(phi);
        for (int j = 0; j <= sectors; j++) {
            float theta = 2.0f * M_PI * float(j) / sectors;
            float sinTheta = sin(theta), cosTheta = cos(theta);
            float nx = sinPhi * cosTheta;
            float ny = cosPhi;
            float nz = sinPhi * sinTheta;
            push3f(v, radius * nx, radius * ny, radius * nz);
            push3f(v, nx, ny, nz);
        }
    }

    std::vector<float> indexed;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < sectors; j++) {
            int a = i * (sectors + 1) + j;
            int b = a + sectors + 1;
            int c = a + 1;
            int d = b + 1;
            for (int k : {a, b, c, c, b, d}) {
                int base = k * 6;
                for (int m = 0; m < 6; m++)
                    indexed.push_back(v[base + m]);
            }
        }
    }

    return uploadMesh(gl, indexed, 6);
}

Mesh createCylinder(GL* gl, float radiusTop, float radiusBottom, float height, int sectors) {
    float hh = height * 0.5f;
    std::vector<float> v;
    for (int i = 0; i <= sectors; i++) {
        float angle = 2.0f * M_PI * float(i) / sectors;
        float ca = cos(angle), sa = sin(angle);
        push3f(v, radiusBottom * ca, -hh, radiusBottom * sa);
        push3f(v, ca, 0.0f, sa);
        push3f(v, radiusTop * ca, hh, radiusTop * sa);
        push3f(v, ca, 0.0f, sa);
    }

    std::vector<float> indexed;
    for (int i = 0; i < sectors; i++) {
        int a = i * 2, b = a + 1, c = a + 2, d = a + 3;
        for (int k : {a, b, c, c, b, d}) {
            int base = k * 6;
            for (int m = 0; m < 6; m++) indexed.push_back(v[base + m]);
        }
    }

    return uploadMesh(gl, indexed, 6);
}

Mesh createTorus(GL* gl, float R, float r, int sectors, int sides) {
    std::vector<float> verts;
    for (int i = 0; i <= sectors; i++) {
        float u = 2.0f * M_PI * float(i) / sectors;
        float cu = cos(u), su = sin(u);
        for (int j = 0; j <= sides; j++) {
            float v = 2.0f * M_PI * float(j) / sides;
            float cv = cos(v), sv = sin(v);
            float x = (R + r * cv) * cu;
            float y = r * sv;
            float z = (R + r * cv) * su;
            float nx = cv * cu;
            float ny = sv;
            float nz = cv * su;
            push3f(verts, x, y, z);
            push3f(verts, nx, ny, nz);
        }
    }

    std::vector<float> indexed;
    for (int i = 0; i < sectors; i++) {
        for (int j = 0; j < sides; j++) {
            int a = i * (sides + 1) + j;
            int b = a + sides + 1;
            int c = a + 1;
            int d = b + 1;
            for (int k : {a, b, c, c, b, d}) {
                int base = k * 6;
                for (int m = 0; m < 6; m++)
                    indexed.push_back(verts[base + m]);
            }
        }
    }

    return uploadMesh(gl, indexed, 6);
}

Mesh createQuad(GL* gl, float w, float h) {
    float hw = w * 0.5f, hh = h * 0.5f;
    std::vector<float> v;
    auto add = [&](float x, float y, float z, float tx, float ty) {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(tx); v.push_back(ty);
    };
    add(-hw, -hh, 0, 0, 0);
    add( hw, -hh, 0, 1, 0);
    add( hw,  hh, 0, 1, 1);
    add(-hw, -hh, 0, 0, 0);
    add( hw,  hh, 0, 1, 1);
    add(-hw,  hh, 0, 0, 1);

    Mesh mesh;
    mesh.vertexCount = static_cast<int>(v.size()) / 5;
    gl->glGenVertexArrays(1, &mesh.vao);
    gl->glGenBuffers(1, &mesh.vbo);
    gl->glBindVertexArray(mesh.vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)),
                     v.data(), GL_STATIC_DRAW);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void*)(3 * sizeof(float)));
    gl->glEnableVertexAttribArray(1);
    gl->glBindVertexArray(0);
    return mesh;
}

Mesh createParticleMesh(GL* gl, const std::vector<float>& data) {
    Mesh mesh;
    mesh.vertexCount = static_cast<int>(data.size()) / 6;

    gl->glGenVertexArrays(1, &mesh.vao);
    gl->glGenBuffers(1, &mesh.vbo);
    gl->glBindVertexArray(mesh.vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                     data.data(), GL_DYNAMIC_DRAW);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*)(3 * sizeof(float)));
    gl->glEnableVertexAttribArray(1);
    gl->glBindVertexArray(0);
    return mesh;
}

void deleteMesh(GL* gl, Mesh& mesh) {
    if (mesh.vao) gl->glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo) gl->glDeleteBuffers(1, &mesh.vbo);
    mesh = Mesh{};
}

} // namespace MeshUtils
