#pragma once
namespace NeoMIPS
{
	class OptionBase
	{

	};

	template<typename T>
	class Option : public OptionBase
	{
	private:
		T m_value;

	public:
		constexpr Option() {};
		constexpr Option(const T& v) : m_value(v) {}
		constexpr Option(T&& v) : m_value(v) {}
		constexpr void SetValue(const T& v) { m_value = v; }
		constexpr void SetValue(T&& v) { m_value = v; }
		constexpr T& GetValue() { return m_value; }
	};
}