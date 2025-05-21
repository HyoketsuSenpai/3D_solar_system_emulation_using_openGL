#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "shader_s_shader.h"
#include "shader_s.h"
#include "glm/gtc/matrix_transform.hpp"

#include "glm/mat4x4.hpp"

#include <memory>
#include <vector>

class Sphere
{
private:

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_IBO;
    int m_numVertices;

    glm::mat4 view;
    glm::mat4 projection;

    std::vector<float> vertices;
    std::vector<float> indices;

    float x, y, z;

public:
    glm::mat4 model;
    Shader m_Shader;
    
    Sphere(const float r, const char* vsFile, const char* fsFile, glm::mat4 model, glm::mat4 view, glm::mat4 projection);
    ~Sphere();
    void initBuffer(int numRows, int numCols, float radius);
    void render();
    void initBySphericalCoords(float radius, float pitch, float heading);
};
