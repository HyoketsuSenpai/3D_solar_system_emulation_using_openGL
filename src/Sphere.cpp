#include "Sphere.h"

Sphere::Sphere(const float r, const char* vsFile, const char* fsFile, glm::mat4 model, glm::mat4 view, glm::mat4 projection, std::string texFile)
    : m_Shader(vsFile, fsFile), model(model), view(view), projection(projection)
{
    
    initTexture(texFile);

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    initBuffer(100, 100, r);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    

    glBindVertexArray(0);

}

Sphere::~Sphere()
{
}

void Sphere::initBuffer(int numRows, int numCols, float radius){

    int numVerticesTopStrip = 3 * numCols * 2;
    int numVerticesRegularStrip = 6 * numCols;
    m_numVertices = numVerticesTopStrip + (numRows - 1) * numVerticesRegularStrip;

    float pitchAngle = 180.0f / (float)numRows;
    float headAngle = 360.0f / (float)numCols;

    glm::vec3 apex = glm::vec3(0.0f, radius, 0.0f);

    float pitch = -90.0f;

    for (float heading = 0.0f; heading < 360.0f; heading+=headAngle)
    {
        x = apex.x;
        y = apex.y;
        z = apex.z;

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);        
        
        vertices.push_back(x/radius);
        vertices.push_back(y/radius);
        vertices.push_back(z/radius);

        float u = heading / 360.0f;
        float v = (90.0f + pitch) / 180.0f;

        vertices.push_back(u);
        vertices.push_back(v);


        initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);
        initBySphericalCoords(radius, pitch + pitchAngle, heading);

    }

    for (pitch = -90.0f + pitchAngle; pitch < 90.0f; pitch+=pitchAngle)
    {
        for (float heading = 0.0f; heading < 360.0f; heading+=headAngle)
        {
            initBySphericalCoords(radius, pitch, heading);
            initBySphericalCoords(radius, pitch, heading + headAngle);
            initBySphericalCoords(radius, pitch + pitchAngle, heading);

            initBySphericalCoords(radius, pitch, heading + headAngle);
            initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);
            initBySphericalCoords(radius, pitch + pitchAngle, heading);
            
        }
        
    }
    
    pitch = 0.0f;

    apex.x = x;
    apex.y = y;
    apex.z = z;

    for (float heading = 0.0f; heading < 360.0f; heading+=headAngle)
    {
        x = apex.x;
        y = apex.y;
        z = apex.z;

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);

        vertices.push_back(x/radius);
        vertices.push_back(y/radius);
        vertices.push_back(z/radius);

        float u = heading / 360.0f;
        float v = (90.0f + pitch) / 180.0f;

        vertices.push_back(u);
        vertices.push_back(v);

        initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);
        initBySphericalCoords(radius, pitch + pitchAngle, heading);

    }
    
}

void Sphere::render(){
    
    m_Shader.use();
    m_Shader.setInt("ourTexture", 0);
    m_Shader.SetUniformMat4f("model", model);
    m_Shader.SetUniformMat4f("projection", projection);

    glBindTexture(GL_TEXTURE_2D, m_Texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindVertexArray(m_VAO);
    m_Shader.use();
    glDrawArrays(GL_TRIANGLES, 0, m_numVertices);
    glBindVertexArray(0);
}

void Sphere::initBySphericalCoords(float radius, float pitch, float heading){
    x = radius * cosf(glm::radians(pitch)) * sinf(glm::radians(heading)); 
    y = -radius * sinf(glm::radians(pitch)); 
    z = radius * cosf(glm::radians(pitch)) * cosf(glm::radians(heading)); 

    float nx = x / radius;
    float ny = y / radius;
    float nz = z / radius;

    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(nx);
    vertices.push_back(ny);
    vertices.push_back(nz);

    float u = heading / 360.0f;
    float v = (90.0f + pitch) / 180.0f;

    vertices.push_back(u);
    vertices.push_back(v);

}

void Sphere::initTexture(std::string texFile){

    int width, height, nrChannels; 
    unsigned char *data = stbi_load(texFile.c_str(), &width, &height, &nrChannels, 0); 

    glGenTextures(1, &m_Texture);  

    glBindTexture(GL_TEXTURE_2D, m_Texture);  

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
}