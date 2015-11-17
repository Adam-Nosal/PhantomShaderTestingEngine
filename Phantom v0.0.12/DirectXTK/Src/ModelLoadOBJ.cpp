//--------------------------------------------------------------------------------------
// File: ModelLoadOBJ.cpp
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Model.h"

#include "Effects.h"
#include "VertexTypes.h"

#include "DirectXHelpers.h"
#include "PlatformHelpers.h"
#include "BinaryReader.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

using namespace DirectX;
using namespace std;
using Microsoft::WRL::ComPtr;


//--------------------------------------------------------------------------------------
// The OBJ file format was introduced in the Windows 8.0 ResourceLoading sample. It's
// a simple binary file containing a 16-bit index buffer and a fixed-format vertex buffer.
//
// The meshconvert sample tool for DirectXMesh can produce this file type
// http://go.microsoft.com/fwlink/?LinkID=324981
//--------------------------------------------------------------------------------------

namespace OBJ
{
	static XMFLOAT3 cross(XMFLOAT3 a, XMFLOAT3 b)
	{
		return XMFLOAT3((a.y*b.z) - (a.z*b.y), (a.z*b.x) - (a.x*b.z), (a.x*b.y) - (a.y*b.x));
	}

	static XMFLOAT3 AddFloat (XMFLOAT3 a, XMFLOAT3 b)
	{
		return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
	}

	static  XMFLOAT3 SubFloat(XMFLOAT3 a, XMFLOAT3 b)
	{
		return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
	}

	static  XMFLOAT3 DivFloat(XMFLOAT3 a, XMFLOAT3 b)
	{
		return XMFLOAT3(a.x / b.x, a.y / b.y, a.z / b.z);
	}

}; // namespace

static_assert(sizeof(VertexPositionNormalTexture) == 32, "OBJ vertex size mismatch");


//--------------------------------------------------------------------------------------
// Shared VB input element description
static INIT_ONCE g_InitOnce = INIT_ONCE_STATIC_INIT;
static std::shared_ptr<std::vector<D3D11_INPUT_ELEMENT_DESC>> g_vbdecl;

static BOOL CALLBACK InitializeDecl(PINIT_ONCE initOnce, PVOID Parameter, PVOID *lpContext)
{
    UNREFERENCED_PARAMETER(initOnce);
    UNREFERENCED_PARAMETER(Parameter);
    UNREFERENCED_PARAMETER(lpContext);

    g_vbdecl = std::make_shared<std::vector<D3D11_INPUT_ELEMENT_DESC>>(VertexPositionNormalTexture::InputElements,
                   VertexPositionNormalTexture::InputElements + VertexPositionNormalTexture::InputElementCount);

    return TRUE;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
std::unique_ptr<Model> DirectX::Model::CreateFromOBJ(ID3D11Device* d3dDevice, const wchar_t* szFileName,
                                                     std::shared_ptr<IEffect> ieffect, bool ccw, bool pmalpha)
{

	const wchar_t* InFileName = Model::ConvertObjToVbo(szFileName);

	size_t dataSize = 0;
	std::unique_ptr<uint8_t[]> data;
	HRESULT hr = BinaryReader::ReadEntireFile(InFileName, data, &dataSize);
	if (FAILED(hr))
	{
		DebugTrace("CreateFromOBJ failed (%08X) loading '%ls'\n", hr, szFileName);
		throw std::exception("CreateFromOBJ");
	}

	auto model = CreateFromVBO(d3dDevice, data.get(), dataSize, ieffect, ccw, pmalpha);

    model->name = szFileName;

    return model;
}

struct BasicVertex
{
	XMFLOAT3 pos;  // position
	XMFLOAT3 norm; // surface normal vector
	XMFLOAT2 tex;  // texture coordinate
};

struct IndexTriplet
{
	unsigned short pos;
	unsigned short norm;
	unsigned short tex;
};




bool operator <(const IndexTriplet& lhs, const IndexTriplet& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(IndexTriplet)) < 0;
}

const wchar_t* DirectX::Model::ConvertObjToVbo(const wchar_t* szObjFileName){


	ifstream objFile(szObjFileName, ifstream::in);
	if (!objFile.is_open())
	{
		cout << "error: could not open file \"" << szObjFileName << "\" for read" << endl;
		//return L"-1";
	}

	vector<XMFLOAT3> positions;
	vector<XMFLOAT3> normals;
	vector<XMFLOAT2> texcoords;
	vector<vector<IndexTriplet>> faces;

	unsigned int lineNum = 1;

	while (objFile.good())
	{
		string line;
		getline(objFile, line);
		istringstream lineStream(line);

		// Parse the line if not a comment
		if (lineStream.peek() != '#')
		{
			string tag;
			lineStream >> tag;

			// Warn on unsupported tags
			if (
				tag.compare("mtllib") == 0 ||
				tag.compare("o") == 0 ||
				tag.compare("g") == 0 ||
				tag.compare("usemtl") == 0 ||
				tag.compare("s") == 0
				)
			{
				cout << szObjFileName << "(" << lineNum << "): warning: BasicMesh VBO format does not support tag \"" << tag << "\"" << endl;
			}

			// Parse the vertex position
			else if (tag.compare("v") == 0)
			{
				XMFLOAT3 pos;
				lineStream >> pos.x >> pos.y >> pos.z;
				positions.push_back(pos);
			}

			// Parse the vertex normal
			else if (tag.compare("vn") == 0)
			{
				XMFLOAT3 norm;
				lineStream >> norm.x >> norm.y >> norm.z;
				normals.push_back(norm);
			}

			// Parse the vertex texture coordinate
			else if (tag.compare("vt") == 0)
			{
				XMFLOAT2 tex;
				lineStream >> tex.x >> tex.y;
				texcoords.push_back(tex);
			}

			// Parse the face
			else if (tag.compare("f") == 0)
			{
				vector<IndexTriplet> face;
				while (lineStream.good())
				{
					string tripletString;
					lineStream >> tripletString;
					if (tripletString.size() > 0)
					{
						istringstream tripletStream(tripletString);

						IndexTriplet triplet;
						triplet.pos = 0;
						triplet.norm = 0;
						triplet.tex = 0;

						// Parse the face triplet.  Valid formats include:
						//  [pos] ...
						//  [pos]/[tex] ...
						//  [pos]/[tex]/[norm] ...
						//  [pos]// [norm] ...

						tripletStream >> triplet.pos;
						if (tripletStream.get() == '/')
						{
							if (tripletStream.peek() != '/')
							{
								tripletStream >> triplet.tex;
							}
							if (tripletStream.get() == '/')
							{
								tripletStream >> triplet.norm;
							}
						}
						face.push_back(triplet);
					}
				}
				faces.push_back(face);
			}

			// Fail on unknown tag
			else if (tag.size() > 0)
			{
				cout << szObjFileName << "(" << lineNum << "): error: unknown tag \"" << tag << "\"" << endl;
			}
		}
		lineNum++;
	}

	objFile.close();

	if (positions.size() == 0 || faces.size() == 0)
	{
		cout << "error: obj file \"" << szObjFileName << "\" contains no geometry" << endl;
		return L"-1";
	}

	// Validate mesh data

	for (auto face = faces.begin(); face != faces.end(); face++)
	{
		if (face->size() < 3)
		{
			cout << "error: face size " << face->size() << " invalid" << endl;
			return L"-1";
		}
		for (auto triplet = face->begin(); triplet != face->end(); triplet++)
		{
			if (triplet->pos > positions.size() || triplet->pos < 1)
			{
				cout << "error: position index " << triplet->pos << " out of range" << endl;
				return L"-1";
			}
			if (triplet->norm > normals.size() || triplet->norm < 0)
			{
				cout << "error: normal index " << triplet->norm << " out of range" << endl;
				return L"-1";
			}
			if (triplet->tex > texcoords.size() || triplet->tex < 0)
			{
				cout << "error: texcoord index " << triplet->tex << " out of range" << endl;
				return L"-1";
			}
		}
	}

	// Get the bounding box and center of the mesh

	XMFLOAT3 boxMin = positions[faces[0][0].pos - 1];
	XMFLOAT3 boxMax = boxMin;
	for (auto face = faces.begin(); face != faces.end(); face++)
	{
		for (auto triplet = face->begin(); triplet != face->end(); triplet++)
		{
			XMFLOAT3 pos = positions[triplet->pos - 1];
			boxMin.x = min(boxMin.x, pos.x);
			boxMin.y = min(boxMin.y, pos.y);
			boxMin.z = min(boxMin.z, pos.z);
			boxMax.x = max(boxMax.x, pos.x);
			boxMax.y = max(boxMax.y, pos.y);
			boxMax.z = max(boxMax.z, pos.z);
		}
	}
	XMFLOAT3 boxCenter = OBJ::DivFloat(OBJ::AddFloat(boxMax, boxMin),XMFLOAT3(2.0f, 2.0f, 2.0f));

	
	// Generate missing normals using faceted technique

	for (auto face = faces.begin(); face != faces.end(); face++)
	{
		XMFLOAT3 normal(0, 0, 0);
		bool normalGenerated = false;
		for (auto triplet = face->begin(); triplet != face->end(); triplet++)
		{
			if (!normalGenerated && triplet->norm == 0)
			{
				for (auto triplet = face->begin(); triplet != face->end(); triplet++)
				{
					XMFLOAT3 posThis = positions[triplet->pos - 1];
					XMFLOAT3 posPrev = positions[(triplet == face->begin() ? (face->end() - 1)->pos : (triplet - 1)->pos) - 1];
					XMFLOAT3 posNext = positions[(triplet == face->end() - 1 ? (face->begin())->pos : (triplet + 1)->pos) - 1];
					normal = OBJ::AddFloat(normal, OBJ::cross(OBJ::SubFloat(posNext, posThis), OBJ::SubFloat(posPrev , posThis)));
					triplet->norm = normals.size() + 1;
				}
				normals.push_back(normal);
				normalGenerated = true;
			}
		}
	}

	// Fill in missing texture coordinates with (0, 0)

	bool missingTexcoordCreated = false;
	unsigned int missingTexcoordIndex = 0;
	for (auto face = faces.begin(); face != faces.end(); face++)
	{
		for (auto triplet = face->begin(); triplet != face->end(); triplet++)
		{
			if (triplet->tex == 0)
			{
				if (!missingTexcoordCreated)
				{
					texcoords.push_back(XMFLOAT2(0.0f, 0.0f));
					missingTexcoordIndex = texcoords.size();
					missingTexcoordCreated = true;
				}
				triplet->tex = missingTexcoordIndex;
			}
		}
	}

	// Generate unique vertices and convert counter-clockwise faces to clockwise triangles

	vector<BasicVertex> vertices;
	vector<unsigned short> indices;
	map<IndexTriplet, unsigned short> tripletIndices;
	for (auto face = faces.begin(); face != faces.end(); face++)
	{
		for (auto triplet = face->begin(); triplet != face->end(); triplet++)
		{
			if (tripletIndices.find(*triplet) == tripletIndices.end())
			{
				tripletIndices[*triplet] = static_cast<unsigned short>(vertices.size());
				BasicVertex vertex;
				vertex.pos = positions[triplet->pos - 1];
				vertex.norm = normals[triplet->norm - 1];
				vertex.tex = texcoords[triplet->tex - 1];
				vertices.push_back(vertex);
			}
			if (triplet >= face->begin() + 2)
			{
				indices.push_back(tripletIndices[*face->begin()]);
				indices.push_back(tripletIndices[*triplet]);
				indices.push_back(tripletIndices[*(triplet - 1)]);
			}
		}
	}

	// Dump vertex and index data to the output VBO file


	ofstream vboFile("kutas.vbo", ofstream::out | ofstream::binary);
	if (!vboFile.is_open())
	{
		cout << "error: could not open file \"" <<"kutas.vbo" << "\" for write" << endl;
		return L"-1";
	}

	unsigned int numVertices = vertices.size();
	unsigned int numIndices = indices.size();
	vboFile.write(reinterpret_cast<char*>(&numVertices), sizeof(unsigned int));
	vboFile.write(reinterpret_cast<char*>(&numIndices), sizeof(unsigned int));
	vboFile.write(reinterpret_cast<char*>(&vertices[0]), sizeof(BasicVertex) * vertices.size());
	vboFile.write(reinterpret_cast<char*>(&indices[0]), sizeof(unsigned short) * indices.size());

	vboFile.close();

	return L"kutas.vbo";
}