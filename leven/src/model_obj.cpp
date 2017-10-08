#include	"model_obj.h"

#include	"file_utils.h"
#include	"log.h"
#include	<glm/glm.hpp>
#include	<functional>

// ----------------------------------------------------------------------------

bool IsSpace(const char c) { return isspace(c) != 0; }

std::vector<std::string> Tokenise(const std::string& str, const std::function<bool(const char)>& pred = IsSpace)
{
	std::vector<std::string> tokens;
	std::string s;

	for (size_t i = 0; i < str.size(); i++)
	{
		if (pred(str[i]))
		{
			if (s != "")
			{
				tokens.push_back(s);
				s = "";
			}
		}
		else
		{
			s += str[i];
		}
	}

	if (s != "")
	{
		tokens.push_back(s);
	}

	return tokens;
}

// ----------------------------------------------------------------------------

ObjModel loadModel(const std::string& name,
	const std::vector<std::string>& vertexPositionData,
	const std::vector<std::string>& vertexNormalData,
	const std::vector<std::string>& faceData)
{
	std::vector<glm::vec4> positions(vertexPositionData.size());
	std::vector<glm::vec4> normals(vertexNormalData.size());

	ObjModel model;

	for (size_t i = 0; i < vertexPositionData.size(); i++)
	{
		const std::string& v = vertexPositionData[i];
		glm::vec4 position;
		sscanf(v.c_str(), "v %f %f %f\n", &position.x, &position.y, &position.z);
		positions[i] = position;

		const std::string& n = vertexNormalData[i];
		glm::vec4 normal;
		sscanf(n.c_str(), "vn %f %f %f\n", &normal.x, &normal.y, &normal.z);
		normals[i] = normal;

		// TODO texture co-ords
	}

	// the face data is made up by referencing combinations of the position and normal data
	// e.g. position x and tex coord y and normal z, for each of the face's vertices: "f x/y/z a/b/c/ d/e/f" 
	const auto isSlash = [](const char c) -> bool { return c == '/'; };
	for (const std::string& f: faceData)
	{
		const auto tokens = Tokenise(f);
		if (tokens.size() != 4)
		{
			printf("Error: unsupported face specification '%s'\n", f.c_str());
			return model;
		}

		// parse the data items (TODO texture coords)
		for (int i = 1; i < 4; i++)
		{
			const auto indices = Tokenise(tokens[i], isSlash);
			switch (indices.size())
			{
			case 1:

			}
		}
	}

	// certain (xyz, st) pairs may be specified multiple times, we use
	// this indexMap to map the (xyz, st) pair to the index in m->vertexes_
	std::map<FaceIndex, size_t> indexMap;
	for (size_t i = 0; i < faceData.size(); i++)
	{
		const std::string& f = faceData[i];

		char type[4] = { 0 };
		char buffer[128];
		char* str = buffer;
		strncpy(str, f.c_str(), 128);

		if (sscanf(str, "%s", type) != 1)
		{
			LogPrintf("Error: bad face data '%s'\n", str);
			continue;
		}
		str = strstr(str, " ") + 1;

		std::vector<FaceIndex> faceIndexes;
		while (true)
		{
			unsigned short v = 0, t = 0;
			if (sscanf(str, "%hu/%hu", &v, &t) != 2)
			{
				break;
			}

			FaceIndex face(v, t);
			if (indexMap.find(face) == indexMap.end())
			{
				indexMap[face] = m->vertexes_.size();
				const Position& p = xyz[face.first - 1];
				const TexPosition& t = st[face.second - 1];
				m->vertexes_.push_back(GeometryVertex(p.x, p.y, p.z, t.u, t.v));
			}

			faceIndexes.push_back(face);

			str = strstr(str, " ");
			if (str == NULL)
			{
				break;
			}

			str += 1;
		}

		bool flipWinding = false;
		for (size_t i = 3; i <= faceIndexes.size(); i++)
		{
			Index f0 = indexMap[faceIndexes[0]];
			Index f1 = indexMap[faceIndexes[i-1]];
			Index f2 = indexMap[faceIndexes[i-2]];

			if (!flipWinding)
			{
				m->indexes_.push_back(f0);
				m->indexes_.push_back(f2);
				m->indexes_.push_back(f1);
			}
			else
			{
				m->indexes_.push_back(f1);
				m->indexes_.push_back(f0);
				m->indexes_.push_back(f2);
			}

			flipWinding = !flipWinding;
		}
	}

	std::vector<glm::vec3> normals;
	std::vector<int> normalsUsed;
	normalsUsed.resize(m->vertexes_.size(), 0);
	normals.resize(m->vertexes_.size(), glm::vec3(0));

	for (size_t i = 0; i < m->indexes_.size() - 2; i += 3)
	{
		const Index ia = m->indexes_[i + 0];
		const Index ib = m->indexes_[i + 1];
		const Index ic = m->indexes_[i + 2];

		if (ic >= m->vertexes_.size())
		{
			LogPrintf("ic >= vertexes.size(): %d >= %d\n", ic, m->vertexes_.size());
			continue;
		}

		const glm::vec3 a = Vertex_GetPosition(m->vertexes_[ia]);
		const glm::vec3 b = Vertex_GetPosition(m->vertexes_[ib]);
		const glm::vec3 c = Vertex_GetPosition(m->vertexes_[ic]);
		const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));

		for (size_t j = 0; j < 3; j++)
		{
			Index currentV = m->indexes_[i + j];
			normalsUsed[currentV]++;
			if (normalsUsed[currentV] == 1)
			{
				normals[currentV] = normal;
			}
			else
			{
				const glm::vec3& n = normals[currentV];
				const int used = normalsUsed[currentV];

				normals[currentV] = (n * (1.f - (1.f / used))) + (normal * (1.f / used));
				normals[currentV] = glm::normalize(normals[currentV]);
			}
		}
	}

	for (size_t i = 0; i < m->vertexes_.size(); i++)
	{
		GeometryVertex& v = m->vertexes_[i];
		Vertex_SetNormal(v, normals[i]);
	}

	return m;
}

ObjModel LoadModelsFromFile(const std::string& path)
{
	std::string data;
	if (!LoadTextFile(path, data))
	{
		LogPrintf("WavefrontModel: can't load file '%s'\n", path.c_str());
		return false;
	}

	enum State
	{
		STATE_NONE,
		STATE_VERTEX,
		STATE_FACE
	};

	std::string name; 
	std::vector<std::string> vertexData;
	std::vector<std::string> textureData;
	std::vector<std::string> normalData;
	std::vector<std::string> faceData;

	std::stringstream stream(data);
	std::string line;
	while (!stream.eof())
	{
		getline(stream, line);

		if (line.find("g ") == 0 || line.find("o ") == 0)
		{
			LVN_ALWAYS_ASSERT("Only 1 model per file supported", name == "");
			name = line.substr(2);
		}
		else if (line.find("v ") == 0)
		{
			vertexData.push_back(line);
		}
		else if (line.find("vn ") == 0)
		{
			normalData.push_back(line);
		}
		else if (line.find("vt ") == 0)
		{
			textureData.push_back(line);
		}
		else if (line.find("f ") == 0)
		{
			faceData.push_back(line);
		}
	}

	if (name != "" && !faceData.empty())
	{
		// TODO assumes only 1 model per file
		return loadModel(name, vertexData, normalData, faceData);
	}

	return true;
}

