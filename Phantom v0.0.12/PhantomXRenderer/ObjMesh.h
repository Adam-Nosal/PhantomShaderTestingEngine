#include <d3d11.h>
//#include <d3dx11.h>
#include <directxmath.h>
#include <string>
#include <fstream>
#include <vector>

#include "GeometricPrimitive.h"
#include "Model.h"

using namespace std;
using namespace DirectX;

namespace PhantomXRenderer{
	// struktura opisujaca pojedynczy wierzcholek
	struct VERTEX
	{
		
		XMFLOAT3   position;
		XMFLOAT3   normal;
		XMFLOAT2 texcoord;
	};


	// uzywamy pozycji wierzcholka, wektora normalnego i tekstury
#define VERTEX_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)


	// klasa pojedynczego modelu
	class ObjMesh
	{
	public:
		ObjMesh();
		~ObjMesh();


		void Create(ID3D11Device* _pDevice, string _strPath);


		vector< XMFLOAT3 >   Positions;
		vector< XMFLOAT2>   TexCoords;
		vector< XMFLOAT3 >   Normals;
		vector< VERTEX >      m_Vertices;
		vector< DWORD >         m_Indices;

		void Render();
		void Destroy();


	private:
		void LoadGeometryFromObj(string _strPath);


		ID3D11Device		 *m_pDevice;
		ModelMesh			 *m_pMesh;
		ID3D11Texture3D      *m_pTexture;


		VERTEX   *pVertices;
		WORD   *pIndices;


	};

}