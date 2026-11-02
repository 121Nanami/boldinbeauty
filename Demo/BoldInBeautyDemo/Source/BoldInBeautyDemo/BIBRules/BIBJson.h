// Boldinbeauty demo — a small JSON value, writer and parser. Engine-independent (no Unreal includes), exception-free and
// RTTI-free so it compiles in the rules tests. Used by the save system (BIBSave.h) and by the account client for the
// server's replies. The parser is strict about syntax, limits nesting depth and never reads outside the text it is given,
// because save files and server replies are not trusted input.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace BIB
{
	namespace Json
	{
		enum class EType : int
		{
			Null = 0,
			Bool,
			Number,
			String,
			Array,
			Object
		};

		class FValue
		{
		public:
			FValue() = default;

			static FValue MakeBool(bool bValue);
			static FValue MakeNumber(double Value);
			static FValue MakeString(const std::string& Value);
			static FValue MakeArray();
			static FValue MakeObject();

			EType Type() const { return Kind; }
			bool IsNull() const { return Kind == EType::Null; }
			bool IsObject() const { return Kind == EType::Object; }
			bool IsArray() const { return Kind == EType::Array; }
			bool IsString() const { return Kind == EType::String; }
			bool IsNumber() const { return Kind == EType::Number; }
			bool IsBool() const { return Kind == EType::Bool; }

			// Object: Set replaces an existing key. Find returns nullptr when missing or when this is not an object.
			FValue& Set(const std::string& Key, const FValue& Value);
			const FValue* Find(const std::string& Key) const;

			// Array.
			void Push(const FValue& Value);
			size_t Size() const;
			const FValue& At(size_t Index) const; // out of range -> a shared null value

			// Typed reads with a fallback when the value is missing or of another type.
			bool AsBool(bool bFallback = false) const;
			double AsNumber(double Fallback = 0.0) const;
			int AsInt(int Fallback = 0) const;            // clamped to the int range, truncated
			int64_t AsInt64(int64_t Fallback = 0) const;
			const std::string& AsString() const;          // empty when not a string

			// Convenience: read member Key of an object with a fallback.
			bool GetBool(const std::string& Key, bool bFallback = false) const;
			double GetNumber(const std::string& Key, double Fallback = 0.0) const;
			int GetInt(const std::string& Key, int Fallback = 0) const;
			int64_t GetInt64(const std::string& Key, int64_t Fallback = 0) const;
			std::string GetString(const std::string& Key, const std::string& Fallback = std::string()) const;

			const std::vector<std::string>& Keys() const { return ObjectKeys; }
			const std::vector<FValue>& Items() const { return Elements; } // array items or object values (same order as Keys)

		private:
			EType Kind = EType::Null;
			bool bBoolean = false;
			double NumberValue = 0.0;
			std::string Text;
			std::vector<FValue> Elements;
			std::vector<std::string> ObjectKeys;
		};

		// Compact output. Strings are escaped; non-ASCII bytes pass through as UTF-8. NaN and infinity become null.
		std::string Write(const FValue& Value);

		// Parses a whole document. Returns false (and a message) on any syntax error, trailing text, or nesting deeper than
		// MaxDepth.
		constexpr int MaxDepth = 64;
		bool Parse(const std::string& Text, FValue& Out, std::string& Error);
	}
}
