#include <iostream> //std::cerr
#include <cstdlib>  //rand
 
#include "GL/glew.h"
#include "GL/freeglut.h"
 
namespace
{
  struct SVertex2D
  {
    float x,y;  //Position
    float u,v;  //Uv
  };

  struct SDrawElementsCommand
  {
    GLuint vertexCount;
    GLuint instanceCount;
    GLuint firstIndex;
    GLuint baseVertex;
    GLuint baseInstance;
  };

  const GLchar* gVertexShaderSource[] = 
  {
    "#version 430 core\n"
    "layout (location = 0 ) in vec2 position;\n"
    "layout (location = 1 ) in vec2 texCoord;\n"
    "layout (location = 2 ) in uint drawid;\n"
    "out vec2 uv;\n"
    "flat out uint drawID;\n"
    "void main(void)\n"
    "{\n"
    "  gl_Position = vec4(position,0.0,1.0);\n"
    "  uv = texCoord;\n"
    "  drawID = drawid;\n"
    "}\n"
  };

  const GLchar* gFragmentShaderSource[] = 
  {
    "#version 430 core\n"
    "out vec4 color;\n"
    "in vec2 uv;\n"
    "flat in uint drawID;\n"
    "layout (binding=0) uniform sampler2DArray textureArray;\n"
    "void main(void)\n"
    "{\n"
    "  color = texture(textureArray, vec3(uv.x,uv.y,drawID) );\n"
    "}\n"
  };

  const SVertex2D gQuad[] = { {0.0f,0.0f,0.0f,0.0f},
                              {0.1f,0.0f,1.0f,0.0f},
                              {0.0f,0.1f,0.0f,1.0f},
                              {0.1f,0.1f,1.0f,1.0f}
                            };

  const unsigned int gIndex[] = {0,1,2,1,3,2};

  GLuint gArrayTexture(0);
  GLuint gVertexBuffer(0);
  GLuint gElementBuffer(0);
  GLuint gIndirectBuffer(0);
  GLuint gDrawIdBuffer(0);
  GLuint gProgram(0);

}//Unnamed namespace
void GenerateGeometry()
{
  //Generate 100 little quads
  SVertex2D vVertex[400];
  int index(0);
  float xOffset(-0.95f);
  float yOffset(-0.95f );
  for( unsigned int i(0); i!=10; ++i )
  {
    for( unsigned int j(0); j!=10; ++j )
    {
        for( unsigned int k(0); k!=4; ++k)
       {
         vVertex[index].x = gQuad[k].x+xOffset;
         vVertex[index].y = gQuad[k].y+yOffset;
         vVertex[index].u = gQuad[k].u;
         vVertex[index].v = gQuad[k].v;
	 index++;
      } 
      xOffset += 0.2f;
    }
    yOffset += 0.2f;
    xOffset = -0.95f;
  }
 
  GLuint vao;
  glGenVertexArrays(1,&vao);
  glBindVertexArray(vao);

  //Create a vertex buffer object
  glGenBuffers( 1, &gVertexBuffer );
  glBindBuffer( GL_ARRAY_BUFFER, gVertexBuffer );
  glBufferData( GL_ARRAY_BUFFER, sizeof(vVertex), vVertex, GL_STATIC_DRAW );
 
  //Specify vertex attributes for the shader
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SVertex2D), (GLvoid*)0 );
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SVertex2D), (GLvoid*)2 );
 
  //Create an element buffer
  glGenBuffers( 1, &gElementBuffer );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gElementBuffer );
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(gIndex), gIndex, GL_STATIC_DRAW );

  //Generate draw commands
  SDrawElementsCommand vDrawCommand[100];
  for( unsigned int i(0); i<100; ++i )
  {
    vDrawCommand[i].vertexCount = 6;
    vDrawCommand[i].instanceCount = 1;
    vDrawCommand[i].firstIndex = 0;
    vDrawCommand[i].baseVertex = i*4;
    vDrawCommand[i].baseInstance = i;
  }

  glGenBuffers(1, &gIndirectBuffer );
  glBindBuffer( GL_DRAW_INDIRECT_BUFFER, gIndirectBuffer );
  glBufferData( GL_DRAW_INDIRECT_BUFFER, sizeof(vDrawCommand), vDrawCommand, GL_STATIC_DRAW );


  //Generate an instanced vertex array to identify each draw call in the shader
  GLuint vDrawId[100];
  for( GLuint i(0); i<100; i++ )
  {
    vDrawId[i] = i;
  }

  glGenBuffers( 1, &gDrawIdBuffer );
  glBindBuffer( GL_ARRAY_BUFFER, gDrawIdBuffer );
  glBufferData( GL_ARRAY_BUFFER, sizeof(vDrawId), vDrawId, GL_STATIC_DRAW );

  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 0, (GLvoid*)0 );
  glVertexAttribDivisor(2, 1);

  //NOTE: Instead of creating a new buffer for the drawID as we just did, 
  //we could use the "baseInstance" field from the gIndirectBuffer to 
  //provide the gl_DrawID to the shader. The code will look like this:
  /*
  glBindBuffer(GL_ARRAY_BUFFER, gIndirectBuffer );
  glEnableVertexAttribArray(2);
  glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(SDrawElementsCommand), (void*)( 4 * sizeof(GLuint)) );
  glVertexAttribDivisor(2, 1);1
  */
}
 
void GenerateArrayTexture()
{
  //Generate an array texture
  glGenTextures( 1, &gArrayTexture );
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, gArrayTexture);
 
  //Create storage for the texture. (100 layers of 1x1 texels)
  glTexStorage3D( GL_TEXTURE_2D_ARRAY,
                  1,                    //No mipmaps as textures are 1x1
                  GL_RGB8,              //Internal format
                  1, 1,                 //width,height
                  100                   //Number of layers
                );
 
  for( unsigned int i(0); i!=100;++i)
  {
    //Choose a random color for the i-essim image
    GLubyte color[3] = {rand()%255,rand()%255,rand()%255};
 
    //Specify i-essim image
    glTexSubImage3D( GL_TEXTURE_2D_ARRAY,
                     0,                     //Mipmap number
                     0,0,i,                 //xoffset, yoffset, zoffset
                     1,1,1,                 //width, height, depth
                     GL_RGB,                //format
                     GL_UNSIGNED_BYTE,      //type
                     color);                //pointer to data
  }
 
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
 
GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource )
{
  //Compile vertex shader
  GLuint vertexShader( glCreateShader( GL_VERTEX_SHADER ) );
  glShaderSource( vertexShader, 1, vertexShaderSource, NULL );
  glCompileShader( vertexShader );
 
  //Compile fragment shader
  GLuint fragmentShader( glCreateShader( GL_FRAGMENT_SHADER ) );
  glShaderSource( fragmentShader, 1, fragmentShaderSource, NULL );
  glCompileShader( fragmentShader );
 
  //Link vertex and fragment shader together
  GLuint program( glCreateProgram() );
  glAttachShader( program, vertexShader );
  glAttachShader( program, fragmentShader );
  glLinkProgram( program );
 
  //Delete shaders objects
  glDeleteShader( vertexShader );
  glDeleteShader( fragmentShader );
 
  return program;
}
 
void Init(void)
{
  //Check if Opengl version is at least 3.0
  const GLubyte* glVersion( glGetString(GL_VERSION) );
  int major = glVersion[0] - '0';
  int minor = glVersion[2] - '0';
  if( major < 3 || minor < 0 )
  {
    std::cerr<<"ERROR: Minimum OpenGL version required for this demo is 3.0. Your current version is "<<major<<"."<<minor<<std::endl;
    exit(-1);
  }
 
  //Init glew
  glewInit();
 
  //Set clear color
  glClearColor(1.0, 1.0, 1.0, 0.0);
 
  //Create and bind the shader program
  gProgram = CompileShaders( gVertexShaderSource, gFragmentShaderSource );
  glUseProgram(gProgram);
 
  glUniform1i(0,0); //Sampler refers to texture unit 0
  
  GenerateGeometry();
  GenerateArrayTexture();
}
 
void Display()
{
  glClear( GL_COLOR_BUFFER_BIT );
  glMultiDrawElementsIndirect( GL_TRIANGLES, 
							   GL_UNSIGNED_INT, 
							   (GLvoid*)0, 
							   100, 
							   0 );

  glutSwapBuffers();
}
 
void Quit()
{
  //Clean-up
  glDeleteProgram(gProgram);
  glDeleteBuffers( 1, &gVertexBuffer );
  glDeleteBuffers( 1, &gElementBuffer );
  glDeleteBuffers( 1, &gDrawIdBuffer );
  glDeleteBuffers( 1, &gIndirectBuffer );
  glDeleteTextures( 1, &gArrayTexture );
 
  //Exit application
  exit(0);
}
 
void OnKeyPress( unsigned char key, int x, int y )
{
  //'Esc' key
  if( key == 27 )
    Quit();
}
 
int main( int argc, char** argv )
{
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB );
  glutInitWindowSize(400,400);
  glutCreateWindow("Array Texture Example");
  glutIdleFunc(Display);
  glutKeyboardFunc( OnKeyPress );
 
  Init();
 
  //Enter the GLUT event loop
  glutMainLoop();
}
