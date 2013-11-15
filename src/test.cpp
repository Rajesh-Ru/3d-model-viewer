#include <Windows.h>
#include <WindowsX.h>
#include <CommCtrl.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "resource.h"
#include "SOIL.h"

#define IDC_MAIN_TOOL 201
#define IDC_MAIN_MDI 202
#define ID_OBJ_ROTATION 1001
#define ID_OBJ_TRANSLATION 1002
#define ID_OBJ_SCALING 1003
#define ID_OBJ_SUBDIV 1004
#define ID_OBJ_TEX 1005
#define ID_LIGHTING 1006
#define ID_MDI_FIRSTCHILD 50000

typedef struct _TexCoord
{
	float s;
	float t;
} TexCoord;

typedef struct _Normal
{
	float x;
	float y;
	float z;
} Normal;

typedef struct _Vertex
{
	float x;
	float y;
	float z;

	int i_edge;

	_Vertex() : x(-1.0f), y(-1.0f), z(-1.0f), i_edge(-1)
	{
	}
} Vertex;

typedef struct _HalfEdge
{
	int i_vertex;
	int i_pair;
	int i_face;
	int i_next;

	int i_newVertex;

	_HalfEdge() : i_vertex(-1), i_pair(-1), i_face(-1), i_next(-1),
		i_newVertex(-1)
	{
	}
} HEdge;

typedef struct _Face
{
	int i_edge;

	int i_n[3];
	int i_t[3];

	_Face() : i_edge(-1)
	{
	}
} Face;

typedef struct _Mesh
{
	std::vector<Vertex> v;
	std::vector<Normal> n;
	std::vector<TexCoord> t;
	std::vector<HEdge> he;
	std::vector<Face> f;

	void clear()
	{
		v.clear();
		n.clear();
		t.clear();
		he.clear();
		f.clear();
	}
} Mesh;

enum OPEN_FILE_TYPE {MODEL_FILE, TEXTURE_IMAGE};

const wchar_t *g_szClassName = L"myWindowClass";
const wchar_t *g_szChildClassName = L"myMDIChildWindowClass";

HDC g_hdc = NULL;
HGLRC g_hglrc = NULL;

HWND g_hMainWindow = NULL;
HWND g_hMDIClient = NULL;

Mesh g_3DModel;
bool g_bSubdivided = false;

GLuint g_uiTexName;
bool g_bTexturingEnabled = false;
bool g_bTextureImageExist = false;

float g_fBoundaryX = -1.0f;
float g_fBoundaryY = -1.0f;
float g_fBoundaryZ = -1.0f;

bool g_bLightingEnabled = true;

bool g_bSuccessfulReading = false;

// return true if the orientation agrees and return false otherwise
bool setPair(HEdge &new_e, Mesh &mesh, int i_start, int i_end, int i_new)
{
	for (int i = mesh.he.size() - 1; i >= 0; --i)
	{
		if (mesh.he[i].i_vertex == i_end && mesh.he[mesh.he[i].i_next].i_vertex == i_start)
		{
			new_e.i_pair = i;
			mesh.he[i].i_pair = i_new;
			return true;
		}
		else if (mesh.he[i].i_vertex == i_start && mesh.he[mesh.he[i].i_next].i_vertex == i_end)
		{
			new_e.i_pair = i;
			mesh.he[i].i_pair = i_new;
			return false;
		}
	}
	return true;
}

// -1 for missing indices
void buildFaceAndHalfEdges(Mesh &mesh, int iv1, int it1, int in1,
	int iv2, int it2, int in2, int iv3, int it3, int in3)
{
	HEdge new_e1, new_e2, new_e3;
	Face new_f;
	bool consistentOrientation = true;
				
	// first edge of the face and the face
	new_f.i_edge = mesh.he.size();
	new_f.i_t[0] = it1;
	new_f.i_n[0] = in1;
	new_f.i_t[1] = it2;
	new_f.i_n[1] = in2;
	new_f.i_t[2] = it3;
	new_f.i_n[2] = in3;

	new_e1.i_face = mesh.f.size();
	new_e1.i_next = mesh.he.size() + 1;
	new_e1.i_vertex = iv1;
	if (consistentOrientation)
		consistentOrientation = setPair(new_e1, mesh, iv1, iv2, mesh.he.size());
	else
		setPair(new_e1, mesh, iv1, iv2, mesh.he.size());
	if (mesh.v[iv1].i_edge == -1)
		mesh.v[iv1].i_edge = mesh.he.size();
	//if (in1 != -1 && mesh.v[iv1].i_n == -1)
	//	mesh.v[iv1].i_n = in1;
	//if (it1 != -1 && mesh.v[iv1].i_t == -1)
	//	mesh.v[iv1].i_t = it1;

	mesh.f.push_back(new_f);
	mesh.he.push_back(new_e1);

	// second edge of the face
	new_e2.i_face = mesh.f.size() - 1;
	new_e2.i_next = mesh.he.size() + 1;
	new_e2.i_vertex = iv2;
	if (consistentOrientation)
		consistentOrientation = setPair(new_e2, mesh, iv2, iv3, mesh.he.size());
	else
		setPair(new_e2, mesh, iv2, iv3, mesh.he.size());
	if (mesh.v[iv2].i_edge == -1)
		mesh.v[iv2].i_edge = mesh.he.size();
	//if (in2 != -1 && mesh.v[iv2].i_n == -1)
	//	mesh.v[iv2].i_n = in2;
	//if (it2 != -1 && mesh.v[iv2].i_t == -1)
	//	mesh.v[iv2].i_t = it2;

	mesh.he.push_back(new_e2);

	// third edge of the face
	new_e3.i_face = mesh.f.size() - 1;
	new_e3.i_next = mesh.he.size() - 2;
	new_e3.i_vertex = iv3;
	if (consistentOrientation)
		consistentOrientation = setPair(new_e3, mesh, iv3, iv1, mesh.he.size());
	else
		setPair(new_e3, mesh, iv3, iv1, mesh.he.size());
	if (mesh.v[iv3].i_edge == -1)
		mesh.v[iv3].i_edge = mesh.he.size();
	//if (in3 != -1 && mesh.v[iv3].i_n == -1)
	//	mesh.v[iv3].i_n = in3;
	//if (it3 != -1 && mesh.v[iv3].i_t == -1)
	//	mesh.v[iv3].i_t = it3;

	mesh.he.push_back(new_e3);

	if (!consistentOrientation)
	{
		int idx = mesh.he.size() - 3;

		mesh.he[idx].i_vertex = iv2;
		if (mesh.v[iv1].i_edge == idx)
			mesh.v[iv1].i_edge = idx + 1;

		mesh.he[idx+1].i_vertex = iv1;
		if (mesh.v[iv2].i_edge == idx + 1)
			mesh.v[iv2].i_edge = idx;
	}
}

// read an .obj file and store data using half-edge data structure
bool readOBJ(const char *fn, Mesh &mesh)
{
	FILE *pObjFile = fopen(fn, "r");
	if (pObjFile == NULL)
		return false;

	char type[100];
	int iv1, iv2, iv3;
	int in1, in2, in3;
	int it1, it2, it3;
	Normal new_n;
	Vertex new_v;
	TexCoord new_t;
	bool doNotCheck = false;

	g_fBoundaryX = -1.0f;
	g_fBoundaryY = -1.0f;
	g_fBoundaryZ = -1.0f;

	while(doNotCheck || fscanf(pObjFile, "%s", type) != EOF)
	{
		doNotCheck = false;

		if (strcmp(type, "v") == 0)
		{
			fscanf(pObjFile, "%f %f %f", &new_v.x, &new_v.y, &new_v.z);
			if (abs(new_v.x) > g_fBoundaryX)
				g_fBoundaryX = abs(new_v.x);
			if (abs(new_v.y) > g_fBoundaryY)
				g_fBoundaryY = abs(new_v.y);
			if (abs(new_v.z) > g_fBoundaryZ)
				g_fBoundaryZ = abs(new_v.z);
			mesh.v.push_back(new_v);
		}
		else if (strcmp(type, "vt") == 0)
		{
			fscanf(pObjFile, "%f %f", &new_t.s, &new_t.t);
			fscanf(pObjFile, "%s", type);
			if (!isdigit(type[0]) && !isdigit(type[1]))
				doNotCheck = true;
			mesh.t.push_back(new_t);
		}
		else if (strcmp(type, "vn") == 0)
		{
			fscanf(pObjFile, "%f %f %f", &new_n.x, &new_n.y, &new_n.z);
			mesh.n.push_back(new_n);
		}
		else if (strcmp(type, "f") == 0)
		{
			if (mesh.n.size() != 0 && mesh.t.size() != 0)
			{
				fscanf(pObjFile, "%d/%d/%d %d/%d/%d %d/%d/%d",
					&iv1, &it1, &in1, &iv2, &it2, &in2, &iv3, &it3, &in3);
				
				buildFaceAndHalfEdges(mesh, iv1 - 1, it1 - 1, in1 - 1,
					iv2 - 1, it2 - 1, in2 - 1, iv3 - 1, it3 - 1, in3 - 1);
			}
			else if (mesh.t.size() != 0 && mesh.n.size() == 0)
			{
				fscanf(pObjFile, "%d/%d %d/%d %d/%d",
					&iv1, &it1, &iv2, &it2, &iv3, &it3);

				buildFaceAndHalfEdges(mesh, iv1 - 1, it1 - 1, -1,
					iv2 - 1, it2 - 1, -1, iv3 - 1, it3 - 1, -1);
			}
			else if (mesh.t.size() == 0 && mesh.n.size() != 0)
			{
				fscanf(pObjFile, "%d//%d %d//%d %d//%d",
					&iv1, &in1, &iv2, &in2, &iv3, &in3);

				buildFaceAndHalfEdges(mesh, iv1 - 1, -1, in1 - 1,
					iv2 - 1, -1, in2 - 1, iv3 - 1, -1, in3 - 1);
			}
			else
			{
				fscanf(pObjFile, "%d %d %d", &iv1, &iv2, &iv3);

				buildFaceAndHalfEdges(mesh, iv1 - 1, -1, -1, iv2 - 1, -1, -1, iv3 - 1, -1, -1);
			}
		}
	}

	fclose(pObjFile);
	return true;
}

void DoFileOpen(HWND hwnd, OPEN_FILE_TYPE type)
{
	OPENFILENAME ofn;
	wchar_t szFileName[MAX_PATH] = L"";
	char cszFileName[MAX_PATH] = "";

	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	switch (type)
	{
		case MODEL_FILE:
			ofn.lpstrFilter = L"Object Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0\0";
			ofn.lpstrDefExt = L"obj";
			break;
		case TEXTURE_IMAGE:
			ofn.lpstrFilter = L"Image Files (*.jpg;*.png;*.bmp;*.tga;*.dds)\0*.jpg;*.png;*.bmp;*.tga;*.dds\0All Files (*.*)\0*.*\0\0";
			ofn.lpstrDefExt = L"jpg";
			break;
	}

	if(GetOpenFileName(&ofn))
	{
		size_t wstr_len = wcslen(szFileName) + 1;
		size_t convertedChars = 0;
		wcstombs_s(&convertedChars, cszFileName, wstr_len, szFileName, _TRUNCATE);

		switch (type)
		{
			case MODEL_FILE:
				g_3DModel.clear();

				if (!readOBJ(cszFileName, g_3DModel))
					MessageBox(g_hMainWindow, L"Could not open file!", L"Error", MB_OK | MB_ICONEXCLAMATION);

				g_bSuccessfulReading = true;
				break;
			case TEXTURE_IMAGE:
			{
				int width, height;
				unsigned char *image;

				image = SOIL_load_image(cszFileName, &width, &height, 0, SOIL_LOAD_RGB);
				gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, GL_RGB, GL_UNSIGNED_BYTE, image);
				SOIL_free_image_data(image);
				
				g_bTextureImageExist = true;
			}
			break;
		}
	}
	else
	{
		switch (type)
		{
			case TEXTURE_IMAGE:
				g_bTextureImageExist = false;
				break;
			case MODEL_FILE:
				g_bSuccessfulReading = false;
				break;
		}
	}
}

void initTexturing()
{
	glGenTextures(1, &g_uiTexName);
	// After binding, whatever you've done with texturing is related
	// to the binded texture object. For example, every texture image
	// that you specify using glTexImage2D will change the texture
	// image of the binded texture object (that is, not other texture
	// objects).
	glBindTexture(GL_TEXTURE_2D, g_uiTexName);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexEnvf(GL_TEXTURE, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void EnableOpenGL(HWND hwnd, HDC *hdc, HGLRC *hrc)
{
	PIXELFORMATDESCRIPTOR pfd;
	int iFormat;

	*hdc = GetDC(hwnd);

	ZeroMemory(&pfd, sizeof(pfd));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;

	iFormat = ChoosePixelFormat(*hdc, &pfd);
	SetPixelFormat(*hdc, iFormat, &pfd);

	*hrc = wglCreateContext(*hdc);
	wglMakeCurrent(*hdc, *hrc);
}

void DisableOpenGL(HWND hwnd, HDC hdc, HGLRC hrc)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(hrc);
	ReleaseDC(hwnd, hdc);
}

void drawCube()
{
	static const float v_data[12*3*3] = {
		-0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5,
		0.5, -0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5,
		-0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5,
		-0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, 0.5, 0.5,
		-0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, 0.5,
		0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5,
		0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5,
		-0.5, -0.5, -0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5,
		0.5, 0.5, -0.5, 0.5, -0.5, -0.5, 0.5, 0.5, 0.5,
		0.5, 0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5,
		0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5,
		-0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5};

	glBegin(GL_TRIANGLES);
	for (int i = 0; i < 12*3; ++i)
		glVertex3fv(&v_data[3*i]);
	glEnd();
}

void drawMesh(Mesh &mesh)
{
	int ie, iv, in, it;

	if (mesh.n.size() != 0 && mesh.t.size() != 0 && g_bTexturingEnabled
		&& !g_bSubdivided && g_bLightingEnabled)
	{
		glBindTexture(GL_TEXTURE_2D, g_uiTexName);
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < mesh.f.size(); ++i)
		{
			ie = mesh.f[i].i_edge;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[0];
			it = mesh.f[i].i_t[0];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[1];
			it = mesh.f[i].i_t[1];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[2];
			it = mesh.f[i].i_t[2];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);
		}
		glEnd();
	}
	else if (mesh.n.size() == 0 && mesh.t.size() != 0 && g_bTexturingEnabled
		&& !g_bSubdivided && g_bLightingEnabled)
	{
		glBindTexture(GL_TEXTURE_2D, g_uiTexName);
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < mesh.f.size(); ++i)
		{
			ie = mesh.f[i].i_edge;
			iv = mesh.he[ie].i_vertex;
			it = mesh.f[i].i_t[0];
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			it = mesh.f[i].i_t[1];
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			it = mesh.f[i].i_t[2];
			glTexCoord2f(mesh.t[it].s, mesh.t[it].t);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);
		}
		glEnd();
	}
	else if (mesh.n.size() != 0 && (mesh.t.size() == 0 || !g_bTexturingEnabled)
		&& !g_bSubdivided && g_bLightingEnabled)
	{
		glBegin(GL_TRIANGLES);
		for (int i = 0; i < mesh.f.size(); ++i)
		{
			ie = mesh.f[i].i_edge;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[0];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[1];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			in = mesh.f[i].i_n[2];
			glNormal3f(mesh.n[in].x, mesh.n[in].y, mesh.n[in].z);
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);
		}
		glEnd();
	}
	else
	{
		if (!g_bSubdivided && g_bLightingEnabled)
			glBegin(GL_TRIANGLES);
		else
		{
			glDisable(GL_LIGHTING);
			glDisable(GL_TEXTURE_2D);
			glColor3f(1.0f, 1.0f, 1.0f);
		}

		for (int i = 0; i < mesh.f.size(); ++i)
		{
			if (g_bSubdivided || !g_bLightingEnabled)
				glBegin(GL_LINE_LOOP);
			ie = mesh.f[i].i_edge;
			iv = mesh.he[ie].i_vertex;
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);

			ie = mesh.he[ie].i_next;
			iv = mesh.he[ie].i_vertex;
			glVertex3f(mesh.v[iv].x, mesh.v[iv].y, mesh.v[iv].z);
			if (g_bSubdivided || !g_bLightingEnabled)
				glEnd();
		}
		if (!g_bSubdivided && g_bLightingEnabled)
			glEnd();
	}
}

void loopSubdiv(Mesh &mesh)
{
	Mesh oldMesh = mesh;
	float rsX, rsY, rsZ;
	int vc;
	HEdge *pCurEdge = NULL;
	Vertex *pCurVertex = NULL;
	Vertex *pTargetVertex = NULL;
	bool isBoundary;
	//float rstS, rstT;
	//float rsnX, rsnY, rsnZ;

	// update existing vertices
	for (int i = 0; i < mesh.v.size(); ++i)
	{
		isBoundary = false;
		pTargetVertex = &mesh.v[i];
		pCurEdge = &oldMesh.he[oldMesh.v[i].i_edge];
		pCurVertex = &oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex];
		rsX = pCurVertex->x;
		rsY = pCurVertex->y;
		rsZ = pCurVertex->z;
		//if (oldMesh.t.size() != 0)
		//{
		//	rstS = oldMesh.t[pCurVertex->i_t].s;
		//	rstT = oldMesh.t[pCurVertex->i_t].t;
		//}
		//if (oldMesh.n.size() != 0)
		//{
		//	rsnX = oldMesh.n[pCurVertex->i_n].x;
		//	rsnY = oldMesh.n[pCurVertex->i_n].y;
		//	rsnZ = oldMesh.n[pCurVertex->i_n].z;
		//}
		vc = 1;

		if (pCurEdge->i_pair == -1)
		{
			pCurEdge = &oldMesh.he[oldMesh.he[pCurEdge->i_next].i_next];
			while (pCurEdge->i_pair != -1)
				pCurEdge = &oldMesh.he[oldMesh.he[oldMesh.he[pCurEdge->i_pair].i_next].i_next];
			pCurVertex = &oldMesh.v[pCurEdge->i_vertex];
			rsX += pCurVertex->x;
			rsY += pCurVertex->y;
			rsZ += pCurVertex->z;
			//if (oldMesh.t.size() != 0)
			//{
			//	rstS += oldMesh.t[pCurVertex->i_t].s;
			//	rstT += oldMesh.t[pCurVertex->i_t].t;
			//}
			//if (oldMesh.n.size() != 0)
			//{
			//	rsnX += oldMesh.n[pCurVertex->i_n].x;
			//	rsnY += oldMesh.n[pCurVertex->i_n].y;
			//	rsnZ += oldMesh.n[pCurVertex->i_n].z;
			//}
			pTargetVertex->x = 0.75f * pTargetVertex->x + 0.125f * rsX;
			pTargetVertex->y = 0.75f * pTargetVertex->y + 0.125f * rsY;
			pTargetVertex->z = 0.75f * pTargetVertex->z + 0.125f * rsZ;
			//if (oldMesh.t.size() != 0)
			//{
			//	mesh.t[pTargetVertex->i_t].s = 0.75f * mesh.t[pTargetVertex->i_t].s + 0.125f * rstS;
			//	mesh.t[pTargetVertex->i_t].t = 0.75f * mesh.t[pTargetVertex->i_t].t + 0.125f * rstT;
			//}
			//if (oldMesh.n.size() != 0)
			//{
			//	mesh.n[pTargetVertex->i_n].x = 0.75f * mesh.n[pTargetVertex->i_n].x + 0.125f * rsnX;
			//	mesh.n[pTargetVertex->i_n].y = 0.75f * mesh.n[pTargetVertex->i_n].y + 0.125f * rsnY;
			//	mesh.n[pTargetVertex->i_n].z = 0.75f * mesh.n[pTargetVertex->i_n].z + 0.125f * rsnZ;
			//}
			continue;
		}

		while (oldMesh.he[pCurEdge->i_pair].i_next != pTargetVertex->i_edge)
		{
			pCurEdge = &oldMesh.he[oldMesh.he[pCurEdge->i_pair].i_next];
			pCurVertex = &oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex];

			if (pCurEdge->i_pair == -1)
			{
				pCurVertex = &oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex];
				rsX = pCurVertex->x;
				rsY = pCurVertex->y;
				rsZ = pCurVertex->z;
				//if (oldMesh.t.size() != 0)
				//{
				//	rstS = oldMesh.t[pCurVertex->i_t].s;
				//	rstT = oldMesh.t[pCurVertex->i_t].t;
				//}
				//if (oldMesh.n.size() != 0)
				//{
				//	rsnX = oldMesh.n[pCurVertex->i_n].x;
				//	rsnY = oldMesh.n[pCurVertex->i_n].y;
				//	rsnZ = oldMesh.n[pCurVertex->i_n].z;
				//}

				pCurEdge = &oldMesh.he[oldMesh.he[pCurEdge->i_next].i_next];
				while (pCurEdge->i_pair != -1)
					pCurEdge = &oldMesh.he[oldMesh.he[oldMesh.he[pCurEdge->i_pair].i_next].i_next];
				pCurVertex = &oldMesh.v[pCurEdge->i_vertex];
				rsX += pCurVertex->x;
				rsY += pCurVertex->y;
				rsZ += pCurVertex->z;
				//if (oldMesh.t.size() != 0)
				//{
				//	rstS += oldMesh.t[pCurVertex->i_t].s;
				//	rstT += oldMesh.t[pCurVertex->i_t].t;
				//}
				//if (oldMesh.n.size() != 0)
				//{
				//	rsnX += oldMesh.n[pCurVertex->i_n].x;
				//	rsnY += oldMesh.n[pCurVertex->i_n].y;
				//	rsnZ += oldMesh.n[pCurVertex->i_n].z;
				//}
				pTargetVertex->x = 0.75f * pTargetVertex->x + 0.125f * rsX;
				pTargetVertex->y = 0.75f * pTargetVertex->y + 0.125f * rsY;
				pTargetVertex->z = 0.75f * pTargetVertex->z + 0.125f * rsZ;
				//if (oldMesh.t.size() != 0)
				//{
				//	mesh.t[pTargetVertex->i_t].s = 0.75f * mesh.t[pTargetVertex->i_t].s + 0.125f * rstS;
				//	mesh.t[pTargetVertex->i_t].t = 0.75f * mesh.t[pTargetVertex->i_t].t + 0.125f * rstT;
				//}
				//if (oldMesh.n.size() != 0)
				//{
				//	mesh.n[pTargetVertex->i_n].x = 0.75f * mesh.n[pTargetVertex->i_n].x + 0.125f * rsnX;
				//	mesh.n[pTargetVertex->i_n].y = 0.75f * mesh.n[pTargetVertex->i_n].y + 0.125f * rsnY;
				//	mesh.n[pTargetVertex->i_n].z = 0.75f * mesh.n[pTargetVertex->i_n].z + 0.125f * rsnZ;
				//}
				isBoundary = true;
				break;
			}

			rsX += pCurVertex->x;
			rsY += pCurVertex->y;
			rsZ += pCurVertex->z;
			//if (oldMesh.t.size() != 0)
			//{
			//	rstS += oldMesh.t[pCurVertex->i_t].s;
			//	rstT += oldMesh.t[pCurVertex->i_t].t;
			//}
			//if (oldMesh.n.size() != 0)
			//{
			//	rsnX += oldMesh.n[pCurVertex->i_n].x;
			//	rsnY += oldMesh.n[pCurVertex->i_n].y;
			//	rsnZ += oldMesh.n[pCurVertex->i_n].z;
			//}
			++vc;
		}

		if (isBoundary)
			continue;

		float beta = (5.0f/8.0f-pow(3.0f/8.0f+1.0f/4.0f*cos(2*M_PI/(float)vc), 2))/(float)vc;
		float alpha = 1.0f - (float)vc * beta;
		pTargetVertex->x = alpha * pTargetVertex->x + beta * rsX;
		pTargetVertex->y = alpha * pTargetVertex->y + beta * rsY;
		pTargetVertex->z = alpha * pTargetVertex->z + beta * rsZ;
		//if (oldMesh.t.size() != 0)
		//{
		//	mesh.t[pTargetVertex->i_t].s = alpha * mesh.t[pTargetVertex->i_t].s + beta * rstS;
		//	mesh.t[pTargetVertex->i_t].t = alpha * mesh.t[pTargetVertex->i_t].t + beta * rstT;
		//}
		//if (oldMesh.n.size() != 0)
		//{
		//	mesh.n[pTargetVertex->i_n].x = alpha * mesh.n[pTargetVertex->i_n].x + beta * rsnX;
		//	mesh.n[pTargetVertex->i_n].y = alpha * mesh.n[pTargetVertex->i_n].y + beta * rsnY;
		//	mesh.n[pTargetVertex->i_n].z = alpha * mesh.n[pTargetVertex->i_n].z + beta * rsnZ;
		//}
	}

	// insert new vertices
	for (int i = 0; i < mesh.he.size(); ++i)
	{
		pCurEdge = &oldMesh.he[i];

		if (pCurEdge->i_newVertex == -1)
		{
			Vertex new_v;
			//TexCoord new_t;
			//Normal new_n;

			if (pCurEdge->i_pair == -1)
			{
				new_v.x = 0.5f * oldMesh.v[pCurEdge->i_vertex].x
					+ 0.5f * oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].x;
				new_v.y = 0.5f * oldMesh.v[pCurEdge->i_vertex].y
					+ 0.5f * oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].y;
				new_v.z = 0.5f * oldMesh.v[pCurEdge->i_vertex].z
					+ 0.5f * oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].z;
				//if (oldMesh.t.size() != 0)
				//{
				//	new_t.s = 0.5f * oldMesh.t[oldMesh.v[pCurEdge->i_vertex].i_t].s
				//		+ 0.5f * oldMesh.t[oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].i_t].s;
				//	new_t.t = 0.5f * oldMesh.t[oldMesh.v[pCurEdge->i_vertex].i_t].t
				//		+ 0.5f * oldMesh.t[oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].i_t].t;
				//	new_v.i_t = mesh.t.size();
				//	mesh.t.push_back(new_t);
				//}
				//if (oldMesh.n.size() != 0)
				//{
				//	new_n.x = 0.5f * oldMesh.n[oldMesh.v[pCurEdge->i_vertex].i_n].x
				//		+ 0.5f * oldMesh.n[oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].i_n].x;
				//	new_n.y = 0.5f * oldMesh.n[oldMesh.v[pCurEdge->i_vertex].i_n].y
				//		+ 0.5f * oldMesh.n[oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].i_n].y;
				//	new_n.z = 0.5f * oldMesh.n[oldMesh.v[pCurEdge->i_vertex].i_n].z
				//		+ 0.5f * oldMesh.n[oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex].i_n].z;
				//	new_v.i_n = mesh.n.size();
				//	mesh.n.push_back(new_n);
				//}

				pCurEdge->i_newVertex = mesh.v.size();
				mesh.v.push_back(new_v);
			}
			else
			{
				Vertex *pv1 = &oldMesh.v[pCurEdge->i_vertex];
				Vertex *pv2 = &oldMesh.v[oldMesh.he[pCurEdge->i_next].i_vertex];
				Vertex *pv3 = &oldMesh.v[oldMesh.he[oldMesh.he[pCurEdge->i_next].i_next].i_vertex];
				Vertex *pv4 = &oldMesh.v[oldMesh.he[oldMesh.he[oldMesh.he[pCurEdge->i_pair].i_next].i_next].i_vertex];
				
				new_v.x = 3.0f/8.0f*(pv1->x + pv2->x) + 1.0f/8.0f*(pv3->x + pv4->x);
				new_v.y = 3.0f/8.0f*(pv1->y + pv2->y) + 1.0f/8.0f*(pv3->y + pv4->y);
				new_v.z = 3.0f/8.0f*(pv1->z + pv2->z) + 1.0f/8.0f*(pv3->z + pv4->z);
				//if (oldMesh.t.size() != 0)
				//{
				//	new_t.s = 3.0f/8.0f*(oldMesh.t[pv1->i_t].s + oldMesh.t[pv2->i_t].s)
				//		+ 1.0f/8.0f*(oldMesh.t[pv3->i_t].s + oldMesh.t[pv4->i_t].s);
				//	new_t.t = 3.0f/8.0f*(oldMesh.t[pv1->i_t].t + oldMesh.t[pv2->i_t].t)
				//		+ 1.0f/8.0f*(oldMesh.t[pv3->i_t].t + oldMesh.t[pv4->i_t].t);
				//	new_v.i_t = mesh.t.size();
				//	mesh.t.push_back(new_t);
				//}
				//if (oldMesh.n.size() != 0)
				//{
				//	new_n.x = 3.0f/8.0f*(oldMesh.n[pv1->i_n].x + oldMesh.n[pv2->i_n].x)
				//		+ 1.0f/8.0f*(oldMesh.n[pv3->i_n].x + oldMesh.n[pv4->i_n].x);
				//	new_n.y = 3.0f/8.0f*(oldMesh.n[pv1->i_n].y + oldMesh.n[pv2->i_n].y)
				//		+ 1.0f/8.0f*(oldMesh.n[pv3->i_n].y + oldMesh.n[pv4->i_n].y);
				//	new_n.z = 3.0f/8.0f*(oldMesh.n[pv1->i_n].z + oldMesh.n[pv2->i_n].z)
				//		+ 1.0f/8.0f*(oldMesh.n[pv3->i_n].z + oldMesh.n[pv4->i_n].z);
				//	new_v.i_n = mesh.n.size();
				//	mesh.n.push_back(new_n);
				//}

				oldMesh.he[pCurEdge->i_pair].i_newVertex = pCurEdge->i_newVertex = mesh.v.size();
				mesh.v.push_back(new_v);
			}
		}
	}

	// update topology
	for (int i = 0; i < oldMesh.f.size(); ++i)
	{
		HEdge new_e1, new_e2, new_e3, new_e4, new_e5, new_e6, new_e7, new_e8, new_e9;
		Face new_f1, new_f2, new_f3;
		HEdge *pTargetEdge1, *pTargetEdge2, *pTargetEdge3;

		pTargetEdge1 = &mesh.he[mesh.f[i].i_edge];
		pTargetEdge2 = &mesh.he[pTargetEdge1->i_next];
		pTargetEdge3 = &mesh.he[pTargetEdge2->i_next];

		pCurEdge = &oldMesh.he[oldMesh.f[i].i_edge];
		pTargetEdge1->i_face = mesh.f.size();
		pTargetEdge1->i_next = mesh.he.size() + 2;
		if (pCurEdge->i_pair != -1)
			mesh.he[pCurEdge->i_pair].i_pair = mesh.he.size();
		new_e1.i_face = mesh.f.size() + 1;
		new_e1.i_next = pCurEdge->i_next;
		new_e1.i_vertex = pCurEdge->i_newVertex;
		new_e1.i_pair = pCurEdge->i_pair;
		new_e2.i_face = pCurEdge->i_face;
		new_e2.i_next = mesh.he.size() + 4;
		new_e2.i_vertex = pCurEdge->i_newVertex;
		new_e2.i_pair = mesh.he.size() + 5;
		new_e3.i_face = mesh.f.size();
		new_e3.i_next = mesh.he.size() + 6;
		new_e3.i_vertex = pCurEdge->i_newVertex;
		new_e3.i_pair = mesh.he.size() + 7;
		new_f1.i_edge = mesh.he.size() + 2;
		mesh.v[pCurEdge->i_newVertex].i_edge = mesh.he.size(); 

		pCurEdge = &oldMesh.he[pCurEdge->i_next];
		pTargetEdge2->i_face = mesh.f.size() + 1;
		pTargetEdge2->i_next = mesh.he.size() + 5;
		if (pCurEdge->i_pair != -1)
			mesh.he[pCurEdge->i_pair].i_pair = mesh.he.size() + 3;
		new_e4.i_face = mesh.f.size() + 2;
		new_e4.i_next = pCurEdge->i_next;
		new_e4.i_vertex = pCurEdge->i_newVertex;
		new_e4.i_pair = pCurEdge->i_pair;
		new_e5.i_face = pCurEdge->i_face;
		new_e5.i_next = mesh.he.size() + 7;
		new_e5.i_vertex = pCurEdge->i_newVertex;
		new_e5.i_pair = mesh.he.size() + 8;
		new_e6.i_face = mesh.f.size() + 1;
		new_e6.i_next = mesh.he.size();
		new_e6.i_vertex = pCurEdge->i_newVertex;
		new_e6.i_pair = mesh.he.size() + 1;
		new_f2.i_edge = mesh.he.size() + 5;
		mesh.v[pCurEdge->i_newVertex].i_edge = mesh.he.size() + 3;

		pCurEdge = &oldMesh.he[pCurEdge->i_next];
		pTargetEdge3->i_face = mesh.f.size() + 2;
		pTargetEdge3->i_next = mesh.he.size() + 8;
		if (pCurEdge->i_pair != -1)
			mesh.he[pCurEdge->i_pair].i_pair = mesh.he.size() + 6;
		new_e7.i_face = mesh.f.size();
		new_e7.i_next = pCurEdge->i_next;
		new_e7.i_vertex = pCurEdge->i_newVertex;
		new_e7.i_pair = pCurEdge->i_pair;
		new_e8.i_face = pCurEdge->i_face;
		new_e8.i_next = mesh.he.size() + 1;
		new_e8.i_vertex = pCurEdge->i_newVertex;
		new_e8.i_pair = mesh.he.size() + 2;
		new_e9.i_face = mesh.f.size() + 2;
		new_e9.i_next = mesh.he.size() + 3;
		new_e9.i_vertex = pCurEdge->i_newVertex;
		new_e9.i_pair = mesh.he.size() + 4;
		new_f3.i_edge = mesh.he.size() + 8;
		mesh.v[pCurEdge->i_newVertex].i_edge = mesh.he.size() + 6;

		mesh.f[i].i_edge = mesh.he.size() + 7;

		mesh.he.push_back(new_e1);
		mesh.he.push_back(new_e2);
		mesh.he.push_back(new_e3);
		mesh.he.push_back(new_e4);
		mesh.he.push_back(new_e5);
		mesh.he.push_back(new_e6);
		mesh.he.push_back(new_e7);
		mesh.he.push_back(new_e8);
		mesh.he.push_back(new_e9);

		mesh.f.push_back(new_f1);
		mesh.f.push_back(new_f2);
		mesh.f.push_back(new_f3);
	}

	g_bSubdivided = true;
}

HWND CreateNewMDIChild(HWND hMDIClient)
{
	MDICREATESTRUCT mcs;
	HWND hChild;

	mcs.szTitle = L"Model Viewer";
	mcs.szClass = g_szChildClassName;
	mcs.hOwner  = GetModuleHandle(NULL);
	mcs.x = mcs.cx = CW_USEDEFAULT;
	mcs.y = mcs.cy = CW_USEDEFAULT;
	mcs.style = WS_MAXIMIZE;

	hChild = (HWND)SendMessage(hMDIClient, WM_MDICREATE, 0, (LONG)&mcs);
	if(!hChild)
	{
		MessageBox(hMDIClient, L"MDI Child creation failed.", L"Error", MB_ICONEXCLAMATION | MB_OK);
	}
	return hChild;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
			// MDI client window related
			CLIENTCREATESTRUCT ccs;

			ccs.hWindowMenu = GetSubMenu(GetMenu(hwnd), 1);
			ccs.idFirstChild = ID_MDI_FIRSTCHILD;

			g_hMDIClient = CreateWindowEx(0, L"mdiclient", NULL,
				WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				hwnd, (HMENU)IDC_MAIN_MDI, GetModuleHandle(NULL), (LPVOID)&ccs);
			if(g_hMDIClient == NULL)
				MessageBox(hwnd, L"Could not create MDI client!", L"Error", MB_OK | MB_ICONERROR);

			// toolbar related
			HWND hTool = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE,
				0, 0, 0, 0, hwnd, (HMENU)IDC_MAIN_TOOL, GetModuleHandle(NULL), NULL);
			if (hTool == NULL)
				MessageBox(hwnd, L"Could not create toolbar!", L"Error", MB_OK | MB_ICONEXCLAMATION);

			SendMessage(hTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

			DWORD bkColor = GetSysColor(COLOR_BTNFACE);
			COLORMAP colorMap = {RGB(0, 0, 0), bkColor};
			HBITMAP hbm = CreateMappedBitmap(GetModuleHandle(NULL), IDB_BUTTONBITMAP, 0, &colorMap, 1);

			TBADDBITMAP tbab;
			tbab.hInst = NULL;
			tbab.nID = (UINT_PTR)hbm;

			int index = SendMessage(hTool, TB_ADDBITMAP, 6, (LPARAM)&tbab);

			TBBUTTON tbb[6];

			ZeroMemory(&tbb, sizeof(tbb));
			tbb[0].iBitmap = index;
			tbb[0].fsState = TBSTATE_ENABLED;
			tbb[0].fsStyle = TBSTYLE_BUTTON;
			tbb[0].idCommand = ID_OBJ_TRANSLATION;

			tbb[1].iBitmap = index + 1;
			tbb[1].fsState = TBSTATE_ENABLED;
			tbb[1].fsStyle = TBSTYLE_BUTTON;
			tbb[1].idCommand = ID_OBJ_ROTATION;

			tbb[2].iBitmap = index + 2;
			tbb[2].fsState = TBSTATE_ENABLED;
			tbb[2].fsStyle = TBSTYLE_BUTTON;
			tbb[2].idCommand = ID_OBJ_SCALING;

			tbb[3].iBitmap = index + 3;
			tbb[3].fsState = TBSTATE_ENABLED;
			tbb[3].fsStyle = TBSTYLE_BUTTON;
			tbb[3].idCommand = ID_OBJ_SUBDIV;

			tbb[4].iBitmap = index + 4;
			tbb[4].fsState = TBSTATE_ENABLED;
			tbb[4].fsStyle = TBSTYLE_BUTTON;
			tbb[4].idCommand = ID_OBJ_TEX;

			tbb[5].iBitmap = index + 5;
			tbb[5].fsState = TBSTATE_ENABLED;
			tbb[5].fsStyle = TBSTYLE_BUTTON;
			tbb[5].idCommand = ID_LIGHTING;

			SendMessage(hTool, TB_ADDBUTTONS, sizeof(tbb)/sizeof(TBBUTTON), (LPARAM)&tbb);

			// MDI child window related
			HWND hMDIChild = CreateNewMDIChild(g_hMDIClient);
		}
		break;
		case WM_SIZE:
		{
			// size the toolbar
			HWND hTool;
			RECT rcTool;
			int iToolHeight;

			hTool = GetDlgItem(hwnd, IDC_MAIN_TOOL);
			SendMessage(hTool, TB_AUTOSIZE, 0, 0);

			GetWindowRect(hTool, &rcTool);
			iToolHeight = rcTool.bottom - rcTool.top;

			// size the MDI client window
			HWND hMDI;
			RECT rcClient;
			int iMDIHeight;

			GetClientRect(hwnd, &rcClient);
			iMDIHeight = rcClient.bottom - iToolHeight;

			hMDI = GetDlgItem(hwnd, IDC_MAIN_MDI);
			SetWindowPos(hMDI, NULL, 0, iToolHeight, rcClient.right, iMDIHeight, SWP_NOZORDER);
		}
		break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_FILE_EXIT:
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;
				case ID_FILE_OPEN1:
				{
					HWND hChild;

					g_bSuccessfulReading = false;
					DoFileOpen(hwnd, MODEL_FILE);

					if (g_bSuccessfulReading)
					{
						glDisable(GL_TEXTURE_2D);
						glEnable(GL_LIGHTING);
						g_bSubdivided = false;
						g_bTexturingEnabled = false;
						g_bTextureImageExist = false;
						g_bLightingEnabled = true;
					}

					hChild = (HWND)SendMessage(g_hMDIClient, WM_MDIGETACTIVE, 0, 0);
					if (hChild)
					{
						ShowWindow(hChild, SW_SHOWMINIMIZED);
						ShowWindow(hChild, SW_SHOWMAXIMIZED);
						UpdateWindow(hChild);
					}
				}
				break;
				default:
				{
					if(LOWORD(wParam) >= ID_MDI_FIRSTCHILD)
						DefFrameProc(hwnd, g_hMDIClient, WM_COMMAND, wParam, lParam);
					else 
					{
						HWND hChild = (HWND)SendMessage(g_hMDIClient, WM_MDIGETACTIVE,0, 0);
						if(hChild)
							SendMessage(hChild, WM_COMMAND, wParam, lParam);
					}
				}
			}
			break;
		default:
			return DefFrameProc(hwnd, g_hMDIClient, msg, wParam, lParam);
	}
	
	return 0;
}

LRESULT CALLBACK MDIChildWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static float scale = 1.0f;
	static float dx = 0.0f, dy = 0.0f; 
	static float theta_x = 0.0f, theta_y = 0.0f, theta_z = 0.0f;
	static float xPos, xPrevPos = 0.0f, yPos, yPrevPos = 0.0f;
	static bool isLButtonDown = false;
	static enum AO {SCALING, TRANSLATION, ROTATION, NAH};
	static AO curOperation = NAH;

	if (g_bSuccessfulReading)
	{
		scale = 1.0f;
		dx = dy = 0.0f;
		theta_x = theta_y = theta_z = 0.0f;
		xPrevPos = yPrevPos = 0.0f;
		isLButtonDown = false;
		curOperation = NAH;
		g_bSuccessfulReading = false;
	}

	switch(msg)
	{
		case WM_CREATE:
		{
			// OpenGL related
			static const float m_ad[4] = {0.7f, 0.7f, 0.7f, 1.0f};
			RECT rcClient;

			GetClientRect(hwnd, &rcClient);

			EnableOpenGL(hwnd, &g_hdc, &g_hglrc);
			
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glShadeModel(GL_SMOOTH);

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			if (rcClient.right > rcClient.bottom)
				glOrtho(-1.0*((double)rcClient.right/(double)rcClient.bottom),
				1.0*((double)rcClient.right/(double)rcClient.bottom), -1.0, 1.0, -10.0, 10.0);
			else
				glOrtho(-1.0, 1.0, -1.0*((double)rcClient.bottom/(double)rcClient.right),
				1.0*((double)rcClient.bottom/(double)rcClient.right), -10.0, 10.0);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();

			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHT0);

			glEnable(GL_DEPTH_TEST);

			glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, m_ad);

			initTexturing();
		}
		break;
		case WM_DESTROY:
			DisableOpenGL(hwnd, g_hdc, g_hglrc);
			return DefMDIChildProc(hwnd, msg, wParam, lParam);
		case WM_MDIACTIVATE:
		{
			HMENU hMenu;
			UINT EnableFlag;

			hMenu = GetMenu(g_hMainWindow);
			if(hwnd == (HWND)lParam)
				EnableFlag = MF_ENABLED;
			else
				EnableFlag = MF_GRAYED;

			EnableMenuItem(hMenu, 1, MF_BYPOSITION | EnableFlag);

			DrawMenuBar(g_hMainWindow);
		}
		break;
		case WM_SIZE:
		{
			// resize OpenGL viewport
			int height = HIWORD(lParam);
			int width = LOWORD(lParam);

			glViewport(0, 0, width, height);

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			double baseSize = (double)max(g_fBoundaryX, max(g_fBoundaryY, g_fBoundaryZ));
			if (baseSize <= 0)
				baseSize = 1.0;
			if (width > height - 20)
				glOrtho(-baseSize * ((double)width / (double)height),
					baseSize * ((double)width / (double)height),
					-baseSize, baseSize, -10.0 * baseSize, 10.0 * baseSize);
			else
				glOrtho(-baseSize, baseSize, -baseSize * ((double)height / (double)width),
					baseSize * ((double)height / (double)width), -10.0 * baseSize, 10.0 * baseSize);

			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
		}
		return DefMDIChildProc(hwnd, msg, wParam, lParam);
		case WM_PAINT:
		{
			static const float light_pos[4] = {1.0f, 2.0f, 3.0f, 0.0f};

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glPushMatrix();
			glTranslatef(dx, dy, 0.0f);
			glRotatef(theta_x, 1.0f, 0.0f, 0.0f);
			glRotatef(theta_y, 0.0f, 1.0f, 0.0f);
			glRotatef(theta_z, 0.0f, 0.0f, 1.0f);
			glScalef(scale, scale, scale);
			glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
			drawMesh(g_3DModel);
			glPopMatrix();

			SwapBuffers(g_hdc);
		}
		break;
		case WM_LBUTTONDOWN:
			switch (curOperation)
			{
				case ROTATION:
				case TRANSLATION:
					xPrevPos = GET_X_LPARAM(lParam);
					yPrevPos = GET_Y_LPARAM(lParam);
					isLButtonDown = true;
					break;
			}
		break;
		case WM_LBUTTONUP:
			isLButtonDown = false;
			break;
		case WM_MOUSEMOVE:
			switch (curOperation)
			{
				case ROTATION:
					if (isLButtonDown)
					{
						xPos = GET_X_LPARAM(lParam);
						yPos = GET_Y_LPARAM(lParam);

						theta_x += yPos - yPrevPos;
						theta_y += xPos - xPrevPos;
						theta_z += -(atan2(yPos, xPos) - atan2(yPrevPos, xPrevPos)) * 180.0 / M_PI;

						xPrevPos = xPos;
						yPrevPos = yPos;

						UpdateWindow(hwnd);
					}
					break;
				case TRANSLATION:
					if (isLButtonDown)
					{
						RECT rcMDIClient;

						GetWindowRect(g_hMDIClient, &rcMDIClient);

						xPos = GET_X_LPARAM(lParam);
						yPos = GET_Y_LPARAM(lParam);

						float baseSize = max(g_fBoundaryX, max(g_fBoundaryY, g_fBoundaryZ));
						if (baseSize <= 0)
							baseSize = 1.0f;

						dx += 4.0f * baseSize * (xPos - xPrevPos) / (rcMDIClient.right - rcMDIClient.left); 
						dy += -2.0f * baseSize * (yPos - yPrevPos) / (rcMDIClient.bottom - rcMDIClient.top);

						xPrevPos = xPos;
						yPrevPos = yPos;

						UpdateWindow(hwnd);
					}
					break;
			}
			break;
		case WM_MOUSEWHEEL:
		{
			float delta = GET_WHEEL_DELTA_WPARAM(wParam) / 1200.0f;

			if (curOperation == SCALING)
				scale += delta;
		}
		break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_OBJ_ROTATION:
					curOperation = ROTATION;
					break;
				case ID_OBJ_TRANSLATION:
					curOperation = TRANSLATION;
					break;
				case ID_OBJ_SCALING:
					curOperation = SCALING;
					break;
				case ID_OBJ_SUBDIV:
					loopSubdiv(g_3DModel);
					curOperation = NAH;
					UpdateWindow(hwnd);
					break;
				case ID_OBJ_TEX:
					if (!g_bTexturingEnabled)
					{
						DoFileOpen(g_hMainWindow, TEXTURE_IMAGE);
						if (g_bTextureImageExist)
						{
							glEnable(GL_TEXTURE_2D);
							g_bTexturingEnabled = true;
						}
					}
					else
					{
						glDisable(GL_TEXTURE_2D);
						g_bTexturingEnabled = false;
					}
					curOperation = NAH;
					UpdateWindow(hwnd);
					break;
				case ID_LIGHTING:
					if (g_bLightingEnabled)
					{
						glDisable(GL_LIGHTING);
						g_bLightingEnabled = false;
					}
					else
					{
						glEnable(GL_LIGHTING);
						g_bLightingEnabled = true;
						if (g_bTexturingEnabled)
							glEnable(GL_TEXTURE_2D);
					}
					break;
			}
			break;
		default:
			return DefMDIChildProc(hwnd, msg, wParam, lParam);
	
	}
	return 0;
}

BOOL SetUpMDIChildWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wc;

	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.style		 = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = MDIChildWndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = g_szChildClassName;
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION);

	if(!RegisterClassEx(&wc))
	{
		MessageBox(0, L"Could Not Register Child Window", L"Error", MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}
	else
		return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG msg;

	InitCommonControls();

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Could not register window class!", L"Error", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	if (!SetUpMDIChildWindowClass(hInstance))
	{
		MessageBox(NULL, L"Could not register child window class!", L"Error", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	hwnd = CreateWindowEx(0, g_szClassName, L"Comprehensive Demonstration",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL);
	if (hwnd == NULL)
	{
		MessageBox(NULL, L"Could not create window!", L"Error", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	g_hMainWindow = hwnd;

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if (!TranslateMDISysAccel(g_hMDIClient, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}