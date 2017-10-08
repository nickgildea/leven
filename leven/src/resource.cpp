#include	"resource.h"

#include	"log.h"

#define		STBI_HEADER_FILE_ONLY
#include	"stb_image.c"
#undef max

#include	<string>

// ----------------------------------------------------------------------------

struct ImageData
{
	int				width, height, components;
	GLenum			internalFormat, pixelFormat;
	unsigned char*	data;
};

bool StbImageLoad(const std::string& filepath, ImageData* img)
{
	img->data = stbi_load(filepath.c_str(), &img->width, &img->height, &img->components, 0);
	if (img->data == NULL)
	{
		printf("Error: failed to load texture '%s'\n", filepath.c_str());
		return false;
	}

	img->internalFormat = GL_RGB8;
	img->pixelFormat = GL_RGB;
	if (img->components == 4)
	{
		img->internalFormat = GL_RGBA8;
		img->pixelFormat = GL_RGBA;
	}
	else
	{
		const bool flipColours = filepath.find(".tga") != std::string::npos;
		if (flipColours)
		{
			img->pixelFormat = GL_BGR;
		}
	}

	return true;
}

// ----------------------------------------------------------------------------

GLuint Resource_LoadTexture(const char* path)
{
	ImageData img;
	if (!StbImageLoad(path, &img))
	{
		return -1;
	}

	printf("Loaded texture '%s' (%dx%d)\n", path, img.width, img.height);

	const int NUM_MIPMAPS = 1;
	GLuint texture = 0;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexStorage2D(GL_TEXTURE_2D, NUM_MIPMAPS, img.internalFormat, img.width, img.height);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.width, img.height, img.pixelFormat, GL_UNSIGNED_BYTE, img.data);
#ifndef _DEBUG
	// TODO this crashes 9 out of 10 times in debug, no idea why
	// p.s. seems to be when a debugger is attached?
	glGenerateMipmap(GL_TEXTURE_2D);
#endif

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, NUM_MIPMAPS - 1);

	stbi_image_free(img.data);
	img.data = nullptr;

	if (GLenum err = glGetError())
	{
		glDeleteTextures(1, &texture);

		printf("Error %d (0x%08x): loading texture '%s': %s\n", err, err, path, gluGetString(err));
		return -1;
	}

	return texture;
}

// ----------------------------------------------------------------------------

GLuint Resource_LoadTextureArray(const std::vector<std::string>& textureFiles)
{
	GLuint texture = 0;

	GLsizei width = 1024;
	GLsizei height = 1024;
	GLsizei layerCount = textureFiles.size();
	GLsizei mipLevelCount = glm::log2(glm::max(width, height)) + 1;
	 
	glGenTextures(1,&texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
	//Allocate the storage.
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevelCount, GL_RGB8, width, height, layerCount);
	//Upload pixel data.
	//The first 0 refers to the mipmap level (level 0, since there's only 1)
	//The following 2 zeroes refers to the x and y offsets in case you only want to specify a subrectangle.
	//The final 0 refers to the layer index offset (we start from index 0 and have 2 levels).
	//Altogether you can specify a 3D box subset of the overall texture, but only one mip level at a time.
//	glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, width, height, layerCount, GL_RGBA, GL_UNSIGNED_BYTE, texels);

	for (int i = 0; i < textureFiles.size(); i++)
	{
		const auto& path = textureFiles[i];

		ImageData img;
		if (!StbImageLoad(path, &img))
		{
			printf("Error: could not load image '%s'\n", path.c_str());
			return -1;
		}

		LVN_ASSERT(img.width == 1024);
		LVN_ASSERT(img.height == 1024);
		LVN_ASSERT(img.internalFormat == GL_RGB8);

		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, img.width, img.height, 1, img.pixelFormat, GL_UNSIGNED_BYTE, img.data);
	}

	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	 
	//Always set reasonable texture parameters
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, mipLevelCount - 1);

	return texture;
}

// ----------------------------------------------------------------------------
