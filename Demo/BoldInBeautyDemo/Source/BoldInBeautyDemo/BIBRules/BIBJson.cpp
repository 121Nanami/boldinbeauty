// Boldinbeauty demo — JSON value, writer and parser (see BIBJson.h).
#include "BIBJson.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace BIB
{
	namespace Json
	{
		namespace
		{
			const FValue& NullValue()
			{
				static const FValue Null;
				return Null;
			}

			const std::string& EmptyString()
			{
				static const std::string Empty;
				return Empty;
			}

			// ------------------------------------------------------------- writer
			void AppendEscaped(std::string& Out, const std::string& Text)
			{
				Out += '"';
				for (char Raw : Text)
				{
					const unsigned char Character = static_cast<unsigned char>(Raw);
					switch (Character)
					{
					case '"': Out += "\\\""; break;
					case '\\': Out += "\\\\"; break;
					case '\b': Out += "\\b"; break;
					case '\f': Out += "\\f"; break;
					case '\n': Out += "\\n"; break;
					case '\r': Out += "\\r"; break;
					case '\t': Out += "\\t"; break;
					default:
						if (Character < 0x20)
						{
							char Buffer[8];
							std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", static_cast<unsigned>(Character));
							Out += Buffer;
						}
						else
						{
							Out += Raw;
						}
						break;
					}
				}
				Out += '"';
			}

			void AppendNumber(std::string& Out, double Value)
			{
				if (!std::isfinite(Value))
				{
					Out += "null";
					return;
				}
				char Buffer[40];
				if (Value == std::floor(Value) && std::fabs(Value) < 1e15)
				{
					std::snprintf(Buffer, sizeof(Buffer), "%lld", static_cast<long long>(Value));
				}
				else
				{
					std::snprintf(Buffer, sizeof(Buffer), "%.17g", Value);
				}
				Out += Buffer;
			}

			void WriteValue(std::string& Out, const FValue& Value)
			{
				switch (Value.Type())
				{
				case EType::Null: Out += "null"; break;
				case EType::Bool: Out += Value.AsBool() ? "true" : "false"; break;
				case EType::Number: AppendNumber(Out, Value.AsNumber()); break;
				case EType::String: AppendEscaped(Out, Value.AsString()); break;
				case EType::Array:
				{
					Out += '[';
					for (size_t Index = 0; Index < Value.Size(); ++Index)
					{
						if (Index > 0)
						{
							Out += ',';
						}
						WriteValue(Out, Value.At(Index));
					}
					Out += ']';
					break;
				}
				case EType::Object:
				{
					Out += '{';
					const std::vector<std::string>& Keys = Value.Keys();
					const std::vector<FValue>& Items = Value.Items();
					for (size_t Index = 0; Index < Keys.size(); ++Index)
					{
						if (Index > 0)
						{
							Out += ',';
						}
						AppendEscaped(Out, Keys[Index]);
						Out += ':';
						WriteValue(Out, Items[Index]);
					}
					Out += '}';
					break;
				}
				}
			}

			// ------------------------------------------------------------- parser
			class FParser
			{
			public:
				FParser(const std::string& InText) : Text(InText) {}

				bool Run(FValue& Out, std::string& Error)
				{
					SkipSpace();
					if (!ParseValue(Out, 0))
					{
						Error = Message.empty() ? "invalid JSON" : Message;
						return false;
					}
					SkipSpace();
					if (Position != Text.size())
					{
						Error = "unexpected text after the JSON value at offset " + std::to_string(Position);
						return false;
					}
					return true;
				}

			private:
				const std::string& Text;
				size_t Position = 0;
				std::string Message;

				bool Fail(const char* What)
				{
					if (Message.empty())
					{
						Message = std::string(What) + " at offset " + std::to_string(Position);
					}
					return false;
				}

				void SkipSpace()
				{
					while (Position < Text.size())
					{
						const char Character = Text[Position];
						if (Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r')
						{
							++Position;
						}
						else
						{
							break;
						}
					}
				}

				bool Consume(const char* Word)
				{
					size_t Length = 0;
					while (Word[Length] != '\0')
					{
						++Length;
					}
					if (Text.compare(Position, Length, Word) != 0)
					{
						return false;
					}
					Position += Length;
					return true;
				}

				bool ParseValue(FValue& Out, int Depth)
				{
					if (Depth > MaxDepth)
					{
						return Fail("nesting too deep");
					}
					if (Position >= Text.size())
					{
						return Fail("unexpected end of text");
					}
					const char Character = Text[Position];
					if (Character == '{')
					{
						return ParseObject(Out, Depth);
					}
					if (Character == '[')
					{
						return ParseArray(Out, Depth);
					}
					if (Character == '"')
					{
						std::string Value;
						if (!ParseString(Value))
						{
							return false;
						}
						Out = FValue::MakeString(Value);
						return true;
					}
					if (Character == 't')
					{
						if (!Consume("true"))
						{
							return Fail("invalid literal");
						}
						Out = FValue::MakeBool(true);
						return true;
					}
					if (Character == 'f')
					{
						if (!Consume("false"))
						{
							return Fail("invalid literal");
						}
						Out = FValue::MakeBool(false);
						return true;
					}
					if (Character == 'n')
					{
						if (!Consume("null"))
						{
							return Fail("invalid literal");
						}
						Out = FValue();
						return true;
					}
					return ParseNumber(Out);
				}

				bool ParseNumber(FValue& Out)
				{
					const size_t Start = Position;
					if (Position < Text.size() && Text[Position] == '-')
					{
						++Position;
					}
					if (Position >= Text.size() || Text[Position] < '0' || Text[Position] > '9')
					{
						return Fail("invalid number");
					}
					if (Text[Position] == '0')
					{
						++Position;
					}
					else
					{
						while (Position < Text.size() && Text[Position] >= '0' && Text[Position] <= '9')
						{
							++Position;
						}
					}
					if (Position < Text.size() && Text[Position] == '.')
					{
						++Position;
						if (Position >= Text.size() || Text[Position] < '0' || Text[Position] > '9')
						{
							return Fail("invalid number");
						}
						while (Position < Text.size() && Text[Position] >= '0' && Text[Position] <= '9')
						{
							++Position;
						}
					}
					if (Position < Text.size() && (Text[Position] == 'e' || Text[Position] == 'E'))
					{
						++Position;
						if (Position < Text.size() && (Text[Position] == '+' || Text[Position] == '-'))
						{
							++Position;
						}
						if (Position >= Text.size() || Text[Position] < '0' || Text[Position] > '9')
						{
							return Fail("invalid number");
						}
						while (Position < Text.size() && Text[Position] >= '0' && Text[Position] <= '9')
						{
							++Position;
						}
					}
					const std::string Digits = Text.substr(Start, Position - Start);
					const double Value = std::strtod(Digits.c_str(), nullptr);
					Out = FValue::MakeNumber(std::isfinite(Value) ? Value : 0.0);
					return true;
				}

				static int HexValue(char Character)
				{
					if (Character >= '0' && Character <= '9') return Character - '0';
					if (Character >= 'a' && Character <= 'f') return Character - 'a' + 10;
					if (Character >= 'A' && Character <= 'F') return Character - 'A' + 10;
					return -1;
				}

				bool ReadHex4(unsigned& Code)
				{
					if (Position + 4 > Text.size())
					{
						return Fail("truncated \\u escape");
					}
					Code = 0;
					for (int Digit = 0; Digit < 4; ++Digit)
					{
						const int Value = HexValue(Text[Position + static_cast<size_t>(Digit)]);
						if (Value < 0)
						{
							return Fail("invalid \\u escape");
						}
						Code = Code * 16u + static_cast<unsigned>(Value);
					}
					Position += 4;
					return true;
				}

				static void AppendUtf8(std::string& Out, unsigned Code)
				{
					if (Code < 0x80u)
					{
						Out += static_cast<char>(Code);
					}
					else if (Code < 0x800u)
					{
						Out += static_cast<char>(0xC0u | (Code >> 6));
						Out += static_cast<char>(0x80u | (Code & 0x3Fu));
					}
					else if (Code < 0x10000u)
					{
						Out += static_cast<char>(0xE0u | (Code >> 12));
						Out += static_cast<char>(0x80u | ((Code >> 6) & 0x3Fu));
						Out += static_cast<char>(0x80u | (Code & 0x3Fu));
					}
					else
					{
						Out += static_cast<char>(0xF0u | (Code >> 18));
						Out += static_cast<char>(0x80u | ((Code >> 12) & 0x3Fu));
						Out += static_cast<char>(0x80u | ((Code >> 6) & 0x3Fu));
						Out += static_cast<char>(0x80u | (Code & 0x3Fu));
					}
				}

				bool ParseString(std::string& Out)
				{
					++Position; // opening quote
					while (true)
					{
						if (Position >= Text.size())
						{
							return Fail("unterminated string");
						}
						const char Character = Text[Position++];
						if (Character == '"')
						{
							return true;
						}
						if (static_cast<unsigned char>(Character) < 0x20)
						{
							return Fail("control character in string");
						}
						if (Character != '\\')
						{
							Out += Character;
							continue;
						}
						if (Position >= Text.size())
						{
							return Fail("unterminated escape");
						}
						const char Escape = Text[Position++];
						switch (Escape)
						{
						case '"': Out += '"'; break;
						case '\\': Out += '\\'; break;
						case '/': Out += '/'; break;
						case 'b': Out += '\b'; break;
						case 'f': Out += '\f'; break;
						case 'n': Out += '\n'; break;
						case 'r': Out += '\r'; break;
						case 't': Out += '\t'; break;
						case 'u':
						{
							unsigned Code = 0;
							if (!ReadHex4(Code))
							{
								return false;
							}
							if (Code >= 0xD800u && Code <= 0xDBFFu)
							{
								// High surrogate: a \uXXXX low surrogate must follow.
								unsigned Low = 0;
								if (Position + 2 > Text.size() || Text[Position] != '\\' || Text[Position + 1] != 'u')
								{
									return Fail("lone surrogate");
								}
								Position += 2;
								if (!ReadHex4(Low))
								{
									return false;
								}
								if (Low < 0xDC00u || Low > 0xDFFFu)
								{
									return Fail("invalid surrogate pair");
								}
								Code = 0x10000u + ((Code - 0xD800u) << 10) + (Low - 0xDC00u);
							}
							else if (Code >= 0xDC00u && Code <= 0xDFFFu)
							{
								return Fail("lone surrogate");
							}
							AppendUtf8(Out, Code);
							break;
						}
						default:
							return Fail("invalid escape");
						}
					}
				}

				bool ParseArray(FValue& Out, int Depth)
				{
					++Position; // [
					Out = FValue::MakeArray();
					SkipSpace();
					if (Position < Text.size() && Text[Position] == ']')
					{
						++Position;
						return true;
					}
					while (true)
					{
						SkipSpace();
						FValue Item;
						if (!ParseValue(Item, Depth + 1))
						{
							return false;
						}
						Out.Push(Item);
						SkipSpace();
						if (Position >= Text.size())
						{
							return Fail("unterminated array");
						}
						const char Next = Text[Position++];
						if (Next == ']')
						{
							return true;
						}
						if (Next != ',')
						{
							return Fail("expected , or ] in array");
						}
					}
				}

				bool ParseObject(FValue& Out, int Depth)
				{
					++Position; // {
					Out = FValue::MakeObject();
					SkipSpace();
					if (Position < Text.size() && Text[Position] == '}')
					{
						++Position;
						return true;
					}
					while (true)
					{
						SkipSpace();
						if (Position >= Text.size() || Text[Position] != '"')
						{
							return Fail("expected a string key");
						}
						std::string Key;
						if (!ParseString(Key))
						{
							return false;
						}
						SkipSpace();
						if (Position >= Text.size() || Text[Position] != ':')
						{
							return Fail("expected : after key");
						}
						++Position;
						SkipSpace();
						FValue Item;
						if (!ParseValue(Item, Depth + 1))
						{
							return false;
						}
						Out.Set(Key, Item);
						SkipSpace();
						if (Position >= Text.size())
						{
							return Fail("unterminated object");
						}
						const char Next = Text[Position++];
						if (Next == '}')
						{
							return true;
						}
						if (Next != ',')
						{
							return Fail("expected , or } in object");
						}
					}
				}
			};
		}

		// ===================================================================== FValue
		FValue FValue::MakeBool(bool bValue)
		{
			FValue Value;
			Value.Kind = EType::Bool;
			Value.bBoolean = bValue;
			return Value;
		}

		FValue FValue::MakeNumber(double Number)
		{
			FValue Value;
			Value.Kind = EType::Number;
			Value.NumberValue = Number;
			return Value;
		}

		FValue FValue::MakeString(const std::string& Text)
		{
			FValue Value;
			Value.Kind = EType::String;
			Value.Text = Text;
			return Value;
		}

		FValue FValue::MakeArray()
		{
			FValue Value;
			Value.Kind = EType::Array;
			return Value;
		}

		FValue FValue::MakeObject()
		{
			FValue Value;
			Value.Kind = EType::Object;
			return Value;
		}

		FValue& FValue::Set(const std::string& Key, const FValue& Value)
		{
			if (Kind != EType::Object)
			{
				*this = MakeObject();
			}
			for (size_t Index = 0; Index < ObjectKeys.size(); ++Index)
			{
				if (ObjectKeys[Index] == Key)
				{
					Elements[Index] = Value;
					return Elements[Index];
				}
			}
			ObjectKeys.push_back(Key);
			Elements.push_back(Value);
			return Elements.back();
		}

		const FValue* FValue::Find(const std::string& Key) const
		{
			if (Kind != EType::Object)
			{
				return nullptr;
			}
			for (size_t Index = 0; Index < ObjectKeys.size(); ++Index)
			{
				if (ObjectKeys[Index] == Key)
				{
					return &Elements[Index];
				}
			}
			return nullptr;
		}

		void FValue::Push(const FValue& Value)
		{
			if (Kind != EType::Array)
			{
				*this = MakeArray();
			}
			Elements.push_back(Value);
		}

		size_t FValue::Size() const
		{
			return (Kind == EType::Array || Kind == EType::Object) ? Elements.size() : 0;
		}

		const FValue& FValue::At(size_t Index) const
		{
			return (Kind == EType::Array && Index < Elements.size()) ? Elements[Index] : NullValue();
		}

		bool FValue::AsBool(bool bFallback) const
		{
			return Kind == EType::Bool ? bBoolean : bFallback;
		}

		double FValue::AsNumber(double Fallback) const
		{
			return Kind == EType::Number ? NumberValue : Fallback;
		}

		int FValue::AsInt(int Fallback) const
		{
			if (Kind != EType::Number)
			{
				return Fallback;
			}
			if (NumberValue >= static_cast<double>(std::numeric_limits<int>::max()))
			{
				return std::numeric_limits<int>::max();
			}
			if (NumberValue <= static_cast<double>(std::numeric_limits<int>::min()))
			{
				return std::numeric_limits<int>::min();
			}
			return static_cast<int>(NumberValue);
		}

		int64_t FValue::AsInt64(int64_t Fallback) const
		{
			if (Kind != EType::Number)
			{
				return Fallback;
			}
			if (NumberValue >= 9.0e18)
			{
				return static_cast<int64_t>(9000000000000000000LL);
			}
			if (NumberValue <= -9.0e18)
			{
				return static_cast<int64_t>(-9000000000000000000LL);
			}
			return static_cast<int64_t>(NumberValue);
		}

		const std::string& FValue::AsString() const
		{
			return Kind == EType::String ? Text : EmptyString();
		}

		bool FValue::GetBool(const std::string& Key, bool bFallback) const
		{
			const FValue* Item = Find(Key);
			return Item ? Item->AsBool(bFallback) : bFallback;
		}

		double FValue::GetNumber(const std::string& Key, double Fallback) const
		{
			const FValue* Item = Find(Key);
			return Item ? Item->AsNumber(Fallback) : Fallback;
		}

		int FValue::GetInt(const std::string& Key, int Fallback) const
		{
			const FValue* Item = Find(Key);
			return Item ? Item->AsInt(Fallback) : Fallback;
		}

		int64_t FValue::GetInt64(const std::string& Key, int64_t Fallback) const
		{
			const FValue* Item = Find(Key);
			return Item ? Item->AsInt64(Fallback) : Fallback;
		}

		std::string FValue::GetString(const std::string& Key, const std::string& Fallback) const
		{
			const FValue* Item = Find(Key);
			return Item && Item->IsString() ? Item->AsString() : Fallback;
		}

		// ===================================================================== entry points
		std::string Write(const FValue& Value)
		{
			std::string Out;
			WriteValue(Out, Value);
			return Out;
		}

		bool Parse(const std::string& Text, FValue& Out, std::string& Error)
		{
			FParser Parser(Text);
			FValue Parsed;
			if (!Parser.Run(Parsed, Error))
			{
				return false;
			}
			Out = Parsed;
			Error.clear();
			return true;
		}
	}
}
