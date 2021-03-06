/**
  \file G3D-base.lib/source/stringutils.cpp

  G3D Innovation Engine http://casual-effects.com/g3d
  Copyright 2000-2019, Morgan McGuire
  All rights reserved
  Available under the BSD License
*/

#include "G3D-base/platform.h"
#include "G3D-base/stringutils.h"
#include "G3D-base/BinaryInput.h"
#include <algorithm>
#include <regex>

#ifdef G3D_WINDOWS
extern "C" {    
    // Define functions for ffmpeg since we don't link in gcc's c library
    extern int strncasecmp(const char *string1, const char *string2, size_t count) { return _strnicmp(string1, string2, count); }
    extern int strcasecmp(const char *string1, const char *string2) { return _stricmp(string1, string2); }
}
#endif

namespace G3D {

#ifdef _MSC_VER
    // disable: "C++ exception handler used"
#   pragma warning (push)
#   pragma warning (disable : 4530)
#endif
#ifdef G3D_WINDOWS
    const char* NEWLINE = "\r\n";
#else
    const char* NEWLINE = "\n";
    static bool iswspace(int ch) { return (ch==' ' || ch=='\t' || ch=='\n' || ch=='\r'); }
#endif

bool alphabeticalIgnoringCaseG3DFirstLessThan(const String& _a, const String& _b) {
    const String& a = toLower(_a);
    const String& b = toLower(_b);

    if (beginsWith(a, "g3d")) {
        if (beginsWith(b, "g3d")) {
            return (a < b);
        } else {
            return true;
        }
    } else if (beginsWith(b, "g3d")) {
        return false;
    } else if (beginsWith(a, "mcg")) {
        if (beginsWith(b, "mcg")) {
            return (a < b);
        } else {
            return true;
        }
    } else if (beginsWith(b, "mcg")) {
        return false;
    } else if (beginsWith(a, "nxp")) {
        if (beginsWith(b, "nxp")) {
            return (a < b);
        } else {
            return true;
        }
    } else if (beginsWith(b, "nxp")) {
        return false;    
    } else {
        return (a < b);
    }
}

void parseCommaSeparated(const String s, Array<String>& array, bool stripQuotes) {
    array.fastClear();
    if (s == "") {
        return;
    }

    size_t begin = 0;
    const char delimiter = ',';
    const char quote = '\"';
    do {
        size_t end = begin;
        // Find the next comma, or the end of the string
        bool inQuotes = false;
        while ((end < s.length()) && (inQuotes || (s[end] != delimiter))) {
            if (s[end] == quote) {
                if ((end < s.length() - 2) && (s[end + 1] == quote) && (s[end + 2]) == quote) {
                    // Skip over the superquote
                    end += 2;
                }
                inQuotes = ! inQuotes;
            }
            ++end;
        }
        array.append(s.substr(begin, end - begin));
        begin = end + 1;
    } while (begin < s.length());

    if (stripQuotes) {
        for (int i = 0; i < array.length(); ++i) {
            String& t = array[i];
            size_t L = t.length();
            if ((L > 1) && (t[0] == quote) && (t[L - 1] == quote)) {
                if ((L > 6)  && (t[1] == quote) && (t[2] == quote) && (t[L - 3] == quote) && (t[L - 2] == quote)) {
                    // Triple-quote
                    t = t.substr(3, L - 6);
                } else {
                    // Double-quote
                    t = t.substr(1, L - 2);
                }
            }
        }
    }
}

bool beginsWith(
    const String& test,
    const String& pattern) {

    if (test.size() >= pattern.size()) {
        for (int i = 0; i < (int)pattern.size(); ++i) {
            if (pattern[i] != test[i]) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}

String replace(const String& s, const String& pattern, const String& replacement) {
    if (pattern.length() == 0) {
        return s;
    }
    String temp = "";
    size_t lastindex = 0;
    size_t nextindex = 0;
    do {
        nextindex = s.find(pattern, lastindex);
        if (nextindex == String::npos) {
            break;
        }
        temp += s.substr(lastindex, nextindex - lastindex) + replacement;
        lastindex = nextindex + pattern.length();
    } while (lastindex + pattern.length() <= s.length());
    return temp + (lastindex < s.length() ? s.substr(lastindex) : "");
}

bool isValidIdentifier(const String& s) {
    if (s.length() > 0 && (isLetter(s[0]) || s[0] == '_')) {   
        for (size_t i = 1; i < s.length() ; ++i) {
            if (!( isLetter(s[i]) || (s[i] == '_') || isDigit(s[i]) )) {
                return false;
            }
        }
        return true;
    }
    return false;
}

String makeValidIndentifierWithUnderscores(const String& s) {
    
    String valid = s;
    if (!isValidIdentifier(s)) {
        //begin with underscore if invalid first character
        if (!(isLetter(s[0]) || s[0] == '_')) {
            valid = '_' + s;
        }
        for (size_t i = 1; i < valid.length(); ++i) {
            if (! ((valid[i] == '_') || isLetter(valid[i]) || isDigit(valid[i]))) {
                valid[i] = '_';
            }
        }
    }
    return valid;
}

bool endsWith(
    const String& test,
    const String& pattern) {

    if (test.size() >= pattern.size()) {
        size_t te = test.size() - 1;
        size_t pe = pattern.size() - 1;
        for (int i = (int)pattern.size() - 1; i >= 0; --i) {
            if (pattern[pe - i] != test[te - i]) {
                return false;
            }
        }
        return true;
    } else {
        return false;
    }
}


String wordWrap(
    const String&      input,
    int                     numCols) {

    String output;
    size_t      c = 0;
    int         len;

    // Don't make lines less than this length
    int         minLength = numCols / 4;
    size_t      inLen = input.size();

    bool first = true;
    while (c < inLen) {
        if (first) {
            first = false;
        } else {
            output += NEWLINE;
        }

        if ((int)inLen - (int)c - 1 < numCols) {
            // The end
            output += input.substr(c, inLen - c);
            break;
        }

        len = numCols;

        // Look at character c + numCols, see if it is a space.
        while ((len > minLength) &&
               (input[c + len] != ' ')) {
            len--;
        }

        if (len == minLength) {
            // Just crop
            len = numCols;

        }

        output += input.substr(c, len);
        c += len;
        if (c < input.size()) {
            // Collapse multiple spaces.
            while ((input[c] == ' ') && (c < input.size())) {
                ++c;
            }
        }
    }

    return output;
}


int stringCompare(
    const String&      s1,
    const String&      s2) {

    return stringPtrCompare(&s1, &s2);
}


int stringPtrCompare(
    const String*      s1,
    const String*      s2) {

    return s1->compare(*s2);
}


String toUpper(const String& x) {
    String result = x;
    std::transform(result.begin(), result.end(), result.begin(), toupper);
    return result;
}


String toLower(const String& x) {
    String result = x;
    std::transform(result.begin(), result.end(), result.begin(), tolower);
    return result;
}


Array<String> stringSplit(
    const String&          x,
    char                   splitChar) {
    Array<String>  out;
    stringSplit(x, splitChar, out);
    return out;
}


void stringSplit(
    const String&          x,
    char                   splitChar,
    Array<String>&         out) {

    out.fastClear();    
    // Pointers to the beginning and end of the substring
    const char* start = x.c_str();
    const char* stop = start;
    
    while ((stop = strchr(start, splitChar))) {
        out.append(String(start, stop - start));
        start = stop + 1;
    }

    // Append the last one
    out.append(String(start));
}


String stringJoin(
    const Array<String>&   a,
    char                        joinChar) {

    String out;

    for (int i = 0; i < (int)a.size() - 1; ++i) {
        out += a[i] + joinChar;
    }

    if (a.size() > 0) {
        return out + a.last();
    } else {
        return out;
    }
}


String stringJoin(
    const Array<String>&   a,
    const String&          joinStr) {

    String out;

    for (int i = 0; i < (int)a.size() - 1; ++i) {
        out += a[i] + joinStr;
    }

    if (a.size() > 0) {
        return out + a.last();
    } else {
        return out;
    }
}


String trimWhitespace(const String& s) {

    if (s.length() == 0) {
        return s;
    }
    size_t left = 0;
    
    // Trim from left
    while ((left < s.length()) && iswspace(s[left])) {
        ++left;
    }

    size_t right = s.length() - 1;
    // Trim from right
    while ((right > left) && iswspace(s[right])) {
        --right;
    }
    
    return s.substr(left, right - left + 1);
}

Array<String> splitLines(const String& s) {
    Array<String> lines;
    if (s.length() == 0) {
        return lines;
    }

    int start = 0;
    int count = 0;
    for (int pos = 0; pos < (int)s.size(); ++pos) {
        if (s[pos] == '\r') {
            continue;
        } else if (s[pos] == '\n') {
            lines.append(s.substr(start, count));
            start = pos + 1;
            count = 0;
        } else {
            ++count;
        }
    }
    lines.append(s.substr(start, count));
    return lines;
}

    
void toUTF8(const String& s, Array<char>& result) {
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = s[i];
        if (c < 128) {
            result.append(c);
        } else {
            //Split the value into two
            result.append((c & 0x3F) | 0x80);
            result.append((c >> 6) | 0xC0);
        }
    }
}

namespace _internal {
    /** Helper function to PrefixTree. Finds all strings 
        within the given interval which share an indented 
        prefix with the item at index start, continuing the 
        tree traversal with the smallest greatest common 
        prefix of these strings.
        \param list alphabetically sorted list of strings
        \param tree depth first tree traversal, providing functions enterChild(n) and goToParent() 
        \param start beginning of interval, inclusive
        \param end end of interval, exclusive
        \param indent starting index of string being considered */
void buildPrefixTree(const Array<String>& list, DepthFirstTreeBuilder<String>& tree, const size_t start, const size_t end, const size_t indent) {      
    debugAssertM((int)end <= (int)list.length(), "Index out of bounds.");
    debugAssertM(start <= end + 1, "The interval cannot have the inclusive start index more than one past the exclusive end index");
    
    if (start >= end) {
        // reached end of recursion
        return;
    } 
    
    if (start == (end - 1)) {
        // if start == (end - 1), then entry is a leaf
        tree.enterChild(list[int(start)].substr(indent));
        tree.goToParent();
    } else {
        // else, find elements at top of list that share a prefix
        // start < end <  list.length(), therefore start and start + 1 are valid indices 
        // t, leastGCP, and prefix will change upon each iteration
        size_t t = start;
        String leastGCP = greatestCommonPrefix(list[start].substr(indent), list[start + 1].substr(indent));
        String prefix = leastGCP;
    
        while ((! prefix.empty()) && (t < (end - 1))) {
            // when considering children, we must keep track of indent index of where
            // the child starts in the string
            // for example, when considering the children of "G3D Demo" and "G3D Scene",
            // the indent will be 4 since the parent is "G3D " and the children are "Demo" and "Scene"
            prefix = greatestCommonPrefix(list[t].substr(indent), list[t + 1].substr(indent)); 
            if (! prefix.empty()) {
                ++t;
                if (prefix.length() < leastGCP.length()) {
                  leastGCP = prefix; 
                }
            }
        } // while
        
        if (leastGCP.empty()) {
            // if leastGCP is "", then list[start] is a leaf
            buildPrefixTree(list, tree, start, t + 1,   indent);
            buildPrefixTree(list, tree, t + 1, end, indent);
        
        } else { 
            // t represents last index of list that shared a common prefix
            // otherwise, leastGCP is a child and buildPrefixTree can be recursively called
            // if t < end, then there are siblings to be added as well
            tree.enterChild(leastGCP);
            buildPrefixTree(list, tree, start, t + 1, indent + leastGCP.length());
            tree.goToParent();

            if (t < end - 1) {
                buildPrefixTree(list, tree, t + 1, end, indent); 
            } // t < end
        } // leastGCP is empty
    } // start < end
}

} // _internal


void buildPrefixTree(const Array<String>& list, DepthFirstTreeBuilder<String>& tree) {
     _internal::buildPrefixTree(list, tree, 0, list.length(), 0);
}

}; // namespace

#undef NEWLINE
#ifdef _MSC_VER
#   pragma warning (pop)
#endif
