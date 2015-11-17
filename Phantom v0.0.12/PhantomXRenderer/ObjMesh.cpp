#include "ObjMesh.h"


namespace PhantomXRenderer{
	// standardowy konstruktor, zeruje i przygotowuje do pracy wszystkie obiekty
	ObjMesh::ObjMesh()
	{
		m_pDevice = NULL;
		m_pMesh = NULL;
		m_pTexture = NULL;


		pVertices = NULL;
		pIndices = NULL;
	}


	// standardowy destruktor, wywoluje metode destroy
	ObjMesh::~ObjMesh()
	{
		Destroy();
	}


	// metoda destroy sprzata po klasie, zwraca wszystkie zasoby itd.
	void ObjMesh::Destroy()
	{
		m_pDevice = NULL;


		Positions.clear();
		Normals.clear();
		TexCoords.clear();
		m_Vertices.clear();
		m_Indices.clear();


		pVertices = NULL;
		pIndices = NULL;


		if (m_pTexture)
			m_pTexture->Release();


		if (m_pMesh)
			free(m_pMesh);
			//m_pMesh->Release();
	}


	// tworzy nowy mesh
	// parametry: _pDevice - wskaznik na obiekt renderujacy, _strPath - sciezka do modelu *.obj
	void ObjMesh::Create(ID3D11Device *_pDevice, string _strPath)
	{
		// kopiujemy sobie wskaznik zeby potem nie musiec caly czas go przesylac przez
		// argumenty
		m_pDevice = _pDevice;


		// ladujemy geometrie z podanej sciezki
		LoadGeometryFromObj(_strPath);


		// tu mozna umiescic ladowanie plikow *.mtl zawierajacych info o materialach
	}


	// laduje cala geometrie mesh'a
	void ObjMesh::LoadGeometryFromObj(string _strPath)
	{
		char strCommand[256] = { 0 };         // tu beda przechowywane kolejne fragmenty linijek pliku
		string texturePath;               // sciezka do tekstury [nieobowiazkowo]
		ifstream InFile;               // plik *.obj

		// otwieramy naszego mesh'a
		InFile.open(_strPath.c_str());


		// robimy nieskonczana petle - przerwie sie ona po dojsciu do konca pliku
		for (;;)
		{
			// w kazdym przebiegu wczytujemy znaki z kolejnych linii pliku (do napotkania bialego znaku)
			InFile >> strCommand;


			// jesli doszlismy do konca pliku - przerywamy petle
			if (!InFile)
				break;


			// tutaj zaleznie od znaku rozpoczynajacego linie podejmujemy okreslone dzialania
			if (strcmp(strCommand, "#") == 0)
			{
				// linia zaczyna sie od # - jest zignorowana (to chyba komentarze, nie pamietam)
			}
			else if (strcmp(strCommand, "v") == 0)
			{
				// linia zaczyna sie od litery v - deklaracja wspolrzednych wierzcholka
				// wczytujemy wszystkie trzy koordynaty, po czym wysylamy je do kontenera 
				// przechowujacego pozycje wierzcholkow
				float x, y, z;
				InFile >> x >> y >> z;
				Positions.push_back(XMFLOAT3(x, y, z));
			}
			else if (strcmp(strCommand, "vt") == 0)
			{
				// linia zaczyna sie od liter vt - deklaracja wspolrzednych tekstur
				// wczytujemy wspolrzedne u i v tekstury (trzeci koordynat jest ingorowany)
				// po czym przesylamy to do kontenera ze wspolrzednymi tekstur
				float u, v;
				InFile >> u >> v;
				TexCoords.push_back(XMFLOAT2(u, v));
			}
			else if (strcmp(strCommand, "vn") == 0)
			{
				// linia zaczyna sie od liter vn - deklaracja wspolrzednych normalnych
				// to co wyzej, z ta roznica ze wczytujemy x, y i z wektorow normalnych
				float x, y, z;
				InFile >> x >> y >> z;
				Normals.push_back(XMFLOAT3(x, y, z));
			}
			else if (strcmp(strCommand, "f") == 0)
			{
				// linijka zaczyna sie od f - deklaracja face'a

				// w kazdej linijce znajduja sie informacje nt. "budowy" trzech wierzcholkow,
				// czyli ktorych wspolrzednych wierzcholka, tekstur i normalnej ma uzywac dany wierzcholek
				// wczytujemy te trzy dane, wysylamy do kontenera wierzcholkow
				// powtarzamy to wszystko trzy razy - zeby wczytac wszystkie trzy wierzcholki


				int iPosition, iTexCoord, iNormal;      // dane pojedynczego wierzcholka
				VERTEX vertex;                     // pojedynczy wierzcholek


				// wczytujemy trzy wierzcholki
				for (int iFace = 0; iFace < 3; iFace++)
				{
					ZeroMemory(&vertex, sizeof(VERTEX));


					// wczytujemy pozycje
					InFile >> iPosition;
					vertex.position = Positions[iPosition - 1];


					if (InFile.peek() == '/')
					{
						InFile.ignore();


						if (InFile.peek() != '/')
						{

							// wczytujemy texcoord'y - opcjonalne
							InFile >> iTexCoord;
							vertex.texcoord = TexCoords[iTexCoord - 1];
						}


						if (InFile.peek() == '/')
						{
							InFile.ignore();


							// wczytujemy wspolrzedne wektora normalnego
							InFile >> iNormal;
							vertex.normal = Normals[iNormal - 1];
						}
					}


					// wysylamy wierzcholek do kontenera
					m_Vertices.push_back(vertex);


					// ustawiamy indeks wierzcholka - leca kolejno
					m_Indices.push_back(m_Vertices.size() - 1);
				}
			}
			else if (strcmp(strCommand, "usemap") == 0)
			{
				// linijka zaczyna sie od usemap - model uzywa konkretnej tekstury


				// wczytujemy sciezke do tekstury
				InFile >> texturePath;
			}
			else
			{
				// niezaimplementowane komunikaty
			}
		}


		// wczytywanie zakonczone, zamykamy plik
		InFile.close();


		// tworzymy mesh'a z wierzcholkow
		// arguemnty kolejno: ilosc powierzchni, ilosc wierzcholkow, opcje tworzenia mesh'a,
		// deklaracja FVF, urzadzenie renderujace, adres interfejsu przechowujacego nasz model

		//if (D3DXCreateMeshFVF(m_Indices.size() / 3, m_Vertices.size(), D3DXMESH_MANAGED,
		//	VERTEX_FVF, m_pDevice, &m_pMesh) == D3D_OK)
		//{
		//	// operacja sie powiodla
		//}
		//else
		//{
		//	// operacja sie nie powiodla - mozna sobie dac jakis komunikat albo log
		//}




		//// kopiujemy wierzcholki z naszych kontenerow do bufora wierzcholkow mesh'a
		//m_pMesh -> LockVertexBuffer(0, (void**)&pVertices);
		//{
		//	for (int i = 0; i < m_Vertices.size(); i++)
		//	{
		//		pVertices[i] = m_Vertices[i];
		//	}
		//}
		//m_pMesh->UnlockVertexBuffer();




		//// robimy to samo z indeksami
		//m_pMesh->LockIndexBuffer(0, (void**)&pIndices);
		//{
		//	for (int i = 0; i < m_Indices.size(); i++)
		//	{
		//		pIndices[i] = m_Indices[i];
		//	}
		//}
		//m_pMesh->UnlockIndexBuffer();


		D3D11_TEXTURE3D_DESC descDepth;
		ZeroMemory(&descDepth, sizeof(descDepth));
		descDepth.Width = 100; //DOTO: hack
		descDepth.Height = 100;
		descDepth.MipLevels = 1;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;

		m_pDevice->CreateTexture3D(&descDepth, nullptr, &m_pTexture);
		// tworzymy teksture dla modelu
	}


	// funkcja renderujaca mesh'a
	void ObjMesh::Render()
	{
		//SetTexture(0, m_pTexture);      // ustawianie tekstury
		//m_pMesh->DrawSubset(0);                  // rysowanie pojedynczego (jedynego) subset'a
	}
}