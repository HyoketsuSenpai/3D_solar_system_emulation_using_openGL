#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>

#include <memory>
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "shader_s_shader.h"
#include "shader_s.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/mat4x4.hpp"

#include <vector>
#include "Sphere.h"

using namespace std;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int noOfPlanets = 9;

int main() {

glfwInit();
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "The Sun", NULL, NULL);
if (window == NULL)
{
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
}
glfwMakeContextCurrent(window);
glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
{
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
}

glEnable(GL_DEPTH_TEST);

//set mvp
glm::mat4 model = glm::mat4(1.0f);
//model = glm::rotate(model, glm::radians(50.0f), glm::vec3(0.5f, 1.0f, 0.0f));

glm::mat4 projection = glm::mat4(1.0f);
projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

glm::mat4 view = glm::mat4(1.0f);
view = glm::translate(view, glm::vec3(0.0f, -1.0f, -10.0f));

Sphere sun = Sphere(0.1f, "sphere_shader.vs", "sphere_shader.fs", model, view, projection);
vector<Sphere> planets;

for(int i = 0; i < noOfPlanets; i++)
    planets.emplace_back(Sphere(0.1f, "3.3.shader.vs", "3.3.shader.fs", model, view, projection));

while (!glfwWindowShouldClose(window))
{
    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    float t = static_cast<float>(glfwGetTime());
    for(int i = 0; i < noOfPlanets; i++){

        glm::vec3 lightPos = glm::vec3(sun.model[3]);
        glm::vec3 modelTrans = glm::vec3(cosf(t) * (i + 1), 0.0f, sinf(t) * (i + 1));
        planets[i].model = glm::translate(glm::mat4(1.0f), modelTrans);
        planets[i].m_Shader.use();
        planets[i].m_Shader.SetUniformVec3f("lightPos", lightPos);

        planets[i].render();

    }

    sun.render();
    

    
    glfwSwapBuffers(window);
    glfwPollEvents();
}

// glDeleteVertexArrays(1, &VAO);
// glDeleteBuffers(1, &VBO);

glfwTerminate();
return 0;
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
