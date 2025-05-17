#include "Sphere.h"

Sphere::Sphere(const float r, const char* vsFile, const char* fsFile, glm::mat4 model, glm::mat4 view, glm::mat4 projection)
    : m_Shader(vsFile, fsFile), model(model), view(view), projection(projection)
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    initBuffer(100, 100, r);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

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
    int i = 0;

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
            //initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);

            initBySphericalCoords(radius, pitch, heading + headAngle);
            initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);
            initBySphericalCoords(radius, pitch + pitchAngle, heading);
            
        }
        
    }
    
    pitch = 0.0f;

    for (float heading = 0.0f; heading < 360.0f; heading+=headAngle)
    {
        // x = apex.x;
        // y = -apex.y;
        // z = apex.z;

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);

        vertices.push_back(x/radius);
        vertices.push_back(y/radius);
        vertices.push_back(z/radius);

        initBySphericalCoords(radius, pitch + pitchAngle, heading + headAngle);
        initBySphericalCoords(radius, pitch + pitchAngle, heading);

    }
    
}

void Sphere::render(){
    m_Shader.use();
    m_Shader.SetUniformMat4f("model", model);
    m_Shader.SetUniformMat4f("view", view);
    m_Shader.SetUniformMat4f("projection", projection);

    glBindVertexArray(m_VAO);
    m_Shader.use();
    glDrawArrays(GL_TRIANGLES, 0, m_numVertices);
    glBindVertexArray(0);
}

void Sphere::initBySphericalCoords(float radius, float pitch, float heading){
    x = radius * cosf(glm::radians(pitch)) * sinf(glm::radians(heading)); 
    y = -radius * sinf(glm::radians(pitch)); 
    z = radius * cosf(glm::radians(pitch)) * sinf(glm::radians(heading)); 

    float nx = x / radius;
    float ny = y / radius;
    float nz = z / radius;

    vertices.push_back(x);
    vertices.push_back(y);
    vertices.push_back(z);
    vertices.push_back(nx);
    vertices.push_back(ny);
    vertices.push_back(nz);
}
