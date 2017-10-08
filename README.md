# leven
Complete source for my experimental voxel engine which features in posts on my blog: http://ngildea.blogspot.co.uk/

This is a dump of the project in its current state rather than a real release of a project. I had for a long time planned to make some fixes and updates and do a "proper" release but that doesn't seem like it will ever happen so instead I've just uploaded all the code as it exists now.

## Build Instructions
A VS2013 solution and project file are supplied. These include hardcoded paths to the installation dirs of the project dependencies on my machine. The dependencies are:
  * Remotery (disabled by default but there are manually placed instrumentations)
  * Bullet Physics 2.83.6
  * Dear ImGui
  * Catch unit testing library (only required for the unit test configurations)
  * CUDA 
  * GLEW 1.13.0
  * GLM 0.9.3
  * SDL 2.0.3

It's highly likely that versions other than those specified will also work.

## Prebuilt
A prebuilt Win64 executable along with the dependencies can be downloaded here: https://github.com/nickgildea/leven/blob/master/prebuilt-win64.zip

## Features
The main point of the project was to investigate building a chunked terrain world using Dual Contouring. This more or less works, making use of my method for joining the chunk meshes described here: http://ngildea.blogspot.co.uk/2014/09/dual-contouring-chunked-terrain.html

The main chunk meshes are generated via OpenCL as described (more or less, there are some changes) here: http://ngildea.blogspot.co.uk/2015/06/dual-contouring-with-opencl.html

I've also added additional features like Bullet Physics integration so the user can walk around the terrains and spawn interactive objects.

## Problems
There are a few problems I never got around to fixing. 

The main problem is caused by the technologies used to implement the engine. Rendering is implemented via OpenGL and the GPGPU voxel component using OpenCL. On the face of things you might think these two techs work well together. In practise I found the oppposite to be true: the interop between the two APIs requires the thread performing the rendering to block while the GPU data is transferred from one API's ownership to the other. I found this to be unworkable and instead the following happens when a mesh is created:
  * the mesh data is generated on the GPU via OpenCL
  * the data is then downloaded into main memory
  * the mesh data is then reuploaded to the GPU via OpenGL
Obviously this a less than optimal solution. Additionally the mesh generation in OpenCL can occasionally starve the OpenGL operations of execution resources and hitches will occur. The solution to these issues would be to use the Vulkan API, I think.

Aside from that there are problems which I never fixed since it was just a personal project (I was the only user and so didn't particularly care..) like no overall memory management solution and things like fixed-size global arrays used instead which will occasionally become full, causing crashes.

## Contact

If you have an questions you can reach me by email: nick dot gildea at gmail or @ngildea85 on twitter.

