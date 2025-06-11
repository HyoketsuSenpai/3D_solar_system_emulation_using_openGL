#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>
#include <memory>
#include <vector>

#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "shader_s_shader.h"
#include "shader_s.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include "KHR/khrplatform.h"

#include "Sphere.h"
#include "Camera.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, float deltaTime);
unsigned int loadCubemap(std::vector<std::string> faces);
unsigned int loadTexture(char const * path);
void generateRingMesh(std::vector<float>& vertices, std::vector<unsigned int>& indices, float innerRadius, float outerRadius, int segments);

int SCR_WIDTH = 800;
int SCR_HEIGHT = 600;
const unsigned int noOfPlanets = 8;
bool SkyBoxExtra = false;
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


int main() {

std::vector<float> size = {0.38f, 0.87f, 1.00f, 0.53f, 12.54f, 9.36f, 4.04f, 3.84f, 0.3f};
std::vector<float> distance = {4.00f, 5.60f, 6.68f, 7.94f, 15.84f, 25.76f, 33.60f, 40.63f, 0.13f};
std::vector<float> speed = {0.1f, 0.084f, 0.057f, 0.034f, 0.014f, 0.0093f, 0.0049f, 0.0027f, 0.2f};
std::vector<float> rotationSpeed = {0.11f, -0.026f, 5.28f, 5.12f, 12.20f, 11.16f, -7.75f, 8.36f, 0.23f};
std::vector<std::string> textures = {"sun.jpg", "mercury.jpg", "venus.jpg", "earth.jpg", "mars.jpg", "jupiter.jpg", "saturn.jpg", "uranus.jpg", "neptune.jpg" , "moon.jpg"};

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

float skyboxVertices[] = {
    // positions          
   -1.0f,  1.0f, -1.0f,
   -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
   -1.0f,  1.0f, -1.0f,

   -1.0f, -1.0f,  1.0f,
   -1.0f, -1.0f, -1.0f,
   -1.0f,  1.0f, -1.0f,
   -1.0f,  1.0f, -1.0f,
   -1.0f,  1.0f,  1.0f,
   -1.0f, -1.0f,  1.0f,

    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

   -1.0f, -1.0f,  1.0f,
   -1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
   -1.0f, -1.0f,  1.0f,

   -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
   -1.0f,  1.0f,  1.0f,
   -1.0f,  1.0f, -1.0f,

   -1.0f, -1.0f, -1.0f,
   -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
   -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f,  1.0f
};

unsigned int skyboxVAO, skyboxVBO;
glGenVertexArrays(1, &skyboxVAO);
glGenBuffers(1, &skyboxVBO);
glBindVertexArray(skyboxVAO);
glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

std::vector<float> orbitVertices;
float x, z, angle;
for (int i = 0; i < 360; i++){
    angle = glm::radians((float)i);
    x = sin(angle);
    z = cos(angle);
    orbitVertices.emplace_back(x);
    orbitVertices.emplace_back(0.0f);
    orbitVertices.emplace_back(z);
}

GLuint VBO_t, VAO_t;
glGenVertexArrays(1, &VAO_t);
glGenBuffers(1, &VBO_t);
glBindVertexArray(VAO_t);
glBindBuffer(GL_ARRAY_BUFFER, VBO_t);
glBufferData(GL_ARRAY_BUFFER, sizeof(float)*orbitVertices.size(), orbitVertices.data(), GL_STATIC_DRAW);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (GLvoid*)0);
glEnableVertexAttribArray(0);
glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindVertexArray(0);

std::vector<float> ringVertices;
std::vector<unsigned int> ringIndices;
generateRingMesh(ringVertices, ringIndices, size[5] * 0.3, size[5] * 0.5f, 100);
GLuint ringVAO, ringVBO;
glGenVertexArrays(1, &ringVAO);
glGenBuffers(1, &ringVBO);
glBindVertexArray(ringVAO);
glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
glBufferData(GL_ARRAY_BUFFER, ringVertices.size() * sizeof(float), ringVertices.data(), GL_STATIC_DRAW);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
glBindVertexArray(0);
unsigned int ring_texture = loadTexture("ring.jpg");
Shader ringShader("ring_vs.vs", "ring_fs.fs");


glm::mat4 model = glm::mat4(1.0f);

glm::mat4 projection = glm::mat4(1.0f);
projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 1000.0f);

glm::mat4 view = glm::mat4(1.0f);
view = glm::translate(view, glm::vec3(0.0f, -1.0f, -10.0f));

Sphere sun = Sphere(0.1f * 100.0f, "sphere_shader.vs", "sphere_shader.fs", model, view, projection, textures[0]);
std::vector<Sphere> planets;
Sphere moon = Sphere(0.1f * size[8] , "3.3.shader.vs", "3.3.shader.fs", model, view, projection, textures[9]);

for(int i = 0; i < noOfPlanets; i++)
    planets.emplace_back(Sphere(0.25 * size[i], "3.3.shader.vs", "3.3.shader.fs", model, view, projection, textures[i + 1]));

Shader SkyboxShader("skybox.vs", "skybox.fs");
Shader SimpleShader("orbit_vs.vs", "orbit_fs.fs");

std::vector<std::string> faces {
    "include/skybox/starfield/starfield_rt.tga",
    "include/skybox/starfield/starfield_lf.tga",
    "include/skybox/starfield/starfield_up.tga",
    "include/skybox/starfield/starfield_dn.tga",
    "include/skybox/starfield/starfield_ft.tga",
    "include/skybox/starfield/starfield_bk.tga",
};
std::vector<std::string> faces_extra {
    "include/skybox/blue/bkg1_right.png",
    "include/skybox/blue/bkg1_left.png",
    "include/skybox/blue/bkg1_top.png",
    "include/skybox/blue/bkg1_bot.png",
    "include/skybox/blue/bkg1_front.png",
    "include/skybox/blue/bkg1_back.png",
};

unsigned int cubemapTexture = loadCubemap(faces);
unsigned int cubemapTextureExtra = loadCubemap(faces_extra);

camera = std::make_unique<Camera>(glm::vec3(25.3380f, 28.2700f, 60.1150f), glm::vec3(0.0f, 1.0f, 0.0f), -115.0f, -30.0f);
camera->MovementSpeed = 5.0f;

glfwSetCursorPosCallback(window, mouse_callback);
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

float deltaTime = 0.0f;
float lastFrame = 0.0f;
float sunRotationSpeed = 0.2476f;


while (!glfwWindowShouldClose(window))
{
    processInput(window, deltaTime);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    glm::mat4 view = camera->GetViewMatrix();

    glm::vec3 lightPos = glm::vec3(sun.model[3]);
    float t = currentFrame;
    for(int i = 0; i < noOfPlanets; i++){

        planets[i].model = glm::mat4(1.0f);
        glm::vec3 modelTransform = glm::vec3(cosf(t * speed[i]) * (distance[i] + 10.0f), 0.0f, sinf(t * speed[i]) * (distance[i] + 10.0f));
        planets[i].model = glm::translate(glm::mat4(1.0f), modelTransform);

        if(i == 2){
            moon.model = planets[i].model;
            glm::vec3 moonTransform = glm::vec3(cosf(t * speed[8]) * (distance[8] + 0.2f), 0.0f, sinf(t * speed[8]) * (distance[8] + 0.2f));
            moon.model = glm::translate(moon.model, moonTransform);

            moon.model = glm::rotate(moon.model, t * (rotationSpeed[8]), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        planets[i].model = glm::rotate(planets[i].model, t * (rotationSpeed[i] / 10), glm::vec3(0.0f, 1.0f, 0.0f));

        planets[i].m_Shader.use();
        planets[i].m_Shader.SetUniformVec3f("lightPos", lightPos);
        planets[i].m_Shader.SetUniformMat4f("view", view);
        planets[i].m_Shader.SetUniformVec3f("viewPos", view[3]);

        planets[i].render();
    }

    moon.m_Shader.use();
    moon.m_Shader.SetUniformVec3f("lightPos", lightPos);
    moon.m_Shader.SetUniformMat4f("view", view);
    moon.m_Shader.SetUniformVec3f("viewPos", view[3]);
    moon.render();

    sun.model = glm::rotate(sun.model, t * (sunRotationSpeed / 6000.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    sun.m_Shader.use();
    sun.m_Shader.SetUniformMat4f("view", view);
    sun.render();

    glBindVertexArray(VAO_t);
    glLineWidth(1.0f);
    SimpleShader.use();
    SimpleShader.SetUniformMat4f("view", view);
    SimpleShader.SetUniformMat4f("projection", projection);
    glm::mat4 modelorb;
    
    for (int i = 0; i < noOfPlanets; i++)
    {
        float orbitRadius = distance[i] + 10.0f;
        modelorb = glm::mat4(1);
        modelorb = glm::scale(modelorb, glm::vec3(orbitRadius, orbitRadius, orbitRadius));
        SimpleShader.SetUniformMat4f("model", modelorb);
        glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)orbitVertices.size() / 3);
    }
    modelorb = glm::mat4(1);
    modelorb = glm::rotate(modelorb, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    modelorb = glm::rotate(modelorb, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    modelorb = glm::translate(modelorb, glm::vec3(0.0f, 0.0f, 0.0f));
    modelorb = glm::scale(modelorb, glm::vec3(0.5f *1.3f , 0.5f *1.3f, 0.5f *1.3f));
    SimpleShader.SetUniformMat4f("model", modelorb);
    glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)orbitVertices.size() / 3);

    ringShader.use();
    ringShader.SetUniformMat4f("view", view);
    ringShader.SetUniformMat4f("projection", projection);

    glm::mat4 ringModel = glm::mat4(1.0f);
    glm::vec3 saturnPos = glm::vec3(
        cosf(t * speed[5]) * (distance[5] + 10.0f),
        0.0f,
        sinf(t * speed[5]) * (distance[5] + 10.0f)
    );
    ringModel = glm::translate(ringModel, saturnPos);
    ringModel = glm::rotate(ringModel, glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ringShader.SetUniformMat4f("model", ringModel);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ring_texture);

    glBindVertexArray(ringVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, (102 + 1) * 2);
    glBindVertexArray(0);

    
    glDepthFunc(GL_LEQUAL);  
    SkyboxShader.use();
    view = glm::mat4(glm::mat3(camera->GetViewMatrix()));
    SkyboxShader.SetUniformMat4f("view", view);
    SkyboxShader.SetUniformMat4f("projection", projection);
    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    if (SkyBoxExtra)
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureExtra);
    else
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);  
    
    glfwSwapBuffers(window);
    glfwPollEvents();
}

glfwTerminate();
return 0;
}

void processInput(GLFWwindow *window, float deltaTime)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->ProcessKeyboard('W', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera->ProcessKeyboard('S', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->ProcessKeyboard('A', deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->ProcessKeyboard('D', deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;

	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);

		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else
            std::cout << "Unexpected nrComponents: " << nrComponents << std::endl;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
        return 0;
	}

	return textureID;
}

void generateRingMesh(std::vector<float>& vertices, std::vector<unsigned int>& indices, float innerRadius, float outerRadius, int segments){
    for (int i = 0; i <= segments; ++i) {
        float theta = 2.0f * M_PI * float(i) / float(segments);
        float x = cos(theta);
        float z = sin(theta);

        vertices.push_back(outerRadius * x);
        vertices.push_back(0.0f);
        vertices.push_back(outerRadius * z);
        vertices.push_back(1.0f);
        vertices.push_back(float(i) / segments);

        vertices.push_back(innerRadius * x);
        vertices.push_back(0.0f);
        vertices.push_back(innerRadius * z);
        vertices.push_back(0.0f);
        vertices.push_back(float(i) / segments);
    }
}