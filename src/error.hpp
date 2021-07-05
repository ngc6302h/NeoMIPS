#pragma once
#include <string>
#include <stdexcept>

namespace NeoMIPS
{
	namespace Error
	{
		class NeoMIPSException
		{
		public:
			const std::string m_what;
			const std::string m_where;
			const std::string m_why;

			NeoMIPSException(const std::string what, const std::string& where, const std::string& why) : m_what(what), m_where(where), m_why(why) {}
			std::string_view to_string()
			{
				return std::move("line: " + m_where + ", what: " + m_what + ", why: " + m_why);
			}
		};

		class IntegerParsingException : public NeoMIPSException
		{
		public:

			IntegerParsingException(const std::string& where, const std::string& why) : NeoMIPSException("IntegerParsingException", where, why) {}
		};

		class FileNotFoundException : public NeoMIPSException
		{
		public:

			FileNotFoundException(const std::string& where, const std::string& why) : NeoMIPSException("FileNotFoundException", where, why) {}
		};

		class FileReadException : public NeoMIPSException
		{
		public:

			FileReadException(const std::string& where, const std::string& why) : NeoMIPSException("FileReadException", where, why) {}
		};

		class EncodingTranslationException : public NeoMIPSException
		{
		public:

			EncodingTranslationException(const std::string& where, const std::string& why) : NeoMIPSException("EncodingTranslationException", where, why) {}
		};

		class InvalidSyntaxException : public NeoMIPSException
		{
		public:

			InvalidSyntaxException(const std::string& where, const std::string& why) : NeoMIPSException("InvalidSyntaxException", where, why) {}
		};

		class InvalidInstructionException : public NeoMIPSException
		{
		public:

			InvalidInstructionException(const std::string& where, const std::string& why) : NeoMIPSException("InvalidInstructionException", where, why) {}
		};

		class InvalidDirectiveException : public NeoMIPSException
		{
		public:

			InvalidDirectiveException(const std::string& where, const std::string& why) : NeoMIPSException("InvalidSyntaxException", where, why) {}
		};

		class InvalidEscapeSequenceException : public NeoMIPSException
		{
		public:

			InvalidEscapeSequenceException(const std::string& where, const std::string& why) : NeoMIPSException("InvalidEscapeSequenceException", where, why) {}
		};
		
	}
}