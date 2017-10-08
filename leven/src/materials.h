#ifndef		HAS_MATERIALS_H_BEEN_INCLUDED
#define		HAS_MATERIALS_H_BEEN_INCLUDED

#include	"resource.h"

#include	<stdint.h>
#include	<string>
#include	<vector>
#include	<glm/glm.hpp>

class MaterialSet
{
public:

	// ----------------------------------------------------------------------------

	MaterialSet()
		: textureArrayID_(0)
	{
	}

	// ----------------------------------------------------------------------------

	void addMaterial(const std::string& path)
	{
		const float id = (float)texturePaths_.size();
		texturePaths_.push_back(path);
		materials_.push_back(glm::vec3(id));
	}

	// ----------------------------------------------------------------------------

	void addMaterial(const std::string& pathX, const std::string& pathY, const std::string& pathZ)
	{
		const float idX = (float)texturePaths_.size();
		const float idY = idX + 1.f;
		const float idZ = idX + 2.f;

		texturePaths_.push_back(pathX);
		texturePaths_.push_back(pathY);
		texturePaths_.push_back(pathZ);

		materials_.push_back(glm::vec3(idX, idY, idZ));
	}

	// ----------------------------------------------------------------------------

	uint32_t bakeTextureArray()
	{
		if (texturePaths_.empty())
		{
			return 0;
		}

		if (textureArrayID_ == 0)
		{
			textureArrayID_ = Resource_LoadTextureArray(texturePaths_);
		}

		return textureArrayID_;
	}

	// ----------------------------------------------------------------------------

	std::vector<float> exportMaterialTextures() const
	{
		std::vector<float> textureIDs(materials_.size() * 3);
		for (size_t i = 0; i < materials_.size(); i++)
		{
			textureIDs[(i * 3) + 0] = materials_[i].x;
			textureIDs[(i * 3) + 1] = materials_[i].y;
			textureIDs[(i * 3) + 2] = materials_[i].z;
		}

		return textureIDs;
	}

	// ----------------------------------------------------------------------------

	unsigned int size() const
	{
		return materials_.size();
	}

	// ----------------------------------------------------------------------------

private:

	std::vector<std::string>	texturePaths_;
	std::vector<glm::vec3>		materials_;
	uint32_t					textureArrayID_;
};


#endif	//	HAS_MATERIALS_H_BEEN_INCLUDED
