#pragma once

// ==== Forward declarations - full definition only needed in Allocator.cpp ====
typedef struct VmaAllocator_T*  VmaAllocator;
typedef struct VmaAllocation_T* VmaAllocation;

class Allocator
{
public:
	static void Initialize();
	static void Shutdown();

	static VmaAllocator& GetAllocator() { return s_VmaAllocator; }

private:
	static VmaAllocator s_VmaAllocator;
};
