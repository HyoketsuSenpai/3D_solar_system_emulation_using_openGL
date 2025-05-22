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
#include "Camera.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int noOfPlanets = 8;
std::unique_ptr<Camera> camera = std::unique_ptr<Camera>();

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    static float lastX = 320.0f;
    static float lastY = 240.0f;
    static bool firstMouse = true;

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    camera->ProcessMouseMovement(xoffset, yoffset);
}

/**
 * @brief Entry point for the OpenGL solar system simulation.
 *
 * Initializes the OpenGL context and window, sets up camera and projection matrices, and creates a sun and multiple orbiting planet spheres. Runs the main render loop, animating planets in circular orbits around the sun with dynamic lighting, and handles user input and window events. Cleans up and terminates the application on exit.
 *
 * @return int Returns 0 on successful execution, or -1 if initialization fails.
 */
int main() {

std::vector<float> size = {0.35f, 0.87f, 0.91f, 0.49f, 10.04f, 8.36f, 3.64f, 3.54f, 0.17f};
std::vector<float> distance = {1.00f, 1.87f, 2.58f, 3.94f, 13.44f, 24.76f, 49.60f, 77.63f, 102.00f};
std::vector<float> speed = {0.00017f, 0.00004f, 00.1f, 0.0097f , 0.0242f, 0.0223f, 0.0139f, 0.0149f, 0.0016f};
std::vector<std::string> textures = {"sun.jpg", "mercury.jpg", "venus.jpg", "earth.jpg", "mars.jpg", "jupiter.jpg", "saturn.jpg", "uranus.jpg", "neptune.jpg" , ""};

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

glm::mat4 model = glm::mat4(1.0f);

glm::mat4 projection = glm::mat4(1.0f);
projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

glm::mat4 view = glm::mat4(1.0f);
view = glm::translate(view, glm::vec3(0.0f, -1.0f, -10.0f));

Sphere sun = Sphere(0.1f * 100.0f, "sphere_shader.vs", "sphere_shader.fs", model, view, projection, textures[0]);
std::vector<Sphere> planets;

for(int i = 0; i < noOfPlanets; i++)
    planets.emplace_back(Sphere(0.1f * size[i], "3.3.shader.vs", "3.3.shader.fs", model, view, projection, textures[i + 1]));


camera = std::make_unique<Camera>(glm::vec3(7.0f, 3.0f, 7.0f), glm::vec3(0.0f, 1.0f, 0.0f), -135.0f, -10.0f);

glfwSetCursorPosCallback(window, mouse_callback);
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

while (!glfwWindowShouldClose(window))
{
    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->ProcessKeyboard('W', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera->ProcessKeyboard('S', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->ProcessKeyboard('A', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->ProcessKeyboard('D', deltaTime);

    glm::mat4 view = camera->GetViewMatrix();

    glm::vec3 lightPos = glm::vec3(sun.model[3]);
    float t = currentFrame;
    for(int i = 0; i < noOfPlanets; i++){

        glm::vec3 modelTransform = glm::vec3(cosf(t * speed[i]) * (distance[i] + 10.0f), 0.0f, sinf(t * speed[i]) * (distance[i] + 10.0f));
        planets[i].model = glm::translate(glm::mat4(1.0f), modelTransform);
        planets[i].m_Shader.use();
        planets[i].m_Shader.SetUniformVec3f("lightPos", lightPos);
        planets[i].m_Shader.SetUniformMat4f("view", view);

        planets[i].render();

    }

    sun.m_Shader.use();
    sun.m_Shader.SetUniformMat4f("view", view);
    sun.render();
    
    glfwSwapBuffers(window);
    glfwPollEvents();
}

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

