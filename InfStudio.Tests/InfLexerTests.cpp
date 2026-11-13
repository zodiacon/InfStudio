#include "TestCommon.h"
#include "InfLexer.h"
#include <scintilla/Scintilla.h>

#include <algorithm>

using namespace InfLexer;

namespace {
	// one letter per style, so that an expected value lines up under its line
	char Code(uint8_t style) {
		switch (style) {
			case Default: return '.';
			case Comment: return 'c';
			case Section: return 'S';
			case Key: return 'k';
			case Operator: return 'o';
			case String: return 's';
			case Token: return 't';
			case Number: return 'n';
		}
		return '?';
	}

	std::string Codes(std::vector<uint8_t> const& styles, size_t from = 0, size_t to = std::string::npos) {
		std::string codes;
		for (size_t i = from; i < std::min(to, styles.size()); i++)
			codes += Code(styles[i]);
		return codes;
	}

	struct Styled {
		std::string Codes;
		bool Continues;
	};

	Styled StyleOf(std::string_view line, bool continued = false) {
		std::vector<uint8_t> styles(line.size(), 0xFF);
		auto continues = StyleLine(line, continued, styles);
		return { Codes(styles), continues };
	}

	std::string Styles(std::string_view line, bool continued = false) {
		return StyleOf(line, continued).Codes;
	}
}

TEST_CASE("Empty and blank lines", "[lexer]") {
	CHECK(Styles("") == "");
	CHECK(Styles("   \t") == "....");
	CHECK_FALSE(StyleOf("   ").Continues);
}

TEST_CASE("Comments", "[lexer]") {
	CHECK(Styles("; a comment") == "ccccccccccc");
	CHECK(Styles("   ; indented") == "...cccccccccc");
	// after a value, and on a line without '=' (whose '=' in the comment isn't an assignment)
	CHECK(Styles("A = 1 ; c") == "k.o.n.ccc");
	CHECK(Styles("file.sys ; c") == ".........ccc");
	CHECK(Styles("HKR,,Flags,0x1,1 ; c=d") == "...oo.....onnnon.ccccc");
	// '#' and '!' aren't INF comments
	CHECK(Styles("#x") == "..");
}

TEST_CASE("Sections", "[lexer]") {
	CHECK(Styles("[Version]") == "SSSSSSSSS");
	CHECK(Styles("  [Version]") == "..SSSSSSSSS");
	CHECK(Styles("[Version] ; c") == "SSSSSSSSS.ccc");
	CHECK(Styles("[Unterminated") == "SSSSSSSSSSSSS");
	CHECK(Styles("[A;B]") == "SSccc");
	// a continued line isn't a section
	CHECK(Styles("[X]", true) == "...");
}

TEST_CASE("Keys and values", "[lexer]") {
	CHECK(Styles("Class = USB") == "kkkkk.o....");
	CHECK(Styles("Class=USB") == "kkkkko...");
	// only the first '=' separates; one in the value is an operator
	CHECK(Styles("Eq = a=b") == "kk.o..o.");
	// a numeric key is a key
	CHECK(Styles("3426 = x") == "kkkk.o..");
	// ':' is nothing special (the props lexer took it for an assignment)
	CHECK(Styles("HKR,,\"C:\\W\"") == "...oossssss");
}

TEST_CASE("Strings", "[lexer]") {
	// a ';' in a string isn't a comment
	CHECK(Styles("A = \"x;y\" ; c") == "k.o.sssss.ccc");
	CHECK(Styles("S = \"D:P(A;;GA;;;SY)\"") == "k.o." + std::string(17, 's'));
	// "" is a quote in the string
	CHECK(Styles("A = \"a\"\"b\"") == "k.o.ssssss");
	CHECK(Styles("A = \"a\"\"b\" ; c") == "k.o.ssssss.ccc");
	// unterminated: to the end of the line, comment and all
	CHECK(Styles("A = \"x ; y") == "k.o.ssssss");
	// a quoted key; an '=' in it doesn't separate
	CHECK(Styles("\"a=b\" = 1") == "sssss.o.n");
	CHECK(Styles("\"a\"b") == "sss.");
	// a line without a key: an '=' or a ';' in a string doesn't make one, or end the line
	CHECK(Styles("HKR,,\"a=b\"") == "...oosssss");
	CHECK(Styles("HKR,,\"x;y\" ; c=d") == "...oosssss.ccccc");
}

TEST_CASE("String keys", "[lexer]") {
	CHECK(Styles("%Desc% = x") == "tttttt.o..");
	CHECK(Styles("A = %Mfg%") == "k.o.ttttt");
	// inside a string
	CHECK(Styles("A = \"%P%\\x\"") == "k.o.stttsss");
	// %% is a literal percent, not a key's start or end
	CHECK(Styles("A = 100%%") == "k.o......");
	CHECK(Styles("A = %%x%%") == "k.o......");
	CHECK(Styles("A = \"50%% %K%\"") == "k.o.ssssssttts");
	// a lone '%', and one that would take in a blank or a comma, isn't a key
	CHECK(Styles("A = 5% x") == "k.o.....");
	CHECK(Styles("A = %a b%") == "k.o......");
	CHECK(Styles("A = %a,b%") == "k.o...o..");
}

TEST_CASE("Numbers", "[lexer]") {
	CHECK(Styles("A = 0x10, 3, 10.0.1.0, 0xZZ") == "k.o.nnnno.no.........o.....");
	CHECK(Styles("A = 06/21/2006") == "k.o." + std::string(10, '.'));
	CHECK(Styles("A = 0x") == "k.o...");
	CHECK(Styles("HKR,,Start,0x00010001,3") == "...oo.....o" + std::string(10, 'n') + "on");
}

TEST_CASE("Line continuation", "[lexer]") {
	auto first = StyleOf("A = x \\");
	CHECK(first.Codes == "k.o...o");
	CHECK(first.Continues);
	// trailing blanks after the '\' still continue
	CHECK(StyleOf("A = x \\  ").Continues);
	// a continued line has no key
	CHECK(Styles("  b = c", true) == "....o..");
	CHECK(Styles("  b = c", false) == "..k.o..");
	// a '\' inside a value, in a string or in a comment doesn't continue
	auto inside = StyleOf("A = USB\\VID");
	CHECK(inside.Codes == "k.o........");
	CHECK_FALSE(inside.Continues);
	CHECK_FALSE(StyleOf("A = \"x\\\"").Continues);
	CHECK_FALSE(StyleOf("A = x ; c \\").Continues);
	// a continued line can continue again
	CHECK(StyleOf("  more \\", true).Continues);
}

TEST_CASE("Every byte gets a style", "[lexer]") {
	// non-ASCII (UTF-8) bytes are just part of a word or a string
	std::string line = "A = \"Caf\xC3\xA9\" ; \xE6\x97\xA5";
	auto codes = Styles(line);
	REQUIRE(codes.size() == line.size());
	CHECK(codes.find('?') == std::string::npos);
	CHECK(codes == "k.o.sssssss.ccccc");
}

//
// The lexer as Scintilla runs it (InfLexer::Create), on a document with no window: whole lines, line ends, the
// continuation kept in the line state, and lexing that starts in the middle of the document.
//
namespace {
	class FakeDocument : public Scintilla::IDocument {
	public:
		explicit FakeDocument(std::string text) : m_text(std::move(text)), m_styles(m_text.size(), 0x7F) {
			m_lineStarts.push_back(0);
			for (size_t i = 0; i < m_text.size(); i++)
				if (m_text[i] == '\n')
					m_lineStarts.push_back(i + 1);
			m_lineStates.resize(m_lineStarts.size());
			m_levels.resize(m_lineStarts.size(), SC_FOLDLEVELBASE);
		}

		std::string LineCodes(Sci_Position line, bool withEnd = false) const {
			auto start = LineStart(line);
			auto end = withEnd ? LineStart(line + 1) : LineEnd(line);
			std::string codes;
			for (auto i = start; i < end; i++)
				codes += Code((uint8_t)m_styles[i]);
			return codes;
		}

		int SCI_METHOD Version() const override { return Scintilla::dvRelease4; }
		void SCI_METHOD SetErrorStatus(int) override {}
		Sci_Position SCI_METHOD Length() const override { return (Sci_Position)m_text.size(); }
		void SCI_METHOD GetCharRange(char* buffer, Sci_Position position, Sci_Position length) const override {
			memcpy(buffer, m_text.data() + position, length);
		}
		char SCI_METHOD StyleAt(Sci_Position position) const override { return m_styles[position]; }
		Sci_Position SCI_METHOD LineFromPosition(Sci_Position position) const override {
			return (Sci_Position)(std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), (size_t)position) - m_lineStarts.begin()) - 1;
		}
		Sci_Position SCI_METHOD LineStart(Sci_Position line) const override {
			return line < (Sci_Position)m_lineStarts.size() ? (Sci_Position)m_lineStarts[line] : Length();
		}
		int SCI_METHOD GetLevel(Sci_Position line) const override {
			return line < (Sci_Position)m_levels.size() ? m_levels[line] : SC_FOLDLEVELBASE;
		}
		int SCI_METHOD SetLevel(Sci_Position line, int level) override {
			auto old = m_levels[line];
			m_levels[line] = level;
			return old;
		}
		int SCI_METHOD GetLineState(Sci_Position line) const override {
			return line < (Sci_Position)m_lineStates.size() ? m_lineStates[line] : 0;
		}
		int SCI_METHOD SetLineState(Sci_Position line, int state) override {
			auto old = m_lineStates[line];
			m_lineStates[line] = state;
			return old;
		}
		void SCI_METHOD StartStyling(Sci_Position position) override { m_stylingPos = position; }
		bool SCI_METHOD SetStyleFor(Sci_Position length, char style) override {
			std::fill_n(m_styles.begin() + m_stylingPos, length, style);
			m_stylingPos += length;
			return true;
		}
		bool SCI_METHOD SetStyles(Sci_Position length, const char* styles) override {
			std::copy_n(styles, length, m_styles.begin() + m_stylingPos);
			m_stylingPos += length;
			return true;
		}
		void SCI_METHOD DecorationSetCurrentIndicator(int) override {}
		void SCI_METHOD DecorationFillRange(Sci_Position, int, Sci_Position) override {}
		void SCI_METHOD ChangeLexerState(Sci_Position, Sci_Position) override {}
		int SCI_METHOD CodePage() const override { return 65001; }
		bool SCI_METHOD IsDBCSLeadByte(char) const override { return false; }
		const char* SCI_METHOD BufferPointer() override { return m_text.c_str(); }
		int SCI_METHOD GetLineIndentation(Sci_Position) override { return 0; }
		Sci_Position SCI_METHOD LineEnd(Sci_Position line) const override {
			auto end = LineStart(line + 1);
			while (end > LineStart(line) && (m_text[end - 1] == '\n' || m_text[end - 1] == '\r'))
				end--;
			return end;
		}
		Sci_Position SCI_METHOD GetRelativePosition(Sci_Position start, Sci_Position offset) const override { return start + offset; }
		int SCI_METHOD GetCharacterAndWidth(Sci_Position position, Sci_Position* width) const override {
			if (width)
				*width = 1;
			return (unsigned char)m_text[position];
		}

		std::string m_text;
		std::vector<char> m_styles;
		std::vector<size_t> m_lineStarts;
		std::vector<int> m_lineStates;
		std::vector<int> m_levels;
		Sci_Position m_stylingPos{ 0 };
	};

	struct LexerDeleter {
		void operator()(Scintilla::ILexer5* lexer) const { lexer->Release(); }
	};

	void Lex(FakeDocument& doc, Sci_Position start = 0, Sci_Position length = -1) {
		std::unique_ptr<Scintilla::ILexer5, LexerDeleter> lexer(Create());
		REQUIRE(lexer);
		if (length < 0)
			length = doc.Length() - start;
		lexer->Lex(start, length, 0, &doc);
	}
}

TEST_CASE("The Scintilla lexer styles whole documents", "[lexer][scintilla]") {
	FakeDocument doc("[S] ; c\r\nA = x \\\r\n  b = c\r\nB = 1\r\n");
	Lex(doc);
	CHECK(doc.LineCodes(0) == "SSS.ccc");
	CHECK(doc.LineCodes(1) == "k.o...o");
	CHECK(doc.LineCodes(2) == "....o..");		// continued: no key
	CHECK(doc.LineCodes(3) == "k.o.n");
	// the line ends are default
	CHECK(doc.LineCodes(0, true) == "SSS.ccc..");
	// the continuation is kept in the line state
	CHECK(doc.GetLineState(0) == 0);
	CHECK(doc.GetLineState(1) == 1);
	CHECK(doc.GetLineState(2) == 0);
}

TEST_CASE("The Scintilla lexer handles LF line ends and a last line without one", "[lexer][scintilla]") {
	FakeDocument doc("A = 1\nB = \"x\"");
	Lex(doc);
	CHECK(doc.LineCodes(0, true) == "k.o.n.");
	CHECK(doc.LineCodes(1) == "k.o.sss");
}

TEST_CASE("The Scintilla lexer restyles from inside the document", "[lexer][scintilla]") {
	FakeDocument doc("A = x \\\r\n  b = c\r\nC = 2\r\n");
	Lex(doc);
	// again from the line after the continuation, which still is continued (the previous line's state says so)
	std::fill(doc.m_styles.begin(), doc.m_styles.end(), 0x7F);
	Lex(doc, doc.LineStart(1));
	CHECK(doc.LineCodes(1) == "....o..");
	CHECK(doc.LineCodes(2) == "k.o.n");
	// from the middle of a line: the whole line is styled
	std::fill(doc.m_styles.begin(), doc.m_styles.end(), 0x7F);
	Lex(doc, doc.LineStart(2) + 3);
	CHECK(doc.LineCodes(2) == "k.o.n");
}

TEST_CASE("Sections fold", "[lexer][scintilla]") {
	constexpr int Base = SC_FOLDLEVELBASE, Header = SC_FOLDLEVELBASE | SC_FOLDLEVELHEADERFLAG;
	FakeDocument doc(
		"; before the first section\r\n"	// 0
		"\r\n"	// 1
		"[Version]\r\n"	// 2
		"Class = X\r\n"	// 3
		"\r\n"	// 4
		"  [Strings] ; c\r\n"	// 5
		"A = \"[not a section]\"\r\n"	// 6
		"B = x \\\r\n"	// 7
		"[continued, not a section]\r\n"	// 8
		"C = 1");							// 9
	Lex(doc);
	CHECK(doc.GetLevel(0) == Base);
	CHECK(doc.GetLevel(1) == Base);
	CHECK(doc.GetLevel(2) == Header);
	CHECK(doc.GetLevel(3) == Base + 1);
	CHECK(doc.GetLevel(4) == Base + 1);
	CHECK(doc.GetLevel(5) == Header);
	CHECK(doc.GetLevel(6) == Base + 1);
	CHECK(doc.GetLevel(7) == Base + 1);
	CHECK(doc.GetLevel(8) == Base + 1);
	CHECK(doc.GetLevel(9) == Base + 1);

	// from the middle: the lines after a section's header are in it
	std::fill(doc.m_levels.begin(), doc.m_levels.end(), 0);
	doc.m_levels[2] = Header;
	doc.m_levels[3] = Base + 1;
	Lex(doc, doc.LineStart(4));
	CHECK(doc.GetLevel(4) == Base + 1);
	CHECK(doc.GetLevel(5) == Header);
	CHECK(doc.GetLevel(9) == Base + 1);
}
