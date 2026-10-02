#include "Editor/Editor.hpp"

int main()
{
	ApplicationSpecification specification;
	specification.Name = "Blackfrost";

	EditorApplication application(specification);
	application.Run();

	return 0;
}
