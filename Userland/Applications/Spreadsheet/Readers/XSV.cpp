/*
 * Copyright (c) 2020, the SerenityOS developers.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "XSV.h"
#include <AK/StringBuilder.h>

namespace Reader {

ParserBehaviour operator&(ParserBehaviour left, ParserBehaviour right)
{
    return static_cast<ParserBehaviour>(static_cast<u32>(left) & static_cast<u32>(right));
}

ParserBehaviour operator|(ParserBehaviour left, ParserBehaviour right)
{
    return static_cast<ParserBehaviour>(static_cast<u32>(left) | static_cast<u32>(right));
}

void XSV::set_error(ReadError error)
{
    if (m_error == ReadError::None)
        m_error = error;
}

Vector<String> XSV::headers() const
{
    Vector<String> headers;
    if (has_explicit_headers()) {
        for (auto& field : m_names)
            headers.append(field.is_string_view ? field.as_string_view : field.as_string.view());
    } else {
        // No headers read, grab one of the rows and generate empty names
        if (m_rows.is_empty())
            return headers;

        for ([[maybe_unused]] auto& field : m_rows.first())
            headers.append(String::empty());
    }

    return headers;
}

void XSV::parse()
{
    if ((m_behaviours & ParserBehaviour::ReadHeaders) != ParserBehaviour::None)
        read_headers();

    while (!has_error() && !m_lexer.is_eof())
        m_rows.append(read_row());

    // Read and drop any extra lines at the end.
    while (!m_lexer.is_eof()) {
        if (!m_lexer.consume_specific("\r\n") && !m_lexer.consume_specific('\n'))
            break;
    }

    if (!m_lexer.is_eof())
        set_error(ReadError::DataPastLogicalEnd);
}

void XSV::read_headers()
{
    if (!m_names.is_empty()) {
        set_error(ReadError::InternalError);
        m_names.clear();
    }

    m_names = read_row(true);
}

Vector<XSV::Field> XSV::read_row(bool header_row)
{
    Vector<Field> row;
    bool first = true;
    while (!(m_lexer.is_eof() || m_lexer.next_is('\n') || m_lexer.next_is("\r\n")) && (first || m_lexer.consume_specific(m_traits.separator))) {
        first = false;
        row.append(read_one_field());
    }

    if (!m_lexer.is_eof()) {
        auto crlf_ok = m_lexer.consume_specific("\r\n");
        if (!crlf_ok) {
            auto lf_ok = m_lexer.consume_specific('\n');
            if (!lf_ok)
                set_error(ReadError::DataPastLogicalEnd);
        }
    }

    auto is_lenient = (m_behaviours & ParserBehaviour::Lenient) != ParserBehaviour::None;
    if (is_lenient) {
        if (m_rows.is_empty())
            return row;

        auto& last_row = m_rows.last();
        if (row.size() < last_row.size()) {
            if (!m_names.is_empty())
                row.resize(m_names.size());
            else
                row.resize(last_row.size());
        } else if (row.size() > last_row.size()) {
            auto new_size = row.size();
            for (auto& row : m_rows)
                row.resize(new_size);
        }
    } else {
        auto should_read_headers = (m_behaviours & ParserBehaviour::ReadHeaders) != ParserBehaviour::None;
        if (!header_row && should_read_headers && row.size() != m_names.size())
            set_error(ReadError::NonConformingColumnCount);
        else if (!header_row && !has_explicit_headers() && !m_rows.is_empty() && m_rows.first().size() != row.size())
            set_error(ReadError::NonConformingColumnCount);
    }

    return row;
}

XSV::Field XSV::read_one_field()
{
    if ((m_behaviours & ParserBehaviour::TrimLeadingFieldSpaces) != ParserBehaviour::None)
        m_lexer.consume_while(is_any_of(" \t\v"));

    bool is_quoted = false;
    Field field;
    if (m_lexer.next_is(m_traits.quote.view())) {
        is_quoted = true;
        field = read_one_quoted_field();
    } else {
        field = read_one_unquoted_field();
    }

    if ((m_behaviours & ParserBehaviour::TrimTrailingFieldSpaces) != ParserBehaviour::None) {
        m_lexer.consume_while(is_any_of(" \t\v"));

        if (!is_quoted) {
            // Also have to trim trailing spaces from unquoted fields.
            StringView view;
            if (field.is_string_view)
                view = field.as_string_view;
            else
                view = field.as_string;

            if (!view.is_empty()) {
                ssize_t i = view.length() - 1;
                for (; i >= 0; --i) {
                    if (!view.substring_view(i, 1).is_one_of(" ", "\t", "\v"))
                        break;
                }
                view = view.substring_view(0, i + 1);
            }

            if (field.is_string_view)
                field.as_string_view = view;
            else
                field.as_string = field.as_string.substring(0, view.length());
        }
    }

    return field;
}

XSV::Field XSV::read_one_quoted_field()
{
    if (!m_lexer.consume_specific(m_traits.quote))
        set_error(ReadError::InternalError);

    size_t start = m_lexer.tell(), end = start;
    bool is_copy = false;
    StringBuilder builder;
    auto allow_newlines = (m_behaviours & ParserBehaviour::AllowNewlinesInFields) != ParserBehaviour::None;

    for (; !m_lexer.is_eof();) {
        char ch;
        switch (m_traits.quote_escape) {
        case ParserTraits::Backslash:
            if (m_lexer.consume_specific('\\') && m_lexer.consume_specific(m_traits.quote)) {
                // If there is an escaped quote, we have no choice but to make a copy.
                if (!is_copy) {
                    is_copy = true;
                    builder.append(m_source.substring_view(start, end - start));
                }
                builder.append(m_traits.quote);
                end = m_lexer.tell();
                continue;
            }
            break;
        case ParserTraits::Repeat:
            if (m_lexer.consume_specific(m_traits.quote)) {
                if (m_lexer.consume_specific(m_traits.quote)) {
                    // If there is an escaped quote, we have no choice but to make a copy.
                    if (!is_copy) {
                        is_copy = true;
                        builder.append(m_source.substring_view(start, end - start));
                    }
                    builder.append(m_traits.quote);
                    end = m_lexer.tell();
                    continue;
                }
                for (size_t i = 0; i < m_traits.quote.length(); ++i)
                    m_lexer.retreat();
                goto end;
            }
            break;
        }

        if (m_lexer.next_is(m_traits.quote.view()))
            goto end;

        if (!allow_newlines) {
            if (m_lexer.next_is('\n') || m_lexer.next_is("\r\n"))
                goto end;
        }

        ch = m_lexer.consume();
        if (is_copy)
            builder.append(ch);
        end = m_lexer.tell();
        continue;

    end:
        break;
    }

    if (!m_lexer.consume_specific(m_traits.quote))
        set_error(ReadError::QuoteFailure);

    if (is_copy)
        return { {}, builder.to_string(), false };

    return { m_source.substring_view(start, end - start), {}, true };
}

XSV::Field XSV::read_one_unquoted_field()
{
    size_t start = m_lexer.tell(), end = start;
    bool allow_quote_in_field = (m_behaviours & ParserBehaviour::QuoteOnlyInFieldStart) != ParserBehaviour::None;

    for (; !m_lexer.is_eof();) {
        if (m_lexer.next_is(m_traits.separator.view()))
            break;

        if (m_lexer.next_is("\r\n") || m_lexer.next_is("\n"))
            break;

        if (m_lexer.consume_specific(m_traits.quote)) {
            if (!allow_quote_in_field)
                set_error(ReadError::QuoteFailure);
            end = m_lexer.tell();
            continue;
        }

        m_lexer.consume();
        end = m_lexer.tell();
    }

    return { m_source.substring_view(start, end - start), {}, true };
}

StringView XSV::Row::operator[](StringView name) const
{
    VERIFY(!m_xsv.m_names.is_empty());
    auto it = m_xsv.m_names.find_if([&](const auto& entry) { return name == entry; });
    VERIFY(!it.is_end());

    return (*this)[it.index()];
}

StringView XSV::Row::operator[](size_t column) const
{
    auto& field = m_xsv.m_rows[m_index][column];
    if (field.is_string_view)
        return field.as_string_view;
    return field.as_string;
}

const XSV::Row XSV::operator[](size_t index) const
{
    return const_cast<XSV&>(*this)[index];
}

XSV::Row XSV::at(size_t index) const
{
    return this->operator[](index);
}

XSV::Row XSV::operator[](size_t index)
{
    VERIFY(m_rows.size() > index);
    return Row { *this, index };
}

}
