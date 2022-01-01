#include <catch2/catch_test_macros.hpp>
#include <Lexer.hpp>

using namespace vwa;

SCENARIO("Individual tokens")
{
    GIVEN("An empty input string")
    {
        THEN("There should be no tokens")
        {
            std::optional<std::vector<vwa::Token>> tokens = tokenize("");
            REQUIRE(tokens.has_value());
            REQUIRE(tokens->size() == 1);
            REQUIRE(tokens->back().type == Token::Type::eof);
        }
    }

    GIVEN("a simple identifier")
    {
        THEN("There should be exactly one token of type id")
        {
            std::optional<std::vector<Token>> tokens = tokenize("id");
            REQUIRE(tokens.has_value());
            REQUIRE(tokens->size() == 2);
            REQUIRE(tokens->back().type == Token::Type::eof);
            REQUIRE(tokens->at(0).type == Token::Type::id);
            REQUIRE(std::holds_alternative<std::string>(tokens->at(0).value));
            REQUIRE(std::get<std::string>(tokens->at(0).value) == "id");
        }
    }

    SECTION("literal values")
    {
        GIVEN("An integer literal")
        {
            THEN("There should be exactly one token of type int")
            {
                std::optional<std::vector<Token>> tokens = tokenize("42");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::int_literal);
                REQUIRE(std::holds_alternative<int32_t>(tokens->at(0).value));
                REQUIRE(std::get<int32_t>(tokens->at(0).value) == 42);
            }
        }

        GIVEN("A long literal")
        {
            THEN("There should be exactly one token of type long")
            {
                std::optional<std::vector<Token>> tokens = tokenize("42l");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::long_literal);
                REQUIRE(std::holds_alternative<int64_t>(tokens->at(0).value));
                REQUIRE(std::get<int64_t>(tokens->at(0).value) == 42);
            }
        }

        GIVEN("A float literal")
        {
            THEN("There should be exactly one token of type float")
            {
                std::optional<std::vector<Token>> tokens = tokenize("42.0");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::float_literal);
                REQUIRE(std::holds_alternative<float>(tokens->at(0).value));
                REQUIRE(std::get<float>(tokens->at(0).value) == 42.0f);
            }
        }

        GIVEN("A double literal")
        {
            THEN("There should be exactly one token of type double")
            {
                std::optional<std::vector<Token>> tokens = tokenize("42.0d");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::double_literal);
                REQUIRE(std::holds_alternative<double>(tokens->at(0).value));
                REQUIRE(std::get<double>(tokens->at(0).value) == 42.0);
            }
        }

        GIVEN("A character literal")
        {
            THEN("There should be exactly one token of type char")
            {
                std::optional<std::vector<Token>> tokens = tokenize("'a'");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::char_literal);
                REQUIRE(std::holds_alternative<char>(tokens->at(0).value));
                REQUIRE(std::get<char>(tokens->at(0).value) == 'a');
            }
        }

        GIVEN("A string literal")
        {
            THEN("There should be exactly one token of type string")
            {
                std::optional<std::vector<Token>> tokens = tokenize("\"hello\"");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::string_literal);
                REQUIRE(std::holds_alternative<std::string>(tokens->at(0).value));
                REQUIRE(std::get<std::string>(tokens->at(0).value) == "hello");
            }
        }
    }

    SECTION("braces")
    {
        GIVEN("A '{'")
        {
            THEN("There should be exactly one token of type lbrace")
            {
                std::optional<std::vector<Token>> tokens = tokenize("{");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::lbrace);
            }
        }
        GIVEN("A '}'")
        {
            THEN("There should be exactly one token of type rbrace")
            {
                std::optional<std::vector<Token>> tokens = tokenize("}");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::rbrace);
            }
        }
        GIVEN("A '('")
        {
            THEN("There should be exactly one token of type lparen")
            {
                std::optional<std::vector<Token>> tokens = tokenize("(");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::lparen);
            }
        }
        GIVEN("A ')'")
        {
            THEN("There should be exactly one token of type rparen")
            {
                std::optional<std::vector<Token>> tokens = tokenize(")");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::rparen);
            }
        }
        GIVEN("A '['")
        {
            THEN("There should be exactly one token of type lbracket")
            {
                std::optional<std::vector<Token>> tokens = tokenize("[");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::lbracket);
            }
        }
        GIVEN("A ']'")
        {
            THEN("There should be exactly one token of type rbracket")
            {
                std::optional<std::vector<Token>> tokens = tokenize("]");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::rbracket);
            }
        }
    }

    SECTION("Simple symbols")
    {

        GIVEN("A ';'")
        {
            THEN("There should be exactly one token of type semicolon")
            {
                std::optional<std::vector<Token>> tokens = tokenize(";");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::semicolon);
            }
        }
        GIVEN("A ','")
        {
            THEN("There should be exactly one token of type comma")
            {
                std::optional<std::vector<Token>> tokens = tokenize(",");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::comma);
            }
        }
        GIVEN("A '.'")
        {
            THEN("There should be exactly one token of type dot")
            {
                std::optional<std::vector<Token>> tokens = tokenize(".");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::dot);
            }
        }
        GIVEN("A '->'")
        {
            THEN("There should be exactly one token of type arrow")
            {
                std::optional<std::vector<Token>> tokens = tokenize("->");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::arrow_);
            }
        }
        GIVEN("A ':'")
        {
            THEN("There should be exactly one token of type colon")
            {
                std::optional<std::vector<Token>> tokens = tokenize(":");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::colon);
            }
        }

        GIVEN("A '+'")
        {
            THEN("There should be exactly one token of type plus")
            {
                std::optional<std::vector<Token>> tokens = tokenize("+");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::plus);
            }
        }
        GIVEN("A '-'")
        {
            THEN("There should be exactly one token of type minus")
            {
                std::optional<std::vector<Token>> tokens = tokenize("-");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::minus);
            }
        }
        GIVEN("A '*'")
        {
            THEN("There should be exactly one token of type asterix")
            {
                std::optional<std::vector<Token>> tokens = tokenize("*");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::asterix);
            }
        }
        GIVEN("A '**'")
        {
            THEN("There should be exactly one token of type double_asterix")
            {
                std::optional<std::vector<Token>> tokens = tokenize("**");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::double_asterix);
            }
        }
        GIVEN("A '/'")
        {
            THEN("There should be exactly one token of type slash")
            {
                std::optional<std::vector<Token>> tokens = tokenize("/");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::slash);
            }
        }
        GIVEN("A '%'")
        {
            THEN("There should be exactly one token of type percent")
            {
                std::optional<std::vector<Token>> tokens = tokenize("%");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::modulo);
            }
        }
        GIVEN("A '&'")
        {
            THEN("There should be exactly one token of type ampersand")
            {
                std::optional<std::vector<Token>> tokens = tokenize("&");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::ampersand);
            }
        }
        GIVEN("A '&&'")
        {
            THEN("There should be exactly one token of type double_ampersand")
            {
                std::optional<std::vector<Token>> tokens = tokenize("&&");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::double_ampersand);
            }
        }
        GIVEN("A '!'")
        {
            THEN("There should be exactly one token of type exclamation")
            {
                std::optional<std::vector<Token>> tokens = tokenize("!");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::exclamation);
            }
        }
        GIVEN("A '||'")
        {
            THEN("There should be exactly one token of type double_pipe")
            {
                std::optional<std::vector<Token>> tokens = tokenize("||");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::double_pipe);
            }
        }
        GIVEN("A '<'")
        {
            THEN("There should be exactly one token of type lt")
            {
                std::optional<std::vector<Token>> tokens = tokenize("<");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::lt);
            }
        }
        GIVEN("A '<='")
        {
            THEN("There should be exactly one token of type lte")
            {
                std::optional<std::vector<Token>> tokens = tokenize("<=");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::lte);
            }
        }
        GIVEN("A '>'")
        {
            THEN("There should be exactly one token of type gt")
            {
                std::optional<std::vector<Token>> tokens = tokenize(">");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::gt);
            }
        }
        GIVEN("A '>='")
        {
            THEN("There should be exactly one token of type gte")
            {
                std::optional<std::vector<Token>> tokens = tokenize(">=");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::gte);
            }
        }
        GIVEN("A '=='")
        {
            THEN("There should be exactly one token of type eq")
            {
                std::optional<std::vector<Token>> tokens = tokenize("==");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::eq);
            }
        }
        GIVEN("A '!='")
        {
            THEN("There should be exactly one token of type neq")
            {
                std::optional<std::vector<Token>> tokens = tokenize("!=");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::neq);
            }
        }
        GIVEN("A '='")
        {
            THEN("There should be exactly one token of type assign")
            {
                std::optional<std::vector<Token>> tokens = tokenize("=");
                REQUIRE(tokens.has_value());
                REQUIRE(tokens->size() == 2);
                REQUIRE(tokens->back().type == Token::Type::eof);
                REQUIRE(tokens->at(0).type == Token::Type::assign);
            }
        }
    }

    SECTION("keywords")
    {
        GIVEN("Any keyword from the list of reserved identifiers")
        {
            for (auto &kw : reservedIdentifiers)
                THEN("There should be exactly one token matching the type of keyword provided")
                {
                    std::optional<std::vector<Token>> tokens = tokenize(kw.first);
                    REQUIRE(tokens.has_value());
                    REQUIRE(tokens->size() == 2);
                    REQUIRE(tokens->back().type == Token::Type::eof);
                    REQUIRE(tokens->at(0).type == kw.second);
                }
        }
    }
}

// TODO: escape sequences