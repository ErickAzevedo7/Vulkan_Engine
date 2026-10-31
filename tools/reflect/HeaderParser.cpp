#include "HeaderParser.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace Reflect {

    namespace {
        enum class TokenKind {
            Identifier,
            Number,
            String,
            Symbol,
            End,
        };

        struct Token {
            TokenKind Kind = TokenKind::End;
            std::string Text;
            int Line = 0;
        };

        bool IsIdentifierStart(char c) {
            return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
        }

        bool IsIdentifierChar(char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        // Splits a header into tokens, dropping comments and preprocessor lines.
        std::vector<Token> Tokenize(const std::string& source) {
            std::vector<Token> tokens;
            int line = 1;
            bool atLineStart = true;
            size_t i = 0;
            const size_t size = source.size();

            while (i < size) {
                char c = source[i];
                if (c == '\n') {
                    ++line;
                    atLineStart = true;
                    ++i;
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(c))) {
                    ++i;
                    continue;
                }
                if (c == '/' && i + 1 < size && source[i + 1] == '/') {
                    while (i < size && source[i] != '\n') ++i;
                    continue;
                }
                if (c == '/' && i + 1 < size && source[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < size && !(source[i] == '*' && source[i + 1] == '/')) {
                        if (source[i] == '\n') ++line;
                        ++i;
                    }
                    i += 2;
                    continue;
                }
                // A directive runs to the end of the line, or further after a backslash.
                if (c == '#' && atLineStart) {
                    while (i < size && source[i] != '\n') {
                        if (source[i] == '\\' && i + 1 < size && source[i + 1] == '\n') {
                            ++line;
                            ++i;
                        }
                        ++i;
                    }
                    continue;
                }

                atLineStart = false;
                Token token;
                token.Line = line;
                size_t start = i;

                if (IsIdentifierStart(c)) {
                    while (i < size && IsIdentifierChar(source[i])) ++i;
                    token.Kind = TokenKind::Identifier;
                } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                           (c == '.' && i + 1 < size && std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
                    // Digits, letters (hex, suffixes, exponents), dots and an exponent's sign.
                    while (i < size) {
                        char d = source[i];
                        bool exponentSign = (d == '+' || d == '-') && (source[i - 1] == 'e' || source[i - 1] == 'E');
                        if (!IsIdentifierChar(d) && d != '.' && d != '\'' && !exponentSign) break;
                        ++i;
                    }
                    token.Kind = TokenKind::Number;
                } else if (c == '"' || c == '\'') {
                    ++i;
                    while (i < size && source[i] != c) {
                        if (source[i] == '\\') ++i;
                        if (i < size && source[i] == '\n') ++line;
                        ++i;
                    }
                    ++i;
                    token.Kind = TokenKind::String;
                } else if (c == ':' && i + 1 < size && source[i + 1] == ':') {
                    i += 2;
                    token.Kind = TokenKind::Symbol;
                } else {
                    ++i;
                    token.Kind = TokenKind::Symbol;
                }

                token.Text = source.substr(start, i - start);
                tokens.push_back(std::move(token));
            }

            Token end;
            end.Line = line;
            tokens.push_back(end);
            return tokens;
        }

        // Follows namespaces and classes through a header and reads what comes after each mark.
        class Parser {
        public:
            Parser(std::vector<Token> tokens, std::string file, std::string include, ReflectionModel& model)
                : m_Tokens(std::move(tokens)), m_File(std::move(file)), m_Include(std::move(include)),
                  m_Model(model) {}

            void Run() {
                while (Peek().Kind != TokenKind::End) {
                    const Token& token = Peek();
                    if (token.Kind == TokenKind::Identifier) {
                        if (token.Text == "namespace") {
                            ParseNamespace();
                            continue;
                        }
                        if (token.Text == "VKE_ENUM") {
                            ParseEnum();
                            continue;
                        }
                        if (token.Text == "VKE_STRUCT") {
                            ParseStructMark();
                            continue;
                        }
                        if (token.Text == "VKE_COMPONENT" || token.Text == "VKE_SCRIPT") {
                            ParseComponentMark();
                            continue;
                        }
                        if (token.Text == "VKE_API") {
                            ParseApiFunction();
                            continue;
                        }
                        if (token.Text == "VKE_PROPERTY") {
                            ParseProperty();
                            continue;
                        }
                        if (token.Text == "enum") {
                            // An unmarked enum: its body is a plain scope. Its name isn't a class name.
                            ++m_Pos;
                            if (IsIdentifier("class") || IsIdentifier("struct")) ++m_Pos;
                            continue;
                        }
                        if ((token.Text == "class" || token.Text == "struct") && !PreviousIs("friend")) {
                            ParseClassHead();
                            continue;
                        }
                        if ((token.Text == "public" || token.Text == "private" || token.Text == "protected") &&
                            Peek(1).Text == ":" && !m_Scopes.empty() && m_Scopes.back().Kind == ScopeKind::Class) {
                            m_Scopes.back().IsPublic = token.Text == "public";
                        }
                    } else if (token.Kind == TokenKind::Symbol && token.Text == "{") {
                        OpenScope();
                    } else if (token.Kind == TokenKind::Symbol && token.Text == "}") {
                        if (m_Scopes.empty()) Fail(token, "unbalanced '}'");
                        m_Scopes.pop_back();
                    }
                    ++m_Pos;
                }

                if (m_MarkedStruct >= 0) {
                    Fail(Peek(), "VKE_STRUCT isn't followed by a struct definition");
                }
            }

        private:
            enum class ScopeKind {
                Namespace,
                Class,
                Other,
            };

            struct Scope {
                ScopeKind Kind = ScopeKind::Other;
                std::string Name;
                // Index into m_Model.Structs when the class is marked, or -1.
                int Struct = -1;
                bool IsPublic = true;
            };

            const Token& Peek(size_t offset = 0) const {
                size_t index = m_Pos + offset;
                return index < m_Tokens.size() ? m_Tokens[index] : m_Tokens.back();
            }

            bool IsIdentifier(const char* text, size_t offset = 0) const {
                const Token& token = Peek(offset);
                return token.Kind == TokenKind::Identifier && token.Text == text;
            }

            bool IsSymbol(const char* text, size_t offset = 0) const {
                const Token& token = Peek(offset);
                return token.Kind == TokenKind::Symbol && token.Text == text;
            }

            bool PreviousIs(const char* text) const {
                return m_Pos > 0 && m_Tokens[m_Pos - 1].Text == text;
            }

            [[noreturn]] void Fail(const Token& token, const std::string& message) const {
                throw ReflectError{{m_File, token.Line}, message};
            }

            const Token& Expect(TokenKind kind, const char* what) {
                const Token& token = Peek();
                if (token.Kind != kind) Fail(token, std::string("expected ") + what + ", found '" + token.Text + "'");
                ++m_Pos;
                return token;
            }

            void ExpectSymbol(const char* symbol) {
                if (!IsSymbol(symbol)) Fail(Peek(), std::string("expected '") + symbol + "', found '" + Peek().Text + "'");
                ++m_Pos;
            }

            // The names of the enclosing namespaces and classes, joined by "::".
            std::string GetQualifiedPrefix() const {
                std::string prefix;
                for (const Scope& scope : m_Scopes) {
                    if (scope.Kind == ScopeKind::Other || scope.Name.empty()) continue;
                    prefix += scope.Name + "::";
                }
                return prefix;
            }

            // The tokens between a mark's parentheses. Leaves the position after the ')'.
            std::vector<Token> ReadMarkArguments() {
                ++m_Pos;
                ExpectSymbol("(");
                std::vector<Token> arguments;
                int depth = 0;
                while (!(depth == 0 && IsSymbol(")"))) {
                    if (Peek().Kind == TokenKind::End) Fail(Peek(), "unterminated mark arguments");
                    if (IsSymbol("(")) ++depth;
                    if (IsSymbol(")")) --depth;
                    arguments.push_back(Peek());
                    ++m_Pos;
                }
                ++m_Pos;
                return arguments;
            }

            void ParseNamespace() {
                const Token& keyword = Peek();
                ++m_Pos;
                std::string name;
                while (Peek().Kind == TokenKind::Identifier || IsSymbol("::")) {
                    name += Peek().Text;
                    ++m_Pos;
                }
                if (IsSymbol("=")) {
                    // An alias: nothing to follow.
                    while (!IsSymbol(";") && Peek().Kind != TokenKind::End) ++m_Pos;
                    return;
                }
                if (!IsSymbol("{")) Fail(keyword, "expected '{' after the namespace name");
                ++m_Pos;
                m_Scopes.push_back({ScopeKind::Namespace, name});
            }

            void ParseClassHead() {
                const Token& keyword = Peek();
                bool isStruct = keyword.Text == "struct";
                ++m_Pos;
                if (Peek().Kind != TokenKind::Identifier) {
                    if (m_MarkedStruct >= 0) Fail(keyword, "a VKE_STRUCT needs a name");
                    return;
                }
                std::string name = Peek().Text;

                // A definition opens a brace before any ';'. A declaration doesn't.
                size_t brace = std::string::npos;
                int depth = 0;
                for (size_t i = m_Pos + 1; i < m_Tokens.size() && m_Tokens[i].Kind != TokenKind::End; ++i) {
                    const Token& token = m_Tokens[i];
                    if (token.Kind != TokenKind::Symbol) continue;
                    if (token.Text == "(") ++depth;
                    if (token.Text == ")") --depth;
                    if (depth != 0) continue;
                    if (token.Text == ";") break;
                    if (token.Text == "{") {
                        brace = i;
                        break;
                    }
                }

                if (m_MarkedStruct >= 0) {
                    if (brace == std::string::npos) Fail(keyword, "VKE_STRUCT must be followed by a definition");
                    StructDef& marked = m_Model.Structs[static_cast<size_t>(m_MarkedStruct)];
                    marked.Name = name;
                    marked.QualifiedName = GetQualifiedPrefix() + name;
                    marked.Location = {m_File, keyword.Line};
                }
                if (brace != std::string::npos) {
                    m_PendingClassBrace = brace;
                    m_PendingClassName = name;
                    m_PendingClassIsStruct = isStruct;
                    m_PendingClassStruct = m_MarkedStruct;
                }
                m_MarkedStruct = -1;
            }

            void OpenScope() {
                if (m_Pos == m_PendingClassBrace) {
                    Scope scope{ScopeKind::Class, m_PendingClassName, m_PendingClassStruct, m_PendingClassIsStruct};
                    m_Scopes.push_back(scope);
                    m_PendingClassBrace = std::string::npos;
                    m_PendingClassStruct = -1;
                } else {
                    m_Scopes.push_back({ScopeKind::Other, std::string()});
                }
            }

            void ParseEnum() {
                const Token& mark = Peek();
                ReadMarkArguments();
                if (!IsIdentifier("enum")) Fail(mark, "VKE_ENUM must be followed by an enum class");
                ++m_Pos;
                if (!IsIdentifier("class") && !IsIdentifier("struct")) {
                    Fail(mark, "VKE_ENUM needs an 'enum class', whose values are scoped");
                }
                ++m_Pos;

                EnumDef definition;
                definition.Name = Expect(TokenKind::Identifier, "the enum's name").Text;
                definition.QualifiedName = GetQualifiedPrefix() + definition.Name;
                definition.Location = {m_File, mark.Line};

                // The underlying type, if any.
                while (!IsSymbol("{")) {
                    if (Peek().Kind == TokenKind::End || IsSymbol(";")) Fail(mark, "VKE_ENUM needs an enum definition");
                    ++m_Pos;
                }
                ++m_Pos;

                std::uint32_t next = 0;
                while (!IsSymbol("}")) {
                    const Token& value = Expect(TokenKind::Identifier, "an enum value");
                    std::uint32_t number = next;
                    if (IsSymbol("=")) {
                        ++m_Pos;
                        if (IsSymbol("-")) Fail(value, "enum values must not be negative");
                        number = ParseInteger(Expect(TokenKind::Number, "an integer"));
                    }
                    definition.Values.emplace_back(value.Text, number);
                    next = number + 1;

                    if (IsSymbol(",")) {
                        ++m_Pos;
                    } else if (!IsSymbol("}")) {
                        Fail(Peek(), "expected ',' or '}' in the enum, found '" + Peek().Text + "'");
                    }
                }
                ++m_Pos;
                ExpectSymbol(";");
                m_Model.Enums.push_back(std::move(definition));
            }

            std::uint32_t ParseInteger(const Token& token) const {
                std::string digits;
                for (char c : token.Text) {
                    if (c == '\'') continue;
                    digits += c;
                }
                // Drops u, l and their combinations.
                while (!digits.empty() && (digits.back() == 'u' || digits.back() == 'U' || digits.back() == 'l' ||
                                           digits.back() == 'L')) {
                    digits.pop_back();
                }
                try {
                    size_t used = 0;
                    unsigned long long value = std::stoull(digits, &used, 0);
                    if (used != digits.size() || value > 0xFFFFFFFFull) throw std::out_of_range("");
                    return static_cast<std::uint32_t>(value);
                } catch (const std::exception&) {
                    Fail(token, "'" + token.Text + "' isn't an integer the generator can read");
                }
            }

            void ParseStructMark() {
                const Token& mark = Peek();
                ReadMarkArguments();
                if (!IsIdentifier("struct") && !IsIdentifier("class")) {
                    Fail(mark, "VKE_STRUCT must be followed by a struct or class");
                }
                StructDef definition;
                definition.Include = m_Include;
                m_Model.Structs.push_back(std::move(definition));
                m_MarkedStruct = static_cast<int>(m_Model.Structs.size() - 1);
            }

            void ParseComponentMark() {
                const Token& mark = Peek();
                std::vector<Token> arguments = ReadMarkArguments();
                if (arguments.empty() || arguments[0].Kind != TokenKind::Identifier) {
                    Fail(mark, mark.Text + " needs the class name");
                }
                if (m_Scopes.empty() || m_Scopes.back().Kind != ScopeKind::Class ||
                    m_Scopes.back().Name != arguments[0].Text) {
                    Fail(mark, mark.Text + "(" + arguments[0].Text + ") must go in the body of class " +
                                   arguments[0].Text);
                }
                Scope& scope = m_Scopes.back();
                if (scope.Struct >= 0) Fail(mark, "a VKE_STRUCT can't also be a component");

                StructDef definition;
                definition.Name = arguments[0].Text;
                definition.QualifiedName = GetQualifiedPrefix();
                // The prefix ends with the class itself.
                definition.QualifiedName.erase(definition.QualifiedName.size() - 2);
                definition.IsComponent = true;
                definition.Include = m_Include;
                definition.Location = {m_File, mark.Line};

                for (size_t i = 1; i < arguments.size(); ++i) {
                    const Token& argument = arguments[i];
                    if (argument.Text == ",") continue;
                    if (argument.Text == "NotAddable") {
                        definition.Addable = false;
                    } else {
                        Fail(argument, "unknown " + mark.Text + " option '" + argument.Text + "'");
                    }
                }

                m_Model.Structs.push_back(std::move(definition));
                scope.Struct = static_cast<int>(m_Model.Structs.size() - 1);
                // The macro ends in a private section.
                scope.IsPublic = false;
            }

            // A declaration at namespace scope: a return type, a name and parameters.
            void ParseApiFunction() {
                const Token& mark = Peek();
                ++m_Pos;
                ApiFunctionDef function;
                function.Location = {m_File, mark.Line};
                for (const Scope& scope : m_Scopes) {
                    if (scope.Kind != ScopeKind::Namespace) Fail(mark, "VKE_API goes on a function in a namespace");
                    function.Namespace += (function.Namespace.empty() ? "" : "::") + scope.Name;
                }
                if (function.Namespace != "vke" && function.Namespace.rfind("vke::", 0) != 0) {
                    Fail(mark, "VKE_API goes on a function in namespace vke");
                }

                std::vector<std::string> head;
                while (!IsSymbol("(")) {
                    if (Peek().Kind == TokenKind::End || IsSymbol(";")) {
                        Fail(mark, "VKE_API must be followed by a function");
                    }
                    head.push_back(Peek().Text);
                    ++m_Pos;
                }
                if (head.size() < 2) Fail(mark, "VKE_API must be followed by a return type and a name");
                function.Name = head.back();
                head.pop_back();
                function.ReturnType = Join(head);
                ++m_Pos;

                std::vector<std::string> parameter;
                int depth = 0;
                bool inDefault = false;
                auto finish = [&] {
                    inDefault = false;
                    if (parameter.empty()) return;
                    if (parameter.size() < 2) Fail(mark, "parameters of a VKE_API function need names");
                    std::string name = parameter.back();
                    parameter.pop_back();
                    function.Parameters.push_back({Join(parameter), name});
                    parameter.clear();
                };
                while (!(depth == 0 && IsSymbol(")"))) {
                    if (Peek().Kind == TokenKind::End) Fail(mark, "unterminated parameter list");
                    if (IsSymbol("(") || IsSymbol("<")) ++depth;
                    if (IsSymbol(")") || IsSymbol(">")) --depth;
                    if (depth == 0 && IsSymbol(",")) {
                        finish();
                    } else if (depth == 0 && IsSymbol("=")) {
                        inDefault = true;
                    } else if (!inDefault) {
                        parameter.push_back(Peek().Text);
                    }
                    ++m_Pos;
                }
                finish();
                ++m_Pos;
                if (!IsSymbol(";")) Fail(mark, "VKE_API goes on a declaration; the generator writes the body");
                m_Model.Functions.push_back(std::move(function));
            }

            static std::string Join(const std::vector<std::string>& words) {
                std::string text;
                for (const std::string& word : words) {
                    text += (text.empty() ? "" : " ") + word;
                }
                return text;
            }

            void ParseProperty() {
                const Token& mark = Peek();
                std::vector<Token> arguments = ReadMarkArguments();

                if (m_Scopes.empty() || m_Scopes.back().Kind != ScopeKind::Class || m_Scopes.back().Struct < 0) {
                    Fail(mark, "VKE_PROPERTY must be on a member of a VKE_STRUCT or of a component");
                }
                const Scope& scope = m_Scopes.back();
                StructDef& owner = m_Model.Structs[static_cast<size_t>(scope.Struct)];
                if (!owner.IsComponent && !scope.IsPublic) {
                    Fail(mark, "properties of a VKE_STRUCT must be public, so the generated code can reach them");
                }

                PropertyDef property;
                property.Location = {m_File, mark.Line};
                ReadPropertyArguments(arguments, property);

                // The declaration, up to its ';'.
                size_t start = m_Pos;
                int depth = 0;
                while (!(depth == 0 && IsSymbol(";"))) {
                    if (Peek().Kind == TokenKind::End) Fail(mark, "VKE_PROPERTY must be followed by a declaration");
                    if (IsSymbol("(") || IsSymbol("{") || IsSymbol("[")) ++depth;
                    if (IsSymbol(")") || IsSymbol("}") || IsSymbol("]")) --depth;
                    ++m_Pos;
                }
                size_t end = m_Pos;
                ++m_Pos;

                // The declarator ends at the initializer, if there is one.
                size_t declaratorEnd = end;
                int angles = 0;
                for (size_t i = start; i < end; ++i) {
                    const Token& token = m_Tokens[i];
                    if (token.Kind != TokenKind::Symbol) continue;
                    if (token.Text == "<") ++angles;
                    if (token.Text == ">") --angles;
                    if (angles == 0 && (token.Text == "=" || token.Text == "{")) {
                        declaratorEnd = i;
                        break;
                    }
                    if (angles == 0 && token.Text == ",") Fail(token, "VKE_PROPERTY marks one declaration at a time");
                    if (token.Text == "(") Fail(token, "VKE_PROPERTY marks a data member, not a function");
                }

                if (declaratorEnd <= start + 1 || m_Tokens[declaratorEnd - 1].Kind != TokenKind::Identifier) {
                    Fail(mark, "VKE_PROPERTY must be followed by a type and a member name");
                }
                property.Member = m_Tokens[declaratorEnd - 1].Text;

                for (size_t i = start; i < declaratorEnd - 1; ++i) {
                    const std::string& text = m_Tokens[i].Text;
                    if (text == "static" || text == "const" || text == "constexpr" || text == "mutable") {
                        Fail(m_Tokens[i], "a property can't be '" + text + "'");
                    }
                    property.TypeText += text;
                }

                if (property.SavedName.empty()) {
                    property.SavedName = property.Member;
                    if (property.SavedName.rfind("m_", 0) == 0) property.SavedName.erase(0, 2);
                }
                for (const PropertyDef& existing : owner.Properties) {
                    if (existing.SavedName == property.SavedName) {
                        Fail(mark, "two properties of " + owner.Name + " are saved as '" + property.SavedName + "'");
                    }
                }
                owner.Properties.push_back(std::move(property));
            }

            void ReadPropertyArguments(const std::vector<Token>& arguments, PropertyDef& property) {
                size_t i = 0;
                while (i < arguments.size()) {
                    const Token& key = arguments[i];
                    if (key.Kind != TokenKind::Identifier || i + 1 >= arguments.size() || arguments[i + 1].Text != "=") {
                        Fail(key, "VKE_PROPERTY options are written 'Key = value'");
                    }
                    i += 2;

                    std::string value;
                    if (i < arguments.size() && arguments[i].Text == "-") {
                        value = "-";
                        ++i;
                    }
                    if (i >= arguments.size()) Fail(key, "'" + key.Text + "' has no value");
                    const Token& literal = arguments[i];
                    value += literal.Text;
                    ++i;

                    if (key.Text == "Name") {
                        if (literal.Kind != TokenKind::String) Fail(literal, "Name takes a string");
                        property.SavedName = literal.Text.substr(1, literal.Text.size() - 2);
                    } else if (key.Text == "Min" || key.Text == "Max") {
                        if (literal.Kind != TokenKind::Number) Fail(literal, key.Text + " takes a number");
                        (key.Text == "Min" ? property.Min : property.Max) = value;
                    } else if (key.Text == "Display") {
                        if (literal.Text != "Color" && literal.Text != "Angle") {
                            Fail(literal, "Display is Color or Angle");
                        }
                        property.Display = literal.Text;
                    } else {
                        Fail(key, "unknown VKE_PROPERTY option '" + key.Text + "'");
                    }

                    if (i < arguments.size()) {
                        if (arguments[i].Text != ",") Fail(arguments[i], "expected ',' between VKE_PROPERTY options");
                        ++i;
                    }
                }
            }

            std::vector<Token> m_Tokens;
            std::string m_File;
            std::string m_Include;
            ReflectionModel& m_Model;
            size_t m_Pos = 0;
            std::vector<Scope> m_Scopes;

            // A VKE_STRUCT waiting for its struct, as an index into m_Model.Structs.
            int m_MarkedStruct = -1;
            // The brace that will open the class just seen.
            size_t m_PendingClassBrace = std::string::npos;
            std::string m_PendingClassName;
            bool m_PendingClassIsStruct = false;
            int m_PendingClassStruct = -1;
        };
    } // namespace

    void ParseHeader(const std::filesystem::path& file, const std::filesystem::path& includeRoot,
                     ReflectionModel& model) {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) throw ReflectError{{file.generic_string(), 0}, "can't read the file"};
        std::stringstream buffer;
        buffer << stream.rdbuf();

        // Code includes the header by its path under `includeRoot` or a module's "public" folder.
        std::string path = file.generic_string();
        std::string include;
        if (!includeRoot.empty()) {
            include = std::filesystem::relative(file, includeRoot).generic_string();
        } else {
            size_t publicFolder = path.rfind("/public/");
            if (publicFolder == std::string::npos) {
                throw ReflectError{{path, 0}, "the header isn't in a module's public folder"};
            }
            include = path.substr(publicFolder + 8);
        }

        std::vector<Token> tokens = Tokenize(buffer.str());
        for (const Token& token : tokens) {
            for (unsigned char c : token.Text) {
                model.TokenHash = (model.TokenHash ^ c) * 16777619u;
            }
            model.TokenHash = (model.TokenHash ^ 0xFFu) * 16777619u;
        }
        Parser parser(std::move(tokens), path, include, model);
        parser.Run();
        model.Includes.push_back(include);
    }

} // namespace Reflect
