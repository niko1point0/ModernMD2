/* Playable Dragon */
unsigned char RiderM[];
unsigned char RiderT[];
unsigned char DragonM[];
unsigned char DragonT[];

/* Playable Skeleton */
unsigned char SkeletonM[];
unsigned char SkeletonT[];
unsigned char StickM[];
unsigned char StickT[];

char NumPieces[] =
{
		2, /*skeleton*/
		2, /*dragon*/
};

void* BMPpointers[] =
{
	&SkeletonT,
	&StickT,
	&DragonT,
	&RiderT,
};

void* MD2pointers[][10][2] =
{
	// playable skeleton
	{
		{ &SkeletonM, (void*)0 },
		{ &StickM, (void*)1 },
	},

	// playable dragon
	{
		{ &DragonM, (void*)2 },
		{ &RiderM, (void*)3 }
	},
};

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>

GLuint s_program;

typedef struct MD2Vertex
{
	unsigned char pos[3];
	unsigned char normal;
} MD2Vertex;

typedef struct MD2Frame
{
	float scale[3];
	float trans[3];
	unsigned char name[16];
	MD2Vertex vertices[2048];
} MD2Frame;

typedef struct MD2TexCoord
{
	unsigned short texCoordX, texCoordY;
} MD2TexCoord;

typedef struct MD2Triangle
{
	unsigned short vertices[3], texCoords[3];
} MD2Triangle;

typedef struct MD2Model
{
	unsigned short numFrames;
	unsigned short numTriangles;

	GLuint* posBuffer;
	GLuint uvBuffer;

} MD2Model;


void LoadBMP(unsigned char hex[], int* textureId)
{
	unsigned short width, height, dataOffset;
	memcpy(&dataOffset, &hex[10], sizeof(short));
	memcpy(&width, &hex[18], sizeof(short));
	memcpy(&height, &hex[22], sizeof(short));

	glGenTextures(1, (GLuint*)textureId);
	glBindTexture(GL_TEXTURE_2D, *textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0,
		GL_RGB, width, height, 0,
		GL_BGRA, GL_UNSIGNED_BYTE,
		&hex[dataOffset]);
}

void LoadMD2(unsigned char hex[], MD2Model* z)
{
	unsigned short width, height,
		numVertices, numTexCoords, numTriangles, numFrames,
		texCoordOffset, triangleOffset, frameOffset;

	float* FinalPos;
	float* FinalUV;

	unsigned short i, j, k;

	memcpy(&width, &hex[8], sizeof(short));
	memcpy(&height, &hex[12], sizeof(short));
	memcpy(&numVertices, &hex[24], sizeof(short));
	memcpy(&numTexCoords, &hex[28], sizeof(short));
	memcpy(&numTriangles, &hex[32], sizeof(short));
	memcpy(&numFrames, &hex[40], sizeof(short));
	memcpy(&texCoordOffset, &hex[48], sizeof(short));
	memcpy(&triangleOffset, &hex[52], sizeof(short));
	memcpy(&frameOffset, &hex[56], sizeof(short));

	z->numFrames = numFrames;
	z->numTriangles = numTriangles;

	MD2Frame* frames = (MD2Frame*)malloc(sizeof(MD2Frame)*numFrames); //512
	MD2TexCoord* texCoords = (MD2TexCoord*)malloc(sizeof(MD2TexCoord)*numTexCoords);
	MD2Triangle* triangles = (MD2Triangle*)malloc(sizeof(MD2Triangle)*numTriangles);

	memcpy(texCoords, &hex[texCoordOffset], sizeof(MD2TexCoord)*numTexCoords);
	memcpy(triangles, &hex[triangleOffset], sizeof(MD2Triangle)*numTriangles);

	FinalPos = (float*)malloc(sizeof(float)*numTriangles * 3 * 3);
	FinalUV = (float*)malloc(sizeof(float) * numTriangles * 3 * 2);

	z->posBuffer = (GLuint*)malloc(sizeof(GLuint)*numFrames);

	// for every frame
	for (int m = 0; m < numFrames; m++)
	{
		int size = 40 + sizeof(MD2Vertex)*numVertices;
		int a = frameOffset + m * size;

		memcpy(&frames[m], &hex[a], size);

		// for every triangle
		for (i = 0; i < numTriangles; i++)

			// for every vertex
			for (j = 0; j < 3; j++)

				// for x, y, and z
				for (k = 0; k < 3; k++)

					FinalPos[9 * i + 3 * j + k] =
					(frames[m].trans[k] +
					(frames[m].scale[k] *
					 frames[m].vertices[triangles[i].vertices[j]].pos[k]));

		glGenBuffers(1, &z->posBuffer[m]);
		glBindBuffer(GL_ARRAY_BUFFER, z->posBuffer[m]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float)*z->numTriangles * 3 * 3, FinalPos, GL_STATIC_DRAW);
	}

	// for every triangle
	for (i = 0; i < numTriangles; i++)
	{
		// for every vertex
		for (j = 0; j < 3; j++)
		{
			// This only needs to happen once
			FinalUV[6 * i + 2 * j + 0] = ((float)texCoords[triangles[i].texCoords[j]].texCoordX / width);
			FinalUV[6 * i + 2 * j + 1] = (1 - (float)texCoords[triangles[i].texCoords[j]].texCoordY / height);
		}
	}

	glGenBuffers(1, &z->uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, z->uvBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float)*z->numTriangles * 3 * 2, FinalUV, GL_STATIC_DRAW);

	free(frames);
	free(texCoords);
	free(triangles);
	free(FinalPos);
	free(FinalUV);
}

#include <time.h>
#include <math.h>
#include <stdio.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

struct Matrices
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	float frac;
};

// matrix buffer
Matrices	matrixBufferCPU;
GLuint		matrixBufferGPU;

GLFWwindow* window;
clock_t OldTime;
clock_t NewTime;

// MD2models and textures
MD2Model* vault[2][2];
int tex[4];

// time passed
float animTime = 0;

void LoadAssets(int i, int j)
{
	vault[i][j] = (MD2Model*)malloc(sizeof(MD2Model));

	LoadMD2((unsigned char*)MD2pointers[i][j][0], vault[i][j]);
	LoadBMP((unsigned char*)BMPpointers[(int)MD2pointers[i][j][1]], &tex[(int)MD2pointers[i][j][1]]);
}

GLuint VAO;
void Draw(int modelID)
{
	int frameIndex1 = (int)animTime;
	int frameIndex2 = (int)animTime + 1;

	int a;
	for (a = 0; a < NumPieces[modelID]; a++)
	{
		MD2Model* z = vault[modelID][a];

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, z->posBuffer[frameIndex1]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, z->posBuffer[frameIndex2]);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, z->uvBuffer);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
		glEnableVertexAttribArray(2);

		//Unbinding both our OpenGL objects
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// always use texture 0
		glBindTexture(GL_TEXTURE_2D, tex[(int)MD2pointers[modelID][a][1]]);
		glUniform1i(glGetUniformLocation(s_program, "tex_diffuse"), 0);
		
		glm::mat4x4 model = glm::mat4(1);

		if (modelID == 0)
			model = glm::translate(model, glm::vec3(0, -30.0f, 0));
		else
			model = glm::translate(model, glm::vec3(0, 30.0f, 0));

		matrixBufferCPU.model = model;

		// bind buffer, send data, and unbind
		glBindBuffer(GL_UNIFORM_BUFFER, matrixBufferGPU);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(Matrices), &matrixBufferCPU, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		glBindBufferRange(GL_UNIFORM_BUFFER, 0, matrixBufferGPU, 0, sizeof(Matrices));

		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, z->numTriangles * 3);
		glBindVertexArray(0);
	}
}

void GameLoop()
{
	// adjust clock timer
	NewTime = clock();
	float gameSpeed = (float)(NewTime - OldTime) / 1000;
	OldTime = clock();

	// clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.392f, 0.584f, 0.929f, 0.0f);

	// increment animation timer
	animTime += 5.0f * gameSpeed;
	
	// most MD2 models have 198 frames, with range 0 to 197
	// This assures frameIndex and frameIndex + 1 are less than 198
	if ((int)animTime >= 197)
		animTime = 0;

	// interpolation [0 to 1] between frames
	matrixBufferCPU.frac = animTime - (int)animTime;

	// draw both of our characters,
	// each character has two MD2 models
	Draw(0);	// skeleton and bat
	Draw(1);	// dragon and rider

	glfwSwapBuffers(window);
	glfwPollEvents();
}

#define GLSL(src) "#version 450\n" #src

static const char* VS = GLSL(

// input from Vertex Buffer
layout(location = 0) in vec3 inPos1;
layout(location = 1) in vec3 inPos2;
layout(location = 2) in vec2 inUV;

// output to Pixel shader
out vec2 vtxUV;

// uniform buffer #1
layout(binding = 0) uniform Matrices
{
	mat4 model;
	mat4 view;
	mat4 proj;
	float frac;
};

void main()
{
	// interpolation
	vec3 pos = inPos1 * (1 - frac) + inPos2 * frac;

	// position
	gl_Position = proj * view * model * vec4(pos, 1.0);

	// Calculate UV
	vtxUV = inUV;
}
);

static const char* FS = GLSL(

// Input from Vertex Shader
in vec2 vtxUV;

layout(location = 0) uniform sampler2D tex_diffuse;

// output to FrameBuffer Attatchment
out vec4 fragColor;

void main()
{
	fragColor = texture(tex_diffuse, vtxUV);
}
);

int wmain()
{
	glfwInit();

	// Setting the OpenGL version, in this case 4.5
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);

	// MSAA
	glfwWindowHint(GLFW_SAMPLES, 9999);

	window = glfwCreateWindow(1280, 720, "Hello", NULL, NULL);
	glfwMakeContextCurrent(window);

	// Enable GLEW, setting glewExperimental to true.
	// This allows GLEW take the modern approach to retrive function pointers and extensions
	glewExperimental = GL_TRUE;

	// Initialize glew
	glewInit();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &VS, nullptr);
	glCompileShader(vs);

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &FS, nullptr);
	glCompileShader(fs);

	s_program = glCreateProgram();
	glAttachShader(s_program, vs);
	glAttachShader(s_program, fs);
	glLinkProgram(s_program);

	glDeleteShader(vs);
	glDeleteShader(fs);
	glUseProgram(s_program);

	// set viewport
	glViewport(0, 0, 1280, 720);

	// load the models and textures
	LoadAssets(0, 0);
	LoadAssets(0, 1);
	LoadAssets(1, 0);
	LoadAssets(1, 1);

	// start the clock
	OldTime = clock();

	// enable depth and textures
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &matrixBufferGPU);

	// matrices
	glm::mat4x4 proj = glm::perspective(45.0f * 3.14159f / 180.0f, 16.0f / 9.0f, 1.0f, 200.0f);

	glm::mat4x4 view = glm::lookAt(
		glm::vec3(150, 0, 0),
		glm::vec3(0, 0, 0),
		glm::vec3(0, 0, 1)
	);

	matrixBufferCPU.proj = proj;
	matrixBufferCPU.view = view;

	// start the loop
	while (1) GameLoop();
}


