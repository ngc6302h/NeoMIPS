#pragma once
#include "constraints.hpp"
#include "argumentprocessor.hpp"
namespace NeoMIPS
{
	class ExecutionContext
	{
		const argmap_t& m_options;

	public:
		inline ExecutionContext(const argmap_t& options) : m_options(options) {}
		void Run();

		//Shorthands

		inline std::string GetSourcePath()
		{
			return static_cast<Option<std::string>*>(m_options.at(std::string("sourcefile")).get())->GetValue();
		}
	};
}
